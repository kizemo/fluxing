# Prompt — 下个会话接力 spec 071 P2/P3 完成 + 真机 verify

**承接**: `handoff-after-hook-blockers-fix-t101-t205-pending-2026-07-30.md`

**第一句话**: 承接 handoff §0 状态,继续 spec 071 v0.74.1 装机 freeze 修复。已 ship code 层 P1 + T103/T104 + Hook 3 BLOCKERS 修完 + L##-9.7/9.8 lessons 待 T101 → P3 → 真机 sandbox-verify → sftp。

**必交付**:
- T101 (P2): `output/install.nsi:557,604,664,712,750` 5 stage blocks 改 5 unique schtasks task name (`FluxingStage2ShimSwap_<shim>`) + pre-install 6 次 schtasks /Delete 防御清理 + `stage2-shim-install.bat.cmd` 加 SHIM env var 解析 + per-shim pending.json
- T102 (P2): `test/TestInstallerShimStage/Test-Stage2ShimInstall.ps1` add Test 7 (5 unique 验证)
- T201 (P3): `output/install.nsi:970-974` Ready-flag 30s timeout 加 MessageBox (IfSilent skip 包裹)
- T202 (P3): `test/TestReadyFlagMessageBox/Test-ReadyFlagMessageBox.ps1` 新 test (手测触发跳过 Ready 写 + 弹 MessageBox)
- T203 (P3): `docs/adr/0003-installer-shim-stage-at-startup.md` amend §Postscript (5 unique schtasks + glob cleanup rationale)
- T205 (P3): `env.bat` + `weasel.props` bump v0.74 → v0.74.1 + `git status -sb` + `git diff --stat` ship-gate
- Task #5 (P1): 真机 sandbox-verify v0.74.1 (5 binary md5 ≠ previous + Audit 9/9 + Test N/N + Ready-flag 30s + 切输入法 ≤ 1s + rime.weasel log 0 opencc ERROR)
- Task #3 (P3 git push): 等 #5 PASS + 网络恢复 + 用户明确放行

**必读顺序** (per handoff §6):
1. handoff (this-prompt sibling) — 5 min
2. `.specify/specs/071-install-freeze-fix/tasks.md` — 2 min
3. `output/install.nsi:517-518` + 5 stage block locations — 5 min
4. `output/stage2-shim-install.bat.cmd:1-50` — 2 min
5. `.specify/memory/lessons-learned.md` L##-9.7/9.8 (GetSize-units + emoji.ocd) — 3 min

**禁止事项**:
- 不 push (P3 等 Task #5 + 网络)
- 不复用 v0.63-65 浮动栏代码 (那是 Task #10 P1 #10 范围)
- 不再 GetSize "S" units (沉淀为 L##-9.7)
- 单回合 diff >500 行 → 拆
- T101 NSIS cmd escape 仔细测试 (5 schtasks task name + SHIM env var)

**加速模式规则** (CLAUDE.md 默认):
- bash ≥80 calls 时主动停 /exit 或新开会话
- 单回合 diff >500 行 → 拆
- "${GetSize} 使用前 code review" 防 units bug regression
- 不复制粘贴 v0.63-65 浮动栏代码

**第一轮回复模板**: "读取 handoff + spec tasks.md + install.nsi stage block locations,确认 ready,开始 T101"
