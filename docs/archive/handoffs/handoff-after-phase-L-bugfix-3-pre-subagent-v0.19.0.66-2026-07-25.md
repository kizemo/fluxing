# Handoff — Phase L 装机 bugfix 3 v0.19.0.66 subagent-phase (2026-07-25)

> **承接**: `handoff-after-phase-L-bugfix-2-failed-v0.19.0.65-2026-07-25.md`
> **承接**: `task.md` Phase L Bugfix 3 段 (本会话已 Edit 头部追加)
> **状态**: 🟡 **Phase 1 systematic-debugging 进行中** (3 subagent background, 主会话已形成 Hypothesis C5 strong + Alt+, 真路径,待 subagent 验证)
> **方向**: 等 subagent 报告 → 综合分析 → **Phase 4.5 架构讨论 (已触发)** → 用户决策 → 改源码 → ship v0.19.0.66

---

## 1. 已 ship 状态 (v0.19.0.65)

| 项 | 值 |
|---|---|
| branch | `Fluxing` (ahead 943 / behind 831) |
| last commit | `84f01c5` (v0.19.0.65) |
| last tag | `v0.19.0.65` |
| 装机 binary md5 | `BFC81FA7481AF3C09B581D17F8D003D5` ✓ |
| 装机 installer md5 | `1326F50CBCF4BE49EFDFB23CFC5D9667` (NSIS 01:01 build) |
| FluxingPhrasesDialog.exe md5 | `52EA1BA5F2FC08EC0FD3973BD9CCE0A4` (v0.19.0.65 新链) |
| 装机端 weasel-install.log 关键证据 | `ImmGetDefaultIMEWnd(NULL) = 0x00000000` (IMM32 path DEAD) |

**装机端 4 个 user flow 全失败** (3+ ship 累积 v0.19.0.63/.64/.65):
1. Win+Space 切 Fluxing → 不弹
2. Win+Space 切英文 → 不弹 (回归 OK)
3. 切焦点到 text input → 不弹
4. **Alt+, → 不弹 (回归!** 我没改这条路径,却 work 不动)

---

## 2. session 中间形成的核心 insight (本会话新沉淀)

### 2.1 Alt+, 真路径 (handoff §6 prompt bug 修复)

**之前 handoff §6 subagent 1 让 grep WeaselServerApp.cpp 的 RegisterPhrasesHotkey 找 Alt+, — 错位置**。

**Alt+, 真位置** (`WeaselIPCServer/WeaselServerImpl.cpp`):

```
[user 按 Alt+,]
  ↓
Windows OS WM_HOTKEY → ServerImpl m_hWnd (IPC server window)
  ↓
ServerImpl::OnHotkey (WeaselIPCServer/WeaselServerImpl.cpp:152-166)
  ├─ if (wParam == ID_HOTKEY_QUICK_PANEL)
  ├─ if (m_pRequestHandler)   ← AP-036-G alive check
  ├─ ::PostMessage(m_hWnd, WM_COMMAND, ID_WEASELTRAY_QUICK_PANEL, 0)
  ↓
ServerImpl::OnCommand (l.117) → m_MenuHandlers lookup
  ↓
[AddMenuHandler 注册的 lambda] (WeaselServer/WeaselServerApp.cpp:458-)
  ├─ m_handler->IsFullShape()
  ├─ ToggleMode(currentFull)
  ↓
QuickPanelDialog::ToggleMode → 内部 Show
```

**Alt+. (PHRASES_DOT, id=9002)** 才在 `WeaselServerApp.cpp::RegisterPhrasesHotkey`。**两条 hotkey 完全独立路径**。

### 2.2 Hypothesis C5 strong (主会话独立 forming, 等 subagent 3 验证)

**两条路径共用 QuickPanelDialog 单例**:
- FocusIn (v0.19.0.65 改) → `Show(false, nullptr x 6)` 直接调 Show,**6 个 callback 全 null**
- Alt+, (我没改) → `ToggleMode(currentFull)` 内部也 Show
- 如果 `Show(false, nullptr x 6)` 在 null callback 时污染单例 state → 后续 `ToggleMode` 状态判断错 → **不弹 → 完美匹配 Alt+, 回归**

### 2.3 架构 root cause narrative (Phase 4.5 必走)

1. WeaselServer 是 **hidden IPC server** (no IME context, no focus, no GetMessage idle) → IMM32 path 装机证实 dead
2. **TSF shim 架构陷阱** (memory `feedback_tsf_shim_blocks_imm32`): WeaselServer.exe 内 IMM32 API 本质失效,8 ship 失败已沉淀
3. **真修路径** = WeaselTSF::OnActivated (cpp:221-257) → IPC FocusIn → RimeWithWeasel::FocusIn (cpp:444-449) → QuickPanelDialog::Show
4. 但 `Show(false, nullptr x 6)` 在 null callback 时副作用 = **污染单例 state** (待 subagent 3 验证)

