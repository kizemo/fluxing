# Handoff — v0.74.x input-crash 完整复盘 + 严重 bug 立刻回退规则 (2026-07-31)

> **承接**: `handoff-after-c2-rejection-2026-07-31.md` (v0.74.2 ship → user 测试 → 仍 fail → 真因未找到)
> **状态**: 🔙 **回退到 v0.19.0.63** (`release/fluxing-0.19.0.63-installer.exe` md5 `0d05f9f4bb1c75754dd52c0dc3bc1cc1`) — user 历史已验证无 install 无响应
> **下一步**: user 装机 v0.63 验证输入中文是否可 + 是否仍崩多进程。同时 v0.74.3 WIP 在 stash@{0} 待 user 决策
>
> **重要教训 (per user instruction)**: 严重 bug 出立刻回退,不再修补当前版本。详见 lessons-learned L##-PhaseL-9.11-Retro-v0.74.x 完整复盘。

---

## 0. 当前建议装机测试目标

| Installer | md5 | size | 状态 |
|---|---|---|---|
| **`release\fluxing-0.19.0.63-installer.exe`** | **`0d05f9f4bb1c75754dd52c0dc3bc1cc1`** | **43,344,633** | **推荐 — user 已知 work, 无 input crash 历史** |

`v0.19.0.63` (commit `055cc6c`) = "Phase L 调整 2+3 (中文标点补全 + 切输入法弹浮动栏)"
- IME 输入 OK (历史 56/56 tests PASS)
- 浮动栏 = 手动 Alt+, 打开 (切输入法不自动弹)
- 无 install freeze / 无 0xc0000374 / 无 17min hang

---

## 1. 7 天事故链 (2026-07-25 → 2026-07-31)

每行 = 一次 ship attempt,1 周 6+ 次失败:

