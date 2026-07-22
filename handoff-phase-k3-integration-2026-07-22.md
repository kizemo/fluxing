# Handoff — Phase K3: 集成 + 清理 (2026-07-22)

> **承接会话**: Phase K2 named pipe IPC
> **承接 HEAD**: 工作树未 commit，HEAD = `1c85b47` (v0.19.0.49-fluxing installer)
> **不变量**: FluxingPhrasesDialog.exe + WeaselServer.exe 编译 0 error；pipe IPC 45/45 sandbox PASS

---

## 1. Phase K2 已完成 (T005-T009, v0.19.0.51)

### 新增文件 (直接读取即可)

| 文件 | 用途 |
|---|---|
| `include/FluxingPipeProtocol.h` | IPC 协议：`PipeMsgType` 枚举 (`MT_` 前缀)、`BuildPHRASES/ADD/EDIT/DELETE/INJECT/ACK/ERR/SHUTDOWN`、`ParseMessage`、`PipeSend/PipeRecv`、`MakePipeName(pid)` |
| `FluxingPhrasesDialog/PipeClient.h` | `fluxing::PipeClient` — Connect/Disconnect/IsConnected/SendMessage/ReadMessage |
| `FluxingPhrasesDialog/PipeClient.cpp` | 实现：重连 3 次 × 1s 间隔，5s WaitNamedPipe 超时，消息模式 |
| `WeaselServer/PhrasesDialogIPC.h` | `fluxing::PhrasesDialogIPC` — Show/Hide/IsVisible/SetPhrases/GetPhrases/SetYamlPath/LoadPhrasesFromYaml/SavePhrasesToYaml/InjectText |
| `WeaselServer/PhrasesDialogIPC.cpp` | 实现：CreateNamedPipe + ConnectNamedPipe + worker thread (`PipeThreadProc`) + CreateProcess FluxingPhrasesDialog.exe + YAML 读写 (从原 PhrasesDialog 搬来) + SendInput |
| `test/TestPipeProtocol/` | Sandbox 45/45 PASS |

### 修改文件

| 文件 | 改动摘要 |
|---|---|
| `FluxingPhrasesDialog/main.cpp` | 解析 `--pipe=` 命令行参数，调 `PhrasesDialog::SetPipeName()` |
| `FluxingPhrasesDialog/PhrasesDialog.h` | 加 `s_pipeName`/`s_pipeClient` 字段 + `SetPipeName()`/`GetPipeName()` API + `fluxing::PipeClient` 前向声明 |
| `FluxingPhrasesDialog/PhrasesDialog.cpp` | Show(): pipe 连接收 PHRASES (降级到硬编码)；OnCommand Add/Edit/Delete: pipe 发命令 + 读 PHRASES 响应；Inject: 先发 INJECT 再 Hide；Hide(): Disconnect pipe |
| `WeaselServer/WeaselServer.vcxproj` | 加 `PhrasesDialogIPC.cpp` / `PhrasesDialogIPC.h` |
| `FluxingPhrasesDialog/FluxingPhrasesDialog.vcxproj` | 加 `PipeClient.cpp` / `PipeClient.h` |

### IPC 协议约定

```
Pipe: \\.\pipe\FluxingPhrasesDialog\{pid}  (PID = WeaselServer 进程)

Server → Client:
  PHRASES  {"type":"PHRASES","phrases":[{"text":"...","category":"..."}]}  (初始 + 每次 Add/Edit/Delete 后推送全量)
  ACK      {"type":"ACK"}                    (INJECT 完成后)
  ERR      {"type":"ERR","msg":"..."}

Client → Server:
  ADD      {"type":"ADD","text":"..."}
  EDIT     {"type":"EDIT","id":N,"text":"..."}
  DELETE   {"type":"DELETE","id":N}
  INJECT   {"type":"INJECT","id":N}
  SHUTDOWN {"type":"SHUTDOWN"}

编码: UTF-8 JSON, 每行一条消息，以 \n 结尾
Server 发 PHRASES 替代 ACK 用于数据变更操作 (ADD/EDIT/DELETE)
```

---

## 2. Phase K3 目标 (T010-T013, v0.19.0.52)

### T010: WeaselServerApp 集成 (核心)
- **文件**: `WeaselServer/WeaselServerApp.cpp`
- **改法**: 所有 `PhrasesDialog::Show()` 调用 → `fluxing::PhrasesDialogIPC::Show()`
  - L52: `PhrasesHotkeySubclassProc` (WM_HOTKEY ALT+.)
  - L69: `PhrasesHotkeySubclassProc` (WM_HOTKEY Alt+/)
  - L325: QuickPanel 回调 (onPhrases lambda)
- **初始化**: `WeaselServerApp::Run()` 开头设 yaml 路径 + 加载短语:
  ```cpp
  fluxing::PhrasesDialogIPC::SetYamlPath(yamlPath);
  // yamlPath = WeaselUserDataPath() / "phrases.yaml"  (跟原 PhrasesDialog 一致)
  ```
- **头文件**: `WeaselServerApp.cpp` 加 `#include "PhrasesDialogIPC.h"`
- **关键**: T010 改完后，WeaselServer.exe 调用 Show() 时会 CreateProcess FluxingPhrasesDialog.exe → pipe 连 → 送 PHRASES → 显示 UI

