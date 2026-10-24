承接 `handoff-after-phase-L-bugfix-7-v0.68-ship-2026-07-28.md`(v0.68 完整 ship + SFTP sync OK,5 commits + tag v0.19.0.68)。

## 当前状态
- **基线**: `c528728` (Fluxing, ahead 950)
- **tag**: `v0.19.0.68` 在 `67520c3` (未 push,K7 网络持续 500)
- **installer**: `release/fluxing-0.19.0.68-installer.exe` 43.3 MB md5 `79e7787c7eac765d23e48e16ecac3dec`
- **SFTP**: 已 atomic upload 4 文件, server metadata live (cf-cache-status: DYNAMIC)

## 必交付
1. **装机 user flow 6 项验证** (per handoff §3): 跑 v0.68 installer 或 `_hotfix_patch_AB_v068.ps1`, 验证 ① 不卡死 ② 默认 IME 不 Fluxing ③ Win+Space 弹浮动栏 ④ 切英文不弹 ⑤ Alt+, toggle ⑥ §5.1 reg query 4 keys 全部存在
2. **(可选) repo hygiene cleanup**:
   - `librime/` submodule dirty (deps/opencc) — per P3 需 user 授权 + ADR, agent 不擅自 commit
   - 80+ untracked files (`.claude/`, `handoff-*.md`, `release/fluxing-0.19.0.67-installer.exe` 等) — working tree residue,user 决定保留/清理
3. **(可选) push tag v0.19.0.68**: K7 网络恢复后 `git push origin v0.19.0.68`

## 禁止
- ❌ 不 push (K7 仍 500, per handoff `不允许 push`)
- ❌ 不修改 librime/ submodule (P3 需 ADR + 用户授权)
- ❌ 不 commit handoff-*.md / _prompt_*.md (移交文件不归 git)
- ❌ 不擅自 commit 80+ untracked files (per-source 模式,非 v0.68 work)

## 加速模式 (session-health <2h)
- 单回合 diff > 200 行 → 拆回合
- bash ≥ 80 → 切 Write/Edit
- parallel subagent ≤ 2, 装机验证可单独 foreground

## 完成时机
- 6 项装机 user flow 全 PASS + 装机端 reg query 4 keys 全部存在
- 或 user 决定 ship-gate 仍 FAIL → 写新 handoff `handoff-after-phase-L-bugfix-7-v0.68-ship-blocked-2026-07-28.md`
