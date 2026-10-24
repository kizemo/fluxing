# Handoff — Phase K3 T019 Bugfix (Dblclick 不上屏 + 无法重开, 2026-07-22)

> **承接会话**: Phase K3 ship (4 commits at f6200a6) + v0.19.0.54 真机装机验
> **承接 HEAD**: `f6200a6`
> **触发**: user 在 Win11 装机 v0.19.0.54 后, Dblclick fail
> **Installer**: `F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.54-installer.exe`
> **md5**:
>   - WeaselServer.exe        `f15768e37a3d5047ed9a4e51e09ba4c1`
>   - FluxingPhrasesDialog.exe `2a86228ef774cf76369c172f066815ed`
>   - installer               `4d446fffef5f5386811296a7c344ab13`

---

## 1. Background — Phase K3 ship 已落地

### 1.1 已 ship commits (4 at f6200a6)
```
f6200a6 chore(fluxing): v0.19.0.54 T018 ship 装机验 (Option B 验证用 installer)
c465254 docs(memory): Phase K3 out-of-process PhrasesDialog 教训 + 文档同步
cead012 fix(WeaselServer): out-of-process PhrasesDialog INJECT 抢回 user foreground
05a11bd refactor(WeaselServer): out-of-process PhrasesDialog IPC 集成 + 删 in-process
```

### 1.2 已 ship 的核心改动 (T010-T014)
- **WeaselServer/PhrasesDialogIPC.{h,cpp}** (NEW): 服务端 CreateNamedPipe + ConnectNamedPipe
  + worker thread (PipeThreadProc) + CreateProcess 子进程 + YAML 读写 + SendInput
- **FluxingPhrasesDialog/PhrasesDialogIPC.h** + main.cpp + PipeClient.{h,cpp} (NEW): 客户端
  Show() / Hide() + InjectText (本地降级 fallback)
- **WeaselServer/ForegroundCapture.{h,cpp}** (NEW, in T011-B Option B):
  `fluxing::foreground_restore` namespace, CS 锁 + 静态 cache (HWND + DWORD tid)
- **RimeWithWeasel/RimeWithWeasel.cpp** (M, in T011-B): FocusIn 头部调
  `CaptureFromCurrentThread()` — 抓 user foreground
- **WeaselServer/PhrasesDialogIPC.cpp** MT_INJECT case (T011-B):
  AttachThreadInput + SetForegroundWindow + InjectText + detach
- **WeaselServer/PhrasesDialog.cpp+.h** (DEL, T012): ~1820 行全删
- **output/install.nsi** (M, T014): 加 `File "FluxingPhrasesDialog.exe"`

### 1.3 sand
- TestPipeProtocol: 45/45 PASS (PHRASES / ADD / EDIT / DELETE / SHUTDOWN round-trip)
- TestPhrasesDialog (orphan, 测 in-process API): 不在 weasel.sln,不卡 sln build

---

## 2. 装机 user 反馈 (v0.19.0.54 真机, 2026-07-22)

> 引自 user 原话:
> "可以在常用短语UI的输入框输入中文, 点击添加成功。"
> "在常用短语UI, 双击短语列表中的短语, 没有上屏, 且UI消失。消失后无法再通过设置栏点击按钮或快捷键调出。"

### 2.1 4 场景验 — 1 PASS / 3 FAIL

| 场景 | 结果 | 现象 |
|---|---|---|
| **A. 输入 → Add** | ✅ PASS | 短语进列表, 关重开还在 |
| **B. dblclick 短语上屏** | ❌ FAIL | 文本没进 user app (Notepad/Word 等), UI 消失 |
| **C. 快捷键 Alt+. / Alt+/ 重开 dialog** | ❌ FAIL | UI 没出现 (按了没反应) |
| **D. QuickPanel 按钮重开 dialog** | ❌ FAIL | UI 没出现 |

### 2.2 Dblclick + 隐藏 + 无法重开 — 看似 3 个问题, 实际 1 + 1 + 1

3 个 bug 相互关联:
1. **Foreground-ordering 回归 (T011-B Option B 失败)**: dblclick 时 dialog 仍 foreground,
   server SendInput 命中 dialog s_hInput (而非 user app)
2. **Pipe handle 泄漏 (新发现)**: client Hide() → server worker 跳出 → DisconnectNamedPipe 但
   **没 CloseHandle(s_hPipe)** → 下次 CreateNamedPipeW 同名 FAIL → Show() 静默 early return
