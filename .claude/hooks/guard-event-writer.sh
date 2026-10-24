#!/usr/bin/env bash
# guard-event-writer.sh — Append a BLOCK event to .loopx/guard-events-YYYY-MM-DD.jsonl
# Same contract as guard-event-writer.py; bash version for hooks already in bash.
#
# Usage:
#   guard-event-writer.sh --hook <name> --reason <reason> [--tool <tool>] [--input-summary <s>] [--project <p>]
#
# Reads tool_input JSON from stdin to extract cwd.

set -u

HOOK=""
TOOL="unknown"
REASON=""
INPUT_SUMMARY=""
PROJECT=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --hook) HOOK="$2"; shift 2 ;;
    --tool) TOOL="$2"; shift 2 ;;
    --reason) REASON="$2"; shift 2 ;;
    --input-summary) INPUT_SUMMARY="$2"; shift 2 ;;
    --project) PROJECT="$2"; shift 2 ;;
    *) shift ;;
  esac
done

if [[ -z "$HOOK" || -z "$REASON" ]]; then
  echo "[guard-event-writer] ERROR: --hook and --reason required" >&2
  exit 0  # fail soft
fi

# Detect cwd: try stdin JSON first, fallback to pwd
CWD=""
if [[ ! -t 0 ]]; then
  STDIN_JSON="$(cat 2>/dev/null || true)"
  if [[ -n "$STDIN_JSON" ]]; then
    CWD="$(echo "$STDIN_JSON" | python -c '
import json, sys
try:
    p = json.load(sys.stdin)
    for k in ("cwd", "working_directory"):
        if k in p:
            print(p[k]); break
    else:
        ti = p.get("tool_input", {})
        if isinstance(ti, dict):
            for k in ("cwd", "working_directory"):
                if k in ti:
                    print(ti[k]); break
except Exception:
    pass
' 2>/dev/null || true)"
  fi
fi
[[ -z "$CWD" ]] && CWD="$(pwd -W 2>/dev/null || pwd)"

# Auto-detect project from cwd
if [[ -z "$PROJECT" ]]; then
  case "$CWD" in
    *rime_claude*) PROJECT="rime-claude" ;;
    *media-to-doc-ui*) PROJECT="media-to-doc-ui" ;;
    *cut-ad*) PROJECT="cut-ad" ;;
    *sandbox-verify*) PROJECT="sandbox-verify" ;;
  esac
fi

TS="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
YMD="$(date -u +%Y-%m-%d)"
LOOPX_DIR="$CWD/.loopx"
TARGET="$LOOPX_DIR/guard-events-${YMD}.jsonl"

mkdir -p "$LOOPX_DIR" 2>/dev/null || {
  echo "[guard-event-writer] WARN: cannot mkdir $LOOPX_DIR" >&2
  exit 0  # fail soft
}

# Build JSON via python (handles escaping reliably)
JSON_LINE="$(python -c "
import json, sys
e = {
  'ts': '$TS',
  'hook': '$HOOK',
  'tool': '$TOOL',
  'input_summary': '''$INPUT_SUMMARY''',
  'reason': '''$REASON''',
  'exit_code': 2,
  'cwd': '$CWD',
  'project': '''$PROJECT''',
}
print(json.dumps(e, ensure_ascii=False))
" 2>/dev/null)"

if [[ -z "$JSON_LINE" ]]; then
  echo "[guard-event-writer] WARN: JSON build failed" >&2
  exit 0
fi

echo "$JSON_LINE" >> "$TARGET" 2>/dev/null || {
  echo "[guard-event-writer] WARN: cannot write $TARGET" >&2
  exit 0  # fail soft
}

exit 0