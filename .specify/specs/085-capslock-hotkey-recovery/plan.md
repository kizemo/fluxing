# 085 实施计划：CapsLock 打开时快捷键与候选操作恢复可用

## 1. 技术上下文

### 1.1 现状（已核验）

**缺陷链（7 环，逐环取证）**

1. `WeaselTSF/KeyEvent.cpp:18-19` —— CapsLock 打开（`keyState[VK_CAPITAL] & TOGGLED`）时 `result.mask |= ibus::LOCK_MASK`。
2. `RimeWithWeasel/RimeWithWeasel.cpp:43-46` —— `expand_ibus_modifier(m) { return (m & 0xff) | ((m & 0xff00) << 16); }`：低 8 位原样保留 ⇒ LOCK 位进入 librime。
3. `librime/src/rime/key_event.h:43-45` —— `operator==` 为 `keycode_ == other.keycode_ && modifier_ == other.modifier_`：**精确比较**。
4. `librime/src/rime/gear/ascii_composer.cc:151-165` —— `if (key_event.caps()) { if (!good_old_caps_lock_ && ...) {...} else { return kRejected; } }`。本 fork `output/data/default.yaml:72-73` 设 `good_old_caps_lock: true` ⇒ **走 else ⇒ `kRejected`**。
5. `librime/src/rime/engine.cc:99-122` —— `ProcessKey` 循环里 `if (ret == kRejected) break;` ⇒ **整条处理器链中断**（`processor.h:19` 注释：`kRejected, // do the OS default processing`）。
6. ⇒ 候选选择 / 翻页 / 切中英 / 切标点 / 切简繁**全部不再发生**。
7. `test/TestHotkeyBindings/TestHotkeyBindings.cpp:128-186` `BuildReachable()` —— 枚举 `vkey × extended × shift × ctrl × alt × keyup`，`state[]` **从不设置 `VK_CAPITAL` 的 TOGGLED 位** ⇒ **门禁没有 CapsLock 维度**，缺陷不可见。

**为什么这条路径在别的平台不会命中**：`AsciiComposer::ProcessKeyEvent` 的 `if (key_event.caps())` 分支，依赖的是「本次按键带着锁存标记」。IBus 体系下锁存状态体现在**产出字符的大小写**上，普通按键事件不带锁存标记，故该分支在 Linux 上几乎不命中。本 fork 的前端把锁存状态也塞进状态字段，于是命中了这条「为 CapsLock 键本身准备」的分支。

**关键排除项（已逐条核对，不受本次改动影响）**

| 项 | 结论 |
|---|---|
| `ascii_composer` 的 `caps_lock_switch_style` | 本 fork `default.yaml:75` 设 `Caps_Lock: clear` ⇒ `caps_lock_switch_style_ = kAsciiModeSwitchClear`（**不是** noop）⇒ `ProcessCapsLock` 会被调用。修复后按键不带锁存标记 ⇒ `if (key_event.caps())` 不进入 ⇒ 无 `kRejected`。 |
| CapsLock 键本身 | `include/KeyNameMapper.h:407-408` 仍映射 `VK_CAPITAL → ibus::Caps_Lock`。修复后 keydown 的状态位为 0（与今天 `^= LOCK_MASK` 撤销后的结果一致）⇒ `ProcessCapsLock` 第一分支 `SwitchAsciiMode(!caps()=true, clear)` ⇒ **中文模式切英文，行为不变**；keyup 为 release ⇒ `kRejected` 交还系统，行为不变。 |
| 字母键大小写 | 修复**不改**字符解析：`include/KeyNameMapper.h:578-598` `ResolveCharacter` 仍走 `ToUnicodeEx` ⇒ CapsLock 打开时仍产出大写字符。 |
| `chord_composer/use_caps` | 本 fork 未配置 ⇒ `use_caps_` 默认 false ⇒ 不读 `caps()`，无影响。 |
| 英文输入组件的「忽略 CapsLock」分支 | 仅在 `!good_old_caps_lock_` 时进入；本 fork 为 true ⇒ 本就不可达。 |

