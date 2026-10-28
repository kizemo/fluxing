# 085 · Tasks · CapsLock 打开时快捷键与候选操作恢复可用

> 配套：[spec.md](spec.md)、[plan.md](plan.md)、[ADR 0014](../../../docs/adr/0014-front-end-stops-reporting-capslock-state.md)。

## Phase 1 · 前端改造（Q1）

- [x] T001 [P1] [US085-A] `WeaselTSF/KeyEvent.cpp` 删除 CapsLock 锁存标记的**置位**与**撤销**两处（`^=` 异或陷阱：两处必须一起删，代码注释写明）
- [x] T002 [P1] [US085-B] `WeaselTSF/KeyEvent.cpp` 保留 CapsLock 键自身以 `XK_Caps_Lock` 上报（不改 `include/KeyNameMapper.h` 的键码映射），并在注释中写明「切中英 / 交还系统」行为不变
- [x] T003 [P2] [US085-B] `include/KeyNameMapper.h` 更新 `CaptureAccept` 上方的悬空注释（「是否上报 CapsLock 仍待 ADR 0011 Q2 / T20 决定」→ 指向 ADR 0014 的定论）
- [x] T004 [P2] [US085-C] `include/KeyEvent.h` 保留 `LOCK_MASK` 常量（门禁掩码自检仍在用），补一行注释说明「常量保留但前端不再上报」

## Phase 2 · 门禁改造（Q1）

- [x] T005 [P1] [US085-C] `test/TestHotkeyBindings/TestHotkeyBindings.cpp`：`BuildReachable()` 增加 CapsLock 状态维度（`state[VK_CAPITAL] = TOGGLED`），产出「开 / 关」两套可达集合
- [x] T006 [P1] [US085-A] 同上：新增判定——每条**非字母类**绑定在 CapsLock 打开态下必须可达，死绑定数为 0；字母类绑定单列 `caps_limited` 并断言其恰为「键码是 ASCII 字母的绑定」
- [x] T007 [P1] [US085-C] 同上：内建负控——模拟旧行为（把锁存标记塞回状态字段）后，同一批绑定的可达数**必须下降**，否则门禁判定失败（证明门禁对本缺陷敏感）
- [x] T008 [P2] [US085-A] 同上：`SUMMARY|` 行新增 `caps_bindings` / `caps_reachable` / `caps_dead` / `caps_limited` / `caps_legacy_reachable` / `caps_legacy_dead` 字段，既有字段的值保持不变

## Phase 3 · 验证与发布（Q2）

- [x] T009 [P1] [US085-A] 门禁复跑：CapsLock 关闭态结果与修复前逐项一致（`bindings=39 reachable=39 dead=0 mismatches=0 unused=0`）
- [x] T010 [P1] [US085-C] 门禁复跑：记录修复前 / 修复后的 caps 可达数字对比，贴进 `spec.md` §5.3（坐实证据）
- [x] T011 [P2] [US085-A] 全套件复跑 + 整解构建（Release / Win32）+ 改动文件格式检查全清
- [x] T012 [P2] [US085-A] 重新打包 1.1.0，并对**部署产物**复跑门禁（源绿 ≠ 产物绿）
- [x] T013 [P2] [US085-A] CHANGELOG `### 主要更新` 写用户可见修复说明
- [x] T014 [P2] [US085-A] 任务总账 T20 结项 + 交接卡新增 §16
- [x] T015 [P2] [US085-C] ADR 0014 落 `Proposed`；ADR 0011 Q2 标注「已决（见 ADR 0014）」
- [x] T016 [P2] [US085-A] 提交（`fix(WeaselTSF):` + `docs(fluxing):` + `chore(fluxing):` 三笔）并推送 `kizemo/Fluxing`，CI 四 job 终态确认
- [ ] T017 [P2] [US085-A] 交用户真机验收：开着 CapsLock 时选候选 / 翻页 / 切中英 / 切标点 / 切简繁**全部可用**；CapsLock 键自身行为不变；字母仍为大写。验收通过 ⇒ ADR 0014 `Proposed` → `Accepted`

## 状态

- 2026-10-28：立项（spec 085，ADR 0014 `Proposed`）。
- 2026-10-28：T001–T016 全部落地并通过验证 —— 整解 Win32 exit 0 / 全套件
  `=== ALL TESTS PASSED ===` / 门禁四场景 caps 维度 `caps_dead=0` 且负控严格更差 /
  装器载荷复跑一致 / 装包 `PKG-VERIFY=PASS`；**T017 待用户真机验收**。
