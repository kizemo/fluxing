---
name: security-and-hardening
description: Harden code against vulnerabilities. Treat every external input as hostile, every secret as sacred, every authorization check as mandatory. Use when handling user input, file paths, registry, IPC, or external integrations.
---

# Security and Hardening

> Security isn't a phase — it's a constraint on every line of code that touches user data, auth, or external systems.

## When to use

- Handling user input (file path, env var, registry value, IPC message)
- Implementing auth/authorization
- Storing or transmitting secrets
- External API integration
- File uploads, webhooks, callbacks
- IPC message parsing (Fluxing: WeaselIPC)

## Threat model for Fluxing

| Threat | Surface | Mitigation |
|---|---|---|
| Malicious file path in user YAML | `data/*.yaml` load | Validate path doesn't contain `..`, normalize, check root |
| Malicious IPC message | Named pipe `\\.\pipe\<user>\...` | SID-scoped SA (already in place); reject malformed `key=value` |
| Local privilege escalation | Service start | WeaselServer runs as user, not SYSTEM |
| Sensitive data in logs | `output/log/` | Redact user input, schema name is OK, candidate text is not |
| Installer silent-mode flag injection | NSIS `/D=<path>` | L17 fix; do not concat unknown CLI args into $INSTDIR |
| HKLM registry write without admin | installer | imesetup.cpp uses ShellExecuteEx with `runas` |

## Hard rules for this project

- **Untrusted input** = any file content, IPC message, registry value, env var
- **Sensitive output** = never log: user candidate text, password fields, anything in `phrases.json`
- **Path validation** = `std::filesystem::canonical` + check `is_subpath` against expected root
- **No `system()` / `ShellExecute` with user-supplied command** (project uses fixed commands only)

## Pipe SA pattern (L23)

```cpp
// WeaselIPCServer/SecurityAttribute.cpp
SECURITY_ATTRIBUTES sa = {};
sa.nLength = sizeof(sa);
sa.bInheritHandle = FALSE;
sa.lpSecurityDescriptor = BuildUserSidScopedSd(currentUserSid);
// Caller must CloseHandle + free sd
```

Don't bypass this. Don't write to the pipe from outside WeaselIPCServer.

## Registry pattern (L17)

- HKLM writes go through `RimeWithWeaselHandler` or `WeaselSetup/imesetup.cpp`
- HKCU writes are OK from user-level code
- Never write registry from WeaselServer hot path (causes disk I/O on every key event)

## Install / NSIS (L09 / L13 / L17 / L58)

- `InstallDirRegKey` is `.onInit` evaluated before user can change → use `/D=` carefully
- `${If} ${RunningX64}` is a footgun (L11/L14) — don't use for Weasel binaries
- Silent mode (`/S`) does NOT trigger `MUI_PAGE_CUSTOMFUNCTION_LEAVE` (L13)
- User-data path: HKCU `Software\Fluxing\Weasel\RimeUserDir` (NOT NSIS `/userdir=` CLI arg)

## Secret management

- `github_token.txt` (if exists) NEVER commit (.gitignore covers `release/*token*`)
- No SSH keys in chat / commit / log
- Use Windows Credential Manager for any local-only tokens

## L## cross-ref

- L01/L02/L40/L45: byte-level file handling (avoid PowerShell string API for sensitive files)
- L17: install.nsi CLI flag injection
- L23: pipe security
- L54: silent install /D= with stale InstallDirRegKey

## Anti-patterns

- `sprintf(buf, user_input)` (use `std::string` or `swprintf_s`)
- Catch-all `catch (...)` (hides real error)
- Logging full exception including user data
- Trusting `getenv` without bounds check
- Writing user input directly to registry or file path
