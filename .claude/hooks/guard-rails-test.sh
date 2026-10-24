#!/usr/bin/env bash
# Unit tests for 5 project-level guard rail hook templates.
# Run from any directory: bash ~/.claude/hooks/templates/guard-rails-test.sh
#
# Each test simulates a hook input via stdin (Claude Code passes JSON).
# Asserts: exit code (0 = pass, 2 = block) + stderr contains expected marker.

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PASS=0
FAIL=0
TOTAL=0

red()    { printf '\033[31m%s\033[0m' "$1"; }
green()  { printf '\033[32m%s\033[0m' "$1"; }
yellow() { printf '\033[33m%s\033[0m' "$1"; }

# Simulate hook input: tool_name + tool_input fields
make_input() {
    local tool_name="$1"
    local tool_input_json="$2"
    printf '{"tool_name":"%s","tool_input":%s}' "$tool_name" "$tool_input_json"
}

# Test runner: expects <name> <expected_exit> <expected_marker_in_stderr> <cmd_to_run> <input>
run_test() {
    local name="$1"
    local expected_exit="$2"
    local expected_marker="$3"
    local hook_cmd="$4"
    local input="$5"

    TOTAL=$((TOTAL + 1))
    local stderr_file
    stderr_file=$(mktemp)
    local actual_exit=0
    eval "$hook_cmd" >/dev/null 2>"$stderr_file" <<< "$input" || actual_exit=$?
    local actual_stderr
    actual_stderr=$(cat "$stderr_file")
    rm -f "$stderr_file"

    if [ "$actual_exit" -eq "$expected_exit" ] && \
       { [ -z "$expected_marker" ] || printf '%s' "$actual_stderr" | grep -qF "$expected_marker"; }; then
        green "  PASS"; printf ' %s (exit=%d)\n' "$name" "$actual_exit"
        PASS=$((PASS + 1))
    else
        red "  FAIL"; printf ' %s\n    expected exit=%d stderr~=%q\n    actual   exit=%d stderr=%q\n' \
            "$name" "$expected_exit" "$expected_marker" "$actual_exit" "$actual_stderr"
        FAIL=$((FAIL + 1))
    fi
}

echo "$(yellow '== guard-secret-files.js ==')"

run_test "allow: regular .md file" 0 "" \
    "node '$SCRIPT_DIR/guard-secret-files.js'" \
    "$(make_input Edit '{"file_path":"docs/notes.md"}')"

run_test "block: .env file" 2 "guard-secret-files" \
    "node '$SCRIPT_DIR/guard-secret-files.js'" \
    "$(make_input Write '{"file_path":"src/.env"}')"

run_test "block: .pem file" 2 "guard-secret-files" \
    "node '$SCRIPT_DIR/guard-secret-files.js'" \
    "$(make_input Edit '{"file_path":"certs/server.pem"}')"

run_test "block: sqlite3 file" 2 "guard-secret-files" \
    "node '$SCRIPT_DIR/guard-secret-files.js'" \
    "$(make_input Write '{"file_path":"data/app.sqlite3"}')"

run_test "block: Windows path with backslash" 2 "guard-secret-files" \
    "node '$SCRIPT_DIR/guard-secret-files.js'" \
    "$(make_input Write '{"file_path":"C:\\Users\\foo\\credentials.json"}')"

run_test "allow: file_path empty (skip)" 0 "" \
    "node '$SCRIPT_DIR/guard-secret-files.js'" \
    "$(make_input Write '{}')"

echo
echo "$(yellow '== guard-main-branch-push.py ==')"

run_test "allow: git push origin feature-branch" 0 "" \
    "python '$SCRIPT_DIR/guard-main-branch-push.py'" \
    "$(make_input Bash '{"command":"git push origin feature-x"}')"

run_test "block: git push origin main" 2 "guard-main-branch-push" \
    "python '$SCRIPT_DIR/guard-main-branch-push.py'" \
    "$(make_input Bash '{"command":"git push origin main"}')"

run_test "block: git push origin master" 2 "guard-main-branch-push" \
    "python '$SCRIPT_DIR/guard-main-branch-push.py'" \
    "$(make_input Bash '{"command":"git push origin master"}')"

run_test "block: git push -f" 2 "guard-main-branch-push" \
    "python '$SCRIPT_DIR/guard-main-branch-push.py'" \
    "$(make_input Bash '{"command":"git push -f origin dev"}')"

run_test "allow: --dry-run push" 0 "" \
    "python '$SCRIPT_DIR/guard-main-branch-push.py'" \
    "$(make_input Bash '{"command":"git push --dry-run origin main"}')"

run_test "allow: non-git command" 0 "" \
    "python '$SCRIPT_DIR/guard-main-branch-push.py'" \
    "$(make_input Bash '{"command":"echo hello"}')"

echo
echo "$(yellow '== guard-db-migration.sh ==')"

run_test "allow: alembic upgrade --sql dry-run" 0 "" \
    "bash '$SCRIPT_DIR/guard-db-migration.sh'" \
    "$(make_input Bash '{"command":"alembic upgrade head --sql"}')"

run_test "block: alembic upgrade head" 2 "guard-db-migration" \
    "bash '$SCRIPT_DIR/guard-db-migration.sh'" \
    "$(make_input Bash '{"command":"alembic upgrade head"}')"

run_test "block: manage.py migrate" 2 "guard-db-migration" \
    "bash '$SCRIPT_DIR/guard-db-migration.sh'" \
    "$(make_input Bash '{"command":"python manage.py migrate"}')"

