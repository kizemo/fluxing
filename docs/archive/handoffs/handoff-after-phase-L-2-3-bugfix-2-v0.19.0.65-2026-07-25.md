# Handoff — Phase L v0.19.0.65 装机 bugfix 2 ship (2026-07-25)

> **承接**: `handoff-after-phase-L-2-3-bugfix-v0.19.0.64-2026-07-24.md` (v0.19.0.64 ship)
> **承接**: user 装机反馈 + 本会话重启 systematic-debugging Phase 1-4
> **状态**: ✅ ship 完成 (commit + tag + build + NSIS 都过)
> **待 user**: 重装 v0.19.0.65 + 跑 5 项验收 + 看 weasel-install.log 验证 hypothesis

---

## 1. user 装机反馈 (v0.19.0.64 ship 后)

1. ❌ Bug 1 — 部件拆字按 `,` 出英文逗号 — **v0.19.0.64 fix 真生效** (装机端 schema md5 = `BEB5B74D...` 含 `- punctuator`);user 仍感觉存在 → 可能是 build 缓存或 user 用错;本会话按 user 要求**搁置**
2. ❌ Bug 2 — 切回火流猩浮动栏不弹 — **v0.19.0.64 fix 失效** (本会话真修)
3. ✓ 切方案 / Alt+, 行为没受影响 (回归 OK)

**v0.19.0.64 fix 失效确认三连**:
- 装机端 `WeaselServer.exe` md5 = `D314072A5B7B2775D865759912F74EE6` = v0.19.0.64 source build md5 → **不是 L97 stale binary**
- 装机端 `radical_pinyin.schema.yaml` md5 = `BEB5B74D0DD7B4D53D8386898DFA93D7` = v0.19.0.64 source asset md5 → **Bug 1 fix 真的装上**
- 装机实测 Win+Space 切到 Fluxing 浮动栏仍不弹 → **v0.19.0.64 fix 自身有问题**

---

## 2. systematic-debugging Phase 1-4 trace (本会话重做)

### Bug 2 Root Cause 重审视

**Phase 1 (证据)**:
- v0.19.0.64 走的 `ImmGetDefaultIMEWnd(NULL)` 路径在 TSF-aware Win11 不可靠
  - `ImmGetDefaultIMEWnd(NULL)` 返回 calling thread 的默认 IME 窗口
  - WeaselServer.exe 是 **hidden IPC server**,无 IME context (它不处理 text input)
  - Win11 TSF-aware:IME 切换走 `ITfInputProcessorProfileActivationSink::OnActivated` per foreground app thread
  - 不是经 IMM32 默认窗口 broadcast
- 真正可靠路径已经存在:**`WeaselTSF::OnActivated` → `m_client.FocusIn()` IPC → `RimeWithWeaselHandler::FocusIn`**
  - `WeaselTSF::OnActivated` 在 `WeaselTSF/WeaselTSF.cpp:221-257` 真改写 OK
  - `RimeWithWeaselHandler::FocusIn` 在 `RimeWithWeasel/RimeWithWeasel.cpp:384-449` 有 `if (false)` 死代码

**Phase 2 (对比参考)**:
- L81-fix commit (c15745e, v0.19.0.11) 的注释明确说:
  > "L69 的 `if (false) QuickPanelDialog::Hide()` 一直没改回 `if (true)` — 导致用户切走 (Win+Space → en-US) 时 panel 残留在屏幕上"
  - L81-fix **只改 FocusOut** (`if (false) Hide()` → `Hide()`)
  - 但 **FocusIn 的 `if (false) { EnableAlwaysShowMode }` 一直没改回**
- spec 070 (L75 QuickPanel 改纯 GDI) 重启 QuickPanel 触发路径,但 `if (false)` 死代码漏改
- 8 个 ship 版本 (v0.19.0.12 → v0.19.0.64) 此 dead code 一直在