3. **顺序错位 (Option B 设计假设)**: 即便 Option B foreground 抢回 work, server 端
   SetForegroundWindow 是抢 user foreground, 但 INJECT 期间 dialog 仍 foreground —
   时序上 client Hide() 必须先于 server INJECT 处理

---

## 3. Root cause 分析 (3 bug 静态分析)

### 3.1 Bug 1 — Foreground-ordering (T011-B Option B 失败)

**代码位置**: `WeaselServer/PhrasesDialogIPC.cpp` MT_INJECT case (T011-B 改动)

**问题**: T011-B Option B 设计依赖两个假设:
1. `GetForegroundWindow()` 在 IPC worker thread 调 = 拿 user app 当前 foreground
   (因为 session-global, 不看 caller 进程)
2. `SetForegroundWindow(cached_target)` + `AttachThreadInput` 抢回 user foreground

**实测 fail 原因**:
- 假设 1 ✅ partial: GetForegroundWindow 在 IPC worker thread 拿 user foreground
  (FocusIn IPC 时 user 在 edit field), 但 cached 之后 user 可能已切换窗口
- 假设 2 ❌ fail: Win11 22H2+ foreground 限制极严, 即使 AttachThreadInput +
  SetForegroundWindow 仍可能拒 — 服务端调用不在 foreground process, 即使加了
  AttachThreadInput 也未必能从 cluster 抢占

**真实 root cause**: 跨进程 SendInput 必须 Hide 先于 INJECT (per in-process L100 教训),
靠 SetForegroundWindow 抢 foreground 不靠谱 (Win11 限制)

### 3.2 Bug 2 — Pipe handle 泄漏

**代码位置**: `WeaselServer/PhrasesDialogIPC.cpp` PipeThreadProc 退出路径

```cpp
DWORD WINAPI PhrasesDialogIPC::PipeThreadProc(LPVOID /*param*/) {
  ...
  while (s_running) {
    std::string json = PipeRecv(s_hPipe, 4096);
    if (json.empty()) break;  // client 断 → worker 跳出
    ProcessCommand(hPipe, json);
  }
  // cleanup
  DisconnectNamedPipe(s_hPipe);   // ← 只 Disconnect, 没 CloseHandle!
  s_running = false;
  OutputDebugStringW(L"[PhrasesDialogIPC] Worker thread exiting\n");
  return 0;
}
```

**问题**: Worker 跳出时只 `DisconnectNamedPipe(s_hPipe)`, **没 CloseHandle**.
Windows 命名管道: pipe NAME 在 server handle 存在期间被占用. DisconnectNamedPipe
只结束当前连接, 不释放 pipe name.

**后果**:
- 下次 `PhrasesDialogIPC::Show()` 调 CreateNamedPipeW 同名 → FAIL with
  ERROR_ACCESS_DENIED (or ERROR_PIPE_BUSY)
- Show() 静默 early return, s_running 保持 false, 没有任何可见迹象
- User 按 Alt+. → "没反应", 实际是 Show() 返回但啥也没做

**修复**: Worker exit path 加 `CloseHandle(s_hPipe); s_hPipe = INVALID_HANDLE_VALUE;`

### 3.3 Bug 3 — Client NM_DBLCLK 顺序错位

**代码位置**: `FluxingPhrasesDialog/PhrasesDialog.cpp` NM_DBLCLK case

```cpp
case NM_DBLCLK: {
  LPNMITEMACTIVATE pia = ...;
  if (... && pia->iItem < m_phrases.size()) {
    int idx = pia->iItem;
    if (s_pipeClient && s_pipeClient->IsConnected()) {
      // pipe 模式: 先发 INJECT, 再 Hide
      s_pipeClient->SendMessage(fluxing::BuildINJECT(idx));
      s_pipeClient->ReadMessage();  // 读 ACK
      Hide();
    }
  }
  return 0;
}
```

**问题**: 即使 Option B 完美, client NM_DBLCLK 是 "先发 INJECT, 后 Hide",
但 Hide 是 DestroyWindow — DestroyWindow 后 foreground 才归还给原 app.
server 端 INJECT 处理时 (在 SendMessage 跟 ReadMessage 之间) dialog 仍 foreground,
server 的 SetForegroundWindow 即使 work 也只抢到 original focus, 但 SendInput
时如果 dialog 还 visible, foreground 仍是 dialog (取决于 Windows 版本).

