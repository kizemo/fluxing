# Handoff — Spec 071 真因修完 + Hook blockers 修完 + T101/P3 ship 待续 (2026-07-30)

> **承接**: `handoff-after-spec-071-p1-and-t103-p2-2026-07-30.md` + `handoff-after-priority-shift-p0-install-freeze-2026-07-30.md`
> **状态**: ✅ Spec 071 + P1 (T001-T004) + P2 T103/T104 + L##-9.7/9.8 + Hook 3 BLOCKERS 修完。🟡 T101/T102/T201/T202/T203/T205 等下个会话。❌ 真机 sandbox-verify v0.74.1 (Task #6) 等代码全完。❌ 不 push (P3)
> **下个会话第一任务**: T101 (F4 5 unique schtasks) — `output/install.nsi:557,604,664,712,750` + `output/stage2-shim-install.bat.cmd` 加 SHIM env var

---

## 0. 状态快照 (2026-07-30 16:30-18:30 user dialogue + hook feedback)

| Item | Status |
|---|---|
| Spec 071 sign-off | ✅ user OK |
| P1 真因真修 (T001-T004) | ✅ PASS |
| T103 silent Abort + DetailPrint + T104 test | ✅ PASS |
| Hook 3 BLOCKERS + 2 SUGGESTIONS | ✅ ALL FIXED |
| L##-PhaseL-9.7 + 9.8 沉淀 | ✅ written |
| `_check_system32_cleanup.ps1 -Mode Audit` | ✅ PASS 8/8 + 3 cross-checks (Trap 5 / L66 / L106) |
| `TestOpenccDataPath` | ✅ PASS 6/6 |
| `TestSilentAbortDetailPrint` | ✅ PASS 7/7 |
| `_check_system32_cleanup.ps1 -Mode PretouchUninstall` | ✅ 11 fake files in System32 (10 真实 + 1 skip 已存在) |

---

## 1. 装机 freeze 真因 (forensics 验证)

**真因**: **L##-PhaseL-9.6 9.6 cleanup 不全** + **L##-PhaseL-9.7 GetSize "S" hidden bug**
- v0.66/67/68 era 在 System32 留了 11 个 backup-pattern DLL (`.old` / `.bak` / `.v465` / `.v466bak-YYYYMMDD-HHMMSS` / `.ime`)
- v0.74 cleanup 只识别精确 `weasel.dll` + `weaselx64.dll` 2 个文件名 → 11 个 backup **全漏**
- 而且 cleanup helper 的 `${GetSize} "S"` 返回 sectors 不是 bytes。`R3 >= 100000` 阈值变成 "≥ 50 MB"。1.16 MB DLL = 2273 sectors 永远小于 → **cleanup 静默 no-op**
- 双重 bug → 11 个 backup 残留 → Windows shell loader / CTF 在切换输入法时反复 retry → 用户感知"几分钟 ~ 17min freeze"

**L##-PhaseL-9.8 opencc emoji.json 路径错位**: installer 装到 `$INSTDIR\data\opencc\` (= `$R3\weasel\data\opencc\`) 理论上对,但 `emoji.ocd` 配套 binary data 缺失 → libRime OpenCC loader 走 `config not found` fallback。Defense-in-depth 二次 install 已加。

---

## 2. 已 ship 代码层 (本会话)

### 2.1 output/install.nsi (5 sections mod)

| Line | Section | 内容 |
|---|---|---|
| 232-247 | `.onInit` 9.6 cleanup | 改 `Call CleanupLegacySystem32Weasel` + L##-9.7 comment |
| 815-836 | opencc install | 加 FR-002 二次 `SetOutPath $INSTDIR\weasel\data\opencc` + emoji.json verification log |
| 1017-1030 | silent Abort | `IfSilent 0 +7` + DetailPrint + FileOpen + FileWrite `[$DATE-$TIME]` + FileClose + SetErrorLevel 6001 + Quit |
| 1104-1107 | `Section Uninstall` 9.6 cleanup | 改 `Call CleanupLegacySystem32Weasel` (mirror) |
| 1117-1184 | `Function CleanupLegacySystem32Weasel` | Push/Pop + exact-match fallback + `FindFirst $WINDIR\System32\weasel*.dll` + `loop_weasel_dll:` + `${GetSize} "" $R2 $R3` (bytes!) + `$R3 >= 102400` + Delete /REBOOTOK + else DetailPrint SKIPPED + FindNext + FindClose + 同样 .ime |

