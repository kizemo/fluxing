# 071 - Tasks

## P1 (must) - 真因真修

- [ ] **T001** [US001-A / US002-A / FR-001] 9.6 cleanup glob 扩展
  - 改 `output/install.nsi:240-247` (`.onInit`) 加 `FindFirst` loop + size > 1 MB 防御
  - 镜像改 `output/install.nsi:1078-1085` (`Section Uninstall`)
  - 镜像改 `output/uninstall.nsi` (.v6x9x9x uninstall 镜像)
  - 验证: `_check_system32_cleanup.ps1 -Mode Audit` 输出 0 FAIL
  - < 1.5h · 3 files

- [ ] **T002** [US001-A / US003-A / FR-002] emoji.json 路径补装
  - 改 `output/install.nsi:818` 后新增一段 `SetOutPath $INSTDIR\weasel\data\opencc` + `File data\opencc\*.json` + oc*
  - 改 `output/install.nsi:1062` (uninstall) 加 mirror `Delete "$INSTDIR\weasel\data\opencc\*.*"`
  - 验证: 真机 sandbox-verify 后 rime.weasel log 0 `opencc config not found` ERROR
  - < 30 min · 2 files

- [ ] **T003** [US002-A / SC-001] `_check_system32_cleanup.ps1` 扩展 glob anchor
  - 加 `weasel*.ime` glob + 老 backup 文件名 anchor (`.old.###` / `.v###` / `.bak`)
  - 验证: 加 `FindFirst` test case,11 个 v0.66/67/68 文件全部命中
  - < 30 min · 1 file

- [ ] **T004** [US003-A / SC-003] TestOpenccDataPath 新 test
  - `test/TestOpenccDataPath/test.ps1` — sandbox 装后 verify rime.weasel log 0 ERROR
  - 验证: `Find /C "opencc config not found" rime.weasel.INFO.*.log` = 0
  - < 30 min · 1 file new

## P2 (should) - F4 verify / silent abort

- [ ] **T101** [US004-A / FR-003] F4 5 unique schtasks 重命名
  - 改 `output/install.nsi` 5 个 stage blocks (557/604/664/712/750) — task name 加 shim 后缀
  - 改 `output/stage2-shim-install.bat.cmd` — 加 SHIM env var / arg 解析 + per-shim pending.json
  - 改 `output/install.nsi:517-518` 段 — pre-install 6 次 schtasks /Delete 防御清理 (含老 task name)
  - 验证: `schtasks /Query /FO LIST /TN FluxingStage2ShimSwap*` 输出 5 unique
  - < 1.5h · 2 files

- [ ] **T102** [US004-A / SC-004] TestInstallerShimStage 升级 5 unique test
  - `test/TestInstallerShimStage/Test-Stage2ShimInstall.ps1` 加 `[Test 7] 5 unique schtasks per shim` (用例 7)
  - 验证: run test,5/5 unique 通过
  - < 30 min · 1 file

- [ ] **T103** [US005-A / FR-004] silent Abort + DetailPrint
  - 改 `output/install.nsi:1003-1006` — 加 `DetailPrint` + `FileOpen "$INSTDIR\fluxing-install-silent.log" a` + `FileWrite` + `SetErrorLevel 6001` + `Quit`
  - 验证: silent install + 故意 staged + verify.log 命中 "reboot required deferred"
  - < 20 min · 1 file

- [ ] **T104** [US005-A / SC-005] TestSilentAbortDetailPrint 新 test
  - `test/TestSilentAbortDetailPrint/test.ps1` — verify silent exit 6001 + DetailPrint 落盘
  - 验证: invoke silent installer, parse verify.log, 命中 "reboot required deferred"
  - < 30 min · 1 file new

## P3 (could) - UX polish + docs

- [ ] **T201** [US006-A / FR-005] Ready-flag MessageBox (non-silent)
  - 改 `output/install.nsi:970-974` — `IfSilent skip_warning_ready` jump 包裹 MessageBox
  - 验证: 真机 + skip Ready write + install → MessageBox 弹出
  - < 30 min · 1 file

- [ ] **T202** [US006-A / SC-006] TestReadyFlagMessageBox 新 test
  - `test/TestReadyFlagMessageBox/test.ps1` — 真机 skip Ready + install → 检查 MessageBox 出现
  - 验证: run test,MessageBox 出现
  - < 1h · 1 file new

- [ ] **T203** ADR 0003 amend
  - 改 `docs/adr/0003-installer-shim-stage-at-startup.md` — 加 §Postscript "Why schtasks rename" + "Why glob cleanup"
  - < 30 min · 1 file

- [ ] **T204** lessons-learned add L##-PhaseL-9.7 + L##-PhaseL-9.8
  - L##-PhaseL-9.7: 9.6 cleanup glob insufficient (本任务真因)
  - L##-PhaseL-9.8: opencc data path mismatch (本任务真因)
  - < 30 min · 1 file

- [ ] **T205** 5 binary md5 验证 + git status -sb + git diff --stat
  - 必走 (CLAUDE.md §2 + AGENTS.md §3)
  - 验证: 5 binary md5 ≠ v0.74 + `git status -sb` clean + `git diff --stat` 不含 weasel.props / env.bat / *.obj
  - < 10 min · manual

## Verification (mandatory before commit)

- [ ] **V001** `pwsh -File _check_system32_cleanup.ps1 -Mode Audit` → 0 FAIL
- [ ] **V002** `pwsh -File _check_system32_cleanup.ps1 -Mode Live` → 0 FAIL
- [ ] **V003** `pwsh -File test/TestInstallerShimStage/Test-Stage2ShimInstall.ps1` → 11/11 PASS
- [ ] **V004** `pwsh -File test/TestOpenccDataPath/test.ps1` → 0 `opencc config not found` ERROR in log
- [ ] **V005** silent install + 故意 stage + check log 含 "reboot required deferred"
- [ ] **V006** 真机 install + skip Ready write → MessageBox 弹出
- [ ] **V007** 5 binary md5 ≠ v0.74 (L17 ship gate)
- [ ] **V008** `git status -sb` + `git diff --stat` clean
- [ ] **V009** env.bat `RELEASE_BUILD=1` + 禁 `git describe --tags`
- [ ] **V010** Conventional Commit message format 合规 (P4)

## Out-of-scope followups (next specs)

- **spec 072**: QuickPanelDialog / 浮动栏 UI polish (Task #10 P1)
- **spec 073**: installer robustness 进阶 (multi-user, sandbox 隔离, etc.)
- spec 074+: uninstall.exe readiness 检测 + 完整 readiness 测试套件

## Tasks 总量

- P1: 4 tasks (T001-T004) — 真因真修,必 ship
- P2: 4 tasks (T101-T104) — F4 verify + silent abort,应 ship
- P3: 5 tasks (T201-T205) — UX polish + docs,could ship

单 task 估算 ≤ 1.5h,总 ≤ 8h 干活 + 1h 真机 verify。
R5: 每 task ≤ 4h ✓ · 1-3 files ✓ · 有 I/O ✓
