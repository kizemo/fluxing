# 084 · Tasks · 快捷设置栏长显模式

> 配套：[spec.md](spec.md)、[plan.md](plan.md)、[ADR 0013](../../../docs/adr/0013-quickpanel-always-show-alpha-factor.md)。

## Phase 1 · 不透明度系数与渐变（Q1）

- [x] T001 [P1] [US084-B] 新建 `include/QuickPanelAlpha.h`：`ScaleAlpha` / `StepAlpha` / `LerpAlpha` 三个纯函数 + `kAlphaFactorFull=255` / `kAlphaFactorIdle=51` / `kAlphaFadeStep=24`，不依赖 `windows.h`
- [x] T002 [P1] [US084-B] `WeaselServer/QuickPanelDialog.cpp` 的 `ApplyAlphaGradient()` 把整体不透明度系数乘进逐像素 alpha（系数满值时逐像素不变）
- [x] T003 [P1] [US084-B] `WeaselServer/QuickPanelDialog.cpp` 的 100ms 轮询定时器算出「常态 / 悬停」目标系数并按步进渐变，变了才重绘

## Phase 2 · 常显模式与开关（Q2）

- [x] T004 [P1] [US084-A] `WeaselServer/QuickPanelDialog.{h,cpp}` 加 `SetAlwaysShowMode()` / `AlwaysShowMode()`，常显态下屏蔽三条自动隐藏路径（鼠标在外 1500ms / 前台切走 / 失焦）
- [x] T005 [P1] [US084-A] `WeaselServer/QuickPanelDialog.cpp` 的 `EnableAlwaysShowMode()` 改为「开长显 + 注入回调 + 显示」，并让 `Show()` 在常显态下以 20% 系数起步
- [x] T006 [P1] [US084-C] 菜单勾选查询钩子（**实现偏差**：不碰第三方 `SystemTraySDK.*`，改挂在 `WeaselTrayIcon::CustomizeMenu()` —— `CSystemTray` 本来就有这个虚函数；ADR 0013 D10 已同步改写）
- [x] T007 [P1] [US084-C] `WeaselServer/WeaselServer.rc` 三个语言变体各加一条「快捷设置栏常驻」菜单项（UTF-16LE + BOM + LF，脚本改写并回读校验）
- [x] T008 [P1] [US084-A] `WeaselServer/WeaselServerApp.{h,cpp}` 接 `ID_QUICKPANEL_ALWAYS_SHOW` handler（非界面线程 `PostMessage` 弹回界面线程）+ 挂菜单勾选钩子 + 复用 `ShowQuickPanel()` 注入真实回调
- [x] T009 [P1] [US084-D] `WeaselServer/QuickPanelDialog.cpp` 的 `ToggleMode()` 不再把 6 个回调传成空指针（原样回传当前静态字段）

## Phase 3 · 验证（Q3）

- [x] T010 [P1] [US084-B] `test/TestQuickPanelDialog/TestQuickPanelDialog.cpp` 加系数演算断言（满值系数逐像素不变、20% 系数结果、渐变步进不越过、常显开关往返）
- [x] T011 [P2] [US084-B] 门禁复跑：全套件 + 整解构建 + `clang-format` 整文件 clean
- [x] T012 [P2] [US084-D] `env.bat` 版本升 `1.1.0`，重打包并复验装器载荷
- [ ] T013 [P2] [US084-A] 交用户真机验收（常驻不消失 / 悬停变清晰 / 勾号跟随 / 按钮仍可用）

## 依赖

- T002 → T001
- T003 → T002
- T005 → T004
- T008 → T004、T006、T007
- T010 → T001、T004
- T011 → T010
- T012 → T011
- T013 → T012

## 状态

- 2026-10-28：立项（spec 052 真实缺口收口，ADR 0013 `Proposed`）
- 2026-10-28：实现完成 —— T001–T012 全部落地并通过验证（clang-format 18 clean / 整解 Win32 exit 0 / 全套件 ALL TESTS PASSED / 1.1.0 装包解包复验 PASS）；**T013 待用户真机验收**，验收通过后 ADR 0013 转 `Accepted`。
