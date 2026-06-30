# 005 · Tasks · 默认快捷键 rev2

> 任务清单。已完成的用 [x]；待做的 [ ]。
> 每条 1-3 文件，< 4h。

## Phase 1 · 实施

- [x] T001 [P1] [US1-A/B] 改 `output/data/default.yaml` `key_binder.bindings` 加入 `has_menu: comma/period → Page_Up/Down` (commit d30f69c)
- [x] T002 [P1] [US1-B] 改 `output/data/default.yaml` 加入 `has_menu: Shift+Shift_L/R → send 2/3` + `ascii_composer.switch_key.Shift_L/R: noop` (commit d30f69c)
- [x] T003 [P1] [US1-D] 改 `output/data/default.yaml` 加入 `always: toggle ascii_punct/traditionalization, accept: Control+Shift+9/0` (commits 6277b59)
- [x] T004 [P1] [US1-B] 改 `output/data/default.yaml` 移除 `Shift+l/r` 组合键（L16 修复；commit 828ae07）
- [x] T005 [P1] [US1-C/E] 改 `output/data/default.yaml` 用 `always: Shift+space toggle ascii_mode` 替代 `Shift+Shift_L/R` 切中英（L18 修复；待 0.18.6 commit）
- [x] T006 [P1] 升级 `test/TestDefaultHotkeys.cpp` 反映 L18 修复（25/25 PASS）

## Phase 2 · 验证

- [x] T007 [P1] `xbuild.bat weasel` → 0 errors（2026-07-01 验证）
- [x] T008 [P1] `TestDefaultHotkeys.exe output\data\default.yaml` → Passed 25/25
- [ ] T009 [P2] 用户侧手动验证 L18 回归（`shift+=` 等组合键不切中英）— 待 0.18.6 真实安装后

## Phase 3 · 提交 & Release

- [ ] T010 [P1] commit L18 修复：`fix(fluxing): spec 005 - Shift+space for ascii_mode toggle (L18)`（含 default.yaml + TestDefaultHotkeys.cpp + lessons-learned.md L18）
- [ ] T011 [P1] release `fluxing-0.18.6.0-installer.exe` 推 `kizemo`（含 L18 + 13-smoke-test）