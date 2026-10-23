# 080 - 切换到 Fluxing 时自动显示快捷设置栏 (QuickPanel)

- **状态**: 实现中 (2026-10-23)
- **架构决策**: [ADR 0008](../../docs/adr/0008-ime-profile-activation-notification.md)
- **关联**: spec 052 (US052-A) / spec 055 / spec 056 / spec 060 / spec 061 / spec 066 /
  spec 070 / spec 075 / L57bis / L59-L62 / L67-L69 / L75 / constitution P2 / P4

## 0. 背景

用户诉求: **切换到输入法时默认显示快捷设置栏**。

这个功能不是没设计过, 而是**设计过又被回滚**:

- spec 052 (US052-A) 引入 "always show mode"
- spec 055 / 056 / 060 / 061 把它接到 `ITfActiveLanguageProfileNotifySink::OnActivated`
- spec 066 (v0.18.41.0) 在 `RimeWithWeaselHandler::FocusIn` 里加
  `EnableAlwaysShowMode()`, 触发 GDI+ `Bitmap(IStream*)` 生命周期 bug
  (L67-L69) → `STATUS_HEAP_CORRUPTION` → WeaselServer 崩溃
- v0.18.41.4 用 `if (false && ipc_id > 0)` 禁掉全部 QuickPanel 触发
- spec 070 / L75 把 QuickPanelDialog 改成 **WIC 解码 + 纯 GDI**, 崩溃链被切断,
  **但这个开关一直没恢复**
- spec 075 (v0.74.3) 又因为 0xc0000374 跨进程崩溃 (主因是 System32 残留 shim +
  `WeaselUI/WeaselPanel.cpp` 的 GDI+ 残留) 再次确认禁用

⇒ 本 spec 的正身是: **恢复这个开关, 并用 P2-safe 的方式做切换触发**。

## 1. 目标

- **G1** 用户把活动输入配置切到 Fluxing 时, 快捷设置栏自动出现 (一次, 在前台应用)
- **G2** TSF 回调内零 I/O (constitution P2)
- **G3** 面板只弹一次, 且不会被后台进程抢走 active session
- **G4** 面板在 server 的消息循环线程上创建 (窗口 / GDI / timer 归属)

## 2. 用户故事

- **US080-A** [P1]: 用户在任意应用里按 Ctrl+Space 切到 Fluxing, 快捷设置栏立刻出现,
  里面有 logo 与 4 个按钮 (方案 / 短语 / 设置 / 账号), 按钮可点
- **US080-B** [P1]: 用户切到 en-US 时快捷设置栏消失, 不残留在屏幕上
- **US080-C** [P2]: 切输入法**不会**让前台应用卡住 (TSF 线程不得被 I/O 拖住)
- **US080-D** [P2]: 后台应用不会把快捷设置栏顶出来 (spec 055 bug 2 不复现)

## 3. 功能需求

- **FR-001**: `WEASEL_IPC_FOCUS_IN` 的 `wParam` 增加位标志
  `WEASEL_IPC_FOCUS_IN_FLAG_PROFILE_ACTIVATED` (bit 0), 区分
  「切换到 Fluxing」与「窗口获得焦点」。不新增消息类型, 不改 wire struct
- **FR-002**: `WeaselTSF::OnActivated` 不做任何 pipe I/O, 只置标志 + 唤醒专用通知线程
- **FR-003**: 通知线程用**自己那条**管道连接发一次通知, 发完立刻关掉
  (`ClientImpl::CloseThreadChannel`), 不给 WeaselServer 留空闲连接
- **FR-004**: `RimeWithWeaselHandler::FocusIn` 用 `m_active_session` 当闸门:
  不是当前活跃会话的通知一律忽略, 且该分支**不改** `m_active_session`
- **FR-005**: 显示面板的请求经 `PostMessage` 转到 server 消息循环线程再执行
- **FR-006**: 已有的 `ID_WEASELTRAY_QUICK_PANEL` handler 同样 marshal 到 UI 线程
  (顺带修好语言栏左键路径 —— 它原本在 IPC 管道线程上创建窗口)
- **FR-007**: 删除 `RimeWithWeaselHandler::FocusIn` 里的 `if (false && ipc_id > 0)`
  死代码块

## 4. 非目标

- 不做 spec 052 的 "always show mode" (常显)。面板是**瞬时**窗口:
  与手动 Alt+, 路径同款 —— 2s 宽限期 + 鼠标移开自动隐藏
- 不重写 `WeaselUI/WeaselPanel.cpp` 的 GDI+ 路径 (L67-L69 雷区残留, 独立议题)
- 不改 `WeaselDeployer` (它不发 `WEASEL_IPC_FOCUS_IN`)
- 不动 `output/install.nsi` (与本特性无关)

## 5. 验收标准

| ID | 标准 | 验证方式 |
|---|---|---|
| SC-001-1 | 切到 Fluxing → 面板出现 | 真机: 切输入法后截图 + server 日志 `profile activated, request QuickPanel` |
| SC-001-2 | 切到 en-US → 面板消失 | 真机: 切走后截图 |
| SC-001-3 | 面板按钮可点 | 真机: 点「设置」→ 部署器窗口弹出 |
| SC-002-1 | TSF 回调零 I/O | 代码审查: `OnActivated` 内无 `m_client.*` 调用 |
| SC-002-2 | 编译零错误 | msbuild Win32 + x64 |
| SC-003-1 | 只弹一次 | server 日志里 `profile activated` 出现次数 == 切换次数 (不是进程数) |
| SC-003-2 | 后台进程不抢 active session | 日志里 `profile activated ignored, not the active session` 出现 |
| SC-004-1 | 无崩溃 | 装机后 Application Error 事件 0 条 |

## 6. 风险

- **R-001**: `WeaselUI/WeaselPanel.cpp` 仍用 GDI+ (spec 075 记录的残留雷区)。
  本特性**不增加** GDI+ 用量 (QuickPanelDialog 已是 WIC + 纯 GDI; 本次还让
  profile-activated 分支跳过 `_UpdateUI`), 但历史崩溃链并未彻底消灭
- **R-002**: 每个加载了 Fluxing TIP 的前台进程会多一个常驻通知线程
- **R-003**: server 卡死时通知线程会卡在管道读上 → `Deactivate` 用
  3s 超时 + `TerminateThread` 兜底, 保证不冻结宿主应用
- **R-004**: 老版本 WeaselServer 不认识新标志位 → 会当成普通 FocusIn 处理
  (不弹面板, 也不崩) ⇒ 混装时功能降级但不炸

## 7. 依赖

- 无新依赖 (constitution P3)
- ADR 0008 (本次的架构决策)
