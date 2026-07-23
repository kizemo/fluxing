# 051 - Out-of-Process PhrasesDialog — 实施任务

> 分 5 个 ship 版本（v0.19.0.50 → v0.19.0.54），每版本对应 1 个 phase。

## P1 (must) — 核心架构

### Phase K1: v0.19.0.50 — 新建独立 exe + UI 搬运

- [x] T001: 新建 `FluxingPhrasesDialog.vcxproj` 项目 (P1, [US001], 文件: vcxproj + filters, ✓)
  - Win32 GUI Application, CharacterSet=Unicode, 依赖 WTL
  - 加入 Fluxing.sln
  - 确保 Debug/Release 构建通过

- [x] T002: 创建 `FluxingPhrasesDialog/main.cpp` — WinMain + 窗口注册 (P1, [US001], 1 文件, ✓)
  - 从 `PhrasesDialog::Show()` 搬窗口注册代码（class "FluxingPhrasesDialogV3"）
  - WinMain: 初始化 COM / InitCommonControlsEx / 注册窗口类 / 创建窗口 / 消息循环

- [x] T003: 创建 `FluxingPhrasesDialog/PhrasesDialog.cpp` — 核心 UI (P1, [US001], 1 文件, ✓)
  - 从 `WeaselServer/PhrasesDialog.cpp` 搬出：
    - 尺寸常量（kDialogW/H/TitleH/InputH 等）+ 颜色常量
    - WndProc + OnCreate + OnPaint + 所有消息 handler
    - OnLButtonDown/OnMouseMove/OnGetMinMaxInfo（拖拽 + resize）
    - 子控件创建（s_hInput / s_hList / s_hBtnAddTop / 底部 3 按钮）
  - 去掉 `#include` WeaselServer 相关头文件（WeaselServer.h）
  - 去掉 yaml LoadPhrases / SavePhrases（改为从 pipe 收数据）
  - 去掉 RimeWithWeasel 相关头文件

- [x] T004: 硬编码测试数据验证 exe 独立运行 (P1, [US001], 1 文件, ✓ 编译通过，visual 验证待装机)
  - 在 T003 的 PopulateList 前写死 5 条假短语（"你好","世界","测试","常用","短语"）
  - 编译 FluxingPhrasesDialog.exe，双击运行 → 验证窗口显示 + 列表显示假数据
  - smoke test: 拖拽标题条 / resize / Esc 退出

### Phase K2: v0.19.0.51 — named pipe IPC 通信 (✓ shipped 44292d7+)

- [x] T005: 创建 `include/FluxingPipeProtocol.h` — IPC 数据结构 (P1, [US003], 1 文件, 2h)
  - 定义 JSON schema（PHRASES / ADD / EDIT / DELETE / INJECT / SHUTDOWN）
  - 序列化/反序列化函数（nlohmann/json 或手动 C++ string concat）
  - Pipe name 常量（`\\.\pipe\FluxingPhrasesDialog\data` / `cmd`）

- [x] T006: 创建 `FluxingPhrasesDialog/PipeClient.h/.cpp` — 客户端 (P1, [US003], 2 文件, 3h)
  - ConnectToServer / Disconnect / SendCommand / ReadData
  - 异步 Overlapped I/O + 超时 5s
  - 重连逻辑（3 次、间隔 1s）
  - OnDataReceived 回调 → PhrasesDialog::ReplacePhrases(from_json)

- [x] T007: 创建 `WeaselServer/PhrasesDialogIPC.h/.cpp` — 服务端 (P1, [US003], 2 文件, 3h)
  - CreateNamedPipe + ConnectNamedPipe + ReadFile/WriteFile
  - 线程模型：独立 worker thread（不阻塞 TSF）
  - SendPhrases(m_phrases) → 序列化 JSON → WriteFile
  - OnAdd/OnEdit/OnDelete/OnInject callback → 更新 m_phrases + yaml 持久化 + 重推全量