**正确顺序**: Hide 先 → DestroyWindow → foreground 归位 user app → INJECT →
server SendInput 到 user app foreground.

---

## 4. 修法方案 (推荐 Option A + Bug 2 修)

### 4.1 Bug 2 修 — Worker exit path CloseHandle

**必须改**, 因为即使 Bug 1 / Bug 3 修好, pipe 泄漏仍让 Show() 静默 fail, 体现为
"按 Alt+. 无反应". 这跟 user 反馈的"无法重开"直接对应。

```cpp
// WeaselServer/PhrasesDialogIPC.cpp PipeThreadProc
  // cleanup (现在)
  DisconnectNamedPipe(s_hPipe);
  s_running = false;
  OutputDebugStringW(L"...\n");
  return 0;

// 改成:
  // cleanup
  DisconnectNamedPipe(s_hPipe);
  CloseHandle(s_hPipe);                   // ← NEW
  s_hPipe = INVALID_HANDLE_VALUE;          // ← NEW
  s_running = false;
  OutputDebugStringW(L"...\n");
  return 0;
```

### 4.2 Bug 3 修 — Client NM_DBLCLK Hide 先于 INJECT

```cpp
// FluxingPhrasesDialog/PhrasesDialog.cpp NM_DBLCLK case (当前)
if (s_pipeClient && s_pipeClient->IsConnected()) {
  s_pipeClient->SendMessage(fluxing::BuildINJECT(idx));
  s_pipeClient->ReadMessage();
  Hide();
}

// 改成:
if (s_pipeClient && s_pipeClient->IsConnected()) {
  // Hide 先 (DestroyWindow 同步归还 foreground) → 再 SendMessage INJECT
  // → server worker 收到时 foreground = user app → SendInput 命中 user app
  // Hide 必须保留 pipe 连接 (不 Disconnect), 让后续 INJECT/ACK 可发
  Hide();
  if (s_pipeClient->IsConnected()) {  // Hide 不 Disconnect → 仍 connected
    s_pipeClient->SendMessage(fluxing::BuildINJECT(idx));
    s_pipeClient->ReadMessage();
  }
}
```

**配套改**: `PhrasesDialog::Hide()` 当前会 Disconnect pipe — 必须改成
"不 Disconnect, 仅 DestroyWindow", 给 NM_DBLCLK 路径用。
但其他 Hide 触发 (Alt+. 第二次按 / Escape / 失去焦点) 仍要 Disconnect.
所以需要:
- 加一个 `HideWithoutDisconnect()` API (NM_DBLCLK 用)
- 原 `Hide()` 不变 (其他路径用)

### 4.3 Bug 1 修 — Option B 可选保留 / 简化

Hide 先于 INJECT 后, foreground 已自动归位 (Win32 standard 行为), Option B 抢
foreground 不再必要。**可保留作为防御性 fallback**, 但简化:
- 不再依赖 GetForegroundWindow + AttachThreadInput
- 仅: InjectText 走 (foreground 已是 user app)

如果保留 Option B, 加防御性 read PHRASES_INIT 字段 (client 在 Hide 前 SendMessage
一条 PHRASES_INIT 报当前 HWND+TID), 但 IPC 协议变更成本高. 简化方案:
- Option B 仅作 fallback: 如果 cached target 还在且 IsWindow → SetForegroundWindow
  (best effort); 否则直接 InjectText

### 4.4 IPC 协议可选增强 (为防御)

如果 user 反馈 Option B 也不稳 (Win11 22H2+ 不同 machine 表现不同), 长期方案:
- Client 在每次 Show 入口 SendMessage PHRASES_INIT (带当前 foreground HWND + TID)
- Server 收 PHRASES_INIT → cache 到 fluxing::foreground_restore (覆盖 FocusIn 抓的)
- INJECT 时用最近一次 PHRASES_INIT (而非 FocusIn)

但 v0.19.0.55 先上 Option A + Bug 2 修, PHRASES_INIT 协议增强看后续 user 反馈.

---

## 5. 调查计划 (建议下一步会话执行)

1. **跑 TestPipeProtocol** — 确认 pipe 协议层无回归 (45/45 PASS)
2. **加 DebugView log** — PhrasesDialogIPC MT_INJECT handler 加详细 log:
   - target = ... (cached hwnd 值)
   - target IsWindow = ...
   - SetForegroundWindow 返回值
   - AttachThreadInput 返回值
   - SendInput 调用结果
   这样装机端 DebugView 能看到 Option B 是否被调用
