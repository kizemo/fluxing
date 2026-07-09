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

# Cross-process IPC

Wire protocol in `include/WeaselIPCData.h` (`weasel::Status`, `Context`,
`Response`, `TextRange` 等 POD), 格式 `key=value` + `.\n` 终止, per-user
named-pipe transport via `PipeChannel`.

## Hard rule — change in lockstep

`WeaselIPCData.h` 任何变更必须一次性同步 4 处 + 一个 ADR + 一个测试:

| 端 | 文件 |
|---|---|
| Wire structs | `include/WeaselIPCData.h` |
| Server (writer) | `WeaselServer/`, `WeaselIPCServer/` |
| TSF (reader) | `WeaselTSF/` |
| Deployer (reader) | `WeaselDeployer/` |
| ADR | `docs/adr/NNNN-*.md` (MADR) |
| Test | `test/TestWeaselIPC/<...>.cpp` |

不对称变更 (e.g. 只在 server 端读新字段) → 停下写 ADR 说明为何安全, 不要静默放过。

## TSF thread — must not block I/O

P2: `ITfTextInputProcessor::OnXxx` 回调内禁止 `CreateFile` / 同步注册表 /
`BeginWaitForSingleObject`。长操作 → `WeaselServer` 单 supervisor (P7) →
通过 pipe 异步回。

> L10 / L17 / L18 / L21 反复打破过这条。

## Concurrency

- `WeaselServer.exe` 是 **PPL** — `taskkill /F` 无效;累计二进制变更后
  inner-loop 测试需 logout/reboot (L17 / L18)。
- `RimeLeversApi::{export,import}_user_dict` **必须** `client.StartMaintenance()` → 操作 → `client.EndMaintenance()`, 否则 leveldb LOCK 拒绝。