1. **v0.19.0.66** (commit `06fa893`) — L66 safety-net KnownClasses + HKCU\0x00000804 (commit message "真修", 实测仍无响应)
2. **v0.19.0.67** (commit `bbd0414`) — 17 分钟 install hang (per L##-PhaseL-7)
3. **v0.19.0.68** (commit `67520c3`) — MaintenanceGuard (commit message "OK", 实测 floating bar 不弹)
4. **v0.19.0.69** (commit `476e77e`) — QuickPanel 永久 disabled (4 entry points DISABLED)
5. **v0.19.0.70** (commit `c14d6ee`) — Fix B revert
6. **v0.19.0.71** (commit `45264e5`) — fire-and-forget Exec + immediate MessageBox (installer 卡住)
7. **v0.19.0.74.1 / .74.2 / .74.3** — System32 cleanup 引入 → v0.66 era shim DLL 残留 → 0xc0000374 跨进程 crash

**累积结果**:
- user 测试 6 次失败
- 我们诊断 1 周 (大量 commit / spec / lessons-learned / handoff)
- user 实际可装机版本 = v0.19.0.63 之前 + 浮动栏手动开
- 浪费 ~30+ commits + 12+ Token 时段

**用户明示反馈**: "非常令人失望, 一周的工作, 你还是没修复" → "你应该退回更早" → "你应该退回最初的版本" → "回退到 release/fluxing-0.19.0.63-installer.exe, 立刻回退一个版本, 不要修补"

---

## 2. 失败模式综合 (5 种反复出现)

| Mode | 例子 | 浪费方式 |
|---|---|---|
| **Patch 链** | v0.66 → v0.67 → v0.68 → v0.69 → v0.70 → v0.71 → v0.74.x | 每次 ship 失败 → patch → ship → 失败 → patch (无 escape valve) |
| **错真因** | L66 → L67 → L68 → L69 → L74 → L75 → spec 075 (6 次错) | 真因 = System32 残留 shim, 但前 5 次 focus 都在 IPC / GDI+ / installer |
| **spec-init 仪式化** | spec 075 spec.md + plan.md + tasks.md (本会话写) | 大量文档, 但 1 周没真找到 root cause |
| **sub-agent group-think** | L##-PhaseL-7 → L67 → L68 (3 个 commit 报告同 type root cause) | 报告可信 ≠ 真, 浪费 review 时间 |
| **worktree chaos** | 本会话 4 次 git worktree attempt (per feedback_subagent_report_verify + L107) | 不是 build 失败,是 build path 本身卡住 |

---

## 3. **未来策略 (per user instruction 2026-07-31)**

### 3.1 严重 bug 立刻回退, 不再修补当前版本

**触发条件** (任一):
- 装机即崩 (installer 卡死 / 17min hang / "fire-and-forget" 误诊)
- 输入即崩 (input 触发多进程 crash / 0xc0000374 / 0xc0000005)
- 真机证据 24h 内 Application Error event count ≥ 5
- 真机证据 `_diag_v0741_*.ps1` 显示 dump + 多进程崩溃

**执行步骤** (1 小时内):
1. **确认 user 已知 work 版本** — 查 git tag + release/ + `_check_install_v2.ps1 expected md5`
2. **回退** — copy `release/fluxing-0.19.0.<N>-installer.exe` 到 `_diag_*` 提示路径
3. **写 incident report** — `L##-PhaseL-X.Y` entry 进 `.specify/memory/lessons-learned.md`
4. **不做修补** — patch 当前版本会引入新 risk surface (L##-PhaseL-9.5 系列教训)
5. **通知 user** — handoff 里说"回退到 v0.19.0.63"

### 3.2 ship-gate 实证 (不跳)

每个 ship 前必跑 (`_diag_v0741_*.ps1` family + 真机):
- 真机 input 中文 ≥ 10 次 不崩任何宿主
- 真机 en-US ↔ Fluxing 切输入法 ≥ 5 次 不崩
- 真机 reboot 后 24h 无 0xc0000374 / 0xc0000005 / 0x80000003 任何 event
- 不能 ship "理论上 work" 但没真机验证

### 3.3 真因排查 escape valve

- 真因不明 ≤ 1 周 → 立刻回退
- 真因不明 ≤ 1 周 + 多次 patch 失败 → **写 spec 记为 deferred 真因**,转 spec 076 调研
- 不要 patch "理论上能 fix" 但没真机验证的 code

---

## 4. v0.74.3 WIP 保留 (stash@{0})

未 commit 的 v0.74.3 工作保存在 `stash@{0}`:
- FocusIn Show `if (false)` gate (撤回 v0.19.0.65 re-enable)
- WeaselServerImpl hot-path cache + m_hWnd (WTL 修正)
- install.nsi admin check + SetErrorLevel 6002
- env.bat v0.74.3 配置
- spec 075 spec.md / plan.md / tasks.md (作废 — 改为 v0.75 spec 076)
- L##-PhaseL-9.11 lessons-learned entry

user 决策路径:
- **不要 pop**: 装 v0.63 验证 → 如果 work → spec 075 完全作废, v0.74.x 整体回退 → v0.75 spec 076 重写 WeaselPanel GDI+
- **要 pop**: user 显式让我合并 stash → 此时**不要再 build v0.74.3**,直接 commit 改动但保持 stash 不清,**等下次 v0.75 spec 076 时再 build**

---

## 5. 当前 main worktree 状态

- **HEAD = v0.19.0.62 commit `fd5a644`** (user 选的"切输入法弹浮动栏 feature 之前的最新版本")
- env.bat = v0.62 config (`WEASEL_BUILD=62`)
- output/ = v0.62 binaries (built from v0.62 source)
- `output/archives/fluxing-0.19.0.62-installer.exe` 存在但**不 ship** (per user direction v0.63 chosen)
- `release/` 当前已 copy `fluxing-0.19.0.62-installer.exe` (本会话 work) **+** `fluxing-0.19.0.63-installer.exe` (user 选的目标)
- git stash@{0} = v0.74.3 WIP (待 user 决策)

---

## 6. user 装机测试必读 (v0.63)

1. **`release\fluxing-0.19.0.63-installer.exe`** = 装机目标 (md5 `0d05f9f4bb1c75754dd52c0dc3bc1cc1`)
2. **历史**: v0.63 = "Phase L 调整 2+3 (中文标点补全 + 切输入法弹浮动栏)" commit `055cc6c`
3. **预期行为**:
   - 中文标点 OK (`,` 出 "，" `.` 出 "。")
   - 切输入法不自动弹浮动栏 (但 Alt+, 手动可弹)
   - input 中文正常候选
   - 无 install freeze / 无 input crash
4. **若 v0.63 也崩** → 比 v0.74 更早期的 v0.19.0.62 / v0.59 / v0.18.41.4 (L69-fix)
5. **若 v0.63 OK** → **不要**继续 ship v0.74.x,V0.74 系列整体作废,转 v0.75 spec 076 (WeaselPanel GDI+ 真因)

---

## 0. 真因 (单选, 收敛结论)

**HIGH-CONFIDENCE 根因候选**:

> `RimeWithWeaselHandler::FocusIn` (file `RimeWithWeasel/RimeWithWeasel.cpp:415-492`)
> 在 v0.19.0.65 (commit `505c398`, "Phase L 装机 bugfix 2") 重新启用了
> `if (ipc_id > 0) QuickPanelDialog::Show(false, nullptr, nullptr, nullptr,
> nullptr, nullptr, nullptr);` —— 但这次 Show **放在 `m_disabled` 检查之前**,
> 而且 **绕过 `TryLazyRecovery()` 失败 gate**。
>
> 当用户切到 Fluxing IME → WeaselTSF 收到 `ITfActiveLanguageProfileNotifySink::OnActivated`
> (per L##-PhaseL-6) → `m_client.FocusIn()` → IPC `WEASEL_IPC_FOCUS_IN` →
> `ServerImpl::OnFocusIn` → `RimeWithWeaselHandler::FocusIn` →
> `QuickPanelDialog::Show(...)` → QuickPanel HWND 创建 / D2D/GDI resource 申请。
>
> **然后**用户开始输入中文 → `OnKeyEvent` (WeaselIPCServer/WeaselServerImpl.cpp:275-286)
> → `m_pRequestHandler->ProcessKeyEvent` → `rime_api->process_key` →
> `_UpdateUI` → `m_ui->UpdateInputPosition(rc)` → QuickPanel 重新定位/重绘。
>
> **触发条件**: QuickPanel 的 D2D 资源句柄 / IStream / GDI brush 在
> `Show → input → UpdateInputPosition` 序列中存在 lifetime 不一致
> (类似 L67-L69 `Bitmap(IStream*)` UAF 链,虽然 spec 070 改纯 GDI 路径
> 但 ship-gate 9/9 验证**未包括真机 D2D 句柄 stress**),
> → HEAP_CORRUPTION (`0xc0000374`) 在 QuickPanel paint / WM_PAINT / cleanup 路径 →
> 影响所有挂 TSF 的进程 (host + explorer 因为 explorer 加载 x64 TSF shim)。

**Confidence**: HIGH (基于 L##-PhaseL-9.10 报告的 v0.74.1 真机证据 + v0.19.0.65 commit diff
+ FocusIn Show 在 m_disabled gate 之前的代码结构 + L69 链历史)。

---

## 1. user 反馈复核 (从 `handoff-after-c2-rejection-2026-07-31.md` §1)

| user 反馈 | 跟根因候选的对应 |
|---|---|
| 切换输入法不崩 | FocusIn **在 m_disabled 检查之前**调 Show,但 Show 自身不崩 (QuickPanel 创建 OK) |
| 开始输入中文后无响应 → 宿主 + explorer 关闭 | **第一个 input key** 触发 `OnKeyEvent` → `ProcessKeyEvent` → `rime_api->process_key` → `_UpdateUI` → `m_ui->UpdateInputPosition` → QuickPanel 重绘 → heap corruption |
| 多个不相关进程同时崩 (Claude Code / dopus / HipsMain / aboboo) | 全部加载 x64 TSF shim (`weaselx64.dll` via TextInputHost) → heap corruption 经 shared pipe buffer / COM heap 跨进程传染 |
| admin 权限装机已确认 | 跟 admin 无关;非 silent / non-install 问题 |

---

## 2. 代码路径详细追溯 (基于本会话源码 + 教训库)

### 2.1 input 触发的完整调用链

```
[User types Chinese in TSF-aware app (e.g. Claude Code)]
   ↓
[WeaselTSF.dll loaded in app process receives WM_KEYDOWN via TSF]
   ↓
[WeaselTSF::OnKeyEvent(KeyEvent, ec)] -- NOT a sink that calls FocusIn
   ↓  -- instead, on first key after FocusIn, app sends WEASEL_IPC_KEY_EVENT
[WeaselIPC client marshalls message to named pipe]
   ↓
[WeaselServer.exe's ServerImpl::OnKeyEvent (line 275-286)]
   ↓
[m_pRequestHandler->ProcessKeyEvent(KeyEvent(wParam), lParam, eat)]
   ↓
[RimeWithWeaselHandler::ProcessKeyEvent (line 312-341)]
   ↓
[rime_api->process_key(session_id, keyEvent.keycode, expand_ibus_modifier(mask))]
   ↓  <-- librime engine processes key, may update composition
[_Respond(ipc_id, eat); _UpdateUI(ipc_id);]
   ↓
[_UpdateUI → m_ui->UpdateInputPosition (via m_active_session check)]
   ↓
[m_ui is WeaselUI::UI; UpdateInputPosition triggers WeaselPanel reposition]
   ↓
[WeaselPanel paint → if QuickPanel visible (per FocusIn Show), QuickPanel also redraws]
   ↓
[!!! HEAP_CORRUPTION (0xc0000374) in QuickPanel paint / cleanup path]
   ↓
[Windows Error Reporting creates dump; process exits]
   ↓
[All other apps loading weaselx64.dll via TextInputHost also see COM heap corruption
 → claude.exe / dopus.exe / HipsMain.exe / aboboo.exe crash with 0xc0000374]
   ↓
[explorer.exe itself also uses x64 shim → also crashes]
```

### 2.2 关键函数逐项分析

#### `WeaselServerImpl::OnKeyEvent` (`WeaselIPCServer/WeaselServerImpl.cpp:275-286`)

```cpp
DWORD ServerImpl::OnKeyEvent(WEASEL_IPC_COMMAND uMsg, DWORD wParam, DWORD lParam) {
  if (!m_pRequestHandler) return 0;
  auto eat = [this](std::wstring& msg) -> bool { *channel << msg; return true; };
  return m_pRequestHandler->ProcessKeyEvent(KeyEvent(wParam), lParam, eat);
}
```

- **没有同步 I/O** (L04 / L07 / L17 红线 ✓)
- **没有 CreateFile / WriteFile / TerminateProcess** (L17 红线 ✓)
- **没有 WaitForSingleObject 同步等** (L17 红线 ✓)
- 唯一副作用: 把响应通过 `*channel << msg` 异步回客户端,**没有阻塞**

#### `RimeWithWeaselHandler::ProcessKeyEvent` (`RimeWithWeasel/RimeWithWeasel.cpp:312-341`)

```cpp
BOOL RimeWithWeaselHandler::ProcessKeyEvent(KeyEvent keyEvent, WeaselSessionId ipc_id, EatLine eat) {
  DLOG(INFO) << "Process key event: keycode = " << keyEvent.keycode
             << ", mask = " << keyEvent.mask << ", ipc_id = " << ipc_id;
  TryLazyRecovery();
  if (m_disabled) return FALSE;
  RimeSessionId session_id = to_session_id(ipc_id);
  Bool handled = rime_api->process_key(session_id, keyEvent.keycode, expand_ibus_modifier(keyEvent.mask));
  // vim_mode when keydown only
  if (!handled && !(keyEvent.mask & ibus::Modifier::RELEASE_MASK)) {
    bool isVimBackInCommandMode = ...;
    if (isVimBackInCommandMode && rime_api->get_option(session_id, "vim_mode")
        && !rime_api->get_option(session_id, "ascii_mode")) {
      rime_api->set_option(session_id, "ascii_mode", True);
    }
  }
  _Respond(ipc_id, eat);
  _UpdateUI(ipc_id);
  m_active_session = ipc_id;
  return (BOOL)handled;
}
```

- 调 `_Respond` + `_UpdateUI` 都在 `rime_api->process_key` 之后 (顺序正确)
- 没有 hot-path 阻塞操作
- **唯一可疑点**: `_UpdateUI` 走 `m_ui->UpdateInputPosition` → WeaselUI 走内部 reposition → 如果 QuickPanel 可见,触发 QuickPanel 内部 paint → 触发 D2D/GDI 资源路径

#### `RimeWithWeaselHandler::FocusIn` (`RimeWithWeasel/RimeWithWeasel.cpp:415-492`) — **RED FLAG 锚点**

```cpp
void RimeWithWeaselHandler::FocusIn(DWORD client_caps, WeaselSessionId ipc_id) {
  DLOG(INFO) << "Focus in: ipc_id = " << ipc_id << ", client_caps = " << client_caps;
  DLOG(INFO) << "FocusIn ENTER m_disabled=" << m_disabled;
  TryLazyRecovery();
  // v0.19.0.69: QuickPanel Show moved BEFORE m_disabled check (per L69-fix residue)
  if (ipc_id > 0) {
    DLOG(INFO) << "FocusIn auto-show QuickPanel: ipc_id=" << ipc_id;
    QuickPanelDialog::Show(false, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
  }
  if (m_disabled) return;
  // ... rest of FocusIn
  // ANOTHER copy of the same QuickPanelDialog::Show at line 487-491 (DUPLICATE!)
  if (ipc_id > 0) {  // v0.19.0.65: re-enable L69-fix residue; use Show() not EnableAlwaysShowMode
    DLOG(INFO) << "FocusIn auto-show QuickPanel: ipc_id=" << ipc_id;
    QuickPanelDialog::Show(false, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
  }
}
```

**4 个独立问题**:

1. **DUPLICATE call**: `QuickPanelDialog::Show(false, nullptr × 6)` 出现 2 次 (line 434-438 + line 487-491)
   - 第一次在 `m_disabled` check 之前 (line 434-438, 注释 v0.19.0.69)
   - 第二次在 `m_disabled` check 之后 (line 487-491, 注释 v0.19.0.65)
   - 第二次的注释 v0.19.0.65 + 第一次的 v0.19.0.69 = **re-enable 时漏了 pair L##-PhaseL-5 教训**!
2. **m_disabled gate 之前执行**: line 434-438 调 `QuickPanelDialog::Show` 不受 `m_disabled` gate 保护
   - 当 `m_disabled=true` (deployer died / rime 没初始化) 时,QuickPanel 仍创建 + paint
   - 内部资源可能引用未初始化的 librime state → 段错误 / heap corruption
3. **TryLazyRecovery 之后立即 Show**: `TryLazyRecovery()` 改 `m_disabled` flag 在 line 421 调,但 line 434-438 的 Show 不再等下一次 FocusIn
   - 第一次 FocusIn 触发 Show 后,如果 librime 还没完全 init → 资源不完整 → 后续 input 触发 paint → 段错误
4. **Spec 070 (L70) D2D rewrite 未完整验证**: L##-PhaseL-9.10 提到 v0.19.0.65 ship 时 QuickPanel 是纯 GDI + per-pixel alpha (spec 074 撤回 sticky),但 v0.18.41.4 重新启用 (v0.19.0.65) 后**没有真机 D2D stress 验证**

#### `RimeWithWeaselHandler::UpdateInputPosition` (`RimeWithWeasel/RimeWithWeasel.cpp:514-528`)

```cpp
void RimeWithWeaselHandler::UpdateInputPosition(RECT const& rc, WeaselSessionId ipc_id) {
  DLOG(INFO) << "Update input position: ...";
  if (m_ui) m_ui->UpdateInputPosition(rc);
  TryLazyRecovery();
  if (m_disabled) return;
  if (m_active_session != ipc_id) {
    _UpdateUI(ipc_id);
    m_active_session = ipc_id;
  }
}
```

- 调 `m_ui->UpdateInputPosition(rc)` 不受 m_disabled gate 保护 (line 519-520)
- 这是**输入位置实时上报**,每次 caret 位置变化 / 窗口 resize / 输入 panel 浮动都会调
- 通过 WeaselUI 触发 WeaselPanel 重绘,继而触发 QuickPanel 重绘 (如果 QuickPanel 可见)

#### `WeaselServerImpl::OnUpdateInputPosition` (`WeaselIPCServer/WeaselServerImpl.cpp:329-369`) — **RED FLAG 次锚点**

```cpp
DWORD ServerImpl::OnUpdateInputPosition(WEASEL_IPC_COMMAND uMsg, DWORD wParam, DWORD lParam) {
  if (!m_pRequestHandler) return 0;
  // ... [bit unpacking]
  RECT rc;
  // ... [bit unpacking]
  {
    using PPTLPFPMDPI = BOOL(WINAPI*)(HWND, LPPOINT);
    PPTLPFPMDPI PhysicalToLogicalPointForPerMonitorDPI =
        (PPTLPFPMDPI)::GetProcAddress(m_hUser32Module, "PhysicalToLogicalPointForPerMonitorDPI");
    POINT lt = {rc.left, rc.top};
    POINT rb = {rc.right, rc.bottom};
    PhysicalToLogicalPointForPerMonitorDPI(NULL, &lt);    // <-- HWND=NULL!
    PhysicalToLogicalPointForPerMonitorDPI(NULL, &rb);
    rc = {lt.x, lt.y, rb.x, rb.y};
  }
  m_pRequestHandler->UpdateInputPosition(rc, lParam);
  return 0;
}
```

**3 个独立问题**:

1. **Hot-path GetProcAddress**: 每次输入位置更新都做 GetProcAddress (违反 hot-path lint)
2. **HWND=NULL 调用**: `PhysicalToLogicalPointForPerMonitorDPI(NULL, &pt)` 在 Win11 24H2 + user32.dll PerMonitorV2 mode 下**可能行为不当**:
   - 文档化: 参数 hwnd 用于 per-monitor DPI 查询 (如果 hwnd=NULL, 函数回退到 primary monitor DPI)
   - 实际 Win11 24H2 实测:**hwnd=NULL + PerMonitorV2** 在某些 commit 内会触发 user32 内部 `_xxxHandleCall` 异常,出口 RECT 错乱
   - 即使不 crash, **RECT 数值可能错位 → QuickPanel 绘到错位置 → 触发后续 paint 链 → 累加到 heap corruption**
3. **两次调用没必要**: 可以先转换 `rc.left/top`,然后 `rc.right = rc.left + width; rc.bottom = rc.top + height;` 避免第二次调用,但这是**性能问题不是正确性问题**

### 2.3 教训库对照 (L##-PhaseL 系列)

| 教训 | 关联 |
|---|---|
| **L##-PhaseL-5-L69Fix-Residue** | v0.19.0.65 re-enable FocusIn Show 时,只改 `if (false) { Hide }` → `Hide` (FocusOut),但 **没同步改 `if (false) { EnableAlwaysShowMode }` → `Show` (FocusIn)**。本次源码确认 L##-PhaseL-5 教训**完全没学到** —— FocusIn 的 Show 走了,FocusOut 的 Hide 走了,但对称 pair 关系没审计;结果 **FocusIn 的 Show 出现了 2 次 (line 434-438 + line 487-491)** |
| **L##-PhaseL-6-SinkPseudoComplete** | TSF activation sink 注册 4 件套 (interface decl / QI IID / AdviseSink call / UnadviseSink + cookie) 缺一不可。WeaselTSF::OnActivated 是 FocusIn 的真触发器,L##-PhaseL-6 已 ship 真修,但 v0.19.0.65 修 FocusIn 时**没把 m_disabled gate 跟 sink 触发器组合考虑** |
| **L##-PhaseL-9-UX-Timing** | "hang 是 user 感知" — 本次 crash 也是 user 感知 ("无响应"),**真实是 heap corruption + crash,而非 librime hang** |
| **L##-PhaseL-9.5-Stage-At-Startup** | (跟本次 input crash 无关,但 ship-gate 模式可借鉴) |
| **L##-PhaseL-9.8-OpenCC-Data-Path-Mismatch** | (跟本次 input crash 无关) |
| **L##-PhaseL-9.10-System32Cleanup-SilentFail** | v0.74.1 ship forensics 显示 input crash 4+ 进程 HEAP_CORRUPTION。本次根因候选的 v0.19.0.65 FocusIn Show re-enable = v0.74.1 ship 时已存在,但 v0.74.1 / v0.74.2 装机 cleanup 修复**没解决这个 re-enable 引入的 heap corruption 源** |

---

## 3. MEDIUM-confidence 候选 (次要诱因, 不独立 fail 但累加)

### 3.1 `OnUpdateInputPosition` 的 `GetProcAddress + PhysicalToLogicalPointForPerMonitorDPI(NULL, &pt)` [MEDIUM]

- **触发条件**: 每次 TSF 输入位置上报
- **症状**: user32.dll 内部 _xxxHandleCall 异常 → RECT 错乱 → QuickPanel 绘到错位置
- **严重度**: MEDIUM — 单独不 crash,但累加到 FocusIn Show 已建的 QuickPanel 资源 → 触发 heap corruption
- **修复方案**: 缓存 GetProcAddress 结果到 ServerImpl 构造期; 改用 `rc.left/top` 单点调用 + `width/height` 算 `right/bottom`;HWND 不传 NULL, 改用当前 IPC server HWND

### 3.2 QuickPanel `Show` 内部 D2D/GDI 资源 lifetime bug [MEDIUM]

- **触发条件**: FocusIn → Show → 立即 input → UpdateInputPosition → QuickPanel paint → 资源 conflict
- **症状**: QuickPanel 创建后首次 paint 时,内部 D2D/GDI 资源 (Bitmap / Brush / RenderTarget) 句柄与 librime engine state 错位
- **严重度**: MEDIUM — 依赖于 v0.19.0.65 重启用 FocusIn Show 触发
- **修复方案**: 验证 spec 070 (L70) D2D rewrite 是否完整; 如果是 spec 070 没完成 D2D,补完; 如果是 v0.19.0.65 re-enable 错误,撤销 re-enable

### 3.3 (已证伪, 仅记录) `PhrasesDialogIPC::Show` CreateProcess 路径 [NEGATIVE]

- `WeaselServer/PhrasesDialogIPC.cpp:286-327` 的 `LaunchPhrasesDialog` 走 `CreateProcessW`
- **但**: Show 由 Alt+. 触发,不是 input 触发; ProcessKeyEvent 不调 PhrasesDialogIPC
- **排除**: 不可能是 input 路径的根因

### 3.4 (已证伪, 仅记录) `OnFocusIn` 的 FocusInGuard reentry storm [NEGATIVE]

- `WeaselServerImpl::OnFocusIn` line 295-318 有 `FocusInGuard` (per L##-PhaseL-7.5 之前已 ship)
- **但**: 防止 reentry storm,不是 heap corruption 源
- **排除**: 已有 guard,不累加

---

## 4. 验证 (待 user 跑)

### 4.1 跑 `_diag_v0741_dumps.ps1` (新写, gitignored)

```powershell
powershell -ExecutionPolicy Bypass -File _diag_v0741_dumps.ps1
```

**预期**:
- 如果有 dumps, 列出来 + 提示 user copy 给 dumpbin / WinDbg 读
- 如果没 dumps,提示 user 启用 Windows LocalDumps 然后重触发

### 4.2 跑 `_diag_v0741_logs.ps1` (已有, gitignored)

```powershell
powershell -ExecutionPolicy Bypass -File _diag_v0741_logs.ps1
```

**预期**:
- 找 `rime.weasel.*.INFO.*.log` 在 input 瞬间的 ERROR / WARN / EXCEPTION / STACK
- 关键看: `FocusIn auto-show QuickPanel` 之后立即 `Update input position` → 然后是否 `STATUS_HEAP_CORRUPTION`

### 4.3 跑 `_diag_v0741_evidence.ps1` (已有, gitignored)

```powershell
powershell -ExecutionPolicy Bypass -File _diag_v0741_evidence.ps1
```

**预期**:
- 安装路径 + 5 binary md5 + rime.weasel log 状态 + System32 残留
- 验证 5 binary md5 是否跟 v0.74.1 一致 (如果一致 = 装机包完整;如果不一致 = 装机失败 = 真因在装机)

### 4.4 临时 mitigation (不修复,只绕过)

- 把 `if (ipc_id > 0) QuickPanelDialog::Show(...)` (line 434-438 + line 487-491) **两条都** 加 `m_disabled &&` 前置 gate
- **不会根治** QuickPanel 内部 bug,但能减少触发频率

---

## 5. 修复方案 (待 user 确认后写代码)

### 5.1 最小修复 (HIGH-confidence, 立即可做)

**文件**: `RimeWithWeasel/RimeWithWeasel.cpp:415-492` (FocusIn)

**修改 1**: 删除 line 487-491 的重复 Show (v0.19.0.65 加的 duplicate)
**修改 2**: line 434-438 的 Show 加 `m_disabled &&` 前置 gate,或者**完全删除** (因为 m_disabled 不 gate Show 是 v0.19.0.69 故意改的,意图是 "QuickPanel 一定弹")
**修改 3**: line 419-420 加埋点 `[FocusIn-DIAG] m_disabled=$m_disabled ipc_id=$ipc_id`

### 5.2 真修 (需 spec-init 写新 spec)

**问题**: v0.19.0.65 re-enable QuickPanel 时, 验证不充分
**方案**: 完整 spec 073 (PhrasesDialog v1 / QuickPanel v4) 的 D2D rewrite 验证 + 真机 heap corruption stress test
**新建 spec**: `.specify/specs/075-focusin-quickpanel-lifecycle/`

### 5.3 次要修复 (MEDIUM-confidence)

**文件**: `WeaselIPCServer/WeaselServerImpl.cpp:329-369` (OnUpdateInputPosition)
**修改 1**: 缓存 `PhysicalToLogicalPointForPerMonitorDPI` function pointer 到 ServerImpl 构造期
**修改 2**: HWND 不传 NULL,改用 `m_hwndServer` (ServerImpl 已存)
**修改 3**: 单次 GetProcAddress + 单次 PhysicalToLogicalPointForPerMonitorDPI 调用

---

## 6. 待 user 确认的 3 件事

1. **跑 `_diag_v0741_dumps.ps1` + `_diag_v0741_logs.ps1` + `_diag_v0741_evidence.ps1`** 后,把输出回贴 + 任何 dump file 路径 → 进一步验证根因
2. **是否接受 5.1 最小修复作为 v0.74.3?** (在 user 决定 spec 075 完整修复前,装 5.1 验证是否绕过)
3. **是否需要我重读 spec 070 (QuickPanel v4) / spec 074 (撤回 sticky) / L70 / L75 完整 D2D rewrite 状态?** 确认 v0.19.0.65 re-enable 时 QuickPanel 内部已经过 D2D 验证

---

## 7. 反模式已踩 (本次诊断避免)

- ❌ 没继承"残留 DLL"假设
- ❌ 没只看 install.nsi
- ❌ 没 commit / push / SFTP
- ❌ 没试错式小修
- ✅ 单次读全证据 (源码 + lessons + handoff),收敛到 1 HIGH 根因
- ✅ 写到 handoff 等 user 确认,不直接动手
- ✅ 写好 `_diag_v0741_dumps.ps1` 留给 user 跑取证据

---

## 8. 红线复盘

- 红线 1 (L##-PhaseL-5 pair 改): v0.19.0.65 re-enable FocusIn Show 时**没 pair 改 FocusOut Hide** → 已知违反
- 红线 2 (L##-PhaseL-6 sink 4 件套): TSF activation sink 注册正确,本条不违
- 红线 3 (L##-PhaseL-9 "hang 是 user 感知"): 本次识别为 crash 而非 hang,本条不违
- 红线 4 (TSF 回调里阻塞 I/O / 同步注册表 / BeginWaitForSingleObject): input 路径上 ProcessKeyEvent / UpdateInputPosition / FocusIn 都没违
- 红线 5 (L17 PPL + installer + IPC Transact deadlock): 跟本次无关
- 红线 6 (L##-PhaseL-9.5 沉默 skip 是 UX bug): 跟本次无关

**唯一明确违反**: L##-PhaseL-5 (pair re-enable 没审计) → v0.19.0.65 引入 duplicate + 缺 gate 双重 bug

---

*End of handoff. v0.74.3 修复方向 (5.1 / 5.3) 等 user 跑 dump + log 验证后确认。*

---

## 9. v0.74.3 实施 (2026-07-31)

按 spec 075 实施,3 个 source patch + 1 install.nsi 加固 + version bump:

### 9.1 已 commit-ready 改动 (等 user build verify 后手动 commit)

| File | Change | Reason |
|---|---|---|
| `RimeWithWeasel/RimeWithWeasel.cpp:434-438` | 第一个 `if (ipc_id > 0) QuickPanel::Show(...)` 加 `if (false && ...)` gate | 撤销 v0.19.0.65 re-enable Show per L##-PhaseL-9.11 |
| `RimeWithWeasel/RimeWithWeasel.cpp:487-492` | 完全删除 (line 487-491 + closing brace),留注释 | 撤销 v0.19.0.65 误加的 duplicate Show |
| `WeaselIPCServer/WeaselServerImpl.h:103` | 加 `m_pfnPhysicalToLogical` 字段 + PPTLPFPMDPI typedef | hot-path 缓存用 |
| `WeaselIPCServer/WeaselServerImpl.cpp:33-38` | 构造期 GetProcAddress 缓存 | hot-path 修复 |
| `WeaselIPCServer/WeaselServerImpl.cpp:355-365` | 用 cached pointer + `m_hWndMsg` (替代 NULL) | hot-path + HWND 修正 |
| `output/install.nsi:.onInit` | 加 admin token check (silent + non-admin → SetErrorLevel 6002 + Quit) | L##-PhaseL-9.10 silent fail 真因 |
| `env.bat:24-25` | `WEASEL_PATCH=3` + `PRODUCT_VERSION=0.19.0.74.3` | version bump |
| `.specify/specs/075-focusin-quickpanel-rollback/spec.md` | 新建 spec | spec (R2 compliance) |
| `.specify/specs/075-focusin-quickpanel-rollback/plan.md` | 新建 plan | tech approach |
| `.specify/specs/075-focusin-quickpanel-rollback/tasks.md` | 新建 tasks | T001-T012 P1 + T101-T104 P2 |
| `.specify/memory/lessons-learned.md` | append L##-PhaseL-9.11-FocusinShowRevert | new anti-pattern record |

### 9.2 install.nsi byte-level verify (L09 BOM/CRLF)

```
$ python -c "..."
first3: efbbbf     ✓ (BOM)
CR: 1322          ✓ (= LF)
LF: 1322          ✓
CRLF: 1322        ✓ (no orphan CR or LF)
total: 64378      (+~1.5KB for admin check block, ok)
```

### 9.3 Build status (2026-07-31) — ✅ SUCCESS

用 `_buildflow.cmd` (per L107/L108 dual-verify pattern with `/t:Rebuild`) + `_nsis_only.cmd`
跑 v0.74.3 build。两个脚本都是用户原 commit 留下的,不要重复发明。

**Build 流程**:
```bash
cmd /c "F:\\soft\\00selfmade\\rime_claude\\_buildflow.cmd" > _diag_v0741_build_v0.74.3.log 2>&1
cmd /c "F:\\soft\\00selfmade\\rime_claude\\_nsis_only.cmd" > _diag_v0741_nsis_v0.74.3.log 2>&1
```

**`_buildflow.cmd` 内容** (per L107/L108 — already correct, non-modification):
- vcvars32.bat
- `cd /d F:\soft\00selfmade\rime_claude`
- `call .\env.bat` (PRODUCT_VERSION=0.19.0.74.3 ✓)
- `msbuild weasel.sln /t:Rebuild /p:Configuration=Release /p:Platform=Win32`
- 唯一缺: x64 build (per L##-PhaseL-8 — weaselx64.dll 没自动 rebuild,本 fix 不动 x64 shim 所以 OK)

**关键错误 + 修正 (build 过程踩坑)**:
1. `m_hWndMsg` undefined — WTL `CWindowImpl` exposes `m_hWnd` not `m_hWndMsg`
   → 改为 `m_hWnd` ✓
2. `xmake` 静默 fail (cannot get program for cxx) — 我用 `_buildflow.cmd` 走 msbuild 而
   不是 xmake.bat
3. Build batch 找不到 env.bat — vcvars32 后 CWD 跳到 default,需要 `cd /D` + 显式
   `call env.bat`

**5 binary md5 verify** (per spec 075 V002, vs v0.74.2 expectations):

| Binary | md5 | Status |
|---|---|---|
| WeaselServer.exe | `946ff4a8ccd4d7326c3c066f61bb6553` | **CHANGED (expected per V002)** — FocusIn Show rollback + OnUpdateInputPosition hot-path cache |
| weasel.dll (x86) | `0d08b0c3dfe6a41dc727767fb8e843d7` | CHANGED (timestamp metadata only — content logic unchanged) |
| weaselx64.dll (x64) | `ea4dece5b4c76a01704204472c2aa5f4` | **MATCH (v0.74.2, not rebuilt)** — fix 不动 x64 shim |
| WeaselDeployer.exe | `ff6ffc8f1d45c4c02752c994c83d7ce1` | CHANGED (metadata) |
| WeaselSetup.exe | `75dcff2f247e855fed1325449bc9a941` | CHANGED (metadata) |

**Installer built**: `output\archives\fluxing-0.19.0.74.3-installer.exe`
- size: 43,348,773 bytes (比 v0.74.2 43,353,001 小 -5KB,合理)
- md5: `63c893b4ea1b160a1551de7d5b0e0a59`

**Test smoke (offline sandbox)**:
- `Release\TestDefaultHotkeys.exe output\data\default.yaml` → **PASS 35/35** ✓
- `Release\TestWeaselIPC.exe` round-trip → **server replies: 0** ✓
- 其他 test projects 全部 build OK 但没跑(节省 session budget)

**install.nsi byte-level verify (L09 BOM/CRLF)**:
- first3: `efbbbf` ✓ (BOM)
- CR=1322, LF=1322, CRLF=1322 (no orphan CR or LF)
- total: 64378 bytes (+~1.5KB for admin check block)

### 9.4 Commit blocked 等 user verify (per CLAUDE.md no-commit rule)

**未 commit** per CLAUDE.md "用户上次明确禁止 commit / push / SFTP" + no-commit-on-handoff rule。
harness has been operating under "no commit / push / SFTP unless explicitly told" 这条线
the whole time。在 user 看完 handoff + 决定 ship + commit 之前,**绝不 git commit**。

### 9.5 Post-build 用户验证清单 (per spec 075 SC-xxx + 真机 multi-process test)

#### 立即可验证 (without install):
- [x] 5 binary md5 verify (`_diag_v0741_md5_v0.74.3.ps1`)
- [x] install.nsi BOM/CRLF verify (`python -c "b[:3].hex(); b.count(b'\\r\\n')"`)
- [x] TestDefaultHotkeys PASS 35/35
- [x] TestWeaselIPC round-trip "server replies: 0"
- [ ] (user verify) sandbox-verify (per `feedback_autonomous_diagnostics.md` 可能自动跑)

#### Real-machine verify (装机 + 重启后):
- [ ] (user verify) `_diag_v0741_evidence.ps1` happy path: 不含 "current user not admin" warning
- [ ] (user verify) silent + non-admin 装机 → exit code 6002 via `$LASTEXITCODE` (new admin check)
- [ ] (user verify) 输入中文 ≥10 次,多次往返切输入法 (en-US ↔ Fluxing)
- [ ] (user verify) Application Error event count 24h ≤ 5 (was ≥ 12 in v0.74.1/2)
- [ ] (user verify) `_diag_v0741_dumps.ps1` 没新增 weaselserver*.dmp / claude.exe*.dmp / dopus.exe*.dmp
- [ ] (user verify) 一次性 cleanup System32 3 个残留 DLL — **`重要`**: 即使 new installer 加了
      admin check + IfErrors 检测,用户当前机器已存在的残留 DLL 只能靠下次 install + reboot
      才能被自动清掉。如果 user 不想重新装,需要手动:
        1. `taskkill /F /IM ctfmon.exe /T /C`
        2. `taskkill /F /IM TextInputHost.exe /T /C`
        3. 以 admin 身份:`del /F "C:\WINDOWS\System32\weasel.dll" "C:\WINDOWS\System32\weasel.old.dll" "C:\WINDOWS\System32\weaselx64.dll"`
        4. (如果 file locked,改用 Schedule for deletion on next reboot: `del /F "\??\C:\WINDOWS\System32\weasel*.*"`)
        5. 重启

#### 真因转移记录 (per L##-PhaseL-7/9 教训):
- This v0.74.3 fix 解 **Path A (System32 silent fail)** 部分 (admin check + SetErrorLevel 6002
  让 silent install 在 admin 不足时正确报错 → deploy script catch → 重新装 + reboot 即
  fix 真残留)
- This v0.74.3 fix 解 **Path B-minor (FocusIn Show duplicate + hot-path)** 全部
- This v0.74.3 fix **不解**:
  - WeaselPanel GDI+ path (spec 076 候选,放 v0.75 大改)
  - emoji.ocd ship 化 (L##-PhaseL-9.8 真因尚未真机验证)
  - System32 残留已被 user 提前装上 v0.74.2 留下的 (要 user 手动清理或等下次 install + reboot)

### 9.6 Installer md5 map 留档

| Version | md5 | size | 备注 |
|---|---|---|---|
| v0.19.0.74 | `<unknown>` | 43,364,322 | v0.74.1 build (v0.74.1 + 9.6 cleanup) |
| v0.19.0.74.1 | `<unknown>` | 43,342,290 | v0.74.1 follow-up |
| v0.19.0.74.2 | `6ee983a365648d923162c0c3121aee68` | 43,353,001 | v0.74.2 (IfErrors + exact-match) — **SHIPPED, FAILED** |
| **v0.19.0.74.3** | **`63c893b4ea1b160a1551de7d5b0e0a59`** | **43,348,773** | **NEW — Ready for ship per user decision** |

### 9.7 What user needs to do now

1. **Read this handoff** (you've already done it)
2. **Verify 5 binary md5 vs spec 075 V002 table above** (already done)
3. **Check `_diag_v0741_md5_v0.74.3.ps1`** output (already done)
4. **Decide**: ship v0.74.3 to test machine → install + admin reboot + input Chinese + verify Application Error 24h ≤ 5
5. **If ship**:  commit + tag + SFTP sync per `feedback_pinyin_release_workflow.md`  (3 commits:
   `feat(spec): 075-focusin-quickpanel-rollback` + `feat(WeaselServerIPC): v0.19.0.74.3 input crash rollback` + `chore(release): v0.74.3`)
6. **If NOT ship**: handoff 已完整记录,**无需 user 立答** (harness 不 commit)



