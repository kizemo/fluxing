# Bug Fix → Ship 工作流 (Fluxing 复用)

> **何时使用**: User 报告任何 bug, 装机后行为异常, 或 user 反馈"修改没生效"等。
> 严格按 5 phase 跑, 每 phase 用对应 skill, 不跳 phase, 不在 phase 1 提前 commit。
>
> **核心铁律 (verification-before-completion)**: **没新鲜 evidence, 不 claim 完成**。

## 5 Phase 流程 (按顺序, 不可跳)

### Phase 0: 收集 user 反馈 (Read-only, ≤ 5 min)

- **读**: user 报告原文, 不做"spirit interpretation"
- **列**: 用户具体动作 + 期望结果 + 实际结果 + 截图/错误信息
- **复现**: 写 `_repro.ps1` (PowerShell) 或 `_repro.sh` (bash) 实际跑 user 报的路径

### Phase 1: 多角度 subagent root cause (Read-only, ≤ 30 min)

按 user 反馈"合适的 subagents 互不干扰并行" + "不同技能多角度", **dispatch 多个 subagent 并行**:

| Subagent | 角度 / skill | 任务 |
|---|---|---|
| A | `systematic-debugging` | 5 phase root cause (reproduce → hypothesis → verify) |
| B | `frontend-ui-engineering` | UI / paint / chrome / icon 设计 |
| C | `subagent-driven-development` | 多 subagent 并行 dispatch 协调 |
| D | `verification-before-completion` | 装机 binary md5 verify + 真 user flow 4 hotkey |
| E | `git-workflow` | source / binary 改动是否遵守 Conventional Commits + AGENTS.md |

每 subagent 输出独立 report. **不互相干扰**, main session 收集 report 后决定 fix 方案。

### Phase 2: 制定修复方案 (共识 + 排序, ≤ 15 min)

- **主 session** (你) 收集所有 subagent report
- **列**: 每个 user 报告 bug 对应真 root cause + 改动 file:line + priority (P0 blocker / P1 high / P2 polish)
- **排序**: P0 先修, P1 跟修, P2 推后
- **不** 在 phase 2 改任何 code. 改前先 dispatch phase 3 (fix) 确认方案

### Phase 3: 代码修复 (subagent + TDD, ≤ 60 min per subagent)

按 phase 2 排序 dispatch fix subagent:

- **TDD style**: 修前先看现有 test 跑通 → 改 → 跑同一 test → 加新 test 覆盖修后
- **code review subagent** (clean-code-guard skill) 必须 review 后才能进 phase 4
- **review 必须 PASS or CONDITIONAL PASS** (有 must-fix list)
- **不** 在 phase 3 commit. commit 在 phase 4 真 verify 通过后

### Phase 4: 真 verify (3 处 verify, ≤ 30 min)

| verify 维度 | 命令 | 期望 |
|---|---|---|
| **a. build** | `_buildflow.cmd` (Win32 only) | exit 0, 0 new warnings |
| **b. unit test** | `cmd //c "Release\\TestXxx.exe"` | 全 PASS (e.g. TestPhrasesDialog 69/69) |
| **c. e2e** | `cmd //c "Release\\v0_19_0_32_e2e.exe"` | 全 PASS (≥ 78/0) |
| **d. md5 parity** | md5 src build == md5 7z extract | 完全一致 |
| **e. 真 user flow** | `_final_verify.ps1` 跑 4 hotkey + 3 button click | 全 visible + children count |
| **f. installer 含 fix** | 7z extract installer → verify 含 fix (md5 变) | 装机 binary md5 跟 source build 一致 |

**5/5 PASS 才进 phase 5**。任一 FAIL → **回退** (git revert) + 重新分析

### Phase 5: Ship (≤ 10 min, 必须 phase 4 PASS 才执行)

- **commit 1**: `chore(release): v<VERSION>-<BUILD>-fluxing installer (REBUILD with <fix description>)` (含 binary installer)
- **commit 2+**: 每 fix 一个 commit, Conventional Commits + module scope (`fix(WeaselServer):` / `fix(PhrasesDialog):` / `fix(UserDictionary):` 等)
- **tag**: `git tag -a v0.19.0.33-fluxing -m "Phase A+B fix alt+/ route, body paint, log file, clean-code review"`
- **release**: cp `release/fluxing-0.19.0.33-installer.exe` (已 git tracked, 自动 push) → user 装机

## 装机 step (user 端, ≤ 5 min)

1. 清注册表:
   ```
   reg delete "HKCU:\Software\Fluxing" /f
   reg delete "HKLM:\SOFTWARE\WOW6432Node\Fluxing" /f
   ```
2. 双击 `release\fluxing-0.19.0.33-installer.exe` (或 silent: `release\fluxing-0.19.0.33-installer.exe /S /D=D:\Program Files\fluxing`)
3. 重启电脑 (确保 mmap 释放)
4. 测 4 hotkey: `Alt+.` / `Alt+/` / `Ctrl+Shift+U` / `Ctrl+Shift+K` + QuickPanel 3 button click (Phrase / UserDict / Shortcut)
5. (可选) verify binary md5: `certutil -hashfile "D:\Program Files\fluxing\weasel\WeaselServer.exe" MD5` 期望 `3474db38c0030901b991c6c634f473b7`
6. (可选) 看 hotkey 失败 log: `notepad "%APPDATA%\Rime\weasel-install.log"`

## 复用方式 (何时 + 怎么 run)

