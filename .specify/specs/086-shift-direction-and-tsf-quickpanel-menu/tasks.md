# 086 · Tasks · 左右 Shift 上屏候选方向修正 + 输入法弹出菜单补齐「快速面板常驻」

> 配套：[spec.md](spec.md)、[plan.md](plan.md)、[ADR 0015](../../../docs/adr/0015-shift-direction-single-source-and-tsf-popup-menu.md)。

## Phase 1 · Shift 方向同源化（Q1）

- [x] T001 [P1] [US086-C] `include/ShiftStateMachine.h` 新增 `EncodeShiftIsLeft` / `DecodeShiftIsLeft` 一对纯函数，并在注释里写明线缆约定（0=左、1=右）与本次事故原因
- [x] T002 [P1] [US086-A] `WeaselIPC/WeaselClientImpl.cpp` 两处 `is_left ? 0u : 1u` 改调 `fluxing::EncodeShiftIsLeft(is_left)`（并补 `#include <ShiftStateMachine.h>`）
- [x] T003 [P1] [US086-A] `WeaselIPCServer/WeaselServerImpl.cpp` 两处 `(wParam & 1u) != 0` 改调 `fluxing::DecodeShiftIsLeft(wParam)`（**修反解**，并订正那段把约定写反的注释）
- [x] T004 [P1] [US086-C] `test/TestShiftIPCStateMachine/TestShiftIPCStateMachine.cpp` 新增 `Test_EncodeDecode_RoundTrip()`：断言**位值本身**（左编码=0、右编码=1、`Decode(0)==true`、`Decode(1)==false`），使断言对「两端一起错」也能证伪；并在 `main()` 中调用

## Phase 2 · 弹出菜单补项（Q1）

- [x] T005 [P1] [US086-B] `include/resource.h` 新增 `#define ID_QUICKPANEL_ALWAYS_SHOW 40019`，注释写明「与服务端自己的头文件取值必须一致」
- [x] T006 [P1] [US086-B] `WeaselTSF/WeaselTSF.rc` 三个弹出菜单（简 / 繁 / 英）各在「快捷设置栏」之后插入 `MENUITEM "快速面板常驻 (&A)", ID_QUICKPANEL_ALWAYS_SHOW`；繁体块用「快速面板常駐」；改完复核「三个语言块各插一行 + 缩进对齐 + 无乱码」

## Phase 3 · 验证（Q2）

- [x] T007 [P1] [US086-C] 格式检查：基线干净的文件保持干净（`include/ShiftStateMachine.h`、`WeaselIPC/WeaselClientImpl.cpp`、`WeaselIPCServer/WeaselServerImpl.cpp`、测试文件）；`include/resource.h` 基线本就有历史违规 ⇒ 只保证新增行与文件既有风格一致，不做全量重排
- [x] T008 [P1] [US086-A] 整解构建（Release / Win32）+ 64 位前端补编，均 exit 0
- [x] T009 [P1] [US086-C] 全套件复跑通过（含新增的 `Test_EncodeDecode_RoundTrip`）
- [x] T010 [P2] [US086-B] 资源级复核：对构建产物做 PE 资源树检查，确认 `weasel.dll` / `weaselx64.dll` 的 MENU 105 / 108 / 109 **已含**「快速面板常驻」（修复前为 −1）
- [x] T011 [P2] [US086-B] 重新打包 1.1.0，并对**部署产物**复跑门禁（源绿 ≠ 产物绿）+ 解包校验

## Phase 4 · 文档与发布（Q2）

- [x] T012 [P2] [US086-B] CHANGELOG `### 主要更新` 写两条用户可见修复说明
- [x] T013 [P2] [US086-C] 任务总账登记本轮两条缺陷条目 + 交接卡新增 §17
- [x] T014 [P2] [US086-C] ADR 0015 落 `Proposed`（D1–D7）
- [x] T015 [P2] [US086-A] lessons-learned 记两条事故（单端位约定漂移 / 两份菜单资源只改一份）
- [x] T016 [P2] [US086-A] 提交（`fix(fluxing):` + `docs(fluxing):` + `chore(fluxing):`）并推送 `kizemo/Fluxing`，CI 四 job 终态确认
- [ ] T017 [P2] [US086-A] 交用户真机验收：左 Shift 上屏第 2 个候选、右 Shift 上屏第 3 个候选；输入法弹出菜单出现「快速面板常驻」且勾选后快捷设置栏不再自动隐藏。验收通过 ⇒ ADR 0015 `Proposed` → `Accepted`
- [ ] T018 [P2] [US086-B] 交用户真机验收：重启完成 64 位前端换装后复测反馈 1 / 2（CapsLock 打开时候选窗与快捷键）

## 状态

- 2026-10-28：立项（spec 086，ADR 0015 `Proposed`）。
- 2026-10-28：T001–T016 全部落地并通过验证 —— 整解 Win32 exit 0 / 64 位前端补编 exit 0 /
  全套件 `=== ALL TESTS PASSED ===` / 格式检查基线不劣化 / PE 资源级复核确认三份弹出菜单已含该项 /
  装器载荷复跑一致 / 装包 `PKG-VERIFY=PASS`；**T017 / T018 待用户真机验收**。