**Phase 3 (新 root cause)**:
- v0.19.0.64 ship 时 root cause 错诊:以为是 "IPC server window 收不到 WM_INPUTLANGCHANGE"
- 真正 root cause: **L69-fix 残留 `if (false)` 死代码** 堵死了 TSF 路径的 `FocusIn`
- v0.19.0.64 走的全新 `ImmGetDefaultIMEWnd` 路径是对的 fallback 思路,但 WeaselServer hidden IPC server 没 IME context,这条路径在 TSF-aware Win11 收不到 WM_INPUTLANGCHANGE

**Phase 4 (fix)**:
- `RimeWithWeasel/RimeWithWeasel.cpp:444-448` 改 `if (false && ipc_id > 0)` → `if (ipc_id > 0)`, `EnableAlwaysShowMode(6 callback)` → `QuickPanelDialog::Show(false, nullptr x 6)`
- `Show` 单次显示 (不用 `EnableAlwaysShowMode` 永远显示, spec 074 撤回 sticky)
- `FocusOut` (L81-fix) 已有 `Hide()`,对称完美
- 加埋点 (写 `%APPDATA%\Rime\weasel-install.log`) 验证 hypothesis:
  - `[Run] s_fluxingHkl=0x...` (启动时记录 Fluxing HKL)
  - `[RegisterPhrasesHotkey] ImmGetDefaultIMEWnd(NULL) = 0x...` (记录 IMM32 路径 hwnd)
  - `[ImeLangSubclass] WM_INPUTLANGCHANGE ...` (v0.19.0.64 fallback 是否触发)
- 保留 `ImmGetDefaultIMEWnd` 子类化作 fallback (legacy / 装 TSF 不全场景)

---

## 3. ship 链 (5 步全过)

### Step 1 — Source 修复 (1 commit `84f01c5`)

**`fix(WeaselServer+RimeWithWeasel): v0.19.0.65 — Phase L 装机 bugfix 2 (切输入法浮动栏真修)`**
- `RimeWithWeasel/RimeWithWeasel.cpp` `FocusIn()` 改 `if (false)` → `if (true)`, `EnableAlwaysShowMode` → `Show(false, nullptr x 6)`, 加 DLOG
- `WeaselServer/WeaselServerApp.cpp` 加 3 处埋点 (Run / RegisterPhrasesHotkey / ImeLangChangeSubclassProc)
- `.specify/memory/lessons-learned.md` 加 L##-PhaseL-4 (ImmGetDefaultIMEWnd-Dead) + L##-PhaseL-5 (L69Fix-Residue)
- `CHANGELOG.md` 加 v0.19.0.65 段 (含 v0.19.0.64 失效 root cause + 修法)
- `_check_install_v2.ps1` expected md5 同步更新

**`env.bat` 已 bump 0.64 → 0.65** (gitignored, 不 commit)。

### Step 2 — 真重建 (msbuild `/t:Rebuild`)

`_build_v065.cmd` 用 `.cmd` + background launch (避免 inline `cmd /c "..."` 卡 stdin):
- WeaselServer.exe 重建: md5 `BFC81FA7481AF3C09B581D17F8D003D5` / size 2,184,192 / mtime 2026-07-25 00:57
- FluxingPhrasesDialog.exe 重链: md5 `52EA1BA5F2FC08EC0FD3973BD9CCE0A4` (RimeWithWeasel.lib 改 → trigger 重链)
- 跟 v0.19.0.64 md5 `D314072A...` 不同, 确认真重建

### Step 3 — installer 生成

`_nsis_v065.cmd` 用 `.cmd` + background launch:
- `output/archives/fluxing-0.19.0.65-installer.exe` 43,340,676 bytes
- md5 `1326F50CBCF4BE49EFDFB23CFC5D9667`
- 7z extract 后 WeaselServer.exe md5 = `BFC81FA...` = source build md5 (无 L97 stale binary ✓)
- 7z extract 后 radical_pinyin.schema.yaml md5 `BEB5B74D0DD7B4D53D8386898DFA93D7` ✓ (Bug 1 fix preserved)

