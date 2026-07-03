# 04 · 模块深入

## 4.1 WeaselTSF（TSF 文本服务 / 共享库）

| 文件 | 角色 |
|---|---|
| `WeaselTSF.h/.cpp` | `CWeaselTSF` 主类，实现 `ITfTextInputProcessor(Ex) / ThreadMgrEventSink / TextEditSink / TextLayoutSink / KeyEventSink / CompositionSink / EditSession / ThreadFocusSink / DisplayAttributeProvider` |
| `Register.h/.cpp` | DLL 入口 `DllMain` / `DllRegisterServer`（注册 TSF CLSID、Profile、PreservedKey） |
| `dllmain.cpp` | 真正的 `DllMain`；`DllGetClassObject`、`DllCanUnloadNow`、`DllRegisterServer` 等 |
| `Compartment.h/.cpp` | TSF Compartment 监听（ascii_mode、disable、inline_preedit、show_notifications_time） |
| `CandidateList.h/.cpp` | 候选列表显示相关；与 `_cand` 配合 |
| `DisplayAttributeInfo.h/.cpp`、`DisplayAttributeProvider.cpp`、`EnumDisplayAttributeInfo.h/.cpp` | 文本编辑属性提供（高亮、半角下划线等） |
| `EditSession.h/.cpp` | 同步编辑会话（commit/insert text、set selection） |
| `KeyEvent.cpp` | KeyEvent ↔ ibus Keycode 转换（`KeyEvent.h` 提供） |
| `KeyEventSink.cpp` | `ITfKeyEventSink` 实现：`OnTestKeyDown/Up` → 调 `m_client.ProcessKeyEvent` |
| `TextEditSink.cpp` | `ITfTextEditSink` 实现：跟 focus 变化、提交等 |
| `ThreadMgrEventSink.cpp` | `ITfThreadMgrEventSink` |
| `LanguageBar.h/.cpp` | 语言栏图标 + 菜单 |
| `Server.cpp` | 工具：退出时（关机）发命令给后台 |
| `Globals.h/.cpp` | DLL 全局 ref count + lock |
| `WeaselTSF.rc` | DLL 资源（图标、版本） |
| `WeaselTSF.def` | 导出表（`DllGetClassObject` 等） |
| `ctffunc.h` | TSF 头文件（Windows SDK 没有，仓库自带） |

### 关键状态字段（CWeaselTSF）

```cpp
com_ptr<ITfThreadMgr> _pThreadMgr;
TfClientId _tfClientId;
DWORD _dwThreadMgrEventSinkCookie, _dwTextEditSinkCookie, _dwTextLayoutSinkCookie,
      _dwThreadFocusSinkCookie;
BOOL _fTestKeyDownPending, _fTestKeyUpPending;
CCandidateList* _cand;            // 候选窗口
weasel::Client m_client;          // IPC client
weasel::Status _status;           // 最近一次 IME 状态
```

### 启动时一定要看的不变量

- `_EnsureServerConnected()` 是 `ActivateEx` 的最后一步；它最多重试 6 次（第 7 次拉起 `start_service.bat`）。
- `m_client.ProcessKeyEvent(0)` 用于"focus 时同步一次状态"（`OnSetThreadFocus`）。

## 4.2 WeaselServer（后台服务 / 进程）

| 文件 | 角色 |
|---|---|
| `WeaselServer.cpp` | `_tWinMain`：参数解析（`/q` `/userdir` `/weaseldir` `/ascii` `/nascii` `/update`），创建 `WeaselServerApp` |
| `WeaselServerApp.h/.cpp` | 组合：`m_handler(RimeWithWeaselHandler(&m_ui))` + `m_server + m_ui + tray_icon`；`SetupMenuHandlers` 把每个 `ID_WEASELTRAY_*` 绑到 `execute/explore/open/check_update` |
| `WeaselService.h/.cpp` | Windows SCM 集成（ServiceMain / ServiceCtrlHandler） |
| `WeaselTrayIcon.h/.cpp` | 托盘图标 + 菜单项；`Refresh` 时拉 server status |
| `SystemTraySDK.h/.cpp` | 老式托盘封装 |
| `WeaselServer.rc` | 资源（图标、字符串、菜单） |
| `resource.h` | `ID_WEASELTRAY_*` 命令 ID 集中定义 |