### 1.2 为什么不能用另外两个方案

- **保留锁存标记 + 让配置写出 `Lock+`**（ADR 0011 Q2 候选②）：要让全部既有绑定都改写一遍才能恢复，且**修不掉环节 5 的处理器链中断**——锁存标记在，`kRejected` 就会发生。放弃。
- **只改门禁不改前端**（候选③单独使用）：只能让缺陷「可见」，不能修。采纳为配套（与候选①同时做），但不是替代。

## 2. 技术方案

### 2.1 前端：不再上报 CapsLock 锁存状态（核心）

改动 `WeaselTSF/KeyEvent.cpp`：

- 删除 `if ((keyState[VK_CAPITAL] & TOGGLED) != 0) result.mask |= ibus::LOCK_MASK;`
- 删除 `if (vkey == VK_CAPITAL && !kinfo.isKeyUp) { result.mask ^= ibus::LOCK_MASK; }`

⚠️ **两处必须一起删**：`^=` 是异或。只删置位而留撤销，会让 CapsLock 键自身的状态位凭空变成 `LOCK`（`0 ^ LOCK = LOCK`），从而把「切英文」翻成「切中文」。这是本方案唯一的实现陷阱，必须在代码注释里写明。

`include/KeyEvent.h` 的 `LOCK_MASK` 常量**保留**（`test/TestHotkeyBindings` 的掩码自检仍在用它验证 `expand_ibus_modifier` 的一致性；删常量会牵连既有门禁）。

`include/KeyNameMapper.h:621-624` 的注释（「是否上报 CapsLock 仍待 ADR 0011 Q2 / T20 决定」）更新为指向 ADR 0014 的定论。

### 2.2 门禁：新增 CapsLock 维度 + 负控（坐实与回归）

改动 `test/TestHotkeyBindings/TestHotkeyBindings.cpp`：

1. `BuildReachable()` 增加 `caps` 维度：`state[VK_CAPITAL] = 0x01`（TOGGLED）枚举开/关两态，产出两套可达集合。
2. 门禁新增判定：**每条非字母类绑定，在 CapsLock 打开态下也必须可达**（死绑定数必须为 0）。
3. **负控**：门禁内再建一套「模拟旧行为」的可达集合（把锁存标记塞回状态字段），断言同一批绑定的可达数**必须下降**——否则门禁对该缺陷不敏感，判定失败。
4. 门禁输出新增字段：`caps_bindings` / `caps_reachable` / `caps_dead` / `caps_legacy_reachable` / `caps_legacy_dead`，与既有 `SUMMARY|` 行并列，既有字段的值不变（SC-085-3）。

**字母类绑定的处理**：CapsLock 打开时字母键产出大写字符，故字母类绑定在打开态下天然不可达。门禁把它们单列为 `caps_limited` 并断言该集合恰为「键码是 ASCII 字母的绑定」——新增字母类绑定时列表自动跟随，且失败信息会点名。

**门禁建模的两条依据（均来自上游只读源码）**

- 精确比较：`librime/src/rime/key_event.h:43-45`。
- 处理器链中断：`librime/src/rime/gear/ascii_composer.cc:151-165` + `librime/src/rime/engine.cc:105-107`。门禁在「CapsLock 打开态」下额外扣除被英文输入组件 `kRejected` 拦掉的键（`Shift+Ctrl` 组合、`Alt`、`Super` 除外，见 `ascii_composer.cc:60-63`）。

### 2.3 顺带修掉的地雷

- `include/KeyNameMapper.h` 的旧注释与 ADR 0011 Q2 的悬空引用（改为指向 ADR 0014）。
- ADR 0011 的 Q2 由「未决」改为「已决（见 ADR 0014）」。

## 3. 里程碑

