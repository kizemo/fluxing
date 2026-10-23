# 080 plan - 切换到 Fluxing 时自动显示快捷设置栏

- **spec**: [spec.md](./spec.md)
- **ADR**: [0008-ime-profile-activation-notification](../../docs/adr/0008-ime-profile-activation-notification.md)

## 1. 设计摘要

```
[用户切换输入配置]
        │
        ▼
TSF 进程 (每个加载了 Fluxing TIP 的进程各一份)
WeaselTSF::OnActivated(isActivated)
        │  ← TSF 回调内零 I/O (P2)
        ▼ 仅置标志 + notify_one
通知线程 (WeaselTSF 私有, 每次激活一条)
        │  m_client.FocusIn(WEASEL_IPC_FOCUS_IN_FLAG_PROFILE_ACTIVATED)
        │  m_client.CloseThreadChannel()   ← 发完即关, 不留空闲连接
        ▼ 命名管道 (现有 WeaselIPC)
WeaselServer: ServerImpl::OnFocusIn → RimeWithWeaselHandler::FocusIn
        │  闸门: ipc_id != m_active_session → 忽略 (后台进程)
        │  且不改 m_active_session
        ▼ m_onShowQuickPanel()
PostMessage(serverHwnd, WM_COMMAND, ID_WEASELTRAY_QUICK_PANEL_SHOW)
        │  ← 从 IPC 管道线程 marshal 到消息循环线程
        ▼
WeaselServerApp::ShowQuickPanel()  (UI 线程)
        ▼
QuickPanelDialog::Show(...)  ← 窗口 / GDI / timer 都归 UI 线程
```

### 为什么 flag 而不是新消息类型

`WEASEL_IPC_FOCUS_IN` 的 `wParam` 原本是未使用的 "client caps" 字段。
「切换到 Fluxing」本质上是 FocusIn 的一个**变体**, 用一个标志位表达,
协议表面不变, 老 server 也能安全降级。新增消息类型要动 `WeaselIPC.h`
枚举 + deployer 侧说明 + 测试, 收益不抵成本。

### 为什么闸门放在 server 而不是 TSF

`OnActivated` 是 **per-process** 订阅 —— 切换一次, N 个进程各发一次。
若在 TSF 侧用 `GetForegroundWindow()` 过滤, 会踩两个坑:
(1) 用户点任务栏语言栏切换时, 那一刻的前台窗口可能变成语言栏自己的窗口,
过滤会误杀; (2) 前台窗口判定是竞态。
server 侧已经有权威答案: `m_active_session` 就是「当前获得焦点的那个会话」。
用它当闸门, 既天然去重, 又保证面板落在用户正在输入的那个应用上。

## 2. 变更清单

| 文件 | 变更 |
|---|---|
| `include/WeaselIPC.h` | 新增 `WEASEL_IPC_FOCUS_IN_FLAG_PROFILE_ACTIVATED`; `Client::FocusIn(DWORD)`; `Client::CloseThreadChannel()`; `Client::HasSession()` |
| `WeaselIPC/WeaselClientImpl.h/.cpp` | `session_id` 改 `std::atomic<UINT>`; `FocusIn(DWORD)`; `CloseThreadChannel()`; `HasSession()` |
| `WeaselTSF/WeaselTSF.h/.cpp` | 通知线程 (`_StartProfileNotifyThread` / `_StopProfileNotifyThread` / `_PostProfileNotify` / `_ProfileNotifyLoop`); `OnActivated` 改为零 I/O |
| `include/RimeWithWeasel.h` | `SetOnShowQuickPanel()` + `m_onShowQuickPanel` |
| `RimeWithWeasel/RimeWithWeasel.cpp` | `FocusIn` 增加 profile-activated 分支; 删除 `if (false && ipc_id > 0)` 死代码 |
| `WeaselServer/resource.h` | `ID_WEASELTRAY_QUICK_PANEL_SHOW 40020` |
| `WeaselServer/WeaselServerApp.h/.cpp` | `ShowQuickPanel()` / `ToggleQuickPanel()` 抽取; UI 线程 marshal; 新菜单 handler |

## 3. Constitution Check

| 原则 | 结论 | 依据 |
|---|---|---|
| P1 Code Style | ✅ | 头文件 `#pragma once`; 类 `PascalCase` / 成员 `camelBack_` / 常量 `kCamelCase`; 新增成员命名 `m_profileNotify*` / `kProfileNotify*` |
| P2 Windows-Specific | ✅ | TSF 回调内零 I/O; 用户可见字符串未新增(本特性不引入新文案); 未新增 IPC 消息类型, 只扩展既有消息的未使用字段; QuickPanel 在 UI 线程操作 |
| P3 Build & Dependencies | ✅ | 无新第三方依赖; 未动 librime submodule |
| P4 Commit & PR | ✅ | `feat(WeaselTSF): ...` + `feat(WeaselServer): ...` 两笔, 模块前缀合规; CHANGELOG 已更新 |
| P5 UX | ✅ | 不改默认键位 / 方案行为 / 用户可见字符串; 面板行为与手动 Alt+, 路径一致 |
| P6 Backwards Compat | ✅ | 不动 `%AppData%\Rime`; 用户 yaml / 词库不受影响; 老 server 混装时降级不炸 |
| P7 Subprocess Lifecycle | ✅ | 未新增进程; 通知线程在前端进程内, 不是新 supervisor |
| P8 Brand Fork | ✅ | 只在 `Fluxing` 分支上; 不改上游 GUID / 品牌字符串 |

## 4. 验证计划

1. `msbuild weasel.sln /p:Configuration=Release /p:Platform=Win32` → 0 error
2. `msbuild weasel.sln /p:Configuration=Release /p:Platform=x64` →
   WeaselTSF (weaselx64.dll) 必须成功; WeaselServer x64 因本机没有 x64 librime
   而链接失败 (环境既有问题, 与本改动无关)
3. 打包安装器 → 静默升级装机 → 真机切输入法验证
4. server 日志 (`%AppData%\Rime\Rime\...` 或 WeaselServer log) 应出现
   `profile activated, request QuickPanel`; 后台进程的重复通知应出现
   `profile activated ignored, not the active session`
5. Application Error 事件计数保持 0
