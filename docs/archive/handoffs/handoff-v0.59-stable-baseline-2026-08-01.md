# Handoff — v0.19.0.59 stable baseline milestone (2026-08-01)

> **承接**: 4 周 6+ 次 ship 失败链 (`v0.66 → v0.67 → v0.68 → v0.69 → v0.70 → v0.71 → v0.74.1 → v0.74.2 → v0.74.3 → v0.62 rollback`)
> **状态**: v0.19.0.59 (commit `1c0d711`, 2026-07-24, A5 sftp_sync 通用化 + 56 tests PASS) **用户装机验证 OK**
> **下一步**: 新会话第一件事 = `git tag v0.59-stable-baseline + push` 把 v0.59 设为里程碑节点

---

## 0. 当前状态

| 状态 | 值 |
|---|---|
| HEAD | `fd5a644` (v0.19.0.62 commit, **working tree 有 v0.62 source edits + v0.59 installer in release/**) |
| Branch | `Fluxing` |
| User 装机版本 | v0.19.0.59 installer (md5 `69fa60cd5a30763c970ad54867ed10d7`, 43,379,147 bytes) |
| User 验证 | **OK** — 输入不卡,QuickPanel manual Alt+, work,常用短语 Alt+. work |
| Stash@0 | 完整 v0.74.3 WIP (FocusIn 撤回 + hot-path cache + admin check + spec 075 + L##-PhaseL-9.11),等下 v0.75 spec 076 处理 |
| Output MD5 | w0.62 (用户已装) — 但**真用 v0.59** (用户已装 v0.59) |

## 1. v0.59 为什么是 last good baseline

per commit `1c0d711` 2026-07-24 "v0.19.0.59 installer (A5 sftp_sync 通用化 + 56 tests)" message:

| 特性 | v0.59 状态 |
|---|---|
| QuickPanel manual Alt+, | ✅ spec 070 v3-rev3 (pure GDI + WIC) |
| 常用短语 out-of-process | ✅ Phase K3 spec 051 |
| 56/56 tests PASS | ✅ |
| FocusIn Show re-enable | ❌ (v0.65 commit `84f01c5` 才加, **崩溃源**) |
| System32 shim DLL pollution | ❌ (v0.66 era 才引入, commit `c9bb103`) |
| Stage 2 install | ❌ (v0.74.1 commit `4694a1d` 才加) |
| WeaselPanel GDI+ | **⚠️ always there** (WeaselPanel.cpp:107) — per-input crash 仍可能,但用户未报告 |

## 2. 4 周事故链 (L##-PhaseL-9.11-Retro v0.74.x 已写)

```
v0.66 era (2026-06-22): 写真因 + 写 System32 残留 shim DLLs
v0.67 (2026-07-25): 17min hang on install (PPL lock)
v0.68: MaintenanceGuard
v0.69: QuickPanel 永久 disabled (L69-fix residue)
v0.70: Fix B revert
v0.71: fire-and-forget Exec + immediate MessageBox (freeze feel)
v0.74.1: F4 stage-at-startup + 9.6 system32 cleanup (silent fail)
v0.74.2: silent Abort + IfErrors (still silent fail)
v0.74.3: FocusIn Show 撤回 + hot-path cache (没修真因)
v0.62 rollback: install.nsi 不动 System32,12 个 shim DLL 仍 load → 0xc0000374
v0.59 ship OK: A5 sftp_sync 通用化,真 last good ✅
```

## 3. 关键 lessons-learned (`L##-PhaseL-9.10 / 9.11-Retro / 9.12 / 9.13`)

- **L##-PhaseL-9.10**: NSIS Delete + IfErrors silent fail (per cmem 2026-07-30)
- **L##-PhaseL-9.11-Retro v0.74.x**: 严重 bug 出 → 立刻回退,不修补当前 (per 2026-07-31)
- **L##-PhaseL-9.12-GeneralizeStateRisk**: 装机 cycle inbound state pollution must be first-class pre-check (per 2026-07-31)
- **L##-PhaseL-9.13-CleanupPreserveCriticalKeys**: cleanup script 必 selective delete,永远保留 InstallDir / RimeUserDir / Run key / CTF Assemblies (per 2026-07-31)

**真因 (未修)** = `WeaselUI/WeaselPanel.cpp:107` `GdiplusStartup()` + `WeaselPanel` per-input GDI+ 画 → heap corruption → 跨进程 0xc0000374. v0.59 也存在 WeaselPanel GDI+,但用户未报告 per-input crash (可能因为 system state 已 clean + v0.59 在 v0.66 系统 pollution 之前 ship).

## 4. 文件 + 路径索引

| Path | 用途 |
|---|---|
| `release/fluxing-0.19.0.59-installer.exe` | **当前装机版本** (md5 `69fa60cd5a30763c970ad54867ed10d7`, 43,379,147 bytes) |
| `output/archives/fluxing-0.19.0.59-installer.exe` | 原始 archive (同 md5) |
| `output/archives/fluxing-0.19.0.62-installer.exe` | 测试装版本 (用户已卸载) |
| `_diag_v0.62_clean_state_admin.bat` | admin system state cleanup (selective delete, 保 critical keys) |
| `_diag_v0.62_state_pollution_audit.ps1` | 5-class state pollution audit |
| `_diag_v0.62_kill_and_delete.ps1` | aggressive System32 shim DLL cleanup (Win32 DeleteFile API) |
| `_diag_v0.62_fix_run_key.ps1` | HKLM Run key + 5 critical registry keys restore |
| `_diag_v0.62_schedule_delete_system32.ps1` | MoveFileEx MOVEFILE_DELAY_UNTIL_REBOOT 备份 |
| `_diag_v0741_*.ps1` | original Phase L diagnostic scripts |
| `CODE_MAP.md` | 完整 code map (commit 2026-07-31) |
| `DIAGNOSTIC_REPORT_v0.62_state_pollution_2026-07-31.md` | v0.62 State pollution 诊断报告 |
| `handoff-input-crash-root-cause-2026-07-31.md` | v0.74.3 input-crash 完整 handoff (with system state fix + diagnosis) |
| `stash@{0}` | 完整 v0.74.3 WIP (save for v0.75 spec 076 WeaselPanel GDI+ rewrite) |
| `.specify/specs/075-focusin-quickpanel-rollback/` | spec 075 (v0.74.3 WIP, deferred to v0.75) |

## 5. New session should:

**第一步 (最重要)**:
1. ⚠️ **保留 working tree 状态 — 不要 rebase / reset**
2. ⚠️ **保留 stash@{0}** (v0.74.3 WIP)
3. **第一件事**: `git tag v0.59-stable-baseline v0.19.0.59` (commit `1c0d711`) — tag v0.59 commit as stable baseline
4. **第二件事**: `git push kizemo Fluxing --tags` (push tag to GitHub)
5. **或**: `git checkout v0.19.0.59 && git checkout -b v0.59-stable-baseline && git push kizemo v0.59-stable-baseline` (branch 代替 tag)
6. **不要** 修改 source code, **不要** 重 build v0.62
7. **不要** 触发 v0.74.3 changes (从 stash 0)

**用户已装机 v0.59, OK confirmed**. 不需要再装机测试。新会话应该把 v0.59 设为 github branch, 保留作为 easy rollback point.

## 6. v0.75 / spec 076 待办 (NEVER do now)

- WeaselPanel.cpp:107 GdiplusStartup() 重写 — pure GDI or D2D
- WeaselPanel per-input GDI+ draw 不 leak
- 入 process 重启后真机 verify 24h
- 这是未来"v0.75 spec 076 WeaselPanel GDI+ rewrite" 的工作

**不要**现在做. **不要**写新 spec for v0.75 unless user explicitly asks.

## 7. **没改任何 source code**, 仅做 wrapper work

- ✅ System32 shim DLLs 全删 (admin)
- ✅ HKCU/HKLM\Software\Fluxing critical keys 重写
- ✅ HKLM\...\Run\WeaselServer 重写
- ✅ WeaselServer.exe 进程 restart (新 PID 12876)
- ✅ v0.59 installer 复制到 release/

**未 commit / 未 push / 未 SFTP.** 等新会话做 tag + push.

---

## 配套: 新会话 prompt (给 user 直接复制)

单指令新会话,严格按这个 handoff.md + 第一步执行:

```
承接 Fluxing v0.19.0.59 stable baseline 里程碑节点,complete handoff 在
F:\soft\00selfmade\rime_claude\handoff-v0.59-stable-baseline-2026-08-01.md.

第一步 (必做):
1. Read 整个 handoff.md
2. git status -sb  + git stash list  + git tag --list "v0.1*" verify state
3. **第一件事**: git tag v0.59-stable-baseline v0.19.0.59 && git push kizemo Fluxing --tags
   (or: git checkout v0.19.0.59 && git checkout -b v0.59-stable-baseline && git push kizemo v0.59-stable-baseline)
4. 跑 _diag_v0.62_state_pollution_audit.ps1 verify machine state clean
5. Confirm v0.59 installer md5 = 69fa60cd5a30763c970ad54867ed10d7 in release/
6. 简短报告: tag/push 成功 + 当前 git state + user 装机 v0.59 OK

**约束**:
- 不动 source code (v0.74.3 WIP 在 stash@{0} 保留)
- 不重 build v0.62
- 不装 v0.59 (user 已装)
- 不写新 spec (v0.75 spec 076 WeaselPanel GDI+ rewrite 是未来工作)
- 任何改动先 Read handoff.md
```
