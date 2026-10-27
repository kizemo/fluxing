# 082 实施计划：裸 Shift 切换中英文（ADR 0010 上下文消歧）

> 配套：[spec.md](spec.md)、[ADR 0010](../../../docs/adr/0010-bare-shift-toggles-ascii-mode.md)、
> ADR 0007 / ADR 0009、spec 077 / spec 081、L18 / L19 / L21 / L148。
>
> 本轮（2026-10-27）已完成的部分标 **[已做]**；其余是下一阶段的工作项。

## 0. 本轮已交付

| 交付物 | 位置 | 状态 |
|---|---|---|
| 决策 | `docs/adr/0010-bare-shift-toggles-ascii-mode.md` | **[已做]** Accepted，3 个待定参数（P1/P2/P3）已答 |
| 判定纯函数 | `include/ShiftStateMachine.h` `DecideShiftUpAction()` | **[已做]** 无依赖，可单测 |
| 状态机成员 | `include/RimeWithWeasel.h` `m_bare_shift_toggles_ascii` | **[已做]** 默认 `true` |
| 服务端接线 | `RimeWithWeasel/RimeWithWeasel.cpp` `ShiftUp()` | **[已做]** 三分支 + `menu_open` 谓词 |
| 配置开关 | `output/data/weasel.yaml` `[global settings]` | **[已做]** `bare_shift_toggles_ascii: true` |
| 单测扩展 | `test/TestShiftIPCStateMachine/` | **[已做]** +6 函数 / +11 断言 ⇒ 28 PASS |
| 发布记录 | `CHANGELOG.md` 0.21.0.9 节 | **[已做]** |
| 说明书同步 | `Rime-设置说明书-重点快捷键.{md,html}` | **[已做]** §6.8 / §9.3 / §10 的过时说法 |
| 领域不变量 | `CONTEXT.md` §4 invariant 17 | **[已做]** |
| 开发清单 | `Fluxing-二次开发内容清单.md` §2 C8 | **[已做]** |
| 安装包 | `output/archives/fluxing-0.21.0.9-installer.exe` | **[已做]** |
| 下阶段 spec | `.specify/specs/082-bare-shift-ascii-toggle/` | **[已做]** 本目录 |

## 1. 里程碑

### M0 · 拍板与文档债清理（已完成）

| # | 任务 | 验收 |
|---|---|---|
| T0.1 | 拍板 ADR 0010 的三个参数：P1 默认开 / P2 只切当前会话 / P3 候选窗吃掉按下、无双击逃生门 | ADR 状态 `Proposed` → `Accepted` |
| T0.2 | 确认「yaml 路径不可达」的根因链（前端整段吃 Shift ⇒ librime 看不到 ⇒ `switch_key` / `key_binder` 的 Shift 条目全死） | ADR 0010 `Context` 节 |
| T0.3 | 确认 L18 的历史阻塞已失效（前端不再把 Shift 转给 librime ⇒ release 误匹配不可能再发生） | ADR 0010 `Context` 节 |
| T0.4 | 确认「裸 Shift 已被占用」不构成硬冲突（`Context::Select()` 在 composition 为空时 `return false`） | ADR 0010 `Context` 节 |
| T0.5 | 说明书 §6.8 / §9.3 / §10 里「目前做不到」「release 事件 modifier 比较 bug」等**已被推翻**的归因全部改正（md + html 两份） | 说明书不再给出错误归因 |

**依赖**：无。**风险**：低（文档 + 决策）。

---

### M1 · 服务端实现（已完成）

**目标**：裸 Shift 在「候选窗没开」时切中英，「开着」时保持原样。

| # | 任务 | 验收 |
|---|---|---|
| T1.1 | `include/ShiftStateMachine.h`：抽出纯函数 `DecideShiftUpAction()` + `enum class ShiftUpAction` | 无 rime_api / IPC / GUI 依赖 |
| T1.2 | `include/RimeWithWeasel.h`：新增成员 `m_bare_shift_toggles_ascii` | 构造默认 `true`（P1） |
| T1.3 | `RimeWithWeasel.cpp` 构造初始化列表加该成员 | 与 `m_global_ascii_mode` 相邻 |
| T1.4 | `RimeWithWeasel.cpp` `Initialize()`：用 `config_get_bool` 读 `bare_shift_toggles_ascii` | **键缺失 ⇒ 保持默认 `true`** |
| T1.5 | `RimeWithWeasel.cpp` `ShiftUp()`：读 `status.is_ascii_mode` + `ctx.menu.num_candidates`，算 `menu_open` | 谓词 = `num_candidates > 0 && !is_ascii_mode` |
| T1.6 | `RimeWithWeasel.cpp` `ShiftUp()`：按动作分支 —— `kSelectCandidate` 走 `select_candidate`；`kToggleAsciiMode` 走 `get_option`/`set_option` + `_UpdateUI` | 切换**不写** `m_global_ascii_mode` |
| T1.7 | `output/data/weasel.yaml`：新增 `bare_shift_toggles_ascii: true` | 与 `global_ascii` 同一 `config_get_bool` 机制 |
| T1.8 | `test/TestShiftIPCStateMachine`：+6 个测试函数 / +11 条断言 | `Result: 28 PASS / 0 FAIL` |
| T1.9 | 修掉测试文件里过时的 TDD 注释（「skeleton…must fail at HEAD ff03ad8」已不成立） | 注释与实测数字背书 |