### Q1 · 前端改造 + 门禁改造（核心）

- 删除锁存标记的置位与撤销；同步更新共享单元注释。
- 门禁加 CapsLock 维度、负控、`caps_limited` 分类、输出字段。

### Q2 · 验证与发布

- 整解构建、全套件、格式检查全绿。
- 打包 1.1.0 并对部署产物复跑门禁。
- CHANGELOG、任务总账、交接卡同步；提交并推送；CI 四 job 终态确认。

## 4. Constitution Check

| 原则 | 落点 |
|---|---|
| **I · 先定意图** | R1 满足：动手前先写明意图与验收——意图＝前端不再上报锁存状态；验收＝SC-085-1…SC-085-6 写在 `spec.md` §5，且每条可复算。 |
| **II · 测试支撑 / R6 证据先行** | R6 满足：本条**先取证再改码**——7 环链条逐环落到文件与行号（§1.1），修复前后的门禁数字贴进 `spec.md` §5.3；门禁本体新增断言 + 负控。R2 满足：`spec.md` 不含技术栈词，模块名与库名只出现在 `plan.md`。 |
| **III · ≥3 文件需 spec/plan/tasks** | 满足：本条新建 `085-capslock-hotkey-recovery/{spec,plan,tasks}.md`；改动文件为 4 个（`WeaselTSF/KeyEvent.cpp`、`include/KeyNameMapper.h`、`test/TestHotkeyBindings/TestHotkeyBindings.cpp`、`docs/adr/0014-*.md`）。 |
| **IV · 澄清** | 满足：无歧义残留——「字母类快捷键在 CapsLock 打开时是否也要可用」已由用户原指令边界回答（用户报的是「快捷键全部失配」，不是「字母快捷键」），并在 `spec.md` §4 与 ADR 0014 写明限制与绕行写法。 |
| **V · 增量交付** | 满足：Q1（改造）与 Q2（验证发布）分界清晰，可单独回退；本条不夹带其它功能。 |

**必披露项**

| 披露项 | 结论 |
|---|---|
| 是否触及遗留用户数据路径 | **否**。只改按键上报与门禁，不读写用户词库/配置目录，不涉及 leveldb 与 `StartMaintenance`/`EndMaintenance` 包裹。P6 豁免不适用。 |
| 是否新增 IPC 消息类型 | **否**。本条不触碰 `include/WeaselIPCData.h` 与协议，server / TSF / deployer 三端无需同步。 |
| 用户可见变更是否写 CHANGELOG | **是**。CapsLock 打开时快捷键恢复可用属用户可见修复，写入 CHANGELOG `### 主要更新`。 |
| 是否修改 `librime/` | **否**（宪法 P3）。上游源码仅作只读取证依据。 |
| 是否新增第三方依赖 | **否**。 |

## 5. 未决问题

- **Q-085-1**：字母类快捷键在 CapsLock 打开时不可达。若用户确有此需求，绕行写法是显式写出大写变体（如配置里写 `Control+A`）。是否要把这条做成「门禁自动建议大写变体」的提示，**留到后续条目**，本条不实现。
- **Q-085-2**：本 fork 的 `output/data/default.yaml` 设 `good_old_caps_lock: true`，其语义是「英文模式下 CapsLock 仍是大写锁定」。该值与本条修复**正交**，本条不动它。

## 6. 提交纪律

- Conventional Commits + 模块前缀；本条改动跨 `WeaselTSF` / `test` / `docs` ⇒ 拆两个提交：`fix(WeaselTSF): …`（含测试）与 `docs(fluxing): …`（spec/ADR/账本/交接卡）。
- 提交前：`git status -sb` + `git diff --stat`（防 `weasel.props` / `env.bat` / `*.obj` 误提交），格式检查、整解构建、全套件、CHANGELOG 四项门禁全绿。
- commit message 以 UTF-8 无 BOM 写文件后 `git commit -F <file>`。