### T011: InjectText 集成验证
- 理论上 T007 已实现 (PhrasesDialogIPC::InjectText 用 SendInput)
- 验证路径: 双击 ListView → client 发 INJECT → server 收 → SendInput → text 注入
- 如果 SendInput 从 WeaselServer.exe (TSF shim 进程) 发出有问题 → 考虑回退到 client 端 SendInput
- **参考**: 原 `PhrasesDialog::DefaultInject` (SendInput with KEYEVENTF_UNICODE pairs)

### T012: WeaselServer/PhrasesDialog.cpp 删除
- 删 `WeaselServer/PhrasesDialog.cpp` + `WeaselServer/PhrasesDialog.h`
- 从 `WeaselServer.vcxproj` 移除引用
- 确保编译通过 (无残留引用)
- **检查**: `grep -r "PhrasesDialog::" WeaselServer/` 应只剩 WeaselServerApp.cpp 的几条调用 (已改为 PhrasesDialogIPC)

### T013: 确认 ModalChrome 共享 (已 done)
- T003 已复制 ModalChrome.h/.cpp 到 FluxingPhrasesDialog/
- 不需要额外改动

---

## 3. 关键上下文

### 当前调用点 (需改的 3 处)

```cpp
// 1. WeaselServerApp.cpp:52 — ALT+. hotkey
PhrasesDialog::Show();

// 2. WeaselServerApp.cpp:69 — Alt+/ hotkey
PhrasesDialog::Show();

// 3. WeaselServerApp.cpp:325 — QuickPanel button 1 click
PhrasesDialog::Show();
```

全改为: `fluxing::PhrasesDialogIPC::Show();`

### 原 yaml 路径

原 `PhrasesDialog::s_yamlPath` 默认值在哪里设的？在 WeaselServerApp 调用 Show 之前：

```cpp
// 原 PhrasesDialog.cpp Show() 的构造逻辑：
// 如果 s_yamlPath 为空，自动用 WeaselUserDataPath() + L"\\phrases.yaml"
```

需要在 `PhrasesDialogIPC` 中处理同样逻辑，或者在 Run() 时显式 set yaml path。

### IPC 类名/命名空间

- `fluxing::PipeClient` — 客户端 (FluxingPhrasesDialog.exe 端)
- `fluxing::PhrasesDialogIPC` — 服务端 (WeaselServer.exe 端)
- `fluxing::PipePhrase` — 短语数据 (`{text, category}`)
- `fluxing::PipeMessage` — 消息体
- `fluxing::PipeMsgType` — 消息类型枚举 (`MT_PHRASES`, `MT_ADD`, etc.)
- `fluxing::MakePipeName(pid)` — 生成 pipe 路径
- `fluxing::BuildPHRASES/ADD/EDIT/DELETE/INJECT/ACK/ERR/SHUTDOWN` — 序列化
- `fluxing::ParseMessage(utf8)` — 反序列化
- `fluxing::PipeSend/PipeRecv` — I/O

### PhrasesDialogIPC 内部状态

```cpp
static HANDLE s_hPipe;            // pipe handle
static HANDLE s_hThread;          // worker thread
static HANDLE s_hProcess;         // FluxingPhrasesDialog.exe process
static DWORD s_pid;               // 本进程 PID (pipe name 后缀)
static volatile bool s_running;   // worker thread 运行中
static std::vector<PipePhrase> s_phrases;  // 内存短语数据
static std::wstring s_yamlPath;   // YAML 文件路径
```

### 当前残留引用 (T012 删前必须清)

```
WeaselServerApp.cpp: #include "PhrasesDialog.h"
WeaselServerApp.cpp: PhrasesDialog::Show()   ×3
WeaselServer.vcxproj: PhrasesDialog.cpp       (ClCompile)
WeaselServer.vcxproj: PhrasesDialog.h?        (ClInclude — 目前没在 vcxproj 里，但在 include 目录作为头文件)
```

### 重要约束

1. **不阻塞 TSF**: PhrasesDialogIPC 用独立 worker thread，不会在 TSF 回调中做 I/O
2. **WeaselServer.exe 的 stdafx.h** 含 ATL/WTL/CAppModule，PhrasesDialogIPC.cpp 用 `#include "stdafx.h"`
3. **FluxingPhrasesDialog.exe 的 stdafx.h** 无 ATL/WTL，只有 Win32 + commctrl
4. **ENABLE_RELEASE_BUILD=1** or `RELEASE_BUILD=1` in env.bat
5. **Commit 规范**: 模块前缀 `WeaselServer` / `fluxing`

---

## 4. 编译命令

```bash
# FluxingPhrasesDialog x64
"/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/MSBuild.exe" \
  "F:/soft/00selfmade/rime_claude/weasel.sln" \
  -t:FluxingPhrasesDialog -p:Configuration=Release -p:Platform=x64 -verbosity:minimal

# WeaselServer Win32
"/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/MSBuild.exe" \
  "F:/soft/00selfmade/rime_claude/weasel.sln" \
  -t:WeaselServer -p:Configuration=Release -p:Platform=Win32 -verbosity:minimal

# 关进程 (编译前)
cmd.exe //c "taskkill /F /IM WeaselServer.exe & taskkill /F /IM FluxingPhrasesDialog.exe & exit"
```

---

## 5. 一句话总结

> Phase K2 已完成 named pipe IPC 协议 + 客户端 + 服务端 + sandbox 测试 (45/45 PASS)。
> Phase K3 只需把 WeaselServerApp 的 3 处 `PhrasesDialog::Show()` 改为 `fluxing::PhrasesDialogIPC::Show()`，
> 设好 yaml 路径，编译通过，然后删掉旧的 in-process PhrasesDialog.cpp/.h。

— END OF HANDOFF —
