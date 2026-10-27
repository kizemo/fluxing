# 083 - 设置界面抓键按「实际产生的字符」生成（ADR 0011）

> Scope: 让设置界面抓到的组合键，产出的 `accept:` 键名**一定能被 librime 解析、且前端真的会发这个键**。
> 关联: ADR 0011（决策，**Proposed**）、ADR 0009（TSF 前端发的是字符不是物理键）、
> ADR 0010、spec 077 / 081 / 082、L148 / L149、清单 T2。

## 0. Background

### 用户要什么

在设置界面按一个组合键，它就该被记住成**按下去就能触发**的快捷键。
现在不是这样：界面记下的键名，在真实输入时**永远不触发**，而且界面不告诉你。

### 问题已被门禁坐实（2026-10-28 对照实验）

把 `output/data/default.yaml` 的 `Control+Shift+numbersign` 改成 `Control+Shift+9`，
丢给快捷键门禁 `TestHotkeyBindings`：

```
BINDING|...|265|always|Control+Shift+9||toggle|DEAD_KEYCODE|
  the front end never sends keycode 0x0039 (9) with modifier 0x0005;
  the front end sends 0x0028 (parenleft) for that chord;
  keys that do fire with the same modifier: ampersand, asciicircum,
  asciitilde, asterisk, at, bar, braceleft, braceright, colon, dollar,
  exclam, greater, ...
SUMMARY|bindings=27|reachable=26|dead=1|known_dead=0|...|unused=0
RESULT|FAIL
```

同一份文件里的 `Control+Shift+numbersign` 是 `REACHABLE`。
⇒ **前端在该组合下发的 keycode 是 `0x0028`（`(`），不是 `0x0039`（`9`）**。

### 抓键为什么必然踩这个坑

`FluxingConfigEditor/KeyRecorder.cpp:22-88` 的 `MapKeyCodeToLibrimeName(vk)` 是
**纯虚拟键码映射**：

- 数字 `vk 0x30..0x39` → 键名 `0`..`9`；
- 字母 `vk 0x41..0x5A` → 键名 **小写** `a`..`z`；
- OEM 键 → 固定表（`semicolon` / `plus` / `grave` / ...）；
- 修饰键由 `GetAsyncKeyState` 拼接，且 `win` 被**错拼成 `Control+`**（:118）。

四个缺陷：

| # | 缺陷 | 后果 |
|---|---|---|
| D-a | Shift 修饰的**数字/符号**名错了字符 | `Ctrl+Shift+9` → `Control+Shift+9`（实际是 `(`）⇒ 死 |
| D-b | Shift 修饰的**字母**被强制小写 | `Ctrl+Shift+A` → `Control+Shift+a` ⇒ 死 |
| D-c | **Win 键**被静默改写成 `Control+` | 产出与用户意图完全不同的绑定 |
| D-d | **从不查当前布局**（`MapVirtualKeyW(MAPVK_VK_TO_SSC)` + `GetKeyNameTextW` 用的是系统默认布局） | 非美式布局下名不副实 |

### 两条抓键实现，互相矛盾

| 生产点 | 位置 | 说明 |
|---|---|---|
| ① | `FluxingConfigEditor/KeyRecorder.cpp:22-88` + `:103-130` | 设置界面「抓键」对话框 |
| ② | `WeaselServer/ShortcutSettings.cpp:622-699`（`VKeyToName`）+ `:701-718`（`NormalizeCombo`） | 快捷键设置面板的第二套独立映射 |

② 的 `VKeyToName` 把 `VK_OEM_PLUS` 映射成 `equal`，① 的 `:42` 映射成 `plus` ⇒
**两套实现已经漂移**。

落盘链路：① → `WeaselDeployer/HotkeyEditorDialog.cpp:281` →
`FluxingConfigEditor/HotkeyBinding.cpp:169`（`b.accept = rec.accept()`）→ yaml 的 `accept:`。

### 三条必须遵守的约束（已核实的源码事实）

- **F1 — 带修饰键的绑定只能用 Rime 有名字的字符。**
  `librime/src/rime/key_event.cc:51-82`：单字符 `accept:` 直接取字符码；
  带修饰键的走 `RimeGetKeycodeByName`，而它（`librime/src/rime/key_table.cc:2026-2033`）
  对 `keys_by_keyval` 做**纯 strcmp** —— 该表**只有大写字母**（`A`–`Z` = `0x41`–`0x5A`），
  **没有小写**。
  ⇒ `Control+f` 连解析都失败；**只有 `Control+Shift+F` 可表达**。
  这正是内置快捷键表用 `Control+Shift+F` 的原因。
- **F2 — 前端在算字符前会清掉 Ctrl / Alt。**
  `WeaselTSF/KeyEvent.cpp:44-51`：`ToUnicodeEx` 前把 `VK_CONTROL` / `VK_MENU` 置零
  ⇒ 字符的大小写**只由 Shift 决定**。
  ⇒ 不复刻这条规则就分不清 `Ctrl+F` 和 `Ctrl+Shift+F`。
- **F3 — CapsLock 打开时前端会多带一位 `LOCK_MASK`（疑点，待实测）。**
  `WeaselTSF/KeyEvent.cpp:18-19` 在 `keyState[VK_CAPITAL]` 的 toggled 位为 1 时
  `|= ibus::LOCK_MASK`（`0x2`，`include/KeyEvent.h:225`）；
  `RimeWithWeasel/RimeWithWeasel.cpp:44-46` 的 `expand_ibus_modifier` 保留低 8 位。
  而 librime **精确比较 modifier**（`librime/src/rime/key_event.h:43-44`）。
  ⇒ **CapsLock 打开期间，所有没写 `Lock+` 的绑定都可能失配。**
  ⇒ 记为清单 **T20**；**必须先实测再决定怎么修**（ADR 0011 Q2）。

