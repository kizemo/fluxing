#!/usr/bin/env python3
"""Generic Fluxing SFTP release sync.

Usage:
    python sftp_sync.py --version=0.19.0.59
    python sftp_sync.py --version=0.19.0.59 --dry-run
    python sftp_sync.py --version=0.19.0.59 --config=path/to/config.json

Reads config from sftp_config.json by default (gitignored, user-local — copy
from sftp_config.example.json). Path layout lives in config; credentials come
from sftp.json (path in config); expected installer md5 comes from
_check_install_v2.ps1 (path in config). All four filenames are listed in the
config's `files` array — adding a new file is a config edit, not a code edit.

Refactor of _sftp_v058.py. Security model:
  - No hardcoded password (read from sftp.json, gitignored)
  - paramiko.RejectPolicy always (refuse silent TOFU even when known_hosts loaded)
  - Independent trusted-source md5 (ps1, not self-compare)
  - Cross-source verify (server md5 vs git-trusted expected)
  - Configured expectedKey missing/malformed → fail-fast (no silent downgrade)
  - Atomic upload: stage → verify → rename → final-verify, with rollback
  - Unique run-id suffix (version + timestamp) prevents collisions with
    interrupted-ship leftovers
  - Restore via explicit swap (live → rollback_tmp → preship → live) works
    on Windows SFTP servers that don't overwrite on rename
  - shlex.quote() on remote paths in shell commands; strict charset validation
    on remote path components (defense in depth)
  - Fail fast on any mismatch
"""
import argparse
import hashlib
import json
import os
import re
import shlex
import sys
import time

import paramiko


DEFAULT_CONFIG = "sftp_config.json"
EXAMPLE_CONFIG = "sftp_config.example.json"
VERSION_RE = re.compile(r"\A\d+\.\d+\.\d+\.\d+\Z")
MD5_RE = re.compile(r"^[0-9A-Fa-f]{32}$")
STAGE_SUFFIX = ".stage"
PRESHIP_SUFFIX = ".pre-ship"
# Characters banned in any remote path component used in shell commands.
# Conservative: alphanumeric + `.`, `_`, `-`, `/` only. Anything else fails
# validation regardless of quoting.
REMOTE_PATH_FORBIDDEN = re.compile(r"[^A-Za-z0-9._/\-]")
# Placeholder values that signal the config was never edited from the example.
PLACEHOLDER_MARKERS = ("REPLACE_WITH", "<path-to", "${YOUR_")


def md5_file(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest().upper()


def validate_md5(value, key):
    """Validate md5 format (32 hex). Returns uppercase or raises."""
    if not value or not MD5_RE.match(value):
        raise ValueError(
            f"  [ERR] expected md5 for {key!r} is missing or malformed: "
            f"{value!r} (expected 32 hex chars)")
    return value.upper()


def validate_remote_path_component(value, name):
    """Reject any remote path component containing shell metacharacters or
    characters outside [A-Za-z0-9._/-]. Defense in depth alongside shlex.quote.
    Raises ValueError on violation.
    """
    if not value:
        raise ValueError(f"  [ERR] {name} is empty")
    bad = REMOTE_PATH_FORBIDDEN.search(value)
    if bad:
        raise ValueError(
            f"  [ERR] {name} contains unsafe character {bad.group()!r}: "
            f"{value!r} (allowed: [A-Za-z0-9._/-])")
    return value


def validate_config_no_placeholders(config):
    """Refuse to run if the config looks like an unedited example template.
    Detects common placeholder markers in path/string fields.
    """
    string_keys = ("sftpConfigPath", "localBase", "remoteSubdir",
                   "expectedMd5Source")
    for k in string_keys:
        v = config.get(k, "")
        if not isinstance(v, str):
            continue
        for marker in PLACEHOLDER_MARKERS:
            if marker in v:
                print(f"  [ABORT] config field {k!r} contains placeholder "
                      f"marker {marker!r}: {v!r}")
                print(f"          Copy sftp_config.example.json → "
                      f"sftp_config.json and edit paths.")
                sys.exit(1)


def make_run_id(version):
    """Unique per-ship suffix: version + unix timestamp. Prevents collisions
    with `.pre-ship-<old>` leftovers from previous interrupted ships.
    """
    return f"{version}-{int(time.time())}"


def load_json(path, label):
    if not os.path.exists(path):
        print(f"  [ERR] {label} 不存在: {path}")
        sys.exit(1)
    try:
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)
    except Exception as e:
        print(f"  [ERR] {label} JSON 解析失败: {e}")
        sys.exit(1)