**tag v0.19.0.65** 指向 commit `84f01c5` (source commit 兼 release commit, 文件已含 installer)。

### Step 4 — SFTP 同步 (skip, user 验收后再传)

按 user 反馈 Bug 2 真修后再传 sftp。**当前未传** — `_sftp_v065.py` 还没跑。

### Step 5 — Sandbox 验证 (skip, tooling 编码问题)

sandbox-verify 跑失败, `rime-verify.ps1` 第 47 行 PowerShell 解析错误 (UTF-8 encoding issue,不是我的修复范围)。装机 user flow 验证直接走 user 装机。

---

## 4. 装机 user flow 命令 (v0.19.0.65 重装)

```powershell
taskkill /F /IM WeaselServer.exe /T
reg delete "HKLM\SOFTWARE\Fluxing\Weasel" /f
reg delete "HKCU\Software\Fluxing" /f
& "F:\soft\00selfmade\rime_claude\output\archives\fluxing-0.19.0.65-installer.exe" /S /D=D:\Program Files\fluxing
& "F:\soft\00selfmade\rime_claude\_check_install_v2.ps1"
```

期望 verify:
- Module 1+2 WeaselServer.exe md5 = `BFC81FA7481AF3C09B581D17F8D003D5`
- Module 2 installer md5 = `1326F50CBCF4BE49EFDFB23CFC5D9667`
- Module 3 L66 4 keys 全写出

---

## 5. 装机端 5 项 user 验收 (per CHANGELOG v0.19.0.65)

1. ✅ Win+Space 切到火流猩 → 浮动设置栏自动弹 (Bug 2 ✓)
2. ✅ Win+Space 切到英文 → 不弹
3. ⚠️ 在不同 app 之间切焦点到带 Fluxing 的 text input → 浮动栏弹 (TSF 路径副作用, 因为 OnActivated(true) 在每个 TSF thread 触发) — 此项是 spec 061 设计的副作用, **待 user 反馈是否接受**
4. ✅ 按 Alt+, 收起浮动栏 → 再按显示 (回归 v0.19.0.62 ✓)
5. ✅ 按 Ctrl+Shift+K → ShortcutSettings 弹 (回归 v0.19.0.28 ✓)
6. ✅ `_check_install_v2.ps1` Module 1+2 md5 = `BFC81FA...` / installer md5 = `1326f50c...`
7. 📋 期望 log (在 `%APPDATA%\Rime\weasel-install.log`):
   - `[Run] s_fluxingHkl=0xXXXXXXXX` (启动时记录 Fluxing HKL)
   - `[RegisterPhrasesHotkey] ImmGetDefaultIMEWnd(NULL) = 0x...` (hwnd 非 NULL 表示 v0.19.0.64 路径 hook 成功)
   - 切到 Fluxing 后: `[ImeLangSubclass] WM_INPUTLANGCHANGE ...` (v0.19.0.64 IMM32 fallback 路径是否触发)
   - 切到 Fluxing 后: DLOG `[FocusIn auto-show QuickPanel: ipc_id=...]` (真修 — RimeWithWeasel 路径)

---

## 6. 沉淀 (lessons-learned.md L##)

### L##-PhaseL-4-ImmGetDefaultIMEWnd-Dead
**教训**: `ImmGetDefaultIMEWnd(NULL)` 在 hidden IPC server 上不可靠 — 拿到 default IME 窗口收不到 TSF-aware foreground app IME 切换广播。
**Why**: WeaselServer.exe 是 hidden IPC server,无 IME context。Win11 TSF-aware 模式下 IME 切换走 `ITfInputProcessorProfileActivationSink::OnActivated` per foreground app thread,不是 IMM32 默认窗口 broadcast。`ImmGetDefaultIMEWnd` 只对**有 IME context** 的进程有用 (WeaselTSF.dll 加载线程)。
**How to apply**: 检测 IME 切换必须走 TSF-aware 路径 — `WeaselTSF::OnActivated` → IPC `FocusIn` → WeaselServer。**不**在 out-of-process IPC server 装 `ImmGetDefaultIMEWnd` 子类化。

