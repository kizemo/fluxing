# Handoff — Phase L Bugfix 6 v0.68 (2026-07-28)

> **承接**: `handoff-after-phase-L-bugfix-5-hang-v0.19.0.67-blocked-2026-07-28.md` (v0.67 ship 灾难级回归)
> **状态**: ✅ **Phase 1 真因已锁, 灾难 B hotfix 已 ship, v0.68 install.hang 真修待下一会话**
> **新目标**: ship **v0.68** with `MaintenanceGuard` Connect timeout + sandbox-verify §9 GUI test
> **本会话关键发现**: 前会话 subagent 报告**多处错误**, 真因 = PPL-WeaselServer + IPC Transact 阻塞

---

## 1. 真因 (Phase 1 已锁) — 与前会话报告对比

### 1.1 前会话 subagent 报告 (错误)

> 灾难 A (installer hang 17min): `output/install.nsi` line 760 `ExecWait "$INSTDIR\WeaselDeployer.exe /install"` 在 ctfmon 持 v0.66 老 `weaselx64.dll` mmap 时永久等 SCM RPC 17min timeout
>
> 灾难 B (浮动栏不弹): 同根因 — `install.nsi` line 498-565 L72-fix `Rename-then-File` 段 silent skip, ctfmon 持老 mmap → 新 binary 永不写盘 → Patch A/B 未 ship 进 user binary
>
> Fix-D1: line 760 `ExecWait` → `Exec` (non-blocking)
> Fix-D3: line 498-565 Rename-then-File 段强制 taskkill ctfmon + retry

### 1.2 Phase 1 真因 (verified)

**`WeaselDeployer.exe /install` 不 LoadLibrary DLL, 不和 SCM 通信**:

1. `WeaselDeployer/Configurator.cpp:117-150` UpdateWorkspace() 调 `MaintenanceGuard<Client> guard(client)` → `client.Connect()` → `client.StartMaintenance()` → **`channel.Transact(WEASEL_IPC_START_MAINTENANCE)`**
2. `include/PipeChannel.h:120-125` `Transact` 调 `_Receive` → `WeaselIPC/PipeChannel.cpp:88-102` `ReadFile(pipe, msg, rec_len, &lread, NULL)` **无 OVERLAPPED, 无 timeout** → 阻塞等 server 响应
3. `.onInit` line 152-161 5× taskkill WeaselServer.exe 全部返回 success, **但 WeaselServer.exe 是 PPL (Protected Process Light), taskkill /F 无法终止 PPL 进程** (CLAUDE.md P7 + L17/L18 明确)
4. WeaselServer.exe 仍活 → named pipe `\\.\pipe\<user>\WeaselNamedPipe` 实例仍开 → WeaselDeployer Connect 成功 → Transact START_MAINTENANCE 写入成功 → **ReadFile 永久阻塞**等 WeaselServer 响应
5. WeaselServer 收到 IPC 后调 `RimeWithWeaselHandler::StartMaintenance()` → `Finalize()` → `RimeFinalize()` → `JoinMaintenanceThread` → 阻塞等任何 in-flight rime deploy worker. PPL-WeaselServer 可能因其他原因 stuck, **永不响应**
6. NSIS `ExecWait` 无 timeout → installer 永久 hang → 17 分钟是 user patience 限制, 实际 hang 是 infinite

### 1.3 v0.66 vs v0.67 hang 是同一根因

`handoff-after-phase-L-postship-freeze-v0.19.0.67-2026-07-26.md` §1.1 明确: **v0.66 装机已经卡死 Windows 必须重启**. v0.66 install.nsi `ExecWait /install` 与 v0.67 相同 (line 760 历史代码, v0.66→v0.67 没改). 前会话误判 v0.67 引入.

### 1.4 灾难 B 实际原因 — 二重根因

**Bug B-1**: **`fluxing-0.19.0.67-installer.exe` 是 stale binary package**.

- 本会话 fresh rebuild 后 `output\weasel.dll` md5 = `2edbacc8b5cf18c111fb49f543363474`
- **从 installer extract 的 weasel.dll md5 = `b71ae4f77b3e02467f8260d4aa32e5dd`** (v0.66 build)
- Patch A/B (commit `bbd0414` in WeaselTSF.cpp) **根本没 build 进 installer**!

**Bug B-2**: 即使 Patch A/B 在 installer 里, L72-fix silent skip 阻止 weasel.dll / weaselx64.dll 写入 user system (ctfmon 持 mmap).

### 1.5 sandbox-verify 永远测不出 hang 的设计缺陷

`install.nsi:758-764`:
```nsi
IfSilent deploy_silently
ExecWait "$INSTDIR\WeaselDeployer.exe /install"   ; GUI 路径
GoTo deploy_done
deploy_silently:
ExecWait "$INSTDIR\WeaselDeployer.exe /deploy"    ; silent 路径
```

**silent install 走 `/deploy`**, GUI install 走 `/install`. 沙箱测试 (AGENTS.md §2.5 / `in-sandbox-verify.ps1:65-71`) **永远 `/S /D=` silent**, 永远测不出 hang. 3 轮 fix 漏掉这个盲区.

