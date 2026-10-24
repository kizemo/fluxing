# Handoff — Phase L Bugfix 8.2 installer finalization freeze (2026-07-29)

> **承接**: `handoff-after-phase-L-bugfix-8.1-v0.71-corrected-2026-07-29.md`
> **状态**: 🚨 **新 bug** — v0.71 corrected 装机最后一步"完成"dialog → OK → 系统无响应
> **新目标**: 确认是 WeaselServer init hang 还是 TSF OnActivated storm,装机 log capture

---

## 1. 一句话总结 (大白话)

v0.71 corrected 装到最后一步,弹"安装完成, 需要注销重启"对话框,**点确定按钮 → 系统立即无响应**, 必须调出任务管理器 kill WeaselServer.exe 才能恢复。**这不是 v0.70 同款 freeze** (那个是 boot 后 TSF OnActivated 风暴致 Echo 永久阻塞, 系统级 input pipeline 卡死)。这次是 **installer finalization 阶段就触发的 WeaselServer hang**, 跟 v0.70 的"boot 才触发"机制不同。

**关键区别**:
- v0.70 freeze 触发: boot → Fluxing 默认 IME 激活 → OnActivated storm → Echo 永久阻塞
- v0.71 corrected freeze 触发: 装机最后一步 → installer 启动 WeaselServer.exe (NSIS line 802 `Exec`) → WeaselServer 启动 hang → 系统无响应 (CPU 100% 或 deadlock) → taskkill 才解

**Echo(100) Fix 1 对此 bug 无效** — 因为是 WeaselServer 自身 hang, 跟 TSF 线程无关。FocusInGuard Fix 2 也无效 — 没到 FocusIn 阶段。

## 2. 触发链路 (NSIS install.nsi 分析)

```
Section -PostInstall (主 install section 末尾)
├─ Line 760: ExecWait "$INSTDIR\WeaselDeployer.exe /install"   ← 等 deployer install
├─ Line 764: ExecWait "$INSTDIR\WeaselDeployer.exe /deploy"   ← 等 deployer deploy
├─ Line 802: Exec "$INSTDIR\WeaselServer.exe"                ← 启动 WeaselServer, **不 wait**
├─ Line 806: MessageBox "CheckForUpdates" (yes/no)
└─ 显示 MUI Finish Page "安装完成, 需要注销重启"

User 点 OK → NSIS 退出 → WeaselServer.exe 仍 hang → 系统无响应
```

**Line 802 `Exec` (不 wait) 是关键** — WeaselServer 启动后被 fire-and-forget, 但 WeaselServer 启动 hang 导致整个系统 stall (因为某种 GDI/USER 资源竞争, 或 librime init 死循环)。

## 3. 已知证据

### 装机端 user 反馈 (2026-07-29)
- 装机 v0.71 corrected installer (`release\fluxing-0.19.0.71-installer.exe` md5 `FA7C7580...`)
- 装机步骤全部正常 (deployer /deploy OK, regsvr32 OK, 文件 copy OK)
- 最后一步"安装完成"对话框出现
- **点确定按钮 → 系统立即无响应**
- 必须 `Ctrl+Alt+Del` → 任务管理器 → kill `WeaselServer.exe` → 系统恢复
- kill WeaselServer 后其他所有操作正常

### v0.70 prior log 证据 (Phase L 7.6 BLOCKED handoff)
```
2026-07-29 15:54:34  5260 engine.cc:72] starting engine.
[hang — never returned]
```
**librime engine init 在 15:54:34 卡死, 之后无任何 engine 日志**。这是 v0.70 boot freeze 的核心症状之一 — 跟 v0.71 corrected 装机的 freeze 极可能同源 (engine.cc:72 是 librime Engine::Start 入口)。

### 4 binary md5 全 MATCH (v0.71 corrected 已 ship)
- weasel.dll: 7f736160
- weaselx64.dll: ea4dece5
- WeaselServer.exe: 6cfa366b
- WeaselDeployer.exe: f81a5c4b
- installer: FA7C7580

**WeaselServer.exe md5 跟 v0.71 first ship (caee5c93) 不同**, 这次 rebuild 包含 FocusInGuard impl 变化。但**没有修 engine init path** (RimeWithWeasel::Initialize / RimeLeversApi 调用链)。

## 4. 工作假设 (未确认)