### 2.4 L## pair-symmetry trap 已沉淀 (Phase L Bugfix 1 L## 段)

新 lesson (commit `505c398` / `84f01c5`):
- **AP-L##-B**: re-enable 对称 X/Y 对在不同 commit (FocusOut / FocusIn 分两次!) → 8 ship 失败根因模式
- **AP-L##-C**: 跳过 `if (false)` 因为 sibling 已 re-enable,死代码 orphaned
- **Trigger L3 gate**: 任何 commit 涉及 FocusIn/FocusOut/OnActivated/QuickPanel 必须 `grep -RIn "if (false)"` diff confirmation + `/t:Rebuild` smoke test

**项目级 `if (false)` audit** (主会话完成): 只剩 librime deps (third party),**项目内代码无残留**。

### 2.5 QuickPanel dialog 4 触发路径 (设计意图综合)

| 路径 | 入口 | 代码位置 | 当前 v0.19.0.65 状态 |
|---|---|---|---|
| Alt+, | WM_HOTKEY → PostMessage(WM_COMMAND, ID_WEASELTRAY_QUICK_PANEL) → AddMenuHandler | `WeaselIPCServer/WeaselServerImpl.cpp:152` | 未改 |
| tray 左键 click | WM_LBUTTONUP → PostMessage WM_COMMAND ID_WEASELTRAY_QUICK_PANEL | `WeaselServer/SystemTraySDK.cpp:842` | 未改 |
| tray 菜单 "QuickPanel" 项 | WM_COMMAND ID_WEASELTRAY_QUICK_PANEL → AddMenuHandler | `WeaselServer/WeaselServerApp.cpp:458` | 未改 |
| 切语言栏 → QuickPanel button | ITfLangBarItem::OnClick → IPC → m_client.TrayCommand → WM_COMMAND | `WeaselTSF/LanguageBar.cpp:163-167` | 未改 |
| Win+Space 切 Fluxing → 弹 | TSF OnActivated → IPC FocusIn → RimeWithWeasel::FocusIn → Show(false, nullptr x 6) | `RimeWithWeasel.cpp:444-449` | **v0.19.0.65 改** |

**前 4 路径: 完全未动。Show(false, nullptr x 6) 是 v0.19.0.65 唯一新增入口**。

---

## 3. 3 subagent background dispatch 状态

| Agent | Agent ID | dispatch at | .jsonl size | last mod | 任务 |
|---|---|---|---|---|---|
| alt-comma-regression | `af60513d9a1435fde` | session start (08:48) | 308575 bytes | 08:54:45 | Alt+, 回归根因 (虽然 prompt 漏了真位置,主会话已补 grep) |
| focusin-reality | `a4eed3ff43ddd524e` | session start (08:48) | 236733 bytes | 08:53:39 | FocusIn 是否真触发 (DLOG 输出 / m_client.Echo) |
| quickpanel-trigger-matrix | `a9a6e38a0f3885abf` | session start (08:48) | 132165 bytes | 08:52:09 | QuickPanelDialog 触发路径 compatibility matrix |

**注意**:subagent 1 的 prompt 漏 Alt+, 真位置 (handoff §6 错让看 WeaselServerApp.cpp)。**主会话已独立 grep WeaselIPCServer/WeaselServerImpl.cpp 补回**。

**JSONL 路径** (后续读用):
- `C:\Users\Duanyi\AppData\Local\Temp\claude\F--soft-00selfmade-rime-claude\634bc624-36ec-4afe-9f8f-b3569bae7690\tasks\a4eed3ff43ddd524e.output` (symlink)
- `/c/Users/Duanyi/.claude/projects/F--soft-00selfmade-rime-claude/634bc624-36ec-4afe-9f8f-b3569bae7690/subagents/agent-a4eed3ff43ddd524e.jsonl`

3 个 agent 都在 final response writing 阶段 (estimated, based on file size stability)。

---

## 4. Phase 4.5 已触发 — 必走架构讨论

**条件**: 3+ ship 失败 (v0.19.0.63 / .64 / .65)。systematic-debugging Phase 4.5 强制要求用户决策:
> "The architecture is wrong. Discuss with your human partner before attempting more fixes."

