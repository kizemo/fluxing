#!/usr/bin/env python3
"""Provenance and coverage audit for the spec 078 rewrite.

Three questions, one run:

1. Coverage   -- of the lines that were damaged in the pre-repair revision,
                  is every single one covered by the rewrite map?
2. Provenance -- for every mapped line, where does the new text come from?
                   EXACT   the new text is a verbatim line of a clean baseline
                   PARTIAL the new text is a substring of a clean baseline line
                   SELF    the new text is de-coded from the damaged line itself
                   EMPTY   the line was deliberately blanked (the paragraph
                           already exists on a neighbouring line)
                   NEW     no clean historical source -- a manual decision
3. Duplicates -- does any mapped, non-empty line repeat a paragraph that
                  already sits on a line within +/-3 lines?  Such a line has
                  to be blanked, otherwise the document grows a duplicate
                  paragraph.

The clean baselines are the last revisions that carry no damage:

    0c0716c36a   clean parent before the FFFD corruption
    879cb2e914   last fully pristine revision of lessons-learned.md

Usage:
    python scripts/test-infra/verify-rewrite-provenance.py
    python scripts/test-infra/verify-rewrite-provenance.py --before-rev 7c094b8
    python scripts/test-infra/verify-rewrite-provenance.py --write-report out.md

Exit code 0 means: every damaged line is covered and no duplicate fragment
is left.  A NEW line is reported but does not fail the run -- those are the
lines that were written after the corruption point and had to be rewritten by
hand.
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SPEC_DIR = ".specify/specs/078-repair-encoding-corruption"
TARGET_REL = ".specify/memory/lessons-learned.md"
MAP_REL = SPEC_DIR + "/rewrite-map-lessons-learned.json"

BASELINES = [
    ("0c0716c36a", "clean parent before the FFFD corruption"),
    ("879cb2e914", "last fully pristine revision"),
]
WINDOW = 3

# A control character in the damaged line usually CONSUMED the letter that
# formed the C escape with the vanished backslash:
#   "\accept:"  -> BEL + "ccept:"  -> restoring puts the 'a' back
#   "\build"    -> BS  + "uild"
#   "\verbose"  -> VT  + "erbose"
#   "\false"    -> FF  + "alse"
ESCAPE_LETTER = {0x07: "a", 0x08: "b", 0x0B: "v", 0x0C: "f"}


def git_bytes(rev: str, rel: str) -> bytes | None:
    r = subprocess.run(["git", "-C", str(REPO), "cat-file", "blob",
                        "{}:{}".format(rev, rel)],
                       stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    return r.stdout if r.returncode == 0 else None


def git_log_commits(rel: str, limit: int) -> list[str]:
    r = subprocess.run(["git", "-C", str(REPO), "log", "--format=%H", "-n", str(limit), "--", rel],
                       stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    if r.returncode != 0:
        return []
    return [x.strip() for x in r.stdout.decode("utf-8", "replace").split("\n") if x.strip()]


def resolve_before_rev(target: str, before_rev: str) -> tuple[str, str]:
    """Return (revision holding the damaged file, human readable label).

    With "auto" the damaged state is the parent of the newest commit that
    touched the file.  The rule ignores the rewrite map on purpose -- picking
    a baseline by the changed set would let the map confirm itself.
    """
    if before_rev and before_rev != "auto":
        return before_rev, before_rev
    for sha in git_log_commits(target, 30):
        parent = sha + "^"
        if git_bytes(parent, target) is not None:
            return parent, parent + " (parent of the newest commit touching the target)"
    return "HEAD", "HEAD"


def damaged_flags(line: str) -> tuple[int, int, int]:
    return (line.count("\ufffd"),
            line.count("\x00"),
            sum(1 for ch in line if ord(ch) < 0x20 and ch not in "\t\r"))


def mark(line: str) -> str:
    out = []
    for ch in line:
        o = ord(ch)
        if ch == "\ufffd":
            out.append("[F]")
        elif ch == "\x00":
            out.append("[N]")
        elif o < 0x20 and ch not in "\t\r":
            out.append("<%02X>" % o)
        else:
            out.append(ch)
    return "".join(out)


def core(s: str) -> str:
    """Normalise for comparison: drop whitespace and punctuation, unify the
    full width brackets, so a fragment matches its covering line even when the
    two differ in bracket style or trailing punctuation."""
    drop = set("，。、；：？！…—·（）()[]{}<>\"'`~!@#$%^&*-_=+|\\/,.")
    t = s.replace("（", "(").replace("）", ")")
    t = "".join(ch for ch in t if not ch.isspace())
    t = t.rstrip("。.")
    return "".join(ch for ch in t if ch not in drop)


def aligned(old: str, new: str) -> bool:
    """Walk `old` against `new` with backtracking.

    A control character in `old` swallowed one character of the original
    text -- the letter that formed the C escape with the vanished backslash.
    Restoring it may yield either the bare letter ("\\accept:" -> BEL +
    "ccept:" -> "accept:") or the backslash plus the letter ("Program
    Files\\fluxing" -> FF + "luxing" -> "...\\fluxing").  A control character
    may also have simply been deleted.

    U+FFFD in `old` marks a lost CJK character and is skipped, so a line
    whose Chinese text was destroyed can never align -- which is exactly the
    signal that it needs a human decision.
    """
    memo: dict[tuple[int, int], bool] = {}

    def walk(i: int, j: int) -> bool:
        key = (i, j)
        if key in memo:
            return memo[key]
        if i == len(old):
            memo[key] = (j == len(new))
            return memo[key]
        ch = old[i]
        if ch == "\ufffd":
            res = walk(i + 1, j)
        elif ord(ch) < 0x20 and ch not in "\t":
            letter = ESCAPE_LETTER.get(ord(ch))
            res = False
            if letter is not None:
                for repl in (letter, "\\" + letter):
                    if new.startswith(repl, j) and walk(i + 1, j + len(repl)):
                        res = True
                        break
            if not res:
                res = walk(i + 1, j)
        else:
            res = (j < len(new) and new[j] == ch) and walk(i + 1, j + 1)
        memo[key] = res
        return res

    return walk(0, 0)


def main() -> int:
    sys.stdout.reconfigure(encoding="ascii", errors="backslashreplace")
    ap = argparse.ArgumentParser()
    ap.add_argument("--before-rev", default="auto",
                    help="revision holding the damaged file (default: auto)")
    ap.add_argument("--map", default=MAP_REL, help="rewrite map (repo relative)")
    ap.add_argument("--target", default=TARGET_REL, help="target file (repo relative)")
    ap.add_argument("--write-report", default=None,
                    help="also write the per-line report to this path")
    args = ap.parse_args()

    map_path = REPO / args.map
    if not map_path.exists():
        print("missing map:", args.map)
        return 1
    final = json.loads(map_path.read_bytes().decode("utf-8"))["lines"]

    rev, rev_label = resolve_before_rev(args.target, args.before_rev)
    raw = git_bytes(rev, args.target)
    if raw is None:
        print("cannot read revision", rev)
        return 1
    cur = [x.rstrip("\r") for x in raw.decode("utf-8", "replace").split("\n")]

    damaged = [i for i, l in enumerate(cur, start=1) if any(damaged_flags(l))]

    def final_text(n: int) -> str:
        mv = final.get(str(n))
        if mv is not None:
            return mv
        if 1 <= n <= len(cur):
            return cur[n - 1]
        return ""

    bases = []
    for sha, note in BASELINES:
        blob = git_bytes(sha, args.target)
        if blob is None:
            print("WARN: cannot read baseline", sha)
            continue
        bl = blob.decode("utf-8", "replace").split("\n")
        bases.append((sha, note, bl, set(x.rstrip("\r") for x in bl if x.strip())))

    stats = {"EXACT": 0, "PARTIAL": 0, "SELF": 0, "EMPTY": 0, "NEW": 0}
    rows = []
    for k in sorted(final, key=lambda x: int(x)):
        n = int(k)
        new = final[k]
        old = cur[n - 1] if n <= len(cur) else ""
        if new.strip() == "":
            kind = "EMPTY"
            where = "deliberate blank (fragment absorbed by a recovered neighbour)"
        else:
            hit = None
            for sha, note, bl, bset in bases:
                if new in bset:
                    hit = (sha, note, "exact line match")
                    break
            if hit is not None:
                kind = "EXACT"
                where = "verbatim line in {} ({})".format(hit[0], hit[2])
            else:
                sub = None
                for sha, note, bl, bset in bases:
                    for bl_line in bl:
                        if bl_line.strip() and new in bl_line:
                            sub = (sha, note, "substring of a baseline line")
                            break
                    if sub:
                        break
                if sub:
                    kind = "PARTIAL"
                    where = "substring of a baseline line in {} ({})".format(sub[0], sub[2])
                elif aligned(old, new):
                    kind = "SELF"
                    where = "derived from the damaged line itself (control-char / escape de-coding)"
                else:
                    kind = "NEW"
                    where = "no clean historical source -- written after the corruption point"
        stats[kind] += 1
        rows.append((n, kind, old, new, where))

    uncovered = [n for n in damaged if str(n) not in final]
    extra = [int(k) for k in final if int(k) not in damaged]

    dups = []
    for k in sorted(final, key=lambda x: int(x)):
        n = int(k)
        txt = final[k]
        if not txt.strip():
            continue
        probe = core(txt)
        if len(probe) < 3:
            continue
        for m in range(max(1, n - WINDOW), min(len(cur), n + WINDOW) + 1):
            if m == n:
                continue
            other = final_text(m)
            if not other.strip():
                continue
            if probe in core(other):
                dups.append((n, m, txt, other))
                break

    print("target            : {}".format(args.target))
    print("map               : {} ({} entries)".format(args.map, len(final)))
    print("damaged revision  : {}".format(rev_label))
    print("damaged lines     : {}".format(len(damaged)))
    print("uncovered         : {}".format(len(uncovered)))
    if uncovered:
        print("  -> {}".format(",".join(str(n) for n in uncovered)))
    print("mapped-not-damaged: {}  (invisible damage: the bytes look clean)".format(len(extra)))
    for k in ("EXACT", "PARTIAL", "SELF", "EMPTY", "NEW"):
        print("  {:<8} {}".format(k, stats[k]))
    print("duplicate fragments: {}".format(len(dups)))
    for n, m, _t, _o in dups:
        print("  L{} -> L{}".format(n, m))

    if args.write_report:
        out = ["# rewrite provenance report", ""]
        out.append("- target: `{}`".format(args.target))
        out.append("- map: `{}` ({} entries)".format(args.map, len(final)))
        out.append("- damaged revision: `{}`".format(rev))
        for sha, note, bl, _ in bases:
            out.append("- baseline `{}`: {} ({} lines)".format(sha, note, len(bl)))
        out.append("")
        out.append("| class | count | meaning |")
        out.append("|---|---|---|")
        out.append("| EXACT | {} | verbatim line found in a clean baseline |".format(stats["EXACT"]))
        out.append("| PARTIAL | {} | proposed text is a substring of a clean baseline line |".format(stats["PARTIAL"]))
        out.append("| SELF | {} | de-coded from the damaged line itself |".format(stats["SELF"]))
        out.append("| EMPTY | {} | deliberately blanked fragment |".format(stats["EMPTY"]))
        out.append("| NEW | {} | no source at all -- needs a manual context decision |".format(stats["NEW"]))
        out.append("")
        out.append("## coverage")
        out.append("")
        out.append("- damaged lines: {}".format(len(damaged)))
        out.append("- uncovered damaged lines: {}".format(len(uncovered)))
        out.append("- mapped but not damaged (invisible damage): {}".format(len(extra)))
        out.append("")
        out.append("## duplicate fragments")
        out.append("")
        for n, m, txt, other in dups:
            out.append("### line {} duplicates line {}".format(n, m))
            out.append("")
            out.append("- THIS : `{}`".format(txt))
            out.append("- OTHER: `{}`".format(other))
            out.append("")
        out.append("## detail")
        out.append("")
        for n, kind, old, new, where in rows:
            out.append("### line {} [{}]".format(n, kind))
            out.append("")
            out.append("- OLD: `{}`".format(mark(old)))
            out.append("- NEW: `{}`".format(mark(new) if kind != "EMPTY" else "(blank)"))
            out.append("- provenance: {}".format(where))
            out.append("")
        Path(args.write_report).write_bytes("\n".join(out).encode("utf-8"))
        print("report            : {}".format(args.write_report))

    return 0 if (not uncovered and not dups) else 1


if __name__ == "__main__":
    sys.exit(main())
