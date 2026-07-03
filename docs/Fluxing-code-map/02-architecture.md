# 02 · 架构与运行期数据流

## 2.1 进程与组件图

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              Windows 系统                                    │
│  ┌──────────┐  按键事件 (TSF)   ┌─────────────────────────┐                 │
│  │ Focus App├──────────────────►│ TSF 文本服务             │                 │
│  │ (notepad, ├──────────────────►│ weasel*.dll (WeaselTSF) │                 │
│  │  explorer │   会话/编辑上下文 │  - ITfKeyEventSink      │                 │
│  │  ...)     │◄─────────────────┤  - ITfTextEditSink      │                 │
│  └──────────┘   提交文本        │  - ITfCompositionSink   │                 │
│       ▲                         │  - ITfThreadFocusSink   │                 │
│       │ ImmGetCompositionString │  - LanguageBar          │                 │
│       │ (内联预编辑由 TSF 完成)  │  - CandidateList         │                 │
│       │                         └──────────┬──────────────┘                 │
│       │                                    │ 命名管道（per-user pipe）     │
│       │                                    │ \\.\pipe\<username>\...       │
│       │                                    ▼                                │
│       │                         ┌──────────────────────────┐                │
│       │                         │ 后台服务 WeaselServer.exe │                │
│       │                         │  - weasel::Server (HWND)  │                │
│       │                         │  - weasel::UI (面板宿主)  │                │
│       │                         │  - RimeWithWeaselHandler  │                │
│       │                         │  - WeaselTrayIcon         │                │
│       │                         │  - WinSparkle             │                │
│       │                         └─────┬────────┬────────────┘                │
│       │                               │        │                            │
│       │                               │ C API  │ 共享内存                    │
│       │                               ▼        ▼                            │
│       │                         ┌──────────────────────────┐                │
│       │                         │ librime (rime_api)        │                │
│       │                         │  - rime_get_api()         │                │
│       │                         │  - setup / initialize     │                │
│       │                         │  - process_key / commit   │                │
│       │                         └──────────┬───────────────┘                │
│       │                                    │                                │
│       │                                    ▼                                │
│       │                         ┌──────────────────────────┐                │
│       │                         │ 用户数据目录              │                │
│       │                         │  %AppData%\Rime           │                │
│       │                         │   *.yaml  *.txt  *.bin    │                │
│       │                         └──────────────────────────┘                │
│       │                                                                  │
│       │   ┌──────────────────────────────────────────────────────┐       │
│       └───┤ UI 面板（WS_POPUP / WS_EX_LAYERED / WS_EX_NOACTIVATE）│       │
│           │  - WeaselPanel::DoPaint()                             │       │
│           │  - D2D/DirectWrite 渲染 + Gdiplus 圆角 / 模糊         │       │
│           │  - 在焦点 APP 的 caret 附近定位                        │       │
│           └──────────────────────────────────────────────────────┘       │
│                                                                          │
│   ┌────────────────┐  ShellExecuteW   ┌──────────────────────────┐        │
│   │ WeaselTrayIcon ├────────────────►│ WeaselDeployer.exe        │        │
│   │  菜单          │                 │  /deploy | /dict | /sync  │        │
│   │  打开 / 关闭    │                 │  /install                 │        │
│   └────────────────┘                 └──────────────────────────┘        │
│                                                                          │
│   ┌────────────────┐  WinSparkle      ┌──────────────────────────┐        │
│   │ WinSparkle      ├────────────────►│ update/appcast.xml        │        │
│   └────────────────┘                 └──────────────────────────┘        │
└─────────────────────────────────────────────────────────────────────────────┘
```

## 2.2 启动时序

### 2.2.1 WeaselServer 启动路径

1. 用户首次切到 Weasel IME（`0404:`/`0804:` layout 字符串）→ Windows 加载 `weasel*.dll`。
2. `WeaselTSF::ActivateEx`：
   - 初始化 ThreadMgrEventSink / TextEditSink / KeyEventSink / PreservedKey / LanguageBar / Compartment / ThreadFocusSink。
   - 调 `m_client.Echo()` 检测服务是否在跑。
   - 若不在跑：`_EnsureServerConnected()` → `_Reconnect()` 失败 6 次后 `ShellExecuteW(L"start_service.bat")`（后台线程，500ms 后再连）。
3. `WeaselServer.exe`（`WeaselServer.cpp::_tWinMain`）：
   - 检查系统 ≥ Windows 8.1（`IsWindowsBlueOrLaterEx`），设置 `SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE)`，`ImmDisableIME(-1)`，拒绝 SYSTEM 用户。
   - 若有 `/q` 旧实例，关掉。
   - 构造 `WeaselServerApp`：内部 `m_handler(RimeWithWeaselHandler(&m_ui))` + `m_server(SetRequestHandler(m_handler))`。
   - `app.Run()`：
     - `m_server.Start()` 创建 IPC 窗口（`WeaselIPCWindow_1.0`）并启动 pipe 监听线程。
     - 初始化 WinSparkle（appcast、registry path、lang）。
     - `m_ui.Create(server.GetHWnd())` 创建 UI 面板。
     - `m_handler->Initialize()`：rime_api 初始化、读 `weasel.yaml` 样式、读 `show_notifications_time`、`global_ascii`、`m_app_options`、dark mode 分支。
     - `tray_icon.Create + Refresh`。
     - `m_server.Run()` 消息循环。
4. 关闭：`Stop()` → `Finalize()` → `m_ui.Destroy()` → `tray_icon.RemoveIcon()` → `win_sparkle_cleanup()`。

### 2.2.2 TSF 会话/按键

- 用户按键 → `WeaselTSF::OnTestKeyDown` → `m_client.ProcessKeyEvent(keyEvent)` → 服务端 `ServerImpl::OnKeyEvent` → `m_pRequestHandler->ProcessKeyEvent()`（=`RimeWithWeaselHandler`）→ `rime_api->process_key` → `rime_api->get_context` / `get_status` → 写回 `m_session_status_map`。
- 服务端构造响应文本 → `ClientImpl::GetResponseData` 触发 `ResponseParser` 解析 → 更新 `_cand`（`CCandidateList`）与 `_status`。
- `_UpdateLanguageBar`、`_cand->Show`、`UI::Update` → 重绘 `WeaselPanel`。

## 2.3 IPC 协议

### 2.3.1 传输层

- 实现：`include\PipeChannel.h` + `WeaselIPC\PipeChannel.cpp` + `WeaselIPCServer\PipeServer`。
- 命名：`\\.\pipe\<username>...`（per-user，避免权限提升）。
- 消息模式：`PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE`，缓冲区 4 KB。
- Server 端：`PIPE_UNLIMITED_INSTANCES`；`SA` 由 `WeaselIPCServer\SecurityAttribute.cpp` 提供（限定到当前用户 SID）。
- 客户端使用同步 transact：写 `PipeMessage{Msg,wParam,lParam}` → 服务端回 `PipeMessage` → 客户端再读响应文本。
- 响应文本放在共享内存：`WEASEL_IPC_SHARED_MEMORY_SIZE = sizeof(PipeMessage) + 4 KB`，buffer 内是 `key=value` 行序列 + `.\n` 终止。

### 2.3.2 命令枚举（`include\WeaselIPC.h`）

```cpp
enum WEASEL_IPC_COMMAND {
  WEASEL_IPC_ECHO = WM_APP + 1,
  WEASEL_IPC_START_SESSION, WEASEL_IPC_END_SESSION,
  WEASEL_IPC_PROCESS_KEY_EVENT, WEASEL_IPC_SHUTDOWN_SERVER,
  WEASEL_IPC_FOCUS_IN, WEASEL_IPC_FOCUS_OUT,
  WEASEL_IPC_UPDATE_INPUT_POS,
  WEASEL_IPC_START_MAINTENANCE, WEASEL_IPC_END_MAINTENANCE,
  WEASEL_IPC_COMMIT_COMPOSITION, WEASEL_IPC_CLEAR_COMPOSITION,
  WEASEL_IPC_TRAY_COMMAND,
  WEASEL_IPC_SELECT_CANDIDATE_ON_CURRENT_PAGE,
  WEASEL_IPC_HIGHLIGHT_CANDIDATE_ON_CURRENT_PAGE,
  WEASEL_IPC_CHANGE_PAGE,
  WEASEL_IPC_LAST_COMMAND
};
```

### 2.3.3 请求体 — `key=value` 文本（仅在部分命令需要时）

```
action=session
session.client_app=notepad.exe
session.client_type=tsf
.
```

- 终止：单行 `.`
- 转义：`\\`, `\n`, `\t`（`WeaselUtility.h::escape_string`）
- 解析：`WeaselIPC\ResponseParser.cpp::Feed` 走 `Deserializer` 字典表（`Deserializer.cpp`）

### 2.3.4 响应体 — `key=value` 文本（"action" = 第一段 key）

常用 action：
- `commit` — `commit=` 单行字符串
- `context` — `context.preedit`, `context.sel_start`, `context.sel_end`, `context.cursor`, `context.attributes`, `context.caret_x/y`，以及候选信息 `context.cinfo.currentPage/totalPages/highlighted/is_last_page/candies/comments/labels`
- `status` — `status.ascii_mode/zhung/schema_id/schema_name/disabled`
- `style` — 完整 `UIStyle`（详见 `05-data-and-config.md`）
- `config` — `config.inline_preedit`, `config.inline_code` 等

`Uis style` 由 `RimeWithWeaselHandler::_UpdateUIStyle` 写入大量键值。客户端由 `ResponseParser` 调用各 `Deserializer::Store` 填充到 `UIStyle`。

### 2.3.5 关键消息路径

| 命令 | Client 入口 | Server 处理 | RequestHandler 回调 |
|---|---|---|---|
| `ECHO` | `ClientImpl::Echo` | `ServerImpl::OnEcho` | 直接回 session_id |
| `START_SESSION` | `ClientImpl::StartSession`（先写 client info） | `OnStartSession` → 解析 `client_app`/`client_type` | `RequestHandler::AddSession` |
| `END_SESSION` | `ClientImpl::EndSession` | `OnEndSession` | `RequestHandler::RemoveSession` |
| `PROCESS_KEY_EVENT` | `ClientImpl::ProcessKeyEvent` | `OnKeyEvent` | `RequestHandler::ProcessKeyEvent` |
| `COMMIT_COMPOSITION` | `ClientImpl::CommitComposition` | `OnCommitComposition` | `RequestHandler::CommitComposition` |
| `CLEAR_COMPOSITION` | `ClientImpl::ClearComposition` | `OnClearComposition` | `RequestHandler::ClearComposition` |
| `UPDATE_INPUT_POS` | `ClientImpl::UpdateInputPosition`（位打包） | `OnUpdateInputPosition` | `RequestHandler::UpdateInputPosition` |
| `FOCUS_IN/OUT` | `ClientImpl::FocusIn/Out` | `OnFocusIn/Out` | `RequestHandler::FocusIn/Out` |
| `TRAY_COMMAND` | `ClientImpl::TrayCommand` | `OnCommand`（按 menu id 分发） | `SetOption` / `StartMaintenance` |
| `SELECT/HIGHLIGHT/PAGE` | `ClientImpl::Select/Highlight/ChangePage` | `OnSelect/Highlight/ChangePage` | `RequestHandler::SelectCandidateOnCurrentPage` / `Highlight` / `ChangePage` |
| `START/END_MAINTENANCE` | `ClientImpl::Start/EndMaintenance` | `OnStart/EndMaintenance` | `RequestHandler::Start/EndMaintenance` |
| `SHUTDOWN_SERVER` | `ClientImpl::ShutdownServer` | `OnShutdownServer` | 直接 `PostQuitMessage` |

## 2.4 状态机

### 2.4.1 服务端全局

```
            ┌──────────────┐ m_disabled=true (Deployer 在跑)        ┌──────────────┐
   start ──►│ DISABLED     ├──────────► 不处理按键，但回应 session ──►│  ACTIVE       │
            └──────┬───────┘                                          │              │
                   │ Deployer 退出后 (EndMaintenance)                 │              │
                   ▼                                                  │              │
            ┌──────────────┐                                          │              │
            │  ACTIVE      │◄──── 正常处理 ──────────────────────────│              │
            │              │                                          └──────┬───────┘
            │              │                                                 │
            │              │ Rime 维护（start_maintenance）                  │
            │              ├──────────────────► MAINTENANCE                  │
            │              │◄──────────────────┤                              │
            │              │  join_maintenance_thread                       │
            └──────┬───────┘                                                 │
                   │ shutdown / 系统关机                                    │
                   ▼                                                        │
              EXIT (PostQuitMessage)                                       │
