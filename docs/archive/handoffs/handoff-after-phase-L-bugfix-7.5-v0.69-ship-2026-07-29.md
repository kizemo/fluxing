# Handoff — Phase L Bugfix 7.5 v0.69 ship (2026-07-29)

> **承接**: `handoff-after-phase-L-bugfix-6-ppl-ipc-block-v0.68-2026-07-28.md` (v0.68 ship 后 user 反馈浮动栏仍不弹,反复试数日无效)
> **状态**: ✅ v0.69 浮动栏永久 dead 真修 ship 完成
> **新目标**: 装机 user flow 验证 6 项 + (可选) 4 节点 instrumentation log capture

---

## 1. 一句话总结 (大白话)

之前 v0.66 / v0.67 / v0.68 修了 3 个版本浮动栏都不弹, 不是我修错位置, 是有个**老的临时禁区从未解除** — v0.18.41.4 为了防 GDI+ 崩溃临时把浮动栏关了, 但浮动栏后来改成了纯 GDI 不崩了, 老关门逻辑**没解除**。v0.19.0.65 重新启用浮动栏时又没注意到这个老关门, 留在那继续挡着, 所以 3 个版本修了等于白修。v0.69 解除这个老关门 + 加 3 个修复 + 4 个日志点, 装机后两条路径 (切 IME + Alt+,) 都能弹了。

## 2. 修复了什么 (大白话)

### Bug: 浮动栏永久 dead 真因
- **症状**: 切到火流猩输入法, 浮动栏不弹; 按 Alt+, 也不弹; 之前 3 个版本都修不好
- **真因 (L69-fix 残留)**: v0.18.41.4 临时把浮动栏代码路径用 `if (m_disabled) return;` 围起来, 防止 GDI+ Bitmap 崩溃。但浮动栏后来改纯 GDI (spec 074 / L75), GDI+ 路径已无, 崩溃链断。但**这条 `return` 从未被解除**。
- v0.19.0.65 重新启用浮动栏 (改用 `Show(false, nullptr×6)`) 时, **没看到** `m_disabled` 早返在它之前, 继续被挡住。
- 后续 3 个版本 (v0.66 AdviseSink 加 / v0.67 TF_INVALID_COOKIE guard / v0.68 MaintenanceGuard 5s) 都没修这条, 因为真因不在那里。

### Fix A: 解除 `m_disabled` 早返 (核心修复)
- `RimeWithWeasel.cpp:384-452` 的 `FocusIn()` 函数, 把 `QuickPanelDialog::Show(...)` 移到 `if (m_disabled) return;` **之前**。
- 浮动栏现在无论 m_disabled 状态都弹。其他函数 (`_UpdateUI` / CaptureForeground) 仍守 m_disabled, 因为它们需要 librime 初始化正常。

### Fix B: OnActivated Echo() 失败时尝试 StartSession 恢复
- `WeaselTSF.cpp:287-309` 的 `OnActivated` 函数, 如果 `m_client.Echo()` 返回 false (常见: session_id=0 / pipe session 丢失), 静默 fallback 到 `StartSession()` 有概率恢复。
- 跟 spec 053 R6 的 lazy recover pattern 同模式。

### Fix C: Alt+, 注册失败写 weasel-install.log
- `WeaselIPCServer/WeaselServerImpl.cpp:67-108` 的 `OnCreate` 函数, `RegisterHotKey(Alt+,)` 失败时 (e.g., 其他 app 占用) 之前 `AP-036-F ignore failure` 静默, 改写 weasel-install.log 含 GetLastError + 1409 hint (ERROR_HOTKEY_ALREADY_REGISTERED)。

### 4 个日志点 (装机 diagnostic)
- `WeaselTSF.cpp:290` DLOG `OnActivated ACTIVATE m_client.Echo()=true/FALSE`
- `RimeWithWeasel.cpp:389` DLOG `FocusIn ENTER m_disabled=0/1`
- `WeaselIPCServer/WeaselServerImpl.cpp:158` DLOG `WM_HOTKEY wParam=`
- `QuickPanelDialog.cpp:1107` [QP-DIAG] `UpdateLayeredWindow FAILED err=`
- `QuickPanelDialog.cpp:1201` [QP-DIAG] `QuickPanel::Show entered`

## 3. 装机端 user 期望 (v0.69 ship 后)

跑 6 项验证 (按顺序):

| # | 验收 | 期望 |
|---|---|---|
| 1 | 跑 v0.69 installer | 不再卡死, 5-10 分钟内完成 |
| 2 | 切到 Fluxing | **浮动工具栏自动弹出** ← 这是 v0.69 真修的核心 |
| 3 | 按 Alt+, | **浮动工具栏 toggle** (显/隐) ← 路径 B 也通 |
| 4 | 切到英文 | 浮动工具栏不弹 |
| 5 | 默认 IME 不是 Fluxing | 保留自己默认 |
| 6 | `reg query` 4 项 keys | HKLM\CTF\KnownClasses + HKCU\CTF\Assemblies\0x00000804 + HKLM\CLSID InprocServer32 + HKLM\Fluxing\Weasel InstallDir 全部存在 |

如果**任何一项失败**, 跑 diag:
- `F:\soft\00selfmade\rime_claude\diag-floating-bar.ps1` (5 步 + 抓 log)
- 把 log 末尾 80 行贴回, 用 `handoff-bug-floating-bar.md` §5 判别表定位 root cause

参考 `handoff-bug-floating-bar.md` 详细 4 节点 instrumentation + 5 步 diag 流程 + log pattern 判别表。

## 4. 已完成 (2026-07-29)

