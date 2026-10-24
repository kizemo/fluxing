# Handoff — Priority shift to install-freeze fix (2026-07-30)

> **承接**: `handoff-after-push-blocked-network-2026-07-30.md`(已废弃 push 为 P0)
> **状态**: ✅ Source-level ship-gate PASS (5 binary md5 MATCH + Audit 9/9 + Test 6/6) | ⚠️ **网络阻塞 push 降级为 P3** | 🔴 **装机无响应升级为 P0** | 🟡 **切输入法弹浮动栏 P1**
> **未做**: 装机 freeze 诊断 + 修复 + 浮动栏 UX 完善 + 真机 sandbox-verify
> **下个会话第一任务**: Brainstorm 装机 freeze 现象,跟 L##-PhaseL-7/8/9 已知 hang 模式对比,定位真因

---

## 0. 优先级调整 (2026-07-30 16:30+0800 用户调整)

| 旧优先级 | 新优先级 | 任务 |
|---|---|---|
| P0: git push + 真机 sandbox-verify v0.74 ship | **P3 (降级)** | 网络阻塞 + 用户明确 "安装文件存在 bug 时不适合推送" |
| 无 | **P0 (新增)** | 解决装机时无响应问题 |
| 无 | **P1 (新增)** | 实现切换火流猩输入法显示设置栏 |
| P0: 真机 sandbox-verify v0.74 | **P1 (降级)** | 等装机 freeze 真修后才跑 v0.74.1 verify |

**关键判断**: 装机 freeze 修之前,push 阻塞反而不算坏事 — 避免 ship broken installer。

---

## 1. Git 状态 (本会话末期,未变)

```
HEAD:    654d3b2 (fix(adr+installer-test): v0.74 evidence-gap closure)
ahead:   963 commits  (本地独有,等装机 bug 修后 + 网络恢复后 push)
behind:  831 commits  (远端独有,7-10~7-11 Claude 累积 push)
remote:  kizemo (HTTPS) — 已恢复
```

**3 commits 待 push (但 P3,等装机修后):**
- `654d3b2` (2 files, 207 ins / 34 del) — ADR 0003 scope amend + Test 6 source-level ship-gate
- `4ea9a14` (4 files, 164 ins / 19 del) — silent Abort + reorder + reset
- `4694a1d` (5 files, 721 ins / 17 del) — F4 stage-at-startup + 9.6 cleanup + ADR + test + lessons

**远端 backup branch 仍有效** (D 方案保底):
- `kizemo/Fluxing-backup-pre-2026-07-30 = 7d9f24b` ✅
- push 走 D 方案 = backup + force-push,零丢失