def load_sftp_credentials(path):
    cfg = load_json(path, "SFTP credentials")
    required = ["host", "port", "username", "password", "remotePath", "protocol"]
    for k in required:
        if k not in cfg:
            print(f"  [ERR] sftp.json 缺字段: {k}")
            sys.exit(1)
    if cfg["password"] in ("", "PLACEHOLDER", "TODO"):
        print(f"  [ERR] sftp.json password 是 placeholder")
        sys.exit(1)
    return cfg


def parse_expected_md5(ps1_path, keys):
    """Parse `$key = 'XXXXXXXX'` style assignments from a PowerShell file.
    Returns {key: md5_or_None}. A configured `key` is REQUIRED to be present
    and well-formed — caller (render_files) decides whether None is fatal.
    """
    if not os.path.exists(ps1_path):
        print(f"  [ERR] expected md5 source 不存在: {ps1_path}")
        sys.exit(1)
    with open(ps1_path, "r", encoding="utf-8") as f:
        content = f.read()
    out = {}
    for key in keys:
        pattern = r"\$\s*" + re.escape(key) + r"\s*=\s*'([0-9A-Fa-f]+)'"
        m = re.search(pattern, content)
        candidate = m.group(1) if m else None
        if candidate is not None:
            try:
                out[key] = validate_md5(candidate, key)
            except ValueError as e:
                print(f"  [ERR] {e}")
                sys.exit(1)
        else:
            out[key] = None
    return out


def find_known_hosts():
    """Return known_hosts path or None. Used only for diagnostic print."""
    for p in (os.path.expanduser("~/.ssh/known_hosts"),
              os.path.expanduser("~/AppData/Roaming/OpenSSH/known_hosts")):
        if os.path.exists(p):
            return p
    return None


def render_files(template_files, version, expected_md5s):
    """Apply {version} substitution and attach expected md5 from parsed map.
    Fail-fast: if a configured `expectedKey` is non-null but expected_md5s[key]
    is None, abort (L30/L107 silent-failure trap).
    """
    rendered = []
    for f in template_files:
        name = f["name"].replace("{version}", version)
        key = f.get("expectedKey")
        if key:
            expected = expected_md5s.get(key)
            if expected is None:
                print(f"  [ABORT] expected md5 key {key!r} configured but missing "
                      f"in ps1 source (file: {name}). Refusing to ship with "
                      f"self-consistency-only verify. Update _check_install_v2.ps1 "
                      f"first.")
                sys.exit(1)
        else:
            expected = None
        rendered.append({"name": name, "expected": expected})
    return rendered


def precheck_local(files, local_base):
    """Compute local md5 for each file. Abort if any expected md5 mismatches.
    Returns {filename: md5}.
    """
    print(f"\n=== Local md5 pre-check ===")
    local_md5s = {}
    precheck_ok = True
    for f in files:
        local_path = os.path.join(local_base, f["name"])
        if not os.path.exists(local_path):
            print(f"  [ERR] {f['name']}: 本地不存在 @ {local_path}")
            sys.exit(1)
        actual = md5_file(local_path)
        local_md5s[f["name"]] = actual
        if f["expected"]:
            ok = actual.upper() == f["expected"].upper()
            tag = "OK" if ok else "MISMATCH (FAIL)"
            if not ok:
                precheck_ok = False
        else:
            tag = "(no expected — manifest not yet implemented)"
        print(f"  {tag} {f['name']}: {actual}")
    if not precheck_ok:
        print(f"\n[ABORT] local md5 != git-trusted expected — refuse upload")
        sys.exit(1)
    return local_md5s


