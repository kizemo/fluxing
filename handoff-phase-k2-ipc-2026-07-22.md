# Handoff — Phase K2: Named Pipe IPC (2026-07-22)

> **承接会话**: S-2026-07-22-03-phase-k-spec-init
> **承接 HEAD**: 工作树未 commit（FluxingPhrasesDialog 新项目 + weasel.sln 修改）
> **不变量**: FluxingPhrasesDialog.exe 独立编译通过，5 条硬编码测试短语，零 ATL/WTL/rime 依赖

---

## 1. 已完成 (T001-T004)

### 1.1 T001 新建 vcxproj

`FluxingPhrasesDialog/FluxingPhrasesDialog.vcxproj` — GUID `{D4E8F2A6-1B3C-4E5F-8A9B-7C6D5E4F3021}`
- 配置: Debug/Release × Win32/x64
- 导入: `..\weasel.props` (跟 WeaselServer 一致)
- 链接: gdi32 + user32 + comctl32 + shell32 + msimg32（纯 Win32，零 ATL/WTL/rime）
- 输出: `$(SolutionDir)output\FluxingPhrasesDialog.exe` (x64) / `$(SolutionDir)output\Win32\FluxingPhrasesDialog.exe` (Win32)

`weasel.sln` — 新增项目行 + 8 条配置条目（Debug/Release × Win32/x64）

### 1.2 T002 main.cpp

`FluxingPhrasesDialog/main.cpp` — WinMain 入口：
- InitCommonControlsEx(ICC_LISTVIEW_CLASSES)
- PhrasesDialog::Show()
- GetMessage/DispatchMessage 消息循环

### 1.3 T003 PhrasesDialog.cpp 搬运

从 `WeaselServer/PhrasesDialog.cpp` 复制到 `FluxingPhrasesDialog/`，做了 3 处关键修改：

**修改 1 — 删除 yaml 加载，替换为硬编码测试数据**（`Show()` 函数内）：
```cpp
m_phrases.clear();
m_phrases.push_back({L"你好", L""});
m_phrases.push_back({L"世界", L""});
m_phrases.push_back({L"测试", L""});
m_phrases.push_back({L"常用", L""});
m_phrases.push_back({L"短语", L""});
```

**修改 2 — LoadPhrases/SavePhrases 改为 no-op stub**：
```cpp
bool PhrasesDialog::LoadPhrases(...) { (void)path; (void)out; return false; }
bool PhrasesDialog::SavePhrases(...) { (void)path; (void)data; return false; }
```

**修改 3 — 删除 IMM32 fallback 代码**（原 v0.19.0.49 的 `ImmGetContext`/`ImmAssociateContext`/`ImmSetOpenStatus` 调用）：
理由：独立进程不运行在 TSF shim 环境下，系统自动提供 IMM32 IME context，不需要手动干预。

### 1.4 其他文件

| 文件 | 来源 | 修改 |
|---|---|---|
| `PhrasesDialog.h` | WeaselServer 原文件 | 无修改（直接复制） |
| `ModalChrome.h` | WeaselServer 原文件 | 无修改（直接复制） |
| `ModalChrome.cpp` | WeaselServer 原文件 | 无修改（直接复制） |
| `stdafx.h` | 新建 | 最小化（无 ATL/WTL），仅 windows.h + commctrl.h + shellapi.h |
| `stdafx.cpp` | 新建 | `#include "stdafx.h"` |

### 1.5 编译结果

```bash
MSBuild.exe weasel.sln -t:FluxingPhrasesDialog -p:Configuration=Release -p:Platform=x64
→ output/FluxingPhrasesDialog.exe  1,377,792 bytes  PE32+ x64 GUI
→ 0 errors, 7 warnings (C4312 reinterpret_cast HMENU + C4002 ListView_SetInsertMark)
```

---

## 2. 下一步 (T005-T009, Phase K2)

> 目标：给独立 exe 加上 named pipe IPC，实现 WeaselServer ↔ FluxingPhrasesDialog 双向通信

### T005: IPC 数据结构 (`include/FluxingPipeProtocol.h`)

定义 pipe 协议常量 + 消息格式：

```cpp
// Pipe names
#define PIPE_DATA_NAME  L"\\\\.\\pipe\\FluxingPhrasesDialog\\data"
#define PIPE_CMD_NAME   L"\\\\.\\pipe\\FluxingPhrasesDialog\\cmd"

// 消息类型 (JSON over pipe)
// Client → Server:
//   ADD    {"text":"新短语","category":""}
//   EDIT   {"id":3,"text":"修改后"}
//   DELETE {"id":3}
//   INJECT {"id":3}
//   RELOAD
//   SHUTDOWN
// Server → Client:
//   ACK    {"status":"ok"}
//   ERR    {"msg":"..."}
//   PHRASES {"phrases":[{"text":"...","category":"..."},...]}
//   SHUTDOWN
```

方案选择：因为 payload < 10KB（短语 < 100 条），用 **简单 UTF-8 JSON text over pipe**，不用二进制序列化。不需要引入 nlohmann/json — 手动拼 JSON 字符串即可（JSON 结构极简单）。