**working tree vs HEAD**:
- 0 source code changes (install.nsi 没动)
- 56 D release/*.exe (K7 push 失败累积,跟 ship 无关)
- 143 ?? untracked scratch (handoff / build_test / reports / docs/agents / etc,跟 ship 无关)
- librime submodule (0-byte pointer change,deps/googletest uncommitted)

---

## 2. 装机 freeze 历史脉络 (P0 任务背景)

**已知 hang 模式 lessons-learned.md** (按时间顺序):

| Lesson | 版本 | 真因 | 关键修复 |
|---|---|---|---|
| L##-PhaseL-7-PPL-IPC-Block | v0.67 | NSIS `ExecWait WeaselDeployer /install` hangs 17min when PPL-WeaselServer alive (NOT ctfmon mmap, NOT SCM RPC) | PPL taskkill 缩短 + ReadFile timeout |
| L##-PhaseL-8-Hotfix-Partial | v0.68 | hotfix 是 partial fix when x64 shim also locked | Rename-then-File + 9.5 stage-at-startup |
| L##-PhaseL-9-UX-Timing | v0.72 | "freeze" perception 实为冷启动延迟 (log 显示 25+ engine init 成功) | Ready-flag handshake |
| L##-PhaseL-9.5-Stage-At-Startup | v0.74 | silent-skip 路径装 shim 锁时 silent-skip → 装机 broken | F4 stage-at-startup (commit 4694a1d + 4ea9a14) |
| L##-PhaseL-9.6-System32-Legacy-Cleanup | v0.74 | v0.66/67 era System32 残留 weaselx64.dll/weasel.dll,Windows shell 弹 "no associated application" | .onInit + Section Uninstall 双向 cleanup (commit 4694a1d) |

**5 lessons 累积 = v0.74 ship 的 freeze mitigation 集合**。但用户反馈"装机无响应" = **v0.74 还有未修的 freeze 场景**。

**最可能的真因 (需 brainstorm 确认)**:
1. **F4 引入的新 race condition** — stage-at-startup + schtasks + post-reboot 三方时序可能引入新 hang
2. **silent Abort (SetErrorLevel 6001 + Quit) + 部署 automation 误判** — 4ea9a14 引入的新行为可能在某些场景误 abort
3. **TSF 进程 kill + WeaselServer 重启时序** — L66/L106 hardening 在某些 lockup 状态可能让 WeaselServer 死锁
4. **librime 冷启动 25+s 但 v0.74 期间更长** — 9.6 system32 cleanup 删除期间 Windows 重新 scan,可能让 librime init 更慢
5. **v0.74 装机流程某步骤 ExecWait 没 timeout** — L17 trap 但 v0.74 引入了新的 5 shim stage CopyFiles,可能有未加 timeout 的 ExecWait

---

## 3. 切输入法弹浮动栏 历史脉络 (P1 任务背景)

**已有 commits** (按时间顺序):
- `055cc6c` v0.63 feat(WeaselServer): Phase L 调整 2+3 (中文标点补全 + 切输入法弹浮动栏) — 首次实现
- `55e7991` v0.64 fix(WeaselServer): Phase L 装机 bugfix (中文标点生效 + 切输入法弹浮动栏)
- `84f01c5` v0.65 fix(WeaselServer+RimeWithWeasel): Phase L 装机 bugfix 2 (切输入法浮动栏真修)
- `476e77e` v0.69 fix(FloatingBar): QuickPanel 永久 dead 真修 (Phase L Bugfix 7.5)
- `c14d6ee` v0.70 fix(FloatingBar): Fix B revert (P2 violation)
- `v0.71+` freeze mitigation 期间浮动栏稳定

**关键文件**:
- `WeaselServer/QuickPanelDialog.cpp` — 浮动栏 (D2D 实现,见 `feedback_d2d_translucent_window.md`)
- `WeaselServer/QuickPanelDialog.h`
- `RimeWithWeasel/...` — 浮动栏触发逻辑 (commit 84f01c5 改过)

**v0.74 ship 时浮动栏状态**: 继承 v0.71+ 的稳定实现,理论上能弹。

**用户 P1 任务 "切火流猩显示设置栏" 的几种可能解读** (需 brainstorm 确认):
1. = "切输入法时弹 QuickPanel" (跟 v0.63-65 一致,可能行为没达到期望)
2. = "切输入法时弹 FloatingBar (状态栏)" (类似 Alt+, 切换)
3. = "切输入法时弹一个 settings 对话框" (新功能,设置项 UI)
4. = "切输入法时让 QuickPanel 持续显示" (toggle 模式)

---

## 4. 下个会话第一任务 (P0 装机 freeze)

### 4.1 Brainstorm (新会话第一步)

**必问问题** (用 AskUserQuestion 或 plain 询问):
1. **装机无响应的现象**:
   - 在哪一步无响应?installer UI 卡?WeaselServer 启动卡?librime engine init 卡?激活输入法卡?
   - 真机测试还是 sandbox-verify 失败?有 log 吗?`C:\Users\Duanyi\sandbox-artifacts\rime\logs\verify.log` 或 `output\verify-v066\*.log`?
   - 持续时间?几秒?几分钟?一直挂起?
   - Task Manager 看到的进程状态?WeaselServer.exe CPU?ctfmon.exe?WindowsSandbox?
2. **重现方式**:
   - 每次都无响应?还是概率性?
   - 干净机器 (全新 v0.74 装)?还是覆盖 v0.73 升级?
   - 用户侧动作触发?激活 Fluxing?切回英文?
3. **跟 L##-PhaseL-7/8/9 的相似度**:
   - 跟 v0.67 17min hang 像?还是 v0.72 25s 冷启动像?还是新模式?

### 4.2 调研 (Brainstorm 之后)

- 读 `WeaselServer/WeaselServer.cpp` main loop + startup sequence
- 读 `WeaselServer/QuickPanelDialog.cpp` 浮动栏触发
- 读 `output/install.nsi` F4 stage-at-startup + silent Abort
- 读 `RimeWithWeasel/WeaselTSF.cpp` TSF 激活
- grep `WaitForSingleObject` / `ReadFile` / `ExecWait` 看 timeout 设置

### 4.3 写 spec (调研后)

按 CLAUDE.md / `feedback_*` 规则,在 `.specify/specs/NNN-install-freeze-fix/spec.md` 写:
- **Why**: 用户 P0 优先级 + 装机 freeze 真因 + 影响范围
- **What / What not**: 修复目标 (装机不无响应) / 范围外 (新功能)
- **How**: 实施步骤 (startup timeout / Ready-flag 增强 / F4 race condition 排查 / installer timeout)
- **How to verify**: 真机 sandbox-verify + installer log + 5 binary md5

---

## 5. 浮动栏 UX (P1,等 P0 完成)

### 5.1 Brainstorm
- 当前 v0.74 浮动栏行为?用户期望?差异在哪?
- 是新功能 (settings 对话框) 还是已有功能的 UX 调整?

### 5.2 spec 路径
- `.specify/specs/NNN-floatingbar-ux/spec.md` (具体编号 P0 spec 后)

---

## 6. 关键文件路径 (下个会话必读)

| 路径 | 用途 |
|---|---|
| `.specify/memory/lessons-learned.md` | L##-PhaseL-7/8/9/9.5/9.6 详细根因 + How to apply |
| `output/install.nsi` | NSIS installer,F4 stage + 9.6 cleanup + silent Abort |
| `output/stage2-shim-install.bat.cmd` | Stage 2 batch (6150 bytes, F4 部分) |
| `WeaselServer/WeaselServer.cpp` | WeaselServer main + startup |
| `WeaselServer/QuickPanelDialog.cpp` | 浮动栏 D2D 实现 |
| `WeaselServer/PhrasesDialogIPC.cpp` | 已有 1 个 `WAIT_TIMEOUT` 用法可参考 |
| `RimeWithWeasel/WeaselTSF.cpp` | TSF 激活 + 浮动栏触发 |
| `docs/adr/0003-installer-shim-stage-at-startup.md` | F4 stage-at-startup ADR (375 行) |
| `_check_install_v3.ps1` | F4 ship-gate 验证 (3 mode) |
| `_check_system32_cleanup.ps1` | 9.6 ship-gate 验证 (Audit / Live / PretouchUninstall) |
| `test/TestInstallerShimStage/Test-Stage2ShimInstall.ps1` | Stage 2 batch tests (6/6 PASS) |
| `memory/2026-07-26-v066-disaster-rootcause.md` | v0.66 17min hang 详细根因 |
| `memory/2026-07-28-v068-ship.md` | v0.68 ship 教训 |
| `feedback_ppl_ipc_block_17min.md` | v0.67 17min hang 真因 |
| `feedback_l9_install_freeze_ux_timing.md` | v0.71 freeze UX timing 教训 |
| `feedback_nsis_silent_taskkill.md` | v0.67 Trap 4+5 教训 |
| `feedback_d2d_translucent_window.md` | QuickPanel D2D rendering 教训 |
| `feedback_phrases_p2_polish.md` | PhrasesDialog polish + hotkey subclass guard |

---

## 7. Task list (新优先级,下个会话接手)

| # | task | 优先级 | 状态 |
|---|---|---|---|
| 9 | 诊断装机无响应真因 | **P0** | pending (本会话未做) |
| 7 | 修复装机无响应 (基于诊断) | **P0** | pending (等 #9) |
| 10 | 完善切输入法弹浮动栏 UX | **P1** | pending (等 #7) |
| 6 | 真机 sandbox-verify v0.74.1 | **P1** | pending (等 #7 修完) |
| 8 | git push (网络恢复后 + bug 修完) | **P3** | pending (等 #6 PASS) |

---

## 8. Session health (本会话)

- bash calls: ~30 (红线 80,健康)
- subagent dispatched: 0 (不需要)
- active < 1.5h (接手自上一会话,符合 2h 规则)
- 0 装机 freeze 实际诊断 (用户才告知 P0 调整)
- 0 浮动栏 UX 工作 (等 P0 修完)

---

*End of priority-shift handoff. v0.74 ship 暂停,转入装机 freeze 真因诊断 + 修复。*

**重要**: 装机 freeze 修之前不 push (用户明确指示),push 阻塞在网络反而不是当前主要矛盾。