### 托盘菜单命令（来自 `WeaselServer.rc` + `WeaselServerApp::SetupMenuHandlers`）

```
ID_WEASELTRAY_DEPLOY          → WeaselDeployer.exe /deploy
ID_WEASELTRAY_SETTINGS        → WeaselDeployer.exe
ID_WEASELTRAY_DICT_MANAGEMENT → WeaselDeployer.exe /dict
ID_WEASELTRAY_SYNC            → WeaselDeployer.exe /sync
ID_WEASELTRAY_ENABLE_ASCII    → SetOption(session_id, "ascii_mode", true)
ID_WEASELTRAY_DISABLE_ASCII   → SetOption(session_id, "ascii_mode", false)
ID_WEASELTRAY_CHECKUPDATE     → WinSparkle
ID_WEASELTRAY_INSTALLDIR      → explore install_dir
ID_WEASELTRAY_USERCONFIG      → explore WeaselUserDataPath
ID_WEASELTRAY_LOGDIR          → explore WeaselLogPath
ID_WEASELTRAY_WIKI/HOMEPAGE/FORUM → open URL
ID_WEASELTRAY_QUIT            → Stop()
```

## 4.3 WeaselDeployer（设置/部署 GUI）

| 文件 | 角色 |
|---|---|
| `WeaselDeployer.cpp` | `_tWinMain` → 单实例互斥（`WeaselDeployerExclusiveMutex`）→ `Run(lpCmdLine)`；`/deploy /dict /sync /install /?` 命令分发 |
| `Configurator.h/.cpp` | 主控制器：维护 `m_rime`、调 `rime_api->initialize/deploy/sync_user_data`、驱动 UI |
| `UIStyleSettings.h/.cpp` + `UIStyleSettingsDialog.h/.cpp` | 样式设置对话框（读 weasel.yaml、写入） |
| `SwitcherSettingsDialog.h/.cpp` | 切换器（方案列表）设置 |
| `DictManagementDialog.h/.cpp` | 词库管理 |
| `WeaselDeployer.rc` | 资源（图标、对话框模板、字符串） |
| `resource.h` | UI 控件 ID |

### 部署流程

`Configurator::UpdateWorkspace` 大致：
1. `rime_api->start_maintenance(true)` 同步等。
2. 遍历用户目录、预装目录，逐个 `rime_api->deploy_schema` / `deploy_config_file`。
3. `rime_api->join_maintenance_thread` 等线程。
4. 若用户目录为空，跑 `rime-install-config.bat` 引导初始化。

## 4.4 WeaselSetup（安装/卸载器，x86 only）

| 文件 | 角色 |
|---|---|
| `WeaselSetup.cpp` | `_tWinMain`；根据命令行 `/i /s /t /toggleascii /togglehan /testing /release` 分发；需要管理员权限时 `ShellExecuteEx(..., L"runas", ...)` 提升 |
| `InstallOptionsDlg.h/.cpp` | 安装选项对话框（简体/繁体、用户目录、是否保留旧配置） |
| `imesetup.cpp` | 核心：`install(bool hant, bool silent)` / `uninstall(bool silent)` / `has_installed()`。关键 TSF 布局字符串、注册表写入、文件复制（含 WER 注册） |
| `WeaselSetup.ico` | 安装器图标 |
| `WeaselSetup.rc` | 资源 |
| `resource.h` | 字符串/控件 ID |

### imesetup 关键

