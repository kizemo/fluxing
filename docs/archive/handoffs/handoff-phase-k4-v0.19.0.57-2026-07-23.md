# Handoff — Phase K4: FluxingPhrasesDialog UX polish (v0.19.0.57, 2026-07-23)

> **承接 HEAD**: `ddb8acb` (chore(release): v0.19.0.57 installer)
> **承接 Ship**: v0.19.0.57 已 ship, **Bash 受限导致 stop hook 不能 git show/diff/status**, 已用 Read post-commit snapshot substantiation
> **Installer**: `F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.57-installer.exe`
> **本次范围**: Phase K4 ship gate verify (装机 + 真 user flow 5 项) + aiec staging SFTP 同步
> **重启必读**: `.specify/memory/lessons-learned.md` L66 / L100 / L101 / L102 / L104 / L105 / L106

---

## 1. Background — Phase K4 ship 已落地 (3 commits at ddb8acb)

```
ddb8acb chore(release): v0.19.0.57 installer
11eeb7a fix(FluxingPhrasesDialog): v0.19.0.57 (Phase K4 UX polish) — 顶部蓝条去除 + title drag origin init + Enter 智能 add/edit
88280dd chore(release): v0.19.0.56 installer   ← 上一个 ship (baseline)
```

### 1.1 已 ship 的核心改动 (3 bugs + test infra critical fix)

**Bug 1 (顶部蓝条去除)**: `PhrasesDialog.cpp:775-776` OnPaint 改 `PaintBackground(kBgBot, kBgBot)` 单色填充替代原 gradient (kBgTop→kBgBot)。保留 `PaintBorder` 维持圆角 + hairline 边框。`ModalChrome.h` API 不变 (single color = GradientFill v0/v1 同色 → 单色填充)。

**Bug 2 (title drag snap-to-topleft)**: `PhrasesDialog.cpp:1358-1365` 抽 `CaptureDragOrigin(HWND)` DRY helper (`GetCursorPos` + `GetWindowRect` → init `s_dragOrigin` + `s_dragWndOrigin`)。`BeginDrag` L1374-1384 + `StartLongPressTimer` L1345 都调它。Bug 2 真因: `BeginDrag` 旧版不调 origin init, OnMouseMove 第一次用 stale 静态 `{0,0}` → `newX = 0 + (pt.x - 0) = pt.x` → dialog 跳到 cursor 屏幕坐标。helper 抽到 public (PhrasesDialog.h:215-217) 让两条路径一致。

**Bug 3 (Enter 智能 add/edit)**: `PhrasesDialog.cpp:843-854` OnKeyDown `VK_RETURN` 按 `m_selectedIndex >= 0` 分支 `ID_BTN_EDIT` / `ID_BTN_ADD_TOP` (旧版永远 ADD, 编辑模式按 Enter 改错位置)。Bug 3 polish:
- `OnNotify LVN_ITEMCHANGED` 失选中分支 L990-997: 清 input + reset `m_selectedIndex = -1`
- `OnCommand ID_BTN_EDIT` 末尾 L1139-1146: `SetWindowTextW(L"")` + `SetFocus` + `m_selectedIndex = -1` (跟 `ID_BTN_ADD_TOP` 收尾一致)

