# handoff-bug-floating-bar.md — v0.19.0.68 浮动设置栏不弹 / Alt+, 无效

> **状态**: Phase 2 instrumentation 已落地, **未修未 ship 未 commit**。
> **目的**: 给装机 user / 下一个会话一份「5 步抓 4 节点 log」剧本 + root cause 判别表 + 修复方向。
> **基线**: branch `Fluxing`, HEAD `c528728` (2026-07-28 v0.19.0.68 ship 台账)。

---

## 1. 现象 (双路径 dead bug)

- **Path A (TSF)**: 切到 Fluxing 后浮动设置栏不弹。
- **Path B (Hotkey)**: Alt+, 也无法弹出浮动栏。
- 两条路径在 `QuickPanelDialog::Show()` 汇合, **都死**, 因此:
  - 共享终点的根因(原因 D) = 极可能
  - 但 gate(m_disabled / Echo())失败也会让两条路径在汇合前死 (原因 A/B)
  - hotkey path 单独的根因(原因 C)只杀 Path B, 但本次两个都死, 因此 C 概率低

---

## 2. 4 个潜在 root cause (已知候选)

| 标号 | 假设 | 关键证据 (装机后) |
|---|---|---|
| **A** | `m_disabled = true` 永久 blocking TSF path (L69-fix 残留) | `FocusIn ENTER m_disabled=1` 出现, 后面 `FocusIn auto-show QuickPanel` 不出现 |
| **B** | `m_client.Echo()` 返回 false 静默 blocking TSF path | `OnActivated ACTIVATE m_client.Echo()=FALSE` 出现, 而 `m_client.Echo()=true` 后面也没有 `FocusIn ENTER` |
| **C** | `RegisterHotKey` 失败 silent (ERROR_HOTKEY_ALREADY_REGISTERED 等) | 完全看不到 `WM_HOTKEY wParam=` (任何 wParam 都没有) |
| **D** | `QuickPanelDialog::Show()` 内部失败 (`CreateWindowExW` / `UpdateLayeredWindow` 失败, `(void)err` 吞掉) | `WM_HOTKEY` / `FocusIn ENTER` 都能进, 但 `QuickPanel::Show entered` 之后 panel 仍不显示; `UpdateLayeredWindow FAILED err=...` 出现 (root cause D 强证据) |

> 注: A 与 B 都会让 TSF 路径死于汇合前; A 不会杀 hotkey 路径, B 也不会(但 B 会让 m_disabled 在某次 _UpdateUI 之后 flip)。**装机 log 必须覆盖两条路径, 才能排除 A vs B vs C vs D**。

---

## 3. 4 节点 instrumentation (DLOG / [QP-DIAG]) — **未 ship**

代码改动只在本地, 没有 commit, 装机 v0.19.0.68 installer **不含**这些埋点; 装机后必须先用含埋点的 instrumentation binary 跑 5 步 diag (见 §4)。

| 节点 | 文件 : 行 | log 前缀 | 内容 | 假设命中 |
|---|---|---|---|---|
| #1 TSF 入口 | `WeaselTSF/WeaselTSF.cpp:289` | `DLOG(INFO)` | `OnActivated ACTIVATE m_client.Echo()=true/FALSE` | B: `=FALSE` |
| #2 IPC 入口 | `RimeWithWeasel/RimeWithWeasel.cpp:387` | `DLOG(INFO)` | `FocusIn ENTER m_disabled=0/1` | A: `=1` |
| #3 Hotkey 入口 | `WeaselIPCServer/WeaselServerImpl.cpp:156` | `DLOG(INFO)` | `WM_HOTKEY wParam=<ID>` | C: 完全没有 |
| #4 Show 入口 | `WeaselServer/QuickPanelDialog.cpp:1178 (QpDiagLog)` | `[QP-DIAG]` 写 weasel-install.log | `QuickPanel::Show entered s_hwnd=...` | D: 有但屏上不显示 |
| #4b ULW 失败 | `WeaselServer/QuickPanelDialog.cpp:1107` | `[QP-DIAG] UpdateLayeredWindow FAILED err=...` | `GetLastError()` 落到 log | D: 强证据 |

> **Why [QP-DIAG] not DLOG**: `WeaselServer/stdafx.h` 不 include `glog/logging.h`, `WeaselServer/QuickPanelDialog.cpp` 编译单元**没有** DLOG。引入 glog 需要改 PCH / xmake.lua 链, 风险大于本次 phase 2 instrumentation。沿用 `ImeLangChangeSubclassProc` 已用的 `weasel-install.log` (CreateFileW 模式) 写同一份 log, user 一次抓全。