run_test "allow: manage.py migrate --plan" 0 "" \
    "bash '$SCRIPT_DIR/guard-db-migration.sh'" \
    "$(make_input Bash '{"command":"python manage.py migrate --plan"}')"

run_test "block: prisma migrate deploy" 2 "guard-db-migration" \
    "bash '$SCRIPT_DIR/guard-db-migration.sh'" \
    "$(make_input Bash '{"command":"prisma migrate deploy"}')"

run_test "block: dbt run tag:prod" 2 "guard-db-migration" \
    "bash '$SCRIPT_DIR/guard-db-migration.sh'" \
    "$(make_input Bash '{"command":"dbt run --select tag:prod"}')"

run_test "allow: dbt run tag:dev" 0 "" \
    "bash '$SCRIPT_DIR/guard-db-migration.sh'" \
    "$(make_input Bash '{"command":"dbt run --select tag:dev"}')"

run_test "block: DROP TABLE" 2 "guard-db-migration" \
    "bash '$SCRIPT_DIR/guard-db-migration.sh'" \
    "$(make_input Bash '{"command":"psql -c \"DROP TABLE users;\""}')"

run_test "allow: SELECT query" 0 "" \
    "bash '$SCRIPT_DIR/guard-db-migration.sh'" \
    "$(make_input Bash '{"command":"psql -c \"SELECT * FROM users;\""}')"

echo
echo "$(yellow '== guard-package-publish.sh ==')"

run_test "allow: npm publish --dry-run" 0 "" \
    "bash '$SCRIPT_DIR/guard-package-publish.sh'" \
    "$(make_input Bash '{"command":"npm publish --dry-run"}')"

run_test "block: npm publish" 2 "guard-package-publish" \
    "bash '$SCRIPT_DIR/guard-package-publish.sh'" \
    "$(make_input Bash '{"command":"npm publish"}')"

run_test "allow: npm publish --tag next" 0 "" \
    "bash '$SCRIPT_DIR/guard-package-publish.sh'" \
    "$(make_input Bash '{"command":"npm publish --tag next"}')"

run_test "block: twine upload" 2 "guard-package-publish" \
    "bash '$SCRIPT_DIR/guard-package-publish.sh'" \
    "$(make_input Bash '{"command":"twine upload dist/*"}')"

run_test "allow: twine upload --repository testpypi" 0 "" \
    "bash '$SCRIPT_DIR/guard-package-publish.sh'" \
    "$(make_input Bash '{"command":"twine upload --repository testpypi dist/*"}')"

run_test "block: vsce publish" 2 "guard-package-publish" \
    "bash '$SCRIPT_DIR/guard-package-publish.sh'" \
    "$(make_input Bash '{"command":"vsce publish"}')"

run_test "block: gh release create" 2 "guard-package-publish" \
    "bash '$SCRIPT_DIR/guard-package-publish.sh'" \
    "$(make_input Bash '{"command":"gh release create v1.0.0"}')"

run_test "allow: gh release create --draft" 0 "" \
    "bash '$SCRIPT_DIR/guard-package-publish.sh'" \
    "$(make_input Bash '{"command":"gh release create v1.0.0 --draft"}')"

run_test "block: cargo publish" 2 "guard-package-publish" \
    "bash '$SCRIPT_DIR/guard-package-publish.sh'" \
    "$(make_input Bash '{"command":"cargo publish"}')"

run_test "allow: cargo publish --dry-run" 0 "" \
    "bash '$SCRIPT_DIR/guard-package-publish.sh'" \
    "$(make_input Bash '{"command":"cargo publish --dry-run"}')"

echo
echo "$(yellow '== guard-installer-path.sh ==')"

run_test "allow: write to target/release/bundle/" 0 "" \
    "bash '$SCRIPT_DIR/guard-installer-path.sh'" \
    "$(make_input Write '{"file_path":"target/release/bundle/installer.exe"}')"

run_test "allow: write to release/installer.exe" 0 "" \
    "bash '$SCRIPT_DIR/guard-installer-path.sh'" \
    "$(make_input Write '{"file_path":"release/installer.exe"}')"

run_test "block: write to /usr/bin/installer.exe" 2 "guard-installer-path" \
    "bash '$SCRIPT_DIR/guard-installer-path.sh'" \
    "$(make_input Write '{"file_path":"/usr/bin/installer.exe"}')"

run_test "block: write to C:\\Windows\\installer.exe" 2 "guard-installer-path" \
    "bash '$SCRIPT_DIR/guard-installer-path.sh'" \
    "$(make_input Write '{"file_path":"C:\\Windows\\installer.exe"}')"

run_test "block: Bash command writing to root" 2 "guard-installer-path" \
    "bash '$SCRIPT_DIR/guard-installer-path.sh'" \
    "$(make_input Bash '{"command":"curl -o /usr/local/bin/installer.exe https://example.com/installer.exe"}')"

run_test "allow: write .md file" 0 "" \
    "bash '$SCRIPT_DIR/guard-installer-path.sh'" \
    "$(make_input Write '{"file_path":"docs/readme.md"}')"

run_test "allow: no installer files in input" 0 "" \
    "bash '$SCRIPT_DIR/guard-installer-path.sh'" \
    "$(make_input Bash '{"command":"echo hello"}')"

echo
echo "$(yellow '=== Summary ===')"
TOTAL_TESTED=$((PASS + FAIL))
if [ "$FAIL" -eq 0 ]; then
    green "  $PASS / $TOTAL_TESTED passed"
    exit 0
else
    red "  $PASS passed, $FAIL failed (total $TOTAL_TESTED)"
    exit 1
fi
