# Handoff — Phase L 装机 bugfix 2 v0.19.0.65 ship 后 user 验收失败 (2026-07-25)

> **承接**: `handoff-after-phase-L-2-3-bugfix-2-v0.19.0.65-2026-07-25.md` (v0.19.0.65 ship)
> **承接**: 本会话 systematic-debugging Phase 1-4 + ship 后 user 装机验收
> **状态**: ❌ **user 装机验收 3 个 user flow 全失败 + 1 个回归** — v0.19.0.65 fix 失效,且 Alt+, 回归
> **方向**: 必须用 subagents 并行深度分析 (严禁盲目试错)
> **待 user**: 重启新会话,按 `_prompt_phase_L_bugfix_3_next_session.md` 第一句话接力

---

## 1. user 装机验收结果 (v0.19.0.65 ship 后 2026-07-25 07:49+)

| User flow | 期望 | 实际 | 状态 |
|---|---|---|---|
| 1. Win+Space 切到 Fluxing | 浮动设置栏自动弹 | 不弹 | ❌ fix 失效 |
| 2. Win+Space 切到英文 | 不弹 | 不弹 | ✓ 回归 OK |
| 3. 切焦点到带 Fluxing 的 text input | 浮动栏弹 (TSF 副作用) | 不弹 | ❌ FocusIn 路径也失效 |
| 4. 按 Alt+, 收起浮动栏 → 再按显示 | toggle | **不弹** (全黑屏?) | ❌ **回归** — Alt+, 之前 work,现在死 |
| 5. Ctrl+Shift+K → ShortcutSettings | 弹 | (未测) | ? |
| 6. Bug 1 部件拆字按 `,` 出 "，" | 出中文标点 | (user 搁置) | ? |

**关键观察**:
- ❌ Bug 2 没修好
- ❌ Bug 2 真修路径 (FocusIn) 也失效
- ❌ **Alt+, 回归** — 这条路径**我没动**,却 work 不动了
- 这是 3 次 ship (v0.19.0.63 / .64 / .65) 都失败的累积状态

---

## 2. 装机端 fresh state (2026-07-25 ~07:49 后)

```
WeaselServer.exe         PID 12984   running, Console session, 75,580 K
binary mtime:            Jul 25 00:57  (v0.19.0.65 build time)
binary md5:              BFC81FA7481AF3C09B581D17F8D003D5  ✓ v0.19.0.65 build
FluxingPhrasesDialog.exe md5: C969AEB31E24D9EE1468F8869150970D  (v0.19.0.59+, unchanged)
radical_pinyin.schema.yaml md5: BEB5B74D0DD7B4D53D8386898DFA93D7  (含 - punctuator)
HKLM\...Uninstall\Fluxing\DisplayVersion: 0.19.0.65  ✓
```

weasel-install.log (580 bytes, 2 sessions):

```
[2026-07-24 23:49:08] [Run] s_fluxingHkl=08040804 (initial)
[2026-07-24 23:49:10] [RegisterPhrasesHotkey] ImmGetDefaultIMEWnd(NULL) = 0x00000000
[2026-07-25 00:29:21] [Run] s_fluxingHkl=08040804 (initial)
[2026-07-25 00:29:21] [RegisterPhrasesHotkey] ImmGetDefaultIMEWnd(NULL) = 0x00000000
```

**关键埋点发现**:
- ✅ `s_fluxingHkl = 0x08040804` 启动时记录 (Fluxing HKL)
- ✅ `ImmGetDefaultIMEWnd(NULL) = 0x00000000` — **IMM32 path DEAD** (hwnd 为 NULL,确认 hidden IPC server 无 IME context)
- ❌ **无 `[ImeLangSubclass] WM_INPUTLANGCHANGE`** 任何 entry — 因为 hwnd=NULL,没装 subclass,所以没 WM_INPUTLANGCHANGE handler,自然没 log (符合 hypothesis)
- ❌ **无 `DLOG FocusIn auto-show QuickPanel`** — DLOG 走 glog 不走 file,无法直接看到。**必须验证 FocusIn 是否被触发**

