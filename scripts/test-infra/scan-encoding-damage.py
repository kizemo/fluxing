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
# ASCII runs of any length - used for structural alignment checks.
ASCII_RUN = re.compile(r"[\x20-\x7e]+")
# a path segment that still carries its separator, e.g. "test/TestX.cpp" or
# "tools\\build_verify.lua" - the shape a token is left in when the letter
# that started it was swallowed by an un-escaped "\t".
PATH_SEGMENT = re.compile(r"[A-Za-z0-9_.-]+[\\/]")

# The set of characters the file under inspection actually uses.  A GBK
# round trip is only accepted when the recovered text could plausibly be the
# original *for this document*; see _plausible().
_REPERTOIRE: set[str] | None = None


def set_repertoire(chars) -> None:
    """Publish the character repertoire of the file under inspection."""
    global _REPERTOIRE
    _REPERTOIRE = set(chars)


def _plausible(recovered: str) -> bool:
    """Could ``recovered`` be the original text of this document?

    A GBK misread turns clean UTF-8 into mojibake and the round trip brings
    the original back.  But a perfectly clean Chinese line can round trip as
    well, purely by coincidence: the GBK bytes of some characters are also
    valid UTF-8.  Measured 2026-10-23, the heading
    ``### 10.5 spec 037 spec / plan / tasks 状态`` round trips to
    ``### 10.5 spec 037 spec / plan / tasks ״̬`` and was reported
    as damaged although the file holds the correct UTF-8 (verified byte by
    byte: ``e7 8a b6 e6 80 81``).

    The tell is that the recovered text uses characters the document never
    contains anywhere else, so it cannot be what the author wrote.  Without
    a known repertoire the round trip is accepted, exactly as before.
    """
    if _REPERTOIRE is None:
        return True
    return all(ch in _REPERTOIRE or ch.isascii() for ch in recovered)