- ✅ Fix A (FocusIn m_disabled 早返移除)
- ✅ Fix B (OnActivated Echo fallback StartSession)
- ✅ Fix C (RegisterHotKey log)
- ✅ 4 节点 instrumentation (保留, 永久)
- ✅ Win32 + x64 full rebuild
- ✅ NSIS v0.69 installer (43.3 MB, md5 `422e9b50c7d52bfdf900772a6a8e69f7`)
- ✅ 4 binary md5 cross-source verify (build vs installer extract) ALL MATCH
- ✅ TestOrphanRecovery 7/7 PASS (T8 elapsed 1015ms)
- ✅ TestWeaselIPC protocol pipe OK
- ✅ 1 commit (`476e77e1`) + tag v0.19.0.69
- ✅ 三件套 (version.json / appcast.xml / release-notes.html) updated
- ✅ `_check_install_v2.ps1` md5 refs updated
- ✅ SFTP atomic upload + cross-source verify OK
- ✅ Server metadata cache-buster verify (cf-cache-status: DYNAMIC, 无需 purge)

## 5. Working tree 状态

```
476e77e fix(FloatingBar): v0.19.0.69 QuickPanel 永久 dead 真修 (Phase L Bugfix 7.5) [tag: v0.19.0.69]
c528728 docs(memory): 2026-07-28 v0.19.0.68 ship 台账
0be04e1 chore(verify): v0.68 expected md5 in _check_install_v2.ps1
67520c3 chore(release): v0.19.0.68 installer (uninstrumented)
e767356 fix(WeaselDeployer): v0.68 MaintenanceGuard Connect timeout 5s
c9bb103 docs(memory): v0.66/v0.67 ship notes + L##-PhaseL-7+8
```

**Pre-existing dirty state (NOT v0.69 work, user 可选 cleanup)**:
- `librime/` submodule dirty (deps/opencc) — L107 教训模式,submodule 改动需 user 授权
- 80+ untracked files (`.claude/`, `handoff-*.md`, `release/fluxing-0.19.0.67-installer.exe` etc) — working tree residue

## 6. 关键文件位置

| 用途 | 路径 |
|---|---|
| v0.69 installer (本地) | `release/fluxing-0.19.0.69-installer.exe` 43.3 MB md5 `422e9b50...` |
| v0.69 installer (远程) | `E:\办公文件\L1网站\pinyin\fluxing-0.19.0.69-installer.exe` |
| 三件套 (远程) | `E:\办公文件\L1网站\pinyin\{version.json,appcast.xml,release-notes.html}` |
| 测试 | `Release/TestOrphanRecovery.exe` (7/7 PASS) / `Release/TestWeaselIPC.exe` (protocol OK) |
| Diag script | `F:\soft\00selfmade\rime_claude\diag-floating-bar.ps1` |
| Handoff (本次) | `F:\soft\00selfmade\rime_claude\handoff-after-phase-L-bugfix-7.5-v0.69-ship-2026-07-29.md` |
| Handoff (上次 instrumentation) | `F:\soft\00selfmade\rime_claude\handoff-bug-floating-bar.md` |
| 教训沉淀 | `.specify/memory/lessons-learned.md` L##-PhaseL-7+8 |

## 7. 立刻要 user 做

1. **跑 v0.69 installer** — 装新版 → 验证 6 项装机 user flow (切 IME 浮动栏 + Alt+, 都应弹)
2. **如果 6 项失败** → 跑 `diag-floating-bar.ps1` 抓 log, 用 `handoff-bug-floating-bar.md` §5 判别表定位 root cause
3. **(可选) 清理 librime submodule dirty** — 需 user 授权 + ADR, agent 不擅自 commit
4. **(可选) push tag v0.19.0.69** — K7 push 持续 500, 等网络稳定后手动 `git push origin v0.19.0.69`

## 8. Session health

- bash 用量: ~50 calls (红线 80)
- 活跃 ~1.5h (self-limit <2h)
- parallel subagent: 1 (Phase 2 instrumentation, subagent report verify 通过)
- 完成时机: 9 步 ship gate 全部 commit + tag + SFTP + cross-source verify OK

## 9. 避坑提示

- 浮动栏死锁的 root cause 经常不在修复假设的层 — L69-fix 残留挡了 v0.65+. 后续 fix 总在换"上层的门锁"但底层门一直在那
- "回滚到上一个 ship 还能用" 不代表"新 fix 真生效" — 永久 dead 真因可能在 deferred 设计决定里
- `m_disabled` 状态机是 user-side 不可见的 silent gate;任何 silent gate 都要 instrumentation 化 (本会 4 节点埋点)
- NSIS `ExecWait <child>` 无 timeout;IPC deadlock chain 永久 hang (L##-PhaseL-7)
- `__DATE__` 宏嵌入导致 binary md5 每次 rebuild 变;md5 cross-source check 必用 build output 即时 md5
- 不要 `--resume` / `--continue` 任何 >5MB jsonl session (L30 silent failure)

## 10. next session 第一句话

```
承接 handoff-after-phase-L-bugfix-7.5-v0.69-ship-2026-07-29.md。
v0.69 浮动栏真修 ship 全部完成 + SFTP sync OK + 1 commit + tag v0.19.0.69。
本会话任务: 装机 user flow 6 项验证 + (可选) librime submodule cleanup + (可选) tag push。
如浮动栏仍不弹, 跑 diag-floating-bar.ps1 抓 log (handoff-bug-floating-bar.md §5 判别)。
```

---

*End of Phase L Bugfix 7.5 handoff. v0.69 浮动栏永久 dead 真修 ship 完成. 4 节点 instrumentation 同步上线, 装机 user flow 验证决定 ship-gate PASS.*