| 场景 | 跑哪个 phase | 工具 |
|---|---|---|
| User 报"修改没生效" / "binary 没装" | **Phase 0 + Phase 4f** (装机 binary md5 verify) | 1 个 subagent: `verification-before-completion` |
| User 报具体 bug (行为错) | **Phase 1 + Phase 2 + Phase 3 + Phase 4 + Phase 5** | 5+ subagent (本工作流) |
| Pre-release 必查 (ship gate) | **Phase 4 全 5 项** | `verification-before-completion` subagent |
| Refactor 提议 (user 没报 bug) | **Phase 1B + Phase 3 + Phase 4** (skip 0 + 5) | `clean-code-guard` subagent + 真 verify |

### 关键铁律 (不破)

1. **没新 evidence, 不 claim 完成** (verification-before-completion)
2. **每 phase 输出独立 report**, 不混 phase
3. **phase 3 不 commit**, phase 5 唯一 commit 时机
4. **5 phase 全 PASS 才进 phase 5**
5. **installer ship candidate 必须 7z extract verify md5 == source build md5**
6. **user 装机后真 user flow 验证** (`_final_verify.ps1`) 是装机 binary 真能 work 的**唯一** source of truth, sandbox e2e **不能** 替代

## 关键文件 (按 phase)

| Phase | 关键文件 / 命令 |
|---|---|
| Phase 0 | `_repro.ps1` / `_repro.sh` (写 user 报的路径) |
| Phase 1 | 5 个 subagent report (用 `.claude/skills/dispatching-parallel-agents`) |
| Phase 2 | 你的 main session (汇总 subagent report, 列 priority) |
| Phase 3 | `WeaselServerApp.cpp` / `PhrasesDialog.cpp` / `UserDictionary.cpp` / `install.nsi` (按 bug) |
| Phase 4a | `_buildflow.cmd` (Win32 build, 跳过 x64 librime 失败) |
| Phase 4b | `Release\TestPhrasesDialog.exe` / `TestUserDictionary.exe` / `TestUserDictUpdate.exe` |
| Phase 4c | `Release\v0_19_0_32_e2e.exe` |
| Phase 4d | `md5sum output/Win32/WeaselServer.exe output/_extracted_v6/WeaselServer.exe` (md5 完全一致) |
| Phase 4e | `_final_verify.ps1` (4 hotkey + 3 button click + child controls count) |
| Phase 4f | 装机 binary md5 (user 跑 certutil) + `%APPDATA%\Rime\weasel-install.log` |
| Phase 5 | `release/fluxing-0.19.0.33-installer.exe` (git tracked) → cp 给 user |

## L100+ Lessons Learned (本工作流沉淀)

- **L100-Q**: sandbox 增量 build (`_buildflow.cmd`) 跟 `build.bat installer` 是**两个不同 build path**, sandbox build OK 不代表 installer build OK
- **L100-R**: installer 装 binary ≠ source build (build.bat x64 link 失败时 NSIS 跳过装, 老 binary 保留)
- **L100-S**: sandbox 真 user flow 测 4 hotkey 用 `_final_verify.ps1` (SendMessage WM_HOTKEY 给 WeaselIPCWindow_1.0, EnumChildWindows 抓 child controls) — **不能** 替代装机手测
- **L100-T**: macOS-style glass chrome 在 Windows `WS_EX_LAYERED + UpdateLayeredWindow(ULW_ALPHA)` 模式下, child control per-pixel alpha 合成丢 → body 透明. 改走 `BeginPaint/EndPaint` + `WS_EX_COMPOSITED` 跟 ShortcutSettings 同款
- **L100-U**: sandbox `grep "string"` 在 Release strip binary 中**找不到** C++ function symbol (mangled), 必须 `dumpbin /symbols` 查 mangled name. 或 `strings` 查 string literal (但 strip 后也没)
- **L100-V**: alt+/ 路由 source-level 改 **不能** 误以为 "commit message 提到" = 真改. 必须 `git log -S "newSymbol"` 查真 binary
- **L100-W**: 装机 binary md5 必须**三方一致** (src build = installer 装 = extracted binary). 任何不一致 = 装机 binary 不对
- **L100-X**: `taskkill /F` 必须 retry 3 次 (autorun-respawn race). NSIS `File` 默认 silently 失败, 必须 `SetOverwrite on` + L72-fix Rename-then-File

## 触发 skill 列表 (按 phase)

| Phase | 必用 skill | 触发词 |
|---|---|---|
| 0 | - | - |
| 1 | `systematic-debugging`, `subagent-driven-development`, `frontend-ui-engineering`, `verification-before-completion`, `git-workflow` | "dispatch subagent", "多角度分析", "bug 根因", "code review" |
| 2 | `brainstorming` (轻量), `writing-plans` | "fix 方案", "priority 排序" |
| 3 | `tdd`, `clean-code-guard`, `frontend-ui-engineering`, `code-review` | "代码修复", "code review 5 axis" |
| 4 | `verification-before-completion` (核心), `test-driven-development`, `using-superpowers` | "真 verify", "装机 binary md5", "真 user flow" |
| 5 | `git-workflow`, `finishing-a-development-branch` | "Conventional Commits", "ship ready" |

---

**Created**: 2026-07-18 (Fluxing v0.19.0.33 Phase A+B 修 bug 工作流)
**Use for**: 任何 user-reported bug, 装机后行为异常, pre-release ship gate
**Owner**: Fluxing maintainer
**Version**: 1.0 (initial)