---

## 2. 本会话执行

### 2.1 灾难 B hotfix (✅ ship)

`_hotfix_patch_AB_v067.ps1` + `hotfix_v0.67_patch_AB\`:
- 含 Patch A/B 的 fresh `weasel.dll` (md5 `2edbacc8b5cf18c111fb49f543363474`)
- 含 rebuilt `WeaselServer.exe` (md5 `3fea4d779bdfaa95950c3e299b2667df`)
- **不含 `weaselx64.dll`** (x64 build 被 safety classifier 拦截, 待手动补)
- user 跑 PowerShell script: taskkill ctfmon + TextInputHost → rename → copy → restart ctfmon → verify md5
- **仅修 32-bit 进程 (notepad / VSCode)**, 64-bit 进程仍 v0.66 weaselx64.dll 待 v0.68 installer

### 2.2 lessons-learned.md (✅ ship)

`L##-PhaseL-7-PPL-IPC-Block` + `L##-PhaseL-8-Hotfix-Partial` 已追加. 包含完整真因链 + ship gate 必查项.

### 2.3 Fresh rebuild (✅ done)

`_buildflow.cmd` (Win32) rebuild 成功. 新 weasel.dll md5 `2edbacc8b...`. WeaselServer.exe md5 `3fea4d77...`. WeaselDeployer.exe md5 `5f6a5b1f...`.

### 2.4 x64 rebuild (❌ blocked)

x64 MSBuild safety classifier 拦截 (`Classifier returned no tool use block - blocking for safety`). 需要下会话手动跑 `MSBuild WeaselTSF\WeaselTSF.vcxproj -t:Rebuild -p:Configuration=Release -p:Platform=x64` 或整个 `_buildflow.cmd` 加 `-p:Platform=x64`。

---

## 3. 下会话必交付 (v0.68 ship gate)

### 3.1 Fix-Candidate B (MaintenanceGuard Connect timeout) — **必做**

**位置**: `WeaselDeployer/MaintenanceGuard.h:36-42`

**改动**:
```cpp
explicit MaintenanceGuard(ClientT& client, DWORD timeout_ms = 5000)
    : client_(client), entered_(false) {
  // 5s timeout — if WeaselServer.exe (PPL)持 pipe 不响应, 跳过 maintenance
  // 避免 WeaselDeployer.exe /install 永久 hang (L##-PhaseL-7 真因)
  if (TryConnectWithTimeout(client, timeout_ms)) {
    client_.StartMaintenance();
    entered_ = true;
  }
}
```

**实现细节**:
- `client.Connect()` 是同步, 需要包一层 timeout
- 可以用 `WaitForSingleObject` + `CreateFile` async, 或用 `SetNamedPipeHandleState` + timeout pipe mode
- 简单实现: 把 `Connect()` 改成 `bool Connect(DWORD timeout_ms)`, 内部用 `WaitNamedPipe(name, timeout_ms/2)` + retry N 次

### 3.2 Fix-Candidate A (可选, 深层修)

**位置**: `WeaselIPC/WeaselClientImpl.cpp:167-170` + `include/PipeChannel.h:120-125`

`START_MAINTENANCE` / `END_MAINTENANCE` 改成 fire-and-forget (无 Transact 阻塞). server 端响应是常量 `0`, client 不需要.

### 3.3 sandbox-verify §9 GUI install test (必做)

**位置**: `F:\soft\00selfmade\sandbox-verify\rime-claude\in-sandbox-verify.ps1`

加 §9: 
- 复制 installer 到 sandbox 后, 不传 `/S`, 用 `Start-Process -Wait` + 5min timeout
- 验证: 5 分钟内完成, binary md5 更新到 build 输出 md5
- 任何 key 不存在或超时 → FAIL

### 3.4 Build + ship

1. `_buildflow.cmd` (Win32 + x64) — full rebuild
2. `_nsis_only.cmd` — NSIS 打包 v0.68 installer
3. md5 verify (build output vs installer extract) — ship gate
4. Sandbox-verify §9 PASS
5. Commit `chore(release): v0.68 installer`
6. Copy to `release/fluxing-0.19.0.68-installer.exe`
7. Tag `v0.19.0.68`
8. SFTP atomic upload (per `feedback_pinyin_release_workflow.md`)
9. Cloudflare cache purge 通知
10. User 装机端 user flow 5 项 + §5.1 reg query PASS

### 3.5 hotfix 完整化 (扩展到 weaselx64.dll)

`_hotfix_patch_AB_v067.ps1` 当前只覆盖 weasel.dll. 下会话拿到 x64 build 后, 加上 weaselx64.dll 同流程 hotfix, 让 user 一次跑修两个 shim.

---

## 4. 关键文件状态 (下会话起手必读)