### T006: Pipe 客户端 (`FluxingPhrasesDialog/PipeClient.h/.cpp`)

核心 API：
```cpp
class PipeClient {
  bool Connect();                       // CreateFile + SetNamedPipeHandleState(PIPE_READMODE_MESSAGE)
  void Disconnect();                    // CloseHandle
  bool SendCommand(const std::string& json);  // WriteFile → ReadFile ACK
  bool ReadPhrases(std::string& json);  // 阻塞读 PHRASES 消息
  HANDLE m_hPipe;
};
```

关键细节：
- `CreateFile(pipeName, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL)`
- `SetNamedPipeHandleState(hPipe, &mode, NULL, NULL)` with `mode = PIPE_READMODE_MESSAGE`
- 连接超时 `WaitNamedPipe(pipeName, 5000)` (5s)
- 重连逻辑：3 次，间隔 1s

### T007: Pipe 服务端 (`WeaselServer/PhrasesDialogIPC.h/.cpp`)

核心 API：
```cpp
class PhrasesDialogIPC {
  static void Show();             // CreateProcess + pipe 等待
  static void Hide();             // 发 SHUTDOWN
  static void OnAdd(const std::wstring& text);   // pipe 收 ADD → m_phrases push + yaml 写 + 重推
  static void OnInject(int id);                  // pipe 收 INJECT → SendInput
  // 内部
  static DWORD WINAPI PipeThread(LPVOID);        // 独立线程，不阻塞 TSF
  static void SendPhrases(HANDLE hPipe);         // 序列化 m_phrases → JSON → WriteFile
};
```

关键细节：
- `CreateNamedPipe(pipeName, PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT, 1, 4096, 4096, 0, NULL)`
- 独立 worker thread（`CreateThread`），不与 TSF 回调共享线程
- CreateProcess 时通过命令行传 pipe 名称：`FluxingPhrasesDialog.exe --pipe=FluxingPhrasesDialog.12345`
  - 用 PID 后缀区分多实例

### T008: 替换硬编码数据为 pipe 真实数据

在 `FluxingPhrasesDialog/PhrasesDialog.cpp` 的 Show() 中：
1. 解析命令行获取 pipe 名称
2. `PipeClient::Connect()` 连 pipe
3. `PipeClient::ReadPhrases(json)` 收 PHRASES
4. 解析 JSON 填入 `m_phrases`
5. 删掉现有的 5 条硬编码数据

### T009: Sandbox 测试 pipe 通信

手动测试流程：
1. 写临时测试 main 启动 WeaselServer 端的 PipeServer（5 条测试短语）
2. 手动启动 FluxingPhrasesDialog.exe
3. 验证列表显示 5 条短语
4. 验证 Add → pipe 收到 → 重推全量 → 列表刷新

---

## 3. Working tree 未 commit 文件

```
?? FluxingPhrasesDialog/                  ← 新增目录（全部 untracked）
 M weasel.sln                              ← 新增项目引用
 M task.md                                 ← Phase K 进度更新
?? memory/2026-07-22.md
?? memory/2026-07-22-phase-k-bug-a-report.md
?? .specify/specs/051-out-of-process-phrases-dialog/  ← spec 三件套
```

当前 HEAD = `1c85b47` (v0.19.0.49-fluxing installer)。

---

## 4. 不变量

- `WeaselServer/PhrasesDialog.cpp` 原件**未修改**（仍在原位，WeaselServer.exe 仍可独立构建）
- `env.bat` / `weasel.props` 未变
- 现有 TestPhrasesDialog 186/186 PASS（尚未适配新架构）
- 装机路径未变（D:\Program Files\fluxing\weasel）

---

## 5. 关键文件索引

| 文件 | 用途 |
|---|---|
| `.specify/specs/051-out-of-process-phrases-dialog/plan.md` | 完整技术方案（架构图 + IPC 协议设计） |
| `.specify/specs/051-out-of-process-phrases-dialog/tasks.md` | T005-T009 详细步骤 |
| `FluxingPhrasesDialog/FluxingPhrasesDialog.vcxproj` | MSBuild 项目 |
| `FluxingPhrasesDialog/PhrasesDialog.cpp` | 核心 UI（待接 pipe） |
| `WeaselServer/PhrasesDialog.cpp` | 原件（供参考，后续 T010-T012 会删） |
| `include/WeaselIPCData.h` | 参考现有的 IPC 数据结构风格 |
| `WeaselServer/WeaselServerApp.cpp` | `PhrasesDialog::Show()` 调用处（Phase K3 改为 IPC） |

---

## 6. 一句话总结

> Phase K1 (v0.19.0.50) 完成：FluxingPhrasesDialog.exe 独立编译通过，硬编码 5 条测试数据显示 UI。下一步 Phase K2 (v0.19.0.51)：新建 named pipe IPC 数据通道 + 指令通道，替换硬编码数据为真实 pipe 通信，sandbox 测试验证双向数据流。

— END OF HANDOFF —
