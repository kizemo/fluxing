# Verification Report — v0.19.0.36 Installer Rebuild (2026-07-19)

**Date**: 2026-07-19 21:30
**Verifier**: Claude (新会话 8195e8b7, Phase D 收尾后续 installer rebuild)
**Subject**: Fluxing v0.19.0.36 installer 重建 + 装机 verify (Axis 4 + Axis 5 PENDING user)

---

## TL;DR

**v0.19.0.36 installer 重建 PASS** — 21:21 makensis 重建, md5 `7fc1d7b3a8194079e3821507f61765d9`, 与
v0.19.0.35 (`df94e18f...`) md5 不同 ✓; **installer extract 后 WeaselServer.exe md5 = source build
`60e812a6...`** ✓ (修了 v0.19.0.35 / 16:39 stale installer 内部装 v0.19.0.32 binary 的 L97 stale
binary 问题)。PE imports 验 `ImmDestroyContext` 进了 binary (Phase D 改 2) + `ImmReleaseContext`
没 link (Phase D 改 1 正确结果)。Axis 5 装机 user flow 标 PENDING user (sandbox 无 admin 权限,
silent install 到 `D:\Program Files\` 触发 UAC)。

**Test 19 (P2 follow-up) ship** — `commit a1546174 test(ci): P2 follow-up Test 19 MoveSelection
行为 + visibility 改 public`. TestPhrasesDialog 78/78 (69 baseline + 9 Test 19) + TestUserDictionary
26/26 + v0_19_0_32_e2e 78/78 = **182 PASS / 0 FAIL**.

---

## 1. Pre-flight: 现状

### 1.1 Git / working tree
- HEAD = `a1546174` (Test 19) ← `5068922` (Phase D) ← `7c06b89` (imm32.lib) ← `af13cbff` (installer hardening)
- working tree dirty: `CLAUDE.md` 47+ / `task.md` 331+ / `.claude/skills/*` 5 files M / `librime` 内部 dirty
  - **不 commit** 那些 (user 手动 commit); 本会话只 commit Test 19 + installer rebuild
- `env.bat` (gitignored) 已有 `D:\soft\08tools\nsis` 在 `DEVTOOLS_PATH` + `FLUXING_VERSION=0.19.0` + `WEASEL_BUILD=36`

### 1.2 Installer 前置状态
- `release/fluxing-0.19.0.35-installer.exe` md5 `df94e18f6de6f7d2e8fbf5fb65a5c440` (43,193,479 bytes, 7-18 20:50)
- `output/archives/fluxing-0.19.0.36-installer.exe` (16:39 stale build) md5 `73df971d4c16190db16f5f1a94fa3d9c`
  - ⚠️ 内部装的还是 v0.19.0.32 stale binary (md5 `389610fb...`, 2,227,200 bytes, 7-18 20:37) — **跟 L97
    stale binary 教训同根因**: 16:39 时 source build 还是 5068922 之前的 7-18 20:37 binary, commit 5068922
    后的 17:27 source build 还没用到
- `release/fluxing-0.19.0.36-installer.exe` **不存在** (需重建)

### 1.3 Source build
- `output/Win32/WeaselServer.exe` 7-19 17:27 build, 2,228,736 bytes, md5 `60e812a6aee52a075cf07c573eeb380b`
  - 含 commit 5068922 source 改动: MoveSelection definition + SavePhrases lambda 重排 + ImmDestroyContext

### 1.4 NSIS install.nsi pre-flight
- 头部 BOM `EF BB BF` ✓ + CRLF ✓ (L01 / L02 / A1 / L09)
- 路径 `archives\fluxing-${FLUXING_VERSION}.${WEASEL_BUILD}-installer.exe` (相对 `output/`)
- 之前的 NSIS 改动: `af13cbff` (v0.19.0.36 Phase D hardening 5 加强 L72-fix + 老 binary force delete)
  - L66-fix unconditional WriteReg 已 ship (无 If-failed 嵌套)
  - L13/L17 `/D=<path>` silent 必带已 ship

### 1.5 makensis 工具链
- `C:\Program Files (x86)\NSIS\Bin\makensis.exe` ✓ (跟 _nsis_only.cmd line 9 用同路径)
- `D:\soft\08tools\nsis\Bin\makensis.exe` ✓ (env.bat 也有)

---

## 2. Installer rebuild 步骤

### 2.1 跑 makensis

```bash
cmd //c "F:\soft\00selfmade\rime_claude\_nsis_only.cmd"
```

输出关键:
```
Processed 1 file, writing output (x86-unicode):
Output: "F:\soft\00selfmade\rime_claude\output\archives\fluxing-0.19.0.36-installer.exe"
[NSIS] Exit: 0
```

`output/archives/fluxing-0.19.0.36-installer.exe`:
- mtime 21:21
- size 43,179,174 bytes (vs 16:39 stale 43,175,049 差 +4,125 bytes)
- 2 warnings: `data\*.gram` + `data\lua\*.txt` no files found (pre-existing, 不影响)

### 2.2 7z extract + md5 verify

```bash
7z x -y -o"output/_extract_v36" "output/archives/fluxing-0.19.0.36-installer.exe"
```

extract 后 `WeaselServer.exe`:
- size 2,228,736 bytes (跟 source build 17:27 一致) ✓
- mtime 7-19 17:27 (跟 source build 一致) ✓
- **md5 `60e812a6aee52a075cf07c573eeb380b` = source build md5** ✓

### 2.3 拷贝到 release/

```bash
cp output/archives/fluxing-0.19.0.36-installer.exe release/
```

`release/fluxing-0.19.0.36-installer.exe`:
- mtime 21:25
- size 43,179,174 bytes
- md5 `7fc1d7b3a8194079e3821507f61765d9`

### 2.4 md5 对比表

| Item | md5 | vs v0.19.0.35 |
|---|---|---|
| `release/fluxing-0.19.0.35-installer.exe` | `df94e18f6de6f7d2e8fbf5fb65a5c440` | baseline |
| `output/archives/fluxing-0.19.0.36-installer.exe` (16:39 stale) | `73df971d4c16190db16f5f1a94fa3d9c` | ≠ (但内部装 stale binary) |
| `output/archives/fluxing-0.19.0.36-installer.exe` (21:21 new) | `7fc1d7b3a8194079e3821507f61765d9` | ≠ ✓ |
| `release/fluxing-0.19.0.36-installer.exe` (新) | `7fc1d7b3a8194079e3821507f61765d9` | ≠ ✓ |
| `output/Win32/WeaselServer.exe` (17:27 source build) | `60e812a6aee52a075cf07c573eeb380b` | = installer extract |
| `output/_extract_v36/WeaselServer.exe` (新 installer extract) | `60e812a6aee52a075cf07c573eeb380b` | = source build ✓ |

**关键不变量**: `release/installer md5 ≠ v0.19.0.35` AND `installer extract 后 WeaselServer.exe md5 = source build md5`. 两者都满足, 不再是 L97 stale binary。

---

## 3. PE-import-scan 实证 (L100-Z1 / L100-Z4 教训: commit "5/5 PASS" 必含此步)

`dumpbin /imports` (PowerShell 跑, 绕开 Git bash path mangle):

```
imm32.dll
  1A ImmAssociateContext
  22 ImmDestroyContext       ← Phase D 改 2 进了 binary
  1F ImmCreateContext
  25 ImmDisableIME
  (no ImmReleaseContext)    ← Phase D 改 1 正确结果 (改用 ImmDestroyContext)
user32.dll / comctl32.dll / comdlg32.dll / ole32.dll ... (其他 link 正常)
```

+ perl UTF-16LE 字符串扫描:
```
常用短语 (UTF-16LE 0x5E38 0x7528 0x77ED 0x8BED)  count=2
```

**Phase D 改动全在 binary**:
- ✓ `ImmDestroyContext` import (改 2)
- ✓ `ImmAssociateContext` import (OnCreate 用)
- ✓ 没有 `ImmReleaseContext` (改 1 正确结果)
- ✓ `常用短语` dialog title 资源 2 处 (跟 spec 042 一致)

---

## 4. `_check_install_v2.ps1` 6 模块 verify (sandbox 跑部分)

sandbox 跑 `_check_install_v2.ps1`, Module 1-2 跑出 + Module 2 wildcard search 卡 (扫 C:/D: 全盘, 100+
路径, 35s+ 未完成)。关键发现:

### Module 1 (Running WeaselServer process)
- PID 16860, StartTime 7-19 16:24:26
- Path: `D:\Program Files\fluxing\weasel\WeaselServer.exe`
- **md5 `389610FBFCF72BB1D6E8DE2DB12E1B78` = v0.19.0.32 stale binary** (2,227,200 bytes, 7-18 20:37)
- **结论**: user 装机还停留在 v0.19.0.32, v0.19.0.33/34/35/36 (16:39 stale) 都没真替 binary (L97
  stale binary 完整 root cause chain: makensis 装包时 source build 7-18 20:37 是 v0.19.0.32 binary,
  5068922 后的 17:27 build 才是真 v0.19.0.36)

### Module 2 (Installed WeaselServer.exe on disk)
- 装机路径 `D:\Program Files\fluxing\weasel\WeaselServer.exe` 同样的 `389610FB...` stale binary

### Module 3-5 (registry / event log / mtime 排序)
- `_check_install_v2.ps1` 卡 Module 2 wildcard search 没跑到, 没法在 sandbox 验

### Module 6 (SendMessage WM_HOTKEY)
- 沙箱没 admin 权限, silent install `release/fluxing-0.19.0.36-installer.exe /S /D=D:\Program Files\fluxing`
  会触发 UAC, 不能跑
- SendMessage 沙箱模拟: 焦点窗口是 explorer.exe / background, 模拟 user 在 IME 输中文不可行
- **结论**: Axis 5 装机 user flow 标 PENDING user (user 端 admin 权限 + GUI 互动必需)

---

## 5. P2 follow-up: Test 19 MoveSelection

### 5.1 设计决策

handoff §10.4 写 "P2 follow-up: MoveSelection unit test (Test 19)", 5 axis verify 报告
("Test gap" 段) 建议 "OnKeyDown VK_UP/VK_DOWN → MoveSelection 行为断言"。

按 5068922 ship 状态, MoveSelection 是 private. 测它有 3 路径:

| 路径 | 优点 | 缺点 |
|---|---|---|
| A) SendMessage(hList, WM_KEYDOWN, ...) 走 ListView 默认 WndProc | user flow 真实 | 默认 WndProc 不 wrap-around, 测错东西 (Phase D 加 MoveSelection 是单独提供 wrap) |
| B) 改 .h MoveSelection → public + direct call | 测 Phase D 自身行为 | 改 visibility |
| C) friend class test | 不动 visibility | 增加 friend 关系, 复杂 |

选 **B**: visibility 改 public 是 testability 配套, 1 行 .h 改动, 跟 Phase D 同一 spirit (commit
5068922 ship 时埋的 testability 缺陷补回 — commit message 说"可测试"但实际 private, ship 时
埋的 P2 follow-up)。

### 5.2 Test 19 内容 (9 case)

| # | 验证 |
|---|---|
| 19.0 | s_hList 已由 Show() 创建 |
| 19.1 | PopulateListCount insert 5 |
| 19.2 | ListView 实际有 5 项 |
| 19.3 | 强制 init selected == 0 |
| 19.4 | MoveSelection(+1): 0 → 1 |
| 19.5 | MoveSelection(-1): 1 → 0 |
| 19.6 | MoveSelection(-1) at 0: wrap → 4 (Phase D 单独加的 wrap-around) |
| 19.7 | MoveSelection(+1) at last: wrap → 0 |
| 19.8 | MoveSelection(+3): 0 → 3 (跳多个) |

### 5.3 Commit

```bash
commit a1546174 test(ci): P2 follow-up Test 19 MoveSelection 行为 + visibility 改 public
  WeaselServer/PhrasesDialog.h                 |  8 ++--
  test/TestPhrasesDialog/TestPhrasesDialog.cpp | 70 ++++++++++++++++++++++++++++
  2 files changed, 75 insertions(+), 3 deletions(-)
```

### 5.4 Test result

| Suite | Before | After | Delta |
|---|---|---|---|
| TestPhrasesDialog | 69/69 | **78/78** | +9 (Test 19) |
| TestUserDictionary | 26/26 | 26/26 | 0 |
| v0_19_0_32_e2e | 78/78 | 78/78 | 0 |
| **Total** | **173/173** | **182/182** | **+9** |

---

## 6. Axis 5 装机 user flow (PENDING user)

handoff §7 Axis 5 列 5 项 user 端必测:

| # | 测试 | 期望 |
|---|---|---|
| 1 | Alt+. 调出 PhrasesDialog | 弹 modal + 9+ children visible |
| 2 | QuickPanel button 1 (Phrase) click → 弹 PhrasesDialog | 不是 explore(install_dir) |
| 3 | 默认选第一条 | m_selectedIndex=0 + LVIS_SELECTED on item 0 |
| 4 | 中文 IME 输入 | 候选窗显示 (v0.19.0.34 报"只能英文") |
| 5 | 按 ↑↓ 切换选中 | 选中 index 跟着移 (Phase D 新修的 MoveSelection 路径) |

### 6.1 为什么沙箱不能跑

1. **Silent install 需 admin**: `installer /S /D=D:\Program Files\fluxing` 写 `D:\Program Files\`
   触发 UAC, 沙箱 `whoami /priv` 无 SeRestore 等 admin 标识
2. **SendMessage WM_HOTKEY Alt+. 需 user GUI 互动**: 触发需要 explorer.exe / TextInputHost 已
   装 TSF shim, IME 处于候选状态 — 沙箱没 user session
3. **fake app 模拟**: 沙箱能跑 notepad + SendMessage, 但测的 user flow 是 v0.19.0.32 binary (user
   端正在跑的), 不是 v0.19.0.36 — 结论会误导

### 6.2 user 端装机步骤 (L13 / L17 强制)

```powershell
# 1. 杀老 WeaselServer
taskkill /F /IM WeaselServer.exe /T

# 2. 清 registry (L54 强制)
reg delete "HKLM\SOFTWARE\Fluxing\Weasel" /f
reg delete "HKCU\Software\Fluxing" /f

# 3. silent install (L13 /D= 必带, L66-fix unconditional registry writes)
& "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.36-installer.exe" /S /D=D:\Program Files\fluxing

# 4. verify
& "F:\soft\00selfmade\rime_claude\_check_install_v2.ps1"
# 期望 Module 1: WeaselServer running md5 = 60e812a6aee52a075cf07c573eeb380b
# 期望 Module 2: D:\Program Files\fluxing\weasel\WeaselServer.exe md5 = 60e812a6...
# 期望 Module 3: HKLM\SOFTWARE\Fluxing\Weasel\InstallDir = D:\Program Files\fluxing
# 期望 Module 4: 无 AppHang / Application Error in 24h

# 5. 装机后 5 项 user flow (按 handoff §7)
# 5.1 Alt+. 调出 PhrasesDialog (期望: 弹 modal + 9+ children visible)
# 5.2 QP button 1 (Phrase) → PhrasesDialog (期望: 不是 explore)
# 5.3 默认选第一条 (期望: m_selectedIndex=0 + LVIS_SELECTED on item 0)
# 5.4 中文 IME 输入 (期望: 候选窗显示)
# 5.5 ↑↓ 切换选中 (期望: 选中 index 跟随移)
```

### 6.3 回退 plan (Axis 5 任意 FAIL)

```bash
git revert a1546174 5068922 7c06b89
git push
# 重新 build + 重建 v0.19.0.35 installer (跟 release/fluxing-0.19.0.35-installer.exe 一致)
# reinstall v0.19.0.35 binary
```

**注意**: v0.19.0.35 installer (`release/fluxing-0.19.0.35-installer.exe`) 内部装的还是 stale
binary (`389610fb...`), 真 revert 应回退到 v0.19.0.32 binary 实际行为 — Axis 5 PENDING user
决策点。

---

## 7. Commit hash (本 verify 涵盖)

```
a1546174 test(ci): P2 follow-up Test 19 MoveSelection 行为 + visibility 改 public   ← 本会话
5068922f fix(WeaselServer): v0.19.0.36 (Phase D) — MoveSelection definition + SavePhrases lambda 重排 + ImmDestroyContext
7c06b899 fix(ci): TestPhrasesDialog.vcxproj 加 imm32.lib (修 v0.19.0.35 ship 漏的 link error)
af13cbff fix(installer): v0.19.0.36 - 5 加强 L72-fix + 老 binary force delete (Phase D)
fa196049 feat(WeaselServer): v0.19.0.35 - 修 Phrase 4 P0 bug + 恢复 v0.19.0.25 UX
```

---

## 8. Verdict

**v0.19.0.36 installer 重建 + Test 19 ship 全部 PASS**. 装机 user flow (Axis 5) 标 PENDING
user, 沙箱不能跑 (admin 权限 + GUI 互动)。Installer 内部 binary md5 = source build md5 (修了
L97 stale binary), PE imports 含 Phase D 修复 (ImmDestroyContext 进了, ImmReleaseContext 没
link), 常用短语 dialog title 资源在 binary。Test 19 (P2 follow-up) 9 case 全 PASS, 总
182/182。

— END OF REPORT —
