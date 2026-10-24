# Handoff — v0.19.0.70 装机卡死, Phase L Bugfix 7.6 BLOCKED (2026-07-29)

> **承接**: `handoff-after-phase-L-bugfix-7.5-v0.69-ship-2026-07-29.md` + `_prompt_after_v0.70_install_test_2026-07-29.md`。
> **状态**: ❌ v0.70 装机后系统级卡死, **根因未确认**, 拒绝 code change。
> **新目标**: capture 现场状态 + A/B test v0.69 binary 决定是否本会话 ship 一个 mitigation 修复,或升级到 Phase 3 架构层面讨论。

---

## 1. 一句话总结 (大白话)

v0.70 装机后,user 切到 Fluxing 触发 **FocusIn 重入风暴** (QuickPanel::Show 1 秒内被调 4 次, 引擎 ascii_mode 选项 17 秒被 update 100+ 次), 风暴让 WeaselServer 忙到不能响应 Echo IPC, **TSF 输入线程被 Echo() 阻塞数秒** → TSF 整个 IME pipeline 死锁 → **系统级输入卡死** (鼠标 loading, 软件全无法操作, 只能 taskkill WeaselServer)。taskkill 后 WeaselServer 不在, 切 IME → QuickPanel 不弹。

**不是 v0.70 单点 bug**, 是 Phase L 长期架构问题的暴露: 之前 v0.66-v0.68 QuickPanel 永久 dead 把 FocusIn 风暴掩盖了, v0.69 Fix A (m_disabled 早返移除) 把 QuickPanel 救活, 同时也**暴露了 FocusIn 风暴这个一直存在的 latent bug**。

---

## 2. 已确认证据 (Phase 1 done)

### 装机端 — 全部预期 PASS
| 项 | 结果 | 证据 |
|---|---|---|
| 装机 binary md5 | ✅ 4 binary 全 MATCH v0.70 ship md5 | diag-v070.ps1 §8 |
| WeaselTSF.dll | 82e9e924b25634ca7ffe869856dc8bb7 MATCH | |
| weaselx64.dll | 80615749557d4e04c6e7451ad00e37c5 MATCH | |
| WeaselServer.exe | BAD4E61A3624B4DFD516BA561B23BED6 MATCH | |
| WeaselDeployer.exe | ED0EAEAA5F099F68374F53A535FE9BAD MATCH | |
| §5.1 reg query 4 项 | ✅ 全部存在 | diag-v070.ps1 §5-6 |
| HKLM\CTF\KnownClasses | {A3F4CDED-...} REG_SZ "Fluxing Text Service" | |
| HKCU\CTF\Assemblies\0x00000804 | 2 GUID, Profile=A3F4CDED-... | |
| HKLM\CLSID\InprocServer32 | D:\Program Files\fluxing\weasel\weaselx64.dll | |
| HKCU\Software\Fluxing\Weasel | RimeUserDir=D:\Program Files\fluxing\user1\fluxing | |
| HKCU\Preload #1=00000804 (Fluxing) #2=00000409 (en-US) | ⚠️ Fluxing 是默认 IME, **这就是风暴的触发点** | diag-v070.ps1 §3 |

### 关键异常 — 风暴证据
| 项 | 观察 | 推断 |
|---|---|---|
| 11:32:38-11:32:55 (17 秒) | `engine.cc:133] updated option: ascii_mode` × 100+ 次 (thread 16316) | **ascii_mode 风暴**, session 活跃期已经在跑, 当时没卡死 (CPU 100% 但 server 不在 IPC 路径) |
| 15:54:03-15:54:34 (31 秒) | librime 启动 + dict 加载 + engine init start, 之后**静默** | **librime engine init hang** at 15:54:34 (thread 5260, 永远不结束 Engine::Start) |
| 07:54:28 (1 秒内 3 次) | `[QP-DIAG] QuickPanel::Show entered s_hwnd=0x000300C4` × 3 | **QuickPanel::Show 重入风暴** (FocusIn 在 1 秒内被调 ≥3 次) |
| 07:54:27 first Show | s_hwnd=0x00000000 (首次创建, OK) | 正常路径 |
| 07:54:34 +6 秒 | 再 Show 一次 | 间歇性 FocusIn 触发 |
| 当前 tasklist | 0 个 WeaselServer, 0 个 weasel 命名管道 | user 已 taskkill 成功 |

### 关键代码路径
- **TSF OnActivated** (`WeaselTSF.cpp:286-308`): v0.70 = 2× `m_client.Echo()` + 1× `m_client.FocusIn()` on TSF 线程 (P2 边界, 仍属同步 I/O)
- **Server FocusIn handler** (`WeaselServerImpl.cpp:294-299`): `OnFocusIn` → `m_pRequestHandler->FocusIn(wParam, lParam)` → `RimeWithWeasel::FocusIn`
- **RimeWithWeasel::FocusIn** (`RimeWithWeasel.cpp:384-450`): Fix A 已移到 m_disabled check 之前, Show 一定弹; **设置 ascii_mode 走 `set_option` → rime engine 状态变更**
- **QuickPanel::Show** (`QuickPanelDialog.cpp:1184-1264`): spec 070 path, `WS_EX_TOPMOST | WS_EX_LAYERED`, `SetWindowPos(HWND_TOPMOST, ...)` z-order 强制
- **ascii_mode storm origin** (`RimeWithWeasel.cpp:1000-1005`): session 状态变化时, **如果 m_global_ascii_mode 启用且某 session ascii_mode ≠ 全局, 对每个 session 调 set_option**

