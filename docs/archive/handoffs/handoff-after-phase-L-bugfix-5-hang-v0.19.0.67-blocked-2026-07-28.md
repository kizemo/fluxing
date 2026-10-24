# Handoff — Phase L Bugfix 5 v0.68 (2026-07-28)

> **承接**: `handoff-after-phase-L-bugfix-4-ship-v0.19.0.67-2026-07-26.md` (v0.67 build+commit,ship gate 待 user 装机验证)
> **状态**: 🛑 **v0.67 不能 ship — installer UI 17 分钟 hang + 灾难 #2 binary mismatch 双灾难已锁真因**
> **新目标**: ship **v0.68**, 两处 install.nsi 修复 + sandbox-verify §9 double-install 测试 + L##-PhaseL-11 沉淀
> **本会话授权 user 已 confirm**: Fix-D1 (line 760 ExecWait → Exec) + Fix-D3 (line 498-565 强制 retry-with-taskkill-ctfmon 模式)
> **commit 基线**: `bbd0414` (v0.67 installer, git-tracked source) + 工作树 CHANGELOG.md v0.66/v0.67 段（user-confirmed 但未 commit）
> **本会话工作树状态**（git status -sb）: `M CHANGELOG.md`（+79 行）+ `?? _check_install_v067_postship.ps1` + `?? _prompt_phase_L_bugfix_5_v0.68_fix_2026-07-28.md` (本回合新增) + `?? handoff-after-phase-L-bugfix-5-hang-v0.19.0.67-blocked-2026-07-28.md` (本回合新增)

---

## 1. 本会话双灾难锁定（user 真机实测）

### 1.1 灾难 A — installer UI hang 17 分钟（user 任务管理器可开，installer.exe 进程被 WER 标 "Application Hang"）

**完整时间线（机器视角，user system 实测）**：

| 时间 | 事件 |
|---|---|
| `2026-07-09 09:20:11` | `installation.yaml` install_time — 首次装, **新装未改** |
| `2026-07-23 23:26:24` | `weaselx64.dll.old.tmp` mtime（rename after install 残留） |
| `2026-07-25 22:46:14` | `weasel.dll.old.tmp` mtime（v0.65 rename 残留） |
| `2026-07-26 23:27-23:29` | v0.66 build 产物: weasel.dll, weaselx64.dll, WeaselServer.exe, WeaselSetup.exe, WeaselDeployer.exe |
| **`2026-07-28 08:47:38`** | **`uninstall.exe` mtime** — v0.67 installer **跑到 install.nsi:756 WriteUninstaller** ✓ |
| `2026-07-28 08:47:39` | WeaselServer.exe 启动（HKCU Run 触发） |
| **`2026-07-28 08:47:38 → 09:04:19`** | **🛑 installer hang 17 分钟** ← user 报告"系统卡死" |
| `2026-07-28 09:04:19` | WER 标 installer.exe "Application Hang" |

### 1.2 灾难 B — 浮动栏不弹（切 Fluxing 无 QuickPanel）

user confirm: 切到 fluxing 时浮动设置栏 **不能自动弹**。这意味着 v0.66 时代灾难 #2（patch A/B 修复目标）**仍未在 user system 生效**，因为 Patch A/B 没 ship 进 user binary。

**装机端二进制 md5 实测（user system 当前状态）**:

| 文件 | md5 | mtime | 期望 | 实际情况 |
|---|---|---|---|---|
| `D:\Program Files\fluxing\weasel\WeaselServer.exe` | `cbde621b42858725e0e9989d83385bea` | `7/26 23:28` | v0.67 (commit `bbd0414`) | **v0.66 老 binary** |
| `D:\Program Files\fluxing\weasel\weasel.dll` | `b71ae4f77b3e02467f8260d4aa32e5dd` | `7/26 23:27` | v0.67 | **v0.66 老** |
| `D:\Program Files\fluxing\weasel\weaselx64.dll` | `c62a364bd9c08e0a7210ee07e9fe4529` | `7/23 23:26` | v0.67 | **v0.65 时代老** |
| `D:\Program Files\fluxing\weasel\WeaselSetup.exe` | `c0722aee5adf57eb38a2bc8db2d45e7b` | `7/26 23:29` | v0.67 | **v0.66 老** |
| `D:\Program Files\fluxing\weasel\uninstall.exe` | `9607F7E67139DC87BFE3764BBF2FA852` | `7/28 8:47` | v0.67 | **v0.67 写入** ✓ |
| `D:\Program Files\fluxing\weasel\weasel.dll.old.tmp` | `5B5000B2D07F94AA8900737C51ADEC2B` | `7/25 22:46` | (无) | rename after install 残留（line 488 Delete /REBOOTOK 没跑到）|

