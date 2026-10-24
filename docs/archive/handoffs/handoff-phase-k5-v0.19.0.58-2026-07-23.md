# Handoff — Phase K5: input 子控件 Enter/Esc bubble via subclass (v0.19.0.58, 2026-07-23)

> **承接 HEAD**: `bbbe843` (chore(release): v0.19.0.58 installer)
> **承接 Ship**: v0.19.0.58 已 ship, Phase K5 hotfix 装机 v0.19.0.57 bug 3+4
> **Installer**: `F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.58-installer.exe`
> **本次范围**: 装机 user flow verify (3 项) + aiec staging SFTP 同步 + lessons 沉淀

---

## 1. Background — Phase K5 ship 已落地 (2 commits at bbbe843)

```
bbbe843 chore(release): v0.19.0.58 installer
9f8a796 fix(FluxingPhrasesDialog): v0.19.0.58 (Phase K5 Bug 3+4 真修 hotfix) — input 子控件 Enter/Esc bubble via subclass
ddb8acb chore(release): v0.19.0.57 installer   ← 上一个 ship (Phase K4 baseline)
```

### 1.1 Phase K5 ship 触发原因

User 装机 v0.19.0.57 后**立即**反馈 2 bug:
1. **Bug 3 (Phase K4 ship) 失效**: 编辑栏 Enter 不能完成添加新词 (Phase K4 fix `11eeb7a` 装机端不生效)
2. **Bug 4 (新 bug)**: Esc 不能退出常用短语 UI

### 1.2 Root cause (铁证, systematic-debugging Phase 1-3)

**两者同一真因**:
- WS_POPUP + `main.cpp` 简单 message loop (`TranslateMessage + DispatchMessageW`, **无 `IsDialogMessage`**)
- → 子控件 (`Edit control s_hInput`) focus 时, `WM_KEYDOWN VK_RETURN` / `VK_ESCAPE` 发到 Edit 子控件 WndProc
- → Edit WndProc default no-op 处理 Enter / Esc (single-line Edit + WS_POPUP 无 IsDialogMessage)
- → **不 bubble** 到 PhrasesDialog parent WndProc
- → `OnKeyDown` handler 永不触发

**为什么 Phase K4 Test 40/41 假阳 PASS** (跟 v0.19.0.40 TestEscKeyHidesDialog 同 pattern, L100-PhaseF-Bug3 教训):
- Test 模拟 `SendMessageW(hwnd, WM_KEYDOWN, ...)` 直接派发到 dialog WndProc
- 这条路径**绕过** input 子控件 focus routing, sandbox PASS
- 但装机 user 不显式 set focus → click input 打字 → Edit focus → Enter 走 Edit WndProc → no-op → fix 失效

### 1.3 修法 (Phase K5 hotfix)

**Subclass `s_hInput` (Edit control) WndProc** via `SetWindowLongPtr(GWL_WNDPROC, InputSubclassProc)`:
- Hook `WM_KEYDOWN VK_RETURN` / `VK_ESCAPE` → `SendMessage(parent, WM_KEYDOWN, wp, lp)` 转派发到 PhrasesDialog WndProc → `OnKeyDown` 触发
- 其他消息 (char input, mouse, focus) 走原 Edit WndProc (CallWindowProcW) 保持 default behavior 完整
- 实施细节:
  - `s_inputOrigWndProc` (匿名 namespace static, 缓存原 WndProc)
  - `InputSubclassProc` (匿名 namespace free function, subclass handler)
  - `PhrasesDialog::InstallInputSubclass(HWND)` / `RemoveInputSubclass(HWND)` 公开 helpers (test 验证)
  - `OnCreate` 末 Install (s_hInput 创建后)
  - `OnDestroy` Remove (避免 leak)

### 1.4 Critical build infra lesson (ship gate)