def git(repo: str, *args: str) -> bytes:
    out = subprocess.run(
        ["git", "-C", repo, *args],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    return out.stdout


def c0_controls(line: str) -> int:
    """Count C0 control characters other than TAB.

    A Markdown document has no legitimate C0 controls: every one of them is
    damage.  They appear when a C-style escape sequence (``\\a``, ``\\b``,
    ``\\v``, ``\\f``) was un-escaped into a real control byte, which also
    swallowed the letter that had formed the escape (``\\accept:`` became
    BEL + "ccept:").  A byte scan that only looks for U+FFFD and NUL misses
    this damage class completely -- measured 2026-10-22, 54 lines of
    lessons-learned.md carried control characters and nothing else.
    """
    return sum(1 for ch in line if ord(ch) < 0x20 and ch != "\t")


def inline_code_spans(line: str) -> list[tuple[int, int]]:
    """Index ranges covered by `...` inline code spans.

    CommonMark: a code span opened by an unmatched backtick runs to the end
    of the line, so an odd number of backticks leaves the tail covered.
    """
    ticks = [i for i, ch in enumerate(line) if ch == "`"]
    spans: list[tuple[int, int]] = []
    k = 0
    while k + 1 < len(ticks):
        spans.append((ticks[k], ticks[k + 1]))
        k += 2
    if k < len(ticks):
        spans.append((ticks[k], len(line) - 1))
    return spans


def fenced_code_lines(lines: list[str]) -> list[bool]:
    """True for every line that sits inside a ``` fenced code block."""
    inside = False
    flags: list[bool] = []
    for line in lines:
        if line.strip().startswith("```"):
            inside = not inside
            flags.append(True)
            continue
        flags.append(inside)
    return flags


def tsv_block_lines(lines: list[str]) -> list[bool]:
    """True for lines belonging to a TAB separated data block.

    A contiguous run of TAB bearing lines anchored by a ``#@/`` line is the
    rime userdb header quoted in project-knowledge.md; its TABs are field
    separators, not damage.
    """
    flags = [False] * len(lines)
    i = 0
    while i < len(lines):
        if "\t" not in lines[i]:
            i += 1
            continue
        j = i
        while j < len(lines) and "\t" in lines[j]:
            j += 1
        if any(line.startswith("#@/") for line in lines[i:j]):
            for k in range(i, j):
                flags[k] = True
        i = j
    return flags


def inline_tab_damage(lines: list[str]) -> list[int]:
    """Line numbers (0-based) that carry an un-escaped ``\\t`` TAB.

    TAB is the one C0 control that can be legitimate: it is the
    indentation character of every C/C++ source file and the field
    separator of the rime userdb header.  It therefore cannot be judged
    one line at a time - the judgement needs the surrounding document.

    The damage fingerprint is precise.  An un-escaped ``\\t`` swallowed the
    letter that had formed it, so the byte that replaced the escape is a
    TAB and what follows it is the *rest* of the original word.  In the
    middle of a line that rest is unmistakable -- a path, an identifier, a
    command.  At the start of a line it is ambiguous with indentation, so
    the line is only reported when the remainder still looks like the
    first segment of a path (``est/TestX.cpp`` -> ``test/TestX.cpp``),
    which is what a swallowed ``t`` leaves behind.

    Exemptions, each one a documented convention of this repository:
      * a TAB inside an inline-code span - the spec 078 provenance
        transcripts quote the damaged bytes verbatim on purpose;
      * a TAB inside a fenced code block - same reason;
      * a contiguous TSV block anchored by a ``#@/`` line - the rime
        userdb header quoted in project-knowledge.md;
      * a ``- OLD:`` line - spec 078's before/after transcript marker.
    """
    fences = fenced_code_lines(lines)
    tsv = tsv_block_lines(lines)
    hits: list[int] = []
    for i, line in enumerate(lines):
        if fences[i] or tsv[i]:
            continue
        if line.startswith("- OLD: "):
            continue
        if "\t" not in line:
            continue
        spans = inline_code_spans(line)
        lead = len(line) - len(line.lstrip(" \t"))
        for pos, ch in enumerate(line):
            if ch != "\t":
                continue
            if any(a <= pos <= b for a, b in spans):
                continue
            if pos < lead:
                # A leading TAB is normally indentation.  It is damage only
                # when the swallowed letter left a path shaped token behind
                # it: the first segment of a path such as "test/TestX.cpp"
                # lost its "t", so the segment still ends in a separator.
                # A TAB that indents a plain word ("\tindented item") is not
                # damage and must not be reported.
                tail = line[pos + 1:]
                token = re.split(r"[ \t]", tail)[0] if tail else ""
                if not PATH_SEGMENT.match(token):
                    continue
            hits.append(i)
            break
    return hits


def is_damaged(line: str) -> bool:
    """Primary damage: a character was replaced by U+FFFD or NUL.

    Tertiary damage: a C0 control character that survived an un-escaping pass.

    Secondary damage: the line is UTF-8 that was read as GBK and written back
    as UTF-8 (``§`` -> ``搂``, ``U+FFFD`` -> ``锟?``).  This form carries no
    U+FFFD at all, so a byte scan for ``EF BF BD`` alone under-reports the
    damage.  It is detected by a GBK round-trip: a genuinely clean Chinese
    line encodes to GBK bytes that are *not* valid UTF-8, so the round trip
    fails; a GBK-misread line round-trips back to its original UTF-8.
    """
    if REPLACEMENT in line or NUL in line:
        return True
    if c0_controls(line):
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
    if not _plausible(out):
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


def _write_checked(path: Path, payload: bytes, original: bytes) -> None:
    """Write only if the line/terminator structure is unchanged.

    A rewrite tool must never alter the line count or the CRLF discipline of
    a tracked document: those are the facts the byte-level verification in
    spec 078 section 5.1 checks. Abort loudly instead of writing a file that
    silently violates them.
    """
    def stats(buf: bytes) -> tuple[int, int]:
        return buf.count(b"\n"), buf.count(b"\r")

    new_nl, new_cr = stats(payload)
    old_nl, old_cr = stats(original)
    if (new_nl, new_cr) != (old_nl, old_cr):
        raise SystemExit(
            "refusing to write: line/terminator structure changed "
            f"(newline {old_nl}->{new_nl}, CR {old_cr}->{new_cr})"
        )
    path.write_bytes(payload)


def ascii_runs(text: str) -> list[str]:
    """All contiguous printable-ASCII runs, regardless of length."""
    return [m.group(0) for m in ASCII_RUN.finditer(text)]


def structurally_compatible(damaged: str, restored: str) -> bool:
    """True when the restored line lines up run-for-run with the damaged one.

    The first-generation damage only destroyed non-ASCII characters, so all
    ASCII of the original survives inside the damaged line. A candidate
    recovered from history must therefore have the same number of ASCII runs,
    and each restored run must appear inside the corresponding damaged run
    (the damaged run may carry extra ASCII adjacent to a destroyed char).
    """
    d = ascii_runs(damaged)
    r = ascii_runs(restored)
    if len(d) != len(r):
        return False
    return all(b in a for a, b in zip(d, r))


def strip_bom_artifacts(text: str) -> str:
    """Drop U+FEFF that was meaningful only as a file-leading BOM.

    A candidate from history may carry the BOM of the revision where that
    line sat at the top of the file. Restored mid-file, that U+FEFF is an
    artefact, not content.
    """
    return text.replace("\ufeff", "")


def restore_candidate(candidate: str) -> str:
    """Undo the GBK-misread layer on a historical candidate (idempotent)."""
    cur = candidate
    for _ in range(4):
        nxt = gbk_misread(cur)
        if nxt is None:
            break
        cur = nxt
    return cur


def candidate_is_supported(
    damaged: str, restored: str, min_overlap: float = 0.6
) -> bool:
    """Is this candidate demonstrably the same line as the damaged one?

    The anchors are the surviving ASCII facts (version numbers, commit ids,
    file names, test counts). A candidate whose anchors are largely absent
    from the damaged line is a different line and must not be applied.
    """
    if structurally_compatible(damaged, restored):
        return True
    r_runs = [r for r in ascii_runs(restored) if len(r) >= 8]
    if not r_runs:
        return False
    total = sum(len(r) for r in r_runs)
    hit = sum(len(r) for r in r_runs if r in damaged)
    if total == 0 or hit / total < min_overlap:
        return False
    longest = max(r_runs, key=len)
    return len(longest) < 12 or longest in damaged


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
    ap.add_argument(
        "--apply-partial",
        action="store_true",
        help=(
            "recover unmatched damaged lines from the closest historical "
            "candidate, after undoing its GBK-misread layer"
        ),
    )
    ap.add_argument("--min-score", type=float, default=0.99)
    args = ap.parse_args()

    # The console codepage on this machine is GBK (cp936). Without this,
    # printing a report that contains U+FFFD raises UnicodeEncodeError and
    # the tool dies before it can report its verdict -- which would silently
    # disable the CI gate that depends on its exit code. Encoding errors
    # become "?" instead of an exception; the exit code stays authoritative.
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")

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

    revs = revisions(repo, args.path)
    corpus: list[tuple[str, list[str]]] = []
    for rev in revs:
        hist = historical_lines(repo, args.path, rev)
        if hist:
            corpus.append((rev, hist))
    corpus.reverse()  # oldest first - prefer the earliest clean wording

    # The character repertoire of this document: the current file plus every
    # line in its history.  A GBK round trip is only accepted when the text
    # it recovers could have been written in this document at all; see
    # _plausible().  Publishing the repertoire before the damage scan is what
    # keeps a clean Chinese line from being reported as damaged.
    repertoire = set("".join(stripped))
    for _rev, hist in corpus:
        repertoire.update("".join(hist))
    set_repertoire(repertoire)

    tab_idx = inline_tab_damage(stripped)
    tab_set = set(tab_idx)
    damaged_idx = sorted(
        {i for i, line in enumerate(stripped) if is_damaged(line)} | tab_set
    )

    index = build_index(corpus)

    fixes: list[tuple[int, str, str, float, str]] = []
    unmatched: list[tuple[int, str, str | None, float]] = []
    for idx in damaged_idx:
        if idx in tab_set:
            # A TAB class line has no clean ancestor to copy from; the
            # restoration is mechanical (TAB -> the letter it swallowed),
            # so it is reported on its own instead of being matched
            # against the history.
            continue
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
            "- damaged: `" + cur.replace(REPLACEMENT, "<U+FFFD>").replace(NUL, "<NUL>") + "`",
            "- clean:   `" + new + "`",
            "",
        ]
    for idx, cur, cand, score in unmatched:
        out_lines += [
            f"## line {idx + 1}  (NO CLEAN MATCH - human rewrite required)",
            "",
            "- damaged: `" + cur.replace(REPLACEMENT, "<U+FFFD>").replace(NUL, "<NUL>") + "`",
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

    if tab_idx:
        out_lines += [
            "## TAB class damage (an un-escaped `\\t` swallowed a letter)",
            "",
            f"count = {len(tab_idx)}",
            "",
            "The restoration is mechanical: the TAB byte goes back to the",
            "letter it swallowed.  Review each line before applying it.",
            "",
        ]
        for idx in tab_idx:
            out_lines += [
                f"### line {idx + 1}",
                "",
                "- damaged:  `" + stripped[idx].replace("\t", "<TAB>") + "`",
                "- restored: `" + stripped[idx].replace("\t", "t") + "`",
                "",
            ]

    report = "\n".join(out_lines)
    if args.report:
        Path(args.report).write_text(report, encoding="utf-8")
    else:
        print(report)

    fffd = sum(line.count(REPLACEMENT) for line in stripped)
    nul = sum(line.count(NUL) for line in stripped)
    c0 = sum(c0_controls(line) for line in stripped)
    print(
        "damage-scan: {} damaged_lines={} fffd={} nul={} c0={} tab={} exit={}".format(
            args.path, len(damaged_idx), fffd, nul, c0, len(tab_idx),
            1 if damaged_idx else 0
        )
    )

    if args.apply_roundtrip and roundtrip_lines:
        for idx, _before, after in roundtrip_lines:
            eol = "\r" if lines[idx].endswith("\r") else ""
            lines[idx] = after + eol
        out = "\n".join(lines)
        _write_checked(target_path, out.encode("utf-8"), raw)
        print(
            f"round-trip repaired {len(roundtrip_lines)} line(s)", file=sys.stderr
        )

    if args.apply and fixes:
        for idx, _cur, new, _score, _rev in fixes:
            new = strip_bom_artifacts(new)
            # lines came from text.split("\n"), so each element already owns
            # its trailing "\r" when the file is CRLF. Carry that "\r" only -
            # the join below supplies the "\n". Appending "\r\n" here would
            # emit a spurious blank line and a bare LF per replacement.
            eol = "\r" if lines[idx].endswith("\r") else ""
            lines[idx] = new + eol
        out = "\n".join(lines)
        _write_checked(target_path, out.encode("utf-8"), raw)
        print(f"applied {len(fixes)} recovered line(s)", file=sys.stderr)

    if args.apply_partial:
        recovered = []
        for idx, cur, cand, score in unmatched:
            if not cand:
                continue
            restored = strip_bom_artifacts(restore_candidate(cand))
            if restored == cur:
                continue
            if REPLACEMENT in restored or NUL in restored:
                continue
            if not candidate_is_supported(cur, restored):
                continue
            recovered.append((idx, cur, restored, score))
        for idx, _cur, restored, _score in recovered:
            restored = strip_bom_artifacts(restored)
            eol = "\r" if lines[idx].endswith("\r") else ""
            lines[idx] = restored + eol
        if recovered:
            out = "\n".join(lines)
            _write_checked(target_path, out.encode("utf-8"), raw)
        print(
            f"partial-roundtrip repaired {len(recovered)} line(s)", file=sys.stderr
        )

    return 1 if damaged_idx else 0


if __name__ == "__main__":
    sys.exit(main())
