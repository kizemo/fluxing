#!/usr/bin/env python3
"""Apply a line-indexed rewrite map to a tracked document, byte-safely.

The rewriter (human or agent) only produces *content*; this tool owns every
byte-level decision, so the discipline lives in one place:

  * read raw bytes, decode strict UTF-8 (a decode failure aborts)
  * split on b"\\n" only - never str.splitlines(), which would also break on
    \\x0b \\x0c \\x85 U+2028 U+2029 and desynchronise the line numbers
  * carry the trailing "\\r" over from the original line, never re-add it
  * keep the line count and the CR count identical to the input
  * keep the file's BOM state identical to the input
  * reject any replacement text that still contains U+FFFD or NUL

Usage
-----
    python scripts/test-infra/apply-line-rewrites.py \
        --repo <repo> --target <repo-relative path> --map <rewrites.json>

The map file is UTF-8 JSON:

    {
      "lines": {
        "1053": "the rewritten line",
        "1054": "another one"
      }
    }

Line numbers are 1-based and refer to the file *before* the rewrite.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REPLACEMENT = "\ufffd"
NUL = "\x00"
BOM = "\ufeff"


def fail(msg: str) -> int:
    print("apply-line-rewrites: {}".format(msg), file=sys.stderr)
    return 2


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--target", required=True)
    ap.add_argument("--map", required=True, dest="map_path")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    repo = Path(args.repo).resolve()
    target = repo / args.target
    if not target.is_file():
        return fail("not found: {}".format(target))

    raw = target.read_bytes()
    had_bom = raw.startswith(b"\xef\xbb\xbf")
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError as exc:
        return fail("target is not valid UTF-8: {}".format(exc))

    lines = text.split("\n")
    orig_line_count = len(lines)
    orig_cr = raw.count(b"\r")
    orig_nl = raw.count(b"\n")

    payload = json.loads(Path(args.map_path).read_text(encoding="utf-8"))
    rewrites = payload.get("lines")
    if not isinstance(rewrites, dict) or not rewrites:
        return fail("map file has no non-empty 'lines' object")

    applied = 0
    problems = []
    for key, new_text in sorted(rewrites.items(), key=lambda kv: int(kv[0])):
        try:
            idx = int(key) - 1
        except ValueError:
            problems.append("line key not an integer: {!r}".format(key))
            continue
        if idx < 0 or idx >= orig_line_count:
            problems.append("line {} out of range (file has {} lines)".format(
                idx + 1, orig_line_count))
            continue
        if not isinstance(new_text, str):
            problems.append("line {}: replacement is not a string".format(idx + 1))
            continue
        if REPLACEMENT in new_text:
            problems.append("line {}: replacement still contains U+FFFD".format(idx + 1))
            continue
        if NUL in new_text:
            problems.append("line {}: replacement contains NUL".format(idx + 1))
            continue
        if BOM in new_text:
            problems.append("line {}: replacement contains U+FEFF".format(idx + 1))
            continue

        original = lines[idx]
        eol = "\r" if original.endswith("\r") else ""
        if original.rstrip("\r") == new_text:
            problems.append("line {}: replacement is identical to the original".format(
                idx + 1))
            continue
        lines[idx] = new_text + eol
        applied += 1

    if problems:
        for p in problems:
            print("  ! {}".format(p), file=sys.stderr)
        return fail("{} problem(s); nothing written".format(len(problems)))

    out = "\n".join(lines)
    out_bytes = out.encode("utf-8")

    # invariants
    if out_bytes.count(b"\n") != orig_nl:
        return fail("newline count changed ({} -> {}); nothing written".format(
            orig_nl, out_bytes.count(b"\n")))
    if out_bytes.count(b"\r") != orig_cr:
        return fail("CR count changed ({} -> {}); nothing written".format(
            orig_cr, out_bytes.count(b"\r")))
    if out_bytes.startswith(b"\xef\xbb\xbf") != had_bom:
        return fail("BOM state changed; nothing written")
    if len(out.split("\n")) != orig_line_count:
        return fail("line count changed; nothing written")

    if args.dry_run:
        print("dry-run: {} line(s) would be rewritten in {} (invariants hold)".format(
            applied, args.target))
        return 0

    target.write_bytes(out_bytes)
    print("applied {} rewrite(s) to {} ({} bytes, {} lines, CRLF {})".format(
        applied, args.target, len(out_bytes), orig_line_count, orig_cr))
    return 0


if __name__ == "__main__":
    sys.exit(main())
