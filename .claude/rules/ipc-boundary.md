---
paths:
  - "WeaselIPC/**/*.cpp"
  - "WeaselIPC/**/*.h"
  - "WeaselIPCServer/**/*.cpp"
  - "WeaselIPCServer/**/*.h"
  - "include/WeaselIPCData.h"
  - "**/*IPCData*"
  - "**/*PipeChannel*"
---

# Cross-process IPC boundary rules

Cross-process boundaries in this project are **sealed and slow to change**.
Treat any IPC change as architecture work, not refactor work.

## Wire protocol (current)

Per-project protocols live in `include/WeaselIPCData.h`
(`weasel::Status`, `weasel::Context`, `weasel::Response`, `weasel::TextRange`).
On-the-wire format is `key=value` lines + `.\n` terminator.
Transport is per-user named pipe (`\\.\pipe\<user>...`) via `PipeChannel`.

## Hard rule — change the protocol in lockstep

A new IPC message type, a new field on an existing struct, or a renamed
enum value **must** be updated in **all four** call sites **at once**:

| Layer | Files |
|---|---|
| Wire structs | `include/WeaselIPCData.h` |
| Server (writer / owner of the rime session) | `WeaselServer/`, `WeaselIPCServer/` |
| TSF (reader) | `WeaselTSF/` |
| Deployer (reader) | `WeaselDeployer/` |

Then:

1. Open an ADR first (`docs/adr/NNNN-*.md`, MADR format). Cite spec / issue.
2. Update `CHANGELOG.md` under `### 主要更新` (since this is a protocol break
   visible to anyone tracing `WeaselServer.exe`).
3. Add or extend a test under `test/TestWeaselIPC/` to lock the new message
   shape (P2 + R2).

If the change is asymmetric (e.g. only the server reads a new field), **stop**
and write an ADR describing why the asymmetry is safe — do not proceed
silently.

## TSF thread — never block

Per constitution P2 (`constitution.md` §Project-Specific Rules), the TSF
thread must never block on I/O. Specifically:

- No `std::ifstream` / `CreateFile` / synchronous registry call inside an
  `ITfTextInputProcessor::OnXxx` callback.
- No `BeginWaitForSingleObject` on the TSF thread.
- Long operations must be deferred to `WeaselServer` (single supervisor per P7)
  and returned asynchronously through the pipe.

History: this rule has been broken before — see `lessons-learned.md` L10,
L17, L18, L21.

## Concurrency caveats

- `WeaselServer` is **PPL** on Windows; you cannot `taskkill /F` it. Inner-loop
  testing requires logout/reboot per cumulative binary change (L17/L18).
- User-dictionary operations (`export_user_dict`, `import_user_dict`) **must**
  be wrapped in `client.StartMaintenance()` / `client.EndMaintenance()` —
  otherwise the leveldb LOCK held by `WeaselServer` denies the access.
