# 005 · 火流猩输入法 v2 · 默认快捷键 rev2

> 元 spec 004 拆分。定义"翻页 / 切换键 / 中英文切换 / 候选字上屏"4 类快捷键的 v2 默认值。
> 已通过 4 个 commit（d30f69c + 6277b59 + 828ae07 + L18 修复）在 0.18.0-0.18.5 ship；本 spec 是追溯规范。

## 0. 上下文

- 旧默认值（rime/weasel 0.17.x）：`=` / `-` 翻页、`Control+Shift+3/4` 切标点/简繁、`Shift_L` 单键切中英（commit_code）。
- 0.18.0+ 默认值：`,`（上页）/ `.`（下页）、`Control+Shift+9/0` 切标点/简繁、`Shift_L/R` 单键选候选（has_menu）+ `Shift+space` 切中英（L18 修复）。

## 1. 产品视角

### 1.1 目标

让 Fluxing v2.0.0 开箱即用英文阅读风格的候选翻页（`.` 下页 / `,` 上页）+ 搜狗拼音风格的 Shift 单键选候选 + RIME 社区标准的 Shift+space 切中英。

### 1.2 用户故事

- **US1-A** [P1]：候选窗打开时按 `.` 翻到下一页（Page_Down）；按 `,` 翻到上一页（Page_Up）。
- **US1-B** [P1]：候选窗打开时按 `Shift_L` 上屏第 2 候选；按 `Shift_R` 上屏第 3 候选。
- **US1-C** [P1]：无候选时按 `Shift+space` 切到英文；再按切回中文。
- **US1-D** [P1]：按 `Ctrl+Shift+9` 切中英标点；按 `Ctrl+Shift+0` 切简繁。
- **US1-E** [P1]（L18 回归）：按 `shift+=` / `shift+Enter` / `shift+<letter>` 等组合键时，ascii_mode **不**被误切换。

### 1.3 验收

- Given 全新安装 Fluxing 0.18.5+，
- When 用户按 `,` / `.` / `Shift+space` / `Shift_L/R` / `Ctrl+Shift+9/0`，
- Then 行为符合 spec 005 design.md §1.1 表格。
- And `TestDefaultHotkeys.exe output\data\default.yaml` → `Passed: 25 / 25`。
- And `xbuild.bat weasel` → 0 错误。

## 2. Out of scope

- 不为不同方案（luna-pinyin / rime_ice）做单独的快捷键 patch — 所有方案继承 default.yaml。
- 不改 `weasel.yaml` 样式（spec 004 SC-003 范围内）。
- 不实现 key_binder 之外的键位（speller/algebra）。
- L18 修复后的 "has_menu Shift+Shift_L/R 选候选" 在 `shift+<other>` 时的 release-event 误匹配是已知测试 gap（详见 L18 action items），不修。

## 3. 依赖

- **librime 1.13** `key_binder` 4 种 action type（`send` / `toggle` / `select` / implicit）— L04。
- **librime 1.13** modifier 大小写敏感 — L16。
- **librime 1.13** `key_event.cc::Parse` 接受 `Shift+space` 形式 — L18 验证。
- `output/data/default.yaml` 顶层 `key_binder:` 块（非 `patch:` 块）。
- `ascii_composer.switch_key.Shift_L/R: noop`（librime `load_bindings:37-38` 跳过 noop）。

## 4. 状态

- 0.18.0 → 0.18.5 已 ship；L18 修复（`Shift+space` 切中英）待 0.18.6 release。
- 配套测试 `test/TestDefaultHotkeys.cpp` 25/25 PASS（2026-07-01 验证）。
- 详见 `design.md` §2.2 的 yaml 样例（与 0.18.5+ shipped 版本 1:1 对齐）。