# Phase L Bugfix 5 v0.68 — 新会话第一句话 prompt

承接 `handoff-after-phase-L-bugfix-5-hang-v0.19.0.67-blocked-2026-07-28.md` (v0.67 ship 灾难级回归, 双灾难锁定)。

## 当前状态

- **基线 commit**: `bbd0414` (v0.67 installer + Patch A/B + Patch C1/C2)
- **branch**: `Fluxing` (ahead 945, behind 831, K7 push 持续 HTTP 500 不依赖 GitHub 同步)
- **working tree**: `M CHANGELOG.md` (+79 行 v0.66/v0.67 段, 待 commit 进 v0.68 ship commit) + `??` 3 个文件 (上一会话写的 verify 脚本 + 本 prompt + 本 handoff)
- **装机端实测**: v0.67 installer 在 user system 8:47:38 → 9:04:19 永久 hang 17 分钟 (WER "Application Hang"), 浮动栏不弹 (灾难 #2 仍 fail)。**装机 binary 仍是 v0.66 老版**, 仅 uninstall.exe 是 v0.67 写入。

## 真因 (Phase 1-3 已锁)

- 灾难 A (installer hang 17min): `output/install.nsi` line 760 `ExecWait "$INSTDIR\WeaselDeployer.exe /install"` 在 ctfmon 持 v0.66 老 `weaselx64.dll` mmap 时永久等 SCM RPC 17min timeout
- 灾难 B (浮动栏不弹): 同根因 — `install.nsi` line 498-565 L72-fix `Rename-then-File` 段 silent skip, ctfmon 持老 mmap → 新 binary 永不写盘 → Patch A/B 未 ship 进 user binary

## 必交付 (按 ship gate §5 顺序,禁止改顺序)

1. **Failing Test First** — `in-sandbox-verify.ps1` 加 §9 double-install test (装完 v0.67 → 装 v0.68 → 验证 10min 完成 + binary md5 更新), 跑 v0.67 installer 必 FAIL
2. **Fix-D1** (最小) — `output/install.nsi` line 760 `ExecWait` → `Exec` (non-blocking), commit 单独 "fix(installer): v0.68 Fix-D1"
3. **Rebuild + re-run §9** — 看 §9 是否 PASS, FAIL 则进 4
4. **Fix-D3** (完整) — `output/install.nsi` line 498-522 (weaselx64.dll) + 542-565 (WeaselServer.exe) + 580-599 (WeaselDeployer.exe) `Rename-then-File` 段: fail 之前强制 `nsExec::ExecToStack 'taskkill /F /IM ctfmon.exe /T'` + Sleep 5000 + retry once, commit "fix(installer): v0.68 Fix-D3"
5. **L##-PhaseL-11 沉淀** — `.specify/memory/lessons-learned.md` 加 L## 段 (内容已在 handoff §4.4, byte-level replace 走 L01 规则)
6. **Rebuild + rerun §9** — 必须 PASS, 然后 commit "chore(release): v0.68 installer" 一并含 CHANGELOG.md v0.68 段
7. **Copy installer to release/ + tag v0.19.0.68 + SFTP** (per feedback_pinyin_release_workflow.md)

## 禁止 (本阶段)

- ❌ 不动 `RimeWithWeasel/` `WeaselTSF/` 等 Runtime 代码 — Patch A/B 已 commit `bbd0414`, 不要重新解读 v0.66 sink 真因 (那是 subagent 误判, 已 verify)
- ❌ 不动 `sandbox-verify/lib/` (PowerShell 共享 lib)
- ❌ 不动 `output/uninstall` 路径 (uninstall.nsi 不存在, uninst 逻辑在 install.nsi Section "Uninstall" line 849)
- ❌ 不做 `if (false)` 残留清理 (L81 fix 范围外)
- ❌ 不 commit `_prompt_*.md` / `handoff-*.md` (移交文件不归 git)
- ❌ 不 git push (K7 push 持续 500)
- ❌ 不写任何 release/ + output/archives/ 之外的临时拷贝 (.gitignore 保护)
- ❌ Fix-D3 不要在一个 commit 里和 Fix-D1 合并 (systematic-debugging "1 fix at a time")

## 加速模式 (session-health <2h)

- 单回合 diff > 200 行 → 拆回合, 优先 verify script 单独 commit
- bash ≥ 80 → 切 Write/Edit, 不反复 grep 大文件
- parallel subagent ≤ 2, sandbox-verify 测试可单独 foreground
- session > 1.5h → 立即写 handoff + 新开会话接力 (CLAUDE.md §8)

## 完成时机 (新会话退出条件)

- 上述 7 步全部 commit + release/fluxing-0.19.0.68-installer.exe 在 sandbox-verify §9 PASS
- 或任何 fix 后仍 FAIL → 写新 handoff (`handoff-after-phase-L-bugfix-5-blocker-v0.19.0.68-blocked-<date>.md`), 注明哪条假设需要 Phase 1 重审