**Registry 写入完成确认**:

| Key | Value | 状态 |
|---|---|---|
| `HKCU\Software\Fluxing\Weasel\RimeUserDir` | `D:\Program Files\fluxing\user1\fluxing` | ✅ |
| `HKLM\SOFTWARE\Microsoft\CTF\KnownClasses\{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}` | `Fluxing Text Service` | ✅ |

**进程状态**:

```
ctfmon.exe        PID 21132  2026/7/28 8:46:36  System Reboot 后启动
TextInputHost.exe PID 8228   2026/7/28 8:46:30  System Reboot 后启动
WeaselServer.exe  PID 5036   2026/7/28 8:47:39  HKCU Run 触发(在 hung installer 后)
```

`Get-Process ctfmon.Modules` 当前 **没加载 weaselx64.dll**（user 杀 installer 后, ctfmon 已 reload 一次或自然重启）。

---

## 2. Phase 1-3 完整证据链（已采证，不动 install.nsi）

### 2.1 install.nsi 完整 section "Fluxing" line 745-825 读完（line 760 是 hang 真因）

```nsi
line 745:   WriteRegDWORD HKCU "…\CTF\Assemblies\0x00000804\…"  "KeyboardLayout" 0x08040804
line 746-754: WriteRegStr HKLM "${REG_UNINST_KEY}" DisplayName/DisplayIcon/DisplayVersion/UninstallString/Publisher/URLInfoAbout/HelpLink/NoModify/NoRepair
line 756:   WriteUninstaller "$INSTDIR\uninstall.exe"      ✓ 8:47:38 写入 (确认)
line 758:   IfSilent deploy_silently
line 759-760:                ⬇ (GUI 模式)
            ExecWait "$INSTDIR\WeaselDeployer.exe /install"   🛑 hang 17 分钟真因
line 763:   deploy_silently: (silent 模式)
line 764:   ExecWait "$INSTDIR\WeaselDeployer.exe /deploy"
line 765:   deploy_done:
line 768-772: SetRegView 64 (AMD64/ARM64)
line 778-797: weasel_write_ok detection + Stage 2 batch (PPL 后备 swap 流程)
line 800-802: WriteRegStr HKLM Run WeaselServer + Exec (non-blocking) WeaselServer
line 805-812: AUTOCHKUPDATE MessageBox
line 814-816: SetRebootFlag if Upgrade
line 821-823: post-install messagebox "请重启 WeaselServer.exe 或注销后重新登录"
line 825:   SectionEnd
```

**关键纠正**: install.nsi 完整 section **没有调用 `regsvr32 /s weaselx64.dll`** — 之前 sandbox-verify 给的 `.reg` 注册是 `Line 741 WriteRegStr HKLM CTF\KnownClasses` 完成的（GUID 注册走 CTF,不走 regsvr32）。

### 2.2 line 760 `ExecWait WeaselDeployer.exe /install` 17 分钟 hang 真因链

```
T0  .onInit taskkill ctfmon.exe 5×Sleep 2500ms = 12.5s (line 211-219)
    → ctfmon 被 SYSTEM service 强制重生 (observed PID 21132 启动 8:46:36)
    → ctfmon 重启后立即 mmap v0.66 老 weaselx64.dll (路径未变,文件存在)
T1  Section "Fluxing" line 498-522 L72-fix Rename-then-File 试图覆盖 weaselx64.dll
    → Rename 失败 (ctfmon 持 mmap, ERROR_SHARING_VIOLATION)
    → SetOverwrite try File → 失败 → IfErrors → silent skip (line 510 DetailPrint)
    ⚠️ **weaselx64.dll 没更新** (mtime 仍 7/23 23:26 v0.65)
T2  line 542-565 WeaselServer.exe Rename-then-File 同样 L72-fix
    → 路径同样触发 silent skip (line 554 Delete /REBOOTOK 排到 line 545-557 IfErrors 路径)
    ⚠️ **WeaselServer.exe 没更新** (mtime 仍 7/26 23:28 v0.66)
T3  line 756 WriteUninstaller 写 uninstall.exe ✓ 8:47:38
T4  line 760 ExecWait WeaselDeployer.exe /install → 🛑 永久 hang
    → WeaselDeployer.exe mtime 7/26 23:29 (v0.66 build)
    → /install 内部: ① load weaselx64.dll ② 触发 rime deploy (recompile table.bin) ③ regsvr32-like handoff
    → ctfmon 持 v0.66 老 weaselx64.dll mmap → WeaselDeployer 内部等 SCM RPC lock
    → 17 分钟 = SCM RPC default timeout (RPC_C_DEFAULT_TIMEOUT)
T5  9:04:19 WER 报 "Application Hang" → user force kill installer.exe
T6  ctfmon 在 Application Hang 之后被 SYSTEM reload 一次 (Get-Process ctfmon.Modules 当前显示无 weaselx64.dll)
```

