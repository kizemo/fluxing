82 任务清单

> 勾选框与 [plan.md](plan.md) 的任务编号一一对应。
> `[x]` = 已完成；`[ ]` = 下一阶段待办。
>
> 本轮（2026-10-27）**T1 全部落地并全绿**；**只剩 T3 真机按键验收**。
> 判定表见 [spec.md §2.1](spec.md)，实测数字：`TestShiftIPCStateMachine` **28 PASS / 0 FAIL**。

## M0 · 拍板与文档债

- [x] T0.1 拍板 ADR 0010 的三个参数：P1 **默认开** / P2 **只切当前会话** / P3 **候选窗吃掉按下，无双击逃生门**（2026-10-27）⇒ ADR 状态 `Proposed` → `Accepted`
- [x] T0.2 确认「yaml 路径不可达」的根因链：`WeaselTSF/KeyEventSink.cpp:44-58` 把 `Shift_L`/`Shift_R` 整段吃掉 ⇒ librime 看不到 Shift ⇒ `ascii_composer/switch_key` 与 `key_binder` 的 Shift 条目**构造上全死**
- [x] T0.3 确认 L18 的历史阻塞已失效：前端从 spec 077 起不再把 Shift 转给 librime ⇒ release 误匹配这一类 bug 不可能再发生
- [x] T0.4 确认「裸 Shift 已被占用」不构成硬冲突：`Context::Select()` 在 composition 为空时 `return false`（`librime/src/rime/context.cc:113-115`）⇒ 无菜单时选第 2 候选本来就是空操作
- [x] T0.5 《Rime-设置说明书-重点快捷键.md》§6.8 / §9.3 / §10 的过时说法改正（md + html 两份）：
  - §6.8 「默认配置下 `Shift_L` 单独按下就能切 ASCII」⇒ 改成「上游是这样，**本 fork 不是**」
  - §6.8 警告框归因「release 事件 modifier 比较 bug」⇒ 改成「前端整段吃 Shift」
  - §9.3 「裸按 Shift = 选第 2/3 候选」⇒ 补上 0.21.0.9 起的消歧行为
  - §9.3 「`key_binder` 的 `accept: Shift_L` 永远不匹配 release」⇒ 注明该条对本 fork 已无意义
  - §10 常见误区表「单独 Shift 切不了中英 → 目前**做不到**」⇒ 改成「0.21.0.9 起已修好」

## M1 · 服务端实现

- [x] T1.1 `include/ShiftStateMachine.h`：新增 `enum class ShiftUpAction { kNone, kSelectCandidate, kToggleAsciiMode }` + 纯函数 `DecideShiftUpAction(down_recorded, intervening_key, menu_open, feature_enabled)`（无 rime_api / IPC / GUI 依赖）
- [x] T1.2 `include/RimeWithWeasel.h`：新增成员 `m_bare_shift_toggles_ascii`（插在 `m_global_ascii_mode` 与 `m_show_notifications_time` 之间）
- [x] T1.3 `RimeWithWeasel.cpp` 构造初始化列表加该成员，默认 `true`（ADR 0010 P1）
- [x] T1.4 `RimeWithWeasel.cpp` `Initialize()`：用 `rime_api->config_get_bool(&config, "bare_shift_toggles_ascii", ...)` 读取；**键缺失 ⇒ 保持默认 `true`**（老用户升级即生效）
- [x] T1.5 `RimeWithWeasel.cpp` `ShiftUp()`：读 `status.is_ascii_mode` + `ctx.menu.num_candidates`，算 `menu_open`
  - ⚠️ 谓词 = `num_candidates > 0 && !is_ascii_mode`，**不能只看 `is_composing`**：英文模式下 librime 仍保留 1 个候选的 raw 段（`ascii_segmentor.cc:20`），且 `key_binder.cc:255` 自己插 `has_menu` 上下文时也要 `&& !ascii_mode`
- [x] T1.6 `RimeWithWeasel.cpp` `ShiftUp()`：按动作分支 —— `kSelectCandidate` → `select_candidate(idx)` + `_UpdateUI`；`kToggleAsciiMode` → `get_option`/`set_option("ascii_mode", !cur)` + `_UpdateUI`
  - ⚠️ **绝不走 `SetOption()`** —— 它会写 `m_global_ascii_mode` 全局镜像，违反 ADR 0010 P2（只切当前会话）
  - 作用域与 yaml 的 `Shift+space` 逐字对齐；`global_ascii: true` 时由既有 `_UpdateUI` 传播逻辑接管，不开特例
