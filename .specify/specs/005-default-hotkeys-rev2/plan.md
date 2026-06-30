# 005 · Plan · 默认快捷键 rev2

> Tech-aware plan，与 spec 005 spec.md / design.md 配套。

## 1. 技术上下文

- **librime 1.13** `key_binder` / `ascii_composer` / `switch_key`（L04 / L16 / L18）。
- **修改文件**：`output/data/default.yaml`（顶层 `key_binder:` 块 + `ascii_composer.switch_key`）；`test/TestDefaultHotkeys.cpp`（断言 25 项）。
- **不引入新依赖**（P3）。
- **Conventional Commits**：`fix(fluxing): spec 005 - Shift+space for ascii_mode toggle (L18)`。

## 2. Architecture

- `output/data/default.yaml` 4 类 binding（`has_menu: comma → Page_Up` 等）按列表顺序匹配，第一个命中即停止。`has_menu` 行的 Shift+Shift_L/R 必须在 `always` 行的 Shift+space 之前。
- `ascii_composer.switch_key.Shift_L/R: noop` 让 ascii_composer 不抢 Shift 键，全权交给 key_binder。
- `TestDefaultHotkeys.cpp` 是 string-matching 断言（不验证 librime 行为），25/25 PASS 作为 smoke test。

## 3. Constitution Check

| Rule | Status | Notes |
|---|---|---|
| I. Intent | OK | spec.md §1.1 明确"v2 默认值" + L18 修复意图 |
| II. Test | OK | 25/25 PASS 是 test-backed evidence |
| III. Spec-Artifact | OK | 3 件套齐全 |
| IV. Clarification | OK | 无 [NEEDS CLARIFICATION] |
| V. Incremental | OK | 4 commits across 0.18.0-0.18.5 |
| R1-R9 | OK | design.md 引用 L04/L16/L18；无 tech-words in spec.md |
| P1-P8 | OK | 009 风格的 P8 brand-fork scope 内（fluxing: scope） |

## 4. 风险

- **R1**：单测只验证 yaml 字符串，不验证 librime 行为。L18 测试 gap 已知，action item 待 v2.1+ 实施。
- **R2**：`has_menu: Shift+Shift_L/R` 在 `shift+<other>` 时仍可能误匹配（已知 issue，待 task 评估是否改为 `Control+Shift+1/2`）。