### L##-PhaseL-5-L69Fix-Residue
**教训**: `if (false) { ... }` 死代码残留可跨多个版本延续 (L69→L75→L81→v0.19.0.64 共 8 个 ship 版本)。L81-fix 改对称双向 (`FocusIn`/`FocusOut`) 的一边时,另一边漏改。
**Why**: L69 因 GDI+ 崩溃临时 `if (false)` 关闭 QuickPanel。L75 spec 070 改纯 GDI 修复 GDI+ bug,应解禁。L81-fix commit 注释只提 FocusOut 残留的 `if (false) Hide()`,没提 FocusIn 残留 `if (false) EnableAlwaysShowMode`。
**How to apply**: 
- 任何 `if (false) { ... }` 死代码必须配对 re-enable trigger (issue / comment / spec 引用)
- 改对称 pair (FocusIn/FocusOut, OnAdd/OnRemove, OnStart/OnStop) 必须**同 commit** 改两边
- 改前 `git log -G "if \(false\)"` 找所有匹配,同 commit 改

**触发 L3 守门**: 任何 commit 改 `FocusIn` / `FocusOut` / `OnActivated` / `QuickPanel` 触发器,必带 `grep -RIn "if (false)"` diff 验证 + `/t:Rebuild` smoke test。

---

## 7. 剩余 backlog

**Phase L 计划内**:
- ✅ 调整 1 — v0.19.0.60 (取消用户词典) ship
- ✅ 调整 1.2 — v0.19.0.61 / .62 (UserDict 槽位删除) ship
- ✅ 调整 2+3 — v0.19.0.63 ship + v0.19.0.64 装机 bugfix ship
- ✅ 调整 3 装机 bugfix 2 — v0.19.0.65 ship (本会话)
- **(Phase L 全部 ship 完 → 完成)**

**Phase L 之外活跃项**:
- ⏸ K3 T019 装机事故调查 — PENDING user 反馈 (可顺便用 v0.19.0.65 装机 verify 一起反馈)
- ⏸ K7 GitHub push HTTP 500 — 持续 fail (走 fallback)
- ❌ 浮动设置栏其他 polish (button 0 Schema / button 3 Account no-op)
- ❌ installer.exe 老版本链接 404 (download.php fallback)
- 🆕 L##-PhaseL-4-ImmGetDefaultIMEWnd-Dead 沉淀 (本 handoff §6)
- 🆕 L##-PhaseL-5-L69Fix-Residue 沉淀 (本 handoff §6)
- 🆕 该 commit 检查 v0.19.0.65 装机用户反馈 + weasel-install.log → 验证 hypothesis → 确认「TSF 路径真触发」+「IMM32 路径是否触发」
- 🆕 if user 反馈 OnActivated(true) 每点 text input 都弹栏太烦,后续可加 debounce / 只切 IME (不切 focus) 触发

**未做的**:
- ❌ _sftp_v065.py (sftp sync) — 等 user 验收后
- ❌ Sandbox verify (rime-verify.ps1 UTF-8 encoding issue,tooling not in scope)
- ❌ Bug 1 (部件拆字标点) — user 搁置

---

## 8. 下次会话第一句话

> 承接 v0.19.0.65 ship (Phase L 装机 bugfix 2),见
> `handoff-after-phase-L-2-3-bugfix-2-v0.19.0.65-2026-07-25.md`。
> Phase L 计划内 4 步 (v0.19.0.60/61/62/63/64/65) 全部 ship 完成。
> 下一步: 跑 user 装机 verify + weasel-install.log 验证埋点 →
> 触发 sftp_sync v0.19.0.65 上线 (K7 push 持续 fail, 走 fallback) →
> 开始沉淀 spec 071 (浮动设置栏 polish) 或 K3 T019 装机事故调查。

— END OF HANDOFF —