**触发问题清单** (新会话第一句要问):
1. TSF shim 进程内 IMM32 API 失效的根治 = out-of-process? (跟 Phase K 架构重设计一致,但当前 Bug 2 = QuickPanelShow 不是 IME composition)
2. `Show(false, nullptr x 6)` 是否真破坏 QuickPanelDialog 单例 state? (subagent 3 验证)
3. 是否需要改为 `Show(true, 6 个 valid no-op lambda)` 兼容 null callback? 还是改 Show 内部? (scope 扩张雷区)
4. Alt+, / tray click / menu / language bar button 4 路径都不弹,是否都因 QuickPanelDialog 单例 state 损坏? (subagent 3 验证)
5. **新会话第一次决策**:是改 v0.19.0.65 的 Show 调用 (回归到 EnableAlwaysShowMode 或 6 valid lambdas),还是把 Bug 2 彻底重做?

---

## 5. 必交付清单 (新会话第一件事)

- [ ] 读本 handoff (§1-§4)
- [ ] 读 `task.md` 头部 Phase L Bugfix 3 段
- [ ] 接收 3 subagent 报告 (background notification) → 综合分析
- [ ] 验证 Hypothesis C5 (QuickPanelDialog 单例 state 是否被 Show(null x 6) 污染)
- [ ] **Phase 4.5 架构讨论 — 与 user 决策** (4 个选项,见 §4 触发问题清单)
- [ ] 形成新 hypothesis + 装机诊断埋点方案 (设计埋点必带 `if (false)` audit)
- [ ] **仅在 user 同意后才改源码** (L07 spec-init 流程)
- [ ] sandbox-verify 装机端验证 (sandbox-verify rime-claude ps1)
- [ ] **grep -RIn "if (false)" WeaselServer RimeWithWeasel WeaselTSF** (Trigger L3 gate)
- [ ] **`/t:Rebuild` smoke test** (L108 教训,不能省)
- [ ] ship v0.19.0.66 (build_v066.ps1 + nsis + installer)
- [ ] 装机端 4 user flow 验证
- [ ] 沉淀 L##-PhaseL-6 (QuickPanelDialog 单例 state design trap) + L##-PhaseL-7 (Alt+, 回归根因)

---

## 6. 当前禁止 (per systematic-debugging 红线)

- ❌ 改 install.nsi (L09/L17/L54/L66 雷区)
- ❌ 改 QuickPanelDialog.cpp 内部 (除非 4 路径都不 work, 且走 spec-init 流程)
- ❌ 改 Bug 1 部件拆字 (user 搁置)
- ❌ 删现有埋点 (用于装机 verify)
- ❌ 单步试错改代码 — 必须 user 同意后改
- ❌ **`if (false)` 残留**:任何 commit 必须走 Trigger L3 gate (§2.4)
- ❌ **不验证装机端 binary md5 就 ship** (L97 stale binary)

---

## 7. 严格必读顺序 (新会话)

1. 本 handoff (本文件) — 必读
2. `task.md` 头部 Phase L Bugfix 3 段
3. `handoff-after-phase-L-bugfix-2-failed-v0.19.0.65-2026-07-25.md` (3+ ship 失败全景)
4. memory `feedback_tsf_shim_blocks_imm32` (架构陷阱沉淀)
5. memory `feedback_autonomous_diagnostics` (本机只读诊断授权)
6. 3 subagent 报告 (notification 来就 cat 末尾)
7. 关键源码: `WeaselIPCServer/WeaselServerImpl.cpp:152-166` (Alt+, 入口), `RimeWithWeasel.cpp:444-449` (Show 调用), `WeaselUI/QuickPanelDialog.cpp` Show / ToggleMode (subagent 报告里查)
8. `_check_install_v2.ps1` 期望 md5 (如果 ship v0.19.0.66,必更新)

---

## 8. sandbox-verify 工具

```
F:\soft\00selfmade\sandbox-verify\rime-claude\rime-verify.ps1 -InstallerPath <installer>
```

(详 per `CLAUDE.md` global instruction。)

---

## 9. 时间预算

- 本会话估算时长: ~30 min active (未到 2h 红线)
- 但综合分析 + 决策 + 改源码 + 装机 verify + ship = **多 cycles**, 必然跨 2h
- **新会话接力更安全** (按 session-health.md 强制)

---

## 10. 加速模式规则 (新会话继承)

- 单回合 diff >500 行 → 拆回合
- bash >=60 调用 → 切 Write/Edit
- 并行子agent ≤2 同时 (本会话 3 是 user 显式授权特例,新会话默认 ≤2)

---

*End of Phase L bugfix 3 session-end handoff. New session starts with:*
*承接 `handoff-after-phase-L-bugfix-3-pre-subagent-v0.19.0.66-2026-07-25.md`, Phase L bugfix 3 v0.19.0.66 subagent-phase 主进程接力。*
