#!/usr/bin/env node
// Project-level Guard Rail #1 of 5.
// PreToolUse:Edit|Write|MultiEdit — block writes to secret paths.
//
// Why: 销售 BI 平台 + 装机项目都要保护 .env / *.pem / *.key / credentials.*,
// 用户级 le-gatekeeper.js 只挡 Bash 命令,挡不住 Edit/Write 写文件。
//
// Default secret path patterns (override via <project>/.claude/guard-rails.yaml):
//   *.env / *.env.* / *.pem / *.key / *.p12 / *.pfx
//   secrets/ / credentials.* / *.sqlite3 / *.db
//   .aws/* / .ssh/* / .gnupg/*
//   **/id_rsa* / **/id_dsa* / **/id_ed25519*
//
// Override config (project-level):
//   <project>/.claude/guard-rails.yaml
//   secrets_extra_patterns:
//     - "infra/grafana-secrets/"
//     - "deploy/prod-keys/\\.pem$"
//
// Install: copy to <project>/.claude/hooks/guard-secret-files.js, chmod +x,
// then add matcher to <project>/.claude/settings.json:
//   { "matcher": "Edit|Write|MultiEdit",
//     "hooks": [{ "type": "command",
//                 "command": "node <project>/.claude/hooks/guard-secret-files.js",
//                 "timeout": 2 }] }

const fs = require('fs');
const path = require('path');

// Read hook input from stdin first, fall back to env var (both supported)
let rawInput = '';
try { rawInput = require('fs').readFileSync(0, 'utf-8') || ''; } catch (e) {}
const TOOL_INPUT = rawInput || process.env.CLAUDE_TOOL_INPUT || '';

let filePath = '';
let parsed = {};
try {
  parsed = JSON.parse(TOOL_INPUT);
  filePath = parsed.tool_input?.file_path || parsed.file_path || '';
} catch (e) { process.exit(0); }

if (!filePath) process.exit(0);

// Normalize: convert Windows backslashes → forward slashes for matching
const normalized = filePath.replace(/\\/g, '/');
const basename = path.basename(normalized).toLowerCase();

// Default secret patterns
const defaultPatterns = [
  // Dotenv family
  /(^|\/)\.env(\..+)?$/i,
  /(^|\/)\.envrc$/i,
  // Crypto material
  /\.(pem|key|p12|pfx|crt|cer)$/i,
  // SSH / GPG / AWS
  /(^|\/)\.ssh\//i,
  /(^|\/)\.aws\//i,
  /(^|\/)\.gnupg\//i,
  /(^|\/)(id_rsa|id_dsa|id_ed25519|id_ecdsa)(\.pub)?$/i,
  // Database files (sales BI / analytics)
  /\.(sqlite3?|db|sqlitedb)$/i,
  // Credentials naming
  /(^|\/)credentials(\..+)?$/i,
  /(^|\/)secrets(\..+)?\.(json|ya?ml|toml|env)$/i,
  // Service account files
  /service[_-]?account.*\.json$/i,
  /gcp[_-]?key.*\.json$/i,
  /firebase[_-]?adminsdk.*\.json$/i,
];

// Load project-level overrides
const projectRoot = process.env.CLAUDE_PROJECT_DIR || process.cwd();
const projectYaml = projectRoot
  ? path.join(projectRoot, '.claude', 'guard-rails.yaml')
  : null;
let extraPatterns = [];
if (projectYaml && fs.existsSync(projectYaml)) {
  try {
    const yaml = fs.readFileSync(projectYaml, 'utf-8');
    // Minimal YAML parser: only support `  - "pattern"` lines under keys
    const lines = yaml.split('\n');
    let inSecretsKey = false;
    for (const line of lines) {
      if (/^secrets_extra_patterns:/.test(line)) {
        inSecretsKey = true;
        continue;
      }
      if (inSecretsKey) {
        const m = line.match(/^\s+-\s+["'](.+?)["']\s*$/);
        if (m) {
          try { extraPatterns.push(new RegExp(m[1])); }
          catch (e) { /* invalid regex, skip */ }
        } else if (/^[a-z_]+:/i.test(line.trim())) {
          inSecretsKey = false;
        }
      }
    }
  } catch (e) { /* read/parse failure → fall back to defaults */ }
}

const allPatterns = [...defaultPatterns, ...extraPatterns];

const matched = allPatterns.find(re => re.test(normalized) || re.test(basename));

if (matched) {
  process.stderr.write(
    '[guard-secret-files] BLOCKED: writing to secret path.\n' +
    '  Path:    ' + filePath + '\n' +
    '  Pattern: ' + matched + '\n' +
    '  Action:  Ask user to confirm intent; for test fixtures use a non-secret filename\n' +
    '           (e.g. fixtures/test_secret_sample.json, NOT real .env). Retry only\n' +
    '           after explicit approval.\n'
  );
  // Record to .loopx/guard-events-*.jsonl (best-effort, fail-soft)
// Use --input-file to avoid stdin-pipe issues on Windows.
  try {
    const _tool = parsed.tool_name || parsed.toolName || 'unknown';
    const tmpIn = require('os').tmpdir() + '/guard-event-' + process.pid + '.json';
    require('fs').writeFileSync(tmpIn, TOOL_INPUT, 'utf8');
    const res = require('child_process').spawnSync(
      'python',
      [require('path').join(__dirname, 'guard-event-writer.py'),
       '--hook', 'guard-secret-files',
       '--tool', _tool,
       '--reason', 'writing to secret path',
       '--input-summary', `file_path=${filePath.replace(/"/g, '')}`,
       '--input-file', tmpIn],
      { stdio: ['ignore', 'pipe', 'pipe'], timeout: 5000 }
    );
    try { require('fs').unlinkSync(tmpIn); } catch (_) {}
    if (res.status !== 0 && res.stderr) {
      process.stderr.write('[guard-secret-files] guard-event-writer: ' + res.stderr.toString());
    }
  } catch (e) {
    process.stderr.write('[guard-secret-files] guard-event-writer threw: ' + e.message);
  }
  process.exit(2);
}

process.exit(0);
