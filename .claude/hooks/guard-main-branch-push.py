#!/usr/bin/env python3
# Project-level Guard Rail #2 of 5.
# PreToolUse:Bash — block dangerous git push commands.
#
# Why: 用户偏好"AI 友好" + 5-20 人团队;Push to main 应走 PR 流程。
# 拦截:git push origin main|master / git push -f / git push --force-with-lease
# 允许:git push origin <feature-branch> / git push --dry-run / git push <branch>
#
# Override config (<project>/.claude/guard-rails.yaml):
#   allowed_main_branches:
#     - main
#     - release/*
#   (default = main + master)
#
# Install: copy to <project>/.claude/hooks/guard-main-branch-push.py,
# chmod +x, add matcher:
#   { "matcher": "Bash",
#     "hooks": [{ "type": "command",
#                 "command": "python <project>/.claude/hooks/guard-main-branch-push.py",
#                 "timeout": 2 }] }

import json
import os
import re
import sys
from pathlib import Path

def load_allowed_branches():
    """Read allowed_main_branches from project guard-rails.yaml or return default."""
    default = ["main", "master"]
    proj = os.environ.get("CLAUDE_PROJECT_DIR")
    if not proj:
        return default
    yaml_path = Path(proj) / ".claude" / "guard-rails.yaml"
    if not yaml_path.exists():
        return default
    try:
        text = yaml_path.read_text(encoding="utf-8")
        # Minimal YAML: only `  - branch` lines under `allowed_main_branches:`
        lines = text.split("\n")
        in_key = False
        branches = []
        for line in lines:
            if re.match(r"^allowed_main_branches:", line):
                in_key = True
                continue
            if in_key:
                m = re.match(r'^\s+-\s+["\']?([^"\']+)["\']?\s*$', line)
                if m:
                    branches.append(m.group(1).strip())
                elif re.match(r"^[a-z_]+:", line.strip(), re.IGNORECASE):
                    in_key = False
        return branches if branches else default
    except Exception:
        return default

def glob_match(pattern, text):
    """Minimal glob: `*` matches any segment, exact match otherwise."""
    if pattern == text:
        return True
    if "*" not in pattern:
        return False
    # Convert glob to regex: `*` → `.*`, escape other regex chars
    regex = "^" + re.escape(pattern).replace(r"\*", ".*") + "$"
    return bool(re.match(regex, text))

def main():
    raw = sys.stdin.read()
    try:
        ctx = json.loads(raw) if raw.strip() else {}
    except Exception:
        ctx = {}

    cmd = ctx.get("tool_input", {}).get("command", "") or ""

    # Detect: git push ... <remote> <ref> (e.g. `git push origin main`)
    # Capture remote and ref (last 1-2 tokens that look like branch names)
    push_match = re.search(
        r"\bgit\s+push\b(?P<flags>(?:\s+--?[a-z][\w-]*)*)\s+(?P<rest>.+)$",
        cmd, re.IGNORECASE
    )
    if not push_match:
        process_safe(cmd) if False else sys.exit(0)  # not a push, pass through
        return

    flags = push_match.group("flags") or ""
    rest = push_match.group("rest").strip()

    # Strip dry-run early — allow preview pushes
    if "--dry-run" in flags or "-n" in flags.split():
        sys.exit(0)

    # Detect force push in any form
    force_patterns = [r"\s-f\b", r"\s--force\b", r"\s--force-with-lease\b", r"\s--force-if-includes\b"]
    is_force = any(re.search(p, flags) for p in force_patterns)

    # Parse remote + ref from `rest`. Last token is ref (if not a flag).
    # Strip leading remote (skip if it looks like a flag).
    tokens = [t for t in rest.split() if not t.startswith("-")]
    if not tokens:
        sys.exit(0)
    ref = tokens[-1]

    allowed = load_allowed_branches()
    blocked_branch = is_force or any(glob_match(p, ref) for p in allowed)

    if blocked_branch:
        sys.stderr.write(
            "[guard-main-branch-push] BLOCKED: direct push to protected ref.\n"
            f"  Cmd:    {cmd[:200]}\n"
            f"  Ref:    {ref}\n"
            f"  Force:  {is_force}\n"
            f"  Allowed: {', '.join(allowed)}\n"
            "  Action: open a PR instead. If you must bypass (hotfix), use\n"
            "          `git push origin <branch> --force-with-lease` only after\n"
            "          explicit AskUserQuestion approval.\n"
        )
        # Record to .loopx/guard-events-*.jsonl (best-effort, fail-soft)
        try:
            import subprocess as _sp
            import tempfile as _tf
            here = os.path.dirname(os.path.abspath(__file__))
            # Write raw input to a temp file (Windows-safe vs stdin bytes)
            tmp = _tf.NamedTemporaryFile(mode="w", suffix=".json", delete=False, encoding="utf-8")
            try:
                tmp.write(raw if isinstance(raw, str) else (raw.decode("utf-8") if raw else ""))
                tmp.close()
                _sp.run(
                    ["python", os.path.join(here, "guard-event-writer.py"),
                     "--hook", "guard-main-branch-push",
                     "--tool", "Bash",
                     "--reason", "direct push to protected ref",
                     "--input-summary", f"cmd={cmd[:120]}",
                     "--input-file", tmp.name],
                    capture_output=True, timeout=5
                )
            finally:
                try: os.unlink(tmp.name)
                except Exception: pass
        except Exception:
            pass
        sys.exit(2)

    sys.exit(0)

if __name__ == "__main__":
    main()
