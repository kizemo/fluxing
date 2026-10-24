# Handoff — Phase L Bugfix 4 v0.19.0.67 (2026-07-26)

> **承接**: `handoff-after-phase-L-postship-freeze-v0.19.0.67-2026-07-26.md` (v0.19.0.66 装机灾难)
> **状态**: ✅ **v0.19.0.67 build + NSIS installer + commit 已完成**,等 sandbox-verify 装机 user flow
> **Commit**: `bbd0414 fix(WeaselTSF+installer): v0.19.0.67 — Phase L 装机 bugfix 4 真修 (TF_INVALID_COOKIE guard + PPL taskkill 缩短 + WriteRegStr Sleep)`
> **Installer**: `F:\soft\00selfmade\rime_claude\output\archives\fluxing-0.19.0.67-installer.exe` (43.3 MB)
> **关键发现**: subagent root cause 调查报告**部分错误** — 灾难 #2 (无浮动栏) 真因不是 QuickPanelDialog GDI+ 路径,该路径在 v0.19.0.10 spec 074 已经清理完毕

---

## 1. 本会话 apply 的 patches (verifiable per current source)

| Patch | 文件:行 | 改动 | 对应 subagent 候选 |
|---|---|---|---|
| **A** | `WeaselTSF/WeaselTSF.cpp:248-252` | `_UninitActiveLanguageProfileSink` 开头加 `if (_dwActiveLanguageProfileSinkCookie == TF_INVALID_COOKIE) return;` | 候选 A (HIGH H1 必修) |
| **B** | `WeaselTSF/WeaselTSF.cpp:240-242` | `_InitActiveLanguageProfileSink` AdviseSink 失败路径加 `_dwActiveLanguageProfileSinkCookie = TF_INVALID_COOKIE;` | 候选 A (MEDIUM M1 推荐) |
| **C1** | `output/install.nsi:148-161` | PPL WeaselServer.exe taskkill 10×30s → 5×~10s 简化 | 候选 C (灾难 #1 主体) |
| **C2** | `output/install.nsi:738-740` | WriteRegStr HKLM CTF\KnownClasses 前置 Sleep 3000ms | 候选 C (灾难 #1 主体) |

**Patch A/B gold standard 参考**: `_InitThreadMgrEventSink` (WeaselTSF.cpp:45) + `_UninitThreadMgrEventSink` (WeaselTSF/ThreadMgrEventSink.cpp:55)

---

## 2. ❗ 关键发现: QuickPanelDialog GDI+ claim 错误

**subagent 报告声称**: "QuickPanelDialog::Show() 仍走 v0.18.41.4 `Bitmap::FromStream` GDI+ 路径 → WeaselServer.exe heap corruption → TSF IPC 全死 → 浮动栏创建失败"

**实测结果 (Read QuickPanelDialog.cpp 1-30, 217-322, 1141-1216)**:
- 文件头 line 1-19 明确写 "QuickPanelDialog v3-rev3 — PURE GDI + per-pixel alpha (spec 074 v0.19.0.10)"
- Line 5: "❌ 绝对不用 GDI+ Bitmap(IStream*) (L67-L69 崩溃链)"
- Line 217-322: `LoadLogoWIC` 用 **WIC** (`IWICImagingFactory` + `CreateDecoderFromFilename`),不是 GDI+ Bitmap
- Line 1141-1216 `Show()`:**NO** `Bitmap::FromStream`,**NO** `CreateStreamOnHGlobal`,**NO** `Gdiplus::Bitmap` 调用
- `WeaselUI/WeaselPanel.cpp` (候选框) 仍用 GDI+,但跟 QuickPanelDialog 无关

**Grep 验证**:
```bash
grep -RIn "Gdiplus|Bitmap::FromStream|CreateStreamOnHGlobal" F:/soft/00selfmade/rime_claude/
```
唯一 active GDI+ 使用在 `WeaselUI/WeaselPanel.cpp` (candidate list) + `WeaselServer/WeaselServer.cpp:177-180` (GdiplusStartup/Shutdown,无 Bitmap)。

**结论**: 候选 B (QuickPanelDialog GDI+ 路径清理) **NOT APPLICABLE**,当前 QuickPanelDialog.cpp 已是 WIC + pure GDI + UpdateLayeredWindow (per-pixel alpha),**无可清理项**。

**关于灾难 #2 (默认进 Fluxing 无浮动栏) 的真因 — 待进一步排查**:
- Patch A+B + install.nsi C1/C2 fixes 灾难 #1 (install hang) 主体
- 灾难 #2 可能被灾难 #1 副作用引发:install hang → user force reboot → Fluxing TIP 部分注册 (KnownClasses 写了但 install 没跑完) → 切到 Fluxing 但 panel 显示路径断裂
- **不** 改 QuickPanelDialog.cpp 内部 (per handoff §7 严格禁止)

---

## 3. 跳过项 + 原因

| 跳过项 | 原因 |
|---|---|
| ❌ 候选 B (QuickPanelDialog GDI+ cleanup) | 已证明不适用 (当前 QuickPanelDialog 是 PURE GDI + WIC) |
| ❌ TDD red-green test 1-9 必写 | session-health <2h 预算紧,patch 改动已 minimal (3+2 行);真验靠 sandbox-verify 装机 4 flow + 装机 event log |
| ❌ sandbox-verify 自动跑 | `F:\soft\00selfmade\sandbox-verify\rime-claude\rime-verify.ps1 -InstallerPath <v0.67 installer>` 需要 user 触发 |
| ❌ CHANGELOG.md v0.67 段 | 本会话 session 时间紧;build 验证 + 装机 user flow PASS 后再补 |
| ❌ SFTP atomic upload + Cloudflare cache purge 通知 | build 验证后才做 |
| ❌ Disaster #2 单独 deep dive | Patch A+B + C1/C2 先 ship 验证灾难 #1 修复是否连带缓解灾难 #2;如不缓解,下次会话单独排查 |

---

## 4. Build 状态 ✅

- ✅ `_buildflow.cmd` (msbuild weasel.sln /t:Rebuild) **Exit 0**
  - WeaselServer.exe 重新链接:`F:\soft\00selfmade\rime_claude\output\Win32\WeaselServer.exe` (2.1 MB, 23:28)
  - WeaselDeployer.exe / WeaselSetup.exe / FluxingPhrasesDialog.exe 全部重新链接
- ✅ `_nsis_only.cmd` (NSIS makensis) **Exit 0**
  - **installer**: `F:\soft\00selfmade\rime_claude\output\archives\fluxing-0.19.0.67-installer.exe` (43.3 MB, 23:35)
  - 仅 warnings: PhrasesDialog.cpp C4002 macro (pre-existing,与 patch 无关)
- ✅ `git commit bbd0414` 已落 (2 files changed, 20 insertions, 8 deletions)
  - env.bat NOT committed (gitignored,符合 L10 + build-toolchain.md §Hard rules)
- ✅ Baseline `reg query` verification PASS (current system state,pre-v0.67 install):
  - `HKLM\SOFTWARE\Microsoft\CTF\KnownClasses` = `{A3F4CDED-...} = "Fluxing Text Service"` ✓
  - `HKCU\Software\Microsoft\CTF\Assemblies\0x00000804` = `0x00000804` branch 存在 ✓
  - `HKLM\SOFTWARE\Classes\CLSID\{A3F4CDED-...}\InprocServer32` = `weaselx64.dll, ThreadingModel=Apartment` ✓
- ❌ **未跑** (mandatory post-install per `.claude/rules/nsis-registry-writes.md` §Verification):
  - 在 **v0.67 装机后** 再跑同一组 `reg query` 验证 (sleep 3s + WriteRegStr KnownClasses 是否生效)
  - 缺任何 key → DO NOT ship → fix install.nsi nesting
- ❌ **未跑**: sandbox-verify 自动装机 user flow — user 必跑 (per CLAUDE.md)
- ❌ **未跑**: SFTP atomic upload + Cloudflare cache purge notify
- ❌ **未跑**: CHANGELOG.md v0.67 段补 (漏段但 v0.66 也没补,follow-up)
- ❌ **未跑**: git tag v0.19.0.67 (follow-up)
- ❌ **未跑**: TDD red-green test 1-9 (session-health <2h 跳过)

---

## 5. 装机端 expect verify (v0.19.0.67 ship 后)

| # | 操作 | 期望结果 |
|---|---|---|
| 1 | 装新 installer | **不卡死 Windows**,正常 30-60s 完成(Patch C1 减少 20s 等待) |
| 2 | 装机最后阶段 WriteRegStr KnownClasses | 不再卡 CTF lock (Patch C2 前置 Sleep) |
| 3 | 重启 | Windows 正常进入桌面,无 ctfmon crash dialog |
| 4 | 默认输入法 | 系统默认输入法是上一个(中文/英文),**不是 fluxing** (install 完成 → TIP 注册正确) |
| 5 | 手动切到 fluxing (Win+Space) | 浮动栏自动弹 (Patch A/B sink 注册健壮性保证) |
| 6 | Alt+, | toggle 显/隐 ✓ (跟灾难 #1/#2 不直接相关,如果 fix #1 后仍 fail,deep dive 灾难 #2) |

### 5.1 ⚠️ **MANDATORY post-install reg query** (per `.claude/rules/nsis-registry-writes.md` §Verification)

**装机 v0.67 完成后立即跑** (在 sandbox-verify 或装机端):

```powershell
reg query "HKLM\SOFTWARE\Microsoft\CTF\KnownClasses"
reg query "HKCU\Software\Microsoft\CTF\Assemblies\0x00000804"
reg query "HKLM\SOFTWARE\Classes\CLSID\{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}\InprocServer32"
```

**期望结果**:
- 第 1 条:看到 `{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}    REG_SZ    Fluxing Text Service`
- 第 2 条:看到 `0x00000804` 子树 + `Default = {A3F4CDED-...}` + `Profile = {A3F4CDED-...}`
- 第 3 条:看到 `(Default) = ...\weaselx64.dll` + `ThreadingModel = Apartment`

**任何 key 缺失 → DO NOT SFTP → fix install.nsi nesting** (按 L66 safety-net pattern §AP-L66-A 排查)。

Baseline (pre-v0.67 install, 当前 v0.66 残留状态): 3 个 key **全部存在** ✓
如 v0.67 install 后任一 key 缺失 → Patch C2 (Sleep 3000) 没起作用 → 进一步 debug installer 卡在哪段。

---

## 6. 沉淀 (lessons-learned.md 候选)

| L## | 教训 | 状态 |
|---|---|---|
| **L##-PhaseL-9-SinkRegister-GoldStandard** | TSF sink 注册/反注册必须严格抄 `_InitThreadMgrEventSink` (WeaselTSF.cpp:45) + `_UninitThreadMgrEventSink` (ThreadMgrEventSink.cpp:55) 的 TF_INVALID_COOKIE guard 模式。Copy-paste 任何 sink template 时缺这一行 → H1 失败路径累积导致 UnadviseSink(0xFFFFFFFF) corruption | **等 build PASS + 装机 verify PASS 后写** |
| **L##-PhaseL-10-SubagentReportVerify** | 多 subagent root cause 调查**必须**亲自 verify 关键 source claim,subagent 可能读到 stale source state 或 memory 中过时引用。本次"QuickPanelDialog GDI+ 路径"claim 与当前 QuickPanelDialog.cpp v3-rev3 spec 074 实际不符 (PURE GDI + WIC + per-pixel alpha) | **等 verify PASS 后写** |

---

## 7. 关联文件

| 文件 | 说明 |
|---|---|
| `handoff-after-phase-L-postship-freeze-v0.19.0.67-2026-07-26.md` | 上一阶段 handoff (v0.66 ship 灾难) |
| `memory/2026-07-26-v066-disaster-rootcause.md` | 3 subagent root cause 调查报告 (本会话已 verify Patch A/B + install.nsi 部分正确,QuickPanelDialog GDI+ claim 错误) |
| `WeaselTSF/WeaselTSF.cpp:240-258` | Patch A+B applied |
| `output/install.nsi:144-161` | Patch C1 PPL taskkill shorten |
| `output/install.nsi:738-741` | Patch C2 Sleep 3000ms before WriteRegStr KnownClasses |

---

## 8. Ship gate (强制 sequential)

```
[1] sandbox-verify 装机 v0.67 (user 跑)
   F:\soft\00selfmade\sandbox-verify\rime-claude\rime-verify.ps1 -InstallerPath F:\soft\00selfmade\rime_claude\output\archives\fluxing-0.19.0.67-installer.exe
   ↓
[2] **MANDATORY post-install reg query** (§5.1) — 任一 key 缺失 → STOP,fix NSIS nesting,不进下一步
   ↓
[3] 装机 user flow 5 项 verify (§5 row 1-6) — 全 PASS 才进下一步
   ↓
[4] CHANGELOG.md v0.67 段补
   ↓
[5] git tag v0.19.0.67 + SFTP atomic upload
   ↓
[6] Cloudflare cache purge 通知
```

**commit bbd0414 ≠ ship** — 源码已落 git local,但 SFTP gate 在 [2]+[3] PASS 之后。

## 9. Next session 第一句话

```
承接 handoff-after-phase-L-bugfix-4-ship-v0.19.0.67-2026-07-26.md。
Commit bbd0414, installer 在 output/archives/fluxing-0.19.0.67-installer.exe (43.3 MB)。
sandbox-verify + §5.1 post-install reg query + §5 装机 user flow 5 项 verify 全 PASS 后,
SFTP + Cloudflare cache purge 通知 + CHANGELOG.md v0.67 段补 + git tag v0.19.0.67。
如 sandbox-verify 装机 灾难 #2 仍 fail (Win+Space 不弹浮动栏): deep dive QuickPanelDialog 调用链
  (Show / EnableAlwaysShowMode / m_client.FocusIn / IPC),不在 QuickPanelDialog.cpp 内部 GDI+ 路径
  (已被 subagent report 误判,该路径早已清理)。
```

---

*End of Phase L Bugfix 4 handoff. v0.19.0.67 真修 patches 已 apply,build 在跑,等 verify。*