### 2.2 _check_system32_cleanup.ps1

- Header comment 加 L##-9.7 reference
- `$anchorsInit` 9 anchors (含 `"" $R2 $R3` + `102400` 防 GetSize-units regression)
- `$anchorsUninstall` 2 anchors
- Live mode 枚举 `weasel*.dll` + `weasel*.ime` (替代旧 2 个精确)
- PretouchUninstall 创建 10 fake files 200 KB > 100 KB threshold

### 2.3 新 test 目录

- `test/TestOpenccDataPath/Test-OpenccDataPath.ps1` — 6/6 PASS
- `test/TestSilentAbortDetailPrint/Test-SilentAbortDetailPrint.ps1` — 7/7 PASS

### 2.4 .specify/specs/071-install-freeze-fix/

- spec.md (R2-compliant tech-agnostic)
- plan.md (Constitution Check, tech approach, file-level changes, build/test/release)
- tasks.md (P1 4 + P2 4 + P3 5 + V001-V010)
- spec-check 报告: 🔴 0, 🟡 0, 🟢 3

### 2.5 .specify/memory/lessons-learned.md (新增 L##-9.7 + 9.8)

- **L##-PhaseL-9.7-System32Cleanup-Glob-Incomplete**: 9.6 cleanup 模式不全 + GetSize "S" hidden bug。Hook 抓的隐藏 bug!
- **L##-PhaseL-9.8-OpenCC-Data-Path-Mismatch**: emoji.json + emoji.ocd 配套 binary data 缺失 + defense-in-depth 二次 install。

---

## 3. Hook feedback 处理 (2026-07-30)

| BLOCKER | Fix |
|---|---|
| #1 Spec-vs-code divergence (FR-002) | 加 `SetOutPath $INSTDIR\weasel\data\opencc` 二次 install per spec literal |
| #2 `${GetSize}` "S" units hidden bug | 改 `${GetSize} ""` (bytes) + 阈值 `102400`。Hook 还发现这**可能就是 v0.66/67/68 era 9.6 cleanup 漏 11 个 backup 的真因**。沉淀为 L##-9.7 |
| #3 L66 reg query verification | PowerShell 跑了 3 个 reg query,全部 absent(用户已卸载,符合预期) |

Suggestions:
- ✅ Skipped small file `else DetailPrint` 加进 helper (forensic 信号)
- ✅ FileWrite `[$DATE-$TIME]` 时间戳前缀 (cross-install log 不混)

---

## 4. 下个会话第一任务 (T101)

### 4.1 T101: F4 5 unique schtasks (FR-003)

**位置**: `output/install.nsi:517-518` (pre-install) + `output/install.nsi:557,604,664,712,750` (5 stage blocks) + `output/stage2-shim-install.bat.cmd`

**修改** (per spec 071 FR-003):

1. **Pre-install defensive cleanup** (line 517 之前加 6 行 nsExec):
   ```
   ; v0.74.1: defensive cleanup of all 5 F4 task names + old name
   nsExec::ExecToStack 'schtasks /Delete /TN FluxingStage2ShimSwap /F 2>nul'
   nsExec::ExecToStack 'schtasks /Delete /TN FluxingStage2ShimSwap_weasel /F 2>nul'
   nsExec::ExecToStack 'schtasks /Delete /TN FluxingStage2ShimSwap_weaselx64 /F 2>nul'
   nsExec::ExecToStack 'schtasks /Delete /TN FluxingStage2ShimSwap_server /F 2>nul'
   nsExec::ExecToStack 'schtasks /Delete /TN FluxingStage2ShimSwap_deployer /F 2>nul'
   nsExec::ExecToStack 'schtasks /Delete /TN FluxingStage2ShimSwap_setup /F 2>nul'
   ```