```

`RimeWithWeaselHandler::m_disabled`、`m_session_status_map<weasel_id, SessionStatus>` 是核心状态。

### 2.4.2 客户端 (TSF)

```
   Activate ──►  Connect(Echo) ──┬─ ok ─► StartSession ──► ACTIVE
                                 │                         │
                                 │                         ├── OnKeyDown/Up ─► ProcessKeyEvent
                                 │                         ├── FocusIn/Out
                                 │                         ├── UpdateInputPosition (光标矩形)
                                 │                         └── Deactivate ──► EndSession
                                 │
                                 └── fail ──► 6 次重试 ─► ShellExecuteW(start_service.bat)
                                                       ──► Echo 成功
```

`_EnsureServerConnected` 的重连 + 拉起服务是关键不变量。

## 2.5 关键依赖图

```
   build.bat / xbuild.bat
        │
        ▼
   boost → librime (with its own deps) → RimeWithWeasel ─┐
        │                                             │
        └─→ xmake / msbuild ──→ WeaselIPC            ─┤
                              WeaselIPCServer        ─┤
                              WeaselUI               ─┤
                              RimeWithWeasel         ─┘
                                                    │
                                                    ▼
                          ┌──────────────┐  ┌──────────────┐  ┌──────────────┐
                          │ WeaselServer │  │ WeaselTSF    │  │ WeaselDeployer│
                          │ (exe)        │  │ (dll, 4 abi) │  │ (exe)        │
                          └──────┬───────┘  └──────┬───────┘  └──────┬───────┘
                                 │                 │                 │
                                 └──── NSIS installer (output\install.nsi) ──►
                                 ┘
                                 WeaselSetup.exe (x86 only, installer/uninstaller)
```

## 2.6 错误处理与降级

- TSF `_EnsureServerConnected`：服务挂了 → 重试 6 次 → 拉起 `start_service.bat` → 等 500ms 重连；不阻塞 UI。
- PipeChannel 抛 DWORD 错误码；`ClientImpl::_SendMessage` catch 后返回 0（"无反应"）。
- Pipe 句柄断开由 `PipeChannelBase::_FinalizePipe` 处理。
- `WeaselServer` 服务名 `WeaselIME`，可在 SCM 视图里看到；崩溃转储路径由 `WeaselSetup\imesetup.cpp::WEASEL_WER_KEY` 配置。
- IPC server 启动失败时 `WeaselServerApp::Run` 整体退出，TSF 那边下次按键会再次尝试。
- `RimeWithWeaselHandler` 通知回调 (`OnNotify`) 把 `deploy_start` / `deploy_success` / `deploy_failure` 翻译成 `m_show_notifications` 列表，UI 通过 `ShowWithTimeout` 浮窗。
