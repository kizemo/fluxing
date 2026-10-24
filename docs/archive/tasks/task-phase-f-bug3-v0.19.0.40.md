# Task — Phase F Bug 3 续修 (v0.19.0.40)

> 承接: `handoff-phase-f-bug3-vk_escape-2026-07-20.md`
> 装机反馈:Phrase UI Esc 键仍不退出 dialog
> Ship: 2 commits `4c8d47d` (fix) + `680af24` (release)

## Scope

修 Phase F Bug 3 (v0.19.0.39 ship 漏的真 fix):
- Test 24 false-positive (直接 dispatch WM_KEYDOWN 绕过 ListView 焦点)
- OnNotify 缺 `case LVN_KEYDOWN` → ListView 焦点时 Esc 不响应
- 真 keyboard event 路由:ListView focus → Esc → LVN_KEYDOWN → parent WndProc (WM_NOTIFY) → OnNotify

## 完成项

- [x] WeaselServer/PhrasesDialog.cpp OnNotify 加 `case LVN_KEYDOWN` (line 813)
- [x] test/TestPhrasesDialog/TestPhrasesDialog.cpp 加 TestLVNKeyDownEscapeHides + main() 调用
- [x] env.bat WEASEL_BUILD=40 (gitignored, 不 commit)
- [x] _check_install_v2.ps1 expect md5 + 注释 → v0.19.0.40
- [x] CHANGELOG.md append v0.19.0.40 entry
- [x] _buildflow.cmd Exit 0
- [x] TestPhrasesDialog **102 PASS / 0 FAIL**
- [x] NSIS installer rebuild Exit 0 (43,195,078 bytes)
- [x] install.nsi UTF-8 BOM (EF BB BF) + CRLF ✓
- [x] extract WeaselServer.exe md5 = source build md5 = `463b3c5927504d19e9627e68ca5852c2` (无 L97 stale)
- [x] commit 1 = `4c8d47d` fix(WeaselServer)
- [x] commit 2 = `680af24` chore(release)
- [x] git status -sb 验证 (working tree 累积改动未触动)

## 装机 (PENDING user)

```powershell
taskkill /F /IM WeaselServer.exe /T
reg delete "HKLM\SOFTWARE\Fluxing\Weasel" /f
reg delete "HKCU\Software\Fluxing" /f
& "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.40-installer.exe" /S /D=D:\Program Files\fluxing
& "F:\soft\00selfmade\rime_claude\_check_install_v2.ps1"
```

期望 Module 1+2 md5 = `463b3c5927504d19e9627e68ca5852c2`
期望 Module 3 L66 4 keys 写出
期望装机后设置栏点击"常用短语" → Esc 键真退出 dialog

## 沉淀

- L101-PhaseF-LVN-KEYDOWN candidate (commit message 已含)
- ERRORS.md Pattern-Key: `listview-escape-wndmock-false-positive`
- memory/2026-07-21.md daily log

## 不变量 (维持)

- env.bat gitignored ✓
- release/fluxing-0.19.0.40-installer.exe add + commit ✓
- 不动 `.claude/skills/*` / CLAUDE.md / task.md / librime (working tree 累积) ✓
- P4 module prefix ✓