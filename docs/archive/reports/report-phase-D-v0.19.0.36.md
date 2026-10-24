# Phase D 报告 — v0.19.0.36 install.nsi 5 加强 L72-fix

> 任务: 修 `output/install.nsi` 加强 L72-fix,解决 v0.19.0.35 用户报
> "装机没改进" 问题。**只改 install.nsi**,不动 cpp/h/cmd。

## 1. Root cause (subagent verify 后)

v0.19.0.32 装机后,老 WeaselServer.exe 在 D:\... 跑着,持 mmap lock。
L13 fix 之前 turn 加 retry 3x taskkill, 但 autorun respawn 在 slow 机器
可发生 3+ 次(slow because of Windows Defender / SearchUI enumerating
HKLM Run key)。L72-fix (commit 9b3e082) 应用 Rename-then-File 兜底,
但 silent fail 概率仍存在 (Rename 也失败 → SetOverwrite try → IfErrors
→ skip → 旧 binary 留着)。

v0.19.0.35 用户仍报"没改进" — 说明 3x retry + L72-fix 不够,需要
**多层防御**。

## 2. 5 加强设计

| # | 加强 | 位置 | 等级 |
|---|---|---|---|
| A | .onInit taskkill 3x→5x retry + 老 binary 路径 force delete | L136-191 | HIGH |
| B | call_uninstaller $R1 空 guard | L308-315 | HIGH |
| C | L72-fix SetOverwrite try 失败路径加 Delete /REBOOTOK | L532-535, L561-563, L585-587 | MEDIUM |
| D | Section install 阶段 5x retry + force kill explorer.exe + 重启 | L390-417 | MEDIUM |
| E | Section install 阶段 .old.tmp final sweep | L600-609 | LOW |

## 3. 改动 file:line (git diff 验证)

commit `af13cbff` (`output/install.nsi` | +113 / -7):

- L136-191 (Reinforcement A): `${For} $R9 1 3` → `${For} $R9 1 5`, 加 escalating
  backoff (`Sleep 1000/2000/3000/5000`),后接 5 个 legacy path + 1 个
  reg-read path 的 `Delete /REBOOTOK` + DetailPrint
- L308-315 (Reinforcement B): `call_uninstaller:` 后加 `StrCmp $R1 "" skip_uninst_runner`
  guard,防 `$R1` 为空时走 malformed `"\WeaselServer.exe" /quit`
- L390-417 (Reinforcement D): Section 阶段 taskkill 3x→5x + backoff, 后接
  `ExecWait 'taskkill /F /IM explorer.exe /T'` + `Sleep 2000` + `Exec '"$WINDIR\explorer.exe"'`
- L532-535 (WeaselServer.exe L72-fix): `DetailPrint` 前加
  `Delete /REBOOTOK "$INSTDIR\WeaselServer.exe"` 兜底
- L561-563 (WeaselDeployer.exe L72-fix): 同款
- L585-587 (WeaselSetup.exe L72-fix): 同款
- L600-609 (Reinforcement E): 5 个 `Delete /REBOOTOK "$INSTDIR\*.old.tmp"`

## 4. Build + Verify

### NSIS build
```
=== NSIS build with FLUXING_VERSION=0.19.0 WEASEL_BUILD=36 ===
Command line defined: "FLUXING_VERSION=0.19.0"
Command line defined: "WEASEL_BUILD=36"
...
Output: "F:\soft\00selfmade\rime_claude\output\archives\fluxing-0.19.0.36-installer.exe"
Total size: 43175049 / 156006380 bytes (27.6%)
[NSIS] Exit: 0
```
Only 2 warnings, 均为 pre-existing (`data\*.gram` + `data\lua\*.txt` empty
globs — 与 Phase D 无关)。

### 三方 MD5 一致性

| Artifact | MD5 |
|---|---|
| `output/archives/fluxing-0.19.0.36-installer.exe` | `73df971d4c16190db16f5f1a94fa3d9c` |
| Source `output/Win32/WeaselServer.exe` | `389610fbfcf72bb1d6e8de2db12e1b78` |
| Extracted `_extract_v0.19.0.36/WeaselServer.exe` | `389610fbfcf72bb1d6e8de2db12e1b78` |
| Source `output/Win32/WeaselDeployer.exe` | `dc19449edc034a43312015e057250b1f` |
| Extracted `_extract_v0.19.0.36/WeaselDeployer.exe` | `dc19449edc034a43312015e057250b1f` |

**Source build == embedded in installer** (MD5 一致)。Installer NSIS 压缩
未破坏 binary。

### install.nsi 编码完整性
- BOM `EF BB BF`: present (line 1)
- CRLF 行尾: 838/838 lines, 0 LF-only
- size: 36922 bytes

### TestPhrasesDialog.exe 69/69 PASS
Phase D 不动 WeaselServer.cpp,Phrase 4 P0 fix (commit `fa196049`) 已 ship。
Phase D 仅 NSIS 改动,test PASS 状态继承 v0.19.0.35 (`69/69 PASS`)。

### 真 user flow (4 hotkey + Phrase 4 P0 visible)
需用户装机验证。Phase D 加强的 5 层防御对应原 root cause:
- Respawn race → A (5x retry 13s) + D (5x retry 13s)
- Locked file → C (Delete /REBOOTOK 兜底)
- Stale legacy path → E (5 个 path scan + REBOOTOK)
- Malformed uninst path → B ($R1 guard)

## 5. Commit

```
af13cbff fix(installer): v0.19.0.36 - 5 加强 L72-fix + 老 binary force delete (Phase D)
```

Conventional Commits + module prefix `installer` (AGENTS.md / constitution
P4 compliant)。 1 file changed, +113 / -7。

## 6. Lessons learned (建议沉淀)

- L## candidate: install.nsi L72-fix 单层不够,需要"retry + rename +
  try overwrite + REBOOTOK" 四层防御 (5 层 if include 老 binary path scan)
- L## candidate: NSIS `ExecWait '"$R0\path" ...'` 在 $R0 为空时会走
  malformed literal '"\path" ...' — 必须显式 `StrCmp` guard
- L## candidate: explorer.exe 可持 WeaselServer mmap via shell
  notification hook (这是 Phase D 才发现的,值得入 lessons-learned.md)