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
import difflib
import json
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SPEC_DIR = ".specify/specs/078-repair-encoding-corruption"
# The scanner lives in the repository that owns this script, not in the
# repository that is being checked - otherwise a run against another tree
# cannot even find the tool (measured 2026-10-23: the scanner was reported
# as "exit 2", which is Python's "cannot open file", not a verdict).
TOOL_REPO = Path(__file__).resolve().parents[2]


def git_blob(rev: str, rel: str) -> bytes | None:
    r = subprocess.run(["git", "-C", str(REPO), "cat-file", "blob", "{}:{}".format(rev, rel)],
                       stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    return r.stdout if r.returncode == 0 else None


def git_log_commits(rel: str, limit: int) -> list[str]:
    r = subprocess.run(["git", "-C", str(REPO), "log", "--format=%H", "-n", str(limit), "--", rel],
                       stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    if r.returncode != 0:
        return []
    return [x.strip() for x in r.stdout.decode("utf-8", "replace").split("\n") if x.strip()]


def resolve_baseline(target: str, before_rev: str) -> tuple[bytes | None, str]:
    """Return the pre-repair blob and the revision label it came from.

    The baseline is the state the repair started from.  With
    ``--before-rev auto`` that is HEAD whenever the working tree still
    differs from it: the change under review is then the uncommitted
    repair, and every line it touched is a line the map has to declare.
    When the working tree matches HEAD the repair is already committed and
    the newest commit itself is what gets verified, so its parent is the
    baseline.

    The rule is deliberately independent of the rewrite map: picking a
    baseline by "the changed set equals the map" would let the map confirm
    itself.
    """
    if before_rev and before_rev != "auto":
        return git_blob(before_rev, target), before_rev
    head = git_blob("HEAD", target)
    work_path = REPO / target
    if head is not None and work_path.is_file():
        if work_path.read_bytes() != head:
            return head, "HEAD"
    for sha in git_log_commits(target, 30):
        parent = sha + "^"
        blob = git_blob(parent, target)
        if blob is not None:
            return blob, parent
    return head, "HEAD"


def main() -> int:
    global REPO
    ap = argparse.ArgumentParser()
    ap.add_argument("--target", default=".specify/memory/lessons-learned.md")
    ap.add_argument("--map",
                    default=SPEC_DIR + "/rewrite-map-lessons-learned.json")
    ap.add_argument("--before-rev", default="auto",
                    help="revision holding the pre-repair state; 'auto' picks the "
                         "parent of the newest commit that touched the target "
                         "(default: auto)")
    ap.add_argument("--repo", default=str(REPO),
                    help="repository root (default: inferred from this script)")
    args = ap.parse_args()
    REPO = Path(args.repo).resolve()

    sys.stdout.reconfigure(encoding="ascii", errors="backslashreplace")
    raw = (REPO / args.target).read_bytes()
    before, before_rev = resolve_baseline(args.target, args.before_rev)
    problems = []
    changed: set[int] = set()
    insert_blocks: list[tuple[int, int]] = []
    delete_blocks: list[tuple[int, int]] = []
    cr_delta = 0

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
            before_rev, b_lines, b_cr, b_bom))
        if raw.startswith(b"\xef\xbb\xbf") != b_bom:
            problems.append("BOM state changed (before {}, now {})".format(
                b_bom, raw.startswith(b"\xef\xbb\xbf")))
        # The baseline and the current file are aligned with difflib, not by
        # line position.  A commit that appends a release section shifts every
        # following line number, and a positional comparison then reports the
        # whole tail as rewritten -- measured 2026-10-23, the CHANGELOG gate
        # failed with 5497 undeclared lines right after the 0.21.0.2 section
        # had been added.
        #
        # The line numbers of a rewrite map refer to the file *before* the
        # rewrite (that is the contract of apply-line-rewrites.py), so the
        # comparison is made on the baseline side of the alignment.
        # A line that was rewritten from scratch does not always pair up in
        # the opcode stream, so the opcodes are grouped: a delete next to an
        # insert is one rewrite, not a deletion plus an addition.
        b_lines_list = before.decode("utf-8", "replace").split("\n")
        matcher = difflib.SequenceMatcher(
            a=b_lines_list, b=lines, autojunk=False)
        # Consecutive non-equal opcodes form one rewrite region.  A line that
        # was rewritten from scratch does not always pair up, so a region can
        # hold a delete next to an insert; what matters is that the region
        # replaces as many baseline lines as it adds.
        segments = []
        current: list[tuple[str, int, int, int, int]] = []
        for op in matcher.get_opcodes():
            if op[0] == "equal":
                if current:
                    segments.append(current)
                    current = []
            else:
                current.append(op)
        if current:
            segments.append(current)

        total_del = 0
        total_ins = 0
        all_insert_ranges: list[tuple[int, int]] = []

        for segment in segments:
            base_side: list[int] = []
            del_count = 0
            ins_count = 0
            ins_ranges: list[tuple[int, int]] = []
            cur_cr = 0
            base_cr = 0
            for tag, i1, i2, j1, j2 in segment:
                if tag in ("replace", "delete"):
                    base_side.extend(range(i1 + 1, i2 + 1))
                    base_cr += sum(b_lines_list[i].count("\r")
                                   for i in range(i1, i2))
                if tag == "delete":
                    del_count += i2 - i1
                if tag == "insert":
                    ins_count += j2 - j1
                    ins_ranges.append((j1 + 1, j2 - j1))
                cur_cr += sum(lines[j].count("\r") for j in range(j1, j2))
            changed.update(base_side)
            cr_delta += cur_cr - base_cr
            all_insert_ranges.extend(ins_ranges)
            total_del += del_count
            total_ins += ins_count

        if raw.count(b"\n") != b_lines:
            # the repair changed the number of lines: the added and the
            # removed ones have to be declared, a rewrite that keeps the
            # line count does not.  (The totals of the diff opcodes cannot
            # be used for this: a replace block may map several baseline
            # lines onto a different number of lines.)
            insert_blocks.extend(all_insert_ranges)
            if total_del > total_ins:
                delete_blocks.append((1, total_del - total_ins))
        # A CR byte can itself be the damage: the ``\r`` escape was un-escaped
        # into a raw CR, which swallowed the letter that formed it.  So the CR
        # count is allowed to move by exactly the CR bytes the changed lines
        # remove, and by nothing else.
        if raw.count(b"\r") != b_cr + cr_delta:
            problems.append("CR count changed ({} -> {}, expected {})".format(
                b_cr, raw.count(b"\r"), b_cr + cr_delta))
    else:
        problems.append("cannot read the pre-repair revision {}".format(before_rev))

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
        ["python", str(TOOL_REPO / "scripts" / "test-infra" / "scan-encoding-damage.py"),
         "--repo", str(REPO), "--path", args.target],
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
        payload = json.loads(map_path.read_bytes().decode("utf-8"))
        declared = set(payload["lines"].keys())
        declared_int = {int(k) for k in declared}
        print("map lines  : {}".format(len(declared_int)))
        if declared_int:
            print("map range  : {}..{}".format(min(declared_int), max(declared_int)))
        if before is not None:
            print("rewritten  : {} baseline line(s)".format(len(changed)))
            print("inserted   : {} block(s)".format(len(insert_blocks)))
            print("deleted    : {} block(s)".format(len(delete_blocks)))
            undeclared = sorted(changed - declared_int)
            if undeclared:
                problems.append("{} line(s) changed but were not declared: {}".format(
                    len(undeclared), undeclared[:20]))
            missing = sorted(declared_int - changed)
            if missing:
                problems.append("{} declared line(s) did not change: {}".format(
                    len(missing), missing[:20]))
            declared_inserts = {tuple(x) for x in payload.get("inserts", [])}
            got_inserts = set(insert_blocks)
            if declared_inserts != got_inserts:
                problems.append(
                    "inserted block(s) {} do not match the declared {} "
                    "(every inserted block has to be listed in the map)".format(
                        sorted(got_inserts), sorted(declared_inserts)))
            declared_deletes = {tuple(x) for x in payload.get("deletes", [])}
            got_deletes = set(delete_blocks)
            if declared_deletes != got_deletes:
                problems.append(
                    "deleted block(s) {} do not match the declared {} "
                    "(every deleted block has to be listed in the map)".format(
                        sorted(got_deletes), sorted(declared_deletes)))
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
