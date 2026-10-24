# Phase L Bugfix 7 v0.68 Ship — 新会话第一句话 prompt

承接 `handoff-after-phase-L-bugfix-6-ppl-ipc-block-v0.68-2026-07-28.md` (Phase 1 真因已锁 + 灾难 B hotfix 已 ship).

## 当前状态

- **基线 commit**: `bbd0414` (v0.67 installer source) + 工作树 modifications (L##-PhaseL-7+8 + _hotfix_patch_AB_v067.ps1 + hotfix_v0.67_patch_AB/ + 本 prompt + handoff)
- **branch**: `Fluxing`
- **fresh rebuild done**: `output\weasel.dll` md5 `2edbacc8b5cf18c111fb49f543363474` (含 Patch A/B), `output\Win32\WeaselServer.exe` md5 `3fea4d779bdfaa95950c3e299b2667df`, `output\Win32\WeaselDeployer.exe` md5 `5f6a5b1f0a8a9bbd74a067db57aa6301`
- **x64 build blocked** (本会话 safety classifier 拦截). 下会话手动跑 `MSBuild WeaselTSF\WeaselTSF.vcxproj -t:Rebuild -p:Configuration=Release -p:Platform=x64`

## 必交付 (按 ship gate §5 顺序)

1. **x64 rebuild** — 手动跑 `MSBuild WeaselTSF\WeaselTSF.vcxproj -t:Rebuild -p:Configuration=Release -p:Platform=x64`. 拿到 `output\weaselx64.dll` 含 Patch A/B 的 md5 (新, 不等于 `c62a364bd...`)
2. **Fix-Candidate B** — `WeaselDeployer/MaintenanceGuard.h:36-42` 加 5s timeout (避免 WeaselDeployer.exe /install 在 PPL-WeaselServer 持 pipe 时永久阻塞). 详见 handoff §3.1.
3. **sandbox-verify §9 GUI install test** — `in-sandbox-verify.ps1` 加 §9: 不传 `/S`, `Start-Process -Wait` 加 5min timeout, 验证 binary md5 更新到 build md5. 必须 PASS.
4. **扩展 _hotfix_patch_AB_v067.ps1** — 加 weaselx64.dll hotfix (rename, copy, restart ctfmon, verify md5).
5. **Rebuild + rebuild installer + ship gate verify** — `output\weasel.dll` md5 == installer extract `weasel.dll` md5; `output\weaselx64.dll` md5 == installer extract `weaselx64.dll` md5.
6. **L## PhaseL-7+8 verify + commit** — `chore(memory): L##-PhaseL-7+8` 单独 commit (避免 ship-gate fail 时 memory 也回滚)
7. **CHANGELOG.md v0.68 段** — 写完整 ship notes
8. **Copy to release/ + tag v0.19.0.68 + SFTP** (per `feedback_pinyin_release_workflow.md`)
9. **Cloudflare cache purge 通知**

## 禁止 (本阶段)

- ❌ 不动 install.nsi:758-765 ExecWait (真修在 MaintenanceGuard)
- ❌ 不动 install.nsi:498-565 L72-fix (Fix-D3 错)
- ❌ 不 commit handoff-*.md / _prompt_*.md (移交文件不归 git)
- ❌ 不 git push (K7 push 持续 500)
- ❌ 不修改 ship-gate 已有 8 条不变式 (a-h), 加 §9 不影响现有

## 加速模式 (session-health <2h)

- 单回合 diff > 200 行 → 拆回合
- bash ≥ 80 → 切 Write/Edit, 不反复 grep 大文件
- parallel subagent ≤ 2, sandbox-verify 测试可单独 foreground
- session > 1.5h → 立即写 handoff + 新开会话接力 (CLAUDE.md §8)

## 完成时机 (新会话退出条件)

- 上述 9 步全部 commit + release/fluxing-0.19.0.68-installer.exe 在 sandbox-verify §9 PASS
- 或任何 fix 后仍 FAIL → 写新 handoff (`handoff-after-phase-L-bugfix-7-blocker-v0.19.0.68-blocked-<date>.md`), 注明哪条假设需要 Phase 1 重审

---

*End of Phase L Bugfix 7 prompt. v0.68 ship gate 必查 installer-extract md5 vs build-output md5.*