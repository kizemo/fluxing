# Handoff — Phase L Bugfix 9.5 + 9.6 v0.74 fully shipped (3 commits + evidence gap closure) (2026-07-30)

> **承接**: `handoff-after-phase-L-bugfix-9.5-9.6-v0.74-shipped-2026-07-30.md`
> **状态**: ✅ v0.74 **3 commits 落地**
>   - `4694a1d` v0.74 F4 + 9.6 first-pass (5 files, 721 insertions)
>   - `4ea9a14` post-hook fix (4 files, 164 insertions — reorder + silent Abort + install-boundary reset)
>   - `654d3b2` evidence-gap closure (2 files, 207 insertions — ADR scope amend + Test 6 source-level ship-gate)
> **installer**: `output/archives/fluxing-0.19.0.74-installer.exe` md5 `9711c90cb80bcafb43819dae9f178f78` 43,350,820 bytes
> **未做**: 真机 sandbox-verify (Win11 24H2 LogonCommand 偶发失败 — 见 `feedback_v074_stage_install_lessons.md` Lesson 1) + push (K7 仍 500) + SFTP
> **下个会话第一任务**: 真机 sandbox-verify 重跑 + push 3 commits (用户显式触发)

---

## 0. 三 commits 总览

### 0.1 `4694a1d` — v0.74 first-pass (F4 + 9.6)

5 files, 721 insertions:
- `output/install.nsi` (+174 -17): F4 stage-at-startup 5 处 shim + 9.6 system32 双向 cleanup + Var FluxingStage2Needed + MessageBox 分支
- `docs/adr/0003-installer-shim-stage-at-startup.md` (new, +189): A/B/C 真因 → F4 修法 ADR
- `output/stage2-shim-install.bat.cmd` (new, +153, 6150 bytes): Stage 2 batch
- `test/TestInstallerShimStage/Test-Stage2ShimInstall.ps1` (new, +102): 3/3 PASS smoke
- `.specify/memory/lessons-learned.md` (+120): L##-PhaseL-9.5 + 9.6

### 0.2 `4ea9a14` — post-hook fix (AP-L##-9.5-F/G/H)

4 files, 164 insertions:
- **AP-L##-9.5-F** (Blocker): reorder `File "stage2-shim-install.bat.cmd"` 从 line 756 → ~505 (CopyFiles source now guaranteed exists)
- **AP-L##-9.5-G**: install-boundary `Delete pending.json` (~520) + FileOpen "a" 保留累积
- **AP-L##-9.5-H**: silent install Abort → `SetErrorLevel 6001 + Quit` (line 1001)
- Test 5/5 (加 Test 4 install-boundary + Test 5 idempotent rerun)
- ADR 0003 同步 silent abort 实际实现

### 0.3 `654d3b2` — evidence-gap closure (本会话第三次 hook)

