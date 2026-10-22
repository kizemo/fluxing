#!/usr/bin/env python3
"""Acceptance gate for the spec 078 encoding repair.

Checks, in order:
  1. the file is strict UTF-8, and its line / CR counts and BOM state match
     the pre-repair snapshot taken from git (the BOM rule differs per file:
     lessons-learned.md carries a BOM, CHANGELOG.md does not, and neither
     may change);
  2. no U+FFFD, no NUL, no C0 control character (other than TAB) is left;
  3. the project's own scanner reports 0 damaged lines and exits 0;
  4. every line that changed is one that the rewrite map declared.

Exit code 0 means every check passed.
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SPEC_DIR = ".specify/specs/078-repair-encoding-corruption"


def git_blob(rev: str, rel: str) -> bytes | None:
    r = subprocess.run(["git", "-C", str(REPO), "cat-file", "blob", "{}:{}".format(rev, rel)],
                       stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    return r.stdout if r.returncode == 0 else None


def main() -> int:
    global REPO
    ap = argparse.ArgumentParser()
    ap.add_argument("--target", default=".specify/memory/lessons-learned.md")
    ap.add_argument("--map",
                    default=SPEC_DIR + "/rewrite-map-lessons-learned.json")
    ap.add_argument("--before-rev", default="HEAD",
                    help="revision holding the pre-repair state (default: HEAD)")
    ap.add_argument("--repo", default=str(REPO),
                    help="repository root (default: inferred from this script)")
    args = ap.parse_args()
    REPO = Path(args.repo).resolve()

    sys.stdout.reconfigure(encoding="ascii", errors="backslashreplace")
    raw = (REPO / args.target).read_bytes()
    before = git_blob(args.before_rev, args.target)
    problems = []

    # 1. encoding + shape
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError as exc:
        print("FAIL: not valid UTF-8: {}".format(exc))
        return 1
    lines = text.split("\n")
    print("target     : {}".format(args.target))
    print("bytes      : {}".format(len(raw)))
    print("lines      : {}".format(len(lines)))
    print("CR count   : {}".format(raw.count(b"\r")))
    print("LF count   : {}".format(raw.count(b"\n")))
    print("BOM        : {}".format(raw.startswith(b"\xef\xbb\xbf")))

    if before is not None:
        b_bom = before.startswith(b"\xef\xbb\xbf")
        b_lines = before.count(b"\n")
        b_cr = before.count(b"\r")
        print("before rev : {} (lines {}, CR {}, BOM {})".format(
            args.before_rev, b_lines, b_cr, b_bom))
        if raw.startswith(b"\xef\xbb\xbf") != b_bom:
            problems.append("BOM state changed (before {}, now {})".format(
                b_bom, raw.startswith(b"\xef\xbb\xbf")))
        if raw.count(b"\n") != b_lines:
            problems.append("line count changed ({} -> {})".format(b_lines, raw.count(b"\n")))
        if raw.count(b"\r") != b_cr:
            problems.append("CR count changed ({} -> {})".format(b_cr, raw.count(b"\r")))
    else:
        problems.append("cannot read the pre-repair revision {}".format(args.before_rev))

    # 2. damage markers
    fffd = text.count("\ufffd")
    nul = text.count("\x00")
    ctrl = {}
    for ch in text:
        o = ord(ch)
        if o < 0x20 and ch not in "\t\r\n":
            ctrl[o] = ctrl.get(o, 0) + 1
    print("U+FFFD     : {}".format(fffd))
    print("NUL        : {}".format(nul))
    print("C0 ctrl    : {}".format(ctrl if ctrl else 0))
    if fffd:
        problems.append("{} U+FFFD left".format(fffd))
    if nul:
        problems.append("{} NUL left".format(nul))
    if ctrl:
        problems.append("C0 control characters left: {}".format(ctrl))

    # 3. the project scanner
    r = subprocess.run(
        ["python", str(REPO / "scripts" / "test-infra" / "scan-encoding-damage.py"),
         "--path", args.target],
        cwd=str(REPO), stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    out = r.stdout.decode("utf-8", "replace")
    for line in out.splitlines():
        if line.startswith("- damaged lines") or line.startswith("- recoverable") \
                or line.startswith("- needs human") or line.startswith("damage-scan"):
            print("scanner    : {}".format(line.strip()))
    print("scanner exit: {}".format(r.returncode))
    if r.returncode != 0:
        problems.append("scanner exit code {}".format(r.returncode))

    # 4. only declared lines changed
    map_path = REPO / args.map
    if map_path.exists():
        declared = set(json.loads(map_path.read_bytes().decode("utf-8"))["lines"].keys())
        declared_int = {int(k) for k in declared}
        print("map lines  : {}".format(len(declared_int)))
        if declared_int:
            print("map range  : {}..{}".format(min(declared_int), max(declared_int)))
        if before is not None:
            b_lines_list = before.decode("utf-8", "replace").split("\n")
            changed = set()
            for i, (a, b) in enumerate(zip(b_lines_list, lines), start=1):
                if a.rstrip("\r") != b.rstrip("\r"):
                    changed.add(i)
            print("changed    : {} line(s)".format(len(changed)))
            undeclared = sorted(changed - declared_int)
            if undeclared:
                problems.append("{} line(s) changed but were not declared: {}".format(
                    len(undeclared), undeclared[:20]))
            missing = sorted(declared_int - changed)
            if missing:
                problems.append("{} declared line(s) did not change: {}".format(
                    len(missing), missing[:20]))
    else:
        problems.append("map file not found: {}".format(args.map))

    print("")
    if problems:
        for p in problems:
            print("PROBLEM: {}".format(p))
        return 1
    print("ALL CHECKS PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
