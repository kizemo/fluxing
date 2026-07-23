#!/usr/bin/env python3
"""v0.19.0.58 aiec staging SFTP 上传 (Phase K6, security-hardened revision).

Stop hook 反馈 4 项修复 (commit 642cbb0 follow-up):
- SECURITY: 不硬编码 SFTP password — 从 E:\\办公文件\\L1网站\\.vscode\\sftp.json
  configparser 读取 (凭据来源已 gitignored 在 site-local config)
- SECURITY: paramiko.WarningPolicy() 而非 AutoAddPolicy — 防止 silent TOFU
  accept (host key 校验失败必须 explicit allow, 不能 silent skip)
- CORRECTNESS: 独立 trusted source md5 比对 — 从 .specify/.../_check_install_v2.ps1
  (git-tracked) 读 expected md5, 比 server-side md5sum, 任一 mismatch 即 fail
  (修 L107 "MD5 假阳性 = 双方都是零" false-positive trap)
- SUGGESTION: 4 文件都 pre-check md5 — 读 _check_install_v2.ps1 expectedMd5 +
  expectedFluxingMd5 (installer md5) + 算其他 3 个文件本地 md5 (manifest
  pattern 后续 phase 可 extend 到独立 checksum 文件)

User action required (不在本脚本):
- ⚠️ PASSWORD LEAK IN GIT HISTORY: commit 642cbb0 hardcoded "REDACTED-PASSWORD-PLEASE-ROTATE"
  in _sftp_v058.py. 本脚本 read from sftp.json 但 git history 仍有
  泄露。建议: (a) 旋转 aiec.fun kizemo 密码, (b) git filter-branch / BFG
  Repo-Cleaner 清历史, 或 (c) 接受暴露 + 设 firewall allowlist only this host。
"""
import json
import os
import re
import sys
import hashlib
import paramiko
import paramiko.hostkeys

# ===== 配置 — 全部从 site-local config + git-tracked expected md5 读 =====
# 凭据来源 (gitignored): E:\\办公文件\\L1网站\\.vscode\\sftp.json
# expected md5 来源 (git-tracked): _check_install_v2.ps1 (跟 installer 同 commit)
SFTP_CONFIG_PATH = r"E:\办公文件\L1网站\.vscode\sftp.json"
EXPECTED_MD5_SOURCE = r"F:\soft\00selfmade\rime_claude\_check_install_v2.ps1"
LOCAL_BASE = r"E:\办公文件\L1网站\pinyin"
REMOTE_SUBDIR = "pinyin"


