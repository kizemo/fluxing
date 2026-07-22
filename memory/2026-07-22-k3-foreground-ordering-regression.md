---
name: K3-phase-foreground-ordering-regression
description: Phase K3 named-pipe IPC 引入 SendInput 顺序回归; Option B 落: AttachThreadInput + SetForegroundWindow,Fix in v0.19.0.52
type: project
---

Phase K3 (2026-07-22) — out-of-process PhrasesDialog 集成时发现的回归 + Option B 修复落地。

## 问题

原 in-process PhrasesDialog.cpp NM_DBLCLK handler 顺序: `Hide() → InjectText()` (TestPhrasesDialog.cpp:686 L100 教训)。
原理: `DestroyWindow(s_hwnd)` 后 Windows 归还 foreground 给原 app → `SendInput` 命中用户 app。

out-of-process IPC 版 (`FluxingPhrasesDialog/PhrasesDialog.cpp:954-958`) 顺序:
```
SendMessage(BuildINJECT(idx));   // server 立即收 → SendInput
ReadMessage();  // 读 ACK
Hide();  // dialog 销毁 (太晚!)
```
server (`WeaselServer/PhrasesDialogIPC.cpp:225-231`) 在收到 INJECT 后立刻调 `InjectText(s_phrases[msg.id].text)` → `SendInput`。**此时 dialog (FluxingPhrasesDialog.exe) 仍是 foreground** — client 还没 Hide。

**结果**: SendInput 的 KEYEVENTF_UNICODE 事件发到 FluxingPhrasesDialog 的输入框 (s_hInput),而不是用户的 Notepad/Word 等。

## Option B 修复 (v0.19.0.52 已落)

**核心三件套**:
1. `WeaselServer/ForegroundCapture.{h,cpp}` — `fluxing::foreground_restore::{CaptureFromCurrentThread, GetHwnd, GetThreadId}`,静态 cache (CS 锁)。
2. `RimeWithWeasel/RimeWithWeasel.cpp:382-385` — `FocusIn` IPC handler 头部调 `CaptureFromCurrentThread()`。
   关键洞察: `GetForegroundWindow()` 是 session-global API (跟 caller 进程无关)。
   在 WeaselServer.exe IPC worker thread 调 = 拿 user app 当前 foreground (= 用户在 edit field 输入 → TSF OnSetFocus → WeaselTSF.dll 发 FocusIn IPC → server FocusIn handler 跑 → 此时 system foreground = user app)。
3. `WeaselServer/PhrasesDialogIPC.cpp` `ProcessCommand MT_INJECT`:
   ```cpp
   HWND target = GetHwnd();
   DWORD targetTid = GetThreadId();
   DWORD currentTid = GetCurrentThreadId();
   bool attached = false;
   if (target && IsWindow(target) && targetTid && targetTid != currentTid) {
     AttachThreadInput(targetTid, currentTid, TRUE) → attached = true;
   }
   SetForegroundWindow(target);
   InjectText(text);
   if (attached) AttachThreadInput(targetTid, currentTid, FALSE);
   ```
   AttachThreadInput 把 worker thread 加入 user app thread 的 input cluster,
   绕过 Windows 的 foreground-process 限制,SetForegroundWindow 现在能指向 user app,
   SendInput 派发到 user app 的 input queue (而非 dialog 的 s_hInput)。

**Why AttachThreadInput works**: 这是 Microsoft 文档化的 Windows 标准 IME 模式
("Hooks and the GUI", in MSDN)。当 thread A 通过 AttachThreadInput 进入 thread B
的 input cluster,thread A 获得 thread B 的 foreground-process 权限,
可调 SetForegroundWindow 指到 B 所在的 process 任何窗口。

**How to apply**:
- 任何 out-of-process IME / macro recorder / autotype 在做 SendInput 跟 "user 当前 app"
  不在同 process 时,套用 AttachThreadInput + SetForegroundWindow 模式。
- "user 当前 foreground" 的捕获点: TSF OnSetFocus / IMM OnFocus / user 主动调热键前,
  任何能保证 "user 仍在 edit field" 的 TSF callback 即可。

## 已知限制 (V0.19.0.52 → 用户反馈后再迭代)

1. **Cached HWND 滞后**: 若用户 FocusIn → Alt+Tab 到非 edit window → 按 Alt+. →
   dblclick 短语:注入去 cached target (last edit field),不是 user 当前窗口。
   Mitigations (out of scope): CBT hook (HCBT_ACTIVATE), WH_CALLWNDPROCRET, 
   SetWinEventHook(EVENT_SYSTEM_FOREGROUND) (需 DLL-into-all-process) — 都是大动静。
2. **SetForegroundWindow 仍可能失败**: 在 Win11 22H2+ foreground 限制很严,
   即使 AttachThreadInput + SetForegroundWindow 可能仍被拒。
   可改进: 用 `AllowSetForegroundWindow(targetPid)` + `SetForegroundWindow`,
   需要 capture 时多拿 targetPid (foreground process id);或者 fallback
   给 user 发 WM_COPYDATA 触发 user 进程自己 SendInput。
3. **Same-user app 拦截**: 同 user 的恶意 app 可通过 SetWinEventHook 监听 foreground,
   也可 ATTACHTHREADINPUT 到我们捕获的 thread — 当前 dialog 暴露 named pipe
   (无 DACL 收紧,SUGGESTION 已记)。

## 副产物 — BLOCKER #2 兜底

`PhrasesDialogIPC::Show()` 内部惰性初始化 yaml path:
```cpp
if (s_yamlPath.empty()) {
  s_yamlPath = WeaselUserDataPath().wstring() + L"\\phrases.yaml";
}
```
消除 "hotkey fires before SetYamlPath" 理论 race;同时允许 TestPhrasesDialog 不
依赖外部 SetYamlPath 也能工作。WeaselServerApp.cpp 里 Run() 显式 SetYamlPath 保留
作为 override。
