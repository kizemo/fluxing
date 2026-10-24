# Handoff — Phase L v0.19.0.64 装机 bugfix ship (2026-07-24)

> **承接**: `handoff-after-phase-L-2-3-ship-2026-07-24.md` (v0.19.0.63 ship 完成)
> **承接**: user 装机后 4 条反馈 + stop hook 5 项 review
> **状态**: ✅ ship 完成 (commit + tag + SFTP 都过)
> **待 user**: 重装 v0.19.0.64 + 跑 4 项验收

---

## 1. user 装机反馈 4 项 (v0.19.0.63 ship 后)

1. ❌ 部件拆字 (radical_pinyin) 按 `,` 出**英文逗号** — `punctuator` 段加了但没生效
2. ❌ 切回火流猩浮动设置栏**不弹** — WM_INPUTLANGCHANGE 钩子没生效
3. ✓ 切到拼音/小鹤双拼等按 `,` 仍是中文逗号 (回归 OK)
4. ✓ Alt+, 收起/显示 (回归 OK)

---

## 2. systematic-debugging Phase 1-4 trace

### Bug 1 — 「部件拆字按 , 出英文逗号」

**Phase 1 (证据)**:
- 装机端 `data\radical_pinyin.schema.yaml` md5 = installer 内嵌 md5 = `0FB0EA2EBAC4E0354D069D583A771C21` ✓ (新文件确实装进去)
- 装机端 schema 文件**已含** `punctuator:` 段 (15 行附近)
- `engine.processors` 列表是 `key_binder / speller / selector / navigator / express_editor` — **没有 `- punctuator`**

**Phase 2 (对比参考)**:
- `rime_ice.schema.yaml` L48-49 `engine.processors: ... - punctuator`
- `double_pinyin.schema.yaml` L60 `- punctuator`
- 9 个方案都**显式**在 `engine.processors` 列出 `- punctuator`

**Phase 3 (root cause)**:
- librime **必须**在 `engine.processors` 列表中显式注册 `- punctuator` 才会调用 `punct_segmentor` + `punct_translator`
- v0.19.0.63 Phase L 调整 2 我**只加了** `punctuator:` 段 (mapping + symbols),**漏了**在 processors 注册
- 这是典型的"源码改动看起来对、build 也成功、装机却失效"ship-time bug

**Phase 4 (fix)**:
- `output/data/radical_pinyin.schema.yaml` `engine.processors` 列表加 `- punctuator` (一行)
- 跟 rime_ice / double_pinyin 对齐

### Bug 2 — 「切回火流猩浮动栏不弹」

**Phase 1 (证据)**:
- 装机端 WeaselServer.exe md5 = `5CA3CFBEB9B0A34E66B1F94112601401` (= v0.19.0.63 source build md5, 无 L97 stale binary ✓)
- Alt+, toggle 能 work → ID_HOTKEY_QUICK_PANEL 路径正常
- Win+Space 切输入法浮动栏不弹 → WM_INPUTLANGCHANGE 路径失败

**Phase 2 (对比参考)**:
- `PhrasesHotkeySubclassProc` subclass 化的是 **IPC server window** (`m_server.GetHWnd()`)
- Windows 把 `WM_INPUTLANGCHANGE` 发给**当前 focus 的 top-level 窗口**
- IPC server window 是 hidden + 没焦点 → 收不到 WM_INPUTLANGCHANGE
- `ImmGetDefaultIMEWnd(NULL)` 返回 Windows 自动管理的 hidden IME 窗口,**专门**接收 IME 切换广播

**Phase 3 (root cause)**:
- v0.19.0.63 把 WM_INPUTLANGCHANGE 钩子挂在 IPC server window 上 → 永远收不到事件
- 装机时 Win+Space 切到 Fluxing → IPC server window 收不到 → QuickPanelDialog::Show() 不触发
- Alt+, 是 RegisterHotKey 路径(走 ID_HOTKEY_QUICK_PANEL → ServerImpl::OnHotkey → ToggleMode), 不依赖 WM_INPUTLANGCHANGE, 所以 work

