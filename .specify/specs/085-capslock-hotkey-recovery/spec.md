# 085 - CapsLock 打开时快捷键与候选操作恢复可用

## 0. 背景

用户反馈：**CapsLock 处于打开状态时，输入法的快捷键疑似全部失配**（按下后既没有候选动作发生，界面也没有任何响应）。

本条缺陷在任务总账 `Fluxing-二次开发内容清单.md` §4.3 登记为 **T20**，同时作为 ADR 0011 的未决问题 **Q2** 存在（Q2 原文：「What to do about CapsLock (F3)? Three candidates: drop `LOCK_MASK` in the front end, keep it and let the capture emit `Lock+`, or extend the gate to enumerate CapsLock states. **Must be measured before it is chosen.**」）。

本条已完成源码级取证（记录见 §5.3），结论与最初的怀疑**不同**：

- 缺陷不是「少数按键匹配不上」，而是**输入引擎在 CapsLock 打开后不再处理任何按键**；
- 触发条件是「按键事件里带了 CapsLock 锁存标记」，**不是**「用户按下了 CapsLock 键」；
- 现有门禁放行的原因不是「门禁算错了」，而是**门禁枚举前端状态时从不构造 CapsLock 打开的状态**——缺陷维度在门禁里根本不存在。

## 1. 目标

1. CapsLock 打开时，已配置的快捷键与候选操作恢复可用。
2. CapsLock 打开时的普通文字输入行为不变（字母仍输出大写）。
3. 门禁具备发现「按键状态维度缺失」这类缺陷的能力，且该能力本身可证伪（负控）。

## 2. 用户故事

- **US085-A**：作为重度用户，我开着 CapsLock 打字时仍然能用快捷键选候选、翻页、切换中英、切换标点、切换简繁。（验收 SC-085-1 / SC-085-3）
- **US085-B**：作为用户，我打开 CapsLock 想打大写字母时，输入结果仍然是大写，不会变成小写或被吞掉。（验收 SC-085-4）
- **US085-C**：作为维护者，我希望门禁在我改动按键上报逻辑时立刻告诉我「某些按键在某个状态下失配了」，而不是等用户投诉。（验收 SC-085-2）

## 3. 功能需求

- **FR-085-A（US085-A）**：CapsLock 的「锁存状态」不得出现在按键事件的状态字段里。锁存是终端硬件的持续状态，不是本次按键的修饰键。
- **FR-085-B（US085-B）**：CapsLock 这个键本身仍然要作为独立按键上报，不得被吞掉；它在中文模式下切换到英文模式、在英文模式下交还给系统，这两个行为**保持不变**。
- **FR-085-C（US085-B）**：CapsLock 打开时，字母键仍然产出大写字符。该效果由字符本身承载，不依赖状态字段。
- **FR-085-D（US085-C）**：门禁必须把 CapsLock 打开 / 关闭两种状态**分别枚举**，并且必须内建一个「模拟旧行为」的对照，用来证明门禁对本缺陷敏感。

## 4. 非目标

- 不修改输入引擎本体（上游只读引用；宪法 P3）。
- **不让「字母类快捷键」在 CapsLock 打开时也可用**。这是 CapsLock 的固有语义（CapsLock 打开时字母键产出的是大写字符，快捷键写法必须对应那个大写字符）。本条只在文档中写明这一限制与绕行写法，不在本条内实现。
- 不实现「显式带 CapsLock 标记的快捷键写法」（配置里的 `Lock+` 写法在本条之后不会被触发）。
- 不改变 CapsLock 键切换中英模式的行为，不改变「英文模式下 CapsLock 交还系统」的行为。

## 5. 验收标准

### 5.1 行为验收

- **SC-085-1（US085-A）**：CapsLock 打开状态下，门禁判定「全部非字母类快捷键可达」，死绑定数为 0。
- **SC-085-2（US085-C）**：同一门禁内模拟旧行为（把锁存标记塞回状态字段）后，这批快捷键的可达数**必须下降**；否则视为门禁对本缺陷不敏感，判定失败。
- **SC-085-3（US085-A）**：CapsLock 关闭状态下的门禁结果与修复前**逐项一致**（绑定数 / 可达数 / 死绑定数 / 错配数全部不变）。
- **SC-085-4（US085-B）**：CapsLock 打开时字母键产出的字符仍为大写；CapsLock 键本身的上报行为不变。

### 5.2 工程验收