**Registry 验证** (L66):
- ✅ `HKLM\SOFTWARE\Microsoft\CTF\KnownClasses\{A3F4CDED-...}` = "Fluxing Text Service"
- ✅ `HKLM\SOFTWARE\Classes\CLSID\{A3F4CDED-...}\InprocServer32` = "D:\...\weaselx64.dll"
- ✅ `HKCU\Software\Microsoft\CTF\Assemblies\0x00000804` 有 entries

---

## 3. 3 次 ship 累计 root cause 假说 (都需并行验证)

### Hypothesis A: IMM32 路径天生不可靠 (v0.19.0.64 假设)
**Status**: **确认** — `ImmGetDefaultIMEWnd(NULL) = 0x00000000` 装机证据
**Why**: WeaselServer 是 hidden IPC server,无 IME context,ImmGetDefaultIMEWnd 永远返回 NULL
**结论**: v0.19.0.64 的 `ImeLangChangeSubclassProc` 路径永远不可能触发

### Hypothesis B: L69-fix 残留 `if (false)` 死 FocusIn (v0.19.0.65 假设)
**Status**: **待验证** — 我改了,但 user 反馈 FocusIn 路径也不弹 (切焦点到 text input)
**Why 改**: 改 `if (false && ipc_id > 0)` → `if (ipc_id > 0)`, `EnableAlwaysShowMode` → `Show(false, nullptr x 6)`
**问题**: DLOG 没出来,无法确认 FocusIn 是否被触发。`Show(false, nullptr x 6)` 调 QuickPanelDialog::Show,但 Show 内部需要 callback 全 null — **可能 QuickPanelDialog::Show 在 null callback 时拒绝/崩溃**

### Hypothesis C: Alt+, 回归 — 我没动这条路径却 work 不动了 (新出现)
**Status**: **完全未理解** — 这条路径**没被我改过**,但 v0.19.0.65 装机后失效
**Possible causes**:
- **C1**: 别的 app 抢占了 Alt+, RegisterHotKey — 但 v0.19.0.64 / 之前都没抢过
- **C2**: WeaselServer 进程虽然跑着,但 m_server.GetHWnd() 拿不到有效 hwndServer (IPC server window 异常)
- **C3**: PhrasesHotkeySubclassProc 透传到 m_ipcServerOrigWndProc 时,被某个我新加的埋点打断 (但埋点只写在 file,不应影响 message dispatch)
- **C4**: QuickPanelDialog 单例 state 被前次 Show(false, nullptr x 6) 损坏 (FocusIn 真触发后状态破坏 → Alt+, Show() 因状态失败)
- **C5**: QuickPanelDialog::Show(false, nullptr x 6) 第一次 focus 焦点还在 WeaselServer,跟 Alt+, 抢 foreground 失败

### Hypothesis D: QuickPanelDialog::Show 全 null callback 不 work (新出现)
**Status**: **未验证**
**Why**: v0.19.0.65 FocusIn 调 Show(false, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr) — 6 个 callback 全 null
**风险**: Show 内部可能在 null callback 时拒绝 Show (例如 L75-spec 070 改纯 GDI 后, callback 是 paint required)
**测试需求**: 用一个全 valid no-op callback 测试,或读 Show 实际源码确认

### Hypothesis E: WeaselServer.exe 注册路径跟 Win+Space IPC FocusIn 路径不同步
**Status**: **未验证**
**Why**: FocusIn IPC 是 per-app thread (WeaselTSF.dll 在每个 foreground app 进程加载),Win+Space 切到 Fluxing 后,foreground app 的 WeaselTSF 应该触发 OnActivated → FocusIn → WeaselServer
**问题**: 是否所有 app 都正确加载 WeaselTSF? 是否有 app 没装 / hook 没初始化?
**测试需求**: 看 foreground app 的 WeaselTSF.dll 是否 loaded,看 OnActivated 是否被注册

---

## 4. 已 ship 的 source 改动 (3 次 ship 累计)