- 简体 layout：`PSZTITLE_HANS = L"0804:{A3F4CDED-...}{3D02CAB6-...}"`
- 繁体 layout：`PSZTITLE_HANT = L"0404:{A3F4CDED-...}{3D02CAB6-...}"`
- 写注册表：`HKLM\SOFTWARE\Microsoft\CTF\KnownClasses`（TSF 自注册）、`HKCR\CLSID\{A3F4CDED-...}`（COM）、`HKLM\...\Keyboard Layout\Preload`（布局）。
- 拷贝时若目标文件占用，先重命名为 `.old.N` 并标 `MOVEFILE_DELAY_UNTIL_REBOOT`，下次开机替换。
- `IsWow64Process2` 判断 ARM64 机器。
- WER 转储：HKLM\SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps\WeaselServer.exe

## 4.5 WeaselUI（候选面板 UI / 静态库）

| 文件 | 角色 |
|---|---|
| `WeaselUI.h/.cpp` | `weasel::UI` PIMPL：Create/Destroy/Show/Hide/ShowWithTimeout/Refresh/UpdateInputPosition/Update(ctx, status)；自带 `AUTOHIDE_TIMER` |
| `WeaselPanel.h/.cpp` | `WeaselPanel`：`WS_POPUP \| WS_CLIPSIBLINGS \| WS_DISABLED` + `WS_EX_TOOLWINDOW \| WS_EX_TOPMOST \| WS_EX_NOACTIVATE \| WS_EX_LAYERED` + `WS_EX_TRANSPARENT`；`OnDpiChanged / OnMouseWheel / OnMouseMove / OnMouseLeave / OnLeftClickedUp/Down`；`DoPaint` 渲染；`RedrawWindow` 处理多显示器 |
| `Layout.h` | 抽象基类：`GetPreeditRect/AuxiliaryRect/HighlightRect/CandidateRect/.../IsInlinePreedit/ShouldDisplayStatusIcon/...` |
| `StandardLayout.cpp/.h` | 默认布局（横排） |
| `VerticalLayout.cpp/.h` | 竖排布局 |
| `HorizontalLayout.cpp/.h` | 备用横排 |
| `VHorizontalLayout.cpp/.h` | 竖排横排切换布局（最复杂，约 26 KB） |
| `FullScreenLayout.cpp/.h` | 全屏模式 |
| `DirectWriteResources.cpp` | D2D/DirectWrite 资源（TextFormat、RenderTarget、Brush） |
| `GdiplusBlur.h/.cpp` | Gdiplus 模糊效果（用于阴影） |
| `xmake.lua` | `add_cxflags("/openmp")` 用于并行渲染 |

### 渲染管线（DoPaint）

1. `_ResizeWindow` 计算 panel 矩形（含高 DPI scale `dpiScaleLayout`）
2. `_CreateLayout` 根据 `UIStyle::layout_type` 选具体 Layout（Standard / Vertical / Horizontal / VHorizontal / FullScreen）
3. 双缓冲（`CDoubleBufferImpl<WeaselPanel>`） → `_DrawPreedit` / `_DrawPreeditBack` / `_DrawCandidates`
4. `_LayerUpdate` 输出 layered window

## 4.6 RimeWithWeasel（RIME 引擎对接 / 静态库）

| 文件 | 角色 |
|---|---|
| `RimeWithWeasel.cpp` | `RimeWithWeaselHandler`：实现 `RequestHandler`；管 `m_session_status_map<weasel_id, SessionStatus>`、`m_app_options`、`m_base_style`、`m_show_notifications`、`m_global_ascii_mode`、`m_show_notifications_time`；构造函数中 `rime_get_api() + rime_api->setup(&traits)`；`Initialize` 调 `rime_api->initialize + start_maintenance + join_maintenance_thread` + 读 weasel.yaml；`OnUpdateUI` 回调让托盘刷新 |
| `WeaselUtility.cpp` | `WeaselUserDataPath / WeaselSharedDataPath / GetCustomResource` 实现 |
| `xmake.lua` | 静态库 + `use_weaselconstants` |

### 关键回调

