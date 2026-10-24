承接 `handoff-after-phase-L-bugfix-8.1-v0.71-corrected-2026-07-29.md`。
v0.19.0.71 corrected ship 完成 (3 commits: 45264e5 first/broken, cfcb96d release/broken, 5346c07 re-ship/CORRECTED; SFTP OK; 13/13 test PASS; ADR 0001)。
本会话任务: 接收 user v0.71 corrected 装机反馈 → 6 项 user flow + §5.1 reg query 验证。

## 必交付
1. **verify 装机版本**:
   - installer md5 = `FA7C75806D735B2208539C824FBDAFAE` (via _check_install_v2.ps1)
   - weasel.dll md5 = `7f7361608af6ec321d399076b44ddacf`
   - weaselx64.dll md5 = `ea4dece5b4c76a01704204472c2aa5f4`
   - WeaselServer.exe md5 = `6cfa366b681a082f87fd2b6aeef8c76a`
   - WeaselDeployer.exe md5 = `f81a5c4b7547266552898ec240357948`
2. **跑 6 项 user flow** (跟之前 checklist):
   - 切 Fluxing 浮动栏弹? (Fix A 保留)
   - Alt+, toggle? (Fix C 保留)
   - 切英文不弹?
   - 默认 IME 不是 Fluxing?
   - reg query 4 keys 全在?
   - 装机过程不卡死? (NEW: 即便真卡死, 100ms 后可恢复, 不再永久卡)
3. **如果全 PASS** → 写 lessons-learned L##-PhaseL-9 沉淀 (3 fixes + post-hook 学习: commit message 谎报 detection + __declspec thread standard + shared_ptr worker lifetime + drop counter metric), 关闭 v0.71 corrected task, push 2 commits (5346c07 + cfcb96d re-ship)
4. **如果仍卡死** → 跑 `diag-freeze-capture.ps1` 抓 drop_count (新 metric), 100ms 后应可恢复 (Fix 1 边界验证). 跟 v0.70 "永远卡死" 对照确认 Fix 1 有效. 仍卡 + 100ms 后可恢复 = root cause 未消, Phase 3 架构讨论 (OnActivated PostMessage 异步)
5. **如果 PARTIAL** → 跑 `_check_install_v2.ps1` 验 binary md5, 定位 stale binary 路径

## 禁止
- ❌ 不改 code (除非 Phase 3 架构 user 决定)
- ❌ 不 commit handoff-* / _prompt-* / diag-* / _build_*.cmd / get_md5.ps1 / verify_v071.ps1 (移交 + 工具文件)
- ❌ 不 git push (K7 仍 500, 3 commits 待 push)
- ❌ 不修 install.nsi unless §5.1 reg query 真 FAIL (L66 教训)
- ❌ 不绕过 `_check_install_v2.ps1` 直接信 user 口述 (装机端 PPL stale binary 可能, P3 镜像信任)

## 加速模式 (session-health <2h)
- 单回合 diff > 200 行 → 拆回合
- bash ≥ 80 → 切 Write/Edit
- parallel subagent ≤ 2

## 完成时机
- 6 项 user flow + §5.1 reg query + 装机 binary md5 verify 全 PASS → 关闭 task, 写 handoff v0.71-corrected-validated
- 仍卡死 / PARTIAL → 写新 handoff `handoff-after-phase-L-bugfix-9-v0.71-corrected-blocked-<date>.md` 注明需 user 决策 (A/B test v0.69 vs Phase 3 架构)
