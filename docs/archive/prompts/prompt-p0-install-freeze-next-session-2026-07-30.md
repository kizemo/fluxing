# Prompt — P0 装机 freeze 诊断 + 修复 (新会话第一句话)

承接 handoff `handoff-after-priority-shift-p0-install-freeze-2026-07-30.md` (2026-07-30 用户调整优先级后写的)。

**新优先级**:
1. **P0** 解决装机时无响应(用户首要)
2. **P1** 切输入法显示设置栏
3. **P3** git push(网络阻塞 + 用户明确"安装文件存在 bug 时不适合推送" — 反而不要 push)

**必交付**:
- Task #9: 诊断装机 freeze 真因(brainstorm 装机现象 + 跟 L##-PhaseL-7/8/9 对比)
- Task #7: 写 `.specify/specs/NNN-install-freeze-fix/{spec,plan,tasks}.md` + 实现修复
- Task #6: 真机 sandbox-verify v0.74.1 验证
- (Task #8 push 等 #6 PASS + 网络恢复后)

**当前状态**:
- HEAD 654d3b2,3 commits 待 push (但 P3)
- 远端 backup branch `Fluxing-backup-pre-2026-07-30 = 7d9f24b` ✅
- working tree 干净 (0 source changes)
- 网络 40% packet loss(暂时无法 push,但非主要矛盾)
- v0.74 装机 freeze mitigation 5 lessons 已 ship (L##-PhaseL-7/8/9/9.5/9.6),但用户测试仍 freeze → 有未修场景

**P0 第一步**(不要直接写代码):
1. 读 handoff §2 装机 freeze 历史脉络(5 lessons 表格)
2. 用 AskUserQuestion / 直接问用户**装机 freeze 现象**:哪一步?持续多久?真机/sandbox?有 log 吗?`C:\Users\Duanyi\sandbox-artifacts\rime\logs\verify.log`?
3. 跟 L##-PhaseL-7 (17min PPL hang) / L##-PhaseL-9 (25s 冷启动) / L##-PhaseL-9.5 (F4 race) 已知模式对比
4. 定位真因后,读相关文件 (WeaselServer.cpp / QuickPanelDialog.cpp / install.nsi F4 / RimeWithWeasel) 调研
5. 写 `.specify/specs/NNN-install-freeze-fix/spec.md` 走 spec-driven-development 流程

**关键文件** (handoff §6 完整列表):
- `.specify/memory/lessons-learned.md` L##-PhaseL-7/8/9/9.5/9.6
- `output/install.nsi` F4 stage + silent Abort
- `output/stage2-shim-install.bat.cmd`
- `WeaselServer/WeaselServer.cpp` startup
- `WeaselServer/QuickPanelDialog.cpp` 浮动栏
- `RimeWithWeasel/WeaselTSF.cpp` TSF 激活
- `docs/adr/0003-installer-shim-stage-at-startup.md`
- `_check_install_v3.ps1` (3 mode F4 verify)
- `_check_system32_cleanup.ps1` (Audit/Live/PretouchUninstall)
- `memory/2026-07-26-v066-disaster-rootcause.md`
- `feedback_ppl_ipc_block_17min.md`
- `feedback_l9_install_freeze_ux_timing.md`

**禁止事项**:
- 不 push (P3,等 #6 + 网络恢复)
- 不写 installer 改动而不测 — 装机 bug 真修后才能 ship
- 不复制粘贴 v0.63-65 浮动栏代码(那部分已 ship 稳定,用户要的是 UX 完善,不是 bug 修)
- 不修改 librime / plum submodule pointer

**加速模式规则** (CLAUDE.md 默认):
- 单回合 diff >500 行 → 拆分
- 任何 WeaselServer / WeaselTSF 改动 → 4-axis 验证 (compiles / ctfmon 不退 / 切输入法不卡 / 浮动栏触发)
- Installer 改动 → 走 `feedback_nsis_silent_taskkill.md` 5 trap 检查
- 任何 push 失败累积 → 走 D 方案 (backup + force-push) — 但本会话**不 push**

**第一句回复** (新会话第一轮):
"读取 handoff §2 装机 freeze 历史,然后用 AskUserQuestion 问你 4 个问题:1) 现象在哪一步 2) 持续多久 3) 真机/sandbox 4) 有没有 verify.log / sandbox log"