**Phase K4 ship 隐患**: `_buildflow.cmd` 跑 `msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32` — **默认 Win32 Platform scope**, 不 rebuild x64 FluxingPhrasesDialog.exe.
- Phase K4 ship 时 `output/FluxingPhrasesDialog.exe` (x64) mtime 13:22 仍是 v0.19.0.55 旧 binary (跟 v0.19.0.57 installer md5 `B92137F5...` 一致), 不是 v0.19.0.57 实际 rebuild 后的 v0.19.0.57 binary
- **后果**: v0.19.0.57 installer 装的是 stale Phase K4 source binary (但 Phase K4 fix 在 OnKeyDown 已 ship, 所以 Enter/Esc 即使 bubble 到 parent 也不 work — 因为 root cause 是 bubble 缺失, 不是 source)
- **Phase K5 fix**: 手动 rebuild x64 (`msbuild FluxingPhrasesDialog\FluxingPhrasesDialog.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64`)+ NSIS rebuild → install archive 含 fresh x64 binary verified md5 = source build

### 1.5 Verify (sandbox Win32 Release MSBuild path)

| 项 | 值 |
|---|---|
| TestPhrasesDialog.exe | **212 PASSED / 0 FAILED** (含 2 新增 Test 43/44 + 209 baseline) |
| WeaselServer.exe md5 (Win32) | `D0E568BBBA749931C618B57DD14C27C4` (= v0.19.0.57, 无 source 改动) |
| FluxingPhrasesDialog.exe md5 (x64) | `8E569EA7FE94CA5C27AE99ECF3B3C8B0` (新 source rebuild, 跟 v0.19.0.57 B92137F5... 不同) |
| NSIS installer md5 | `312053473F9D41FCA65EF659F2185B9A` |
| NSIS installer size | 43,356,971 bytes |

---

## 2. 装机 user flow verify (3 项, **PENDING user**)

v0.19.0.58 user 装机后跑:

1. **编辑栏输入文字 + Enter → 新加**: 装机 v0.19.0.58, 调出常用短语 UI → focus 默认在 list (ListView 第 0 项 selected, 这是 v0.19.0.35 默认) → click input → input focus → 输入新词 → 按 Enter → 新加到 list 末尾, input 清空
2. **list 选中短语 + 编辑 + Enter → 改**: click input 自动 fill 选中 phrase text → 改文字 → 按 Enter → 短语内容变, input 清空
3. **Esc → 退出**: 任意 focus + 按 Esc → UI 关

```powershell
# 装机 (L13/L54/L66 强约束)
taskkill /F /IM WeaselServer.exe /T
reg delete "HKLM\SOFTWARE\Fluxing\Weasel" /f
reg delete "HKCU\Software\Fluxing" /f
& "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.58-installer.exe" /S /D=D:\Program Files\Fluxing
& "F:\soft\00selfmade\rime_claude\_check_install_v2.ps1"
# 期望 Module 1+2:
#   WeaselServer.exe md5 = D0E568BBBA749931C618B57DD14C27C4
#   FluxingPhrasesDialog.exe md5 = 8E569EA7FE94CA5C27AE99ECF3B3C8B0
# 期望 Module 3:
#   HKLM\SOFTWARE\Microsoft\CTF\KnownClasses (L66 hardening 保留)
# 期望 installer md5: 312053473F9D41FCA65EF659F2185B9A
```

---

## 3. SFTP 上传 aiec staging remote (PENDING user 凭据)

按 `feedback_pinyin_release_workflow.md` §9 (凭据不写 memory, 每次现问):
- 凭据来源: `E:\办公文件\L1网站\.vscode\sftp.json` (host=47.120.26.175:22, user=kizemo, remote=/var/www/wordpress/pinyin/)
- 凭据用一次性 heredoc, 不写入任何 memory 文件
- 上传 4 个文件: `fluxing-0.19.0.58-installer.exe` + `version.json` + `appcast.xml` + `release-notes.html`
- 服务器端 md5 必须跟本地完全一致 (双向 verify)

工具: Python paramiko 4.0.0 (sshpass/lftp/expect/WinSCP 全没装)

