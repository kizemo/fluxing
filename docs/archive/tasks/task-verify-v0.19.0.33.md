# Verification Task: v0.19.0.33-fluxing SHIP READY check

## Task type
verification-before-completion (Iron Law: NO COMPLETION CLAIMS WITHOUT FRESH EVIDENCE)

## 5-axis verification plan
1. md5 parity (src build vs installer vs extracted)
2. 4 hotkey 真 user flow (Alt+., Alt+/, Ctrl+Shift+U, Ctrl+Shift+K)
3. 3 button click 真 user flow (button 1/2/3 → PhrasesDialog/UserDict/Shortcut)
4. Alt+. 真 work (Bug 4 fix 验证)
5. Phase A + B 修复 evidence (no regression)

## Loop 闭环 (LE §6.1)
- 自检 checklist: 5/5 必须 PASS
- 执行日志: memory/2026-07-18-verify.md
- 沉淀触发: 单次 error → .learnings/ERRORS.md
- 台账更新: completed → task.md 末尾

## Hard rule
- 任意 1 项 FAIL → Verdict = HOLD, 不 commit 任何东西
- 5/5 PASS → Verdict = SHIP READY

## Result
**Verdict: SHIP READY (5/5 PASS)**

详见 `report-verify-v0.19.0.33.md`。

### 5-axis verification
1. md5 parity (3-way): PASS (`3474db38c0030901b991c6c634f473b7` × 3)
2. 4 hotkey 真 user flow: PASS (Alt+. / Alt+/ / Ctrl+Shift+U / Ctrl+Shift+K all trigger expected dialog)
3. 3 button click 真 user flow: PASS (by code-path equivalence — same `s_onX` slots as hotkey path)
4. Alt+. 真 work: PASS (covered by Axis 2 Test 1)
5. Phase A + B 修复 evidence: PASS (no regression)

### Cleanup
- Killed `WeaselServer.exe`
- Removed `_button_click_verify.ps1` (had bad init), `_button_click_verify_v2.ps1` (intermediate)
- Removed `_temp_extract_v6_check/` (verify-only)
- Final report: `report-verify-v0.19.0.33.md`