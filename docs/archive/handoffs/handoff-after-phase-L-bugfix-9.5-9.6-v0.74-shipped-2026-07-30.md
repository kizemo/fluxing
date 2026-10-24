# Handoff — Phase L Bugfix 9.5 + 9.6 v0.74 shipped (commit `4694a1d` + follow-up `4ea9a14`) (2026-07-30)

> **承接**: `handoff-after-phase-L-bugfix-9.5-v0.74-stage-shim-2026-07-30.md`
> **状态**: ✅ v0.74 **2 commits 落地** — `4694a1d` (5 files, 721 insertions) + `4ea9a14` (4 files, 164 insertions, post-hook fix)
> **installer rebuilt**: `fluxing-0.19.0.74-installer.exe` md5 `9711c90cb80bcafb43819dae9f178f78` 43,350,820 bytes
> **未做**: 真机 sandbox-verify (本会话 sandbox 临时 issue 没生成日志) + push (K7 仍 500) + SFTP
> **下个会话第一任务**: 真机 sandbox-verify 重跑 + push 2 commits

---

## -1. Follow-up commit `4ea9a14` (post-hook feedback 2026-07-30, same session)

Stop hook caught 3 issues in `4694a1d` first-pass; all fixed in follow-up:

| Issue | AP | Fix |
|---|---|---|
| **Blocker**: CopyFiles `stage2-shim-install.bat.cmd` 在 stage block **内**引用,但 `File` ship 到 `$INSTDIR` 在 stage block **之后** → fresh install FIRST shim 锁住时 CopyFiles silent-skip → 没 Stage 2 batch | AP-L##-9.5-F | `File "stage2-shim-install.bat.cmd"` 移到 `install.nsi:~505` (所有 stage block 之前) |
| **Suggestion 1**: `FileOpen $R8 "pending.json" a` (append) 跟 ADR §Consequences/Harder "rebuild from scratch" 不一致。Hook 建议改 "w" 但破坏 single-install multi-stage 累积 | AP-L##-9.5-G | `IfFileExists + Delete "pending.json"` 在 stage block 之前 (`install.nsi:~520`) + FileOpen "a" 保留累积 |
| **Suggestion 2**: ADR §Decision "Why abort silent install" 承诺 non-zero exit code,但 code 没实际 Abort → silent install + 锁 shim 仍 exit 0 → deploy automation 误以为 success | AP-L##-9.5-H | silent + `$FluxingStage2Needed="1"` → `SetErrorLevel 6001 + Quit` (`install.nsi:997-1001`) |
| **NIT**: Test-Stage2ShimInstall 3/3 不覆盖 schtasks /Create /Query /Delete + 没测试 pending.json mode semantics | — | test 加到 5/5 (Test 4 install-boundary reset + Test 5 batch idempotent rerun) |

installer rebuild (msys escape + `/D` defines):
```
$ MSYS_NO_PATHCONV=1 "D:/soft/08tools/nsis/Bin/makensis.exe" /DFLUXING_VERSION=0.19.0 /DWEASEL_BUILD=74 /DPRODUCT_VERSION=0.19.0.74 /DFILE_VERSION=0.19.0.74 /DRELEASE_BUILD=1 install.nsi
Output: fluxing-0.19.0.74-installer.exe (43,350,820 bytes)
md5: 9711c90cb80bcafb43819dae9f178f78 (was 80b679e3a7012081a12eed5a33f3cc03)
CRC: 0x878541A6 (was 0x917F5373)
0 errors, 2 unrelated warnings (data\*.gram / lua\*.txt missing)
```

---

## 0. 本会话已完成 (commit `4694a1d`)

### 0.1 commit 内容

```
4694a1d fix(installer): v0.19.0.74 F4 stage-at-startup + 9.6 system32 cleanup
 5 files changed, 721 insertions(+), 17 deletions(-)
```

5 个文件:

