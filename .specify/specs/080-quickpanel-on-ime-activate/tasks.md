# 080 tasks - 切换到 Fluxing 时自动显示快捷设置栏

- [x] **T001** [P1] 取证: 定位「切换输入法自动显示 QuickPanel」的历史设计与回滚点
  (spec 052 US052-A → 055 → 056 → 060 → 061 → 066 → v0.18.41.4 `if (false)` →
  spec 070/L75 切断崩溃链但开关未恢复 → spec 075 再次确认禁用)
- [x] **T002** [P1] 取证: Fluxing TIP **没有**注册键盘布局 ⇒ server 无法独立检测
  输入法切换 ⇒ 必须由前端上报
  (`HKLM\SYSTEM\CurrentControlSet\Control\Keyboard Layouts` 无 Fluxing 项)
- [x] **T003** [P1] 写 ADR 0008: 架构决策 + 被否决的备选方案
  (`docs/adr/0008-ime-profile-activation-notification.md`)
- [x] **T004** [P1] `include/WeaselIPC.h`: 新增 `WEASEL_IPC_FOCUS_IN_FLAG_PROFILE_ACTIVATED`
  位标志; `Client::FocusIn(DWORD client_caps = 0)`; `Client::CloseThreadChannel()`;
  `Client::HasSession()`
- [x] **T005** [P1] `WeaselIPC/WeaselClientImpl.h/.cpp`: `session_id` → `std::atomic<UINT>`;
  `FocusIn(DWORD)`; `CloseThreadChannel()`; `HasSession()`
- [x] **T006** [P1] `WeaselTSF/WeaselTSF.h/.cpp`: 通知线程 + `OnActivated` 改为零 I/O;
  `Deactivate` / 析构里停线程 (3s 超时 + TerminateThread 兜底)
- [x] **T007** [P1] `include/RimeWithWeasel.h` + `RimeWithWeasel/RimeWithWeasel.cpp`:
  profile-activated 分支 + `m_active_session` 闸门; 删除 `if (false && ipc_id > 0)` 死代码
- [x] **T008** [P1] `WeaselServer/resource.h` + `WeaselServer/WeaselServerApp.h/.cpp`:
  `ID_WEASELTRAY_QUICK_PANEL_SHOW`; `ShowQuickPanel()` / `ToggleQuickPanel()` 抽取;
  两条路径统一 marshal 到 server UI 线程
- [x] **T009** [P2] 写 spec 080 (spec.md / plan.md / tasks.md)
- [x] **T010** [P1] 构建: Win32 全量零错误 (x64 因本机无 x64 librime 链接失败, 属环境既有)
- [x] **T011** [P2] 打包安装器 (0.21.0.5) 并静默升级装机; 装机后 md5 与构建产物全等
- [x] **T012** [P1] 真机验证: 切到 Fluxing → 面板出现; 切走 → 面板消失
  (TSF 级端到端驱动 + 对照实验, 见 T018)
- [x] **T013** [P1] 取证与修复: `ITfActiveLanguageProfileNotifySink` **实现了但从未注册**
  ⇒ `OnActivated` 是死代码。补 `QueryInterface` 的 IID 分支 + `ActivateEx` 里
  `AdviseSink` + `Deactivate` 里 `UnadviseSink`; 注册失败不致命 (详见 L110)
- [x] **T014** [P2] 真机验证: Application Error 事件 0 条; 无 QuickPanel 崩溃
- [x] **T015** [P2] 更新 `CHANGELOG.md` (0.21.0.5-fluxing)
- [x] **T016** [P2] 更新 `.specify/memory/lessons-learned.md` (L110: sink 伪完成;
  L111: 程序化切 profile 打崩输入栈 + 测试护栏)
- [x] **T017** [P2] 提交: `feat(fluxing)` + `docs(fluxing)`, Conventional Commits + 模块前缀
- [x] **T018** [P1] 新增 TSF 级端到端驱动 `test/TestWeaselIPC/TsfProfileVerify.cpp`:
  用 `ITfInputProcessorProfileMgr` 真激活 / 停用 profile, 让 Windows 自己派发
  `OnActivated`, 再断言面板可见; **对照实验** (装机前 `visible=0` → 装机后 `visible=1`)
- [x] **T019** [P1] 修复 QuickPanel 复用路径不重启 hover polling timer ⇒ 面板第二次
  显示后不再自动隐藏 (真实 bug, 用户可见)
- [x] **T020** [P1] 给 `/verify-tsf-profile` 加三层崩溃护栏: 显式 opt-in 开关 +
  远程会话检测 + Application 事件日志跑前跑后比对; `auto_hidden` 降级为仅报告
- [x] **T021** [P2] 把自动隐藏的覆盖从危险的 profile 切换测试搬到安全测试:
  `/verify-quickpanel` 新增「显示 → 自动隐藏 → 再显示 (复用路径) → 再自动隐藏」
  回归, 两轮都要过
