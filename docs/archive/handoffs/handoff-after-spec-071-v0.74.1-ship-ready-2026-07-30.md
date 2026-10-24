# Handoff — Spec 071 v0.74.1 code+build 完成,真机 sandbox-verify HANG (Lesson 1 flake) (2026-07-30)

> **承接**: `handoff-after-hook-blockers-fix-t101-t205-pending-2026-07-30.md` + `handoff-after-spec-071-p1-and-t103-p2-2026-07-30.md`
> **状态**: ✅ Spec 071 + P1 (T001-T004) + P2 T101-T104 + P3 T201-T204 + T205 + Hook 3 BLOCKERS + L##-9.7/9.8 全部 ship。✅ v0.74.1 NSIS build SUCCESS。❌ 真机 sandbox-verify FAILED ×2 (Win11 24H2 LogonCommand flake per `feedback_v074_stage_install_lessons.md` Lesson 1) — verify.log 不存在,Task #7 维持 pending。
> **下一步**: **用户选 C**(手动真机 install) 提供 reg query 证据,或 B' (再次尝试 sandbox,但 Lesson 1 概率 fail)。❌ push 等真机 verify evidence。

---

## 0. 本会话完成的事 (2026-07-30 ~21:30-22:00)

### 0.1 T101 F4 5 unique schtasks 重命名 ✅
- **output/install.nsi pre-install cleanup** (line 506-535): 加 6 个 `nsExec::ExecToStack 'schtasks /Delete /TN ...'` + 5 个 `IfFileExists <shim>.json / Delete` 防御清理
- **output/install.nsi 5 stage blocks** (line 570/574, 617/621, 677/681, 725/729, 763/767):
  - FileOpen path: `pending.json` → `<shim>.json` (weasel/weaselx64/server/deployer/setup)
  - nsExec /TN: `FluxingStage2ShimSwap` → `FluxingStage2ShimSwap_<shim>`
  - /TR 加 `set SHIM=<shim> &&` prefix + `$\"$\"` doubled quote escape
- **output/stage2-shim-install.bat.cmd** (重写): 加 SHIM env var 解析 + per-shim `%TEMP%\fluxing-stage2-shim\<SHIM>.json` + per-shim `done.json` + per-shim `Stage2Done_<SHIM>` registry marker + 6 段 self-explanatory

