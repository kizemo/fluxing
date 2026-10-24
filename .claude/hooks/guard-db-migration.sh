#!/usr/bin/env bash
# Project-level Guard Rail #3 of 5.
# PreToolUse:Bash — block irreversible database migrations.
#
# Why: 销售 BI 平台用 alembic / Django / prisma / dbt,生产迁移一旦跑错
# 不可逆。强制先 dry-run 或 plan,且要求工作区 clean。
#
# Blocks:
#   alembic upgrade <rev>            (without --sql dry-run)
#   python manage.py migrate         (without --plan)
#   prisma migrate deploy            (only allows `prisma migrate dev` for dev)
#   dbt run --select <tag>           (production tags blocked)
#   psql -c "DROP TABLE ..."         (already covered by le-gatekeeper.js, but
#                                     added here for projects without user-level hook)
#   mysql ... DROP DATABASE
#
# Allowlist (always pass):
#   alembic upgrade --sql            (dry-run)
#   alembic history / alembic current
#   prisma migrate dev
#   dbt run --select tag:dev
#   psql ... SELECT ...
#
# Install: copy to <project>/.claude/hooks/guard-db-migration.sh,
# chmod +x, add matcher:
#   { "matcher": "Bash",
#     "hooks": [{ "type": "command",
#                 "command": "bash <project>/.claude/hooks/guard-db-migration.sh",
#                 "timeout": 3 }] }

set -u
input=$(cat)

# Parse command via python (more reliable than bash regex)
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

# Normalize whitespace
cmd_norm=$(printf '%s' "$cmd" | tr -s ' \t')

# --- Allowlist short-circuits ---
# Always allow dry-run / plan / read-only variants
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

# Allow dry-run / plan / read-only (return 0 to pass)
if allow_match \
    'alembic[[:space:]]+upgrade[[:space:]]+--sql' \
    'alembic[[:space:]]+(history|current|show|stamp|heads|branches)' \
    'prisma[[:space:]]+migrate[[:space:]]+(dev|status|diff|resolve)' \
    'prisma[[:space:]]+migrate[[:space:]]+deploy[[:space:]]+--dry-run' \
    'manage\.py[[:space:]]+migrate[[:space:]]+--plan' \
    'dbt[[:space:]]+(run|build)[[:space:]]+--select[[:space:]]+tag:dev' \
    'dbt[[:space:]]+(parse|deps|compile|docs|seed|snapshot)' \
    '^[[:space:]]*SELECT[[:space:]]' \
    '^[[:space:]]*EXPLAIN[[:space:]]' \
    '^[[:space:]]*\\\\d[[:space:]]' \
    '^[[:space:]]*\\\\(dt|dv)[[:space:]]'; then
    exit 0
fi

# --- Blocklist patterns ---
blocked=""

# alembic upgrade (without --sql)
if printf '%s' "$cmd_norm" | grep -qE '\balembic[[:space:]]+upgrade\b'; then
    if ! printf '%s' "$cmd_norm" | grep -qE 'alembic[[:space:]]+upgrade.*--sql'; then
        blocked="alembic upgrade without --sql dry-run"
    fi
fi

# Django migrate (without --plan)
if printf '%s' "$cmd_norm" | grep -qE 'manage\.py[[:space:]]+migrate\b'; then
    if ! printf '%s' "$cmd_norm" | grep -qE 'manage\.py[[:space:]]+migrate[[:space:]]+--plan\b'; then
        blocked="python manage.py migrate without --plan"
    fi
fi

# prisma migrate deploy (production)
if printf '%s' "$cmd_norm" | grep -qE '\bprisma[[:space:]]+migrate[[:space:]]+deploy\b'; then
    if ! printf '%s' "$cmd_norm" | grep -qE '--dry-run'; then
        blocked="prisma migrate deploy (production)"
    fi
fi

# dbt run/build with prod tag
if printf '%s' "$cmd_norm" | grep -qE '\bdbt[[:space:]]+(run|build)\b'; then
    if printf '%s' "$cmd_norm" | grep -qE 'tag:prod'; then
        blocked="dbt run/build with tag:prod"
    fi
fi

# Direct SQL DROP (covers gaps where user-level le-gatekeeper.js is not installed)
if printf '%s' "$cmd_norm" | grep -qiE '\b(DROP[[:space:]]+(TABLE|DATABASE|SCHEMA))\b'; then
    blocked="DROP TABLE/DATABASE/SCHEMA detected"
fi

if [ -n "$blocked" ]; then
    cat >&2 <<EOF
[guard-db-migration] BLOCKED: irreversible DB migration.

  Reason: $blocked
  Cmd:    ${cmd:0:200}

  Required before retry:
    1. Run dry-run/plan first (alembic upgrade --sql, manage.py migrate --plan,
       prisma migrate deploy --dry-run, dbt run --select tag:dev).
    2. Confirm with user via AskUserQuestion — show migration diff.
    3. Ensure git working tree is clean (git status --porcelain empty).
    4. Re-run with explicit approval.
EOF
    # Record to .loopx/guard-events-*.jsonl (best-effort, fail-soft)
    bash "$(dirname "$0")/guard-event-writer.sh" \
        --hook guard-db-migration \
        --tool Bash \
        --reason "destructive db migration" \
        --input-summary "cmd=${cmd:0:120}" \
        --input-summary "cmd=${cmd:0:120}" 2>/dev/null || true
    exit 2
fi

exit 0
