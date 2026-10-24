# Handoff — Spec 071 P1 完成 + T103 PASS,接力 T101 / P3 (2026-07-30)

> **承接**: `handoff-after-priority-shift-p0-install-freeze-2026-07-30.md`
> **当前**: spec/plan/tasks.md 写完 (`.specify/specs/071-install-freeze-fix/`),P1 4 task 全完,T103 + T104 已 PASS
> **未做**: T101 (F4 5 unique schtasks) + T102 (TestStage 升级) + T201/T202 (Ready-flag MsgBox + test) + T203 (ADR amend) + T204 (lessons 9.7/9.8) + T205 (ship-gate)
> **下个会话第一任务**: T101 改 5 个 schtasks task name (FR-003),然后 P3 polish + 真机 sandbox-verify v0.74.1

---

## 0. 进度 (2026-07-30 16:30+0800 用户调优先级 → 现在 ~17:30)

| # | task | 状态 |
|---|---|---|
| #9 (Task #2) | 诊断装机 freeze 真因 | ✅ completed (forensics 高 confidence) |
| #7 (Task #1) | 修复装机 freeze (基于 #9) | 🟡 in_progress · P1 全完 + T103/T104 PASS,剩 T101+T102+T201-205 |
| #10 (Task #4) | 浮动栏 UX | pending (等 #7 完) |
| #6 (Task #5) | 真机 sandbox-verify v0.74.1 | pending (等 #7 完) |
| #8 (Task #3) | git push | P3 等 #6 + 网络恢复 |

---

## 1. 已 ship 安装器代码 (本会话)

### 1.1 output/install.nsi (T001 + T002 + T103)