**依赖**：M0.1（方向定了才敢改）。

**风险**：

- ⚠️ **`menu_open` 谓词写错 ⇒ 英文模式切不回中文。** librime 在英文模式下仍保留
  1 个候选的 raw 段（`ascii_segmentor.cc:20`），且 `key_binder.cc:255` 自己插
  `has_menu` 上下文时也要 `&& !ascii_mode` ⇒ 谓词必须是
  `num_candidates > 0 && !is_ascii_mode`，**不能只看 `is_composing`**。
- ⚠️ **切换绝不能走 `SetOption()`** —— 那个函数会写全局镜像 `m_global_ascii_mode`，
  违反 ADR 0010 P2（只切当前会话）。
- ⚠️ **裸 Shift 是对称的**：英文模式下按一下也切。习惯边打字边敲 Shift 的用户会被翻模式
  ⇒ 配置开关**必须**存在（P1 的反面）。

---

### M2 · 打包与门禁（已完成）

| # | 任务 | 验收 |
|---|---|---|
| T2.1 | `env.bat` 升到 `PRODUCT_VERSION`/`FILE_VERSION=0.21.0.9`、`WEASEL_BUILD=9` | 安装包文件名不与 0.21.0.8 撞车 |
| T2.2 | `cmd /c build.bat weasel installer` | 产出 `output/archives/fluxing-0.21.0.9-installer.exe` |
| T2.3 | AGENTS.md §2.5 静默装机 smoke test | 布局不变式全过，结果写进 commit message |
| T2.4 | 全套件 `scripts\test-infra\run-test-suite.bat` | `=== ALL TESTS PASSED ===` |
| T2.5 | 快捷键门禁**扫部署产物**（`<user>\build\*.schema.yaml`） | `dead=0` + `RESULT|PASS` |
| T2.6 | `CHANGELOG.md` 写 0.21.0.9 节（User-visible / Root cause / Fix / 验收） | 与实测数字一致 |

**依赖**：M1。

---

### M3 · 真机行为验收（**未做 —— 门禁覆盖不到的部分**）

**目标**：静态门禁只能证明「**能触发**」，证明不了「**触发后效果正确**」。

待验清单见 [spec.md §3.4](spec.md)（6 条）。要点：

| # | 操作 | 期望 |
|---|---|---|
| T3.1 | 中文模式、候选窗没开，单独按 `Shift` | 切英文 |
| T3.2 | 再按一下 | 切回中文 |
| T3.3 | 先打拼音让候选窗弹出，再按 `Shift` | **仍选第 2 候选**，不切模式 |
| T3.4 | 英文模式、候选窗没开，按 `Shift` | 切回中文 |
| T3.5 | `Shift+X` | 只出 `X`，不切模式 |
| T3.6 | 两个会话，在 A 里按裸 Shift | 只切 A |

**依赖**：M2（否则验的是旧二进制）。

**风险**：**这是本阶段唯一没法自动化的一块**，也**不能在沙箱里做**（沙箱只能装、不能模拟真人打字）。

---

### M4 · 收口（已完成）

| # | 任务 | 验收 |
|---|---|---|
| T4.1 | `CHANGELOG.md` 0.21.0.9 节 | 与实测数字一致 |
| T4.2 | 《Fluxing 二次开发内容清单.md》更新（T1 置完成、C 域补 C8、时间轴、门禁表） | 清单与代码一致 |
| T4.3 | `CONTEXT.md` §4 加 invariant 17（裸 Shift 由候选窗消歧，不靠时序） | §4 编号连续 |
| T4.4 | `Rime-设置说明书-重点快捷键.{md,html}` 同步 | 两份一致 |
| T4.5 | commit + push | Conventional Commits，单一 scope |

## 2. 总览

| 里程碑 | 内容 | 状态 | 依赖 | 风险 |
|---|---|---|---|---|
| M0 | 拍板 + 文档债 | ✅ 已做 | — | 低 |
| M1 | 服务端实现 + 单测 | ✅ 已做 | M0 | 中（`menu_open` 谓词易写错） |
| M2 | 打包 + 门禁 + CHANGELOG | ✅ 已做 | M1 | 低 |
| M3 | 真机按键验收 | ⬜ **未做** | M2 | 高（不能自动化） |
| M4 | 收口发布 | ✅ 已做 | M1, M2 | 低 |

## 3. 明确不做的事

- ❌ 改 `WeaselTSF/KeyEventSink.cpp` 让前端把 Shift 转给 librime
  （ADR 0007 已否决，会重开 L18 的 release 误匹配）。
- ❌ 改 `librime/`（宪法 P3）。
- ❌ 给裸 Shift 加**双击**逃生门（ADR 0010 P3）。
- ❌ 让裸 Shift 切**全局** ascii 模式（ADR 0010 P2）。
- ❌ 区分左右 Shift 的切换行为（候选选择才区分）。
- ❌ 在 `ascii_composer/switch_key` 里配 Shift（构造上不可达）。