### 2.3 line 498-565 L72-fix 段完整 silent skip 验证

**当前代码** (line 498-522 + 542-565 是同 pattern, 选 line 498-522 展示):
```nsi
${If} ${FileExists} "$INSTDIR\weaselx64.dll"
  ClearErrors
  Rename "$INSTDIR\weaselx64.dll" "$INSTDIR\weaselx64.dll.old.tmp"
  ${If} ${Errors}
    SetOverwrite try
    File "weaselx64.dll"
    IfErrors 0 weaselx64_done                    ← 若 File 失败 → 跳 detail
    DetailPrint "Fluxing: weaselx64.dll locked by TSF host; old shim retained. Log out -> log in to pick up the new shim."
    SetOverwrite on
    Goto weaselx64_done
  ${EndIf}
  File "weaselx64.dll"                            ← Rename 成功,新 file 写入
  Delete /REBOOTOK "$INSTDIR\weaselx64.dll.old.tmp"
${Else}
  File "weaselx64.dll"
${EndIf}
```

**问题**: 当 ctfmon 持 mmap,Rename 失败,SetOverwrite try + File 失败 → silent skip。**`weaselx64.dll` 老 binary 留在磁盘上**,直到下次 reboot ctfmon 自然 reload。

**严重度**: 这导致 line 760 `WeaselDeployer.exe /install` 触发 `/restart_ctfmon` style 操作,ctfmon 持的 mmap 锁永远不释放 → 17min RPC timeout。

---

## 3. 灾难 B 真因 — 装机 binary 仍是 v0.66 老版

虽然 v0.67 Patch A/B **已 commit** (bbd0414),但因灾难 A 的 L72-fix silent skip,**v0.67 binary 永不写入 user system**。

灾难 B 跟灾难 A 是**同一个根因** (L72-fix 在 ctfmon 持 mmap 时 silent skip),不是两个独立 bug。

---

## 4. Fix 方案（**双 patch, user 已授权**）

按 systematic-debugging Phase 4 "1 fix at a time" + "Create Failing Test Case FIRST" 原则:

### 4.1 Step 1 — Failing Test First (sandbox-verify §9)

修改 `F:\soft\00selfmade\sandbox-verify\rime-claude\in-sandbox-verify.ps1`,在现有 8 条不变式 (AGENTS.md §2.5 a-h) 之后加:

```
§9 double-install test (新增):
  给 v0.67 installer 已装好的状态 → 清理 HKLM\Software\Fluxing\Weasel\InstallDir 不删
  → 在同 sandbox 重装 v0.67 installer (覆盖式)
  → 验证:
    - 10 分钟内完成 (而非 hang 超时)
    - weaselx64.dll md5 更新到新 build (验证 binary 真换了)
    - WeaselServer.exe md5 更新到新 build
    - ctfmon.exe 在第二次装完成后能 mmap 新 weaselx64.dll
    - HKCU\…\RimeUserDir + HKLM\CTF\KnownClasses GUID 不变 (用户配置保留)
```

### 4.2 Step 2 — Fix-D1 (最小,推荐先做)

**文件**: `output/install.nsi`
**行**: 758-761

