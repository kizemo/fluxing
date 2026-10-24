#!/usr/bin/env python3
"""guard-event-writer.py — Append a BLOCK event to .loopx/guard-events-YYYY-MM-DD.jsonl.

Used by the 5 guard-rails hooks (secret-files, main-branch-push, db-migration,
package-publish, installer-path). Designed to fail soft: if the .loopx directory
doesn't exist (e.g. no LoopX project yet) or the write fails, we just stderr a
warning and exit 0 so the hook's block decision is unaffected.

Usage:
  guard-event-writer.py --hook <name> --tool <tool> --reason <reason> [--input-summary <s>] [--project <p>]

Reads tool_input from stdin (JSON, per Claude Code hook contract) to extract cwd.
"""

from __future__ import annotations
import argparse
import datetime as _dt
import json
import os
import sys
from pathlib import Path


def _now_iso() -> str:
    return _dt.datetime.now(_dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def _today_ymd() -> str:
    return _dt.datetime.now(_dt.timezone.utc).strftime("%Y-%m-%d")


def _detect_cwd_from_payload(payload: dict) -> str | None:
    """Extract cwd from a parsed Claude Code hook payload."""
    if not isinstance(payload, dict):
        return None
    for key in ("cwd", "working_directory"):
        if key in payload:
            return str(payload[key])
    ti = payload.get("tool_input")
    if isinstance(ti, dict):
        for key in ("cwd", "working_directory"):
            if key in ti:
                return str(ti[key])
    return None


def _detect_cwd() -> str:
    """Try stdin first; fall back to process cwd."""
    try:
        if not sys.stdin.isatty():
            raw = sys.stdin.read()
            if raw.strip():
                payload = json.loads(raw)
                cwd = _detect_cwd_from_payload(payload)
                if cwd:
                    return cwd
    except Exception:
        pass
    return os.getcwd()


def _detect_project(cwd: str) -> str | None:
    """Heuristic project key from cwd path."""
    lc = cwd.lower().replace("\\", "/")
    for key, pat in (
        ("rime-claude", "rime_claude"),
        ("media-to-doc-ui", "media-to-doc-ui"),
        ("cut-ad", "/cut-ad/"),
        ("sandbox-verify", "/sandbox-verify/"),
    ):
        if pat in lc:
            return key
    return None


def main() -> int:
    p = argparse.ArgumentParser(description="Append BLOCK event to .loopx/guard-events log")
    p.add_argument("--hook", required=True, help="hook name (e.g. guard-secret-files)")
    p.add_argument("--tool", default="unknown", help="tool name (e.g. Edit, Bash)")
    p.add_argument("--reason", required=True, help="short block reason")
    p.add_argument("--input-summary", default="", help="short summary of offending input")
    p.add_argument("--project", default=None, help="project key (auto-detect from cwd if absent)")
    p.add_argument("--input-file", default=None,
                   help="read tool_input JSON from this file (alternative to stdin)")
    args = p.parse_args()

    # If --input-file given, try to extract cwd from it before falling back to stdin/pwd
    cwd = None
    if args.input_file:
        try:
            with open(args.input_file, "r", encoding="utf-8") as f:
                payload = json.loads(f.read())
            cwd = _detect_cwd_from_payload(payload)
        except Exception:
            pass
    if not cwd:
        cwd = _detect_cwd()
    project = args.project or _detect_project(cwd)

    event = {
        "ts": _now_iso(),
        "hook": args.hook,
        "tool": args.tool,
        "input_summary": args.input_summary,
        "reason": args.reason,
        "exit_code": 2,
        "cwd": cwd,
        "project": project,
    }

    # Write to .loopx/guard-events-YYYY-MM-DD.jsonl in the cwd.
    loopx_dir = Path(cwd) / ".loopx"
    target = loopx_dir / f"guard-events-{_today_ymd()}.jsonl"

    try:
        loopx_dir.mkdir(parents=True, exist_ok=True)
        with target.open("a", encoding="utf-8") as f:
            f.write(json.dumps(event, ensure_ascii=False) + "\n")
    except Exception as exc:
        # Fail soft: don't break the hook's block decision.
        print(f"[guard-event-writer] WARN: failed to write {target}: {exc}", file=sys.stderr)
        return 0

    return 0


if __name__ == "__main__":
    sys.exit(main())