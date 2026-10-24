#!/usr/bin/env bash
# Project-level Guard Rail #4 of 5.
# PreToolUse:Bash — block accidental package publishes.
#
# Why: 装机项目(rime_claude / media-to-doc-ui)发 GitHub Release / VSCode
# Marketplace 一旦跑错 tag 就公开了,需 AskUserQuestion 二次确认。
# Block:
#   npm publish / pnpm publish / yarn publish / bun publish
#   twine upload (PyPI)
#   cargo publish
#   vsce publish / ovsx publish (VSCode)
#   gh release create (without --draft)
# Allow:
#   --dry-run / --tag next / --draft
#
# Install: copy to <project>/.claude/hooks/guard-package-publish.sh,
# chmod +x, add matcher:
#   { "matcher": "Bash",
#     "hooks": [{ "type": "command",
#                 "command": "bash <project>/.claude/hooks/guard-package-publish.sh",
#                 "timeout": 2 }] }

set -u
input=$(cat)

cmd=$(printf '%s' "$input" | python -c "
import json, sys
try:
    d = json.load(sys.stdin)
    print(d.get('tool_input', {}).get('command', '') or '')
except Exception:
    print('')
")

if [ -z "$cmd" ]; then
    exit 0
fi

cmd_norm=$(printf '%s' "$cmd" | tr -s ' \t')

# --- Allowlist short-circuits ---
allow_match() {
    # Returns 0 if cmd_norm matches any allowlist pattern
    local p
    for p in "$@"; do
        if printf '%s' "$cmd_norm" | grep -qE "$p"; then
            return 0
        fi
    done
    return 1
}

if allow_match \
    '(--dry-run|-n[[:space:]])' \
    '(npm|pnpm|yarn|bun)[[:space:]]+pack' \
    'gh[[:space:]]+release[[:space:]]+create.*--draft' \
    '(vsce|ovsx)[[:space:]]+(package|ls|show|unpublish)' \
    '(twine[[:space:]]+upload.*--repository[[:space:]]+testpypi|twine[[:space:]]+check)'; then
    exit 0
fi

blocked=""

# npm / pnpm / yarn / bun publish
if printf '%s' "$cmd_norm" | grep -qE '(npm|pnpm|yarn|bun)[[:space:]]+publish'; then
    if ! printf '%s' "$cmd_norm" | grep -qE '(--dry-run|--tag[[:space:]]+(next|beta|alpha))'; then
        blocked="npm/pnpm/yarn/bun publish (without --dry-run or pre-release tag)"
    fi
fi

# twine upload (PyPI)
if printf '%s' "$cmd_norm" | grep -qE '\btwine[[:space:]]+upload\b'; then
    if ! printf '%s' "$cmd_norm" | grep -qE '\b--repository[[:space:]]+testpypi\b'; then
        blocked="twine upload (PyPI production)"
    fi
fi

# cargo publish
if printf '%s' "$cmd_norm" | grep -qE '\bcargo[[:space:]]+publish\b'; then
    if ! printf '%s' "$cmd_norm" | grep -qE '\b--dry-run\b'; then
        blocked="cargo publish (without --dry-run)"
    fi
fi

# vsce / ovsx publish (VSCode marketplace)
if printf '%s' "$cmd_norm" | grep -qE '\b(vsce|ovsx)[[:space:]]+publish\b'; then
    if ! printf '%s' "$cmd_norm" | grep -qE '\b--dry-run\b'; then
        blocked="vsce/ovsx publish (VSCode marketplace)"
    fi
fi

# gh release create (without --draft)
if printf '%s' "$cmd_norm" | grep -qE '\bgh[[:space:]]+release[[:space:]]+create\b'; then
    if ! printf '%s' "$cmd_norm" | grep -qE '\b--draft\b'; then
        blocked="gh release create (without --draft)"
    fi
fi

if [ -n "$blocked" ]; then
    cat >&2 <<EOF
[guard-package-publish] BLOCKED: package publish detected.

  Reason: $blocked
  Cmd:    ${cmd:0:200}

  Required before retry:
    1. Run with --dry-run first to verify artifacts.
    2. Confirm version + tag with user via AskUserQuestion.
    3. For VSCode: prefer --package-path or --dry-run before live publish.
    4. For GitHub release: start with --draft, then publish from web UI.
    5. Re-run only after explicit approval.
EOF
    # Record to .loopx/guard-events-*.jsonl (best-effort, fail-soft)
    bash "$(dirname "$0")/guard-event-writer.sh" \
        --hook guard-package-publish \
        --tool Bash \
        --reason "package publish detected" \
        --input-summary "cmd=${cmd:0:120}" 2>/dev/null || true
    exit 2
fi

exit 0