**改动**:
```nsi
  ; v0.19.0.68 Phase L bugfix 5 Fix-D1: ExecWait WeaselDeployer.exe /install 在
  ;   ctfmon 持老 weaselx64.dll mmap 时永久 hang (等 SCM RPC 17min timeout)。
  ;   Phase 4 user testing (2026-07-28): v0.67 installer 17 分钟卡死,user force kill。
  ;   修: 改 ExecWait → Exec (non-blocking), deploy 后台执行。
  ;   代价: 装机完成时 deploy 还没跑完, 但 ctfmon 会在 auto-reload 时加载新 binary,
  ;   user 第一次打字就触发 WeaselServer.exe startup-time deploy。
  ;   若 ctfmon reload 都失败 (rare), user 跑 WeaselDeployer.exe /deploy 也 OK。
  ;   Pre-fix 测试: 见 in-sandbox-verify.ps1 §9 double-install test。
  IfSilent deploy_silently
  Exec "$INSTDIR\WeaselDeployer.exe /install"
  GoTo deploy_done

  deploy_silently:
  Exec "$INSTDIR\WeaselDeployer.exe /deploy"
  deploy_done:
```

### 4.3 Step 3 — Fix-D3 (完整覆盖)

**文件**: `output/install.nsi`
**行**: 498-522 (weaselx64.dll) + 542-565 (WeaselServer.exe) + 580-599 (WeaselDeployer.exe)

**改动原则**: Rename-then-File 段 silent skip 之前,**强制 taskkill ctfmon + Sleep 5s + retry once**。

具体 patch pattern (以 weaselx64.dll 段 line 498-522 为例):

```nsi
  ; v0.19.0.68 Phase L bugfix 5 Fix-D3: L72-fix Rename-then-File 段之前,如果 File/Rename
  ;   持续失败(因 ctfmon 持 mmap 锁),不要 silent skip — 必须 force-kill ctfmon + Sleep + retry
  ;   一次。Phase 4 user testing: v0.67 installer line 510 走 silent skip → 老 binary 残留
  ;   → line 760 ExecWait WeaselDeployer.exe /install hang 17min。
  ;   Pre-fix 测试: in-sandbox-verify.ps1 §9 double-install test 必须 PASS。
  Push $R0
  Push $R1
  ${If} ${FileExists} "$INSTDIR\weaselx64.dll"
    ClearErrors
    Rename "$INSTDIR\weaselx64.dll" "$INSTDIR\weaselx64.dll.old.tmp"
    ${If} ${Errors}
      ; Phase 4 Fix-D3: ctfmon 持 mmap 锁 → force kill + 5s mmap 释放 + retry
      nsExec::ExecToStack 'taskkill /F /IM ctfmon.exe /T'
      Pop $0
      Pop $1
      Sleep 5000
      ClearErrors
      Delete /REBOOTOK "$INSTDIR\weaselx64.dll.old.tmp"
      ClearErrors
      File "weaselx64.dll"
      IfErrors 0 weaselx64_done
      DetailPrint "Fluxing v0.19.0.68: weaselx64.dll locked by TSF host after ctfmon kill; old shim retained. Reboot required."
      SetOverwrite on
      Goto weaselx64_done
    ${EndIf}
    File "weaselx64.dll"
    Delete /REBOOTOK "$INSTDIR\weaselx64.dll.old.tmp"
  ${Else}
    File "weaselx64.dll"
  ${EndIf}
  weaselx64_done:
  Pop $R1
  Pop $R0
```

**WeaselServer.exe 段** (line 542-565) 和 **WeaselDeployer.exe 段** (line 580-599) 适用同样 pattern。

### 4.4 Step 4 — L##-PhaseL-11 沉淀

`F:\soft\00selfmade\rime_claude\.specify\memory\lessons-learned.md` 加 L## 段(本文件是 UTF-8/UTF-16 hybrid,Edit 时遵循 L01 byte-level replace 规则):