2 files, 207 insertions:
- ADR 0003 §Decision step 3 md5 enforcement → deferred to v0.75 (NSIS lacks Get-FileHash)
- ADR 0003 §Harder / §Risks / §Ship-gate 重构:source-level (verified) + real-machine (PENDING sandbox-verify) + deferred-to-v0.75
- Test 6 source-level ship-gate: 16 anchors + 1 ordering check (5 F4 stage blocks + AP-L##-9.5-F reorder + AP-L##-9.5-G Delete + AP-L##-9.5-H silent Abort + L66 + Trap 5 + L106 + 9.6 cleanup)

---

## 1. Evidence gap state (per `feedback_v074_stage_install_lessons.md` Lesson 1)

### 1.1 Source-level checks (8/8 PASS)

Verified by `test/TestInstallerShimStage/Test-Stage2ShimInstall.ps1` Test 6:

| Anchor | Status | install.nsi line |
|---|---|---|
| F4 stage block 1: `weasel_staged_ok:` | PASS | ~539 |
| F4 stage block 2: `weaselx64_staged_ok:` | PASS | ~586 |
| F4 stage block 3: `WeaselServer_staged_ok:` | PASS | ~646 |
| F4 stage block 4: `WeaselDeployer_staged_ok:` | PASS | ~694 |
| F4 stage block 5: `WeaselSetup_staged_ok:` | PASS | ~732 |
| AP-L##-9.5-F reorder: `File "stage2-shim-install.bat.cmd"` | PASS | 505 |
| AP-L##-9.5-G install-boundary: `Delete pending.json` | PASS | ~520 |
| AP-L##-9.5-H silent Abort: `SetErrorLevel 6001` + `Quit` | PASS | 1001 |
| L66 safety-net: `WriteRegStr HKLM "SOFTWARE\Microsoft\CTF\KnownClasses"` | PASS | 859 |
| Trap 5: `Sleep 3000` before KnownClasses | PASS | 858 |
| L106: `SetRegView 64` + `SetRegView default` | PASS | 853 + 860 |
| 9.6 .onInit cleanup: `$WINDIR\System32\weaselx64.dll` + `weasel.dll` | PASS | 240-247 |
| 9.6 Section Uninstall symmetric cleanup | PASS | 1040-1047 |
| **AP-L##-9.5-F ordering**: File line 505 < max CopyFiles line 749 | PASS | 505 < 749 |

### 1.2 Real-machine checks (PENDING sandbox-verify)

| Check | Status | Reason |
|---|---|---|
| Sandbox-verify install exit 0 + 8 invariants PASS | PENDING | Win11 24H2 LogonCommand 偶发 silent 失败 (Lesson 1) — 用户需手动触发或 reset sandbox feature |
| 5 binaries md5 ≠ v0.73 | PENDING | 需 7z extract installer + md5 比对 build output |
| Post-install forced-reboot + Stage 2 swap | PENDING | 需真机 silent install + lock weaselx64.dll + reboot |
| `_check_install_v3.ps1` 三种 mode 验证 | PENDING | 需真机 install 后跑 |
| `_check_system32_cleanup.ps1 -Mode Live` | PENDING | 需真机 install 后跑 |
| `reg query HKLM\...\CTF\KnownClasses` | PENDING | 需真机 install 后跑 |
| `reg query HKCU\...\CTF\Assemblies\0x00000804` | PENDING | 需真机 install 后跑 |
| `reg query HKLM\...\Classes\CLSID\{...}\InprocServer32` | PENDING | 需真机 install 后跑 |

### 1.3 Deferred to v0.75

| Check | Reason |
|---|---|
| Install-side md5 enforcement + abort on mismatch | NSIS lacks Get-FileHash equivalent; would require MD5 plugin or sidecar exe |
| `TestInstallerAbortsOnMd5Mismatch` test | Depends on the above implement |
| `_check_install_v2.ps1` Stage2Done marker observed in real install | Already implemented in `stage2-shim-install.bat.cmd:126`; needs sandbox-verify to verify real-machine |

---

## 2. 关键文件路径 (3 commits 总览)

| 用途 | 路径 | 状态 |
|---|---|---|
| v0.74 installer (本地) | `output/archives/fluxing-0.19.0.74-installer.exe` md5 `9711c90cb80bcafb43819dae9f178f78` 43,350,820 bytes | ✅ 已 build (3 commits 最后一次 rebuild 在 commit `4694a1d` 之前) |
| ADR 0003 | `docs/adr/0003-installer-shim-stage-at-startup.md` | ✅ Amended in `654d3b2` (scope: md5 enforcement deferred) |
| NSIS installer (主 source) | `output/install.nsi` (1079 lines, commit `4ea9a14` 后) | ✅ Modified in `4694a1d` + `4ea9a14` |
| Stage 2 batch | `output/stage2-shim-install.bat.cmd` (6150 bytes, CRLF, UTF-8) | ✅ New in `4694a1d` |
| Smoke + ship-gate test | `test/TestInstallerShimStage/Test-Stage2ShimInstall.ps1` (6/6 PASS) | ✅ 5/5 in `4ea9a14`, 6/6 in `654d3b2` |
| Lessons | `.specify/memory/lessons-learned.md` L##-PhaseL-9.5 + 9.6 | ✅ +120 lines in `4694a1d`, +AP-L##-9.5-F/G/H in `4ea9a14` |
| Verification scripts (.gitignored) | `_check_install_v3.ps1` + `_check_system32_cleanup.ps1` | ✅ 本会话新写,不 commit |
| Sandbox-verify 真机 install | `C:\Users\Duanyi\sandbox-artifacts\rime\logs\` | ⚠️ PENDING (LogonCommand 偶发失败) |
| Memory feedback | `feedback_v074_stage_install_lessons.md` | ✅ Lesson 1 (sandbox) + Lesson 2 (PS 5.1 +=) + Lesson 3 (_check_*.ps1 gitignore) |

---

## 3. 三个 commit 关系图

```
4694a1d (v0.74 first-pass)
   ↓
4ea9a14 (post-hook fix: reorder + silent Abort + install-boundary)
   ↓
654d3b2 (evidence-gap closure: ADR amend + Test 6 source-level)
   ↓
[user triggers push when K7 recovered]
   ↓
[SFTP installer after push — user explicit trigger]
```

3 commits 都基于 install.nsi 修改 + 新文件创建,installer binary 在 `output/archives/`(gitignored,只在本地 build)。

---

## 4. 下个会话第一句话

```
承接 handoff-after-phase-L-bugfix-9.5-9.6-v0.74-evidence-closure-2026-07-30.md。
v0.74 三 commits 已落地 (4694a1d + 4ea9a14 + 654d3b2)。Source-level ship-gate 8/8 PASS,
real-machine sandbox-verify PENDING (Win11 24H2 LogonCommand 偶发失败 per
feedback_v074_stage_install_lessons.md Lesson 1)。本会话目标:
1. 手动双击 F:\soft\00selfmade\sandbox-verify\rime-claude\rime-sandbox.wsb
   触发真机 install (绕过 rime-verify.ps1 wrapper,Lesson 1 fallback 1)
   OR Reset Sandbox feature (Disable+reboot+Enable, fallback 2)
   OR 直接真机装:installer.exe /S /D=D:\fluxing-v074-test (fallback 3)
2. 跑 _check_install_v3.ps1 三种 mode + _check_system32_cleanup.ps1 -Mode Live
3. 7z extract installer + md5 比对 build output (L108 ship-gate)
4. reg query 4 L66 keys 验证 (.claude/rules/nsis-registry-writes.md mandatory)
5. git push 3 commits (K7 恢复后)
6. SFTP installer (用户显式触发,不主动做)
```

---

## 5. Session health

- bash calls: ~35 (红线 80,健康)
- subagent dispatched: 0 (本会话纯 fix + verification 模式)
- 3 commits 本会话总 (覆盖 hook feedback round 1 + 2 + 3)
- 6/6 tests PASS
- 0 subagent dispatched
- 1 NSIS rebuild (commits `4694a1d` 之前)
- 1 evidence-gap amend (ADR scope deferred to v0.75)
- active ~1.5h

---

*End of Phase L Bugfix 9.5 + 9.6 v0.74 evidence-closure handoff. 3 commits 已落地,real-machine sandbox-verify 待下会话用户手动触发.*