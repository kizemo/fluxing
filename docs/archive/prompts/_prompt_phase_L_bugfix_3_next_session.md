承接 Phase L 装机 bugfix 2 (v0.19.0.65) ship 后 user 装机验收全失败,见 `handoff-after-phase-L-bugfix-2-failed-v0.19.0.65-2026-07-25.md`。

**3 次 ship 失败累积状态 (v0.19.0.63 / .64 / .65),装机端 4 个 user flow 全失败**:
1. Win+Space 切 Fluxing → 不弹
2. Win+Space 切英文 → 不弹 (回归 OK)
3. 切焦点到 text input → 不弹
4. **Alt+, → 不弹 (回归!** — 之前 work,现在死了,我没改这条路径)

**必交付 (next session 第一件事)**:
1. 并行启 **3 个 Explore subagent** 按 handoff §6 分析矩阵:
   - Subagent 1: Alt+, 回归根因 (我没改这条路径却 work 不动)
   - Subagent 2: FocusIn 路径是否真的被触发 (DLOG 没出来)
   - Subagent 3: 所有 QuickPanelDialog::Show 触发路径 compatibility matrix
2. subagent 报告后,综合分析, **必须先形成新 hypothesis + 加诊断埋点,才能改代码**
3. 严禁盲目试错 — 之前 3 次 ship 都失败,任何新 hypothesis 必须先有装机证据支撑

**当前状态**:
- branch `Fluxing` ahead 943 / behind 831
- commit `84f01c5` / tag `v0.19.0.65`
- 装机端 binary md5 = `BFC81FA7481AF3C09B581D17F8D003D5` (v0.19.0.65 build) ✓
- weasel-install.log 关键证据: `ImmGetDefaultIMEWnd(NULL) = 0x00000000` (IMM32 path dead 装机证实)

**禁止**:
- ❌ 改 install.nsi (L09/L17/L54/L66 雷区)
- ❌ 改 QuickPanelDialog.cpp 内部 (除非所有调用方都不 work)
- ❌ 改 Bug 1 部件拆字 (user 搁置)
- ❌ 删现有埋点
- ❌ 单步试错改代码 — 必须 subagent 先行

**加速模式**: 单回合 diff >500 行 → 拆回合;bash >=60 → 切 Write/Edit