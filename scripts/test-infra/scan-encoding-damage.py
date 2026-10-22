#!/usr/bin/env python3
"""Scan tracked documents for encoding damage and try to recover the lost text
from earlier git revisions.

Damage model (measured 2026-10-22 on this repo):

  * ``U+FFFD`` (bytes ``EF BF BD``) - the writer re-encoded text it could not
    decode, replacing every undecodable character with the replacement char.
    The original characters are **not** recoverable from the damaged file.
  * ``NUL`` (byte ``0x00``) - a character was replaced by a zero byte. The
    position is known, the character is not.

Both are concentrated in a handful of blocks that were written once, in a
single bad-encoding write. Everything written *before* that write is intact
in git history, and the damaged blocks can be matched back to their clean
predecessor lines by the ASCII fragments that survived the corruption.

Usage
-----
    python scripts/test-infra/scan-encoding-damage.py --path <file> [--apply]
    python scripts/test-infra/scan-encoding-damage.py --path <file> --report out.md

Exit code is non-zero when damage is still present, so it can be used as a
gate (per L40 / L44: judge encoding health by bytes, never by eyeballing the
console - a GBK console renders valid UTF-8 CJK as mojibake).
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

REPLACEMENT = "\ufffd"
NUL = "\x00"
PRINTABLE = re.compile(r"[\x20-\x7e]{8,}")
MIN_ANCHOR = 8


def git(repo: str, *args: str) -> bytes:
    out = subprocess.run(
        ["git", "-C", repo, *args],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    return out.stdout


def is_damaged(line: str) -> bool:
    """Primary damage: a character was replaced by U+FFFD or NUL.

    Secondary damage: the line is UTF-8 that was read as GBK and written back
    as UTF-8 (``§`` -> ``搂``, ``U+FFFD`` -> ``锟?``).  This form carries no
    U+FFFD at all, so a byte scan for ``EF BF BD`` alone under-reports the
    damage.  It is detected by a GBK round-trip: a genuinely clean Chinese
    line encodes to GBK bytes that are *not* valid UTF-8, so the round trip
    fails; a GBK-misread line round-trips back to its original UTF-8.
    """
    if REPLACEMENT in line or NUL in line:
        return True
    return gbk_misread(line) is not None


def gbk_misread(line: str) -> str | None:
    """Return the original text if `line` is a GBK-misread of UTF-8."""
    if line.isascii():
        return None
    try:
        raw = line.encode("gb18030")
    except UnicodeEncodeError:
        return None
    try:
        out = raw.decode("utf-8")
    except UnicodeDecodeError:
        return None
    if out == line:
        return None
    return out


def anchors_of(line: str) -> list[str]:
    """Surviving ASCII fragments - the fingerprint of a corrupted line."""
    found = [m.group(0) for m in PRINTABLE.finditer(line)]
    return [a for a in found if len(a) >= MIN_ANCHOR]


def revisions(repo: str, path: str) -> list[str]:
    raw = git(repo, "log", "--format=%H", "--", path).decode("utf-8", "replace")
    return [line.strip() for line in raw.splitlines() if line.strip()]


def historical_lines(repo: str, path: str, rev: str) -> list[str] | None:
    try:
        blob = git(repo, "cat-file", "blob", f"{rev}:{path}")
    except subprocess.CalledProcessError:
        return None
    return blob.decode("utf-8", "replace").splitlines()


def build_index(
    corpus: list[tuple[str, list[str]]]
) -> dict[str, list[tuple[str, str, list[str]]]]:
    """anchor -> [(rev, historical line, that line's anchors)].

    Indexing by the longest surviving ASCII fragment keeps the lookup O(1) per
    damaged line instead of scanning every historical line for every damaged
    line (which was ~150 x 1M string comparisons).
    """
    index: dict[str, list[tuple[str, str, list[str]]]] = {}
    for rev, lines in corpus:
        for line in lines:
            if is_damaged(line):
                continue
            a = anchors_of(line)
            if not a:
                continue
            key = max(a, key=len)
            index.setdefault(key, []).append((rev, line, a))
    return index


def best_match(
    target: str,
    index: dict[str, list[tuple[str, str, list[str]]]],
) -> tuple[str, str, float] | None:
    """Find the clean historical line that matches the most surviving anchors."""
    anchors = anchors_of(target)
    if not anchors:
        return None
    ranked = sorted(anchors, key=len, reverse=True)
    best: tuple[str, str, float] | None = None
    best_score = -1.0
    for key in ranked:
        for rev, line, hist_anchors in index.get(key, ()):
            hit = sum(1 for a in anchors if a in line)
            if hit == 0:
                continue
            score = hit / len(anchors)
            if score > best_score:
                best_score = score
                best = (rev, line, score)
        if best_score >= 1.0:
            break
    if best_score < 0.5:
        return None
    return best


def partial_match(
    target: str,
    index: dict[str, list[tuple[str, str, list[str]]]],
) -> tuple[str, float] | None:
    """Best partial historical line - used to brief the human rewriter."""
    anchors = anchors_of(target)
    if not anchors:
        return None
    best: tuple[str, float] | None = None
    best_score = 0.0
    for key in sorted(anchors, key=len, reverse=True):
        for _rev, line, _ha in index.get(key, ()):
            hit = sum(1 for a in anchors if a in line)
            if hit == 0:
                continue
            score = hit / len(anchors)
            if score > best_score:
                best_score = score
                best = (line, score)
        if best_score >= 0.99:
            break
    return best


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--path", required=True, help="repo-relative path")
    ap.add_argument("--repo", default=".")
    ap.add_argument("--apply", action="store_true", help="write high-confidence fixes")
    ap.add_argument(
        "--apply-roundtrip",
        action="store_true",
        help="repair GBK-misread lines (mechanically reversible damage)",
    )
    ap.add_argument("--report", help="write a markdown report here")
    ap.add_argument("--min-score", type=float, default=0.99)
    args = ap.parse_args()

    repo = str(Path(args.repo).resolve())
    target_path = Path(repo) / args.path
    if not target_path.is_file():
        print(f"not found: {target_path}", file=sys.stderr)
        return 2

    raw = target_path.read_bytes()
    text = raw.decode("utf-8", "replace")
    # split on LF only - str.splitlines() would also break on \x0b \x0c \x85
    # U+2028 U+2029, which shifts every line number away from what an editor
    # (and git) reports.
    lines = text.split("\n")
    stripped = [line[:-1] if line.endswith("\r") else line for line in lines]

    damaged_idx = [i for i, line in enumerate(stripped) if is_damaged(line)]

    revs = revisions(repo, args.path)
    corpus: list[tuple[str, list[str]]] = []
    for rev in revs:
        hist = historical_lines(repo, args.path, rev)
        if hist:
            corpus.append((rev, hist))
    corpus.reverse()  # oldest first - prefer the earliest clean wording
    index = build_index(corpus)

    fixes: list[tuple[int, str, str, float, str]] = []
    unmatched: list[tuple[int, str, str | None, float]] = []
    for idx in damaged_idx:
        match = best_match(stripped[idx], index)
        if match is None:
            partial = partial_match(stripped[idx], index)
            unmatched.append(
                (idx, stripped[idx], partial[0] if partial else None, partial[1] if partial else 0.0)
            )
            continue
        rev, line, score = match
        if score < args.min_score:
            unmatched.append((idx, stripped[idx], line, score))
            continue
        fixes.append((idx, stripped[idx], line, score, rev))

    out_lines = [
        f"# encoding damage report - {args.path}",
        "",
        f"- revisions scanned: {len(corpus)}",
        f"- damaged lines: {len(damaged_idx)}",
        f"- recoverable (score >= {args.min_score}): {len(fixes)}",
        f"- needs human rewrite: {len(unmatched)}",
        "",
    ]
    for idx, cur, new, score, rev in fixes:
        out_lines += [
            f"## line {idx + 1}  (score {score:.2f}, from {rev[:10]})",
            "",
            "- damaged: `" + cur.replace(REPLACEMENT, "�").replace(NUL, "␀") + "`",
            "- clean:   `" + new + "`",
            "",
        ]
    for idx, cur, cand, score in unmatched:
        out_lines += [
            f"## line {idx + 1}  (NO CLEAN MATCH - human rewrite required)",
            "",
            "- damaged: `" + cur.replace(REPLACEMENT, "�").replace(NUL, "␀") + "`",
        ]
        if cand:
            out_lines.append(
                f"- closest historical line (partial {score:.2f}): `" + cand + "`"
            )
        out_lines.append("")

    # Second-generation (GBK-misread) lines: mechanically repairable.
    roundtrip_lines = []
    for idx, line in enumerate(stripped):
        if REPLACEMENT in line or NUL in line:
            continue
        fixed = gbk_misread(line)
        if fixed is not None:
            roundtrip_lines.append((idx, line, fixed))

    if roundtrip_lines:
        out_lines += [
            "## mechanically repairable (GBK round-trip)",
            "",
            f"count = {len(roundtrip_lines)}",
            "",
        ]
        for idx, before, after in roundtrip_lines:
            out_lines += [
                f"### line {idx + 1}",
                "",
                "- before: `" + before + "`",
                "- after:  `" + after + "`",
                "",
            ]

    report = "\n".join(out_lines)
    if args.report:
        Path(args.report).write_text(report, encoding="utf-8")
    else:
        print(report)

    if args.apply_roundtrip and roundtrip_lines:
        for idx, _before, after in roundtrip_lines:
            eol = "\r" if lines[idx].endswith("\r") else ""
            lines[idx] = after + eol
        out = "\n".join(lines)
        target_path.write_bytes(out.encode("utf-8"))
        print(
            f"round-trip repaired {len(roundtrip_lines)} line(s)", file=sys.stderr
        )

    if args.apply and fixes:
        for idx, _cur, new, _score, _rev in fixes:
            eol = "\r\n" if lines[idx].endswith("\r") else ""
            lines[idx] = new + eol
        out = "\n".join(lines)
        target_path.write_bytes(out.encode("utf-8"))
        print(f"applied {len(fixes)} recovered line(s)", file=sys.stderr)

    return 1 if damaged_idx else 0


if __name__ == "__main__":
    sys.exit(main())