**Phase 4 (fix)**:
- `WeaselServer/WeaselServerApp.h` 加 `ImeLangChangeSubclassProc` static WndProc 声明 + `m_imeDefaultOrigWndProc` + `m_hwndImeDefault` 字段
- `WeaselServer/WeaselServerApp.cpp` `RegisterPhrasesHotkey()` 额外子类化 `ImmGetDefaultIMEWnd(NULL)`:
  ```
  m_hwndImeDefault = ::ImmGetDefaultIMEWnd(NULL);
  if (m_hwndImeDefault) {
    m_imeDefaultOrigWndProc = (WNDPROC)::GetWindowLongPtr(m_hwndImeDefault, GWLP_WNDPROC);
    if (m_imeDefaultOrigWndProc)
      ::SetWindowLongPtr(m_hwndImeDefault, GWLP_WNDPROC,
                         (LONG_PTR)&WeaselServerApp::ImeLangChangeSubclassProc);
  }
  ```
- 新 `ImeLangChangeSubclassProc` 仅处理 `WM_INPUTLANGCHANGE` → QuickPanelDialog::Show(), 其他消息透传
- `PhrasesHotkeySubclassProc` 还原成 spec 042 原始 hotkey-only 路径 (移除 v0.19.0.63 误加的 WM_INPUTLANGCHANGE 分支, 避免 IPC server window 路径干扰)
- `UnregisterPhrasesHotkey()` 同步还原两个子类化

### 装机 user flow 4 项验收 (per user 反馈)

1. 切到 `radical_pinyin` (部件拆字) → 中文模式按 `,` → 应出 "，" (Bug 1 fix ✓)
2. 切到 `rime_ice` / `double_pinyin_flypy` → 按 `,` → 应出 "，" (回归 ✓)
3. Win+Space 切到火流猩 → 浮动设置栏自动弹 (Bug 2 fix ✓)
4. Win+Space 切到英文 → 不弹 ✓
5. 按 Alt+, 收起浮动栏 → 再按显示 (回归 v0.19.0.62 ✓)

---

## 3. ship 链 (4 步全过)

### Step 1 — Source 修复 (1 commit `55e7991`)

**`fix(WeaselServer): v0.19.0.64 — Phase L 装机 bugfix (中文标点生效 + 切输入法弹浮动栏)`**
- `WeaselServer/WeaselServerApp.cpp` + `WeaselServerApp.h` 加 ImeLangChangeSubclassProc + ImmGetDefaultIMEWnd subclass (Bug 2 fix)
- `output/data/radical_pinyin.schema.yaml` `engine.processors` 加 `- punctuator` (Bug 1 fix)
- `CHANGELOG.md` 加 v0.19.0.64 段 (含装机反馈 4 项 + 两 bug root cause + 修法)
- `_check_install_v2.ps1` expected md5 同步更新

**`env.bat` 已 bump 0.63 → 0.64** (gitignored, 不 commit)。

### Step 2 — 真重建 (msbuild `/t:Rebuild`)

`_build_v064.cmd` 用 Start-Process 跑 msbuild (`_buildflow.cmd` 在 msys bash + cmd.exe 链路里 stdin 卡住, 不能用 inline `cmd /c "..."`, 必须用 .cmd + Start-Process)。
- WeaselServer.exe 重建: md5 `D314072A5B7B2775D865759912F74EE6` / size 2,186,752 / mtime 20:54
- 跟 v0.19.0.63 md5 `5CA3CFBE...` 不同, 确认真重建

### Step 3 — installer 生成

`_nsis_v064.cmd` 用 Start-Process 跑 makensis:
- `output/archives/fluxing-0.19.0.64-installer.exe` 43,346,540 bytes
- md5 `EA510285DED7F791F536DECCF36E357F`
- sha256 `9526A97E7420876D29A707176C1B291DF46E4678C9009B2336F6982E09701D7A`
- 7z extract 后 WeaselServer.exe md5 = `D314072A...` = source build md5 (无 L97 stale binary ✓)
- 7z extract 后 radical_pinyin.schema.yaml md5 `BEB5B74D0DD7B4D53D8386898DFA93D7` ✓ 含 `- punctuator`

**tag v0.19.0.64** 指向 commit `55e7991` (source commit 兼 release commit, 文件已含 installer)。

### Step 4 — SFTP 同步 (cross-source verify OK)

`python sftp_sync.py --version=0.19.0.64`:
- local md5 pre-check: installer `EA510285...` ✓
- atomic upload 4 文件 (installer + version.json + appcast.xml + release-notes.html)
- staged + final md5 verify 全 OK
- `www.aiec.fun/pinyin/fluxing-0.19.0.64-installer.exe` 已上线

---

## 4. 装机 user flow 命令 (L13/L54/L66)