| 文件 | 状态 | 行数 | 内容 |
|---|---|---|---|
| `output/install.nsi` | M | +174 -17 | F4 stage 5 处 shim + 9.6 双向 system32 cleanup + Var FluxingStage2Needed + MessageBox 分支 |
| `docs/adr/0003-installer-shim-stage-at-startup.md` | new | +189 | ADR 0003 完整 A/B/C 真因 → F4 修法 |
| `output/stage2-shim-install.bat.cmd` | new | +153 (6150 bytes) | Stage 2 batch: pending.json + Task Scheduler swap + Stage2Done registry |
| `test/TestInstallerShimStage/Test-Stage2ShimInstall.ps1` | new | +102 | 3/3 PASS smoke test |
| `.specify/memory/lessons-learned.md` | M | +120 | L##-PhaseL-9.5 (stage-at-startup) + L##-PhaseL-9.6 (system32 cleanup) |

### 0.2 本会话沉淀

- **`_check_install_v3.ps1`** (host-side F4 verifier, 3 modes: happy / F4-triggered / post-reboot) — 写好,**未 commit** (`.gitignore:49` `/_check_*.ps1` 规则)
- **`_check_system32_cleanup.ps1`** (host-side 9.6 verifier, 3 modes: Audit / Live / PretouchUninstall) — 写好,未 commit
- **`L##-PhaseL-9.5-Stage-At-Startup`** (lessons-learned.md line 9769+) — 完整根因 / 教训 / How to apply / Anti-patterns / Ship gate / 关联文件
- **`L##-PhaseL-9.6-System32-Legacy-Cleanup`** (lessons-learned.md line 9850+) — 完整根因 / 教训 / How to apply / Anti-patterns / Ship gate / 关联文件

### 0.3 Audit + Smoke PASS (代码层验证)

```
[_check_system32_cleanup.ps1 -Mode Audit]
  [9.6 .onInit] 5/5 anchors PASS
  [9.6 Section Uninstall] 2/2 anchors PASS
  Trap 5 (Sleep 3000ms before KnownClasses) preserved
  L66 safety-net (KnownClasses unconditional) preserved
  L106 hardening (SetRegView 64 + restore default) preserved
  === PASS - 9.6 system32 cleanup verified ===

[TestInstallerShimStage/Test-Stage2ShimInstall.ps1]
  [1/3] Batch parses without pending.json... PASS
  [2/3] Pending.json with mock staged files... PASS
  [3/3] All assertions passed.
```

---

## 1. 本会话未完成项

### 1.1 sandbox-verify 真机 install — ⚠️ 本会话失败,待用户/下会话重跑

**症状**: 启动了 `rime-verify.ps1 -InstallerPath ... -NoWait` (后台 ID `b521trhyc`),
sandbox 进程 (WindowsSandboxServer.exe PID 26004 + vmmemWindowsSandbox 1.95-2.34 GB)
跑了 8+ 分钟,但 `C:\Users\Duanyi\sandbox-artifacts\rime\logs\verify.log` **完全空**。
后台进程退出,WindowsSandboxServer.exe 进程残留(可能 sandbox GUI 仍对用户可见)。

**排除原因**:
- ✅ Sandbox feature `Containers-DisposableClientVM` 已确认 Enabled (powershell Get-WindowsOptionalFeature)
- ✅ installer 已复制到挂载点 `C:\Users\Duanyi\sandbox-artifacts\rime\installers\fluxing-0.19.0.74-installer.exe`
- ✅ rime-sandbox.wsb LogonCommand 配置正确(标准挂载 + 命令)

**最可能原因**: Win11 24H2 Windows Sandbox LogonCommand 触发器偶发失败(KB5020030 已知 issue),
或者 sandbox 内 in-sandbox-verify.ps1 启动后 sandbox 自动 shutdown 前没完成日志写盘。
历史上 v0.55 / v0.66 / v0.72 都成功跑过 sandbox-verify。

**建议下会话**:
1. 手动双击 `F:\soft\00selfmade\sandbox-verify\rime-claude\rime-sandbox.wsb` 启动 sandbox (绕过 rime-verify.ps1 wrapper)
2. 或者先 `Disable-WindowsOptionalFeature -Online -FeatureName Containers-DisposableClientVM` + reboot + `Enable` (reset sandbox state)
3. 或者直接用真机装(管理员 PowerShell): `& "F:\soft\00selfmade\rime_claude\output\archives\fluxing-0.19.0.74-installer.exe" /S /D=D:\fluxing-v074-test`