## 1. Requirements

### R1 单一来源

「字符 → Rime 键名」与「组合键 → 字符」两份逻辑放进**一个共享、无 GUI 依赖**的单元，
抓键与门禁都消费它。**不允许两处各写一份**（② 已经漂移过一次）。

### R2 实时布局 + 复刻前端换算

抓键得到的字符，必须按**当前输入布局**算，且换算规则与
`WeaselTSF/KeyEvent.cpp` **逐条一致**（清 Ctrl/Alt、Shift 决定大小写、
特殊键走 `TranslateKeycode` 的固定表）。

### R3 修饰键按前端掩码精确输出

输出用户实际按下的修饰键集合（Shift / Control / Alt）。
**Win 键直接拒绝并提示**，不静默改写。
若该组合的字符 librime 无名（见 F1，如 `Ctrl`+小写字母）⇒ **拒绝并给出可操作提示**
（例如「Ctrl+字母请按住 Shift」），**绝不产出死绑定**。

### R4 门禁即验收器

改造后界面能产出的每一条绑定，都必须被 `TestHotkeyBindings` 判为 `REACHABLE`。
测试写一份「抓键产物」的 scratch schema，断言其 `dead=0`。

### R5 不动门禁规则、不动 librime

门禁的「可达」判定本身是对的，坏的是抓键。
门禁规则**不放宽**（空基线保持最严）；`librime/` 保持只读（宪法 P3）。

### R6 两条抓键路径统一

①（`KeyRecorder.cpp`）与 ②（`ShortcutSettings.cpp` 的 `VKeyToName` / `NormalizeCombo`）
都切到 R1 的共享单元，消除 `equal` vs `plus` 这类矛盾。

### R7 不新增构建目标

共享单元为 **header-only**，放在 `include/`（门禁工程已经编译生产的
`WeaselTSF/KeyEvent.cpp`，可直接复用），**不新建 .vcxproj**。

## 2. 验收方式

| 项 | 方式 | 通过标准 |
|---|---|---|
| 门禁（仓库源） | `TestHotkeyBindings.exe --baseline test\TestHotkeyBindings\known-dead-bindings.txt output\data\default.yaml output\data\user-custom\*.custom.yaml` | `SUMMARY\|bindings=39\|reachable=39\|dead=0` + `RESULT\|PASS`（**数字不得变差**） |
| 门禁（部署产物） | 同上，路径换成 `<user_data_dir>\build\*.schema.yaml` | `SUMMARY\|bindings=120\|reachable=120\|dead=0` + `RESULT\|PASS` |
| 抓键产物专项 | 新增测试：`TestHotkeyBindings` 加 `fixtures/recorder-output.yaml` + `.expect.txt` | 抓键产物全部 `REACHABLE`；负控 `Control+Shift+9` 必须 `DEAD_KEYCODE` |
| 布局感知 | 门禁 `--layout` 指向非美式布局时重跑 | 键名随布局变化且仍 `REACHABLE` |
| 全套件 | `cmd /c scripts\test-infra\run-test-suite.bat` | `=== ALL TESTS PASSED ===` |
| 默认快捷键 | `test\TestDefaultHotkeys\Release\TestDefaultHotkeys.exe` | `Passed: 89 / 89` |
| 格式门禁 | `clang-format --style=file:.clang-format --dry-run --Werror <每个触及的 .cpp/.h>` | 全部 clean（CI lint 按整文件判） |

## 3. 已知风险 / 未决

- **T20（F3）未实测**：CapsLock 打开时快捷键是否真的全部失配。
  **先测量再修**，不要在没有证据时改前端行为。
- 布局依赖：抓键取的是**打开对话框那一刻**的布局；之后切换布局不会回改已记住的名字。
- `FluxingConfigEditor/HotkeyBinding.cpp:96-137` 的 `NormalizeAccept`
  把 `comma`→`Comma`、`space`→`Space`，**与它自己的文档 `HotkeyBinding.h:80-83` 相反**
  （librime 只认小写名），且**全仓无调用点**。
  ⇒ **保持不接线**；若将来接线，必须先修这个反过来的映射。
- `WeaselServer/ShortcutSettings.cpp:339-402` 的内置表是**硬编码 accept 串**（不是生成），
  其中 `Control+Shift+F`（:347/:351）符合 F1 口径，**不要顺手改**。

## 4. 实施步骤（建议顺序）

1. **M1 共享单元**：新增 `include/KeyNameMapper.h`（header-only）+ 键名表
   （来源 `librime/src/rime/key_table.cc`，与门禁 `keysyms_generated.inc` **同源同生成器**）。
2. **M2 抓键改造**：`KeyRecorder.cpp` 换成共享函数；`:118` 删掉 `win→Control+`。
3. **M3 第二条路径**：`ShortcutSettings.cpp` 的 `VKeyToName` / `NormalizeCombo` 切到共享单元。
4. **M4 测试**：`fixtures/recorder-output.yaml` + `.expect.txt`；负控钉死 `Control+Shift+9` 仍死。
5. **M5 门禁**：仓库源 + 部署产物各跑一遍，数字不得变差。
6. **M6 打包**：新版本号（0.21.0.10）→ `build.bat weasel installer` → 交用户实机验收。

> **每步都要 clang-format 干净**（CI lint 按整文件判）；**`style(fluxing):` 与功能改动分两个提交**
> （先例 `4ab52db` / `a023ee1`）。