---

## 3. 工作假设 (Phase 2 待验证)

### 假设 A (强支持): FocusIn 重入风暴 + TSF 线程阻塞 = 系统级卡死
**链条**:
1. TSF 第一次 OnActivated → FocusIn → 弹 QuickPanel
2. QuickPanel `SetWindowPos(HWND_TOPMOST)` 抢 z-order → 触发当前 focus window 失去焦点
3. TSF 检测到 profile 变化 (focus 移到非 composition target) → 重新 OnActivated → 再次 FocusIn
4. **反馈循环**: Show → 抢 z-order → focus 移 → OnActivated → FocusIn → Show...
5. 风暴中 WeaselServer 持续处理 `set_option("ascii_mode", ...)` 写入 (storm)
6. 风暴中 TSF 线程调 Echo() → server pipe 队列被 ascii_mode 处理占满 → Echo 阻塞
7. TSF 输入线程 (在 OnActivated 内) 阻塞 → 系统级 input pipeline 卡死

**支持证据**: ascii_mode 100+ 次/17s 风暴 + QuickPanel Show 3 次/1s 重入 + TSF 线程调 Echo
**未解释**: 风暴的最初触发器 — 第一次 FocusIn 怎么会变成重入? TSF 不会无故连续 OnActivated。

### 假设 B (弱支持): librime engine init hang 导致 Echo 阻塞
**链条**:
1. 15:54:34 thread 5260 librime Engine::Start 卡死
2. Server 所有后续 IPC (FocusIn / Echo) 都在同一 worker thread 上
3. 后续 Echo 全部阻塞 → TSF 线程阻塞 → 系统级卡死

**支持证据**: 15:54 log 末段静默
**矛盾**: 假设 B 不能解释 ascii_mode 风暴 (storm 来自 set_option, 不是 Engine::Start)
**未解释**: 什么导致 Engine::Start hang (disk I/O? leveldb lock?)

### 假设 A + B 复合
- 风暴 (A) 让 server 持续忙于 set_option
- leveldb user_dict lock 与 storm 的 write contention 让 Engine::Start hang (B)
- TSF 线程 Echo 阻塞 → 系统级卡死
- 解释了"为什么之前 v0.66-v0.68 没事" — QuickPanel 永久 dead 时 FocusIn 早返, 没有 set_option 风暴, server 不会 hang

---

## 4. 关键事实 (避免再走弯路)

### 4.1 v0.69 vs v0.70 真实代码差异
```
v0.69 (476e77e):
  - Fix A: RimeWithWeasel.cpp FocusIn 解除 m_disabled 早返
  - Fix B: WeaselTSF.cpp OnActivated Echo false → StartSession fallback (P2 违规)
  - Fix C: WeaselIPCServer.cpp RegisterHotKey 失败写 log
  - 4 节点 instrumentation

v0.70 (c14d6ee = 65953a9):
  - Fix A: 保留
  - Fix B: REVERTED (回滚到 v0.68 行为, 仅 Echo check 不 fallback)
  - Fix C: 保留
  - 4 节点 instrumentation: 保留
```

**v0.70 = v0.69 - Fix B**。如果 v0.70 freeze, v0.69 (有 Fix B) 理论上更可能 freeze (Fix B 在 TSF 线程多调 StartSession, 增加阻塞窗口)。