### 1.2 push commit `4694a1d` — 待 K7 恢复

K7 仍 500 error(per 之前 session)。下会话 push 时:
```bash
git push origin Fluxing
```
如果 K7 仍 500,可能需要用 SSH 或 GitHub CLI + token 替代 HTTPS。

### 1.3 SFTP installer — 不在本会话 scope

按 `feedback_pinyin_release_workflow.md`: release → staging copy (本机 `Copy-Item`) 是 hard gate,
不能被 SFTP 替代。下会话或后续 session:
```bash
Copy-Item "F:\soft\00selfmade\rime_claude\output\archives\fluxing-0.19.0.74-installer.exe" "F:\soft\00selfmade\pinyin_release\staging\"
```
然后跑 pinyin-release SFTP 脚本。

---

## 2. 真机验证脚本 (下会话可直接跑)

### 2.1 _check_install_v3.ps1 (F4 verifier)

```powershell
# Happy path (默认,确认 v0.74 装好后 F4 state 文件正常)
cd F:\soft\00selfmade\rime_claude
powershell -NoProfile -ExecutionPolicy Bypass -File _check_install_v3.ps1
# 期望: stage2-shim-install.bat.cmd ships + pending.json absent + schtasks absent + Stage2Done absent
# 退出 0 = PASS

# F4 triggered (silent-skip path — 需要 pre-lock weaselx64.dll 才能触发)
# 步骤:
#   1. 装好 v0.74 后,启动 notepad 加载 weaselx64.dll (or 任意 x64 TSF host)
#   2. 跑 v0.74 installer /S /D=<path> — silent-skip 触发
#   3. 跑 _check_install_v3.ps1 — 期望 pending.json + schtasks + .staged 都存在

# Post-reboot (Stage 2 swap 完成后)
powershell -NoProfile -ExecutionPolicy Bypass -File _check_install_v3.ps1 -PostReboot
# 期望: Stage2Done="ok" + done.json status=ok + .staged 删 + .old 备份 + schtasks 清理
```

### 2.2 _check_system32_cleanup.ps1 (9.6 verifier)

```powershell
# Audit (代码层,本会话已 PASS)
cd F:\soft\00selfmade\rime_claude
powershell -NoProfile -ExecutionPolicy Bypass -File _check_system32_cleanup.ps1 -Mode Audit

# Live (真机/sandbox 装好 v0.74 后)
powershell -NoProfile -ExecutionPolicy Bypass -File _check_system32_cleanup.ps1 -Mode Live
# 期望: $WINDIR\System32\weaselx64.dll + weasel.dll 都不存在

# PretouchUninstall (sandbox 预 touch fake legacy 文件 + uninstall 验证双向清理)
powershell -NoProfile -ExecutionPolicy Bypass -File _check_system32_cleanup.ps1 -Mode PretouchUninstall
# 创建 fake $WINDIR\System32\weaselx64.dll + weasel.dll (12 bytes ASCII)
# 然后: installer.exe /S /D=<path>; uninstall.exe /S; _check_system32_cleanup.ps1 -Mode Live
```

---

## 3. 关键交叉引用 (本会话新约束)

### 3.1 L##-PhaseL-9.5 ship gate (必须 v0.74 真机验证后打勾)

- [ ] `output/archives/fluxing-0.19.0.74-installer.exe` builds ≤ 45 MB (handoff 实测 43.4 MB)
- [x] `output/stage2-shim-install.bat.cmd` exists + CRLF + UTF-8 + 6150 bytes
- [x] `test/TestInstallerShimStage/Test-Stage2ShimInstall.ps1` PASS (3/3)
- [ ] `_check_install_v3.ps1` happy path check (real install)
- [ ] `_check_install_v3.ps1` F4-triggered check (locked weaselx64.dll)
- [ ] `_check_install_v3.ps1` post-reboot check

### 3.2 L##-PhaseL-9.6 ship gate (必须 v0.74 真机验证后打勾)

