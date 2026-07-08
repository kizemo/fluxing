# spec 050 任务清单 - yaml 快捷键可视化编辑器（v0.18.30 F3 MVP）

## P1 (must)

- [ ] T001: 新建 `FluxingConfigEditor/HotkeyBinding.h`（HotkeyBinding 数据结构 + accept 字符串解析 + NormalizeAccept 函数）
- [ ] T002: 新建 `FluxingConfigEditor/KeyRecorder.{h,cpp}`（按键录制器，捕获 WM_KEYDOWN + GetAsyncKeyState + MapVirtualKeyW）
- [ ] T003a: 新建 `FluxingConfigEditor/HotkeyEditorDialog.h`（对话框主框架 + MSG_MAP + ListView + 7 个 handler 声明）
- [ ] T003b: 新建 `FluxingConfigEditor/HotkeyEditorDialog.cpp`（OnInitDialog + LoadBindings + RefreshList + ListCompare）
- [ ] T003c: `HotkeyEditorDialog.cpp` 加 Add/Edit/Delete handlers（启动 KeyRecorderDialog，编辑后 RefreshList）
- [ ] T003d: `HotkeyEditorDialog.cpp` 加 OnSave（写回 custom.yaml）+ OnCancel + OnReset
- [ ] T003e: `HotkeyEditorDialog.cpp` 加 L17/L18/L19 linter（保存前检查 keycode=Shift_L 单键 binding 警告）
- [ ] T003f: `HotkeyEditorDialog.cpp` 加 TriggerDeploy（CreateProcess WeaselDeployer.exe /deploy）
- [ ] T004: `WeaselDeployer/WeaselDeployer.rc` 加 menu item "快捷键编辑器 (&H)\tCtrl+Shift+E" + ID_HOTKEY_EDITOR
- [ ] T005: `WeaselDeployer/Configurator.cpp` 加 `OpenHotkeyEditor()` 函数 + menu handler 注册
- [ ] T006: `include/resource.h` 加 IDD_HOTKEY_EDITOR + IDD_KEY_RECORDER + IDC_LIST 等对话框 ID
- [ ] T007: `weasel.sln` 注册新模块 + `xmake.lua` + `scripts/test-infra/run-test-suite.bat` 加 TestHotkeyEditor
- [ ] T008: 新建 `test/TestHotkeyEditor/{TestHotkeyEditor.cpp, vcxproj, xmake.lua, stdafx.h, stdafx.cpp, targetver.h}`
- [ ] T009: `xbuild.bat weasel` → 0 errors 0 warnings
- [ ] T010: 跑 `scripts/test-infra/run-test-suite.bat` → 全部 PASS (16+1 = 17+ test projects)
- [ ] T011: 手动验证：打开编辑器 → 添加 binding → 保存 → 触发 deploy → 新 binding 生效
- [ ] T012: 手动验证：升级时 default.custom.yaml 不被覆盖
- [ ] T013: commit `feat(fluxing): spec 050 yaml hotkey editor MVP (F3 partial ship)`
- [ ] T014: 更新 CHANGELOG（不动 env.bat/weasel.props = 不发 installer）

## P2 (should, 推迟到 spec 051+)

- [ ] T015: ConflictChecker 完整静态 db（JSON 列表 + librime 内置 binding 表）
- [ ] T016: StylePage（配色 / 透明度 / 字体 / 暗色模式）
- [ ] T017: SchemaPage（方案启用 / 拖拽 / 配置）
- [ ] T018: UserDictPage（用户词典搜索 / 编辑 / 删除 / 批量导入）
- [ ] T019: FluxingPanelHost 独立进程（spec 006 路线图）
- [ ] T020: 暗色主题 UI 细节

## P3 (nice, 推迟到 v2.1+)

- [ ] T021: 导入/导出配置包（spec 010 / 055）
- [ ] T022: 按键冲突云端检测（spec 010）

## 估计

- T001: 1 小时
- T002: 2 小时
- T003a-f: 4 小时（拆 6 子任务）
- T004-T006: 1.5 小时
- T007-T008: 1.5 小时
- T009-T012: 1.5 小时
- T013-T014: 30 分钟
- **总计**: ~12 小时（满足 R5: 单任务 < 4h, 1-3 文件, 总计 ≤ 12h）

## 依赖

- spec 024 YamlRoundTrip（已 ship）
- spec 037 FluxingComponents（已 ship，可选）
- spec 041 DPI 修复（已 ship，144 DPI 视觉正确）
- WTL / ATL（vendored）
- WeaselDeployer.exe /deploy（已有）
- L17 / L18 / L19 / L21 lessons（Shift key binding awareness）

## 验收回执

- [ ] T-A1: xbuild.bat weasel → 0 errors
- [ ] T-A2: TestHotkeyEditor 4/4 PASS
- [ ] T-A3: 手动验证：编辑器打开 → 添加 binding → 保存 → deploy → 立即生效
- [ ] T-A4: 升级时 default.custom.yaml 不被覆盖
- [ ] T-A5: L17/L18/L19 linter 警告有效
- [ ] T-A6: 144 DPI 视觉正确（spec 041 验证）

## 状态

- 2026-07-08: spec 050 created
- TBD: T001-T014 实施（下一 session）