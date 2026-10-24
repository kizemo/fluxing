承接 `handoff-after-phase-L-bugfix-7.5-v0.69-ship-2026-07-29.md` + `handoff-bug-floating-bar.md`。v0.19.0.70 已 ship (2 commits: `c14d6ee` Fix B revert + `65953a9` v0.70 installer, tag `v0.19.0.70`, SFTP OK, md5 `6d266fd2...`)。

## User-side 装机测试结果待反馈

User 在本会话跑 v0.70 installer,跑 6 项 user flow + §5.1 reg query,**装机后反馈结果**。New session 接力任务:

## 必交付
1. **读 user 反馈的装机测试结果** — 6 项 user flow 哪几项 PASS / FAIL (§3 装机端 user 期望 from handoff-bug-floating-bar.md):
   - 切 Fluxing 浮动栏弹?
   - Alt+, toggle?
   - 切英文不弹?
   - 默认 IME 不 Fluxing?
   - reg query 4 keys 全部存在?
   - 装机过程不卡死?
2. **如果 FAIL** → 跑 `diag-floating-bar.ps1` (F:\soft\00selfmade\rime_claude\) 抓 log, 按 `handoff-bug-floating-bar.md` §5 判别表定位 root cause (A/B/C/D)。
3. **如果 PASS** → ship-gate 通过, 关闭 v0.70 task, 写 lessons-learned L## 沉淀。
4. **可能** — 如果 Fix A 仍未修通, 需 Phase 3 重新审视 (3+ fix 失败按 systematic-debugging 应 question architecture, 不是再试)。

## 禁止
- ❌ 不再 blind 改 code (3 fix attempts 已过, 按 systematic-debugging 4+ 失败需 question architecture)
- ❌ 不 git push (K7 仍 500)
- ❌ 不 commit handoff-*.md / _prompt_*.md (移交文件)
- ❌ 不修 install.nsi unless §5.1 reg query 真 FAIL

## 加速模式 (session-health <2h)
- 单回合 diff > 200 行 → 拆回合
- bash ≥ 80 → 切 Write/Edit
- parallel subagent ≤ 2
- 跑 diag 时 foreground (不 subagent)

## 完成时机
- 6 项 user flow + §5.1 全 PASS → ship-gate 通过, 关闭 task, 写 handoff v0.70-validated
- 仍 FAIL → 写新 handoff `handoff-after-phase-L-bugfix-7.5-v0.70-blocked-<date>.md` 注明 Phase 3 假设需 user 决策 (架构问题 not 修代码)