### 假设 A (强): librime engine init 死循环
- v0.70 15:54:34 log 显示 engine init 卡死
- v0.71 没改 RimeWithWeasel.cpp (engine init 路径未触碰)
- 装机触发: NSIS line 802 `Exec WeaselServer.exe` → WeaselServer 启动 → librime init → engine init → hang
- 修复方向: RimeWithWeasel::Initialize 路径加 instrumentation + timeout, 看哪一步 hang

### 假设 B (中): deployer /deploy 后 WeaselServer 启动 race
- deployer 已写入 user_dict, 但 WeaselServer 启动时锁冲突
- leveldb lock 等待 (`StartMaintenance/EndMaintenance` 没裹)
- 修复方向: 装机后加 5s 等待 leveldb unlock, 再 Exec WeaselServer

### 假设 C (弱): 9X GB librime 字典加载太慢
- rime_ice.dict.yaml 4141771542 hash (13GB 字典 v0.70 之前 ship)
- librime 初始化加载 30 秒正常, 但 30+ 秒就是 hang
- 修复方向: ship 精简版字典 (e.g., 1-2GB) 或 deploy 时预编译

**最大可能: 假设 A** — librime 路径有 30+ 秒的 lib 编译/加载卡死, 跟 v0.70 boot freeze 同源, 只是触发时机移到 installer finalization。

## 5. 下个会话必做

### Step 1: capture 装机 hang 现场 (CRITICAL)
1. user 重新装 v0.71 corrected (md5 FA7C7580)
2. 装机到"安装完成"对话框出现, **不点 OK**
3. PowerShell 跑 `Get-Process WeaselServer | Select Id, CPU, Threads, WS, StartTime`
4. PowerShell 跑 `Get-Process | Where-Object {$_.Modules.FileName -match 'weasel|rime|librime'}` 看 WeaselServer 装载了哪些 DLL
5. 点 OK 触发 freeze
6. 立即 `Get-Process WeaselServer` (看 PID, CPU, 启动时间)
7. 看 `%TEMP%\rime.weasel\*.log` tail 50 行
8. 必要时 `procdump -ma <PID>` dump (PPL 进程, 需 Process Explorer 或 NtfsDebug)

### Step 2: 定位 engine init hang 阶段
- librime 有 `Engine::Initialize` 入口, 看 `rime_init` 之后的字典加载
- 加 instrumentation 埋点 (DLOG) 在 `RimeWithWeasel::Initialize` 关键阶段
- 用 stopwatch 看每个阶段耗时

### Step 3: 决定 fix 策略
**Option 1 (最小, P2-safe)**: 装机 exec 之前等 5 秒 (leveldb lock settle), Exec 改 ExecWait + 60 秒 timeout
- 风险: 60 秒还是 hang 的话 installer 卡
- 收益: 至少能诊断 hang 阶段

**Option 2 (架构, 需 spec)**: WeaselServer 启动分阶段, init librime 在 worker thread, main thread 只做 ready check
- 风险: invasive refactor, 需 spec + ADR
- 收益: 彻底解

**Option 3 (workaround)**: 装机后不自动 Exec WeaselServer, 让 user 手动重启
- 风险: UX 差
- 收益: 0 — 装机完了必须重启, 不需要 WeaselServer 立即跑

### Step 4: 验证 + ship
1. 写 `TestWeaselServerStartup` 单元测试 (mock librime, 测 init 路径)
2. rebuild + sandbox verify (不装机, 只在 sandbox 跑 WeaselServer.exe 测启动)
3. v0.72 ship 含 fix
4. SFTP + 三件套

## 6. 立即给 user 端的 workaround (装后必重启)

```powershell
# 1. 装机完成 → 弹"安装完成"对话框
# 2. 不要点确定! 先打开任务管理器
# 3. Ctrl+Shift+Esc → 找到 WeaselServer.exe
# 4. 记下 PID (后续诊断用)
# 5. 点确定 → 系统卡
# 6. 任务管理器 → 选中 WeaselServer.exe → 结束任务
# 7. 系统恢复
# 8. 注销或重启 (Fluxing 进入正常使用)
```

或**修改装机行为**:
```nsi
; output/install.nsi line 802: 改 Exec → 删除
; 不再装机后立即启动 WeaselServer.exe, 让 user 重启后自动启动 (HKLM Run key)
; Exec "$INSTDIR\WeaselServer.exe"
```
**修法 1 行的 NSIS 改动**, 但需要 user 同意改 install.nsi (.claude/rules/nsis-registry-writes.md 触发硬约束, 需 ADR)。

