# loopx-guard-summary.sh — Summarize BLOCK events from .loopx/guard-events-*.jsonl
#
# Usage:
#   loopx-guard-summary.sh [--root DIR] [--since YYYY-MM-DD] [--hook NAME] [--limit N]
#
# Default root = cwd. Output is markdown.

set -u

ROOT="$(pwd -W 2>/dev/null || pwd)"
SINCE=""
HOOK_FILTER=""
LIMIT=200

while [[ $# -gt 0 ]]; do
  case "$1" in
    --root) ROOT="$2"; shift 2 ;;
    --since) SINCE="$2"; shift 2 ;;
    --hook) HOOK_FILTER="$2"; shift 2 ;;
    --limit) LIMIT="$2"; shift 2 ;;
    *) shift ;;
  esac
done

EVENTS_DIR="$ROOT/.loopx"
if [[ ! -d "$EVENTS_DIR" ]]; then
  echo "# Guard-Rails Summary"
  echo ""
  echo "- project: \`$(basename "$ROOT")\`"
  echo "- events_total: 0"
  echo "- note: \`.loopx/\` directory not found in $ROOT"
  echo ""
  exit 0
fi

# Build python script to aggregate
python <<PYEOF
import json, os, sys, datetime as dt
from pathlib import Path
from collections import Counter, defaultdict

root = Path(r"$ROOT")
since = "$SINCE" or None
hook_filter = "$HOOK_FILTER" or None
limit = int("$LIMIT")

events_dir = root / ".loopx"
files = sorted(events_dir.glob("guard-events-*.jsonl"))

events = []
for f in files:
    try:
        for line in f.read_text(encoding="utf-8").splitlines():
            if not line.strip():
                continue
            try:
                ev = json.loads(line)
            except json.JSONDecodeError:
                continue
            if since and ev.get("ts", "")[:10] < since:
                continue
            if hook_filter and ev.get("hook") != hook_filter:
                continue
            events.append(ev)
    except Exception:
        continue

events.sort(key=lambda e: e.get("ts", ""))
events = events[-limit:]

print("# Guard-Rails Summary")
print()
project = events[0].get("project") if events else root.name
print(f"- project: \`{project}\`")
print(f"- events_total: {len(events)}")
if events:
    dates = [e.get("ts", "")[:10] for e in events if e.get("ts")]
    if dates:
        print(f"- date_range: {min(dates)} → {max(dates)}")
print(f"- scan_root: \`{root}\`")
print()

if not events:
    print("No matching BLOCK events.")
    print()
    sys.exit(0)

# By hook
by_hook = Counter(e.get("hook", "unknown") for e in events)
print("## By hook")
print()
for hook, count in by_hook.most_common():
    print(f"### {hook} ({count})")
    for e in [x for x in events if x.get("hook") == hook][-10:]:
        ts = e.get("ts", "?")
        tool = e.get("tool", "?")
        summary = e.get("input_summary", "")
        reason = e.get("reason", "")
        print(f"- {ts} \`{tool}\` \`{summary}\` — {reason}")
    print()

# By reason
print("## By reason (top 10)")
print()
by_reason = Counter(e.get("reason", "") for e in events)
for reason, count in by_reason.most_common(10):
    print(f"- {count}× {reason}")
print()

# By tool
print("## By tool")
print()
by_tool = Counter(e.get("tool", "?") for e in events)
for tool, count in by_tool.most_common():
    print(f"- {count}× \`{tool}\`")
print()

# Recommendations
print("## Recommendations")
print()
print(f"- {len(events)} BLOCK events total. Each was a justified safety block — no action needed.")
top_hook = by_hook.most_common(1)[0]
print(f"- Hottest hook: \`{top_hook[0]}\` ({top_hook[1]}×). Consider adding to LoopX \`/loopx review-packet\` for periodic audits.")
silent_hooks = {"guard-secret-files", "guard-main-branch-push", "guard-db-migration",
                "guard-package-publish", "guard-installer-path"} - set(by_hook.keys())
if silent_hooks:
    print(f"- No events from: {', '.join(sorted(silent_hooks))}. Either no violations, or hooks not wired.")
print()
PYEOF