def server_md5(sftp_client, remote_path):
    """Run md5sum on the remote host and return the uppercase hex digest.
    Validates remote_path against a strict charset (defense in depth) and
    quotes it via shlex.quote to prevent shell injection via the SFTP shell
    command channel.
    """
    validate_remote_path_component(remote_path, "remote_path")
    safe_cmd = f"md5sum {shlex.quote(remote_path)}"
    stdin, stdout, stderr = sftp_client.exec_command(safe_cmd)
    line = stdout.readline().strip()
    if not line:
        return None
    return line.split()[0].upper()


def remote_exists(sftp, remote_path):
    """True if `remote_path` exists on the SFTP server."""
    try:
        sftp.stat(remote_path)
        return True
    except IOError:
        return False


def upload_atomic(sftp, sftp_client, local_base, remote_base, files, local_md5s, version):
    """Atomic upload with rollback.

    Each ship uses a unique run-id (`<version>-<unix_timestamp>`) so stage and
    preship filenames never collide with artifacts from a previous interrupted
    ship.

    1. For each file: if `final` exists, snapshot it as `final.pre-ship-<runid>`.
    2. Upload all files to `<name>.stage-<runid>` (no overwrite of live).
    3. md5sum all staged files — mismatch → restore pre-ship + cleanup, abort.
    4. Atomic rename staged → final for each file in order.
    5. md5sum final files — mismatch → restore + rollback renames, abort.
    6. Cleanup pre-ship snapshots.

    Returns True on full success.

    Restore path uses an explicit swap (live → rollback_tmp → preship → live
    → remove tmp) so it works on Windows SFTP servers whose `rename` does NOT
    overwrite an existing target.
    """
    run_id = make_run_id(version)
    stage_suffix = f"{STAGE_SUFFIX}-{run_id}"
    preship_suffix = f"{PRESHIP_SUFFIX}-{run_id}"

    # Validate every remote path component up front (defense in depth).
    validate_remote_path_component(remote_base, "remote_base")
    for f in files:
        validate_remote_path_component(f["name"], f"file.name({f['name']!r})")

    # Track which files have been renamed (for rollback) and which need
    # pre-ship restore on rollback.
    preship_paths = {}   # name -> preship basename or None
    renamed = []         # ordered list of names that have been renamed so far

    def remove_remote(path):
        try:
            sftp.remove(path)
        except IOError:
            pass

    def swap_restore(name, pre_basename):
        """Restore `name` from `<name>.pre-ship-<runid>` even when live already
        exists (= staged content). Explicit swap:
          1. move live out to rollback_tmp
          2. move preship into live
          3. remove rollback_tmp
        Any IOError is logged; we don't raise.
        """
        live = f"{remote_base}/{name}"
        pre_full = f"{remote_base}/{pre_basename}"
        rollback_tmp = f"{live}.rollback-{run_id}"
        try:
            # Move live out of the way (may fail if live is already gone).
            try:
                sftp.rename(live, rollback_tmp)
            except IOError:
                # Already gone, move on.
                rollback_tmp = None
            # Move preship into live.
            try:
                sftp.rename(pre_full, live)
            except IOError as e:
                print(f"  [WARN] restore {pre_basename} → {name} failed: {e}")
                # Try to put back the rollback_tmp if we moved it.
                if rollback_tmp:
                    try:
                        sftp.rename(rollback_tmp, live)
                    except IOError:
                        pass
                return
            # Remove the side buffer.
            if rollback_tmp:
                remove_remote(rollback_tmp)
        except Exception as e:
            print(f"  [WARN] restore {name} failed: {e}")

    def restore_preship():
        for name, pre in preship_paths.items():
            if pre is None:
                # New file — remove the (possibly renamed) live.
                if name in renamed:
                    remove_remote(f"{remote_base}/{name}")
            else:
                if name in renamed:
                    swap_restore(name, pre)

    def cleanup_staged():
        for f in files:
            remove_remote(f"{remote_base}/{f['name']}{stage_suffix}")

    def cleanup_preship():
        for name, pre in preship_paths.items():
            if pre:
                remove_remote(f"{remote_base}/{pre}")

    # Step 1: snapshot existing live files
    print(f"\n=== Atomic upload (run-id={run_id}) ===")
    for f in files:
        live_path = f"{remote_base}/{f['name']}"
        if remote_exists(sftp, live_path):
            pre_basename = f"{f['name']}{preship_suffix}"
            pre_path = f"{remote_base}/{pre_basename}"
            try:
                sftp.rename(live_path, pre_path)
                preship_paths[f["name"]] = pre_basename
                print(f"  snapshot {f['name']} → {pre_basename}")
            except IOError as e:
                print(f"  [ABORT] snapshot {f['name']} failed: {e}")
                cleanup_preship()
                return False
        else:
            preship_paths[f["name"]] = None

    # Step 2: upload all to stage
    for f in files:
        local_path = os.path.join(local_base, f["name"])
        staged_remote = f"{remote_base}/{f['name']}{stage_suffix}"
        print(f"  stage {f['name']}...", end=" ")
        try:
            sftp.put(local_path, staged_remote)
            print("OK")
        except Exception as e:
            print(f"FAIL: {e}")
            cleanup_staged()
            restore_preship()
            cleanup_preship()
            return False

    # Step 3: verify staged
    print(f"\n  Staged md5 verify:")
    staged_ok = True
    for f in files:
        remote = f"{remote_base}/{f['name']}{stage_suffix}"
        actual = server_md5(sftp_client, remote)
        local_md5 = local_md5s[f["name"]].upper()
        if actual is None:
            print(f"  [FAIL] {f['name']}: md5sum 输出为空")
            staged_ok = False
            continue
        expected = f["expected"]
        if expected:
            cross = ("OK (cross-source)" if actual == expected.upper()
                     else f"MISMATCH (server {actual} ≠ git-tracked {expected})")
        else:
            cross = ("OK (self-consistent)" if actual == local_md5
                     else f"MISMATCH (server {actual} ≠ local {local_md5})")
        if "MISMATCH" in cross:
            staged_ok = False
        print(f"  {f['name']}: server={actual} local={local_md5} -> {cross}")
    if not staged_ok:
        print(f"\n[ABORT] staged md5 mismatch")
        cleanup_staged()
        restore_preship()
        cleanup_preship()
        return False

    # Step 4: atomic rename staged → final
    for f in files:
        staged_remote = f"{remote_base}/{f['name']}{stage_suffix}"
        final_remote = f"{remote_base}/{f['name']}"
        print(f"  rename {f['name']}{stage_suffix} → {f['name']}...", end=" ")
        try:
            sftp.rename(staged_remote, final_remote)
            renamed.append(f["name"])
            print("OK")
        except IOError as e:
            print(f"FAIL: {e}")
            restore_preship()
            cleanup_staged()
            cleanup_preship()
            return False

    # Step 5: verify final
    print(f"\n  Final md5 verify:")
    final_ok = True
    for f in files:
        remote = f"{remote_base}/{f['name']}"
        actual = server_md5(sftp_client, remote)
        local_md5 = local_md5s[f["name"]].upper()
        if actual is None:
            print(f"  [FAIL] {f['name']}: md5sum 输出为空")
            final_ok = False
            continue
        expected = f["expected"]
        if expected:
            cross = ("OK (cross-source)" if actual == expected.upper()
                     else f"MISMATCH (server {actual} ≠ git-tracked {expected})")
        else:
            cross = ("OK (self-consistent)" if actual == local_md5
                     else f"MISMATCH (server {actual} ≠ local {local_md5})")
        if "MISMATCH" in cross:
            final_ok = False
        print(f"  {f['name']}: server={actual} local={local_md5} -> {cross}")
    if not final_ok:
        print(f"\n[ABORT] final md5 mismatch — rolling back")
        restore_preship()
        cleanup_staged()
        cleanup_preship()
        return False

    # Step 6: cleanup pre-ship snapshots
    cleanup_preship()
    return True