```cpp
class RimeWithWeaselHandler : public weasel::RequestHandler {
  Initialize()          // 启动 RIME
  Finalize()            // 清理
  FindSession / AddSession / RemoveSession
  ProcessKeyEvent(...)  // 调 rime_api->process_key
  CommitComposition(...)
  ClearComposition(...)
  SelectCandidateOnCurrentPage / HighlightCandidateOnCurrentPage / ChangePage
  FocusIn / FocusOut / UpdateInputPosition
  StartMaintenance / EndMaintenance
  SetOption(session_id, opt, val)   // ascii_mode、ascii_punct 等
  UpdateColorTheme(BOOL darkMode)
  _UpdateUIStyle / _UpdateUIStyleColor / _LoadAppOptions / _LoadSchemaSpecificSettings
  _LoadAppInlinePreeditSet / _UpdateInlinePreeditStatus
  OnNotify(...)         // rime_api->set_notification_handler
};
```

### SessionStatus

```cpp
struct SessionStatus {
  RimeSessionId session_id;
  std::string schema_id;
  Status status;     // ascii_mode / zhung / disabled / schema_*
  UIStyle style;
  std::set<std::string> inline_modes;
  std::set<std::wstring> app_options;
  AppIconType app_icon;
};
```

## 4.7 WeaselIPC（IPC 客户端 / 静态库）

| 文件 | 角色 |
|---|---|
| `WeaselClientImpl.h/.cpp` | `weasel::ClientImpl` + `weasel::Client` 包装；`Connect/Disconnect/ShutdownServer/Echo/ProcessKeyEvent/...`；位打包 `UpdateInputPosition`（12-bit left/top + 7-bit height + 1-bit hi-res flag） |
| `ContextUpdater.h/.cpp`、`Committer.cpp`、`ActionLoader.cpp`、`Configurator.cpp`、`Styler.cpp` | 各种 `RequestHandler` 适配（部署器、WeaselServer 用） |
| `Deserializer.h/.cpp` | 把响应 `key=value` 行的 `Store` 委托到具体字段 |
| `ResponseParser.cpp` | `ResponseParser` 解析整段响应 |
| `PipeChannel.cpp` | PipeChannelBase 通用读/写/重连 |
| `xmake.lua` | 静态库 |

## 4.8 WeaselIPCServer（IPC 服务端 / 静态库）

| 文件 | 角色 |
|---|---|
| `WeaselServerImpl.h/.cpp` | `weasel::ServerImpl`：基于 WTL `CWindowImpl`，窗口类 `WeaselIPCWindow_1.0`；`OnCreate/OnClose/OnDestroy/OnQueryEndSystemSession/OnEndSystemSession/OnColorChange/OnCommand`；每条 IPC 命令对应一个 `OnXxx` 处理函数；`Start/Stop/Run` 控制 `boost::thread pipeThread` |
| `SecurityAttribute.h/.cpp` | 命名管道的安全描述符：限制到当前用户 SID（关键安全点） |
| `xmake.lua` | 静态库 |

### Pipe 监听

```cpp
class PipeServer : public PipeChannel<DWORD, PipeMessage> {
  void Listen(ServerHandler const& handler);
  void _ProcessPipeThread(HANDLE pipe, ServerHandler const& handler);
};
```

`ServerImpl` 在 `OnCreate` 启动一个 `boost::thread` 反复 `ConnectNamedPipe`，把每条 `PipeMessage` 转给 `m_pRequestHandler`。

## 4.9 test（单元测试，仅 Debug 构建）

| 目录 | 测试 |
|---|---|
| `test/TestWeaselIPC` | IPC 客户端 ↔ 服务端往返 |
| `test/TestResponseParser` | 响应文本解析 |

xmake 仅在 `is_mode("debug")` 引入。xmake 顶层 `add_cxflags("/GL") / add_ldflags("/LTCG /INCREMENTAL:NO")` 也只在 release 模式生效。

## 4.10 arm64x_wrapper

仅 4 个文件，作用是把 `weaselx64.dll` 嵌入到 `weaselARM64X.dll`，由系统决定调用 arm64 还是 x64 路径。`xbuild.bat arm64` 在 `xmake` 完成后调用其 `build.bat`，把 `weaselARM64X.dll` 复制到 `output\`。