3. **静态分析确认 Bug 2** — 已完成 (本文 §3.2)
4. **静态分析确认 Bug 3** — 已完成 (本文 §3.3)
5. **实施修法 §4**:
   - 先改 Bug 2 (worker exit CloseHandle) — 1 行改动, 0 风险
   - 再改 Bug 3 (Hide 先于 INJECT + HideWithoutDisconnect API)
   - 简化/保留 Option B (Bug 1)
6. **重 build + 真机验**:
   - env.bat bump v0.19.0.55
   - 跑 xbuild installer → fluxing-0.19.0.55-installer.exe
   - copy 到 release/
   - user 真机重装 + 4 场景验

---

## 6. 验法 (T019 真机)

```powershell
# 装机 (推荐重启先, 避免 PPL 锁)
shutdown /r /t 0
& "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.55-installer.exe" /S /D=D:\Program Files\Fluxing
Start-Sleep -Seconds 30
powershell -ExecutionPolicy Bypass -File "F:\soft\00selfmade\rime_claude\_check_install_v2.ps1"

# 4 场景端到端验:
# A. 输入 "Option A 验证" → Add → 列表有 → 关闭 → 重开 → 还在
# B. Notepad 焦点 → Alt+. → 弹 dialog → dblclick "Option A 验证" → 进 Notepad
# C. Word 焦点 → Alt+/ → 弹 dialog → dblclick "Option A 验证" → 进 Word
# D. QuickPanel 按钮 → dblclick → 进原 app
# 期望: 4 场景全 PASS

# E. 重复 dblclick 多次, 验 "无法重开" 已修 (Bug 2)
#    dblclick → 关闭 → 再按 Alt+. → 弹 dialog → dblclick → 关闭 → 再按 Alt+. → ...
#    反复 5 次, 每次都能重开
```

---

## 7. 文件索引 (新会话 agent 按需读)

| 文件 | 用途 |
|---|---|
| `WeaselServer/PhrasesDialogIPC.cpp` | 修 §4.1 (worker exit CloseHandle) + §4.3 (Option B 简化) |
| `FluxingPhrasesDialog/PhrasesDialog.cpp` | 修 §4.2 (Hide 先于 INJECT) + §4.2 (HideWithoutDisconnect API) |
| `FluxingPhrasesDialog/PhrasesDialog.h` | 加 `HideWithoutDisconnect()` 声明 |
| `env.bat` (gitignored) | bump v0.19.0.54→55 |
| `build_v054.ps1` | 复制改 `_build_v055_*.cmd`, 调 xbuild |
| `output/install.nsi` | 已含 FluxingPhrasesDialog.exe (T014 改), 不动 |
| `_check_install_v2.ps1` | 期望 md5 更新到 v0.19.0.55 |
| `.specify/memory/lessons-learned.md` | 加 L105 (本 bugfix 教训) |

---

## 8. 已知风险 / 不动项

- **不动**: `output/install.nsi` (T014 改的 FluxingPhrasesDialog.exe File 已含)
- **不动**: `WeaselServer/PhrasesDialogIPC.h` API 签名 (除非加新 API)
- **不动**: `RimeWithWeasel/RimeWithWeasel.cpp` (T011-B 改 FocusIn capture, OK 保留)
- **不动**: 4 commits (refactor / fix / docs / chore), 它们是 phase K3 ship
  record, 本 bugfix 是 K3 ship 后的 hotfix, 应是单独 1-2 个 commits

---

## 9. Commit 计划 (T019)

推荐 2 commits:
1. `fix(WeaselServer): PhrasesDialogIPC pipe handle 泄漏 + NM_DBLCLK Hide 先于 INJECT`
   - Bug 2 修 (worker exit CloseHandle)
   - Bug 3 修 (Hide 先于 INJECT + HideWithoutDisconnect API)
   - Bug 1 简化 (Option B 保留作 best-effort fallback, 不依赖 cached HWND)
2. `chore(fluxing): v0.19.0.55 T019 hotfix installer`
   - env.bat bump
   - build_v055.ps1
   - _check_install_v2.ps1 md5 sync
   - handoff update
   - lessons-learned L105

— END OF T019 HANDOFF —