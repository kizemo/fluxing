﻿#!/usr/bin/env bash
# Project-level Guard Rail #5 of 5.
# PreToolUse:Bash|Edit|Write — block installer writes outside allowlist paths.
#
# Why: 装机项目(rime_claude / media-to-doc-ui / cut-ad)在 Windows 上生成
# .exe / .msi / .dmg / .deb / .AppImage,如果 Claude 把二进制写到
# C:\Windows / C:\Program Files\ / /usr/bin/ 就会污染系统路径。
# 强制 installer 只能写到 target/ / release/ / dist/ / build/ / output/。
#
# Override (<project>/.claude/guard-rails.yaml):
#   installer_allow_paths:
#     - target/release/bundle/
#     - release/
#
# Default allowlist: target/ / release/ / dist/ / build/ / output/ / out/
#
# Install: copy to <project>/.claude/hooks/guard-installer-path.sh,
# chmod +x, add matcher to TWO matchers (Edit/Write/Bash):
#   { "matcher": "Edit|Write|MultiEdit",
#     "hooks": [{ "type": "command",
#                 "command": "bash <project>/.claude/hooks/guard-installer-path.sh",
#                 "timeout": 2 }] }
#   { "matcher": "Bash",
#     "hooks": [{ "type": "command",
#                 "command": "bash <project>/.claude/hooks/guard-installer-path.sh",
#                 "timeout": 2 }] }

set -u
input=$(cat)

# Parse tool_input: support Edit/Write (file_path) and Bash (command)
parsed=$(printf '%s' "$input" | python -c "
import json, sys
try:
    d = json.load(sys.stdin)
    ti = d.get('tool_input', {})
    print(ti.get('file_path', '') or ti.get('command', '') or '')
except Exception:
    print('')
")

if [ -z "$parsed" ]; then
    exit 0
fi

# Normalize: convert backslashes to forward slashes for matching
normalized=$(printf '%s' "$parsed" | tr '\\' '/')

# --- Find installer file paths in the input ---
# Extract all candidate paths (Windows drive + Unix + relative).
# Use one unified greedy pattern to avoid double-matching.
# (character classes deliberately omit quote chars to avoid shell escape hell)

# Single greedy regex: matches the longest path-like token ending in installer ext
all_candidates=$(printf '%s' "$normalized" \
    | grep -oEi '[A-Za-z]:[/\\][^[:space:]]+\.(exe|msi|dmg|deb|rpm|AppImage|pkg)|[/\\][^[:space:]]+\.(exe|msi|dmg|deb|rpm|AppImage|pkg)|[A-Za-z0-9_./-]+\.(exe|msi|dmg|deb|rpm|AppImage|pkg)' \
    | grep -v '^$' \
    | sort -u)

if [ -z "$all_candidates" ]; then
    # No installer artifacts detected → pass
    exit 0
fi

# --- Load project allowlist ---
default_paths=("target/" "release/" "dist/" "build/" "output/" "out/")
allow_paths=("${default_paths[@]}")

yaml_file="${CLAUDE_PROJECT_DIR:-}/.claude/guard-rails.yaml"
if [ -f "$yaml_file" ]; then
    while IFS= read -r line; do
        case "$line" in
            "  - "*)
                # Strip leading "  - " and any surrounding single/double quotes
                # Use awk to avoid bash quote-escaping hell
                p=$(printf '%s' "$line" | awk '
                    {
                        s = $0
                        sub(/^[[:space:]]*-[[:space:]]*/, "", s)
                        # If string starts with quote and ends with quote, strip both
                        if (length(s) >= 2 && \
                            (substr(s,1,1) == "\47" || substr(s,1,1) == "\"") && \
                            substr(s,length(s),1) == substr(s,1,1)) {
                            s = substr(s, 2, length(s) - 2)
                        }
                        print s
                    }
                ')
                if [ -n "$p" ]; then
                    allow_paths+=("$p")
                fi
                ;;
        esac
    done < "$yaml_file"
fi

# --- Check each candidate against allowlist ---
violator=""
for candidate in $all_candidates; do
    # Strip leading ./ for normalization
    cand_norm=$(printf '%s' "$candidate" | sed 's|^\./||')

    # Convert Windows paths: C:/foo/bar.exe → check relative-to-allowlist
    case "$cand_norm" in
        [A-Z]:/*) cand_rel=$(printf '%s' "$cand_norm" | sed -E 's|^[A-Z]:/[^/]+/||') ;;
        /*)       cand_rel=$(printf '%s' "$cand_norm" | sed -E 's|^/[^/]+/||') ;;
        *)        cand_rel="$cand_norm" ;;
    esac

    # Check if path starts with any allowed path
    allowed=0
    for ap in "${allow_paths[@]}"; do
        # Convert to grep-friendly pattern
        ap_pattern=$(printf '%s' "$ap" | sed 's|/$||')
        case "$cand_norm" in
            "${ap_pattern}"/*|"./${ap_pattern}"/*|*"${ap_pattern}/"*)
                allowed=1
                break
                ;;
        esac
        case "$cand_rel" in
            "${ap_pattern}"/*|*"${ap_pattern}/"*)
                allowed=1
                break
                ;;
        esac
    done

    if [ "$allowed" -eq 0 ]; then
        violator="$candidate"
        break
    fi
done

if [ -n "$violator" ]; then
    cat >&2 <<EOF
[guard-installer-path] BLOCKED: installer artifact outside allowlist.

  Path:    $violator
  Allow:   ${allow_paths[*]}

  Required before retry:
    1. Move installer output to one of the allowed dirs:
       ${allow_paths[*]}
    2. Or update <project>/.claude/guard-rails.yaml:
         installer_allow_paths:
           - <your/custom/path>/
    3. If user explicitly approves writing to a system path (e.g. C:\\Program Files\\),
       AskUserQuestion first; this hook intentionally blocks unattended writes.
EOF
    # Record to .loopx/guard-events-*.jsonl (best-effort, fail-soft)
    bash "$(dirname "$0")/guard-event-writer.sh" \
        --hook guard-installer-path \
        --tool Bash \
        --reason "installer write to non-allowlisted path" \
        --input-summary "target=${parsed}" 2>/dev/null || true
    exit 2
fi

exit 0