```powershell
taskkill /F /IM WeaselServer.exe /T
reg delete "HKLM\SOFTWARE\Fluxing\Weasel" /f
reg delete "HKCU\Software\Fluxing" /f
& "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.64-installer.exe" /S /D=D:\Program Files\fluxing
& "F:\soft\00selfmade\rime_claude\_check_install_v2.ps1"
```

期望 verify:
- Module 1+2 WeaselServer.exe md5 = `D314072A5B7B2775D865759912F74EE6`
- Module 2 installer md5 = `EA510285DED7F791F536DECCF36E357F`
- Module 3 L66 4 keys 全写出

---

## 5. 装机端 6 项 user 验收 (per CHANGELOG v0.19.0.64)

1. 切到 `radical_pinyin` (部件拆字) → 中文模式按 `,` → "，" (Bug 1 ✓)
2. 切到 `rime_ice` / `double_pinyin_flypy` → 按 `,` → "，" (回归 ✓)
3. Win+Space 切到火流猩 → 浮动设置栏自动弹 (Bug 2 ✓)
4. Win+Space 切到英文 → 不弹 ✓
5. 按 Alt+, 收起浮动栏 → 再按显示 (v0.19.0.62 行为不变 ✓)
6. 按 Ctrl+Shift+K → ShortcutSettings 弹 (回归 v0.19.0.28 ✓)

---

## 6. 沉淀 (lessons-learned.md L##)

### L##-PhaseL-2-PunctuatorProcessor
**教训**: librime schema 加 `punctuator:` 段不够,**必须**在 `engine.processors` 列表显式注册 `- punctuator` 才会生效。
**Why**: librime 的 processor pipeline 是显式声明式,没在 processors 列出的 processor 段会被忽略。rime_ice / double_pinyin / 其他 9 个方案都已在 processors 含 `- punctuator`;radical_pinyin 漏。
**How to apply**: 改任何 schema.yaml 之前,先 `grep -A 10 "^engine:"` 看 processors 列表;如果新加 processor 段,一定要在 processors 也注册。

### L##-PhaseL-3-ImeDefaultWnd
**教训**: Win32 `WM_INPUTLANGCHANGE` 不发给 hidden IPC server window,只发给当前 focus 的 top-level 窗口。要 hook IME 切换必须用 `ImmGetDefaultIMEWnd(NULL)` 拿 Windows 自动管理的 IME 默认窗口。
**Why**: spec 042 时代 hook WM_HOTKEY 在 IPC server window 上 work, 因为 hotkey 是全局的(RegisterHotKey 绑 IPC server hwnd, Windows 自己派发);但 WM_INPUTLANGCHANGE 是窗口消息,只发给当前 focus。IME 切换时 focus 在前台 app, IPC server window 收不到 → QuickPanel 不弹。
**How to apply**: 任何"检测系统级 IME 切换"需求, 用 `ImmGetDefaultIMEWnd(NULL)` subclass 而不是 IPC server window;或者用 `SetWinEventHook(EVENT_OBJECT_FOCUS, ...)` 在 DLL 里全局 hook。

---

## 7. 剩余 backlog

**Phase L 计划内**:
- ✅ 调整 1 — v0.19.0.60 (取消用户词典) ship
- ✅ 调整 1.2 — v0.19.0.61 / .62 (UserDict 槽位删除) ship
- ✅ 调整 2+3 — v0.19.0.63 ship + v0.19.0.64 装机 bugfix ship (本次)
- **(Phase L 全部 ship 完 → 完成)**

**Phase L 之外活跃项**:
- ⏸ K3 T019 装机事故调查 — PENDING user 反馈 (可顺便用 v0.19.0.64 装机 verify 一起反馈)
- ⏸ K7 GitHub push HTTP 500 — 持续 fail (走 fallback)
- ❌ 浮动设置栏其他 polish (button 0 Schema / button 3 Account no-op)
- ❌ installer.exe 老版本链接 404 (download.php fallback)
- 🆕 L##-PhaseL-2-PunctuatorProcessor 沉淀 (本 handoff §6)
- 🆕 L##-PhaseL-3-ImeDefaultWnd 沉淀 (本 handoff §6)

---

## 8. 下次会话第一句话

> 承接 v0.19.0.64 ship (Phase L 装机 bugfix),见
> `handoff-after-phase-L-2-3-bugfix-v0.19.0.64-2026-07-24.md`。
> Phase L 计划内全部 ship 完成。下一步等 user 跑 6 项装机 verify;
> 或者按 stop hook 之前的 review 处理 _buildflow.cmd gitignore + spec/plan/tasks 三件套补齐。

— END OF HANDOFF —