> **Why DLOG in 其它 3 处**: 三个 .cpp 已有 glog PCH (`RimeWithWeasel/RimeWithWeasel.cpp`, `WeaselTSF/WeaselTSF.cpp`, `WeaselIPCServer/WeaselServerImpl.cpp`), 任务字面要求 "DLOG_INFO 输出" — 沿用项目惯例。

---

## 4. 装机 user 5 步 diag 流程

详细在 `F:\soft\00selfmade\rime_claude\diag-floating-bar.ps1` (不修改 git, 不 push)。手动等价流程:

1. **备份**: `copy output\WeaselServer.exe diag-floating-bar-bak\WeaselServer.exe.<ts>.bak`, 同样备份 `WeaselTSF.dll`。
2. **装 instrumentation binary**: 跑含 4 节点埋点的 installer (装机 user 知道路径, 这次 build 由 assistant 单独给, 不经 v0.19.0.68 installer)。
3. **触发两条路径**:
   - TSF: `Win+Space` 切到 Fluxing → 等 1s → `Win+Space` 切到 en-US
   - Hotkey: 在任意 edit field 焦点里按 `Alt+,`
4. **抓 log**: `Get-Content $env:APPDATA\Rime\weasel-install.log -Tail 80 -Encoding Unicode` 把末尾 80 行贴回 assistant。
5. **还原 backup**: user 决定是否还原 backup; backup 留在 `output\diag-floating-bar-bak\` 不会被自动删除。

---

## 5. log pattern → root cause 判别表

把 Step 4 抓到的 log 末尾 80 行对照下表(行号仅作参考, 内容更重要):

| 你看到 | 判别 | 修复方向 (待 phase 3 决定) |
|---|---|---|
| **没有 `OnActivated ACTIVATE`** | TSF 路径根本未触发 (用户没切到 Fluxing? CTF 仍未激活?) | 检查 `OnActivated` 在 `WeaselTSF` 是否 hook 上 (`_InitActiveLanguageProfileSink` 状态); spec 070 T201 |
| **有 `OnActivated ACTIVATE m_client.Echo()=FALSE`** | **B**: pipe 不可达 (TSF 进程或 server 端 pipe 状态异常) | 查 `m_client.Connect()` / `m_client.StartSession()` 状态; spec 053 R6 路径 |
| **有 `OnActivated ACTIVATE m_client.Echo()=true`, 但没有 `FocusIn ENTER`** | Echo 后续 pipe write 失败 / 静默被吞 | 加 `m_client.FocusIn()` 后的 IPC write 返回值检查 |
| **`FocusIn ENTER m_disabled=1`** | **A**: `m_disabled = true`, L69-fix 残留 | 查 `m_disabled` 何时被 set (e.g. `SetDisabled(true)` 路径); 也要查 m_disabled 启动默认值 |
| **`FocusIn ENTER m_disabled=0`, 但** 没有 `FocusIn auto-show QuickPanel` (DLOG 已在 L65 改回) | FocusIn 后续 `if (ipc_id > 0)` false | 查 ipc_id 来源 (WeaselTSF 端的 caps / session id) |
| **没有任何 `WM_HOTKEY wParam=`** | **C**: hotkey 根本没到 server, `RegisterHotKey` 失败 / 没注册 | 查 `RegisterHotKey` 调用 (WeaselIPCServer 端 hotkey 启动) + GetLastError 写 log |
| **有 `WM_HOTKEY wParam=...`, 但没有 `QuickPanel::Show entered`** | m_pRequestHandler 为 null (AP-036-G 已守) 或 PostMessage 路径断 | 查 m_pRequestHandler 生命周期 |
| **有 `WM_HOTKEY`, 有 `QuickPanel::Show entered`, 但 panel 屏上不显示** | **D**: Show 内部 | 看下面 ULW 失败 vs 创建失败 |
| **`UpdateLayeredWindow FAILED err=...`** | **D 强证据**: WS_EX_LAYERED surface 提交失败 (e.g. `ERROR_INVALID_PARAMETER` / `ERROR_NOT_ENOUGH_MEMORY`) | 查 `s_hdcMem` / `s_hBmpMem` 状态, 查 `s_panelW_phys` / `s_panelH_phys` 是否异常 |
| **`QuickPanel::Show entered s_hwnd=0x...` 之后什么都没发生, 也无 ULW FAILED** | Show 成功, panel 已显示但用户**没看到** (位置 / DPI / 透明度 / 桌面前景覆盖) | 查 `ComputeOrigin` 位置, `s_alpha` / `s_targetAlpha`, 桌面其他 app |

---

## 6. 修复方向 (phase 3 候选, **本会话禁止动手**)

> 不在本次 commit, 不动以下任何文件: `RimeWithWeasel/RimeWithWeasel.cpp` 的 `m_disabled` flip / `WeaselTSF/WeaselTSF.cpp` 的 `Echo()` / `WeaselIPCServer/WeaselServerImpl.cpp` 的 `RegisterHotKey` / `WeaselServer/QuickPanelDialog.cpp` 的 `Show` 业务路径。

- **A**: 启动时 m_disabled 默认值审计 + 写一份 `m_disabled` 翻转状态机 mini-spec(谁 set / 谁 reset / 何时), 详见 spec 070 之前 L69 修复的边界条件
- **B**: 给 `m_client.Echo()` 失败路径写一个 `DLOG(WARNING) << "Echo failed, fallback to direct call"` 路径(spec 053 R6 已部分覆盖, 但 R6 是 init 期, 不是 hot path)
- **C**: 给 `RegisterHotKey` 加 `GetLastError` 写 weasel-install.log, 包含 `ERROR_HOTKEY_ALREADY_REGISTERED` / `ERROR_INVALID_PARAMETER` / `ERROR_ACCESS_DENIED` 3 个常见错
- **D**: Show 内部 `CreateWindowExW` 也加 `GetLastError` 写 log (与 ULW 一致); 另加 `IsWindowVisible(s_hwnd)` / `GetWindowRect(s_hwnd, ...)` post-Show 写 log 验证 panel 真在屏上

---

## 7. 下一步 (new session 推荐顺序)

1. **新会话**承接本文 + `MEMORY.md` + `claude_desktop_session_health.md`。
2. 跑 `diag-floating-bar.ps1` (5 步抓 log)。
3. 把 log 末尾 80 行贴回新会话, 用 §5 判别表定位 root cause。
4. **不要**在 log 出来前定 fix —— Iron Law: NO FIXES WITHOUT ROOT CAUSE INVESTIGATION FIRST。
5. 修复决定后, 走 spec 070 / spec 074 / 新 spec 路线; 一个 fix = 一个 PR, 配套 test (`test/TestWeaselIPC/*`); 走 §6 的修复方向, **禁止** 一次性 bundle。

---

## 8. 改动清单 (本会话, 全部未 commit)

| 文件 : 行 | 改动 | 性质 |
|---|---|---|
| `RimeWithWeasel/RimeWithWeasel.cpp:387` | 加 `DLOG(INFO) << "FocusIn ENTER m_disabled=" << m_disabled;` | instrumentation |
| `WeaselTSF/WeaselTSF.cpp:289` | 加 `DLOG(INFO) << "OnActivated ACTIVATE m_client.Echo()=" << ...;` | instrumentation |
| `WeaselIPCServer/WeaselServerImpl.cpp:156` | 加 `DLOG(INFO) << "WM_HOTKEY wParam=" << wParam;` | instrumentation |
| `WeaselServer/QuickPanelDialog.cpp:1107` | `(void)err` 替换为写 `weasel-install.log` 含 `err` | instrumentation |
| `WeaselServer/QuickPanelDialog.cpp:1160-1176` | 新增 `QpDiagLog()` helper + `#` 注释 | instrumentation |
| `WeaselServer/QuickPanelDialog.cpp:1178-1180` | Show 入口 `QpDiagLog(L"QuickPanel::Show entered")` | instrumentation |
| `diag-floating-bar.ps1` | 新建 diag 5 步脚本 | diag tool |
| `handoff-bug-floating-bar.md` | 本文件 | handoff |

---

## 9. 禁止事项 (本会话, 已遵守, 下会话也守)

- ❌ 不 commit 任何修改到 git
- ❌ 不 fix code (仅 instrumentation, 4 节点)
- ❌ 不 push 任何东西
- ❌ 不 ship (4 节点埋点不进 v0.19.0.68 installer)
- ❌ 不在 instrumentation 没 log 之前 fix (Iron Law)

---

## 10. 必读顺序 (new session)

1. `MEMORY.md` + `claude_desktop_session_health.md`
2. 本文件 `handoff-bug-floating-bar.md` (尤其 §4 / §5)
3. `CONTEXT.md` §5 历次 L## 索引 (L66 / L67-L69 GDI+ / L70 / L74 / L81 / L97)
4. `lessons-learned.md` L## Phase L 系列 (L##-PhaseL-3 ~ L##-PhaseL-8)
5. 4 节点代码位置(`RimeWithWeasel.cpp:387` 等, 配合图谱 `search_graph` 看 cross-call)
6. `diag-floating-bar.ps1` (5 步 diag 工具)
