83 任务清单

> 勾选框与 [plan.md](plan.md) 的任务编号一一对应。
> `[x]` = 已完成；`[ ]` = 下一阶段待办。
>
> 本轮（2026-10-28）**只完成 M0（调查与决策）**；**M1–M6 全部待做**。
> 决策见 [ADR 0011](../../../docs/adr/0011-capture-keys-by-produced-character.md)（`Proposed`）。

## M0 · 调查与决策

- [x] T0.1 死绑定坐实（对照实验）：把 `output/data/default.yaml` 副本里的 `Control+Shift+numbersign` 改成 `Control+Shift+9` 后跑门禁 ⇒ `DEAD_KEYCODE`（前端发 `0x0028 parenleft`，不是 `0x0039`）+ `RESULT|FAIL`，`bindings=27 reachable=26 dead=1`；同文件 `Control+Shift+numbersign` = `REACHABLE`
- [x] T0.2 F1 核验：librime `keys_by_keyval` **只有大写字母 A–Z**，`RimeGetKeycodeByName` 是纯 `strcmp` ⇒ `Control+f` 连解析都失败，只有 `Control+Shift+F` 可表达（`librime/src/rime/key_table.cc:2026-2033`）
- [x] T0.3 F2 核验：前端 `ToUnicodeEx` 前清掉 `VK_CONTROL` / `VK_MENU` ⇒ 字符大小写只由 Shift 决定（`WeaselTSF/KeyEvent.cpp:44-51`）
- [x] T0.4 F3 记为疑点：CapsLock 打开时前端额外带 `LOCK_MASK`（`WeaselTSF/KeyEvent.cpp:18-19`），而 librime 精确比较 modifier ⇒ **疑似全部失配** ⇒ 记为清单 **T20**，**先实测再修**
- [x] T0.5 两条抓键路径漂移坐实：`KeyRecorder.cpp` 把 `VK_OEM_PLUS` 映射成 `plus`，`ShortcutSettings.cpp:622-699` 映射成 `equal`；`KeyRecorder.cpp:118` 把 `win` 错拼成 `Control+`
- [x] T0.6 拍板 ADR 0011 的 D1–D6，写出 spec 083 的 R1–R7、验收表与实施步骤

## M1 · 共享单元 `include/KeyNameMapper.h`

- [ ] T1.1 新增 header-only `include/KeyNameMapper.h`（**不新建 .vcxproj**）
- [ ] T1.2 键名表与 `test/TestHotkeyBindings/keysyms_generated.inc` **同源同生成器**
- [ ] T1.3 提供「字符 → 键名」「组合键 → 字符」两个方向的纯函数（无 GUI / 无 rime_api 依赖）
- [ ] T1.4 复刻 F2（清 Ctrl/Alt、Shift 定大小写），与 `WeaselTSF/KeyEvent.cpp` 逐条对齐
- [ ] T1.5 librime 无名的字符返回「不可表达」，**绝不产出死绑定**

## M2 · 抓键改造 `FluxingConfigEditor/KeyRecorder.cpp`

- [ ] T2.1 `MapKeyCodeToLibrimeName`（:22-88）换成共享单元
- [ ] T2.2 删掉 `:118` 的 `win → Control+` 误拼，改为**拒绝 + 提示**
- [ ] T2.3 修饰键按前端掩码精确输出（Shift / Control / Alt）
- [ ] T2.4 `Ctrl`+小写字母 ⇒ 拒绝并提示「请按住 Shift」

## M3 · 第二条路径 `WeaselServer/ShortcutSettings.cpp`

- [ ] T3.1 `VKeyToName`（:622-699）切到共享单元
- [ ] T3.2 `NormalizeCombo`（:701-718）切到共享单元，消除 `equal` vs `plus` 矛盾
- [ ] T3.3 **不动**内置表（:339-402，硬编码 accept 串，`Control+Shift+F` 保持原样）

## M4 · 测试（抓键产物专项）

- [ ] T4.1 `test/TestHotkeyBindings/fixtures/recorder-output.yaml`
- [ ] T4.2 同目录 `.expect.txt`（逐条 `REACHABLE`）
- [ ] T4.3 负控：`Control+Shift+9` 必须仍判 `DEAD_KEYCODE`（防止门禁被改瞎）

## M5 · 门禁复跑（仓库源 + 部署产物）

- [ ] T5.1 仓库源：`SUMMARY|bindings=39|reachable=39|dead=0` + `RESULT|PASS`（**数字不得变差**）
- [ ] T5.2 部署产物：`bindings=120|reachable=120|dead=0` + `RESULT|PASS`
- [ ] T5.3 全套件 `=== ALL TESTS PASSED ===`；`TestDefaultHotkeys` `Passed: 89 / 89`
- [ ] T5.4 clang-format 整文件 clean

## M6 · 打包与实机验收

- [ ] T6.1 版本号升 `0.21.0.10`（`env.bat`，gitignored）
- [ ] T6.2 `build.bat weasel installer` ⇒ `output\archives\fluxing-0.21.0.10-installer.exe`
- [ ] T6.3 交用户实机验收（抓键可用性 + 原有快捷键不回归）
- [ ] T6.4 实测通过后 ADR 0011 `Proposed` → `Accepted`

## T20 联动（CapsLock 疑点）

- [ ] T7.1 **实测**：CapsLock 打开时，已配置的快捷键是否真的全部失配
- [ ] T7.2 依据实测结果，在 ADR 0011 Q2 的三个候选中选一个并落到 spec / 清单
- [ ] T7.3 若选「前端清 `LOCK_MASK`」⇒ 独立 `fix(fluxing):` 提交 + `style(fluxing):` 前置提交 + 单测