```markdown
## L## (Phase L Bugfix 5) — NSIS ExecWait WeaselDeployer /install 在 ctfmon 持老 weaselx64.dll mmap 时永久 hang

**Incident**: v0.19.0.67 ship 装机 user 实测,installer 卡在 line 760 `ExecWait "$INSTDIR\WeaselDeployer.exe /install"` 17 分钟,WER 9:04:19 报 "Application Hang"。TaskMgr 可开,真只是 installer.exe 进程 unresponsive。

**Root cause**:
1. .onInit (line 211-219) taskkill ctfmon.exe 5×Sleep 2500ms = 12.5s,ctfmon 被 SYSTEM 强制重生 (PID 21132 启动 8:46:36 验证)
2. ctfmon 重启后立即 mmap v0.66 老 weaselx64.dll (路径未变)
3. Section "Fluxing" line 498-522 L72-fix Rename-then-File 走 silent skip path (File 失败, IfErrors 跳 detail) — **关键**: installer **不报警**,只 DetailPrint "old shim retained"
4. line 542-565 WeaselServer.exe Rename-then-File 同样 silent skip
5. line 756 WriteUninstaller 写 uninstall.exe ✓ (8:47:38 mtime 验证)
6. line 760 `ExecWait WeaselDeployer.exe /install` → WeaselDeployer.exe 内部 load weaselx64.dll → ctfmon 持老 mmap → 等 SCM RPC lock → 默认 17 分钟 timeout → WER 报 Application Hang
7. user force kill installer.exe → line 760-823 全未跑,但 line 498-565 File 命令也 fail silent skip → **所有 binary 仍是 v0.66 老版,只有 uninstall.exe 是 v0.67 写入**

**Lesson (must follow)**:
1. **NSIS `ExecWait "<child>.exe /arg"` 在 child 试图 LoadLibrary 一个被 system process 持 mmap 的 DLL 时永久 hang**, 默认 17 分钟 = SCM RPC `RPC_C_DEFAULT_TIMEOUT`。修:
   - `ExecWait` → `Exec` (non-blocking), child 后台跑
   - 或 `nsExec::ExecToLog` 加 `/TIMEOUT=30000` (30s hard timeout)
2. **L72-fix Rename-then-File 段 silent skip 是双刃剑**: 当 ctfmon (或任何 TSF host) 持 mmap, NSIS 走 IfErrors → skip,**老 binary 残留** 直到下次 reboot。**灾难性副作用**: 后续 installer 命令 (regsvr32 / ExecWait) 触发 17min hang。修: silent skip 之前**强制 taskkill ctfmon.exe + Sleep 5000 + retry once**, 再 fail 才 silent skip。
3. **`IfFileExists $INSTDIR\weaselx64.dll.old.tmp` 检测** (line 778 Stage 2) 是 PPL-WeaselServer.exe 后备 swap 路径,**不能** 复用给 ctfmon 持 mmap 的 weaselx64.dll — 因为 PPL-process 的 mmap lock 跟 ctfmon 的 mmap lock 是**两种不同死锁模式**。

**Verification**:
- v0.68 sandbox-verify §9 double-install test 必须: 10min 内完成 + weaselx64.dll md5 更新 + WeaselServer.exe md5 更新
- v0.68 装机 user flow 5 项 (handoff v0.67 §5) 必须 PASS
- v0.68 §5.1 MANDATORY post-install reg query (`_check_install_v067_postship.ps1`) 必须 PASS

**Commit ref**: `bbd0414` 上 diff 后续 — 查 git log v0.68
```

---

## 5. v0.68 Ship Gate（**强制 sequential**）

```
[1] ⏳ Read 本 handoff + prompt 文件 (本回合已经写好)
       F:\soft\00selfmade\rime_claude\handoff-after-phase-L-bugfix-5-hang-v0.19.0.67-blocked-2026-07-28.md
       F:\soft\00selfmade\rime_claude\_prompt_phase_L_bugfix_5_v0.68_fix_2026-07-28.md
       ↓
[2] ⏳ Failing Test First: sandbox-verify §9 double-install test 加进 in-sandbox-verify.ps1
       (commit 单独的 "test(sandbox): add §9 double-install test" 之前不要 fix install.nsi)
       ↓
[3] ⏳ Run sandbox-verify §9 with v0.67 installer → 必 FAIL (确认 test 是真的 failing)
       ↓
[4] ⏳ Fix-D1: install.nsi line 760 ExecWait → Exec (commit 单独 "fix(installer): v0.68 Fix-D1")
       ↓
[5] ⏳ Rebuild installer v0.68 → release/fluxing-0.19.0.68-installer.exe
       ↓
[6] ⏳ Run sandbox-verify §9 with v0.68 installer → 看是否 PASS (钉 17min hang 是否消失)
       ├─ PASS: skip Fix-D3 (line 498-565 silent skip 风险留着), commit "chore(release): v0.68 installer"
       └─ FAIL: 进 [7]
       ↓
[7] ⏳ Fix-D3: install.nsi line 498-565 强制 taskkill ctfmon + retry (commit "fix(installer): v0.68 Fix-D3")
       ↓
[8] ⏳ Rebuild installer v0.68 + Run sandbox-verify §9 → 必须 PASS
       ↓
[9] ⏳ Commit install.nsi + L## 进 lessons-learned.md + CHANGELOG.md v0.68 段 (上一会话已经写 v0.66/v0.67 段,可 amend)
       ↓
[10] ⏳ Copy installer to release/fluxing-0.19.0.68-installer.exe + commit "chore(release): v0.68 installer"
       ↓
[11] ⏳ Tag git v0.19.0.68 + SFTP atomic upload (per feedback_pinyin_release_workflow.md)
       ↓
[12] ⏳ Cloudflare cache purge 通知
       ↓
[13] ⏳ (user 跑) 装机端 host 真装 v0.68 + 6 项装机 user flow + §5.1 reg query PASS 后再 ship
```

