承接 v0.19.0.65 ship (Phase L 装机 bugfix 2),见 `handoff-after-phase-L-2-3-bugfix-2-v0.19.0.65-2026-07-25.md`。

**当前状态**:
- commit `84f01c5` + tag `v0.19.0.65` 已 ship
- WeaselServer.exe md5 = `BFC81FA7481AF3C09B581D17F8D003D5`
- installer md5 = `1326F50CBCF4BE49EFDFB23CFC5D9667`
- 装机端 schema md5 preserved = `BEB5B74D0DD7B4D53D8386898DFA93D7` (Bug 1 fix 不变)

**必交付 (next session)**:
1. 跑装机 user flow 5 项验收 (Win+Space 弹栏 / 切英不弹 / Alt+, toggle / Ctrl+Shift+K shortcut / _check_install_v2.ps1)
2. 检查 `%APPDATA%\Rime\weasel-install.log` 验证埋点:
   - `[Run] s_fluxingHkl=0x...`
   - `[RegisterPhrasesHotkey] ImmGetDefaultIMEWnd(NULL) = 0x...`
   - 切 Fluxing 时 `[ImeLangSubclass] WM_INPUTLANGCHANGE ...` (v0.19.0.64 fallback 路径)
3. 反馈 hypothesis:FocusIn (L69-fix residue) 真修 路径 vs v0.19.0.64 IMM32 fallback 路径哪条触发了
4. 如果验证通过 → 跑 sftp_sync v0.19.0.65 上线 (K7 push 持续 fail, 走 fallback)
5. 如果 Win+Space 切到 Fluxing 浮动栏仍不弹 → 检查 log, 进一步 debug

**当前分支/基线**: `Fluxing` (本地 fork), commit `84f01c5`, tag `v0.19.0.65`

**禁止**:
- ❌ 不要改 Bug 1 (部件拆字标点) — user 搁置
- ❌ 不要新增 spec 071 (浮动设置栏 polish) — 等 v0.19.0.65 装机验证后
- ❌ 不要 push 到 GitHub (K7 持续 500)

**加速模式规则**: 单个回合 diff >500 行 → 拆回合;bash >=60 → 切 Write/Edit