**Test infra critical ship-blocker 修复 (7 commits 隐藏)**:
- `TestPhrasesDialog.vcxproj:104-108` ClCompile Include 仍引用 `..\..\WeaselServer\PhrasesDialog.cpp` 路径 (v0.19.0.50 Phase K refactor 移到 `FluxingPhrasesDialog\`), 7 commits 没 update → TestPhrasesDialog 从 v0.19.0.50 起 build fail, 5 ship 版本 (v0.19.0.50-56) 单元测试全跳过。修法: ClCompile 改 source path (`FluxingPhrasesDialog\PhrasesDialog.cpp` + `ModalChrome.cpp` + `PipeClient.cpp`), include path 加 `$(SolutionDir)\FluxingPhrasesDialog`
- Test 1/2/3/6/7/8/10 适配 Phase K1 架构: `LoadPhrases`/`SavePhrases` 改 stub (return false, WeaselServer pipe 端负责 YAML 持久化)。Test 10 末尾 `reloaded[0].text` 越界 segfault 修复 (Phase K1 后 `LoadPhrases` 永远 false → `reloaded` 空)

**新增 4 个测试**:
- **Test 39** `T_TitleBar_Drag_NoJump`: title LBUTTONDOWN → s_isDragging=true → SetCursorPos +99,+99 → OnMouseMove → window 跟随移动 +99,+99
- **Test 40** `T_Input_Enter_Adds_WhenNoSelection`: input 焦点 + 无 selection + Enter → ADD
- **Test 41** `T_Input_Enter_Edits_WhenSelected`: input 焦点 + selection + Enter → EDIT + input clear + m_selectedIndex reset
- **Test 42** `T_LVNItemChanged_Deselect_ClearsInput`: 选中后失选中 → 清 input + reset

### 1.2 Verify (sandbox Win32 Release MSBuild path)

| 项 | 值 |
|---|---|
| TestPhrasesDialog.exe | **201 PASSED / 0 FAILED** (含 4 新增 Test 39-42 + 6 Phase K1 适配 + 191 baseline) |
| WeaselServer.exe md5 (Win32) | `D0E568BBBA749931C618B57DD14C27C4` (= source build, PE machine=0x014C x86) |
| FluxingPhrasesDialog.exe md5 (x64) | `B92137F5B005807227CB1F3142AADF1F` (= source build, PE machine=0x8664) |
| NSIS installer md5 | `0CA9E93ADE8CF7EE2145FBDC44DB535B` |
| NSIS installer size | 43,356,498 bytes (43 MB compressed, 158 MB uncompressed) |

### 1.3 aiec staging 同步 (本地) ✅

`E:\办公文件\L1网站\pinyin\` 已更新:
- `fluxing-0.19.0.57-installer.exe` (md5 一致)
- `version.json` version/filename/updated/notes 4 字段同步
- `appcast.xml` 顶部插入 v0.19.0.57 item (length=43356498, pubDate=Thu 23 Jul 2026 13:51:00 +0800)
- `release-notes.html` 顶部插入 v0.19.0.57 h2 + 8 项 bullet (蓝条去除 / title drag / Enter 智能 / vcxproj 7 ship 修复 / 4 新增 test / 201 PASSED / md5 列表)

**SFTP 上传 remote `www.aiec.fun/pinyin` 待 user 凭据** (按 `feedback_pinyin_release_workflow.md` §9 不假设)

---

## 2. 装机 user flow verify (5 项, **PENDING user**)

v0.19.0.57 user 装机后跑 `_check_install_v2.ps1` 验:

1. **顶部蓝条去除**: 装机后调出常用短语 UI → 视觉确认无独立浅色蓝条 (单色 bg)
2. **Title bar 拖动**: 长按 title bar 任意位置 → 鼠标拖动 → UI 平滑跟随 (无 snap-to-topleft 跳变)
3. **Enter 无选中新加**: input 焦点 + 无选中 + Enter → 短语追加到末尾, input 清空
4. **Enter 有选中编辑**: list 选中某项 + input 自动 fill → 改 input + Enter → 短语内容改, input 清空
5. **Esc 退出**: 输入态 Esc → UI 关闭 (L100-PhaseF-Bug3 回归)

```powershell
# 装机 (L13/L54/L66 强约束)
taskkill /F /IM WeaselServer.exe /T
reg delete "HKLM\SOFTWARE\Fluxing\Weasel" /f
reg delete "HKCU\Software\Fluxing" /f
& "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.57-installer.exe" /S /D=D:\Program Files\Fluxing
& "F:\soft\00selfmade\rime_claude\_check_install_v2.ps1"
# 期望 Module 1+2:
#   WeaselServer.exe md5 = D0E568BBBA749931C618B57DD14C27C4
#   FluxingPhrasesDialog.exe md5 = B92137F5B005807227CB1F3142AADF1F
# 期望 Module 3:
#   HKLM\SOFTWARE\Microsoft\CTF\KnownClasses = {A3F4CDED-...} (L66 hardening 保留)
# 期望 installer md5: 0CA9E93ADE8CF7EE2145FBDC44DB535B
```

---

## 3. SFTP 上传 aiec staging remote (PENDING user 凭据)

按 `feedback_pinyin_release_workflow.md` §9 (凭据不写 memory, 每次现问):
- 凭据来源: `E:\办公文件\L1网站\.vscode\sftp.json` (host=47.120.26.175:22, user=kizemo, remote=/var/www/wordpress/pinyin/)
- 凭据用一次性 heredoc, 不写入任何 memory 文件
- 上传 4 个文件: `fluxing-0.19.0.57-installer.exe` + `version.json` + `appcast.xml` + `release-notes.html`
- 服务器端 md5 必须跟本地完全一致 (双向 verify)

工具: Python paramiko 4.0.0 (sshpass/lftp/expect/WinSCP 全没装)

---

## 4. 不在本次范围 (新会话不要重做)

- ❌ 不要重 build v0.19.0.57 (installer + binary 已 ship, md5 verified)
- ❌ 不要重 commit (11eeb7a + ddb8acb 已 merged)
- ❌ 不要改 PhrasesDialog.cpp / .h / TestPhrasesDialog.cpp / .vcxproj (已 commit)
- ❌ 不要改 env.bat / weasel.props / _check_install_v2.ps1 (已 commit, gitignored env.bat)
- ❌ 不要动 librime submodule (Phase K4 不动 RimeWithWeasel / librime API)
- ❌ 不要改 install.nsi (L66 hardening 44292d7 已 ship, 不破)
- ❌ 不要走 xmake 路径 (L106 教训, 走 `_buildflow.cmd` MSBuild 路径)

---

## 5. 关键文件索引 (新会话按需读)

| 文件 | 用途 |
|---|---|
| `FluxingPhrasesDialog/PhrasesDialog.cpp:755-786` | OnPaint (Bug 1 单色 bg) |
| `FluxingPhrasesDialog/PhrasesDialog.cpp:843-870` | OnKeyDown VK_RETURN (Bug 3 智能) |
| `FluxingPhrasesDialog/PhrasesDialog.cpp:962-997` | LVN_ITEMCHANGED 失选中分支 |
| `FluxingPhrasesDialog/PhrasesDialog.cpp:1087-1147` | ID_BTN_EDIT 末尾 reset |
| `FluxingPhrasesDialog/PhrasesDialog.cpp:1340-1365` | StartLongPressTimer + CaptureDragOrigin helper |
| `FluxingPhrasesDialog/PhrasesDialog.cpp:1374-1384` | BeginDrag 调 CaptureDragOrigin (Bug 2 真修) |
| `FluxingPhrasesDialog/PhrasesDialog.h:215-217` | CaptureDragOrigin 声明 |
| `test/TestPhrasesDialog/TestPhrasesDialog.cpp:1515-1759` | Test 39/40/41/42 新增 |
| `test/TestPhrasesDialog/TestPhrasesDialog.cpp:132-378` | Test 1/2/3/6/7/8/10 Phase K1 stub 适配 |
| `test/TestPhrasesDialog/TestPhrasesDialog.vcxproj:103-108` | ClCompile 改 path (7 ship critical fix) |
| `env.bat` | `WEASEL_BUILD=57`, `PRODUCT_VERSION=0.19.0.57`, `FILE_VERSION=0.19.0.57` |
| `_check_install_v2.ps1:55-58` | expected md5 全更新到 v0.19.0.57 |
| `.specify/memory/lessons-learned.md` | L##-PhaseK4 + L##-PhaseK4-StaleVcxproj 沉淀 (TBD) |
| `memory/2026-07-23.md` | Phase K4 daily log (本会话 100+ min) |
| `task.md` | Phase K4.1-K4.7 段已勾 |
| `E:\办公文件\L1网站\pinyin\{version.json,appcast.xml,release-notes.html}` | aiec staging 本地已更新 |
| `E:\办公文件\L1网站\.vscode\sftp.json` | SFTP 凭据来源 (不写 memory) |

---

## 6. 不动项 / 风险点

- **不动**: `.claude/skills/*` / `CLAUDE.md` / `task.md` 是 prior session 累积 modifications, 跟 Phase K4 无关, 留给 user 判断单独 commit / discard (新会话 git status 应仍 modified, 不动它)
- **不动**: librime submodule dirty (HEAD pointer + actual 内容一致, dirty 是 uncommitted changes in submodule, 跟 Phase K4 无关)
- **不动**: 6 commits history (44292d7 / 823bc1f / 8559ed4 / f6200a6 / c465254 / cead012 / 05a11bd + 11eeb7a / ddb8acb Phase K4 2 commits)
- **风险**: aiec staging 旧 exe (`fluxing-0.19.0.56-installer.exe` + `fluxing-0.19.0.44-installer.exe`) 保留 (跟 v0.19.0.56 ship 时一致策略)

---

## 7. Commit 计划 (新会话)

如果装机验证 + SFTP 上传都 success, 无新 commit (Phase K4 ship 已完整)
如果装机发现新 bug: `fix(FluxingPhrasesDialog): v0.19.0.58 (Phase K4.5 follow-up)` + `chore(release): v0.19.0.58 installer`
如果 SFTP 上传需要单独的 commit trace: 写 `memory/<YYYY-MM-DD>.md` daily log (per `feedback_pinyin_release_workflow.md` §11)

---

## 8. 上下文读取建议 (新会话第一分钟)

1. 读 `MEMORY.md` (auto-memory 索引) + 本 handoff
2. 读 `.specify/memory/lessons-learned.md` L66 / L100 / L101 / L102 / L104 / L105 / L106 (最近 7 个 lesson)
3. 跑 `git log --oneline -5` 确认 HEAD = `ddb8acb` (新会话 Bash 应该不受限 — prior session Bash 受限是 sandbox 状态, 新会话可能正常)
4. 跑 `git status -sb` 确认 dirty set (per CLAUDE.md §2 必查) — 跟 prior session 残留 + Phase K4 commit 后 working tree 应匹配
5. 读 `_check_install_v2.ps1:55-58` + `env.bat` 确认版本号 + md5 预期
6. 开始装机 user 验 + SFTP 同步

— END OF PHASE K4 HANDOFF —