**aiec staging 本地已同步**:
- `E:\办公文件\L1网站\pinyin\fluxing-0.19.0.58-installer.exe` 已 copy (md5 = `312053473F9D41FCA65EF659F2185B9A`)
- `version.json` 4 字段更新到 v0.19.0.58
- `appcast.xml` 顶部插入 v0.19.0.58 item (length=43356971, pubDate=Thu 23 Jul 2026 17:22:00 +0800)
- `release-notes.html` 顶部插入 v0.19.0.58 h2 + 7 项 bullet

---

## 4. 不在本次范围 (新会话不要重做)

- ❌ 不要重 build v0.19.0.58 (installer + binary 已 ship, md5 verified)
- ❌ 不要重 commit (9f8a796 + bbbe843 已 merged)
- ❌ 不要改 PhrasesDialog.cpp / .h / TestPhrasesDialog.cpp / .vcxproj (已 commit)
- ❌ 不要改 env.bat / weasel.props / _check_install_v2.ps1 (已 commit)
- ❌ 不要动 librime submodule (Phase K5 不动 RimeWithWeasel / librime API)
- ❌ 不要改 install.nsi (L66 hardening 44292d7 已 ship, 不破)
- ❌ 不要走 xmake 路径 (L106 教训, 走 `_buildflow.cmd` MSBuild Win32 + 手动 rebuild x64 for FluxingPhrasesDialog.exe)

---

## 5. 关键文件索引 (新会话按需读)

| 文件 | 用途 |
|---|---|
| `FluxingPhrasesDialog/PhrasesDialog.cpp:102-144` | `s_inputOrigWndProc` + `InputSubclassProc` 匿名 namespace definition (Phase K5 关键修法) |
| `FluxingPhrasesDialog/PhrasesDialog.cpp:773-778` | OnCreate 末 InstallInputSubclass(s_hInput) |
| `FluxingPhrasesDialog/PhrasesDialog.cpp:786-794` | OnDestroy 卸 input subclass |
| `FluxingPhrasesDialog/PhrasesDialog.cpp:801-830` | InstallInputSubclass + RemoveInputSubclass 实施 (SetWindowLongPtr GWL_WNDPROC) |
| `FluxingPhrasesDialog/PhrasesDialog.cpp:843-858` | OnKeyDown VK_RETURN 智能分支 (Phase K4 source, 仍未触发直接) |
| `FluxingPhrasesDialog/PhrasesDialog.cpp:881-883` | OnKeyDown VK_ESCAPE → Hide (Phase K4 source) |
| `FluxingPhrasesDialog/PhrasesDialog.h:212-237` | InstallInputSubclass + RemoveInputSubclass 声明 + 注释 root cause |
| `test/TestPhrasesDialog/TestPhrasesDialog.cpp:1480-1548` | Test 43 + Test 44 (Phase K5 新增, 模拟 input focus 装机 user 路径) |
| `FluxingPhrasesDialog/main.cpp:37-40` | 简单 message loop (无 IsDialogMessage, 是 Bubble fail 的部分根因) |
| `env.bat` | `WEASEL_BUILD=58`, `PRODUCT_VERSION/FILE_VERSION=0.19.0.58` |
| `_check_install_v2.ps1:55-58` | expected md5 全更新到 v0.19.0.58 |
| `output/install.nsi:647` | `File "FluxingPhrasesDialog.exe"` (引用 x64, 必须手动 rebuild) |
| `E:\办公文件\L1网站\pinyin\{fluxing-0.19.0.58-installer.exe, version.json, appcast.xml, release-notes.html}` | aiec staging 本地已更新 |
| `E:\办公文件\L1网站\.vscode\sftp.json` | SFTP 凭据来源 (不写 memory) |

---

## 6. 不动项 / 风险点