### v0.19.0.63 (commit `055cc6c`) — Phase L 调整 2+3
- `radical_pinyin.schema.yaml` 加 `punctuator:` 段
- `WeaselServerApp.cpp` 加 `WM_INPUTLANGCHANGE` 钩子 (subclass IPC server window)
- **没改 Bug 2**: IPC server window hidden + 无 focus,收不到 WM_INPUTLANGCHANGE

### v0.19.0.64 (commit `55e7991`) — Phase L 装机 bugfix
- `radical_pinyin.schema.yaml` `engine.processors` 加 `- punctuator` (Bug 1 修)
- `WeaselServerApp.cpp` 改 hook `ImmGetDefaultIMEWnd(NULL)` 拿 WM_INPUTLANGCHANGE
- **仍失败**: WeaselServer 无 IME context,`ImmGetDefaultIMEWnd` 返回 NULL,subclass 不到任何窗口

### v0.19.0.65 (commit `84f01c5`) — 本会话 Phase L 装机 bugfix 2
- `RimeWithWeasel/RimeWithWeasel.cpp` `FocusIn` 改 `if (false)` → `if (true)`, `EnableAlwaysShowMode` → `Show(false, nullptr x 6)`, 加 DLOG
- `WeaselServer/WeaselServerApp.cpp` 加 3 处埋点 (Run / RegisterPhrasesHotkey / ImeLangChangeSubclassProc)
- `CHANGELOG.md` + `_check_install_v2.ps1` + `lessons-learned.md` 同步更新
- **失败原因 (Hypothesis 候选)**: Show(null x 6) / QuickPanel 单例 state / Alt+, 回归

---

## 5. 已知 dead ends (下次会话不要再走)

| Dead end | Why dead | 证据 |
|---|---|---|
| IPC server window subclass for WM_INPUTLANGCHANGE | hidden + no focus | v0.19.0.63 ship 时确认 |
| ImmGetDefaultIMEWnd(NULL) subclass | hwnd=NULL (hidden IPC server 无 IME context) | v0.19.0.65 装机 `ImmGetDefaultIMEWnd(NULL) = 0x00000000` 直接证据 |
| `if (false)` block 的死代码假设 (即假设改回去就 work) | FocusIn 路径即使改回去,可能根本不被触发 (m_client.Echo() 返回 false / pipe 异常) | user 装机后切焦点到 text input 也不弹 |
| 改 QuickPanelDialog.cpp 自身 Show 内部 | scope 之外,会触发 L69 GDI+ 类似雷区 | spec 070 T005 才改完纯 GDI,任何改动需走 L07 风险评估 |

---

## 6. 新会话必走的 subagent 分析矩阵

```
Subagent 1 (Explore, "alt-comma-regression"):
  - 读 PhrasesHotkeySubclassProc + RegisterHotKey 路径源码
  - 验证 v0.19.0.65 Alt+, RegisterHotKey 是否成功 (查 stderr / log / 别的 app 占用)
  - 如果 hotkey 注册成功,验证 WM_HOTKEY dispatch 路径
  - 如果 dispatch 成功,验证 QuickPanelDialog::ToggleMode / Show(false, ...) 是否弹

Subagent 2 (Explore, "focusin-reality"):
  - 读 WeaselTSF.cpp OnActivated (cpp:221-257) + 整个 TSF activation sink 注册
  - 读 RimeWithWeasel::FocusIn (cpp:384-449) 当前 v0.19.0.65 状态
  - 读 Show(false, nullptr x 6) 实际行为 (cpp:436-490)
  - 设计一个最小验证 (写一个 hot-key 测试 FocusIn 是否触发) 或读 glog 配置看 DLOG 是否输出

Subagent 3 (general-purpose, "alternative-paths"):
  - 搜项目里所有触发 QuickPanelDialog::Show / ToggleMode / EnableAlwaysShowMode 的地方
  - 列出每个调用点的 caller + 触发条件
  - 评估 spec 061 设计意图 vs spec 074 撤回 vs spec 070 重启的 compatibility matrix
  - 是否有其他路径(例如 tray icon click / language bar button) work?这些路径是不是跟 Win+Space 共享 QuickPanelDialog 单例 state?
```