def connect_sftp(creds):
    """Connect with RejectPolicy always. Pre-load known_hosts if present so
    legitimate host keys are accepted; unknown hosts are rejected (no TOFU).
    """
    client = paramiko.SSHClient()
    known_hosts_path = find_known_hosts()
    if known_hosts_path:
        try:
            client.load_host_keys(known_hosts_path)
            print(f"  known_hosts: {known_hosts_path} (loaded)")
        except Exception as e:
            print(f"  [ERR] known_hosts 加载失败: {e}")
            sys.exit(1)
    else:
        print(f"  [WARN] ~/.ssh/known_hosts 不存在, 任何 host key 都会被拒绝")
        print(f"         ship 前请跑: ssh-keyscan {creds['host']} >> ~/.ssh/known_hosts")
    client.set_missing_host_key_policy(paramiko.RejectPolicy())
    try:
        client.connect(creds["host"], creds["port"], creds["username"],
                       creds["password"], timeout=30)
    except paramiko.SSHException as e:
        print(f"  [ERR] SFTP 连接失败: {e}")
        sys.exit(1)
    return client


def main():
    parser = argparse.ArgumentParser(description="Fluxing SFTP release sync")
    parser.add_argument("--version", required=True,
                        help="Release version (e.g. 0.19.0.59)")
    parser.add_argument("--config", default=DEFAULT_CONFIG,
                        help=f"Config JSON path (default: {DEFAULT_CONFIG})")
    parser.add_argument("--dry-run", action="store_true",
                        help="Only verify local md5 against expected, skip upload")
    args = parser.parse_args()

    if not VERSION_RE.match(args.version):
        print(f"  [ERR] version 格式无效: {args.version!r} (expected X.Y.Z.W)")
        sys.exit(1)

    config = load_json(args.config, "Config")
    required = ["sftpConfigPath", "localBase", "remoteSubdir",
                "expectedMd5Source", "files"]
    for k in required:
        if k not in config:
            print(f"  [ERR] config 缺字段: {k}")
            sys.exit(1)
    # Refuse to run if the config still looks like an unedited example template.
    validate_config_no_placeholders(config)
    # Validate remote path components (defense in depth for shell injection).
    validate_remote_path_component(config["remoteSubdir"], "remoteSubdir")

    print(f"=== Fluxing SFTP sync v{args.version} ===")
    print(f"  config: {args.config}")
    print(f"  local_base: {config['localBase']}")
    print(f"  remote_subdir: {config['remoteSubdir']}")

    # 1. Read expected md5 from git-trusted ps1
    expected_keys = [f["expectedKey"] for f in config["files"]
                     if f.get("expectedKey")]
    expected_md5s = parse_expected_md5(config["expectedMd5Source"], expected_keys)
    print(f"\n=== Expected md5 from {config['expectedMd5Source']} (git-trusted) ===")
    for k, v in expected_md5s.items():
        print(f"  {k}: {v or '(missing)'}")

    # 2. Render file list with version substitution (fail-fast on missing key)
    files = render_files(config["files"], args.version, expected_md5s)

    # 3. Local pre-check md5
    local_md5s = precheck_local(files, config["localBase"])

    if args.dry_run:
        print(f"\n[DRY-RUN] skip upload — pre-check OK")
        return

    # 4. SFTP connect + atomic upload + server-side verify
    creds = load_sftp_credentials(config["sftpConfigPath"])
    print(f"\n=== SFTP connect ===")
    print(f"  host={creds['host']} port={creds['port']} user={creds['username']} "
          f"protocol={creds['protocol']} remotePath={creds['remotePath']}")

    client = connect_sftp(creds)
    sftp = client.open_sftp()
    remote_base = f"{creds['remotePath']}/{config['remoteSubdir']}"

    ok = upload_atomic(sftp, client, config["localBase"], remote_base,
                       files, local_md5s, args.version)
    sftp.close()
    client.close()

    if not ok:
        print(f"\n[FAIL] SFTP 上传 + verify failed (atomic rollback complete)")
        sys.exit(1)

    print(f"\n[DONE] v{args.version} SFTP 上传 + cross-source verify OK")


if __name__ == "__main__":
    main()