- **不动**: `.claude/skills/*` / `CLAUDE.md` / `task.md` 是 prior session 累积 modifications, 跟 Phase K5 无关, 留给 user 判断单独 commit / discard
- **不动**: librime submodule dirty (HEAD pointer + actual 内容一致, dirty 是 uncommitted changes in submodule, 跟 Phase K5 无关)
- **不动**: 8 commits history (88280dd / 44292d7 / 823bc1f / 8559ed4 / f6200a6 / c465254 / cead012 / 05a11bd + 11eeb7a / ddb8acb Phase K4 + 9f8a796 / bbbe843 Phase K5)
- **不动**: aiec staging 旧 exe (`fluxing-0.19.0.57-installer.exe` + `fluxing-0.19.0.44-installer.exe` 等历史) 保留 (跟 v0.19.0.57 ship 时一致策略)
- **风险**: 用户装机 v0.19.0.58 后若仍 fail (Test 43/44 装机端 path 已 RED→GREEN, 装机端应该 work) — 立即 trace `_check_install_v2.ps1` Module 1+2 看 md5 跟 source build 是否 match (L106 教训 x64 binary stale trap)
- **风险**: SFTP 上传前 user 装机 — 顺序: 装机验证后才 SFTP (避免装 hot-fix 失败直接 push 网上)

---

## 7. Commit 计划 (新会话)

如果装机验证 + SFTP 上传都 success, 无新 commit (Phase K5 ship 已完整)
如果装机发现新 bug: `fix(FluxingPhrasesDialog): v0.19.0.59 (Phase K5.5 follow-up)` + `chore(release): v0.19.0.59 installer`
如果 SFTP 上传需要单独的 commit trace: 写 `memory/<YYYY-MM-DD>.md` daily log (per `feedback_pinyin_release_workflow.md` §11)

---

## 8. Lessons 沉淀 (新会话, LE §6.1.3 PENDING)

待沉淀到 `.specify/memory/lessons-learned.md`:

- **L##-PhaseK5-WS-POPUP-message-loop**: WS_POPUP window 的 main loop 必须用 `IsDialogMessage(s_hwnd, &msg)` 处理子控件键盘事件, 或者 subclass 子控件 WndProc hook target 键。简单 `TranslateMessage + DispatchMessageW` 让子控件吃掉 Enter/Esc 不 bubble — Enter 在单行 Edit 默认 no-op, Esc default no-op, dialog 路由废。**装机 user flow 必须先验证 WS_POPUP dialog keyboard routing** — sandbox SendMessage 路径会绕过这条 routing false-positive。

- **L##-PhaseK5-stale-x64-binary** (已存在 L106 教训加强版): `_buildflow.cmd` 走 default Win32 Platform scope, 不 rebuild x64 project (FluxingPhrasesDialog.vcxproj x64 配置)。Ship 时必须:
  1. `msbuild weasel.sln /p:Platform=Win32` (default) → rebuild Win32 production
  2. 单独 `msbuild FluxingPhrasesDialog\FluxingPhrasesDialog.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64` (x64 production)
  3. NSIS installer File 命令引用 `output\FluxingPhrasesDialog.exe` (x64) 必须 rebuild 后才进 installer
  4. 7z extract 后 verify installer 内部 `FluxingPhrasesDialog.exe` md5 = source rebuild x64 binary
  ship gate: 5-axis verify (md5 + 真 binary + L106 hardening 保留 + L66 4 keys + L107 tafkey pair registry)

---

## 9. 上下文读取建议 (新会话第一分钟)

1. 读 `MEMORY.md` (auto-memory 索引) + 本 handoff
2. 读 `.specify/memory/lessons-learned.md` L66 / L100 / L101 / L102 / L104 / L105 / L106 (最近 7 个 lesson)
3. 跑 `git log --oneline -5` 确认 HEAD = `bbbe843`
4. 跑 `git status -sb` 确认 dirty set (per CLAUDE.md §2 必查) — 跟 prior session 残留 + Phase K5 commit 后 working tree 应匹配
5. 读 `_check_install_v2.ps1:55-58` + `env.bat` 确认版本号 + md5 预期
6. 开始装机 user 验 (3 项) + SFTP 同步

— END OF PHASE K5 HANDOFF —