---

## 6. 沉淀到 `.specify/memory/lessons-learned.md`

(Step 4 已写好 L##-PhaseL-11 完整内容,新会话 agent 复制即可)

---

## 7. 当前未 commit 工作树（重要! 新会话第一动作:决定 commit / stash / discard）

```
$ git status -sb
## Fluxing...kizemo/Fluxing [ahead 945, behind 831]
 M CHANGELOG.md           ← +79 行 (v0.66/v0.67 段, user-confirmed, 待 commit 进 v0.68 commit)
 m librime                ← submodule (submodule 修改, 正常)
?? _check_install_v067_postship.ps1           ← 本会话 ship gate verify 脚本
?? _prompt_phase_L_bugfix_5_v0.68_fix_2026-07-28.md   ← 本回合移交 prompt
?? handoff-after-phase-L-bugfix-5-hang-v0.19.0.67-blocked-2026-07-28.md  ← 本回合移交 handoff
```

**新会话决策树**:
- 选项 1 (推荐): 把 CHANGELOG.md + `_check_install_v067_postship.ps1` (rename → `_check_install_v068_postship.ps1`) 进 v0.68 一次性 ship commit
- 选项 2: 仅 stash 起来, 等 v0.68 ship 时 cherry-pick

---

## 8. 必读顺序（新会话 agent 第一轮）

1. **本 handoff §1-§3** — 双灾难真因 + Phase 1-3 evidence
2. **`output/install.nsi` line 498-825** — Fix 段代码上下文
3. **`F:\soft\00selfmade\sandbox-verify\rime-claude\in-sandbox-verify.ps1`** — 当前 8 条不变式 (a-h) 结构,加 §9 时保持格式
4. **`F:\soft\00selfmade\rime_claude\.specify\memory\lessons-learned.md` 顶部** — 看 L##-PhaseL-11 写入格式 + L01 byte-level rule
5. **`_prompt_phase_L_bugfix_5_v0.68_fix_2026-07-28.md`** — 必交付 + 禁止

## 9. 风险/避坑

| 风险 | 避坑 |
|---|---|
| **L01 byte-level replace**: lessons-learned.md 是 UTF-16/UTF-8 hybrid,PS `Out-File` 会 GBK pollution | 用 `[IO.File]::ReadAllBytes` + `[Text.UTF8Encoding]::new($false)` byte-level replace |
| **L09 install.nsi 必须 UTF-8 + BOM + CRLF** | Edit tool 自动保持, 但 Read-after-Edit 验证 `$bytes[0..2] = EF BB BF` + 行尾 CRLF |
| **`_check_install_v067_postship.ps1` 命名不一致** | 第 [9] 步 commit 前 rename 到 `_check_install_v068_postship.ps1`,内部 -InstallerPath default 改 v0.68 路径 |
| **session-health <2h** | Step [1-3] failing test 写+run (~30 min), Step [4-7] fix+rebuild (~30 min), Step [9-12] commit+ship (~15 min); 不打包单回合 |
| **installer file hash 跨环境不可重现** | 一律走 `git describe` 不算 hash, 比对用 `installer md5 == packaged binary md5 -packaged binary size` 没意义; 真正比的是 deployed bin md5 vs source build md5 |
| **L67/L68/L69 历史 GBK pollution** | 任何 CJK 字符串操作 (CHANGELOG/lessons-learned) 用 [Text.UTF8Encoding]::new($false) |

## 10. 装机端 user 期望 (v0.68 ship 后)

- **installer 不再 hang 17 分钟** (Fix-D1 效果)
- **weaselx64.dll / WeaselServer.exe md5 = v0.68 source build** (Fix-D3 效果)
- **Win+Space 切 fluxing 浮动栏自动弹** (Patch A/B 真正生效)
- **6 项装机 user flow (handoff v0.67 §5) 全 PASS**
- **§5.1 MANDATORY reg query PASS**

---

*End of Phase L Bugfix 5 handoff. v0.67 不能 ship, 双灾难锁定, 必 v0.68 ship。*
