承接 `handoff-after-phase-L-bugfix-8-v0.71-ship-2026-07-29.md`。
v0.19.0.71 freeze mitigation 已 ship (2 commits: 45264e5 fix + cfcb96d release, SFTP sync OK, 7/7 test PASS)。
本会话任务: 接收 user v0.71 装机反馈 → 6 项 user flow + §5.1 reg query 验证。

## 必交付
1. **跑 6 项 user flow** (跟 v0.69 同样 checklist, handoff §4):
   - 切 Fluxing 浮动栏弹? (Fix A 保留)
   - Alt+, toggle? (Fix C 保留)
   - 切英文不弹?
   - 默认 IME 不是 Fluxing? (user 已 reg add 改过 Preload)
   - reg query 4 keys 全在?
   - 装机过程不卡死? (NEW: 即便 v0.71 真卡死, 100ms 后可恢复 = 不再永久卡)
2. **如果全 PASS** → 写 lessons-learned L##-PhaseL-9 沉淀 (3 mitigations + A/B test 留待下一 phase), 关闭 v0.71 task, 写 handoff v0.71-validated
3. **如果仍卡死** → 跑 `diag-freeze-capture.ps1` 抓现场 (process state + logs), 跟 user 决定 A/B test v0.69 还是升级到 Phase 3 架构 (OnActivated PostMessage 异步)
4. **如果 PARTIAL** → 跑 `_check_install_v2.ps1` 验 binary md5, 定位 stale binary 路径

## 禁止
- ❌ 不改 code (除非 §3 Phase 3 架构讨论 user 决定)
- ❌ 不 commit handoff-* / _prompt-* / diag-* / _build_*.cmd (移交 + 工具文件, 跟 v0.69 同模式)
- ❌ 不 git push (K7 仍 500, 2 commits 45264e5 + cfcb96d 待 push)
- ❌ 不修 install.nsi unless §5.1 reg query 真 FAIL (L66 教训)
- ❌ 不绕过 `_check_install_v2.ps1` 直接信 user 口述 (装机端可能 PPL stale binary, P3 镜像信任)

## 加速模式 (session-health <2h)
- 单回合 diff > 200 行 → 拆回合
- bash ≥ 80 → 切 Write/Edit
- parallel subagent ≤ 2

## 完成时机
- 6 项 user flow + §5.1 reg query 全 PASS → ship-gate 通过, 关闭 task, 写 handoff v0.71-validated
- 仍卡死 / PARTIAL → 写新 handoff `handoff-after-phase-L-bugfix-9-v0.71-blocked-<date>.md` 注明需 user 决策 (A/B test vs Phase 3)