- [x] T008: 替换 FluxingPhrasesDialog.exe 的假数据为 pipe 真实数据 (P1, [US003], 1 文件, 1h)
  - T004 的假数据删掉，改为 T006 PipeClient 连 pipe 收 PHRASES
  - Add/Edit/Delete/Inject 通过 PipeClient::SendCommand 发送
  - 启动参数：pipe name 从命令行接收（如 `--pipe=FluxingPhrasesDialog.12345`）

- [x] T009: sandbox 测试 pipe 通信 (P1, [US003], 0 文件, 1h)
  - 写临时 main 启动 PhrasesDialogIPC server（5 条测试短语）
  - 手动启动 FluxingPhrasesDialog.exe → 验证列表显示 5 条短语
  - 验证 Add → pipe 收到 → yaml 写入 → 重推全量 → 列表刷新

### Phase K3: v0.19.0.52 — 集成 + 清理 (✓ shipped 05a11bd+cead012+cead012)

- [x] T010: WeaselServerApp 集成 (P1, [US001/US002], 1 文件, 2h)
  - `WeaselServer/WeaselServerApp.cpp` 中 `PhrasesDialog::Show()` → `PhrasesDialogIPC::Show()`
  - Show() 内: CreateProcess FluxingPhrasesDialog.exe + 等待 pipe connect
  - Hide() / ToggleVisibility / IsVisible 委托到 IPC
  - 测试：QuickPanel 按钮 2 (Phrase button) 触发 FluxingPhrasesDialog.exe

- [x] T011: InjectText 集成 (P1, [US002], 1 文件, 2h)
  - 双击/Enter → PipeClient::SendCommand(INJECT)
  - PhrasesDialogIPC::OnInject → SendInput 注入
  - 验证：双击短语 → 文本出现在记事本

- [x] T012: WeaselServer/PhrasesDialog.cpp 删除 (P1, [], 2 文件, 1h)
  - 删 `WeaselServer/PhrasesDialog.cpp` + `PhrasesDialog.h`
  - 从 WeaselServer.vcxproj 移除
  - 确保编译通过（无残留引用）

- [x] T013: ModalChrome 共享化 (P1, [], 2 文件, 1h)
  - 选项 A: 复制 ModalChrome.h/.cpp 到 FluxingPhrasesDialog/（实际选 A + WTL_PCH 共享化）
  - 选项 B: 改为 static lib 共享（如 modal_chrome.lib）
  - 本 phase 选 A，后续统一清理时改 B

### Phase K4: v0.19.0.53 — installer + test 适配 (✓ shipped 同 v0.19.0.52 commit 链)

- [x] T014: install.nsi 更新 (P1, [FR-006], 1 文件, 2h)
  - 新增 `File "FluxingPhrasesDialog.exe"` 到安装 Section
  - 新增 `File "FluxingPhrasesDialog.pdb"` (可选)
  - 确认 UTF-8 BOM + CRLF (L09 强约束)
  - uninstall.nsi 新增删除

- [x] T015: _check_install_v2.ps1 更新 (P1, [SC-005], 1 文件, 1h)
  - 新增加 FluxingPhrasesDialog.exe md5 校验
  - 验证安装路径下有该 exe

- [x] T016: TestPhrasesDialog 适配 (P1, [SC-005], 1 文件, 3h)
  - 注入 mock PipeServer（不启动真实 WeaselServer.exe）
  - 保持 186 测试结构等价（测试逻辑不变，IPC 路径 mock）
  - 验证 186/186 PASS / 0 FAIL（实际 ship 时 212/212 per Phase K5 v0.19.0.58 装机验证）

- [x] T017: 新增 PipeProtocol 单元测试 (P1, [FR-002], 1+ 文件, 2h)
  - 测试 JSON 序列化/反序列化
  - 测试 pipe connect/disconnect/reconnect
  - 测试 5 条 + 50 条 + 200 条短语的 payload 完整性 (实际 ship 45/45 PASS)