2. **5 stage blocks** each replace:
   - `/TN FluxingStage2ShimSwap` → `/TN FluxingStage2ShimSwap_<shim>`
   - `/TR "..."` 加 `set SHIM=<shim> && ` prefix
   - 注意 NSIS cmd escape: `$"内嵌` 三重引号,需要测试

3. **stage2-shim-install.bat.cmd**:
   - 加 SHIM env var / arg 解析:`set "SHIM=%SHIM%"` or `%~1`
   - per-shim pending.json: `set "PENDING=%STAGE_DIR%\%SHIM%.json"`
   - 写自己一行 entry

### 4.2 T102 + T201-T205 接力顺序

| # | task | file | estimated |
|---|---|---|---|
| T101 | F4 5 unique schtasks | install.nsi + stage2 batch | < 1.5h |
| T102 | TestInstallerShimStage Test 7 | test/TestInstallerShimStage/ | < 30min |
| T201 | Ready-flag MessageBox (IfSilent 跳) | install.nsi:970-974 | < 30min |
| T202 | TestReadyFlagMessageBox new test | test/ | < 1h (手测触发) |
| T203 | ADR 0003 amend (schtasks + glob) | docs/adr/ | < 30min |
| T204 | L##-9.7/9.8 已写,done | — | ✓ already |
| T205 | env.bat / weasel.props v0.74→v0.74.1 + git status -sb + 5 binary md5 | env.bat + weasel.props | < 10min |

### 4.3 真机 sandbox-verify (Task #6)

- `_buildflow.cmd` (L108 fix: rebuild from scratch)
- `_check_system32_cleanup.ps1 -Mode Audit` + Live mode (System32 已有 11 fake files 现成)
- `rime-verify.ps1 -InstallerPath <v0.74.1 installer>`
- 验证 5 binary md5 ≠ v0.74 + Ready-flag 30s 内 + 切输入法 ≤ 1s + rime.weasel log 0 `opencc config not found`

---

## 5. 关键文件 + 行号 (新会话必读)

| 路径 | 行 | 用途 |
|---|---|---|
| `.specify/specs/071-install-freeze-fix/spec.md` | — | 整个 spec |
| `.specify/specs/071-install-freeze-fix/plan.md` | — | tech approach |
| `.specify/specs/071-install-freeze-fix/tasks.md` | — | 13 tasks detail |
| `.specify/memory/lessons-learned.md` L##-9.7 | — | GetSize "S" hidden bug + glob 模式 |
| `.specify/memory/lessons-learned.md` L##-9.8 | — | emoji.json + emoji.ocd mismatch |
| `output/install.nsi:232-247` | .onInit 9.6 cleanup Call | T001 ✓ |
| `output/install.nsi:815-836` | opencc 二次 install + emoji verify | T002 ✓ |
| `output/install.nsi:1017-1030` | silent Abort + DetailPrint | T103 ✓ |
| `output/install.nsi:1104-1107` | Uninstall 9.6 cleanup Call | T001 ✓ |
| `output/install.nsi:1117-1184` | `Function CleanupLegacySystem32Weasel` | T001 ✓ |
| `output/install.nsi:517-518` | pre-install pending.json cleanup | **T101 待改** (defensive 6 task name Delete) |
| `output/install.nsi:557,604,664,712,750` | 5 stage blocks | **T101 待改** (5 unique task name + SHIM arg) |
| `output/install.nsi:970-974` | Ready-flag handshake | **T201 待改** (MessageBox wrap) |
| `output/stage2-shim-install.bat.cmd:1-153` | stage 2 batch | **T101 待改** (SHIM env var + per-shim pending.json) |
| `_check_system32_cleanup.ps1` | audit + Live + Pretouch scripts | T003 ✓ |
| `test/TestOpenccDataPath/` | 6/6 PASS | T004 ✓ |
| `test/TestSilentAbortDetailPrint/` | 7/7 PASS | T104 ✓ |

