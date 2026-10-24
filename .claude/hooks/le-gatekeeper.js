#!/usr/bin/env node
// Loop Engineering L4 Gatekeeper hook (LE-Gatekeeper) — Fluxing project scope.
// Same logic as global `~/.claude/hooks/le-gatekeeper.js`, plus Fluxing-specific
// blocking of NSIS-installer-related destructive patterns (reg query hijack, etc.).
//
// Install path (project-level): this file is ready to be referenced from
// `~/.claude/settings.json` (or a future `F:\soft\00selfmade\rime_claude\.claude\settings.json`).
// See `handoff-loop-engineering-2026-07-18.md` for the exact matcher payload.

const TOOL_INPUT = process.env.CLAUDE_TOOL_INPUT || '{}';
let cmd = '';
try { cmd = JSON.parse(TOOL_INPUT).command || ''; } catch (e) { process.exit(0); }

// Generic destructive (cross-platform)
const generic = /\brm\s+-rf\s+\/|\bDrop\s+(Table|Database)\b|\bDELETE\s+FROM\b|\bformat\s+[a-zA-Z]:/i;

// Fluxing-specific: NSIS registry mass-wipe (L54 must be cleared before install,
// never silently). Pattern blocks any `reg delete ... Fluxing ... Weasel ... /f`
// sequence regardless of intermediate path separators (`\` or `/`) and quoted args.
// Single-line scope (`[^\n]*?`) keeps us from trailing onto unrelated commands;
// `/f` flag must be the actual reg-delete force flag, not an arg.
const nsisMassWipe = /\breg\s+delete\s+[^\n]*?Fluxing[\\\/][^\n]*?Weasel[^\n]*?\s\/f/i;

if (generic.test(cmd)) {
  process.stderr.write(
    '[LE-Gatekeeper] Destructive Bash detected — AskUserQuestion required.\n' +
    '  Cmd: ' + cmd.slice(0, 200) + '\n'
  );
  process.exit(2);
}
if (nsisMassWipe.test(cmd)) {
  process.stderr.write(
    '[LE-Gatekeeper] NSIS registry mass-wipe detected (L54 borderline). Confirm with user.\n' +
    '  Cmd: ' + cmd.slice(0, 200) + '\n' +
    '  Reference: AGENTS.md §2.5 silent-install smoke test step 0 (clear HKLM\\Software\\Fluxing\\Weasel before install).\n'
  );
  process.exit(2);
}

process.exit(0);