- **SC-085-5**：改动文件通过格式检查；整解构建通过；全套件通过。
- **SC-085-6**：安装包部署后复跑门禁通过（源绿 ≠ 产物绿）。

### 5.3 取证记录（修复前）

| 环节 | 事实 |
|---|---|
| 1 · 输入前端 | CapsLock 打开时，按键事件的状态字段被置上「锁存」标记 |
| 2 · 按键事件搬运 | 修饰键原样搬运，锁存标记原封不动进入输入引擎 |
| 3 · 按键比较 | 输入引擎比较按键事件时做**精确比较**（键码与状态全等），多一位锁存标记即失配 |
| 4 · 英文输入组件 | 该组件读锁存标记；本项目配置下命中后返回「拒绝处理」 |
| 5 · 按键分发 | 「拒绝处理」会**中断整条处理器链**，后续组件（候选选择、翻页、快捷键绑定）全部不再处理该键 |
| 6 · 后果 | CapsLock 打开后，候选选择 / 翻页 / 切中英 / 切标点 / 切简繁**全部失效** |
| 7 · 门禁 | 门禁枚举前端状态时从不设置 CapsLock 锁存位 ⇒ 该状态维度在门禁里不存在 ⇒ 缺陷不可见 |

⇒ 修复方向取**环节 1**（唯一源头）：前端不再上报锁存状态。环节 4 的「拒绝处理」是输入引擎在「按键确实带着锁存标记」时的既定语义，前端不报这个标记，该分支自然不会被命中。

⇒ 同时取 ADR 0011 Q2 的候选③（门禁枚举 CapsLock 状态）：候选①（前端去掉锁存标记）修掉缺陷，候选③让门禁**看得见**这类缺陷；候选②（让配置写出 `Lock+`）放弃（它要改所有既有绑定的语义，且不能修掉环节 5 的中断）。

#### 5.3.1 门禁数字对照（修复前 → 修复后）

| 门禁运行 | 基线（不开 CapsLock） | CapsLock 打开（修复后） | 负控（模拟修复前） |
|---|---|---|---|
| 源门禁 · shipped configuration | `bindings=39 / reachable=39 / dead=0 / mismatches=0 / unused=0` | `caps_bindings=39 / caps_reachable=39 / caps_dead=0 / caps_limited=0 / caps_baseline_dead=0` | `caps_legacy_reachable=0 / caps_legacy_dead=39` |
| 负控 fixture · negative-controls | `bindings=9 / reachable=3 / dead=6 / mismatches=0 / unused=0` | `caps_bindings=6 / caps_reachable=5 / caps_dead=0 / caps_limited=0 / caps_baseline_dead=1` | `caps_legacy_reachable=0 / caps_legacy_dead=6` |
| 抓键 fixture · recorder-output | `bindings=21 / reachable=20 / dead=1 / mismatches=0 / unused=0` | `caps_bindings=21 / caps_reachable=20 / caps_dead=0 / caps_limited=0 / caps_baseline_dead=1` | `caps_legacy_reachable=0 / caps_legacy_dead=21` |
| 部署产物 · 装器载荷 `output/data/build/*.yaml` | `bindings=337 / reachable=337 / dead=0 / mismatches=0 / unused=0` | `caps_bindings=337 / caps_reachable=337 / caps_dead=0 / caps_limited=0 / caps_baseline_dead=0` | `caps_legacy_reachable=0 / caps_legacy_dead=337` |

⇒ **SC-085-2**：负控一列全是 `caps_legacy_reachable=0`，且 `caps_legacy_dead`
等于该场景的绑定总数 ⇒ 模拟修复前时**每一条**绑定都死 ⇒ 门禁对本缺陷敏感，
不是「门禁恒绿」。

⇒ **SC-085-3**：基线一列的数字与修复前**逐项一致**（`bindings=39 / reachable=39 /
dead=0 / mismatches=0 / unused=0`）⇒ 前端不再上报锁存状态没有动到既有语义。

⇒ **SC-085-1**：修复后一列 `caps_dead=0`；`caps_limited` 只统计「基线可达、
但 latch 态下其大写变体才可达」的字母类绑定（ADR 0014 D5 记录在案的限制）。


## 6. 依赖

- ADR 0011（共享单元与按键语义）——本条不改共享单元，但受其 D1/D3 约束。
- ADR 0014（前端不再上报 CapsLock 锁存状态）——本条的实现决策来源。
- 任务总账 T20。
