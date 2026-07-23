# 051 - Out-of-Process PhrasesDialog

> 把常用短语对话框从 WeaselServer.exe 剥离到独立 GUI 进程，根治 8 个 ship 版本均 fail 的"输入框不能输中文"问题。

## 0. Background

PhrasesDialog（常用短语）当前在 WeaselServer.exe 进程内运行（WS_POPUP modal dialog）。WeaselServer.exe 是 TSF shim 进程（weaselx64.dll 主导），TSF 通过 `ITfThreadMgr` 强制所有 hwnd 走 system TSF-registered IME。PhrasesDialog 内部的 EDIT 控件（s_hInput）不是 TSF text service，其 IMM32 IME context 被 TSF TIP 抢占，导致中文候选词不出现。

自 v0.19.0.35 起至 v0.19.0.49，连续 8 个 ship 版本尝试了 `ImmCreateContext` / `ImmAssociateContext` / `ImmReleaseContext` / 删 isolated HIMC / `SetFocus` 触发 TSF attach / IMM32 fallback / `ImmAssociateContext` 强化，全部 fail。真因是架构层面的：in-process dialog 的 IME 线程被 TSF shim 进程环境劫持，无法通过 IMM32 API 修复。

根治方案：将 PhrasesDialog 剥离到独立 `FluxingPhrasesDialog.exe`（普通 Win32 GUI 进程，不注册 TSF textInputProcessor），通过 named pipe IPC 与 WeaselServer.exe 通信。独立进程的 hwnd 走系统默认 IMM32 IME context（与 notepad.exe 等普通 GUI 进程一致），不受 TSF shim 限制。

## 1. Goals

- 用户在 PhrasesDialog 的输入框中能正常输入中文（打拼音出候选词 + Ctrl+Shift 切换 IME）
- 独立进程不影响 WeaselServer.exe（TSF shim）主编辑框的正常功能
- named pipe IPC 延迟 < 50ms（对用户无感知）
- 现有 186 个 TestPhrasesDialog 测试全部保持 PASS
- 现有 UI 外观和行为（蓝色标题条、拖拽、resize、reorder、双击注入）保持不变
- installer 自动注册 FluxingPhrasesDialog.exe

## 2. User stories

### US001 - 在输入框中输入中文
**Priority**: P1
**描述**: 装机用户在常用短语对话框顶部的输入框中打拼音（如 "ni"），看到中文候选词弹出，选择后输入框显示 "你"。
**当前状态**: FAIL — 8 个 ship 版本候选词不出现。
**成功标准**: 候选词正常出现，可输入任意中文。

### US002 - 双击短语注入到前台应用
**Priority**: P1
**描述**: 用户在常用短语列表中双击某条短语，该短语文本注入到前台应用（如记事本、浏览器输入框）。
**当前状态**: 部分可用（TSF shim 进程中的 SendInput / InjectText 有时受 TSF 线程影响不稳定）。
**成功标准**: 双击任一条短语，文本立即出现在前台应用的焦点输入框。

### US003 - 在短语列表中添加新短语
**Priority**: P1
**描述**: 用户在输入框中输入新短语文本，点击"+ 添加"按钮或按 Enter，新短语出现在列表中并持久化到 yaml 文件。
**当前状态**: 添加功能正常（yaml 写入在 WeaselServer.exe 侧完成）。
**成功标准**: 通过 IPC 触发添加，新短语出现在列表中。

### US004 - Ctrl+Shift 切换输入法
**Priority**: P2
**描述**: 用户在输入框中按 Ctrl+Shift 切换中/英文输入模式。
**当前状态**: FAIL — in-process 时 TSF TIP 抢占导致切换无效。
**成功标准**: Ctrl+Shift 正常切换中/英文输入模式。

### US005 - 拖拽标题条移动窗口
**Priority**: P2
**描述**: 用户在标题条区域按下左键并拖动，窗口跟随移动。
**当前状态**: v0.19.0.49 修复了 kTitleH=48 扩大拖拽命中区，但 Bug A（蓝色标题条视觉未变高）待验证。
**成功标准**: 拖动流畅，无剧烈晃动。

## 3. Functional requirements

### FR-001: 独立进程架构
- FluxingPhrasesDialog.exe 为普通 Win32 GUI 进程
- 不注册 TSF textInputProcessor（不加载 weaselx64.dll）
- 由 WeaselServer.exe 通过 `CreateProcess` 启动
- 退出时 WeaselServer.exe 发送关闭信号并等待进程退出

### FR-002: Named pipe IPC 通信
- 双向 named pipe：数据通道 + 指令通道
- Pipe 命名：`\\.\pipe\FluxingPhrasesDialog\data` 和 `\\.\pipe\FluxingPhrasesDialog\cmd`
- 数据通道传输短语列表（JSON / 二进制结构体）
- 指令通道传输操作指令（Add / Delete / Edit / Inject / Load / Shutdown）

### FR-003: 短语数据同步
- 启动时 WeaselServer.exe 通过 pipe 发送完整 m_phrases 列表
- 添加/删除/编辑短语后 WeaselServer.exe 端 yaml 持久化 + 推新列表到 FluxingPhrasesDialog.exe
- 脱机重连：FluxingPhrasesDialog.exe pipe 断开后自动重连（3 次，间隔 1s）