3 个 subagent 报告后,主 agent 综合分析:
- 哪个 hypothesis 是真的 root cause
- 修复方案 (改源码 / 加 callback / 改 QuickPanelDialog::Show 内部 / 改 hotkey 注册逻辑)
- 装机 verify 方案 (埋点要补哪些 / log 期望是什么样)

---

## 7. 沉淀 (lessons-learned.md 已加的 L##)

| L## | 教训 | 状态 |
|---|---|---|
| L##-PhaseL-2-PunctuatorProcessor | librime schema 加 `punctuator:` 段不够,必须 `engine.processors` 显式注册 | ✅ 沉淀 (v0.19.0.64) |
| L##-PhaseL-3-ImeDefaultWnd | `ImmGetDefaultIMEWnd` 在 hidden IPC server 上不可靠 | ✅ 沉淀 (v0.19.0.64),装机证据 `hwnd=NULL` 进一步证实 |
| L##-PhaseL-4-ImmGetDefaultIMEWnd-Dead | hidden IPC server 无 IME context, `ImmGetDefaultIMEWnd` 永远 NULL | ✅ 沉淀 (本会话) |
| L##-PhaseL-5-L69Fix-Residue | `if (false)` 死代码跨多版本延续 | ✅ 沉淀 (本会话)— 但 user 反馈修回去也 work 不动,**需要新增 L## 关于 single Show state 破坏 / hotkey 回归** |

---

## 8. 剩余 backlog

**Phase L 计划内**:
- ✅ 调整 1 — v0.19.0.60 (取消用户词典) ship
- ✅ 调整 1.2 — v0.19.0.61 / .62 (UserDict 槽位删除) ship
- ✅ 调整 2+3 — v0.19.0.63 ship
- ✅ 装机 bugfix 1 — v0.19.0.64 ship (Bug 1 修,Bug 2 失败)
- ❌ **装机 bugfix 2 — v0.19.0.65 ship (Bug 2 仍失败,Alt+, 回归)** — 需要 Phase L 装机 bugfix 3

**Phase L 之外**:
- ⏸ K3 T019 装机事故调查 — 累积至本会话的 3 次 ship 失败可能就是 T019 类问题
- ⏸ K7 GitHub push HTTP 500 — 持续 fail (走 fallback)
- 🆕 **必须新增 L##**: QuickPanelDialog::Show(false, nullptr x 6) 是否兼容 (null callback 不 work hypothesis)
- 🆕 **必须新增 L##**: Alt+, 回归根因 — 我没改这条路径却 work 不动

---

## 9. 当前 git 状态

```
branch: Fluxing (本地 fork, ahead 943 / behind 831)
last commit: 84f01c5 (v0.19.0.65)
last tag: v0.19.0.65
release/fluxing-0.19.0.65-installer.exe: 已 ship (43,340,676 bytes, md5 1326F50C...)
```

## 10. 新会话必读顺序

1. 本 handoff (`handoff-after-phase-L-bugfix-2-failed-v0.19.0.65-2026-07-25.md`)
2. 上一个 handoff (`handoff-after-phase-L-2-3-bugfix-2-v0.19.0.65-2026-07-25.md`) — ship 流程
3. 上一阶段 handoff (`handoff-after-phase-L-2-3-bugfix-v0.19.0.64-2026-07-24.md`) — Phase L 装机 bugfix 1
4. spec 070 + spec 061 + spec 074 设计意图

## 11. 严格禁止

- ❌ 盲目再改源码试错 — 必须先 subagent 分析
- ❌ 改 QuickPanelDialog::Show 内部 (除非所有调用方都不 work,且走了 spec-init 流程)
- ❌ 改 install.nsi (跟本 bug 无关,且 L09 / L17 / L54 / L66 雷区)
- ❌ 删埋点 (用于下次装机 verify)
- ❌ 改 user 已搁置的 Bug 1 (部件拆字标点)

— END OF HANDOFF —