### 4.2 user 当前状态
- WeaselServer 已死 (taskkill 成功)
- 默认 IME 还是 Fluxing (Preload #1=00000804)
- 切 Fluxing → QuickPanel 不弹 (因为 m_pRequestHandler=null, 后续 FocusIn 早返)
- 切英文 → 正常

### 4.3 已 ship binary 可用性
- `release/fluxing-0.19.0.69-installer.exe` — 本地有, **可装**
- `release/fluxing-0.19.0.70-installer.exe` — 本地有, **可装**
- `release/fluxing-0.19.0.68-installer.exe` — 本地有, QuickPanel 永久 dead 但系统级不卡
- 远程 `E:\办公文件\L1网站\pinyin\` 只有 v0.19.0.44 (v0.66-v0.70 都未上传)

---

## 5. 立即可做 (不需 code change)

### 5.1 User 立即 work-around: 卸掉 Fluxing 默认
让 WeaselServer 不在 boot 时被自动唤醒, 避免 TSF OnActivated 风暴触发:

```powershell
# PowerShell admin
reg add "HKCU\Keyboard Layout\Preload" /v 1 /t REG_SZ /d 00000409 /f
reg add "HKCU\Keyboard Layout\Preload" /v 2 /t REG_SZ /d 00000804 /f
# 现在默认是 en-US, Fluxing 排第二
# 用 Win+Space 手动切到 Fluxing 仍可工作, 但 boot 不自动激活
```

或者从「设置 → 时间和语言 → 语言 → 中文(简体) → 键盘」移除 Fluxing, 只保留微软拼音, 然后**重新添加** Fluxing (顺序会变, Fluxing 不会在第一位)。

### 5.2 A/B test 决定是否 v0.70 独有
1. user 装 `release\fluxing-0.19.0.69-installer.exe` (覆盖 v0.70)
2. 跑同样操作 (切 Fluxing + 切回 + 切 Fluxing)
3. 观察:
   - **A/B 都 freeze** → 根因不是 Fix B revert, 是 Fix A 暴露的 latent bug
   - **只有 v0.70 freeze** → 根因是 Fix B 缺失 → 重新考虑加回 Fix B 但用 P2-safe 实现 (e.g., PostMessage 异步)
   - **两个都不 freeze** → user 重启刚好没触发风暴, 需更多 replicate

### 5.3 Capture 工具 (装机后用)
已写 `diag-v070.ps1`, 现升级为 `diag-freeze-capture.ps1`, 在 freeze 现场跑:
- tasklist weasel/fluxing 进程状态
- 进程 thread 数 + CPU (定位是 100% CPU 忙循环还是 0% CPU deadlock)
- 进程 stack (需 procdump / Process Explorer)
- 命名管道 weasel* 状态
- weasel-install.log + rime.weasel log 末段

新脚本会写在 `diag-freeze-capture.ps1`, 给下个会话跑。

---

## 6. 禁止事项 (本会话已遵守, 下会话也守)

- ❌ 不在 root cause 未验证前改 code (Iron Law)
- ❌ 不 commit / 不 push / 不 ship (binary 也保留本地)
- ❌ 不装任何 installer 给 user 跑 (user 决定)
- ❌ 不把 handoff-* / _prompt_* / diag-* 脚本 commit (移交文件, 跟 L10 同模式)
- ❌ 不重装 v0.69 (user 决定)
- ❌ 不删 §5.1 reg 4 keys (装机端已 PASS)

---

## 7. Session health

- Bash 用量: ~32 calls (红线 80)
- 活跃 ~1.0h (self-limit <2h)
- 1 个 parallel subagent (Explore 检查 QuickPanel 4 节点状态, 已完成)
- 0 个 code 改动, 0 个 commit

---

## 8. 下个会话第一句话

```
承接 handoff-after-phase-L-bugfix-7.6-v0.70-blocked-2026-07-29.md。
v0.70 装机后系统级卡死,根因未确认,工作假设 = FocusIn 重入风暴 + TSF 线程阻塞 I/O。
本会话任务: 跑 user A/B test (v0.69 vs v0.70) 决定 Fix B 是否根因 + 写 FocusIn 重入
guard 防御性修复 (e.g., 1-line reentry guard in WeaselServerImpl::OnFocusIn) +
rebuild + sandbox verify + 装机回 user 端再验证。
如果 A/B 都不 freeze, 升级到 Phase 3 架构层面: 重新审视 FocusIn → QuickPanel::Show
→ HWND_TOPMOST z-order 抢焦 是否应该走 PostMessage 异步, 避免 TSF 线程同步调 Show。
```

---

## 9. 避坑提示

- **不要**直接给 QuickPanelDialog::Show 加异步 (spec 070 之后 Show 是 per-pixel alpha GDI, 改同步→异步会改主路径, 必须先 spec-init)
- **不要**回滚 v0.69 Fix A (m_disabled 早返移除) — 那只是把风暴掩盖, 风暴还在, server 还是在 busy set_option
- **不要**同时改 FocusIn + OnActivated + Show — 一个 fix = 一个 PR, 配套 test
- 任何"在 OnActivated 加 timeout"的修改都属于 P2 边界 (CLAUDE.md §2: TSF 回调禁止 blocking I/O), 需 spec + ADR
- 风暴在 user 端偶尔触发 (可能 1/10 boot), sandbox 复现率低, 真机验证是 ship-gate
- 之前 v0.66-v0.68 QuickPanel 永久 dead 把风暴掩盖, 现在 v0.69+ 救活 QuickPanel, latent bug 暴露 — 这次不是新 bug, 是回归

---

## 10. 必读顺序 (new session)

1. `MEMORY.md` + `claude_desktop_session_health.md`
2. 本文件 `handoff-after-phase-L-bugfix-7.6-v0.70-blocked-2026-07-29.md`
3. `handoff-after-phase-L-bugfix-7.5-v0.69-ship-2026-07-29.md` (v0.69 修复了什么)
4. `handoff-bug-floating-bar.md` (4 节点 instrumentation 由来)
5. `CONTEXT.md` §5 历次 L## 索引 (L66 / L67-L69 / L70 / L74 / L81 / L97 / Phase L 系列)
6. `lessons-learned.md` L##-PhaseL-7+8
7. 图谱 `search_graph` 看 FocusIn → OnActivated → Show cross-call

---

*End of Phase L Bugfix 7.6 handoff. v0.70 BLOCKED on Phase 1 → Phase 2 transition. 拒绝未验证的 code 改动, A/B test + 重入 guard 是下个会话必交付.*