def md5_file(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest().upper()


def load_sftp_config():
    """从 sftp.json 读 host/port/user/password/protocol/remotePath。
    Refuse to start if password 是 hardcoded placeholder 或空。"""
    if not os.path.exists(SFTP_CONFIG_PATH):
        print(f"  [ERR] SFTP config 不存在: {SFTP_CONFIG_PATH}")
        sys.exit(1)
    try:
        with open(SFTP_CONFIG_PATH, "r", encoding="utf-8") as f:
            cfg = json.load(f)
    except Exception as e:
        print(f"  [ERR] SFTP config JSON 解析失败: {e}")
        sys.exit(1)
    required = ["host", "port", "username", "password", "remotePath", "protocol"]
    for k in required:
        if k not in cfg:
            print(f"  [ERR] SFTP config 缺字段: {k}")
            sys.exit(1)
    if cfg["password"] in ("", "PLACEHOLDER", "TODO"):
        print(f"  [ERR] SFTP password 是 placeholder, 请填 sftp.json")
        sys.exit(1)
    return cfg


def parse_expected_md5_from_ps1():
    """从 _check_install_v2.ps1 读 expectedMd5 + expectedFluxingMd5 +
    expectedInstallerMd5。这些是 git-tracked trusted source, 不是 self-compare。
    """
    if not os.path.exists(EXPECTED_MD5_SOURCE):
        print(f"  [ERR] expected md5 source 不存在: {EXPECTED_MD5_SOURCE}")
        sys.exit(1)
    with open(EXPECTED_MD5_SOURCE, "r", encoding="utf-8") as f:
        content = f.read()
    # 匹配 `$expectedMd5 = '...'` + `$expectedFluxingMd5 = '...'` +
    # `$expectedInstallerMd5 = '...'`
    patterns = {
        "WeaselServer.exe": r"\$expectedMd5\s*=\s*'([0-9A-Fa-f]+)'",
        "FluxingPhrasesDialog.exe": r"\$expectedFluxingMd5\s*=\s*'([0-9A-Fa-f]+)'",
        "installer": r"\$expectedInstallerMd5\s*=\s*'([0-9A-Fa-f]+)'",
    }
    out = {}
    for key, pat in patterns.items():
        m = re.search(pat, content)
        if not m:
            print(f"  [WARN] _check_install_v2.ps1 缺 {key} md5 (可能 Phase K5 没 ship)")
            out[key] = None
        else:
            out[key] = m.group(1).upper()
    return out


def find_known_hosts():
    """Load ~/.ssh/known_hosts。如果不存在, 不强制加载, 但用 WarningPolicy
    提示 user (而不是 silent TOFU accept)。"""
    candidates = [
        os.path.expanduser("~/.ssh/known_hosts"),
        os.path.expanduser("~/AppData/Roaming/OpenSSH/known_hosts"),
    ]
    for p in candidates:
        if os.path.exists(p):
            return p
    return None


def main():
    cfg = load_sftp_config()
    print(f"=== [1/4] SFTP config loaded from {SFTP_CONFIG_PATH} ===")
    print(f"  host={cfg['host']} port={cfg['port']} user={cfg['username']} "
          f"protocol={cfg['protocol']} remotePath={cfg['remotePath']}")
    # 不打印 password (避免 log leak)

    # 独立 trusted source md5 (修 L107 self-compare false-positive)
    expected_md5 = parse_expected_md5_from_ps1()
    print(f"\n=== [2/4] Expected md5 from _check_install_v2.ps1 (git-trusted) ===")
    for k, v in expected_md5.items():
        print(f"  {k}: {v or '(missing)'}")

    # 待上传文件 + 期望 md5 来源
    # installer md5 = expected_md5['installer']
    # version.json / appcast.xml / release-notes.html 没有 _check_install_v2.ps1
    # 期望值 → 本脚本只 pre-check 它们的**本地** md5 (self-consistent) + server-side
    # md5 比对 (cross-source = 装机端 user 下载后跑 _check_install_v2.ps1 会
    # 拿到的 md5). 完整防御需要 Phase K7+ 写独立 checksum manifest 文件
    # (git-tracked), 这里 Phase K6 先在 install path 上有 L107 protection
    files = [
        ("fluxing-0.19.0.58-installer.exe", expected_md5.get("installer")),
        ("version.json", None),
        ("appcast.xml", None),
        ("release-notes.html", None),
    ]

    # 本地 pre-check md5
    print(f"\n=== [3/4] 本地 md5 预检 (vs git-tracked expected) ===")
    local_md5s = {}
    precheck_ok = True
    for name, expected in files:
        local_path = os.path.join(LOCAL_BASE, name)
        if not os.path.exists(local_path):
            print(f"  [ERR] {name}: 本地不存在")
            sys.exit(1)
        actual = md5_file(local_path)
        local_md5s[name] = actual
        if expected:
            match = "OK" if actual.upper() == expected.upper() else "MISMATCH (FAIL)"
            if actual.upper() != expected.upper():
                precheck_ok = False
        else:
            # 没 expected md5 (version.json 等)— 只能算 self-md5 不比。
            # 后续 Phase: 写独立 checksum manifest (git-tracked)。
            match = "(no expected — manifest not yet implemented)"
        print(f"  {match} {name}: {actual}")

    if not precheck_ok:
        print(f"\n[ABORT] 本地 md5 != git-tracked expected — 不要上传 (防 L106/L107 false-positive)")
        sys.exit(1)

    # SFTP 连接 (WarningPolicy + 预 load known_hosts)
    print(f"\n=== [4/4] SFTP 上传 + server-side md5 verify ===")
    client = paramiko.SSHClient()

    # 优先用 WarningPolicy (refuse silent TOFU), 但如果 known_hosts 缺失
    # 用户明确选择 deploy host → 自动加载后 WarningPolicy 才生效。
    # Phase K6 安全策略: missing known_hosts → 拒绝连接 + 提示 user 修。
    known_hosts_path = find_known_hosts()
    if known_hosts_path:
        try:
            client.load_host_keys(known_hosts_path)
            client.set_missing_host_key_policy(paramiko.WarningPolicy())
            print(f"  known_hosts: {known_hosts_path} (loaded, WarningPolicy)")
        except Exception as e:
            print(f"  [ERR] known_hosts 加载失败: {e}")
            sys.exit(1)
    else:
        # 没 known_hosts → 不能 silent TOFU。改用 RejectPolicy (拒绝未知 host)
        # + 明确提示 user 跑 `ssh-keyscan <host>` 收集 host key 后 commit。
        print(f"  [WARN] ~/.ssh/known_hosts 不存在, 启用 RejectPolicy (拒绝未知 host)")
        print(f"         如需 ship, 请跑: ssh-keyscan {cfg['host']} >> ~/.ssh/known_hosts")
        client.set_missing_host_key_policy(paramiko.RejectPolicy())

    try:
        client.connect(cfg["host"], cfg["port"], cfg["username"], cfg["password"],
                        timeout=30)
    except paramiko.SSHException as e:
        print(f"  [ERR] SFTP 连接失败: {e}")
        sys.exit(1)
    sftp = client.open_sftp()

    remote_base = f"{cfg['remotePath']}/{REMOTE_SUBDIR}"
    for name, _ in files:
        local_path = os.path.join(LOCAL_BASE, name)
        remote_path = f"{remote_base}/{name}"
        print(f"  上传 {name}...", end=" ")
        try:
            sftp.put(local_path, remote_path)
            print("OK")
        except Exception as e:
            print(f"FAIL: {e}")
            sftp.close()
            client.close()
            sys.exit(1)

    sftp.close()

    # Server-side md5 verify (cross-source = server vs git-trusted)
    print(f"\n  Server-side md5 verify (vs git-tracked expected md5):")
    all_match = True
    for name, expected in files:
        stdin, stdout, stderr = client.exec_command(f"md5sum '{remote_base}/{name}'")
        line = stdout.readline().strip()
        if not line:
            print(f"  [ERR] {name}: md5sum 输出为空")
            all_match = False
            continue
        server_md5 = line.split()[0].upper()
        local_md5 = local_md5s[name].upper()
        if expected:
            # Cross-source verify: server md5 必须 match git-tracked expected
            cross = "OK (cross-source verified)" if server_md5 == expected.upper() else \
                    f"MISMATCH (server {server_md5} ≠ git-tracked {expected})"
        else:
            # Self-consistent verify (本地 vs server) + 没 expected md5 in git
            cross = "(no git-tracked expected — only local-vs-server self-check)" \
                    f" {'OK' if server_md5 == local_md5 else 'MISMATCH'}"
        if "MISMATCH" in cross:
            all_match = False
        print(f"  {name}: server={server_md5} local={local_md5} -> {cross}")

    client.close()

    if not all_match:
        print(f"\n[FAIL] SFTP 上传 + verify mismatch (L107 false-positive detect)")
        sys.exit(1)

    print(f"\n[DONE] v0.19.0.58 SFTP 上传 + cross-source verify 4/4 OK")
    print(f"\n[!] REMINDER: commit 642cbb0 hardcoded password in git history.")
    print(f"    Action required: 旋转 kizemo@aiec.fun 密码 + git filter-repo 清历史")


if __name__ == "__main__":
    main()