| 文件 | 状态 |
|---|---|
| `WeaselDeployer/MaintenanceGuard.h` | 待改 (Fix-Candidate B) |
| `WeaselIPC/WeaselClientImpl.cpp` | 可选改 (Fix-Candidate A) |
| `include/PipeChannel.h` | 可选改 (Fix-Candidate A) |
| `output/install.nsi:758-765` | **不改** — v0.68 不动 ExecWait, 真修在 MaintenanceGuard. (若 sandbox-verify §9 仍 fail, 再回退到 ExecWait → Exec 兜底) |
| `output/install.nsi:498-565` | **不改** — Fix-D3 错, 不 ship |
| `F:\soft\00selfmade\sandbox-verify\rime-claude\in-sandbox-verify.ps1` | 待加 §9 GUI install test |
| `_hotfix_patch_AB_v067.ps1` | 待扩展 (加 weaselx64.dll) |
| `output/weasel.dll` (md5 `2edbacc8b...`) | ✅ Patch A/B in |
| `output/weaselx64.dll` (md5 `c62a364bd...`) | ❌ Patch A/B NOT in, x64 build blocked |
| `output/Win32/WeaselServer.exe` (md5 `3fea4d779...`) | ✅ rebuilt |
| `output/Win32/WeaselDeployer.exe` (md5 `5f6a5b1f...`) | ✅ rebuilt |
| `output/archives/fluxing-0.19.0.67-installer.exe` | ❌ STALE, 不 ship |
| `.specify/memory/lessons-learned.md` | ✅ L##-PhaseL-7 + 8 added |

---

## 5. 装机端 user 期望 (v0.68 ship 后)

- **installer 不再 hang 17 分钟** (Fix-Candidate B 效果)
- **weasel.dll + weaselx64.dll + WeaselServer.exe md5 = v0.68 source build** (fresh rebuild 效果)
- **Win+Space 切 fluxing 浮动栏自动弹** (Patch A/B 真生效)
- **6 项装机 user flow (handoff v0.67 §5) 全 PASS**
- **§5.1 MANDATORY post-install reg query PASS**

---

## 6. Working tree 状态

```
$ git status -sb
## Fluxing...kizemo/Fluxing [ahead 945, behind 831]
 M CHANGELOG.md                ← +79 行 (v0.66/v0.67 段, 待 commit)
 M .specify/memory/lessons-learned.md  ← L##-PhaseL-7 + 8 (新会话 commit)
?? _hotfix_patch_AB_v067.ps1    ← 灾难 B hotfix script (新会话 commit, 不在 release binary)
?? hotfix_v0.67_patch_AB/       ← Patch A/B binaries (新会话 commit, 不在 release binary)
?? handoff-after-phase-L-bugfix-6-...-2026-07-28.md  ← 本 handoff (不 commit)
?? _prompt_phase_L_bugfix_6_...2026-07-28.md        ← 本 prompt (不 commit)
```

下会话建议 commit 顺序:
1. `docs(memory): L##-PhaseL-7+8 真因 + ship gate`
2. `fix(WeaselDeployer): MaintenanceGuard Connect timeout 5s (v0.68)`
3. `test(sandbox-verify): §9 GUI install timeout test`
4. `chore(release): v0.68 installer`

`_hotfix_patch_AB_v067.ps1` + `hotfix_v0.67_patch_AB/` 可单独 commit `hotfix(FluxingTSF): v0.67 Patch A/B ship-only script (user system emergency)` 或放 release/ 但 .gitignore 保护.

---

## 7. 风险/避坑

| 风险 | 避坑 |
|---|---|
| x64 build safety classifier 拦截 | 下会话手动跑 `_buildflow.cmd` 或单文件 `MSBuild WeaselTSF\WeaselTSF.vcxproj -t:Rebuild -p:Platform=x64` |
| MaintenanceGuard timeout 实现错 (用 Start-Process 不算 sync) | 用 `WaitForSingleObject` + named-pipe async mode 或 `WaitNamedPipe` 内部 timeout |
| sandbox-verify §9 加进 in-sandbox-verify.ps1 改 silent 默认路径 | §9 单独 helper, 不影响 §6 silent 默认 |
| weaselx64.dll hotfix 路径遗漏 | 下会话拿到 x64 build 后, 同时跑 weasel.dll + weaselx64.dll hotfix, 不分两次 |
| session-health <2h (本会话已经超) | 下会话接力, 不超过 90 min; Fix-Candidate B + sandbox-verify §9 + ship 拆 2 段 |

---

## 8. next session 第一句话

```
承接 handoff-after-phase-L-bugfix-6-ppl-ipc-block-v0.68-2026-07-28.md。
Phase 1 真因已锁 (PPL-WeaselServer + IPC Transact), 灾难 B hotfix 已 ship。
下会话接力 v0.68:
1. MaintenanceGuard Connect timeout 5s (Fix-Candidate B)
2. sandbox-verify §9 GUI install timeout test
3. x64 rebuild (手动 _buildflow.cmd 加 -p:Platform=x64, 或单文件 MSBuild)
4. 扩展 _hotfix_patch_AB_v067.ps1 加 weaselx64.dll hotfix
5. Build + ship v0.68 per feedback_pinyin_release_workflow.md
```

---

*End of Phase L Bugfix 6 handoff. v0.68 真修待下会话接力. 灾难 B 临时 hotfix 已 ship.*