## 7. Working tree 状态 (继承自 8.1 handoff)

3 commits on HEAD:
- `45264e5` v0.71 first ship (broken, WeaselTSF.cpp 没改)
- `cfcb96d` v0.71 release (bad binaries)
- `5346c07` v0.71 corrected re-ship (real fix)

**未触碰**:
- `output/install.nsi` (last touched bbd0414 v0.67)
- `WeaselSetup/` (installer GUI, last touched v0.66)
- `RimeWithWeasel/RimeWithWeasel.cpp` (librime init path, **这里可能是新 bug 根因**)
- `librime/` (submodule, dirty, L107 教训模式)

**next session 必查**:
- `RimeWithWeasel/RimeWithWeasel.cpp` Initialize / StartMaintenance / deploy 路径
- `librime/src/rime/engine.cc:72` (engine init 入口)
- `output/install.nsi` line 802 (是否改 Exec 为 ExecWait + timeout)

## 8. Session health

- Bash 用量: ~95 calls (已超红线 80)
- 活跃 ~2.8h (远超 self-limit <2h, 因 stop hook 阻塞 + user 反馈连环)
- 0 个 subagent
- 5-axis verification + 3 commits + 2 SFTP + 1 ADR 完成

**session health 警告**: 本会话已严重超时, 任何新工作建议开新会话。本 handoff 正是为此设计。

## 9. 避坑提示

- **Echo(100) / FocusInGuard 不解此 bug** — 不要继续在 TSF 路径找问题
- **installer NSIS line 802 是 fire-and-forget `Exec`, 不是 `ExecWait`** — NSIS 不知道 WeaselServer 状态, 死了也不知道。改 ExecWait + timeout 是最小侵入 fix (但有 UX 风险: installer 卡 60 秒)
- **librime 30 秒 init 正常, 30+ 秒是 hang** — 字典加载 + 编译时间天然长, 不能因为"30 秒慢"就判 hang。要看 hung 之后 CPU / 线程状态
- **PPL 进程 taskkill 受限** — 如果 WeaselServer.exe 是 PPL, `taskkill /F` 会被拒。需要 `taskkill /F /IM WeaselServer.exe /T` 加 `/T` flag (杀子进程)。如果还不行, 用 `sc.exe stop WeaselServer` (如有 service 注册) 或 PowerShell `Stop-Process -Force`
- **不动 install.nsi 除非 user 同意** (.claude/rules/nsis-registry-writes.md 硬约束 + 之前的 L66 spec 066 fix 教训)
- **不动 librime/ 子模块** (L107 教训: submodule 改动需 user 授权 + ADR)

## 10. 下个会话第一句话

```
承接 handoff-after-phase-L-bugfix-8.2-installer-finalize-freeze-2026-07-29.md。
v0.71 corrected 已 ship (3 commits 45264e5 / cfcb96d / 5346c07), 但 user 装机最后一步
"完成"对话框 → OK → 系统无响应。区别于 v0.70 boot freeze, 这次是 installer
finalization 阶段 WeaselServer 启动 hang (line 802 `Exec` fire-and-forget)。
本会话任务:
1. Step 1 (CRITICAL) - capture 装机 hang 现场: 装机时不点 OK, 跑 Get-Process WeaselServer
   看 PID/CPU/threads, 然后点 OK 触发 freeze, 看 rime.weasel log tail
2. Step 2 - 定位 engine init hang 阶段 (librime Engine::Start 在 rime/engine.cc:72 卡死)
3. Step 3 - 决定 fix 策略 (3 个 Option 见 handoff §5):
   - Option 1: install.nsi line 802 改 ExecWait + 60s timeout (最小)
   - Option 2: WeaselServer 启动分阶段 (架构, 需 spec)
   - Option 3: 装机不启 WeaselServer, 让 user 手动重启 (workaround)
4. 写 TestWeaselServerStartup 单元测试 (mock librime)
5. rebuild + sandbox verify (不装机, 只在 sandbox 跑 WeaselServer.exe)
6. ship v0.72 (含 fix) + SFTP + 三件套
```

---

*End of Phase L Bugfix 8.2 handoff. v0.71 corrected ship 之后的新 bug: 装机 finalization hang. 区别于 v0.70 boot freeze, 触发时机是 NSIS line 802 `Exec WeaselServer.exe` fire-and-forget. 下个会话必 capture 现场 (Step 1), 定位 engine init hang, 决定 fix 策略.*