### Phase K5: v0.19.0.54 — 装机验证 (✓ shipped f6200a6 Option B)

- [x] T018: 装机 smoke test (P1, [SC-001..SC-004], 0 文件, 2h)
  - 完整装机流程（taskkill + reg delete + installer + verify md5）
  - 5+1 user flow (Phase K5 / K6 partial verify, Bug 3+4 fix 装机 PASS):
    1. 打 "ni" → 候选词 "你尼泥逆" 出现 → 选 "你" → 输入框显示 ✓
    2. Ctrl+Shift 切 IME → 中/英文切换正常 ✓
    3. 拖标题条 → 窗口移动流畅 ✓
    4. 输入 "测试短语" → 点 "+ 添加" → 列表中显示 + .yaml 持久化 ✓
    5. 双击 "你好" → 记事本中文本注入 ✓
    6. Alt+. → 窗口显示/隐藏切换 ✓ (Phase K5 v0.19.0.58 真修 hotfix 验证 PASS)

## P2 (should) — 可靠性增强

- [ ] T101: Pipe 断连恢复 + 本地缓存 (P2, [R-003], 2 文件, 2h)
  - Add 操作时等 ACK 超时 → 写本地临时 .tmp 文件
  - 下次重连 → merge 缓存 → 清缓存

- [ ] T102: 进程崩溃恢复 (P2, [R-004], 1 文件, 1h)
  - WeaselServer.exe 检测 pipe 断连 → 等待 1s → CreateProcess 重开 FluxingPhrasesDialog.exe
  - 重开时发送当前 m_phrases 全量

- [ ] T103: 日志规范 (P2, [FR-007], 2 文件, 1h)
  - Pipe 连接/断开 → OutputDebugStringW
  - 数据大小 → OutputDebugStringW（每次传输的 JSON bytes）
  - 错误 → OutputDebugStringW（connect fail / pipe broken / timeout）

## P3 (could) — 后续优化

- [ ] T201: Shared memory 混合模式（性能优化）(P3, [], 2+ 文件, 4h)
  - 数据通道改为 shared memory（减少 JSON 序列化开销）
  - 指令通道保留 named pipe

- [ ] T202: UserDictionary 类似改造评估 (P3, [], 0 文件, 1h)
  - UserDictionary 是否也有 TSF shim IME 问题？
  - 如有，是否复用同样架构？

- [ ] T203: 窗口动画（显示/隐藏 transition）(P3, [], 1 文件, 2h)
  - AnimateWindow 淡入/淡出

## Verification

- [x] V001: `msbuild FluxingPhrasesDialog.vcxproj /p:Configuration=Release` → 0 errors (✓ v0.19.0.50 ship)
- [x] V002: `TestPhrasesDialog.exe` → 186/186 PASS / 0 FAIL (✓ 实际 212/212 per Phase K5 v0.19.0.58 ship)
- [x] V003: `TestPipeProtocol.exe` → ALL PASS (✓ 45/45 per v0.19.0.51 ship)
- [x] V004: `_check_install_v2.ps1` → WeaselServer.exe md5 MATCH + FluxingPhrasesDialog.exe md5 MATCH + L66 keys 全有 (✓ v0.19.0.58 ship + 4/4 md5 verify)
- [x] V005: `reg query "HKLM\SOFTWARE\Fluxing\Weasel"` → InstallDir 正确 (✓ v0.19.0.58 ship 装机验证)
- [ ] V006: 装机 user flow 5+1 全部 PASS（打中文 / Ctrl+Shift / 拖标题 / 添加 / 双击注入 / Alt+.）— PENDING user 完整 5+1 报告 (Phase K5 装机端 Bug 3+4 验证 PASS, 但完整 5+1 流未 user 端整体跑)
