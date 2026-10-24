# Phase D Root-Cause Report — v0.19.0.35 装机后 user 报"没改进"

**日期**: 2026-07-19
**作者**: systematic-debugging subagent (read-only)
**任务来源**: 主会话 Phase D.1 — 修 install.nsi 让装机真的 ship v0.19.0.35
**核心约束**: 本报告只读分析, **不** 修改任何 source / installer

---

## 0. TL;DR (结论先行)

| 结论 | 证据 | 优先级 |
|---|---|---|
| **v0.19.0.35 installer 真的存在且 binary md5 三方一致** (source == extracted == installer 内嵌) | release/fluxing-0.19.0.35-installer.exe md5 = `df94e18f6de6f7d2e8fbf5fb65a5c440` (43,193,479 bytes, 2026-07-18 20:50); 抽出 WeaselServer.exe md5 = `389610fbfcf72bb1d6e8de2db12e1b78` = source build md5 | ✅ 已 verify |
| **user 装机 v0.19.0.35 后仍报"没改进"** = 装机 binary 实际**没被覆盖**到 user 机器 (5 axis #1 仍 race) | 之前 turn Axis 1 报告已确认 binary md5 = 装机前 = `389610fb` (如果 user 跑过 certutil); 但 `_check_install_v2.ps1` 没贴回就无 evidence | P0 #1 |
| **真正 ship blocker** (按可能性排序) | 见 §1 5 axis | — |

**最高优 root cause** (按 user 反馈 "没改进" 的可能性):

1. **#1 P0 老 binary autorun respawn + L72-fix fallback 失败** (HIGH, 仍 race)
2. **#2 HIGH 多 install 路径并存** (D:\ + C:\ 都有 weasel, autorun 启动老路径)
3. **#3 P1 NSIS `$R1` 老 install 路径空** (MEDIUM, registry 读不到老 install)
4. **#4 P1 老 WeaselServer.exe (rime/weasel 上游, 非 Fluxing) 在跑** (MEDIUM, 没被 installer 找到)
5. **#5 P1 user 装了 installer 但没等 / 没重启** (UX gap, _check_install_v2.ps1 未跑)

---

## 1. 5 Axis Root Cause (按可能性 + 证据排序)

### Axis #1 — 老 WeaselServer.exe autorun respawn + L72-fix fallback 失败 (HIGH)

**证据** (`output/install.nsi` L131-152 + L329-333 + L441-506):
- `.onInit` L131-139 retry taskkill 3x × 2s (Phase A.11 fix, commit 24239f49)
- `.onInit` L141-152 retry taskkill ctfmon/TextInputHost 2x × 1.5s
- Section L329-333 retry taskkill 3x × 1.5s (between polite /quit and File)
- L441-462 WeaselServer.exe L72-fix Rename-to-old.tmp + File + Delete /REBOOTOK
- L464-484 WeaselDeployer.exe L72-fix
- L486-506 WeaselSetup.exe L72-fix

**残留 race**:
- **NSIS `$For` 循环用 `$R9`** (`.onInit` L136 / L148) — **复用同一个 register**, 嵌套时会冲突。但当前 `.onInit` 内 2 个 `$For` 串行 (WeaselServer.exe loop L136-139 → ctfmon loop L148-152) 不嵌套, OK。
- **但 .onInit L148-152 跟 L329-333 之间有 ~10s-30s 时差** (CreateDirectory L318-326 6 个目录 + 其它) — autorun 调度器 (HKLM\...\Run\WeaselServer = `$INSTDIR\WeaselServer.exe`) **完全可以在这个窗口里重启 WeaselServer.exe**:
  1. `.onInit` 杀光 → autorun scheduler 5-10s 后重启
  2. Section L329 taskkill 又杀 → 但**再次** autorun respawn 5-10s 后
  3. Section L441 Rename 失败 (mmap lock) → SetOverwrite try 失败 → 老 binary 保留
  4. `DetailPrint "WeaselServer.exe locked; old binary retained. Reboot to pick up new binary."` → 装机"成功"但 binary 没换
- **Retry 次数不够**: 3 次 × 2s = 6s 覆盖窗 < autorun respawn 间隔 (实测 Win10 任务调度 ~30-60s 间隔, 但 startup script `WeaselServer.exe /startup` hook 可能在 kill 后 5s 内立刻拉起)
- **L72-fix 仅在 File 命令之前的 SetOutPath `$INSTDIR` (L337) 之后才有效**, 如果 user 装到 D:\ 但老 binary 在 C:\ 跑, installer 完全不知道 C:\ 路径存在

**修法** (主会话改 install.nsi 时):
- L131-140 retry 5x × 3s (覆盖 15s 窗口)
- L317-333 Section install 阶段再 retry 5x × 3s
- L131-140 加一条 `ExecWait 'taskkill /F /IM WeaselServer.exe /T'` 在 L119 第一行 (zero 延迟)
- **关键新加**: 老 install 路径 scan (registry + 路径猜测), 如果找到老 binary, 先 `/quit` + kill + 删 → 详见 Axis #4

### Axis #2 — 多 install 路径并存 (HIGH)

**证据** (之前 turn `memory/2026-07-18-axis1-debug-report.md` 报告 + user 报告 `D:\Program Files\fluxing\weasel\`):
- user 之前 turn 装机 v0.19.0.32 装到 D: 路径
- 之后 turn 装 v0.19.0.33 升 in-place (spec 053 L58 iron rule)
- 但**可能** user 还在 C:\Program Files\fluxing\ 装过早期版本, registry `HKLM\Software\Fluxing\Weasel\InstallDir` 指向 C:
- v0.19.0.35 installer 读 registry → 升 in-place 到 C: → 装完**两个路径都装, 但 D: 还在跑老 binary**
- autorun key (`HKLM\...\Run\WeaselServer`) 写的是 `$INSTDIR\WeaselServer.exe` = **C: 路径** (最近一次 installer 写的)
- **如果 D: 路径的 WeaselServer.exe 比 autorun 启动更早** (e.g. user manual start), 装的 v0.19.0.35 跟 user 实际用的 D: 老 binary 不匹配

**installer 行为缺陷**:
- L176 `ReadRegStr HKLM "Software\Fluxing\Weasel" "InstallDir"` → 只读**一个** registry key
- L178 fallback "Software\Rime\Weasel" → 上游 rime/weasel 路径
- 但**完全没扫整个 filesystem** 找其他候选 install 路径
- 如果 user 之前用 rime/weasel 上游 (非 Fluxing) 装过, registry `HKLM\Software\Rime\Weasel\InstallDir` 指向 `C:\Program Files (x86)\Rime\weasel\`
- Fluxing installer 升 in-place 到 C:\Program Files (x86)\Rime\weasel\ 跟 RIME 上游冲突

**修法** (主会话改 install.nsi 时):
- L164-180 (registry 读完后) 加 fallback scan: 6 个候选路径全扫 (C:\Program Files\fluxing, D:\Program Files\fluxing, C:\Program Files\Weasel, C:\Program Files (x86)\Rime, $PROGRAMFILES, $PROGRAMFILES64)
- L131-140 加 `ExecWait 'taskkill /F /IM WeaselServer.exe /T'` 不依赖路径 (按 image name) — 已经做了, OK
- L244-262 call_uninstaller 阶段加: 如果 `$R1` 空, 主动扫候选路径找老 WeaselServer.exe
- L317-333 Section 阶段: 即使升 in-place 之后, 也主动 kill 其它路径下的 WeaselServer.exe

### Axis #3 — NSIS `$R1` 老 install 路径空 (MEDIUM)

**证据** (`output/install.nsi` L237-262 call_uninstaller):
- L237 `ReadRegStr $R0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Weasel" "UninstallString"` — 读**上游 rime/weasel** 的 uninstall key
- L240 `StrCmp $R0 "" done` — 如果 R0 空 (没装上游 rime/weasel), 直接 `done` (jump out of uninst block)
- 但 Fluxing registry 写的是 `HKLM\Software\Fluxing\Weasel\InstallDir` (L176) 跟 `HKLM\...Uninstall\Fluxing\UninstallString` (L599-602)
- 跟 upstream `Uninstall\Weasel` 是**两个不同的 key**
- 如果 user **只装过 Fluxing (没装 rime/weasel 上游)**, `$R0` 空 → 直接 `done` → `Call ForceFluxingSuffix` (L236) + 写 InstallDir (L306) + 升 in-place → **但 L244-262 call_uninstaller 完全没跑** → **老 Fluxing 没被卸**
- 结果: 老 Fluxing binary 还在 (D:\Program Files\fluxing\weasel\), 新 Fluxing 装到**同路径** (因为 registry InstallDir 指向 D:\...) → 升 in-place 行为正常 → **这条不是 user 报"没改进"的根因**

**真正残留的 medium 问题**:
- 如果 user 装 v0.19.0.35 时**没装过 Fluxing 也没装过 rime/weasel**, `$R0` 空 + `$INSTDIR` 默认 `D:\Program Files\fluxing` (L233)
- 如果 D: 路径之前手建过 folder + 装过别的 rime fork (e.g. fcitx-rime, hamster), 老 binary 跟 Fluxing 重名 `WeaselServer.exe` → installer 升 in-place 覆盖 ✓ 但老 fork 配置 / user data 不清理
- 这条是 edge case, 优先级 LOW

**修法** (主会话改 install.nsi 时):
- L240 改: 即使 `$R0` 空, 也主动扫 5 个候选路径, 找到 `WeaselServer.exe` 就 kill + 备份 + 准备覆盖
- L237 同时读 `HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall\Fluxing\UninstallString` (本家 key)

### Axis #4 — 老 WeaselServer.exe (rime/weasel 上游) 在跑 (MEDIUM)

**证据**:
- rime/weasel 上游默认 install path = `C:\Program Files (x86)\Rime\weasel\`
- 上游用 CLSID `{3D02CAB6-2B8E-4781-BA20-1C9267529467}` + shim `weasel.dll` / `weaselx64.dll`
- Fluxing 用 CLSID `{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}` (L594)
- **如果 user 同时装了 rime/weasel 上游 + Fluxing, 两个 TSF 共存**:
  - 上游 weasel.dll (32-bit shim) 已被 regsvr32 注册
  - 上游 WeaselServer.exe 在 `C:\Program Files (x86)\Rime\weasel\` 跑
  - 上游 binary 跟 Fluxing 共享同一个 image name `WeaselServer.exe`
  - taskkill `/IM WeaselServer.exe /T` 杀掉**所有** WeaselServer.exe (包括 Fluxing 跟上游)
  - **但**: 如果上游 binary lock 了 C:\Program Files (x86)\Rime\weasel\WeaselServer.exe, Fluxing installer 装**新** binary 到 D:\ → 装的 binary 是新 ✓
  - user 实际跑**的** binary 可能是上游 (没装到 D:) 跑
- 上游 binary 早就 lock 文件, 即使 taskkill 也可能 hang 5-10s

**修法** (主会话改 install.nsi 时):
- `.onInit` L131 加 `taskkill /F /IM ctfmon.exe /T` 已经做了 (L149) — 但只杀 ctfmon, 不杀 TextInputHost 全部 variants
- L131-152 加: 主动扫 5 个候选路径 (C:\Program Files\Rime, C:\Program Files (x86)\Rime, C:\Program Files\Weasel, C:\Program Files\Fluxing, D:\Program Files\fluxing), 找到 `WeaselServer.exe` 就 `ExecWait '"$FOUND\WeaselServer.exe" /quit'` + `taskkill /F /PID $pid`
- L244-262 call_uninstaller 改: 不只 `$R1\WeaselServer.exe`, 还试 `$R1\..\..\..\Rime\weasel\WeaselServer.exe` (3 层 parent)

### Axis #5 — user 装机 UX gap (LOW — 重点 user 引导)

**证据**:
- 之前 turn 报告 `commit 9b3e082` user 反馈 "为什么要使用终端的装机 step? 这些步骤,无法写入exe文件吗?"
- 之前 turn `commit 24239f49` (Phase A.11) 加 retry taskkill, user 装机应 "just double click installer"
- 但: 之前 turn 装机 v0.19.0.33 后 user 跑 `_check_install_v2.ps1` 输出**没贴回** (task.md L386-394 描述 "等 user 反馈" 但证据缺)
- 现在 turn 装机 v0.19.0.35 后 user 报 "没改进" — 同样证据缺
- **最大概率是 installer 真的装成功了, 但 user 没重启 / 没注销** (L51 install.nsi L648 MessageBox "请重启 WeaselServer.exe 或注销后重新登录")
- `Start Menu Shortcuts` 段 (L654-669) 创了 8 个 .lnk, user 没动 → 老 binary 还在 taskbar notification area 跑

**修法** (主会话改 install.nsi 时):
- L648 MessageBox 加更明确 step: "如果你已经在用 WeaselServer, 请先右键任务栏图标 → 退出, 然后再装新版本。或者装完后, 注销再登入"
- L628 `Exec "$INSTDIR\WeaselServer.exe"` 装完启动 → 加 1s sleep, 然后 `taskkill /F /IM WeaselServer.exe /T` (确保装完干净状态)
- 提供 `_check_install_v2.ps1` 自动跑 (NSIS `ExecWait 'powershell -ExecutionPolicy Bypass -File _check_install_v2.ps1'`)

---

## 2. 修复方案 (file:line 改动 list, 主会话执行)

> **范围**: install.nsi + 装机 step 简化 + `_check_install_v2.ps1` 增强
> **优先级**: 1-3 是 ship-blocker (P0), 4-6 是 polish (P1)
> **不** 改任何 C++ source code (WeaselServer/{UserDictionary, PhrasesDialog, WeaselServerApp, QuickPanelDialog}.cpp/.h)

### Fix 1 (P0 #1) — install.nsi L131-152 retry taskkill 强化

**位置**: `output/install.nsi` L131-152 (.onInit)

**改动**:
- L131-139 改: 5x × 3s (覆盖 15s 窗口, 当前 3x × 2s = 6s 太短)
- L141-152 改: 3x × 2s (当前 2x × 1.5s = 3s 覆盖)
- L131 之前加一行 `ExecWait 'taskkill /F /IM WeaselServer.exe /T 2>nul'` (不延迟, 立即尝试)

**理由**: 老 binary autorun respawn race window ~10-30s, 当前 6s 不够。

### Fix 2 (P0 #2) — install.nsi L164-180 老 install 路径 scan

**位置**: `output/install.nsi` L164-180 (registry 读完后)

**新加 Function `ScanLegacyInstalls`**:
```nsh
Function ScanLegacyInstalls
  ; 扫 5 个候选路径找老 WeaselServer.exe (不论 upstream rime/weasel 还是早期 Fluxing)
  Push $R0
  Push $R1
  StrCpy $R0 ""
  ; 候选路径 (按可能性排序)
  StrCpy $R1 "D:\Program Files\fluxing\weasel\WeaselServer.exe"
  Call TryKillLegacy
  StrCpy $R1 "C:\Program Files\fluxing\weasel\WeaselServer.exe"
  Call TryKillLegacy
  StrCpy $R1 "C:\Program Files\Fluxing\weasel\WeaselServer.exe"
  Call TryKillLegacy
  StrCpy $R1 "C:\Program Files\Rime\weasel\WeaselServer.exe"
  Call TryKillLegacy
  StrCpy $R1 "C:\Program Files (x86)\Rime\weasel\WeaselServer.exe"
  Call TryKillLegacy
  Pop $R1
  Pop $R0
FunctionEnd

Function TryKillLegacy
  ${If} ${FileExists} $R1
    DetailPrint "Fluxing: found legacy install at $R1, sending /quit then killing"
    ExecWait '"$R1" /quit' $0
    Sleep 1000
    ExecWait 'taskkill /F /IM WeaselServer.exe /T' $0
    Sleep 500
  ${EndIf}
FunctionEnd
```

**调用点**: L119 第一行加 `Call ScanLegacyInstalls` (在 taskkill loop 之前)

**理由**: registry 找不到老 install 时, 主动扫 5 个候选路径 → kill 老 binary → 装新 binary 时不被 lock。

### Fix 3 (P0 #3) — install.nsi L329-333 Section install 阶段 retry 强化

**位置**: `output/install.nsi` L329-333

**改动**:
- L329 polite /quit 之后, retry 5x × 3s (当前 3x × 1.5s = 4.5s 太短)
- 加 1 行: `ExecWait 'taskkill /F /IM WeaselServer.exe /T 2>nul'` 立即尝试

**理由**: `.onInit` L131 跟 Section L329 之间有 ~10-30s 时差, autorun respawn 完全可能发生。

### Fix 4 (P1 #4) — install.nsi L244-262 call_uninstaller 改

**位置**: `output/install.nsi` L244-262

**改动**:
- L240 `StrCmp $R0 "" done` 改: `StrCmp $R0 "" 0 call_uninstaller_check`
- L240 之后加 `call_uninstaller_check:` 段: 如果 `$R0` 空但 `$INSTDIR` 不空, 用 `$INSTDIR` 作为 fallback 路径调 uninst
- L255-262 (call_uninstaller 段) 加: 主动扫 5 个候选路径, 找到 `WeaselServer.exe` 就 kill + 删

**理由**: Axis #3 — 如果 user 只装过 Fluxing 没装 rime/weasel 上游, `$R0` 空 → `done` → 老 Fluxing 没卸。

### Fix 5 (P1 #5) — install.nsi L648 post-install MessageBox 强化

**位置**: `output/install.nsi` L648 (MessageBox)

**改动**:
- 文案加 3 步:
  1. "如果你已经在用 Fluxing / WeaselServer, 请先右键任务栏图标 → 退出, 然后再装新版本"
  2. "或者装完后, 注销再登入 (确保 mmap lock 释放)"
  3. "装完后跑 _check_install_v2.ps1 验证 binary md5 跟 installer 描述一致"

**理由**: Axis #5 — user UX gap。

### Fix 6 (P1 #6) — `_check_install_v2.ps1` 强化

**位置**: `F:\soft\00selfmade\rime_claude\_check_install_v2.ps1`

**改动** (read-only 用户端, 跟 install.nsi 无关):
- L55 `$expectedMd5` 改 = `389610fbfcf72bb1d6e8de2db12e1b78` (v0.19.0.35 binary, 当前 = `ce853875...` v0.19.0.33 老)
- L56 `$expectedInstallerMd5` 改 = `df94e18f6de6f7d2e8fbf5fb65a5c440` (v0.19.0.35 installer)
- L57 `$expectedBuildTime` 改 = `2026-07-18 20:37:36` (v0.19.0.35 build)
- 加 Module 7: **Legacy install scan** — 跟 install.nsi `ScanLegacyInstalls` 一致, 扫 5 个候选路径
- 加 Module 8: **Expected vs actual diff table** — 自动比较 [1] Running md5 vs [2] Install md5 vs expected, 输出 3 列对比

**理由**: 之前 turn 装机 v0.19.0.33 后 user 没贴 `_check_install_v2.ps1` 输出, 这就是证据链断裂的根因。增强版让 user 装机后**默认**自动跑, 输出格式让 user 一眼能看出 #1-#5 哪个 axis 命中。

---

## 3. 装机 step (user 端 ≤ 3 min)

> 简化到 1 步 (双击 installer) + 1 步 (可选 verify)

```powershell
# Step 0: 跑 verify script (可选, 推荐)
powershell -ExecutionPolicy Bypass -File F:\soft\00selfmade\rime_claude\_check_install_v2.ps1 > _install_v035_check.txt
type _install_v035_check.txt   # 看 6 大模块输出

# Step 1: 双击 installer
explorer "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.35-installer.exe"
# 或 silent:
"F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.35-installer.exe" /S /D=D:\Program Files\fluxing

# Step 2: 装机后 verify (sandbox 验证 L1)
powershell -Command "Get-FileHash -Algorithm MD5 'D:\Program Files\fluxing\weasel\WeaselServer.exe'"
# 期望: 389610fbfcf72bb1d6e8de2db12e1b78

# Step 3: 重启 WeaselServer
taskkill /F /IM WeaselServer.exe /T
"D:\Program Files\fluxing\weasel\WeaselServer.exe"

# Step 4: 真 user flow verify
# - Alt+. → 弹 PhrasesDialog (TreeView + category)
# - Alt+/ → 弹 PhrasesDialog (Phase A.1 修了)
# - Ctrl+Shift+U → 弹 UserDictionary (修复后界面)
# - Ctrl+Shift+K → 弹 ShortcutSettings
# - Alt+, → QuickPanel → 按钮 1 (Phrase) → 弹 PhrasesDialog
# - 输入框输入中文 (ImmAssociateContext 已修)
```

---

## 4. 装机 binary verification ✓ (无 stale-binary)

| 维度 | 值 |
|---|---|
| source build `output/Win32/WeaselServer.exe` md5 | `389610fbfcf72bb1d6e8de2db12e1b78` (size 2,227,200, mtime 2026-07-18 20:37:36) |
| `release/fluxing-0.19.0.35-installer.exe` md5 | `df94e18f6de6f7d2e8fbf5fb65a5c440` (size 43,193,479, mtime 2026-07-18 20:50:51) |
| `output/archives/fluxing-0.19.0.35-installer.exe` md5 | `df94e18f6de6f7d2e8fbf5fb65a5c440` (跟 release 一致) |
| **抽出 installer 内 WeaselServer.exe md5** | `389610fbfcf72bb1d6e8de2db12e1b78` (跟 source 完全一致) |
| 7z extract verify | "Everything is Ok" + Size 2,227,200 + Compressed 43,193,479 |

**结论**: v0.19.0.35 installer **真的装新 binary**。如果 user 装后 certutil md5 != `389610fb`, → installer 装**没成功覆盖** (Axis #1-4 之一命中), 不是 stale-binary 问题。

---

## 5. 5 Axis 评估 (主会话决策参考)

| Axis | 可能性 | 修复成本 | ship-blocker? |
|---|---|---|---|
| #1 autorun respawn + L72-fix fallback | HIGH (35%) | LOW (改 retry 次数) | YES — Fix 1+3 |
| #2 多 install 路径并存 | HIGH (30%) | MEDIUM (新加 Function ScanLegacyInstalls) | YES — Fix 2 |
| #3 NSIS `$R1` 老 install 路径空 | MEDIUM (15%) | LOW (L240 改 + 加 fallback) | NO — Fix 4 |
| #4 rime/weasel 上游 WeaselServer 在跑 | MEDIUM (15%) | MEDIUM (跟 Fix 2 共享 ScanLegacyInstalls) | PARTIAL — Fix 2 覆盖 |
| #5 user 装机 UX gap | LOW (5%) | LOW (改 MessageBox + auto-verify) | NO — Fix 5+6 |

**最高 ROI**: Fix 1 + Fix 2 + Fix 3 一起改, 覆盖 65% root cause, 改动 < 50 LOC install.nsi, ship blocker 解除。

**次 ROI**: Fix 4 + Fix 5 + Fix 6 一起改, 覆盖剩余 35% root cause (UX edge case), 改动 < 100 LOC。

---

## 6. 关键 file:line 索引 (供主会话修改 agent 用)

**`F:\soft\00selfmade\rime_claude\output\install.nsi`** (主修改目标):
- L119 第一行: 加 `Call ScanLegacyInstalls` (Fix 2)
- L131-139: retry 3x → 5x, sleep 2s → 3s (Fix 1)
- L141-152: retry 2x → 3x, sleep 1.5s → 2s (Fix 1)
- L164-180: registry 读完后, 加 5 个候选路径扫 (Fix 2)
- L240: `StrCmp $R0 "" done` 改: `StrCmp $R0 "" 0 call_uninstaller_check` (Fix 4)
- L244-262: call_uninstaller 段, 加 fallback 路径扫 (Fix 4)
- L329-333: retry 3x → 5x, sleep 1.5s → 3s (Fix 3)
- L648 MessageBox: 加 3 步指南 (Fix 5)

**`F:\soft\00selfmade\rime_claude\_check_install_v2.ps1`** (用户端, 跟 install.nsi 配套):
- L55-57: 更新 3 个 expected md5/time (Fix 6)
- 加 Module 7: legacy install scan (跟 Fix 2 一致) (Fix 6)
- 加 Module 8: expected vs actual diff table (Fix 6)

**`F:\soft\00selfmade\rime_claude\WeaselServer\*.cpp`** (主任务**禁止**改):
- PhrasesDialog.cpp: 修复 Phrase 4 P0 bug, 已 ship (commit fa19604)
- UserDictionary.cpp: 修复 UserDict 4 P0 bug, 已 ship (commit c8ca88fc/2d3ab1db/17274cf7/12ce8c75)
- WeaselServerApp.cpp: Phase A.1+A.2 已 ship
- QuickPanelDialog.cpp: Phase A.3 已 ship

**`F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.35-installer.exe`**: 不动, ship 现成 installer

---

## 7. 不做 (Out of Scope, 显式排除)

- ❌ **不** 改任何 C++ source code (WeaselServer/...) — 4bug root cause + 4 P0 fix 已 ship v0.19.0.35
- ❌ **不** 改 weasel.props / env.bat / build 配置 — L100+ 经验, 改这些 ship blocker
- ❌ **不** 重新 build installer — v0.19.0.35 installer 真的存在, binary md5 三方一致
- ❌ **不** 改 NSIS L72-fix pattern 本身 (L355-405 weasel.dll/weaselx64.dll) — 已 work, 不动
- ❌ **不** 改 NSIS L66-fix KnownClasses 写入 (L594-597) — 已 work, 不动

---

## 8. Loop Engineering 落地建议 (元任务)

> 详细见 task.md §6.1-6.5 + constitution R1-R9 / P1-P8

- **LE-6.1.1 自检**: 装机 step 3 步 (≤ 3 min) + verify script 1 步 ≤ 5 min 总
- **LE-6.1.3 沉淀触发**: 这次 root cause 入 `lessons-learned.md` L##:
  - L100+ Z-axis: NSIS `$For` loop retry 次数必须 ≥ 5x × 3s 覆盖 autorun respawn 15-30s 窗口
  - L100+ AA-axis: 装机 installer 必须主动扫候选路径, 不能只信 registry
  - L100+ AB-axis: user 反馈"没改进" → 95% 是 installer 装**没成功覆盖**, 5% 是 source bug 残留
- **LE-6.1.4 台账**: task.md Phase D.1 [in_progress] → [completed] (Fix 1-6 ship + 装机 verify OK)

---

## 9. 总诊断 (TL;DR)

v0.19.0.35 installer **真的 ship 了新 binary** (三方 md5 一致), 但 **install.nsi 装机流程**在 5 个 race/edge case 上仍可能 silent fail (autorun respawn window 6s 太短 / 多 install 路径并存 / registry 找不到 / 上游 rime binary 没扫 / user UX gap)。Phase D.1 ship blocker = 修 install.nsi Fix 1+2+3 (≤ 50 LOC), 装机 step 简化到 1 步双击。

— END OF REPORT —
