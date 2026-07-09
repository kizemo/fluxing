---
name: observability-and-instrumentation
description: Make production behavior visible. Add logging / metrics / traces alongside the feature, not after. Use when shipping any feature that runs in production.
---

# Observability and Instrumentation

> Code you can't observe is code you can't operate. If a feature ships without telemetry, the first user-reported bug becomes archaeology instead of a query.

## When to use

- Building any feature that runs in production
- Adding a new endpoint / service / background job / external integration
- Production incident took too long to diagnose
- Setting up alerting

## When NOT to use

- Tests (use assertion output)
- Pure build/CI steps

## Fluxing telemetry stack

| Layer | Tool | Output |
|---|---|---|
| **Runtime log** | rime engine log | `%TEMP%\rime.weasel\rime.log` |
| **Debug stream** | `include/WeaselUtility.h::DebugStream` | OutputDebugString (DebugView) |
| **Structured log** | `LOG / DLOG` (`include/logging.h`) | glog stream (INFO/WARNING/ERROR) |
| **Crash dump** | WER LocalDumps | `HKLM\...\LocalDumps\WeaselServer.exe` (per L## L43) |
| **WinSparkle** | Update check | `update/appcast.xml` |

## What to instrument

For every new code path, log:
- **Entry**: function name, key params (NOT user input text)
- **Exit**: status (success / error code), duration
- **Error path**: full error context, stack (in debug build)

For background work (deploy / sync / leveldb compaction):
- Start time
- Item count
- End time + duration
- Errors (with retry count)

## Log message conventions

```
[MODULE] EVENT [params]
```

Example:
```
[WeaselServer] DeploySchema start schema=rime_ice
[WeaselServer] DeploySchema success schema=rime_ice duration_ms=1247
[WeaselServer] DeploySchema FAIL schema=rime_ice error="leveldb LOCK timeout"
```

## What NOT to log

- User candidate text (privacy)
- Passwords / API keys
- Full file paths containing user data
- Rime private state (per constitution: "no input history sync")

## Performance

- Logs in hot path (60 FPS) → batch + flush at end of frame, not per-line
- Use `DLOG` (debug only) for verbose traces
- Use `LOG(INFO)` for ship-time events
- Use `LOG(ERROR)` for errors that should alert

## Alerting

For Fluxing, alerting is **not** a separate system (no Sentry, no PagerDuty — single-developer project).

Instead, "alerting" = L## entry in lessons-learned.md when an issue is found in the wild.

## Adding a new log channel

1. Define in `include/WeaselUtility.h` (DebugStream) or `include/logging.h` (LOG)
2. Use it from the new code path
3. Update L## if the new channel reveals a pattern

## L## cross-ref

- L42: byte-level verification of dark-mode bridge
- L48: test exit mode (`ExitProcess(rc)` to skip atexit)
- L49: ATL message map runtime construct — debug via `DebugStream` hook
- L52: D2D backing store backing error