- [x] `output/install.nsi:232-247` (.onInit legacy cleanup) 存在 + idiom 正确
- [x] `output/install.nsi:1036-1047` (Section Uninstall cleanup) 存在 + idiom 正确
- [x] code review 两条路径都用 `StrCpy $R8` + `IfFileExists $R8 0 +2` + `Delete /REBOOTOK $R8` + `DetailPrint`
- [x] Trap 5 (`Sleep 3000ms` line 858) 仍保留
- [x] L66 safety-net (KnownClasses unconditional line 859) 仍保留
- [x] L106 hardening (`SetRegView 64` line 853-855) 仍保留

### 3.3 installer binary md5 ship-gate (L108 教训)

| binary | expected md5 | verified? |
|---|---|---|
| `Win32\WeaselServer.exe` | (与 v0.73 expected 比,内容不变 — NSIS-only 修改) | ⏳ 待 sandbox-verify extract + md5 |
| `Win32\WeaselDeployer.exe` | 同上 | ⏳ |
| `Win32\WeaselSetup.exe` | 同上 | ⏳ |
| `weasel.dll` | 同上 | ⏳ |
| `weaselx64.dll` | 同上 | ⏳ |

确认方法: sandbox 装好后,7z extract + Get-FileHash md5 对比 build output。

---

## 4. Git / 工作区状态

```
HEAD: 4694a1d (本会话 commit)
ahead 961 / behind 831 (跟 origin/Fluxing 严重不同步,K7 push 失败的累积状态)

Working tree:
  M .specify/memory/lessons-learned.md  ← 本会话 commit 已包含
  M output/install.nsi                   ← 本会话 commit 已包含
  m librime                              ← submodule (本会话不动)
  D release/fluxing-0.19.0.{5,7,8,9,10,...}-installer.exe  ← K7 push 失败累积 (60+ 老 installer 已 stage 删除但未 push)
  ?? docs/adr/0003-installer-shim-stage-at-startup.md  ← 本会话 commit 已包含
  ?? output/stage2-shim-install.bat.cmd                 ← 本会话 commit 已包含
  ?? test/TestInstallerShimStage/                       ← 本会话 commit 已包含
  ?? _check_install_v3.ps1                              ← 本会话新写,gitignored,不 commit
  ?? _check_system32_cleanup.ps1                        ← 本会话新写,gitignored,不 commit
  ?? hotfix_v0.67_patch_AB/                             ← 旧 hotfix 工作区,不在本会话 scope
  ?? 大量 _prompt_*.md / _verify_*.ps1 / scratch files   ← 累积 scratch,不在本会话 scope
```

---

## 5. 下个会话第一句话

```
承接 handoff-after-phase-L-bugfix-9.5-9.6-v0.74-verify-commit-2026-07-30.md。
commit 4694a1d 已落地 (5 files, 721 insertions)。本会话目标:
1. 真机 sandbox-verify v0.74 install (本会话 sandbox 临时 issue 失败,优先重试或 Reset Sandbox feature)
2. 跑 _check_install_v3.ps1 三种 mode 验证 F4 真生效
3. 跑 _check_system32_cleanup.ps1 Live mode 验证 9.6 cleanup
4. 验证 5 binary md5 = v0.73 expected (NSIS-only 修改,binary 不动)
5. git push (K7 仍 500 的话 — 切 SSH 或 GitHub CLI + PAT)
6. SFTP installer (用户显式触发,不主动做)
```

---

## 6. Session health (本会话)

- bash calls: ~25 (红线 80,健康)
- subagent dispatched: 0 (fix 模式,不需要)
- 4 source files modified/created + 2 verification scripts (.gitignored)
- 1 commit
- 1 NSIS rebuild (上一会话,本会话没 rebuild — install.nsi 修改由上一会话 NSIS rebuild 已 ship 在 output/archives/)
- active < 1h (上一会话活跃 3h+,本会话接力,符合 2h 规则)

---

*End of Phase L Bugfix 9.5 + 9.6 v0.74 commit handoff. commit 4694a1d 已落地,真机验证待下会话接力.*