**T001 9.6 cleanup glob 扩展** (L##-PhaseL-9.7 fix):

- `.onInit` 9.6 cleanup 块改为 `Call CleanupLegacySystem32Weasel`
- `Section Uninstall` 9.6 cleanup 块改为 `Call CleanupLegacySystem32Weasel`
- 新增 `Function CleanupLegacySystem32Weasel` 在 Function ForceFluxingSuffix 之前:
  - Push $R0-$R3,$R8 / Pop 兼容 caller
  - exact-match fallback (weaselx64.dll + weasel.dll)
  - `FindFirst $R0 $R1 "$WINDIR\System32\weasel*.dll"` + `loop_weasel_dll:` + `${GetSize}` ≥ 100000 + `Delete /REBOOTOK` + `FindNext` + `FindClose`
  - `FindFirst $R0 $R1 "$WINDIR\System32\weasel*.ime"` + `loop_weasel_ime:` 同上
  - 全部 DetailPrint log + size 报告

**T002 emoji.json 路径补装** (L##-PhaseL-9.8 fix):

- install.nsi 在 line 818 File block 之后加 verification log:
  ```
  StrCpy $R8 "$INSTDIR\data\opencc\emoji.json"
  IfFileExists $R8 emoji_json_ok
    DetailPrint "Fluxing v0.74.1: WARNING emoji.json missing at $R8 (OpenCC emoji conversion unavailable; emoji.ocd companion absent in build output)"
  emoji_json_ok:
  ```
- (Uninstall mirror 已有 `$INSTDIR\data\opencc\*.*` Delete,no new code)

**T103 silent Abort + DetailPrint** (FR-004):

- line 1017 `IfSilent 0 +4` 改为 `IfSilent 0 +7` (3 new lines 增加)
- 在 `SetErrorLevel 6001` + `Quit` 之前加:
  ```
  DetailPrint "Fluxing v0.74.1: deferred shim swap pending; silent exit 6001 (sandbox/CI detect reboot required)"
  FileOpen $9 "$INSTDIR\fluxing-install-silent.log" a
  FileWrite $9 "Fluxing v0.74.1: deferred shim swap pending; reboot required after this silent install$\r$\n"
  FileClose $9
  ```

### 1.2 _check_system32_cleanup.ps1 (T003)

- Header comment 加 L##-PhaseL-9.7 reference
- `$anchorsInit` 7 anchors: `v0.74.1 (Phase L Bugfix 9.6-Glob)` + `Call CleanupLegacySystem32Weasel` + `Function CleanupLegacySystem32Weasel` + `FindFirst ... weasel*.dll` + `FindFirst ... weasel*.ime` + `${GetSize}` + `100000`
- `$anchorsUninstall` 2 anchors: `v0.74.1 (Phase L Bugfix 9.6-Glob): symmetric cleanup` + `Call CleanupLegacySystem32Weasel`
- Live mode: 枚举 `weasel*.dll` + `weasel*.ime` (替代旧 hard-coded `weaselx64.dll` + `weasel.dll`)
- PretouchUninstall: 创建 10 个 fake legacy 文件 (200 KB 每个,大于 100KB threshold 防止 skip)

### 1.3 新 test 目录

| Path | 状态 |
|---|---|
| `test/TestOpenccDataPath/Test-OpenccDataPath.ps1` | ✅ PASS |
| `test/TestSilentAbortDetailPrint/Test-SilentAbortDetailPrint.ps1` | ✅ PASS |

### 1.4 `.specify/specs/071-install-freeze-fix/` (spec-init 3 文件 + spec-check)

- spec.md (R2-compliant tech-agnostic)
- plan.md (Constitution Check, 5 tech approach, File-level changes, Build/Test/Release impact, Risk mitigation)
- tasks.md (P1 4 + P2 4 + P3 5 + Verification 10)
- spec-check 报告: 🔴 BLOCKER 0, 🟡 WARNING 0, 🟢 INFO 3

---

## 2. 下个会话第一任务 (T101)

### 2.1 T101: F4 5 unique schtasks (FR-003)

**位置**: `output/install.nsi:517-518` (pre-install) + `output/install.nsi:557,604,664,712,750` (5 stage blocks) + `output/stage2-shim-install.bat.cmd`

**修改** (按 spec 071 FR-003):

1. **Pre-install defensive cleanup** (line 517 之前加):
   ```
   ; v0.74.1: defensive cleanup of all 5 F4 task names + old name
   nsExec::ExecToStack 'schtasks /Delete /TN FluxingStage2ShimSwap /F 2>nul'
   nsExec::ExecToStack 'schtasks /Delete /TN FluxingStage2ShimSwap_weasel /F 2>nul'
   nsExec::ExecToStack 'schtasks /Delete /TN FluxingStage2ShimSwap_weaselx64 /F 2>nul'
   nsExec::ExecToStack 'schtasks /Delete /TN FluxingStage2ShimSwap_server /F 2>nul'
   nsExec::ExecToStack 'schtasks /Delete /TN FluxingStage2ShimSwap_deployer /F 2>nul'
   nsExec::ExecToStack 'schtasks /Delete /TN FluxingStage2ShimSwap_setup /F 2>nul'
   ```

2. **5 stage blocks** each:
   - `schtasks /Create /TN FluxingStage2ShimSwap` → `FluxingStage2ShimSwap_<shim>`
   - `/TR "cmd /c $\"..."` 改为 `/TR "cmd /c $\"set SHIM=<shim> && $\"$\"$TEMP\\fluxing-stage2-shim\\stage2-shim-install.bat.cmd$\"$\""`
   - 注意: NSIS 的 cmd escape 比较棘手,需要测试

3. **stage2-shim-install.bat.cmd**:
   - 加 SHIM env var 解析 (`set "SHIM=%SHIM%"` or `%~1`)
   - Per-shim pending.json: `set "PENDING=%STAGE_DIR%\%SHIM%.json"` (替换原 all-shares)
   - file append: pending.json 由每个 stage block 写自己一行 (`'<shim>.dll$\r$\n'`)
   - boot 时 5 任务并行跑 (no shared file write race)

### 2.2 T102: TestInstallerShimStage 升级 add Test 7

**位置**: `test/TestInstallerShimStage/Test-Stage2ShimInstall.ps1`

**Test 7 验证**:5 unique `FluxingStage2ShimSwap_<shim>` task names 在 install.nsi 中

```powershell
$expectedNames = @(
    'FluxingStage2ShimSwap_weasel',
    'FluxingStage2ShimSwap_weaselx64',
    'FluxingStage2ShimSwap_server',
    'FluxingStage2ShimSwap_deployer',
    'FluxingStage2ShimSwap_setup'
)
foreach ($name in $expectedNames) {
    if ($nsiText.Contains($name)) { ok }
    else { fail }
}
```

### 2.3 P3 (T201-T205) 接力顺序

- T201 改 install.nsi:970-974 Ready-flag MessageBox 包裹 `IfSilent skip_warning_ready` jump
- T202 test/TestReadyFlagMessageBox/ 新 test (手测触发跳过 Ready 写,弹 MessageBox)
- T203 docs/adr/0003-...  amend schtasks + glob cleanup §Postscript
- T204 lessons-learned.md 加 L##-PhaseL-9.7 + L##-PhaseL-9.8
- T205 ship-gate: env.bat / weasel.props bump v0.74 → v0.74.1 + 5 binary md5 ≠ previous + git status -sb clean

### 2.4 真机 sandbox-verify (Task #6, P1)

- T101-T205 全部 ship
- `_buildflow.cmd` (L108 fix: rebuild from scratch)
- `_check_system32_cleanup.ps1 -Mode Audit` + Live mode (在真机 System32 有 11 个 fake files 现成)
- `rime-verify.ps1 -InstallerPath <v0.74.1 installer>` sandbox 跑
- 验证 5 binary md5 ≠ v0.74 + Ready-flag 30s 内 + 切输入法 ≤ 1s

---

## 3. 关键文件 + 行号参考

| 文件 | 行 | 用途 |
|---|---|---|
| `output/install.nsi:232-247` | .onInit 9.6 cleanup | T001 ✓ |
| `output/install.nsi:1074-1086` | Section Uninstall 9.6 cleanup | T001 ✓ |
| `output/install.nsi:818-829` | emoji.json verification | T002 ✓ |
| `output/install.nsi:1017-1027` | silent Abort + DetailPrint | T103 ✓ |
| `output/install.nsi:517-518` | pre-install pending.json cleanup | T101 待改 |
| `output/install.nsi:557,604,664,712,750` | 5 stage blocks | T101 待改 |
| `output/install.nsi:970-974` | Ready-flag handshake | T201 待改 |
| `output/install.nsi:1098+` | Function CleanupLegacySystem32Weasel | T001 ✓ |
| `output/stage2-shim-install.bat.cmd:1-153` | stage 2 batch | T101 待改 |
| `_check_system32_cleanup.ps1:1-200` | ship-gate | T003 ✓ |
| `test/TestOpenccDataPath/...` | emoji.json verification | T004 ✓ |
| `test/TestSilentAbortDetailPrint/...` | silent abort | T104 ✓ |

---

## 4. 已通过的 verification

| Test | Mode | Result |
|---|---|---|
| TestOpenccDataPath | n/a | ✅ PASS 6/6 (3 install.nsi anchors + emoji.json file + .ocd files count + comment) |
| TestSilentAbortDetailPrint | n/a | ✅ PASS 6/6 (IfSilent 0 +7 + DetailPrint + FileOpen + FileWrite + FileClose + SetErrorLevel/Quit) |
| _check_system32_cleanup | Audit | ✅ PASS 8/8 (7 .onInit + 2 Section Uninstall + Trap 5 + L66 + L106) |
| _check_system32_cleanup | PretouchUninstall | ✅ PASS - 1 fake created (weasel.dll),9 already existed |

---

## 5. 必交付 (接力下个会话)

| # | 必交付 |
|---|---|
| T101 | F4 5 unique schtasks in install.nsi + stage2 batch |
| T102 | TestInstallerShimStage Test 7 |
| T201 | Ready-flag MessageBox |
| T202 | TestReadyFlagMessageBox |
| T203 | ADR 0003 amend |
| T204 | lessons-learned L##-PhaseL-9.7 + L##-PhaseL-9.8 |
| T205 | env.bat / weasel.props bump v0.74 → v0.74.1 + git status -sb clean |
| #6 | sandbox-verify 真机 (Task #6 P1) |

---

## 6. 禁止事项

- 不 push (P3 等 #6 + 网络恢复)
- 不复制 v0.63-65 浮动栏代码 (那是 #10 P1,跟 P0 无关)
- 不修改 librime / plum submodule pointer
- 单回合 diff >500 行 → 拆分
- 写 .nsi 5 schtasks task name 注意 NSIS cmd escape (`$"`) 仔细测试

*End of P1 + T103 handoff. 接力下个会话:T101 → P3 → 真机 sandbox-verify → sftp ship.*