### FR-004: 文本注入
- 双击/Enter 触发 InjectText 请求通过 pipe 发送到 WeaselServer.exe
- WeaselServer.exe 通过 SendInput / TSF 注入到前台应用
- 或者：FluxingPhrasesDialog.exe 自行调 SendInput（需要实测确认哪个路径更可靠）

### FR-005: 窗口管理
- WS_POPUP + WS_THICKFRAME + WS_EX_TOOLWINDOW + WS_EX_TOPMOST（保持现有风格）
- 拖拽/长按拖拽/resize 行为不变
- 关闭按钮（✕ 或 Esc）Hide 窗口，不退出进程（进程等待下次 Show）

### FR-006: installer 集成
- FluxingPhrasesDialog.exe 安装到安装目录（与 WeaselServer.exe 同级）
- named pipe ACL 配置（允许同用户进程连接）
- uninstall 时自动删除

### FR-007: 错误处理与日志
- Pipe 超时 30s → 弹 MessageBox 警告用户"短语服务连接失败"
- OutputDebugStringW 写入诊断日志（pipe connect/disconnect/data size）
- FluxingPhrasesDialog.exe 崩溃时 WeaselServer.exe 检测 pipe 断开 → 重新 CreateProcess

## 4. Non-goals

- 不迁移 UserDictionary / ShortcutSettings 到 out-of-process
- 不改变 yaml 短语存储格式
- 不添加新的 UI 功能或视觉改版
- 不修改 TSF shim (weaselx64.dll)
- 不做 QuickPanel 的 out-of-process 改造
- 不引入跨机器 IPC（保持单机）

## 5. Acceptance criteria

### SC-001: 中文输入可用 (P1)
- [ ] SC-001-1: 装机后在 PhrasesDialog 输入框中打 "ni" → 候选词 "你尼泥逆" 出现
- [ ] SC-001-2: 选择候选词 → 输入框显示 "你"
- [ ] SC-001-3: Ctrl+Shift 切换中/英文模式正常

### SC-002: 短语操作正常 (P1)
- [ ] SC-002-1: 输入新短语 → 点击 "+ 添加" → 短语出现在列表中
- [ ] SC-002-2: 双击列表中的短语 → 文本注入到记事本（前台应用）
- [ ] SC-002-3: 删除短语 → 列表中移除 + yaml 持久化

### SC-003: 窗口交互正常 (P2)
- [ ] SC-003-1: 在标题条区域按下左键拖拽 → 窗口跟随移动
- [ ] SC-003-2: 拖拽窗口边界（resize） → 列表/按钮自适应
- [ ] SC-003-3: Esc / ✕ 关闭 → 窗口隐藏
- [ ] SC-003-4: Alt+. 再次触发 → 窗口恢复显示（数据同步）

### SC-004: 进程管理 (P2)
- [ ] SC-004-1: WeaselServer.exe 退出 → FluxingPhrasesDialog.exe 自动退出
- [ ] SC-004-2: FluxingPhrasesDialog.exe 崩溃 → WeaselServer.exe 重新启动它
- [ ] SC-004-3: installer 卸载 → FluxingPhrasesDialog.exe 被删除

### SC-005: 测试保持 (P1)
- [ ] SC-005-1: TestPhrasesDialog 186/186 PASS / 0 FAIL（或适配后的等价测试全部 PASS）
- [ ] SC-005-2: _check_install_v2.ps1 验证通过（新增 FluxingPhrasesDialog.exe md5 校验）

## 6. Risks

| ID | 风险 | 概率 | 影响 | 缓解 |
|---|---|---|---|---|
| R-001 | Named pipe IPC 延迟 > 50ms，用户感知卡顿 | 低 | 中 | Pipe 用异步 I/O + 批量传输；实测后如有延迟改 shared memory |
| R-002 | SendInput 在独立进程中行为不同（UIPI 限制） | 中 | 高 | 优先走 WeaselServer.exe 端 SendInput；如 fail 尝试 FluxingPhrasesDialog.exe 端 SendInput |
| R-003 | Pipe 断连导致数据丢失（用户添加的短语未保存） | 低 | 高 | 每次操作等 ACK；pipe 断时写本地临时缓存 + 重连后 merge |
| R-004 | 两个进程间窗口 Z-order 竞争（TopMost 冲突） | 低 | 低 | FluxingPhrasesDialog.exe 用 WS_EX_TOPMOST + SetWindowPos(HWND_TOPMOST) |
| R-005 | DPI 缩放在两进程间不同步（主屏 vs 副屏） | 低 | 低 | FluxingPhrasesDialog.exe 独立计算 DPI（跟现有 OnCreate 一致） |
| R-006 | 现有 186 测试依赖 in-process 假设，大规模 rewrite | 中 | 中 | 测试适配优先于新功能；拆分 test IPC mock layer |

## 7. Dependencies

- WeaselServer.exe 现有 Named Pipe 基础设施（`PipeChannel*` — 如已有）
- librime (yaml 读写 — 仍在 WeaselServer.exe 侧)
- WeaselIPCData.h (IPC 数据结构定义)
- install.nsi (installer 注册)
- _check_install_v2.ps1 (安装验证脚本)

## 8. Out of scope

- UserDictionary / ShortcutSettings 的 out-of-process 改造
- QuickPanel 的 out-of-process 改造
- 跨机器 IPC 或网络通信
- 短语数据的云端同步
- 新的 UI 设计或视觉主题
- TSF shim 行为变更
