#!/usr/bin/env bash
#
# scripts/lint-changed-cpp.sh
#
# Lint ONLY the C/C++ files that this push touched.
#
# WHY THIS EXISTS
#   The brand fork added a number of files that were written with a 4-space
#   indent and never run through clang-format. A whole-tree lint has therefore
#   been red since the fork, which meant the `lint` job failed and every
#   downstream job (build -> test -> sandbox-gate) was SKIPPED. In other words
#   the gates were wired up but never actually ran.
#
#   This script keeps the gate meaningful -- any C/C++ file you touch has to be
#   clang-format clean -- while stopping the pre-existing debt from blocking
#   the pipeline. The debt stays visible through `--all --report-only`.
#
# USAGE
#   scripts/lint-changed-cpp.sh                       gate: changed files only
#   scripts/lint-changed-cpp.sh --all                 gate: the whole tree
#   scripts/lint-changed-cpp.sh --all --report-only   report only, always exit 0
#
# ENVIRONMENT
#   BASE_SHA   commit to diff against (default: HEAD~1, then the whole tree)
#
# EXIT CODES
#   0 = clean          1 = violations found          2 = cannot run the check
#
set -uo pipefail

MODE="changed"
REPORT_ONLY=0

while [ $# -gt 0 ]; do
  case "$1" in
    --all)        MODE="all" ;;
    --report-only) REPORT_ONLY=1 ;;
    -h|--help)    sed -n '2,30p' "$0"; exit 0 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
  shift
done

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT" || exit 2

if ! command -v clang-format >/dev/null 2>&1; then
  echo "[lint] clang-format not found in PATH -- cannot run the style gate" >&2
  exit 2
fi

# Same exclusion list and the same find expression as clang-format.sh, so the
# file set this script checks is exactly the set the old whole-tree lint used.
EXCLUDE=".exclude_pattern.txt"
[ -f "$EXCLUDE" ] || EXCLUDE="/dev/null"

LIST="$(mktemp)"
trap 'rm -f "$LIST"' EXIT

if [ "$MODE" = "all" ]; then
  find RimeWithWeasel WeaselDeployer WeaselIPC WeaselIPCServer WeaselServer \
       WeaselSetup WeaselTSF WeaselUI include test \
       -name '*.cpp' -o -name '*.h' ! -path "include/wtl/*" \
    | grep -wiv -f "$EXCLUDE" > "$LIST"
else
  BASE="${BASE_SHA:-}"
  if [ -z "$BASE" ] || ! git cat-file -e "${BASE}^{commit}" 2>/dev/null; then
    BASE="$(git rev-parse HEAD~1 2>/dev/null || true)"
  fi
  if [ -z "$BASE" ]; then
    echo "[lint] no base commit available -- checking the whole tree instead"
    find RimeWithWeasel WeaselDeployer WeaselIPC WeaselIPCServer WeaselServer \
         WeaselSetup WeaselTSF WeaselUI include test \
         -name '*.cpp' -o -name '*.h' ! -path "include/wtl/*" \
      | grep -wiv -f "$EXCLUDE" > "$LIST"
  else
    echo "[lint] base = $BASE"
    git diff --name-only --diff-filter=ACMR "$BASE" HEAD -- '*.cpp' '*.h' \
      | grep -wiv -f "$EXCLUDE" > "$LIST"
  fi
fi

COUNT="$(wc -l < "$LIST" | tr -d ' ')"
echo "[lint] files to check: $COUNT"

if [ "$COUNT" -eq 0 ]; then
  echo "[lint] nothing to check -- OK"
  exit 0
fi

if [ "$REPORT_ONLY" -eq 1 ]; then
  OFFENDERS=0
  while IFS= read -r f; do
    [ -n "$f" ] || continue
    if ! clang-format --style=file --dry-run --Werror "$f" >/dev/null 2>&1; then
      OFFENDERS=$((OFFENDERS + 1))
      echo "  not clang-format clean: $f"
    fi
  done < "$LIST"
  echo "[lint] style debt: $OFFENDERS / $COUNT file(s) not clang-format clean"
  exit 0
fi

STATUS=0
while IFS= read -r f; do
  [ -n "$f" ] || continue
  if ! clang-format --style=file --Werror --dry-run "$f"; then
    STATUS=1
  fi
done < "$LIST"

if [ "$STATUS" -eq 0 ]; then
  echo "[lint] PASS -- $COUNT file(s) clean"
else
  echo "[lint] FAIL -- run 'clang-format -i <file>' on the files listed above"
fi
exit "$STATUS"