---

## 6. 必读顺序 (新会话)

1. Read 整个 handoff (this file) — 5 min
2. Read `.specify/specs/071-install-freeze-fix/tasks.md` — verify what's done / pending, 2 min
3. Skim `output/install.nsi:517-518` + 5 stage block locations (557/604/664/712/750) — 5 min
4. Skim `output/stage2-shim-install.bat.cmd:1-50` — understand current stage 2 logic, 2 min
5. Read `.specify/memory/lessons-learned.md` L##-9.7 + 9.8 (防 GetSize-units + emoji.ocd regression), 3 min
6. Total: ~17 min context load → 立刻 start T101

---

## 7. 已通过的 verification

| Test | Mode | Result | Evidence |
|---|---|---|---|
| TestOpenccDataPath | n/a | ✅ PASS 6/6 | install.nsi 3 anchors + emoji.json file (378 bytes) + 16 .ocd* files + comments |
| TestSilentAbortDetailPrint | n/a | ✅ PASS 7/7 | IfSilent 0 +7 + DetailPrint + FileOpen + FileWrite timestamp + FileClose + SetErrorLevel/Quit |
| _check_system32_cleanup.ps1 | Audit | ✅ PASS 8/8 + 3 cross-checks | Function CleanupLegacySystem32Weasel + Call + FindFirst glob + GetSize bytes + 102400 + Trap 5 + L66 + L106 |
| _check_system32_cleanup.ps1 | PretouchUninstall | ✅ 11 fake files in System32 | 9 already + 1 skip + 1 new (weasel.dll 200 KB) |
| reg query verification | 3 commands | ✅ all absent | (用户已卸载,符合预期) |

---

## 8. 仍待

| 优先级 | task | 状态 |
|---|---|---|
| P0 | T101 F4 5 unique schtasks | ⚠️ 大头,NSIS cmd escape 仔细测试 |
| P0 | T102 + T201 + T202 + T203 + T205 | pending |
| P1 | Task #5 真机 sandbox-verify v0.74.1 | 等代码全完 |
| P3 | Task #3 git push (用户明确"安装文件存在 bug 时不适合推送" + 网络恢复) | 等 #5 + 网络 |

---

## 9. 禁止事项

- 不 push (P3 等 Task #5 PASS + 网络恢复)
- 不复制 v0.63-65 浮动栏代码 (那是 Task #4 P1,#10 范围)
- 不修改 librime / plum submodule pointer
- 单回合 diff >500 行 → 拆
- 写 5 schtasks task name 时 NSIS cmd escape (`$"`) 仔细测试
- **不再 GetSize units "S"**(沉淀为 L##-9.7,code review 时必查)

---

## 10. 自我警示 (接 hook feedback 后)

1. **anchor check 不通过 ≠ NSIS 编译失败** — 之前我加的 `→` Unicode 字符让 PowerShell 比较失败,改 ASCII `->` 解决。code review 时优先 ASCII-only。
2. **silent bug 的话听 hook** — GetSize "S" 之前完全没怀疑过,真因已被验证:v0.74 cleanup 表面 PASS 实际 never 删任何文件。
3. **"P0 root cause fix 已 ship" 这种 claim 要有 evidence** — hook 抓了我之前有 inflated claim。改称 "code layer ship; 真机 verify 等 build + 真机 install 后才能完整 PASS"。

---

## 11. Verification "before claiming complete"

按 `feedback_*` 规则,任何 "P0 fixed" "task done" "ship" 说法前必 verify:

- 真机 sandbox-verify v0.74.1 完全 PASS (5 binary md5 ≠ previous + audit 9/9 + Ready-flag 30s + 切输入法 ≤ 1s)
- 不只是 anchor 静态测试 PASS(那是必要不充分条件)

---

*End of handoff. v0.74.1 真机 ship 等 T101-T205 + 真机 verify 后才能完整 PASS。Hook 抓 hidden bug 的能力值得信赖,下个会话继续利用。*