- [x] T1.7 `output/data/weasel.yaml`：`[global settings]` 新增 `bare_shift_toggles_ascii: true`（与 `global_ascii` 同一 `config_get_bool` 机制）
- [x] T1.8 `test/TestShiftIPCStateMachine`：+6 个测试函数 / +11 条断言
  - #12 `Test_BareShift_NoMenu_TogglesAsciiMode` —— 无菜单 ⇒ 切 ascii_mode
  - #13 `Test_BareShift_MenuOpen_SelectsCandidate` —— 有菜单 ⇒ 选候选
  - #14 `Test_FeatureDisabled_AlwaysSelectsCandidate` —— 开关关 ⇒ 退回选候选
  - #15 `Test_NoDown_NoAction` —— 没有 ShiftDown ⇒ 什么都不做
  - #16 `Test_Intervening_NoAction` —— 中间有别的键 ⇒ 什么都不做
  - #17 `Test_DecisionTable_Exhaustive` —— 16 种组合穷举
  - ⇒ 合计 **17 个测试函数 / 28 条断言**，`Result: 28 PASS / 0 FAIL`
- [x] T1.9 修掉测试文件里过时的 TDD 注释（「the skeleton…has empty implementations, so all 11 tests must fail here at HEAD ff03ad8」已不成立）⇒ 改成「实现是真的，PASS 数 17（spec 077）→ 28（ADR 0010）」

## M2 · 打包与门禁

- [x] T2.1 `env.bat`：`PRODUCT_VERSION=0.21.0.9` / `FILE_VERSION=0.21.0.9` / `WEASEL_BUILD=9` + 0.21.0.9 的 rem 说明
- [x] T2.2 `cmd /c build.bat weasel installer` ⇒ 产出 `output/archives/fluxing-0.21.0.9-installer.exe`
- [x] T2.3 AGENTS.md §2.5 静默装机 smoke test ⇒ 结果写进 commit message
- [x] T2.4 全套件 `cmd /c scripts\test-infra\run-test-suite.bat` ⇒ `=== ALL TESTS PASSED ===`
- [x] T2.5 快捷键门禁**扫部署产物**（`<user_data_dir>\build\*.schema.yaml`）⇒ `dead=0` + `RESULT|PASS`
- [x] T2.6 `CHANGELOG.md` 写 0.21.0.9 节（User-visible / Root cause / Fix / 验收）

## M3 · 真机按键验收（**未做** —— 只能人手敲）

- [ ] T3.1 中文模式、候选窗**没开**，单独按一下 `Shift` ⇒ 切英文
- [ ] T3.2 再单独按一下 `Shift` ⇒ 切回中文
- [ ] T3.3 中文模式、**先打拼音**让候选窗弹出，单独按一下 `Shift` ⇒ **仍选第 2 个候选**，**不**切模式（spec 077 行为不变）
- [ ] T3.4 英文模式、候选窗**没开**，按一下 `Shift` ⇒ 切回中文
- [ ] T3.5 任意状态下 `Shift+X` ⇒ 只出 `X`，**不**切模式
- [ ] T3.6 两个窗口各开一个会话，在 A 里按裸 Shift ⇒ 只切 A，B 不变

## M4 · 收口

- [x] T4.1 `CHANGELOG.md` 0.21.0.9 节
- [x] T4.2 《Fluxing 二次开发内容清单.md》更新：§2 补 C8、§3 删 P-C1、§4.1 T1 标完成、§4.3 T17/T18 改版本、§8 门禁表补 `TestShiftIPCStateMachine`、§9 时间轴补 0.21.0.9、§10 spec 索引 65 份、§11 CHANGELOG 83 节 / ADR 6 份
- [x] T4.3 `CONTEXT.md` §4 加 invariant 17「Bare Shift is disambiguated by the candidate menu, never by timing」
- [x] T4.4 `Rime-设置说明书-重点快捷键.{md,html}` 同步（见 T0.5）
- [x] T4.5 新建本 spec 目录 `.specify/specs/082-bare-shift-ascii-toggle/`
- [x] T4.6 commit + push（Conventional Commits，单一 scope）—— `a023ee1`（`style(fluxing):` clang-format 前置，CI lint 门禁要求）+ `258e708`（`feat(fluxing):` 功能本体），推送到 `kizemo/Fluxing`

## 附：本轮未做、明确留到后面的

- [ ] T2.13 / T14：`build (xmake)` 的 arm64 目标缺失（librime Win32-only 约束待评估）
- [ ] T15：`sandbox-gate` CI job 排队 —— 缺带 `sandbox` 标签、支持嵌套虚拟化的 Win11 自托管 runner
- [ ] T18：0.21.0.9 进 `release/` 发布链