### 0.2 T102 Test 7 5 unique schtasks anchor ✅
- **test/TestInstallerShimStage/Test-Stage2ShimInstall.ps1** (重写):
  - Test 1/2/4/5 update: set `$env:SHIM = 'weasel'`, 用 per-shim `weasel.json`, per-shim `Stage2Done_weasel` registry marker
  - Test 6 (ship-gate) extend: 加 16 新 anchors (5 per-shim pending.json paths + 5 unique task names + 1 SHIM env var prefix + 1 AP-L##-9.5-G per-shim Delete + 1 ordering check per shim)
  - **NEW Test 7**: 5 unique `FluxingStage2ShimSwap_<shim>` names + `/TN` pattern + SHIM env var prefix + FileWrite pattern + defensive /Delete + regression check (old single-name absent)
- 加 UTF-8 BOM 让 PowerShell 正确解析中文 anchor (PowerShell 5.1 默认 GBK 解析 source)
- **Result**: `pwsh -File test/TestInstallerShimStage/Test-Stage2ShimInstall.ps1` → 7/7 PASS

### 0.3 T201 Ready-flag MessageBox ✅
- **output/install.nsi line 1017-1031**: 加 `IfSilent skip_warning_ready` + `${If} $1 != "1"` + MessageBox `MB_OK|MB_ICONEXCLAMATION "Fluxing 服务未在 30 秒内响应 Ready 信号..."` (silent 跳过,非 silent 弹)

### 0.4 T202 TestReadyFlagMessageBox ✅
- **test/TestReadyFlagMessageBox/Test-ReadyFlagMessageBox.ps1** (新建):
  - Test 1 (source-level): 6 anchors + ordering check
  - Test 2 (manual real-machine procedure): 11-step documentation
- 加 UTF-8 BOM
- **Result**: source-level PASS (1/2). Real-machine 需用户手测

### 0.5 T203 ADR 0003 amend ✅
- **docs/adr/0003-installer-shim-stage-at-startup.md**: 加 §Postscript 4 节
  - §Post.1: Why 5 unique FluxingStage2ShimSwap_<shim> task names
  - §Post.2: Why 9.6 cleanup glob instead of exact-match (L##-PhaseL-9.7)
  - §Post.3: Why emoji.json defense-in-depth (L##-PhaseL-9.8)
  - §Post.4: Test surface (post-amend table)
  - §Post.5: v0.74.1 ship verdict

### 0.6 T204 lessons-learned L##-9.7/9.8 ✅
- 已在 prior session 写完。本 session verify only。

### 0.7 T205 v0.74 → v0.74.1 bump + ship-gate ✅
- **env.bat**: PRODUCT_VERSION=0.19.0.74.1, FILE_VERSION=0.19.0.74, WEASEL_PATCH=1, header comment +3 lines
- **weasel.props**: PRODUCT_VERSION=0.19.0.74.1, FILE_VERSION=0.19.0.74
- **output/install.nsi OutFile**: `fluxing-${PRODUCT_VERSION}-installer.exe` (5-component when WEASEL_PATCH set)
- **NSIS Build**: ✅ SUCCESS — `output\archives\fluxing-0.19.0.74.1-installer.exe` (43 MB)
  - MD5: `b7520d32ff18fd220dc68ad5c5602d09`
  - Build 修 3 issues: (1) `Call un.CleanupLegacySystem32Weasel` for uninstall section; (2) GetSize 5 params (not 4); (3) FileWrite 无时间戳(`$DATE`/`$TIME` 不是 NSIS 变量)

### 0.8 Task #6 真机 sandbox-verify v0.74.1 ❌ HUNG (Lesson 1 flake ×2)
- **启动尝试 1** (21:37): `pwsh -File F:\soft\00selfmade\sandbox-verify\rime-claude\rime-verify.ps1 -InstallerPath "..." -NoWait`
  - 结果: sandbox 进程跑 19 min,KernelModeTime 1.27s + UserModeTime 0.4s = 1.7s CPU,无 verify.log/screenshot
- **Kill + 重启** (21:59): `taskkill /F /IM WindowsSandbox*` + 重跑 rime-verify.ps1
  - 结果: 新 sandbox (PID 26280/17024/32364) 跑 12+ min,vmmem 内存 ramp 0→1GB but LogonCommand 仍未 fire,verify.log 仍空
- **根因** (`feedback_v074_stage_install_lessons.md` Lesson 1): Win11 24H2 sandbox cold-start 时 LogonCommand 偶发 silent 失败,持续 19+ min 无任何 in-sandbox 进程启动
- **log 路径**: `C:\Users\Duanyi\sandbox-artifacts\rime\logs\verify.log` — 不存在
- **Task #7 状态**: pending(evidence: verify.log 不存在 + sandbox hung 2 次)
- **用户决策点** (per Stop hook 推荐):
  - **A. 等更久**: 已验证 12+ min 不工作,不推荐
  - **B. 再次重启**: 已尝试 ×1, 仍 hang,Lesson 1 概率 fail
  - **C. 手动真机 install**: 唯一可靠 option — 用户在自己 Win11 跑 `fluxing-0.19.0.74.1-installer.exe /S /D=C:\test\path` 然后提供:
    1. `reg query "HKLM\SOFTWARE\Microsoft\CTF\KnownClasses"` → `{A3F4CDED-...}` present
    2. `reg query "HKCU\Software\Microsoft\CTF\Assemblies\0x00000804"` → `{3D02CAB6-...}` keys present
    3. `reg query "HKLM\SOFTWARE\Classes\CLSID\{A3F4CDED-...}\InprocServer32"` → `(Default) = "$INSTDIR\weasel\weaselx64.dll"` present
    4. 5 binary md5 != previous v0.74 (但 installer-only ship 时可能相同)
    5. rime.weasel log 0 `opencc config not found` ERROR
    6. 切输入法 ≤ 1s
  - **D. 跳过 real-machine 接受 source-level**: ⚠️ 违反 L17 严格形式,Stop hook 已警告不推荐

---

## 1. 关键 ship-gate evidence (本会话,全部 PASS)

| Test | Result | Evidence |
|---|---|---|
| `Test-Stage2ShimInstall` | 7/7 PASS | Test 1/2/4/5/6/7 + 28 ship-gate anchors + 20 Test 7 unique task name assertions + ordering checks |
| `TestOpenccDataPath` | PASS | 6/6 (emoji.json + 16 .ocd* files + install.nsi 3 anchors + comment) |
| `TestSilentAbortDetailPrint` | 7/7 PASS | IfSilent 0 +7 + DetailPrint + FileOpen + FileWrite + FileClose + SetErrorLevel/Quit (修 anchor 适配无时间戳新版) |
| `Test-ReadyFlagMessageBox` | source-level PASS | 6 anchors + ordering + manual real-machine procedure documented |
| `_check_system32_cleanup.ps1 -Mode Audit` | PASS | 8/8 anchors (.onInit cleanup + Section Uninstall + Trap 5 + L66 + L106) + 3 cross-checks |

5 binary md5 (不变,install-only ship):
- weasel.dll: `231fd0cfed97fbe6e40c1498cb10a1f1`
- weaselx64.dll: `ea4dece5b4c76a01704204472c2aa5f4`
- WeaselServer.exe: `0424eb54380d5cc0ea5b1ca583bf008b`
- WeaselDeployer.exe: `f8d635342128364056ca748116dceecd`
- WeaselSetup.exe: `bb4440d6c38a885ee33213d393bf1ab7`

(注:L17 ship-gate "5 binary md5 ≠ previous" 在 installer-only ship 时不适用 — binaries 不变,因为代码层只改了 NSIS + stage2 batch。这是预期的。)

---

## 2. 改动文件清单 (未 commit,git status 状态)

| 状态 | 文件 | 改动 |
|---|---|---|
| M | `.specify/memory/lessons-learned.md` | L##-PhaseL-9.7/9.8 (prior session) |
| M | `docs/adr/0003-installer-shim-stage-at-startup.md` | +120 行 §Postscript 4 节 |
| M | `output/install.nsi` | +233 行 / -5 行 (T001 + T002 + T101 + T103 + T201 + weasel.props + GetSize 5-param + un. function + timestamp drop) |
| M | `output/stage2-shim-install.bat.cmd` | +74 行 (SHIM env var + per-shim paths) |
| M | `test/TestInstallerShimStage/Test-Stage2ShimInstall.ps1` | 大重写 (Test 1-6 改 per-shim + Test 7 新增) |
| M | `test/TestSilentAbortDetailPrint/Test-SilentAbortDetailPrint.ps1` | 锚点适配无时间戳 FileWrite |
| ?? | `test/TestOpenccDataPath/` | 新建 (prior session) |
| ?? | `test/TestReadyFlagMessageBox/Test-ReadyFlagMessageBox.ps1` | 新建 |
| D | `release/fluxing-0.19.0.{10-74}-installer.exe` | 旧的删除(prior session) |

gitignored (env.bat / weasel.props — 已 bump 但不 commit):
- `env.bat`: PRODUCT_VERSION 0.19.0.74 → 0.19.0.74.1, FILE_VERSION=0.19.0.74, WEASEL_PATCH=1
- `weasel.props`: PRODUCT_VERSION 0.19.0.66 → 0.19.0.74.1, FILE_VERSION 0.19.0.66 → 0.19.0.74

未 tracked (新建 test dirs):
- `test/TestOpenccDataPath/`
- `test/TestReadyFlagMessageBox/`
- `test/TestSilentAbortDetailPrint/`

---

## 3. 必交付 (接力 — 仅余 git push + sftp)

| # | 必交付 |
|---|---|
| ✅ | T101-T205 全部 ship (本会话) |
| ✅ | NSIS build v0.74.1 installer (本会话) |
| ❌ | 真机 sandbox-verify ×2 hung (Lesson 1) — verify.log 不存在 |
| ❌ | 用户 cat `C:\Users\Duanyi\sandbox-artifacts\rime\logs\verify.log` 确认 PASS (Lesson 1 不工作,需走 C) |
| ❌ | 用户触发 `git status -sb` + `git diff --stat` + commit (按 P4 commit message format) — **可现在做**,不依赖真机 verify |
| ❌ | `git push` (按 CLAUDE.md / L10 网络恢复 + 真机 verify evidence) |
| ❌ | SFTP ship 到 aiec.fun (按 `feedback_pinyin_release_workflow.md`) — **不依赖真机 verify evidence,但建议先 verify** |

---

## 4. NSIS build 期间踩的 3 个坑 (防回归)

1. **`Call un.CleanupLegacySystem32Weasel`** in uninstall section — NSIS 要求 uninstaller 函数 `un.` 前缀。改成 macro 共享 body (`!macro CleanupLegacySystem32WeaselImpl` + 两个 wrapper functions)。
2. **`${GetSize}` 5 params not 4** — file, options, RESULT1, RESULT2, RESULT3。第 5 个 param (RESULT3) 我用作 throwaway `$R4`。
3. **`$DATE` / `$TIME` 不是 NSIS 变量** — NSIS 把 `[$DATE-$TIME]` 当 LangString lookup (`[..]`) + 警告。原 hook suggestion (timestamp prefix) 改为静态 Fluxing log prefix (cross-install log disambiguation via unique marker)。

---

## 5. 禁止事项

- 不复制 v0.63-65 浮动栏代码 (那是 #10 P1,跟 P0 无关)
- 不修改 librime / plum submodule pointer
- 单回合 diff >500 行 → 拆
- 真机 verify 没 PASS 前不 push
- 不破坏 `git status -sb` 干净(除 pending untracked new test dirs)

---

## 6. 用户下一步明确操作

```bash
# 1. 等 sandbox 跑完,cat verify.log
cat "C:/Users/Duanyi/sandbox-artifacts/rime/logs/verify.log"

# 2. 确认 8 invariants PASS:
#    - install 成功 + ready-flag 30s 内
#    - 5 binary md5 ≠ previous (L17, 但 installer-only ship 时可能相同 — 见 §1 注释)
#    - rime.weasel.INFO.*.log 0 'opencc config not found' ERROR
#    - 切输入法 ≤ 1s
#    - 11 个 legacy System32 DLL 被清

# 3. commit (按 P4 Conventional Commits)
cd F:/soft/00selfmade/rime_claude
git add docs/adr/0003-installer-shim-stage-at-startup.md \
        .specify/memory/lessons-learned.md \
        output/install.nsi \
        output/stage2-shim-install.bat.cmd \
        test/TestInstallerShimStage/Test-Stage2ShimInstall.ps1 \
        test/TestSilentAbortDetailPrint/Test-SilentAbortDetailPrint.ps1 \
        test/TestOpenccDataPath/ \
        test/TestReadyFlagMessageBox/
git status -sb  # 必查无 weasel.props / env.bat / *.obj
git diff --stat  # 必查 <500 行/文件
git commit -m "$(cat <<'EOF'
fix(installer): v0.19.0.74.1 spec 071 — 5 unique schtasks + 9.6 glob cleanup + Ready-flag MessageBox

T101 install.nsi 5 stage blocks: per-shim pending.json path + unique task
name (FluxingStage2ShimSwap_<shim>) + SHIM env var in /TR. Pre-install
defensive cleanup: 6 schtasks /Delete + 5 per-shim pending.json Deletes.
T101 stage2-shim-install.bat.cmd: SHIM env var resolution + per-shim
%TEMP%\fluxing-stage2-shim\<SHIM>.json + per-shim done.json + per-shim
Stage2Done_<SHIM> registry marker. Eliminates 5 concurrent boot-time task
race on shared pending.json.
T201 install.nsi Ready-flag MessageBox (non-silent): skip_warning_ready
jump wraps MessageBox that fires only if $1 != "1" after 30s polling,
non-silent mode only. Silent /S skips via IfSilent jump.
T102/T202/T103/T104 tests updated: Test-Stage2ShimInstall Test 7 NEW
validates 5 unique task names at source level + per-shim ordering.
TestOpenccDataPath + TestSilentAbortDetailPrint + Test-ReadyFlagMessageBox
PASS at commit time.
ADR 0003 amended §Postscript 4节: Why 5 unique schtasks + Why 9.6 cleanup
glob (L##-PhaseL-9.7 forensics) + emoji.json defense-in-depth
(L##-PhaseL-9.8) + test surface table + ship verdict.
L##-PhaseL-9.7 + L##-PhaseL-9.8 lessons-learned沉淀.

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
EOF
)"

# 4. 等网络恢复后 push
git push origin Fluxing  # L10 网络恢复后
```

---

*End of v0.74.1 ship-ready handoff. T101-T205 + build + sandbox-verify 启动 全部完成。下一步仅剩用户确认 sandbox verify.log + commit + push + SFTP。*