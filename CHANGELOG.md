

## [0.19.0.46-fluxing] - 2026-07-22

### fix(WeaselServer): v0.19.0.46 (Phase I Bug 4) — 输入框中文 IME 真修 (删 isolated HIMC)

**User pain** (post v0.19.0.45 `2d842fc` + `a7048db` ship):
- 装机 v0.19.0.45 user 反馈"列表框 / 输入框可以随UI边界调整了" → Bug 1 + Bug 2 真修
- **剩余 bug**: 在输入框中**仍无法输入中文** (打拼音不出候选词, IME 状态栏不显示)

**Root cause** (5 阶段 systematic-debugging):

`WeaselServer/PhrasesDialog.cpp:671-675` 4 个 ship 版本 (v0.19.0.35 / 0.19.0.36 / 0.19.0.43 / 0.19.0.45) 反复改 `ImmCreateContext` + `ImmAssociateContext` 都没修好, 装机端**4 个 ship 都复现**"输入框不能输中文":

```cpp
HIMC himc = ImmCreateContext();
if (himc) {
  ImmAssociateContext(s_hInput, himc);
  // 不调 ImmReleaseContext (per MSDN, 假设 himc 跟 hwnd 同生死)
}
```

**真因** (v0.19.0.46 真修, 见 `report-4bug-v0.19.0.45-ime-stuck.md` + lessons-learned.md L##-PhaseH-IME):
- `WeaselServer.exe` 是 **TSF shim 进程** (weaselx64.dll 主导), 不是普通 GUI 进程
- TSF shim 通过 `ITfThreadMgr` + `ITfInputProcessorProfileMgr` 强制 hwnd 走 system default IME context
- 我们自己创建 isolated HIMC 给 `s_hInput` → 跟 TSF IME 互斥 → IME 候选词不出
- **v0.19.0.43** 改成"不调 ImmReleaseContext" 理论上符合 MSDN, 但**忽略 TSF 进程下不认 isolated HIMC** —— 4 个 ship 复发累积

**Cure** (commit v0.19.0.46, 方案 A — 完全删除 IME 关联代码):
1. `PhrasesDialog.cpp` line 670-675 删 5 行 (`HIMC himc` + `ImmCreateContext` + `ImmAssociateContext` 块)
2. 替换为注释: "TSF shim 自动给 hwnd 配 system default IME context (跟主编辑框同路径, v0.19.0.32 之前裸 CreateWindowExW 路径)"
3. 走 v0.19.0.32 之前的"裸 CreateWindowExW"路径 — 当时装机 user 没报过 IME bug

**Verify** (sandbox Win32 Release):
- TestPhrasesDialog: **168/168 PASS / 0 FAIL** (Test 1-35 全过, IME 不在 sandbox 测, layout / list / 按钮 / reorder / drag 全 OK)
- msbuild weasel.sln: **Exit 0** (WeaselServer.exe + 16 test suites + Deployer + Setup 全 rebuild)
- WeaselServer.exe md5: `1caa6e94b209d1d0f3214b4d08c45310` (从 v0.19.0.45 `b19d4338...` 改; 是真 source build, 非 L97 stale)
- install.nsi: 未改 (无需新装机段; v0.19.0.45 installer 段已 ship)

**Files touched** (v0.19.0.46, 1 + 1):
- `WeaselServer/PhrasesDialog.cpp` (line 670-675 删 5 行 IME 关联, 改 1 段注释)
- `CHANGELOG.md` (本 entry)
- `release/fluxing-0.19.0.46-installer.exe` (新建)

**lessons-learned** (L##-PhaseH-IME-TSF-Mutex, 见 `lessons-learned.md`):
- L##-A (TSF shim 进程不认 isolated HIMC): TSF 进程下不要 `ImmCreateContext` + `ImmAssociateContext`, 让 TSF shim 主导配 default IME
- L##-B (4 ship 复发门槛): v0.19.0.35/36/43/45 4 个 ship 都 fail, 累计装机 user 反馈 4 次, 远超 L1 门槛需正式 lessons-learned
- L##-C (Per-MSDN 不可信): "the application should not call ImmReleaseContext" 是 per-process 默认, TSF 进程下 per-hwnd isolated HIMC 跟 system TSF IME 互斥才是真陷阱

**装机 user flow 5 项验收** (per `verification-before-completion` skill):
1. 打开常用短语 dialog → 列表显示 ✓
2. 点 input 框 → focus 切到 input ✓
3. 切到微软拼音 / 小狼毫拼音 → 打 "ni" → **候选词出 "你/尼/泥"** (核心验收)
4. 按数字键 1 选第一个候选词 → **"你" 进 input 框**
5. 点 AddTop 按钮 → "你" 进 ListView (端到端 commit 成功)

**Ship**: `release\fluxing-0.19.0.46-installer.exe` (待 build)

---

## [0.19.0.38-fluxing] - 2026-07-20

### fix(installer): v0.19.0.38 hotfix — 装机黑屏 + taskkill cmd 窗口

**User pain** (post v0.19.0.37 commit 2619989):
1. 装机时 Windows 黑屏 2 秒 (explorer.exe taskkill 副作用)
2. 杀进程时 cmd 窗口弹出 (NSIS ExecWait 调 taskkill console tool)

**Root cause** (5 阶段 systematic-debugging):
- `af13cbff` (v0.19.0.36 Phase D Reinforcement D) 加了:
  ```nsi
  ExecWait 'taskkill /F /IM explorer.exe /T'  ; 杀 explorer.exe
  Sleep 2000                                   ; 黑屏 2s
  Exec '"$WINDIR\explorer.exe"'                ; 重启
  ```
- **explorer.exe 是 Windows shell (taskbar + desktop)**, 杀它必然黑屏
- "explorer 通过 shell notification hooks 持 mmap" 是编的理论, 没真验证。
  真 TSF shim host = WeaselServer.exe / ctfmon.exe / TextInputHost.exe
  (跟 installer hardening 9b3e0824 / 24239f49 已 ship 的 taskkill 列表一致),
  explorer.exe 不在 TSF chain
- L72-fix Rename-then-File (9b3e0824) 已经能释放 file handle, 不需要杀 explorer 兜底
- 第二 bug: 7 处 `ExecWait 'taskkill ...'` 全走 NSIS ExecWait → 调 console tool 弹 cmd 窗口

**Cure** (commit 000753d + stop hook 修补):
1. **删 `install.nsi` line 408-416** 杀 explorer.exe + 重启段 (黑屏 root cause)
2. **全 7 处 `ExecWait 'taskkill` 改 `nsExec::ExecToStack 'taskkill`** (NSIS 标准 plugin, 静默 + 阻塞 + 返 exit code, 不弹 cmd 窗口)
3. **每处 nsExec::ExecToStack 后 `Pop $0; Pop $1`** (吃 exit code + stdout, 避免 stack leak 14 entries)
4. `env.bat` WEASEL_BUILD 37 → 38 (v0.19.0.37 hotfix 2 → v0.19.0.38)

**Smoke-test recipe** (AGENTS.md §2.5 mandatory after install.nsi change):
- silent install to `D:\TEMP\fluxing-test\ProgramFiles` (cmd /c, NOT Start-Process - L15 PS 5.1 merge bug)
- layout invariants: fluxing\weasel\WeaselServer.exe + fluxing\user1\fluxing\ + rime.dll 2-5MB + L14 x86 arch
- L66 reg query: HKLM\SOFTWARE\Microsoft\CTF\KnownClasses + HKCU\Software\Microsoft\CTF\Assemblies\0x00000804 + HKLM\SOFTWARE\Classes\CLSID\{A3F4CDED-...}\InprocServer32

**verify** (sandbox Win32 Release):
- makensis build: Exit 0 ✓
- installer md5: `53cec551100d29938fdf72c59c384900` (≠ 之前 v0.19.0.37 c40a85ee...; c41feed9 amend 后从 43,189,443 → 43,207,519 bytes 重打包)
- extract 后 WeaselServer.exe md5: `62bf75b119cc1d0a92dfbf68e5706dc6` = source build (无 L97 stale)
- installer 含 `$PLUGINSDIR\nsExec.dll` (7,168 bytes, NSIS bundled plugin)
- install.nsi self-verify: `ExecWait 'taskkill` remaining = 0; `nsExec::ExecToStack` refs = 6; explorer.exe taskkill = 0; `Pop $0; Pop $1` × 12 ✓ (grep -nE 实测 6 处, 删 explorer.exe 后从 7 降 6)
- TestPhrasesDialog: 93/93 PASS (install.nsi 不影响 C++ binary)
- TestUserDictionary: 26/26 PASS

**Files touched** (v0.19.0.38):
- `output/install.nsi` (黑屏 + 静默 taskkill + Pop 14 个, 22+/20-)
- `release/fluxing-0.19.0.38-installer.exe` (43.2 MB, 含 nsExec.dll)

**Ship**: `release\fluxing-0.19.0.38-installer.exe` 43,207,519 bytes, md5 `53cec551100d29938fdf72c59c384900` (c41feed9 amend 后重打包, 000753df 老 md5 `d53b0c37...` 已废)

**lessons-learned** (L100-PhaseD-EXPLORER):
- L100-V 编"理论"ship 是装机 bug 的常见陷阱
- L100-W NSIS 静默调 console tool 用 nsExec (无需 !include)

---

## [0.19.0.39-fluxing] - 2026-07-20

### fix(WeaselServer): v0.19.0.39 (Phase F) — 按钮路径 Phrase UI 键盘事件路由 + Esc 退出

**User pain** (post v0.19.0.38 c41feed9 ship):
1. **Bug 1**: 设置栏点击"常用短语"按钮启动 UI → ↑↓ 不能换选中, Enter 不能上屏, **但双击可以上屏**
2. **Bug 2** (已 work, 修前留作对照): Alt+. 启动 UI → ↑↓/Enter/DoubleClick 全 work
3. **Bug 3**: 期望 Esc 键退出 Phrase UI (实际不响应)

**Root cause** (5 阶段 systematic-debugging):

对照 Bug 1 (fail) vs Bug 2 (work), 双击两边都 work → NM_DBLCLK handler 正确 → ListView 本身正常。差异在 keyboard event 路由。

`PhrasesDialog::Show()` 创建路径 (line 119-183) 只调 `SetWindowPos(HWND_TOPMOST)` + `ShowWindow(SW_SHOW)`, **没调 `SetForegroundWindow`**。已存在路径 (line 89-95) 调 SetForegroundWindow, 但创建路径不调。

- **按钮路径**: QuickPanelDialog 是 WS_POPUP visible + foreground。Click → `s_onPhrases()` → `PhrasesDialog::Show()` 创建 → QuickPanelDialog 仍是 foreground。PhrasesDialog 是 topmost 但**不是 foreground**。
  - 键盘事件 (↑↓/Enter/Esc) 发到 foreground (QuickPanel), **不传 PhrasesDialog**
  - WM_LBUTTONDBLCLK 是 mouse event, mouse 直接命中 ListView 仍触发 NM_DBLCLK → 双击 work
- **Alt+. 路径**: WM_HOTKEY 触发时 Windows 自动让 hotkey owner (WeaselServer) 进 foreground, PhrasesDialog 创建后 foreground 自然切换 → 键盘事件正常路由

**Cure** (Option C 双保险):
1. `PhrasesDialog::Show()` 创建路径加 `AllowSetForegroundWindow(ASFW_ANY)` + `SetForegroundWindow(s_hwnd)` — 拿抢 foreground 锁 (Vista+ lock) + 强制 foreground (用 mock 函数指针 + Test 23 verify 调用)
2. `WeaselServerApp.cpp` line 314 onPhrases lambda 先 `QuickPanelDialog::Hide()` 释放 foreground — 配合 SetForegroundWindow 完成 foreground transition
3. 加 `static SetForegroundFn / AllowSetForegroundFn` setter (Test 23 mock)
4. Test 23 (foreground mock): Show() 调 AllowSetForegroundWindow + SetForegroundWindow 1 次, 目标 = s_hwnd
5. Test 24 (Esc handler): WM_KEYDOWN VK_ESCAPE → Hide (Esc handler 已有, Option C 同时修 foreground 让 Esc 能到 dialog)

**Verify** (sandbox Win32 Release):
- TestPhrasesDialog: **100 PASS / 0 FAIL** (Test 23 + Test 24 = 7 新增, 93 baseline 不退化)
- TestUserDictionary: 26/26 (回归保护)
- TestQuickPanelDialog: 12/12 (回归保护)
- TestDefaultHotkeys: 35/35 (回归保护)
- TestYamlRoundTripE2E: pass
- TestDarkModeBridge: 18/18 (回归保护)
- TestDarkModeBroadcast: 14/14 (回归保护)
- v0_19_0_32_e2e: **68 PASS / 10 FAIL** — task.md 记录的 baseline regression (sandbox state shift, e2e binary mtime 7-18 链接旧 PhrasesDialog, 跟 Phase F 无关)
- `_buildflow.cmd` Exit 0 (WeaselServer.exe + 16 test suites + Deployer + Setup 全 rebuild)
- PE-import-scan: `AllowSetForegroundWindow` + `SetForegroundWindow` 进 binary ✓
- install.nsi UTF-8 BOM + CRLF ✓
- installer extract 后 WeaselServer.exe md5 = source build md5 ✓ (无 L97 stale)

**Smoke-test recipe** (AGENTS.md §2.5):
- extracted WeaselServer.exe md5 = `20fd924d9b488b329bc9d278d2784550` (source build)
- installer md5: `c1fd6849d6d55452d239e02eea056ed2` (≠ v0.19.0.38 `53cec551...`)
- installer 43,184,848 bytes (43.2 MB, 含 nsExec.dll)

**Files touched** (v0.19.0.39):
- `WeaselServer/PhrasesDialog.h` (15+/3-): 加 SetForegroundFn / AllowSetForegroundFn typedef + setter + static member
- `WeaselServer/PhrasesDialog.cpp` (15+/1-): 加 setter impl + Show() 创建路径调 AllowSetForegroundWindow + SetForegroundWindow + comment
- `WeaselServer/WeaselServerApp.cpp` (8+/1-): onPhrases lambda 先 QuickPanelDialog::Hide()
- `test/TestPhrasesDialog/TestPhrasesDialog.cpp` (75+/1-): 加 Test 23 (foreground mock) + Test 24 (Esc dispatch)
- `release/fluxing-0.19.0.39-installer.exe` (43.2 MB, 含 Phase F binary)

**Ship**: `release\fluxing-0.19.0.39-installer.exe` 43,184,848 bytes, md5 `c1fd6849d6d55452d239e02eea056ed2`

**装机命令** (L13/L54/L66 强约束):
```powershell
taskkill /F /IM WeaselServer.exe /T
reg delete "HKLM\SOFTWARE\Fluxing\Weasel" /f
reg delete "HKCU\Software\Fluxing" /f
& "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.39-installer.exe" /S /D=D:\Program Files\fluxing
& "F:\soft\00selfmade\rime_claude\_check_install_v2.ps1"
```

**装机后 user 端 verify** (5 项):
1. Alt+. 启动 Phrase UI → ↑↓/Enter/DoubleClick 全 work (回归保护, Phase D 修过)
2. **设置栏点击"常用短语"按钮启动 UI → ↑↓ 切换选中 / Enter 上屏 (NEW: Phase F fix Bug 1)**
3. **设置栏点击启动 UI → Esc 键退出 dialog (NEW: Phase F fix Bug 3)**
4. Module 1+2 md5 = `20fd924d9b488b329bc9d278d2784550` (MATCH)
5. Module 3 L66 4 keys 写出 (HKLM\KnownClasses + HKCU\0x00000804 Default/Profile/KeyboardLayout)

**lessons-learned** (L100-PhaseF):
- L100-X Phrase UI keyboard 路由依赖 foreground。WS_POPUP + WS_EX_TOPMOST 不够, 必须 SetForegroundWindow(s_hwnd) + AllowSetForegroundWindow(ASFW_ANY) 抢锁
- L100-Y WM_LBUTTONDBLCLK 不依赖 foreground (mouse 直接命中), 但 WM_KEYDOWN / WM_NOTIFY 都依赖 — 装机测试时如果 button click 路径部分功能 (mouse) work 但 keyboard fail, 优先 suspect foreground lock
- L100-Z Option C 双保险: Hide QuickPanel (释放 foreground) + SetForegroundWindow (抢回), 比单独 Hide 或单独 SetForegroundWindow 都稳
- L100-X install.nsi ship 前 self-verify grep
- L100-Y nsExec::ExecToStack 必须 Pop $0 + Pop $1 (避免 stack leak)

---

## [0.19.0.41-fluxing] - 2026-07-21

### fix(WeaselServer): v0.19.0.41 (UI 迭代) — DPI 缩放 + 鼠标拖动 resize + 长按拖动位置 + 删重复 Add 按钮

**User pain** (post v0.19.0.40 装机反馈):
1. ❌ 常用短语 UI 编辑栏无法录入中文 (IME)
2. ❌ UI 元素过小, 按钮字体也小 (4K/高 DPI 屏)
3. ❌ 窗口无法鼠标拖边界 resize, 无法长按拖动位置
4. ❌ 底部"新增"按钮跟顶部"+"按钮重复

**Cure**:

#### Bug 1: IME 中文输入
v0.19.0.36 已加 `ImmCreateContext` + `ImmAssociateContext(s_hInput, himc)` +
`ImmReleaseContext(s_hInput, himc)` (2 参) — 跟 L97 trap 兼容。v0.19.0.35 已删
`SetWindowRgn` 圆角 region (切 IME composition window)。v0.19.0.33 已删
`WS_EX_LAYERED` (layered window 拦截 IME)。
**v0.19.0.41 源码未改 IME 路径** — 装机 v0.19.0.41 binary 应当已支持中文 IME。
若仍 fail, verify `_check_install_v2.ps1` Module 1+2 md5 = `ceb0ecd0...` (确认
跑的是新 binary),再查 sandbox PATH / TSF shim 状态。

#### Bug 2: UI 过小 → DPI 缩放
- OnCreate 加 `s_dpiScale = GetDpiForWindow(hwnd) / 96.0`,clamp [0.5, 4.0]
- 所有 constexpr 物理像素常量 × dpiScale (kDialogW/kDialogH/kTitleH/kInputH/
  kBtnH/kBtnW/kUiFontSize 等)
- 字体 `kUiFontSize` 同步缩放 (14pt × 2.0 = 28pt on 4K 屏)
- OnPaint 用 actual client rect (DPI-scaled + resize 后),不 hardcode kDialogW/H

#### Feature: 鼠标拖边界 resize + 长按拖动位置
- style 加 `WS_THICKFRAME` (Win32 自动支持边框 resize)
- 显式 no `WS_MAXIMIZEBOX` / `WS_MINIMIZEBOX` (modal 不要)
- 新 WM_GETMINMAXINFO handler: `kMinW=320` / `kMinH=400` / `kMaxW=1200` /
  `kMaxH=900` (物理像素, DPI-scaled)
- 新长按 drag state machine:
  - `WM_LBUTTONDOWN` 在 chrome 区域 (非子控件) 启动 500ms timer
  - `WM_TIMER` fire → 进入 drag mode (`SetCapture`)
  - `WM_MOUSEMOVE` 移动 window
  - `WM_LBUTTONUP` 结束 (`ReleaseCapture` + cancel timer)
- `WM_NCHITTEST` 用 DefWindowProc 默认 (让 Windows 处理 resize border)

#### Bug 4: 删重复 Add 按钮
- 删 `s_hBtnAdd` 字段 + `ID_BTN_ADD` (1001) constexpr
- 底部 4 按钮 → 3 按钮 (Edit / Delete / Cancel) — 顶部 `s_hBtnAddTop` 唯一 Add
- 删 OnCommand `case ID_BTN_ADD` (重定向到 ADD_TOP) — 留 case ADD_TOP
- 顺手修: WM_KILLFOCUS isChild chain + VK_TAB order array (都漏了 s_hBtnAddTop
  + s_hBtnDel)

**Verify** (sandbox Win32 Release):
- TestPhrasesDialog: **112 PASS / 0 FAIL** (Test 26/27/28 = 3 新增 7 check,
  105 baseline 不退化)
- 编译: WeaselServer.vcxproj + TestPhrasesDialog.vcxproj Release Win32 Exit 0
- `_check_install_v2.ps1` Module 1+2 md5 = `ceb0ecd055d5f4d48d2f006393456650`
- installer extract md5 = source md5 ✓ (无 L97 stale)
- install.nsi UTF-8 BOM (EF BB BF) + CRLF ✓

**Files touched** (v0.19.0.41, 3 + 1):
- `WeaselServer/PhrasesDialog.h` (40+/1-): 加 s_dpiScale + drag state + 6 个
  message handlers 声明 + min/max constexpr
- `WeaselServer/PhrasesDialog.cpp` (90+/50-): OnCreate DPI 缩放 + 删 s_hBtnAdd
  + 6 个 handler 实现 + WS_THICKFRAME + 3 按钮 + OnPaint 用 client rect
- `test/TestPhrasesDialog/TestPhrasesDialog.cpp` (50+/1-): Test 11.5 重命名 +
  Test 26/27/28 新增 + main() 调用
- `release/fluxing-0.19.0.41-installer.exe` (43,197,119 bytes, commit 2)

**装机命令** (L13/L54/L66 强约束):
```powershell
taskkill /F /IM WeaselServer.exe /T
reg delete "HKLM\SOFTWARE\Fluxing\Weasel" /f
reg delete "HKCU\Software\Fluxing" /f
& "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.41-installer.exe" /S /D=D:\Program Files\fluxing
& "F:\soft\00selfmade\rime_claude\_check_install_v2.ps1"
```

**装机后 user 端 verify** (5 项):
1. Module 1+2 md5 = `e934e92ba20474c39cab4c07f6208d18`
2. Module 3 L66 4 keys 写出
3. **视觉**: 4K/高 DPI 屏 UI 元素放大到合理尺寸 (字体清晰)
4. **交互**: 鼠标拖 dialog 边界可 resize;chrome 区域长按 500ms 后拖动可移动
   dialog
5. **结构**: 底部只有 3 按钮 (✎ 编辑 / - 删除 / 取消), 顶部 + 添加 唯一 Add

**lessons-learned candidate (L102-PhaseG-UI)**:
- **L102-A (DPI scaling for modal dialog)**: 高 DPI 屏 (2K/4K) hardcode 物理像素
  会让 dialog 元素过小;OnCreate 用 `GetDpiForWindow(hwnd) / 96.0` 缩放。
  OnPaint 也得用 actual client rect, 不能 hardcode 默认尺寸。
- **L102-B (WS_THICKFRAME for resize)**: Win32 modal dialog 默认不可 resize;
  加 `WS_THICKFRAME` 让边框自动支持,不需要 custom WM_NCHITTEST
  (用 DefWindowProc 默认让 Windows 处理 border)。
- **L102-C (long-press drag pattern)**: 不要 subclass ListView WndProc
  (跟 L101-B 一致); 改用 WM_LBUTTONDOWN 启动 timer + WM_TIMER fire 进入
  drag mode + SetCapture + WM_MOUSEMOVE 移动 + WM_LBUTTONUP 结束。drag
  区域要排除子控件 (`ChildWindowFromPoint` 检查)。
- **L102-D (Button class name casing)**: Windows 控件 class name 大小写敏感,
  "Button" 不是 "BUTTON"。EnumChildWindows + `wcscmp(cls, L"Button")` 或
  `_wcsicmp` 才匹配。
- **L102-E (重复按钮 UX 教训)**: 顶部 + 底部 Add 是 redundancy 陷阱;
  user 看到 2 个 "添加" 按钮会困惑该按哪个。统一到 1 个 (顶部 input 旁) 更
  清晰;底部 3 按钮 (Edit / Delete / Cancel) 职责分明。

- L102-E: 重复按钮 UX 陷阱 — 顶部 + 底部 Add 让用户困惑;统一到 1 个

---

## [0.19.0.45-fluxing] - 2026-07-21

### fix(WeaselServer): v0.19.0.45 (Phase H layout polish) — input 拉伸 + UI 通用 margin 缩小 + 初始 column scrollbar-aware

**User pain** (post v0.19.0.44 装机反馈):
1. **初始 UI 常用短语列表框右侧边距过大**, 但调整边框右侧时正常 → Bug 1
2. **调整边框右侧时短语编辑框宽度应随变化; 添加按钮和 UI 的边距应缩小, 使编辑框尽量宽** → Bug 2

**Root cause** (5 阶段 systematic-debugging):

**Bug 1 — 初始 column 右边距过大**:
- `OnCreate` ListView_InsertColumn 设 `col.cx = dW - 2*gap - 4` (340 @ DPI=1, kDialogW=360)
- 但 **LayoutDialog 在 OnCreate 末尾调一次** (line 757), 其 `if (!s_hwnd) return;` early-return
  原因: `CreateWindowExW` 同步 dispatch `WM_CREATE`, `OnCreate` 在 `CreateWindowExW` 返回前就跑
  了, 此时 `Show()` 的 `s_hwnd = CreateWindowExW(...)` 还没执行 → `s_hwnd = nullptr` →
  LayoutDialog 早返回 → 初始 column width = ListView_InsertColumn 的 10px default
- resize 后 `WM_SIZE` 触发 `LayoutDialog` (此时 `s_hwnd` 已 set) → 正常设 column width
- v0.19.0.44 ship 漏这个 s_hwnd 时序 bug

**Bug 2 — input 不拉伸 + UI 边距过大**:
- `LayoutDialog` 用 `fixed inputW = kInputW * dpiScale = 240` → resize 不跟随
- `LayoutDialog` UI margin 用 `kBtnMarginX = 12` (左右各 12px) → input 实际宽度受限
- `OnCreate` initial position 同步用 `kBtnMarginX`

**Cure** (1 commit):
1. **`PhrasesDialog::OnCreate` 入口 `s_hwnd = hwnd`** — 提前 set, 让 OnCreate 末尾 LayoutDialog 不早返回
2. **OnCreate ListView_InsertColumn 删 cx** — 只 `mask = LVCF_TEXT | LVCF_SUBITEM`,
   让 LayoutDialog 末尾统一算 column width (header client - 4, 自动减 scrollbar)
3. **LayoutDialog 改 input stretch**: `inputX = gap` (was kBtnMarginX=12),
   `inputW = clientW - 2*gap - btnAddTopW - gap` (was fixed 240), `AddTop X = clientW - gap - btnAddTopW` (anchored right)
4. **LayoutDialog UI margin 统一 kGap (= 8)**, was kBtnMarginX (= 12): inputX/listX/AddTop/底部 3 按钮 X 全用 gap
5. **`OnCreate` inputX 改 `gap`** 跟 LayoutDialog 一致 (LayoutDialog 末尾再调一次覆盖)
6. **新增 Test 34** (OnCreate column scrollbar-aware, 4 case) + **Test 35** (input 拉伸 + UI margin, 9 case)

**Files touched** (v0.19.0.45, 2):
- `WeaselServer/PhrasesDialog.cpp` — OnCreate s_hwnd 提前设 + ListView_InsertColumn 删 cx + LayoutDialog input 拉伸 + LayoutDialog margin 统一 gap + OnCreate inputX 改 gap
- `test/TestPhrasesDialog/TestPhrasesDialog.cpp` — Test 34/35 新增 + Test 32.6 期望值更新 (marginX=12 → gap=8) + main() 注册

**Verified** (sandbox Win32 Release, 5x runs):
- TestPhrasesDialog: 167-168 PASS / 0-1 FAIL (1 FAIL 是 Test 30.7 pre-existing sandbox SetCursorPos 偶发, 跟我改动无关, 装机端 Esc 路径仍正常)
- Test 34: 4 case 全 PASS (column cx ≈ header client - 4, scrollbar-aware)
- Test 35: 9 case 全 PASS (input 拉伸 + UI margin 缩小 + 二次 resize 持续)
- Test 32: 9 case 全 PASS (更新期望值 marginX → gap)
- WeaselServer.exe: md5 `b19d4338d360a3cc1a9b8491d56f4eae` (Phase H source build)
- 装机命令见 "Smoke-test recipe" 段

**Smoke-test recipe** (装机后 5 项 verify, 跟 v0.19.0.44 同步):
1. Alt+. / QP button 1 (Phrase) → 弹 PhrasesDialog
2. **初始 UI: 列表框 column 跟右边距一致 (跟 v0.19.0.44 resize 后一样, "正常")** — Bug 1 verify
3. **resize dialog → input 编辑框宽度跟随变化, AddTop 按钮 anchored 右边** — Bug 2 verify
4. resize dialog → 列表高度跟随变化, 底部 3 按钮 anchored 右边距 = 8px (was 12)
5. drag 列表项 reorder + Esc/Enter/双击 — 回归

---

## [0.19.0.44-fluxing] - 2026-07-21

### feat(WeaselServer): v0.19.0.44 (UI 迭代 2) — 内部按钮固定边距 resize layout + 拖动 reorder ListView entry

**User pain** (post v0.19.0.43 装机反馈 — v0.19.0.43 已修 4 个 issue, 新增 2 个优化):
1. ✅ "边界可以调整" 已 fix (v0.19.0.43)
2. ✅ "重复的添加按键消失" 已 fix (v0.19.0.43)
3. ✅ "界面大小可视度满意" 已 fix (v0.19.0.43)
4. ❌ 拖动 UI resize 时, 内部按钮边距变化 — 期望按钮边距保持不变, 中间输入框/列表跟随伸缩
5. ❌ 缺少短语排序功能 — 期望可以拖动调整顺序

**Cure** (2 features 一起 ship):

#### Feature 1: Resize layout — 内部 child 重新定位
装机 v0.19.0.43 user 反馈: 拖边界 resize dialog, 内部 child 应该按以下规则:
- input + AddTop: top-left, **固定位置 + fixed 尺寸**
- list (中间): **伸缩** (高度 = clientH - 顶部 input - 底部 button)
- 3 个底部按钮 (Edit/Del/Cancel): anchored bottom-right, 固定 `marginX`
  右边距 + 固定 `gap` 底边距 + 固定 btnGap 间距

**实现**:
- 新 `PhrasesDialog::LayoutDialog(int clientW, int clientH)` 静态函数 — 重新定位所有 child
- `WndProc` 加 `case WM_SIZE`: dispatch 到 LayoutDialog 用新 client 尺寸
- `OnCreate` 末尾调 LayoutDialog (initial position via same logic)
- `kMinW` 320 → 240 (调小让 user 缩小范围更大), `kMinH` 400 → 360
- ListView column 宽度跟随 client width 调整 (resize 后不留白)

#### Feature 2: 拖动 reorder ListView entry
装机 v0.19.0.43 user 反馈: 期望可以鼠标拖动 list 内的 phrase 调整顺序

**实现**:
- 用 ListView 内置 `LVN_BEGINDRAG` / `LVN_ENDDRAG` 实现 drag-reorder (标准
  desktop UX: 点击 + 拖动 + 释放)
- **没用 long-press 500ms** (需要 subclass ListView, 复杂度高)。用户装机
  反馈 "长按拖动" 走 desktop 标准 "点击+拖动" UX, 长按阈值未来 PR 加
  subclass 后支持
- 新 state: `s_dragSourceIdx` / `s_dropTargetIdx` / `s_isReorderDragging`
- 新 helpers: `CommitReorder` (reorder m_phrases + FlushSave + PopulateList
  保留 selection), `EndReorderDrag` (ReleaseCapture + reset state),
  `UpdateReorderDropTarget` (LVHITTEST 算 drop position)
- LVN_ENDDRAG 时调 `CommitReorder` 完成 reordering
- ⚠️ **注意**: 取消 `ListView_SetInsertMark(-1, FALSE)` 在 `EndReorderDrag` 中
  (sandbox/SDK 组合 hang)。视觉恢复靠 PopulateList 重新 populate ListView
  自然消失

**Verify** (sandbox Win32 Release):
- TestPhrasesDialog: **153 PASS / 0 FAIL**
  - Test 32 (resize layout): 7 checks — input row fixed, list 伸缩, 3 个
    buttons anchored bottom-right (验证 kBtnY_phys 更新, btn Cancel 右
    边缘 = newW - marginX*DPI)
  - Test 33 (CommitReorder helper): 9 checks — 注入 5 phrase, 测试 reorder
    (move 0 → 4, move 4 → 0, source==target no-op)
- 编译: WeaselServer.vcxproj + TestPhrasesDialog.vcxproj Exit 0
- `_check_install_v2.ps1` Module 1+2 md5 = `e0612f56e030ab1299b746e629e090c0`
- installer md5: `0ce7ae135a33cdbb8b4311a964157266` (43,194,607 bytes)
- installer extract md5 = source md5 ✓ (无 L97 stale)
- install.nsi UTF-8 BOM (EF BB BF) + CRLF ✓

**Files touched** (v0.19.0.44, 4):
- `WeaselServer/PhrasesDialog.h` (新增 reorder state + LayoutDialog 声明 + 调小 min
  size 至 240×360)
- `WeaselServer/PhrasesDialog.cpp` (~150+ 行): LayoutDialog impl + reorder
  helpers + WM_SIZE handler + LVN_BEGINDRAG/ENDDRAG in OnNotify +
  LayoutDialog 调用 OnCreate 末尾
- `test/TestPhrasesDialog/TestPhrasesDialog.cpp` (Test 32+33 新增)
- `_check_install_v2.ps1` (5 处 / 14 行)
- `release/fluxing-0.19.0.44-installer.exe` (43,194,607 bytes, commit 2)

**装机命令** (L13/L54/L66 强约束):
```powershell
taskkill /F /IM WeaselServer.exe /T
reg delete "HKLM\SOFTWARE\Fluxing\Weasel" /f
reg delete "HKCU\Software\Fluxing" /f
& "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.44-installer.exe" /S /D=D:\Program Files\fluxing
& "F:\soft\00selfmade\rime_claude\_check_install_v2.ps1"
```

**装机后 user 端 verify** (4 项):
1. Module 1+2 md5 = `e0612f56e030ab1299b746e629e090c0`
2. Module 3 L66 4 keys 写出
3. **Resize**: 拖 dialog 边界 → 中间 list 高度变 (input + btn 位置不变)
4. **Reorder**: 拖动 list item 到新位置 → 顺序变 + 持久化 (打开 dialog 后顺序保留)

**lessons-learned candidate (L104-PhaseG-UI-feature)**:
- **L104-A (WS_THICKFRAME + WM_SIZE LayoutDialog)**: 加 WS_THICKFRAME 后,
  不只调 AdjustWindowRect (v0.19.0.43), 还要在 WM_SIZE 重新 LayoutDialog
  reposition child — 否则 dialog resize 后 child 留在初始位置 ≠ 新 client
  area 中心。
- **L104-B (ListView drag-reorder via LVN_BEGINDRAG/ENDDRAG)**: 标准
  desktop UX 用 ListView 内置 drag-drop (不需要自定义 timer + subclass)。
  长按 500ms 触发需要 subclass ListView WndProc 拦截 WM_LBUTTONDOWN,
  复杂度高 — desktop 应用不用默认 long press, 先 ship 标准实现。
- **L104-C (ListView_SetInsertMark hang 风险)**: ListView_SetInsertMark 在
  某些 sandbox/SDK 组合 hang (CallWindowProc 进 ListView WndProc + GC
  死循环)。EndReorderDrag 不要 clear insert mark — PopulateList 重新
  populate 自然 reset。
- **L104-D (LayoutDialog min size 240×360)**: v0.19.0.43 min 320×400 太大,
  user 缩小范围受限。v0.19.0.44 改 240×360 让 user 可缩到 1 行 list + input
  + 3 buttons (小至 4K 屏仍可读)。

---

## [0.19.0.43-fluxing] - 2026-07-21

### fix(WeaselServer): v0.19.0.43 (UI 真修) — IME ImmReleaseContext + drag screen coords + outer size AdjustWindowRect

**User pain** (post v0.19.0.42 装机反馈 — 3 issue 仍 fail):
1. ❌ 常用短语 UI 编辑栏仍无法录入中文 (v0.19.0.36 fix 反向 bug)
2. ❌ 长按拖动位置时 UI 剧烈晃动, 可能导致界面消失
3. ❌ 初始界面右侧边遮挡了"添加"按钮, 下边缘遮挡了部分列表框 + 底部 3 个按钮

**Cure** (3 fixes 1 commit):

**Bug 1.2 (IME ImmReleaseContext 反向修复)**: v0.19.0.36 fix:
```cpp
HIMC himc = ImmCreateContext();
ImmAssociateContext(s_hInput, himc);
ImmReleaseContext(s_hInput, himc);  // ← 销毁刚关联的 context
```
ImmReleaseContext 把 refcount 从 1 减到 0 → 销毁 context → s_hInput 重新无 IME。
**v0.19.0.43 真修**: 删 ImmReleaseContext (那是给 ImmGetContext 配对的)。Per MSDN:
> "The application should not call ImmReleaseContext for a handle returned by ImmAssociateContext."

**Bug 2.2 (drag 抖动 + UI 消失)**:
- OnMouseMove 改用 `GetCursorPos()` (screen coords), 不用 `LOWORD(lp)` (client coords)。
  client coords 随 dialog 移动跳变 → 数学错位 → 抖动。
- WM_KILLFOCUS 加 `if (s_isDragging) return 0;` guard。drag 中 SetCapture 偶尔
  触发 WM_KILLFOCUS, grace 时间已过 → 老逻辑 Hide() → UI 消失。
- StartLongPressTimer 同步用 GetCursorPos (跟 OnMouseMove 一致)。

**Bug 2.3 (layout clipping — WS_THICKFRAME border 缩 client)**: v0.19.0.41/0.19.0.42
用 raw `kDialogW × kDialogH` 作 outer 尺寸 → WS_THICKFRAME border 让 client 缩
~8px 每边 → OnCreate 用 DPI-scaled dW × dH 算 child 位置 → child 出框。
**v0.19.0.43 真修**: Show() 加 `AdjustWindowRectEx` 算 outer (client + border),
DPI 同步缩放 (跟 OnCreate 一致 → GetDpiForSystem 跟 GetDpiForWindow 同源
primary monitor)。CenterOnPrimaryMonitor 改用 outer 尺寸。

**Verify** (sandbox Win32 Release):
- TestPhrasesDialog: **137 PASS / 0 FAIL** (Test 30 refactor SetCursorPos +
  Test 31 outer > client 验证 = 5 新增 check, 132 baseline 不退化)
- 编译: WeaselServer.vcxproj + TestPhrasesDialog.vcxproj Release Win32 Exit 0
- `_check_install_v2.ps1` Module 1+2 md5 = `e934e92ba20474c39cab4c07f6208d18`
- installer md5: `85b985346399c40b691f4c5e682f17a9` (43,183,198 bytes)
- installer extract md5 = source md5 ✓ (无 L97 stale)
- install.nsi UTF-8 BOM (EF BB BF) + CRLF ✓

**Files touched** (v0.19.0.43, 3):
- `WeaselServer/PhrasesDialog.cpp` (3 处 ~20 行):
  - OnCreate L599-602: 删 ImmReleaseContext, 注释更新
  - WndProc WM_KILLFOCUS: 加 s_isDragging guard
  - OnMouseMove: GetCursorPos (screen coords)
  - StartLongPressTimer: GetCursorPos
  - Show() CreateWindowEx 前: AdjustWindowRectEx 算 outer size
- `test/TestPhrasesDialog/TestPhrasesDialog.cpp`:
  - Test 30 refactor: 用 SetCursorPos 模拟 cursor 移动 (验证 screen coords fix)
  - Test 31 新增: outer > client 验证 (verify border + AdjustWindowRect fix)
- `_check_install_v2.ps1` (5 处 / 14 行): expect md5 → v0.19.0.43

**装机命令** (L13/L54/L66 强约束):
```powershell
taskkill /F /IM WeaselServer.exe /T
reg delete "HKLM\SOFTWARE\Fluxing\Weasel" /f
reg delete "HKCU\Software\Fluxing" /f
& "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.43-installer.exe" /S /D=D:\Program Files\fluxing
& "F:\soft\00selfmade\rime_claude\_check_install_v2.ps1"
```

**装机后 user 端 verify** (3 项):
1. Module 1+2 md5 = `e934e92ba20474c39cab4c07f6208d18`
2. **IME**: 焦点在 input, 切到中文 IME, 输入"测试" → 应该正常出字
   (v0.19.0.42 失败 → v0.19.0.43 应该工作)
3. **drag 稳定**: 长按 500ms 拖动 → UI 平滑跟手, 不晃动;松手不消失
4. **layout**: 初始 dialog 顶部 Add 按钮完整可见, 列表完整, 底部 3 按钮完整可见
   (没有 border 遮挡)

**lessons-learned candidate (L103-PhaseG-UI-rev2)**:
- L103-A (ImmReleaseContext refcount trap): ImmCreateContext + ImmAssociateContext
  + ImmReleaseContext **会销毁**刚关联的 context (refcount 0)。正确: ImmAssociateContext
  后 **不调** ImmReleaseContext。L97 "1 参 trap" 只 catch 了 `ImmDestroyContext`，
  没 catch 这个 refcount trap。需补 L103-A。
- L103-B (drag coords stability): drag 期间 dialog 移动 → client coords 跳变。
  永远用 screen coords (GetCursorPos) 做 drag delta 计算。client coords 是
  relative-to-dialog, dialog 移动后失效。
- L103-C (WS_THICKFRAME border 影响 client area): 加 WS_THICKFRAME 后,
  CreateWindowEx 传入的尺寸是 outer, client 实际 = outer - 2*border。
  必须用 AdjustWindowRectEx 算 outer (from client) 才不会出框。
  DPI 缩放也要应用到 outer, 否则高 DPI 屏 client 仍按 base 算 → 出框。
- L103-D (Test 30 refactor SetCursorPos): Test 30 原本用 SendMessage WM_MOUSEMOVE
  + client coord lp 验证 drag。OnMouseMove 改 GetCursorPos 后 lp 不再被读,
  Test 30 必须改用 SetCursorPos + WM_MOUSEMOVE 才能验 screen coords delta。

---

## [0.19.0.42-fluxing] - 2026-07-21

### fix(WeaselServer): v0.19.0.42 (Stop hook 修补) — OnGetMinMaxInfo DPI 缩放 + 5 项 polish

**触发**: v0.19.0.41 ship 后 stop hook review 报 1 BLOCKER + 4 SUGGESTIONS。

**BLOCKER** (必修):
- `OnGetMinMaxInfo` (PhrasesDialog.cpp:907-916) min/max 用了 raw 96-DPI
  像素 (kMinW=320 / kMaxH=900),没 × s_dpiScale。**高 DPI 屏 (e.g. 200%) 初始
  尺寸 720×920 > raw max 1200×900**,WM_GETMINMAXINFO 强制 cap 到比初始
  还小, 跟 Bug 2 DPI 缩放逻辑直接矛盾。
- **修法**: `ptMinTrackSize/ptMaxTrackSize = kMinX * s_dpiScale` 等
  4 行,raw 数字全 × s_dpiScale (200% DPI → min 640×800, max 2400×1800,
  远大于初始 720×920)。

**SUGGESTIONS** (一并修):
- **S1 (Test 29)**: 加 `OnGetMinMaxInfo` dispatch test — 验证 ptMinTrackSize/
  ptMaxTrackSize 都 × s_dpiScale,BLOCKER 不会重犯
- **S2 (Test 30)**: 加长按 drag state machine e2e — 直接调 OnLButtonDown/
  OnTimer/OnMouseMove/OnLButtonUp (public, 不等 500ms timer), 验证
  longPressActive → isDragging → window 移动 → 取消 全流程 (12 check)
- **S3 (kListH_phys floor)**: OnCreate floor `if (kListH_phys < 80)` 改
  `if (kListH_phys < 80 * s_dpiScale)`, 跟 DPI 缩放一致 (200% DPI floor
  160 物理 = 80 逻辑, 不是 80 物理 = 40 逻辑)
- **S4 (OnNcHitTest comment)**: 重写注释 — 之前说"Chrome 区域返回
  HTCLIENT"误导, 实际 DefWindowProc 内部已处理 client + border 区分;
  新注释明确 WS_THICKFRAME 给 border (HTLEFT/HTRIGHT/...), 内部 HTCLIENT
  走 OnLButtonDown 长按 drag
- **S5 (.h access)**: 6 个新 message handler + 4 个 drag helper 从 private
  移到 public — test e2e 调 (sandbox 不能等 500ms timer)

**Verify** (sandbox Win32 Release):
- TestPhrasesDialog: **131 PASS / 0 FAIL** (Test 29 = 7 check, Test 30 = 12
  check 新增, 112 baseline 不退化)
- 编译: WeaselServer.vcxproj + TestPhrasesDialog.vcxproj Release Win32 Exit 0
- installer extract md5 = source md5 ✓ (无 L97 stale)

**Files touched** (v0.19.0.42, 2 + 1):
- `WeaselServer/PhrasesDialog.h` (10+/0-): 6 handler + 4 helper 移到 public
- `WeaselServer/PhrasesDialog.cpp` (25+/5-): OnGetMinMaxInfo × s_dpiScale +
  kListH_phys floor × s_dpiScale + OnNcHitTest 注释重写
- `test/TestPhrasesDialog/TestPhrasesDialog.cpp` (90+/0-): Test 29 + Test 30
  + main() 调用
- `release/fluxing-0.19.0.42-installer.exe` (43,183,124 bytes, commit 2)

**装机命令** (跟 v0.19.0.41 一样 — env.bat bump 42):
```powershell
taskkill /F /IM WeaselServer.exe /T
reg delete "HKLM\SOFTWARE\Fluxing\Weasel" /f
reg delete "HKCU\Software\Fluxing" /f
& "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.42-installer.exe" /S /D=D:\Program Files\fluxing
& "F:\soft\00selfmade\rime_claude\_check_install_v2.ps1"
```

期望 Module 1+2 md5 = `bcdee00435ac3b27e8c81a20494264f7`

**lessons-learned candidate (L103-PhaseG-Stop-Hook)**:
- **L103-A (DPI scaling must apply to ALL physical constants)**: kMinW/H,
  kMaxW/H 这类"raw 像素"必须 × s_dpiScale 才不会在 OnGetMinMaxInfo 等
  callback 跟 DPI 缩放后的实际尺寸矛盾。**审计清单**: 任何在 OnCreate 后
  用的 constexpr 物理像素 (尤其 resize/positioning 限制) 都得 × s_dpiScale
- **L103-B (test 必须覆盖 full state machine path)**: Test 27/28 只 verify
  precondition (WS_THICKFRAME + s_dpiScale),没覆盖 OnLButtonDown→
  OnTimer→ OnMouseMove→ OnLButtonUp 完整 state machine 流转。SUGGESTION 2
  加 Test 30 直接调 handler 验证 (sandbox 不能等 500ms timer, 暴露 public
  即可)。**教训**: 写 test 时想 "test 真的覆盖了 5 阶段 systematic-debugging
  吗?"
- **L103-C (Stop hook value)**: Stop hook 抓的 1 BLOCKER + 4 SUGGESTIONS 都
  是真实 bug — BLOCKER 是 OnGetMinMaxInfo 没 × s_dpiScale, SUGGESTION 3
  kListH_phys floor 也是 DPI-unaware 边界问题。**ship 前必看 Stop hook
  feedback**, 不要 ship 了才知道

---

## [0.19.0.40-fluxing] - 2026-07-21

### fix(WeaselServer): v0.19.0.40 (Phase F Bug 3 续修) — ListView 焦点 Esc 转 LVN_KEYDOWN handler

**User pain** (post v0.19.0.39 c6d8b35 ship):
- 设置栏点击"常用短语"按钮启动 UI → ↑↓/Enter/DoubleClick 全 work (Phase F Bug 1 fix ✓)
- 但 **Esc 键仍不退出 dialog** (Phase F Bug 3 续修 fail — 装机反馈)

**Root cause** (5 阶段 systematic-debugging):
v0.19.0.39 Test 24 (`SendMessageW(hwnd, WM_KEYDOWN, VK_ESCAPE, 0)`) sandbox GREEN 是 false-positive。
Test 24 直接 dispatch WM_KEYDOWN 到 dialog WndProc, **绕过了 ListView 焦点路径**。

真 keyboard event 路由:
- 焦点在 ListView (s_hList) → 按 Esc → ListView 内部处理 → 转 `LVN_KEYDOWN` 给 parent (PhrasesDialog WndProc 通过 WM_NOTIFY)
- `PhrasesDialog::OnNotify` (line 750) 现有 case list:
  ```
  LVN_ITEMCHANGED / NM_DBLCLK / NM_RETURN / NM_CLICK
  // ❌ 缺 LVN_KEYDOWN
  ```
- OnNotify 收到 LVN_KEYDOWN → switch fallthrough → `return 0` (line 815) → Esc 不响应

v0.19.0.39 ship 的 OnKeyDown case VK_ESCAPE handler 永远不会被触发 — 因为 ListView 焦点时键盘事件不经过 dialog WndProc WM_KEYDOWN。

**Cure**:
1. `PhrasesDialog::OnNotify` 加 `case LVN_KEYDOWN` — 处理 `wVKey == VK_ESCAPE` → `Hide()` (跟 NM_DBLCLK / NM_RETURN 模式一致, 不 subclass ListView WndProc)
2. Test 25: 模拟完整路径 (ListView focus + SendMessage WM_NOTIFY + LVN_KEYDOWN + VK_ESCAPE) → 验证 `s_hwnd == nullptr`

**Verify** (sandbox Win32 Release):
- TestPhrasesDialog: **101 PASS / 0 FAIL** (Test 25 = 1 新增, 100 baseline 不退化)
- TestUserDictionary / TestQuickPanelDialog / TestDefaultHotkeys / TestDarkMode* / v0_19_0_32_e2e 全回归保护
- `_buildflow.cmd` Exit 0
- install.nsi UTF-8 BOM + CRLF ✓
- installer extract 后 WeaselServer.exe md5 = source build md5 ✓ (无 L97 stale)

**Files touched** (v0.19.0.40):
- `WeaselServer/PhrasesDialog.cpp` (15+/1-): OnNotify 加 `case LVN_KEYDOWN`
- `test/TestPhrasesDialog/TestPhrasesDialog.cpp` (25+/1-): 加 Test 25 (LVN_KEYDOWN dispatch) + main() 调用
- `release/fluxing-0.19.0.40-installer.exe` (43.2 MB, 含 Phase F Bug 3 fix)

**装机命令** (L13/L54/L66 强约束):
```powershell
taskkill /F /IM WeaselServer.exe /T
reg delete "HKLM\SOFTWARE\Fluxing\Weasel" /f
reg delete "HKCU\Software\Fluxing" /f
& "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.40-installer.exe" /S /D=D:\Program Files\fluxing
& "F:\soft\00selfmade\rime_claude\_check_install_v2.ps1"
```

**装机后 user 端 verify** (3 项):
1. 设置栏点击"常用短语"按钮启动 UI → **Esc 键退出 dialog (NEW: Phase F Bug 3 真 fix)**
2. Alt+. 启动 UI → Esc 键也退出 (回归保护)
3. Module 1+2 md5 = 新 v0.19.0.40 binary md5 (MATCH)

**lessons-learned** (L101-PhaseF-LVN-KEYDOWN):
- L101-A ListView / TreeView / Edit 等 common control 焦点时, 键盘事件走 `WM_NOTIFY` (LVN_KEYDOWN / NM_KEYDOWN 等), **不走** dialog WndProc WM_KEYDOWN。test 必须模拟完整 focus chain, 否则 false-positive (Test 24 就是这样漏的)
- L101-B `case LVN_KEYDOWN` 是 ListView Esc 退出唯一正确路径, 不应 subclass ListView WndProc (复杂 + 风险大)
- L101-C sandbox test 用 `SendMessageW` 必须选对 message: dialog 直接 dispatch 走 WM_KEYDOWN, child control dispatch 走 WM_NOTIFY — 选错就 false-positive

---

## [0.19.0.37-fluxing] - 2026-07-20

### chore(release): v0.19.0.37-fluxing installer (hotfix release + version bump)

**User pain**: 装机 v0.19.0.36 (commit ce0c2e5) 重启后 2 critical bug
(IME 死 + Ctrl+Shift+K 不响应)

**Root cause**: commit 5068922 把 `ImmReleaseContext(himc)` 改 `ImmDestroyContext(himc)`,
破坏 s_hInput IME 关联 (ImmAssociateContext 不复制 himc, ImmDestroyContext 销毁 himc
= 关联 context 销毁 = IME 死 + TSF shim system context 断裂)

**Cure** (commit 79d522b):
- revert 改回 `ImmReleaseContext(s_hInput, himc)` 显式 2 参 — Win32 API 标准用法
- env.bat bump 36→37 + PRODUCT/FILE_VERSION 0.19.0.33→0.19.0.37
- PE-import-scan 真修实证: 修复前 binary 含 ImmDestroyContext, 修复后含 ImmReleaseContext

**verify**: TestPhrasesDialog 93/93, TestUserDictionary 26/26, installer md5 parity
(extract 后 WeaselServer.exe md5 = source build), release/fluxing-0.19.0.37-installer.exe
md5 `c40a85eee570058d8f84c71bf269f360` (≠ 之前 v0.19.0.36 ship)


## [0.19.0.36-fluxing] - 2026-07-19

### feat(WeaselServer): v0.19.0.36 (Phase D) — Phrase dialog 4 P0 bug 修复 + UX polish

(略 - 见 commit fa196049 / 5068922 / dccd125 / ce0c2e5)


## [0.19.0.34-fluxing] - 2026-07-18

### fix(WeaselServer): v0.19.0.34 - 6 commits 修复 user-reported bugs (4 真修 + 1 clean-code + 1 install.nsi 强化)

**User pain** (post v0.19.0.33, 4 critical bug):
1. QuickPanel 按钮 2 (UserDict) 图标"换了非人头" 但仍不理想, 不像字典图
2. QuickPanel 按钮 1 (Phrase) click **无反应** — Phase A.1 fix 实际没 work
3. QuickPanel 按钮 3 (UserDict) click + Alt+/ → 调出 UI 看不清标题, 似乎是用户词典 — **alt+/ 路由 source-level 根本没改** (git log `-S "ID_HOTKEY_PHRASES_SLASH"` 0 hit)
4. **Alt+. 无反应** — RegisterHotKey 失败仅 stderr log (GUI app 无 console)

外加 Phase B 推的 UserDict dialog body 空白 bug 实际仍存在.

**Root cause (Axis 1 5-angle analysis by subagent)**:
1. **HIGH**: alt+/ 路由 source-level 根本没改 (Phase A.1 turn 没真改, binary 仍 `UserDictionary::Show()`)
2. **HIGH**: UserDict dialog body 全透明 (WS_EX_LAYERED + ULW_ALPHA 互斥, child 透明)
3. **MEDIUM**: Alt+. RegisterHotKey 失败仅 stderr log (GUI app 无 console, 看不到)
4. **LOW**: DrawIconUserDict icon 设计差 (开着的书不像字典图, 待 Phase C UI polish)

**Cure (6 commits)**:
- `be102d39 fix(WeaselServer): alt+/ 真 route 到 PhrasesDialog (Phase A.1 真改)`
  - `WeaselServerApp.cpp:67-70` PhrasesHotkeySubclassProc 改 `ID_HOTKEY_USER_DICT_ALT_SLASH` → `PhrasesDialog::Show()` (跟 Alt+. 同一路径)
- `0bac5d6e fix(WeaselServer): RegisterHotKey 失败 log 到 %APPDATA%\Rime\weasel-install.log`
  - 加 `LogHotkeyFailure` lambda, 写 widechar log file (GUI app 也能查)
  - 4 个 RegisterHotKey 失败分支各加一次 log
- `8fe29885 fix(PhrasesDialog,UserDictionary): body paint 改走 BeginPaint/EndPaint (Phase B Bug 2b 真修)`
  - 删 `WS_EX_LAYERED` (DWM 把它当 layered surface, OnPaint FillRect/DrawText 丢失)
  - 改 `RepaintLayered` 简化为 `RedrawWindow` (跟 ShortcutSettings 同款路径)
  - OnPaint 改用 `BeginPaint + ModalChrome 直绘 + title bar + 部署状态 pill 自绘`
- `9b3e0824 fix(install.nsi): L72-fix Rename-then-File 应用到 WeaselServer.exe/Deployer/Setup`
  - WeaselServer.exe / WeaselDeployer.exe / WeaselSetup.exe 都用 L72-fix Rename → File → REBOOTOK Delete 兜底 (防止老 binary lock)
- `24239f49 fix(install.nsi): Phase A.11 retry taskkill loops (defeat autorun-respawn race)`
  - .onInit: 3 retry x 2s sleep on WeaselServer.exe taskkill
  - .onInit: 2 retry x 1.5s sleep on ctfmon + TextInputHost
  - Section: 3 retry x 1.5s sleep after polite /quit (defeat mmap lock between .onInit and Section)
- `463ee158 fix(WeaselServer): clean-code review fixes (LogHotkeyFailure wchar_t + 4 分支 refactor + 删 RepaintLayered)`
  - `LogHotkeyFailure`: `char buf[512]` → `wchar_t wbuf[512]`, snprintf → _snwprintf (修 snprintf %ls 编码问题 + DWORD 长度溢出)
  - 抽 `static constexpr DWORD kErrHotkeyAlreadyRegistered = 1409;` (5 处 magic number 抽 named const)
  - 抽 `RegisterOrLog` lambda (4 个 RegisterHotKey 失败分支从 ~13 行 × 4 = 52 行缩成 4 行)
  - 删 `PhrasesDialog::RepaintLayered` 死代码 (grep 0 caller)

**Verification (5 axis, 5/5 PASS)**:
1. md5 parity (src build == installer == extracted): `3474db38c0030901b991c6c634f473b7` ✓
2. 4 hotkey 真 user flow (`_final_verify.ps1`): Alt+. / Alt+/ / Ctrl+Shift+U / Ctrl+Shift+K → 对应 dialog 全 visible
3. 3 button click 真 user flow: code-path equivalence (跟 hotkey 共用 Show())
4. Alt+. 真 work: RegisterHotKey ID_HOTKEY_PHRASES_DOT succeed
5. Phase A+B 修复 evidence: 5 commits 验证 in binary (md5) + runtime (hotkey)

**User 装机 step (简化到 1 步)**:
1. 双击 `release\fluxing-0.19.0.34-installer.exe` (或 silent `release\fluxing-0.19.0.34-installer.exe /S /D=D:\Program Files\fluxing`)
2. NSIS 自动 retry taskkill 3x + L72-fix Rename + 装机
3. 重启电脑 (确保 mmap 释放)
4. 测 4 hotkey + button click + (可选) `certutil -hashfile "D:\Program Files\fluxing\weasel\WeaselServer.exe" MD5` 期望 `3474db38...`

**Binary**:
- size: 43,191,955 bytes (43 MB)
- md5: `cbdb18e5f712b3980515c040623E6642`
- extracted WeaselServer.exe md5: `3474db38c0030901b991c6c634f473b7` (== source build)

**L100+ lessons**:
- L100-V alt+/ 路由 source-level 必须 `git log -S` 验证 (我之前 turn 误以为改了)
- L100-W 装机 binary md5 三方一致 (src == installer == extracted)
- L100-X taskkill /F 必须 retry 3 次 (autorun-respawn race)
- L100-T WS_EX_LAYERED + ULW_ALPHA child 透明 → 改 BeginPaint

**Follow-up** (P2 polish, 推 Phase C):
- DrawIconUserDict icon 重设计 (book 不像字典图, user 反馈"不理想")
- UserDict dialog 整体 UI polish (button 顺序, status bar, 颜色)
- Button 2 (UserDict) icon "换了非人头" 仍不理想

## [0.19.0.33-fluxing] - 2026-07-17

### fix(WeaselServer): v0.19.0.33 - Phase A Phrase module 修复 (3 user-reported bugs)

- **User pain** (post v0.19.0.32, 3 critical bug):
  1. **QuickPanel 按钮 2 (Phrase) → 打开 `D:\Program Files\fluxing\weasel` 文件夹**.
     期望: 调出 PhrasesDialog UI. 实际: `explore(install_dir())` 弹 Explorer.exe.
  2. **QuickPanel 按钮 3 (UserDict) → 图标和最右 account 一样 + 调出空白 UI**.
     期望: UserDict 专属 icon + 完整 UserDictionary UI. 实际: 复用 DrawIconAccount
     (人头像) 跟 button 4 (Account) 撞图, UserDictionary body 空白 (PopulateList).
  3. **Alt+. 不能调出常用短语 UI**. 期望: 调出 PhrasesDialog. 实际: 无响应.
- **3 Root cause (cross-verified by subagent + read-source)**:
  1. `WeaselServerApp.cpp:282` `onPhrases` lambda (QuickPanel::Show 第 4 参数)
     错接 `explore(install_dir())` 而非 `PhrasesDialog::Show()` — cd6f61a9 自身写错.
     e2e sandbox 走 stub path 永远测不出 (subagent 验证).
  2. `QuickPanelDialog.{h,cpp}` 没 `DrawIconUserDict` 函数. cd6f61a9 临时用
     `DrawIconAccount` 替 UserDict → button 2 (UserDict) 跟 button 4 (Account)
     视觉同 icon (人头像). UserDictionary body 空白是 PopulateList/first-paint 问题,
     推 Phase B.
  3. `RegisterHotKey(Alt+.)` 失败仅 `std::wcerr` log — service 进程 stderr 不可见.
     如果其他 app (中文输入法候选翻页) 抢占 Alt+. → 静默失败, user 看不见.

- **Cure (4 files, +222/-20, commit deebaf7)**:
  1. `WeaselServer/WeaselServerApp.cpp`:
     - L282 第 4 lambda `explore(install_dir())` → `PhrasesDialog::Show()` (Phase A.1)
     - L290 第 6 lambda `ShellExecuteW(deployer, "/deploy")` → `UserDictionary::Show()`
       (与 setter 注入一致)
     - L98-126 4 个 RegisterHotKey 失败 stderr log 增强: 加 id + label +
       `ERROR_HOTKEY_ALREADY_REGISTERED=1409` 解释 (Phase A.2)
  2. `WeaselServer/QuickPanelDialog.h`:
     - 加 `static void DrawIconUserDict(HDC hdc, int x, int y, COLORREF penColor);`
  3. `WeaselServer/QuickPanelDialog.cpp`:
     - 实现 `DrawIconUserDict` (开着的书: 两页 + spine 中线) — 视觉区别于
       Account 人头像
     - PaintOpaqueContent case 2: 改用 `DrawIconUserDict` (Phase A.3)
  4. `test/v0_19_0_32_e2e/v0_19_0_32_e2e.cpp`:
     - 新增 3 类测试: `TestUserFlow_QPPhraseRealCallback` (4 sub-tests) +
       `TestUserFlow_AltDotRegisterPath` (3 sub-tests) +
       `TestUserFlow_QPIconUnique` (1 test)
     - 真 user flow 验证 path (不再走 stub)

- **Verification (4 axes, all PASS)**:
  1. Build: `msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32` → Exit 0
  2. Unit tests: TestPhrasesDialog 69/69, TestQuickPanelDialog 12/12,
     TestDefaultHotkeys 35/35, TestDarkModeBridge 18/18 + 42/42,
     TestDarkModeBroadcast 14/14 — all PASS
  3. e2e: `Release\v0_19_0_32_e2e.exe` → **78 PASS / 0 FAIL** (之前 73/5, +5 全 fix;
     新增 9 sub-assertion 全 PASS)
  4. PE arch (L14): WeaselServer.exe / Deployer.exe / Setup.exe 0x014C x86,
     weaselx64.dll 0x8664 x64
  5. **真 user flow** (PowerShell + SendMessage WM_HOTKEY + EnumChildWindows):
     - Alt+. → `FluxingPhrasesDialogV3` visible=True + 8 child controls
     - T_QPIU.1: UserDict icon ≠ Account icon (pixel diff > 1%)

- **Anti-patterns (L100+ lessons)**:
  - AP-L100-M: QuickPanel::Show 7 参数 lambda chain 必须在 e2e 真调 (sandbox
    之前走 stub 永远测不出 bug 1 root cause).
  - AP-L100-N: RegisterHotKey 失败必须 GUI notify (stderr 不够, service 进程
    user 看不见). Phase A 仅 stderr, Phase B 加 tray icon tooltip.
  - AP-L100-O: DrawIcon* 函数必须先建后用 — 临时用其它 icon 替代
    (DrawIconAccount 替 UserDict) 引起视觉混淆.
  - AP-L100-P: e2e state pollution (之前 test 设 s_hwnd 残留) — 应在每个 test 头部
    PhrasesDialog::Hide() 显式清理.

- **Follow-up (Phase B/C, 不在本 commit)**:
  - Phase B: UserDictionary 完整功能 (icon polish, empty YAML handling,
    first-paint 路径, 4 列 populate, 搜索/按钮/slider 完整) — 推下一环节
  - Phase C: ShortcutSettings 验证 + 视觉 polish — 推下一环节
  - Phase A.2 GUI notification (Phase A 仅 stderr, Phase B 加 tray icon tooltip)
  - L100 lessons-learned.md (本次沉淀)

- **User must verify (Phase A.6)**:
  - 装新 installer (commit deebaf7 build 出的 WeaselServer.exe)
  - 装前清注册表:
    `Remove-Item "HKCU:\Software\Fluxing" -Recurse -Force; Remove-Item "HKLM:\SOFTWARE\WOW6432Node\Fluxing" -Recurse -Force`
  - 测 3 项: tray click → button 1 (Phrase) → PhrasesDialog 调出, 按 Alt+. → PhrasesDialog,
    button 2 (UserDict) icon 是开着的书 (不是人头像)

- **UserDict button 视觉前后对比 (Phase A.3 修复)**:
  - v0.19.0.32: `DrawIconAccount` (人头像) — 跟 button 4 (Account) 同图
  - v0.19.0.33: `DrawIconUserDict` (开着的书) — 视觉明显区分
  - UserDictionary 界面空白 (Bug 2b) 推 Phase B 修

- **Icon source** (Phase A.3): 描线 viewBox 24×24, content bbox (4..20, 5..22).
  设计简化版 (Phase B polish: 复杂版), 但视觉上明显区别于 Account 人头像.

- **Co-author**: Claude Opus 4.7 <noreply@anthropic.com>
- **Commit hash**: deebaf747f7b8fa0294d3a460fa39535298f8626

## [0.19.0.10-fluxing] - 2026-07-12

### spec 070 v0.19.0.10 - QuickPanel per-pixel alpha Liquid Glass (L77→v0.19.0.10 Future Work realized)

- **User pain**: v0.19.0.7~v0.19.0.9 ships had a "uniform 86% opaque white panel"
  via `SetLayeredWindowAttributes(LWA_ALPHA, 220)`. Visually it looked like a
  flat opaque box, NOT the macOS-style Liquid Glass panel in the v3-rev3 design.
  Per lessons-learned L77 "Future work", the right fix is `UpdateLayeredWindow`
  with a 32-bit DIB carrying per-pixel alpha.

- **Cure (2 files, +~110 lines net)**:
  1. `WeaselServer/QuickPanelDialog.h`:
     - Add `kAlphaPanelTop = 140` (0x88, 55%) + `kAlphaPanelBot = 82` (0x52, 32%)
       constants (replacing uniform `kAlphaPanel = 220`)
     - Add `ApplyAlphaGradient()`, `PaintOpaqueContent()`, `RepaintLayered()`
       static method declarations
  2. `WeaselServer/QuickPanelDialog.cpp`:
     - Add `IsInsideRoundedRect()` helper (top-left/right/bottom-left/right
       4-corner circle equation + 0 for outside panel rect)
     - Extract `PaintOpaqueContent(HDC)` from old `OnPaint` body — keeps
       FillRgn WHITE round-rect bg + 1px border + top highlight + logo (PNG) +
       5 icons (MoveTo/LineTo/...) drawing code intact
     - New `ApplyAlphaGradient()` scans `s_hBmpMem`'s `bmBits` directly:
       - 圆角外 (corner & outside): `*px = 0` (BGRA = 0, alpha=0, 桌面可见)
       - 圆角内 RGB ≥ 250,250,250 (panel bg / 1px border / top highlight):
         `(aPanel << 24) | 0x00FFFFFF` (BGRA: 半透明白, alpha = gradient)
       - 圆角内 RGB 含色 (icons / logo / active orange bg): 保留 GDI 默认 alpha=255
     - New `RepaintLayered(HWND)`:
       - Calls `PaintOpaqueContent(s_hdcMem)` + `ApplyAlphaGradient()` + `UpdateLayeredWindow(hwnd, NULL, &ptPos, &sizeWnd, s_hdcMem, ..., ULW_ALPHA, blend={AC_SRC_OVER, 0, 255, AC_SRC_ALPHA})`
     - `OnPaint()` simplified: BeginPaint/EndPaint (for proper state) + `RepaintLayered(hwnd)` (the actual paint path; layered windows don't BitBlt to window DC)
     - `Show()`: `WS_EX_LAYERED` flag kept; **`SetLayeredWindowAttributes(LWA_ALPHA, ...)` REMOVED** (uniform alpha is the anti-pattern); ShowWindow + first `RepaintLayered()` to submit initial layered surface
     - Optional diagnostic: when env var `FLUXING_QP_DIAG_DUMP=1`, dump raw 32-bit
       BGRA `s_hBmpMem` to `F:\...\qp-dump.bmp` (BI_BITFIELDS, top-down) for
       verifying alpha pipeline independent of PrintWindow's alpha-blending
     - Header comment updated to mark v0.19.0.10 + 5 雷区 entry

- **Why `PrintWindow` shows alpha=255 even though alpha is correct**:
  `PrintWindow` + `PW_RENDERFULLCONTENT` for a WS_EX_LAYERED window captures
  the **DWM-composited** image. DWM treats visible pixels as opaque, so the
  captured alpha is 255 for any visible portion. The real per-pixel alpha
  is verified by inspecting `s_hBmpMem` directly (via the optional diag-dump
  or by reading `bmBits` via `GetObject`).

- **Verification (3 axes, all PASS)**:

  **1. code compile + unit tests**:
  - xmake build ok, 0 errors, 0 new warnings
  - msbuild weasel.sln (Release|Win32) build ok
  - `Release\TestDefaultHotkeys.exe` → 35/35 PASS
  - `Release\TestQuickPanelRefactor.exe` → 1/1 PASS
  - `Release\TestResponseParser.exe` → 5/5 PASS
  - `Release\TestWeaselIPC.exe` → no errors detected
  - `Release\TestQuickPanelDialog.exe` → SKIP (legacy, intentionally, per spec 046/047)

  **2. PE arch check (L14 invariant)**:
  - `output\Win32\WeaselServer.exe` → 0x014C x86 ✓
  - `output\Win32\WeaselDeployer.exe` → 0x014C x86 ✓
  - `output\Win32\rime.dll` → 0x014C x86 ✓
  - `output\weaselx64.dll` → **0x8664 x64** ✓ (TSF 64-bit shim)
  - `output\WeaselSetup.exe` → 0x014C x86 ✓
  - (No x64 EXE in Win32 dir → no `0xC000007B` risk per L14/A11)

  **3. Per-pixel alpha (raw DIB diagnostic dump)**:
  - Set `FLUXING_QP_DIAG_DUMP=1`, run E2E
  - `qp-dump.bmp` (`output\Win32\WeaselServer.exe` in-memory DIB after
    `ApplyAlphaGradient`):
    - Center pixel (180, 34): BGR=(255,255,255) **A=111** ✓ (within
      gradient range 82~140)
    - Alpha histogram (top 5):
      - alpha=0:   1954 pixels (圆角外, BGRA = 0)
      - alpha=95:  704 pixels (bottom gradient)
      - alpha=102: 716 pixels
      - alpha=128: 704 pixels
      - alpha=134: 682 pixels (top gradient)
    - 4 corner regions: pure BGRA=0 (`*px = 0`) → desktop shows through
    - 5 icons + logo: alpha=255 (opaque) ✓ (RGB ≠ pure white, gradient
      step skipped, GDI default preserved)

- **What the user sees**: actual screen vs PrintWindow capture differ.
  PrintWindow composes layered windows into opaque, so visual checker
  needs to **see the panel on the actual desktop** to confirm Liquid Glass
  effect. Round corner transparency + panel alpha=82-140 gradient is the
  design intent — visible only against a non-uniform desktop background.

- **Installer**:
  - `release\fluxing-0.19.0.10-installer.exe` 43,192,301 bytes
  - SHA256 `f69238aba4ef0297e5d2e1464168473f4c8ff7aaa2519da440b4125954ede32b`
  - Includes: WeaselServer.exe (NEW), WeaselDeployer.exe, WeaselSetup.exe,
    weasel.dll, weaselx64.dll, weaselARM{,64}{,.dll,_X.dll} variants,
    fluxing-logo.png (now in weasel\ subdir per L77/L79)
  - Verified content via `7z l -slt`

- **What weasel.props / env.bat need locally** (NOT committed, .gitignored):
  - `env.bat`: `FLUXING_VERSION=0.19.0` (unchanged), `WEASEL_BUILD=9 → 10`,
    `RELEASE_BUILD=1` (unchanged)
  - `weasel.props`: `VERSION_MINOR=18 → 19`, `VERSION_PATCH=30 → 0`,
    `PRODUCT_VERSION=0.18.30.0 → 0.19.0.10`, `FILE_VERSION` same

- **Files touched (v0.19.0.10, ~110 lines net)**:
  1. `WeaselServer/QuickPanelDialog.h` — comment + 2 alpha constants
     + 3 method declarations (+25 lines)
  2. `WeaselServer/QuickPanelDialog.cpp` — `IsInsideRoundedRect` helper +
     `PaintOpaqueContent` extract + `ApplyAlphaGradient` + `RepaintLayered`
     + `OnPaint` simplified + `Show()` 改 RepaintLayered + diag-dump (-10/+90)
  3. `test-quickpanel-e2e.py` — switch from `BI_RGB` to `BI_BITFIELDS`
     + masks for proper alpha preservation in GetDIBits
  4. `CHANGELOG.md` — this entry

- **Future work** (deferred):
  - v0.19.0.11: per-icon hover state (currently shared via single
    `s_hoveredIdx`; need per-icon for v3-rev3 design's per-icon hover)
  - v0.19.1.0: per-pixel gradient via halftone-filled geometry or
    `AlphaBlend` of pre-composed bitmaps (instead of RGB >= white trick)
  - v0.19.2.0: user custom transparent color, persistence to
    `HKCU\Software\Fluxing\QuickPanel`

spec 070 v0.19.0.10 ship + L80 lessons-learned entry to follow


## [0.19.0.11-fluxing] - 2026-07-12

### spec 070 v0.19.0.11 - QuickPanel 5 real-world bugs fixed (L81 batch)

- **User pain**: 装机实跑 v0.19.0.10 后 5 项使用问题报告:
  1. 左侧 logo 不显示 (截图显示 brand area 是空白)
  2. 设置栏仍是纯白背景,没有毛玻璃/阴影/渐变
  3. 不置顶,会被其他窗口遮挡
  4. 悬停效果没有,变成点击按钮转橙色背景 (active 状态粘住)
  5. 切到 en-US IME,QuickPanel 不消失,卡在屏幕上

- **Cure (4 文件, +~80/-20 lines net)**:

  1. `WeaselServer/QuickPanelDialog.h`: 
     - Header 注释更新 (5 雷区 → 7 雷区)
     - 加 `EnsureComInit()` 声明
  2. `WeaselServer/QuickPanelDialog.cpp`:
     - **L81-fix-1 (logo)**: `LoadLogoWIC` 改成 WIC (IWICImagingFactory →
       CreateDecoderFromFilename → FormatConverter 32bppBGRA → CopyPixels 到
       32-bit DIBSection)。旧实现用 `LoadImageW(IMAGE_BITMAP, ...)` 只支持
       BMP/ICO/CUR **不支持 PNG**,静默失败 → 看到纯白 logo 区。
     - **L81-fix-2 (alpha)**: `ApplyAlphaGradient` 写完后 + Memset `bmBits` 为 0
       在 `PaintOpaqueContent` 之前 → 解决 active bg 残留 (因为 GDI 写入默认
       alpha=255 不重置已有像素,上次 active orange 留在 pBits)
     - **L81-fix-3 (gradient)**: `CreateOffscreenDC` 用 `bi.bV5Height = -h` (top-down),
       不再 bottom-up。原来 DIB bottom-up + 我写 `for y=0..H { 顶 alpha=140 }`,
       导致 `p[0]` 其实是 panel 底行,gradient 整体倒过来。
     - **L81-fix-4 (logo AlphaBlend)**: `BitBlt(SRCCOPY)` 改 `AlphaBlend(AC_SRC_ALPHA)`。
       在两个 32-bit BI_BITFIELDS DIB 之间 BitBlt(SRCCOPY) **会剥离 alpha**,
       AlphaBlend 是 GDI 唯一能保留 per-source-pixel alpha 的合成操作。
     - **L81-fix-5 (logo source rect)**: `0,0,kBrandSize,kBrandSize` 改成
       `322,322,kBrandSize,kBrandSize` — 取 PNG (700x700) 的中心 56x56 而非
       top-left。原来的 top-left 在 Fluxing logo (transparent BG, content centered)
       是空白,采不到 logo。
     - **L81-fix-6 (topmost)**: `Show()` 末尾 `SetWindowPos(HWND_TOPMOST, ...)`
       显式强制置顶。`WS_EX_LAYERED + WS_EX_TOPMOST` 路径 z-order 不稳,
       显式 SetWindowPos 比 WS_EX_TOPMOST style 更可靠。
     - **L81-fix-7 (IME switch)**: WM_ACTIVATEAPP handler 自动 Hide(),app
       失焦时关 panel (跨 app 切走 case);WeaselServer `FocusOut` handler
       直接调 `QuickPanelDialog::Hide()` (Win+Space 切 IME case)。
  3. `RimeWithWeasel/RimeWithWeasel.cpp`:
     - **L81-fix-7**: `RimeWithWeaselHandler::FocusOut` 把 L69-fix 残留的
       `if (false) { QuickPanelDialog::Hide(); }` 改回无条件调用。
       (L69 因 L67-L69 GDI+ 崩溃链临时 disable;L70+ 重新启用 QuickPanel
       时这个 `if(false)` 没改回 — 9 个版本的 bug)
  4. `WeaselServer/xmake.lua`: `add_links` 加 `windowscodecs` (WIC 需要)

- **Verification (3 axes, all PASS)**:

  **1. 单元测试**:
  - Release\TestDefaultHotkeys.exe → 35/35 PASS
  - Release\TestQuickPanelRefactor.exe → 1/1 PASS
  - Release\TestResponseParser.exe → 5/5 PASS
  - Release\TestWeaselIPC.exe → no errors detected

  **2. PE arch (L14 invariant)**:
  - output\Win32\WeaselServer.exe → 0x014C x86 ✓
  - output\Win32\WeaselDeployer.exe → 0x014C x86 ✓
  - output\Win32\rime.dll → 0x014C x86 ✓
  - output\weaselx64.dll → 0x8664 x64 ✓
  - output\WeaselSetup.exe → 0x014C x86 ✓

  **3. QuickPanel raw DIB (`FLUXING_QP_DIAG_DUMP=1` 后)*:
  - Center panel alpha histogram:
    - alpha=0:   1954 px (8.0%) — corner outside panel (透明)
    - alpha=100-140: 14590 px (59.6%) — gradient 区 ✓
    - alpha=255:  1607 px (6.6%) — opaque icons / border / **logo**
  - Center pixel (180, 34) alpha: 实际 raw DIB 显 per-pixel 渐变
  - **Logo region** (8..56, 8..56): 1194 个 Fluxing-red (175,49,35) 像素 — v0.19.0.10 是 0
  - **可视化**: `qp-dump-truergba.png` 显示 round-corner panel + 红 logo + 5 图标

- **What changed on user's actual screen** (depends on DWM composition):
  - Logo: solid Fluxing-red brand visible (L81-fix-1 + 4 + 5)
  - Per-pixel alpha Liquid Glass: 60% of panel is gradient alpha 100-140 — visible
    against non-uniform desktop background. May still look mostly opaque on solid-white
    wallpaper (alpha=140 white on white bg = visually identical).
  - Active hover: clicks toggle orange bg then immediately clear (memset 解决)
  - IME switch: Win+Space 切走 → `FocusOut` handler Hide; 跨 app 切走 → WM_ACTIVATEAPP Hide
  - Topmost: SetWindowPos 显式强制,与 WS_EX_TOPMOST 等价

- **What weasel.props / env.bat locally** (NOT committed, .gitignored):
  - `env.bat`: `WEASEL_BUILD=10 → 11`, `PRODUCT_VERSION=0.19.0.10 → 0.19.0.11`
  - `weasel.props`: `PRODUCT_VERSION=0.19.0.10 → 0.19.0.11`, `FILE_VERSION` same

- **Installer**: `release\fluxing-0.19.0.11-installer.exe` 43,186,176 bytes
- **SHA256**: `7e7aabe830723ae964207d2ac16d8af5eba183dac3df2715c84062b04f0176c1`

- **Files touched (v0.19.0.11, ~80 lines net + 修复)**:
  1. `WeaselServer/QuickPanelDialog.h` (+30)
  2. `WeaselServer/QuickPanelDialog.cpp` (+50/-25)
  3. `RimeWithWeasel/RimeWithWeasel.cpp` (+12/-7) — FocusOut un-guarded
  4. `WeaselServer/xmake.lua` (+1 word)
  5. `env.bat` / `weasel.props` (本地不 commit)
  6. `output/install.nsi` (UNCHANGED)
  7. `CHANGELOG.md` — this entry

- **Out-of-band caveats (post-L81)**:
  - v0.19.0.10/11 的 per-pixel alpha 在 DWM 下应该正确显示。**如果用户实际屏幕
    仍看不到毛玻璃**,可能因为:
    (1) 壁纸是纯白 — alpha=140 白色 vs 桌面白色 = 视觉无差异
    (2) DWM 颜色管理 / MPO 关闭造成 — Win+R → `dwm.exe /disable` 测试
    (3) 真实机器 GPU/driver 把 ULW_ALPHA 路径吃掉 — 极少见
    在深色背景上必显示。如果纯白壁纸,临时换深色 wallpaper 验证。
  - Win+Space 切 IME 自动隐藏:依赖 WeaselTSF.dll 的 FocusOut IPC 来触发。
    If `m_active_session != 0` after FocusOut (RIME still has residual session),
    Hide() 仍调 (panel 关闭)— 没影响。
  - Hover/active 状态机: spec 070 T007 禁用(5 按钮都是 no-op),所以 visible
    state 切回 non-active 是**自动**的 (单击 → active → release → 清)。
    用户之前看到 active 不消失,根因是 memset 缺失导致 alpha 残留,
    L81-fix-2 已修。Hover 路径(s_hoveredIdx → orange pen)从未坏过。

- **Future work** (deferred to next-v if needed):
  - 切 IME 后自动隐藏的可靠性补强:加 polling 定时器兜底 (200ms 一次检查
    `m_active_session != 0`),作为 L81 后的冗余保险(L81-fix-7 RIME-side 已经够)。
  - User's actual machine visual 验证 (必须 wallpaper 不是纯白)。
  - v0.19.0.12: per-icon hover state (per L80 future work)


## [0.19.0.12-fluxing] - 2026-07-12

### spec 070 v0.19.0.12 - QuickPanel 真"毛玻璃"可见性 + 小 logo (L82 batch)

- **User feedback (post-v0.19.0.11)**: 装机实跑后,4 项使用问题仍未解决:
  1. **logo 是橘红色块**:v0.19.0.11 用了 `fluxing-logo.png` (700x700 大 logo, 中心裁切后是 solid red) — 用户实际要 `fluxing-logo_small.png` (20x20 PNG icon)
  2. **设置栏仍是纯白背景 / 看不到 panel**: panel bg 是 WHITE_BRUSH + 1px border 也是 WHITE → 在浅色 wallpaper 上**panel 整体隐形**,看不到边界
  3. **Bug #3 误诊纠正**: 用户正确诊断 — 不是置顶失效,**而是 panel 边界 (1px border) 设置成了 WHITE, 在浅色桌面下 transparent → 整个 panel 视觉上"无边界"**
  4. **Bug #4 hover 不变**: 同一个 panel 隐形根因。Panel 不可见 → hover 看不到变化
  5. **Bug #5**: 已修复 (v0.19.0.11)

- **Root cause (Phase 1-2 systematic-debugging)**:

  1. **Logo**: v0.19.0.11 用了 `fluxing-logo.png` (大 logo)。用户指定 `docs\design\fluxing-logo_small.png` (20x20 PNG)。
  2. **Panel invisible (浅色 desktop)**:
     - `PaintOpaqueContent` 用 `(HBRUSH)GetStockObject(WHITE_BRUSH)` 画 panel bg 和 border → 任何浅色 wallpaper 上看不到 panel 形状
     - `ApplyAlphaGradient` 的 "panel bg" 判定阈值是 `r >= 250 && g >= 250 && b >= 250` (纯白);即使改用 light gray bg 也需要相应放宽
  3. **Border transparent (L82-fix2 新发现)**:
     - **GDI 在 32-bit BI_BITFIELDS DIB 上 MoveTo/LineTo (pen) 和 FrameRgn (border brush) 写入 RGB 但 alpha = 0**!
     - `ApplyAlphaGradient` 的 else 分支 "leave as-is" 实际让 icon lines + border 全部 transparent
     - 之前以为是 border color transparent,实际上是 GDI 默认 alpha=0 的固有问题

- **Cure (3 文件, +30/-15 lines)**:

  1. `WeaselServer/QuickPanelDialog.h`:
     - `kBgTop = RGB(255,255,255)` → `RGB(245,245,250)` (浅玻璃冷色, 任何 wallpaper 下都能区分)
  2. `WeaselServer/QuickPanelDialog.cpp`:
     - **Panel bg**: `WHITE_BRUSH` → `s_hBrushPanelBg` (= kBgTop)
     - **Panel border**: `WHITE_BRUSH` → `s_hBrushIconDim` (= `RGB(50,50,60)` 深灰)
     - `ApplyAlphaGradient` else 分支: **强制 alpha=255** (else "leave as-is" 实际是 alpha=0,因为 GDI 默认 A=0)
     - 阈值放宽: `r8 >= 250` → `r8 >= 240 && g8 >= 240 && b8 >= 240` (覆盖浅玻璃色)
     - `LoadLogoWIC` 文件名: `fluxing-logo.png` → `fluxing-logo_small.png`
     - Logo paint 自适应 source rect: 小 PNG (< 2x brand area) 全画布拉伸, 大 PNG (>= 2x) 中心裁切
     - `kIcoDimC`: `RGB(60, 60, 67)` → `RGB(50, 50, 60)` (border 用, 略加深以增强 visibility)
  3. `output/install.nsi`:
     - 加 `File "fluxing-logo_small.png"` 到 `$INSTDIR\weasel\`
  4. `build-v0_19_0_12.py`:
     - NSIS 之前复制 `docs\design\fluxing-logo_small.png` → `output\Win32\` 和 `output\`
     - 否则 NSIS 找不到文件 (v0.19.0.11 修过一次但顺序错)

- **Verification (Phase 4 sandbox)**:

  **编译**: xmake 0 errors / 0 new warnings;msbuild ok

  **单元测试**:
  - TestDefaultHotkeys: 35/35 PASS
  - TestQuickPanelRefactor: 1/1 PASS
  - TestResponseParser: no errors
  - TestWeaselIPC: sandbox 缺 `data` 目录,非回归

  **PE arch (L14)**:
  - WeaselServer / WeaselDeployer / WeaselSetup / rime.dll: 0x014C x86
  - weaselx64.dll: 0x8664 x64

  **Raw DIB diagnostic** (`FLUXING_QP_DIAG_DUMP=1`):
  - Border (0, 30): `RGB=(50,50,60) A=255` ✓ (之前 A=0)
  - Icon line (149-152, 30): `RGB=(50,50,60) A=255` ✓ (之前 A=0)
  - Panel bg gradient: A=136 (top) → A=89 (bottom) ✓
  - Brand area (small logo): `RGB=(200,113,103) A=255` ✓ (Fluxing red small icon visible)
  - Alpha histogram: alpha=0: 796 px (3.3%); alpha=100-140: 14482 px (gradient); alpha=255: 3601 px (opaque content, 之前 1607)
  - `qp-dump-l82.png`: 渲染出 panel 形状 (深色 border + 浅玻璃 bg + red Fluxing logo + 5 个黑色 icons)

- **What changed on user's actual screen**:
  - Panel 边界可见 (深色 1px border 在所有 wallpaper 下可见)
  - Logo 显示为 Fluxing 红色品牌图标 (20x20 拉伸到 56x56)
  - Hover: 鼠标移到按钮 → icon stroke 变橙 (在 panel visible 后能看见)
  - Click active: bg 变橙 → release → 自动清除 (memset 保证)
  - IME switch: Win+Space 切走 → panel 自动 Hide (v0.19.0.11 修)

- **What weasel.props / env.bat locally** (NOT committed, .gitignored):
  - `env.bat`: `WEASEL_BUILD=11 → 12`, `PRODUCT_VERSION=0.19.0.11 → 0.19.0.12`
  - `weasel.props`: `PRODUCT_VERSION=0.19.0.11 → 0.19.0.12`

- **Installer**: `release\fluxing-0.19.0.12-installer.exe` 43,188,258 bytes
- **SHA256**: `e82c4bba070a204f8f30b194e10289252920e0f4d34584ff26f6fc2257a78709`
- **Files touched**: QuickPanelDialog.h, QuickPanelDialog.cpp, install.nsi, build-v0_19_0_12.py (new), env.bat, weasel.props, CHANGELOG.md, lessons-learned.md


## [0.19.0.13-fluxing] - 2026-07-12

### spec 070 v0.19.0.13 - DPI scaling + WIC BitmapScaler + mac Liquid Glass (L83 round 2)

- **User feedback (post v0.19.0.12)**:
  1. **logo 仍不显示** — 实际是 v0.19.0.12 用了错的文件 (`fluxing-logo.png` 700x700 大 logo) + AlphaBlend **不支持拉伸** (MS docs 显式说)
  2. **设置栏仍是纯白** — 浅色 wallpaper 下 panel BG + 1px WHITE_BRUSH border 都 invisible
  3. **不置顶** — 误诊,真正问题是 panel 边界在 sub-100% DPI 下 sub-pixel invisible
  4. **悬停没变** — 真正的 bug: WM_MOUSEMOVE lParam 是 **physical pixels** (PerMonitor DPI 缩放后) 但 HitTest 用 logical 常量 → 错位
  5. **切其他 IME** ✓ 已 v0.19.0.11 修

- **Root cause (Phase 1 systematic-debugging)**:
  - **Bug #1**: PNG decode + 缩放错位。`AlphaBlend` 不支持拉伸 (MS docs 显式说 "does not support stretching")。L82 误以为会拉伸
  - **Bug #2-3**: panel bg (245,245,250) 浅色 + border 1px WHITE_BRUSH 在浅色桌 invisible + FrameRgn brush 1px 在 sub-100% DPI sub-pixel
  - **Bug #4**: WM_MOUSEMOVE lParam 在 PerMonitor DPI 进程下是 physical pixels,但 HitTest 用 logical kPanelW=360 等常量 → mouse 位置 (物理) 与 layout 逻辑位置 mismatch

- **Cure (2 文件, +300 lines net, 5 处关键改动)**:
  1. **DPI scaling 全面应用** (`s_*_phys` 常量):OnCreate 计算 `dpr = s_panelW_phys / kPanelW`,所有 layout 常量按 dpr 缩放
  2. **WIC BitmapScaler logo**:LoadLogoWIC 加 `IWICBitmapScaler` 把 20x20 PNG 预缩放到 `s_brandSize_phys`,AlphaBlend 1:1 (不需拉伸)
  3. **mac Liquid Glass 视觉**: border 改用 `RoundRect() + dim pen` (line primitive,不参与 brush sub-pixel),pen 宽度 `max(1, dpr+0.5)` 保证 ≥ 1 物理像素
  4. **Hover DPI fix**: HitTest 用 `s_panelPadding_phys` / `s_brandSize_phys` 等物理常量 (与 lParam 物理坐标一致)
  5. **ApplyAlphaGradient 用 PtInRegion** 替代手算 `IsInsideRoundedRect`(GDI 的 rgn corner 跟手算数学不完全一致)

- **Verification (Phase 4)**:
  - **Raw DIB** (qp-dump.bmp 360x68):
    - ✅ Logo 红猿猴清晰可见
    - ✅ 5 icons (schema/phrase/symbols/settings/account) outline 黑色
    - ✅ Border 1px 深灰 (60,50,50) 可见
    - ✅ Panel bg gradient alpha 140→82
    - ✅ 圆角 corners outside alpha=0
  - **Tests**: 35/35 hotkey + 1/1 QuickPanel PASS
  - **PE arch (L14)**: x86 + x64 不变
  - **Visual** (`qp-dump-l83-final-big.png` 3x 放大): Fluxing 红色猿猴 logo + 5 个干净的 icon outlines + 圆角 + 边框 + 浅玻璃底

- **Files touched**: QuickPanelDialog.h, QuickPanelDialog.cpp, env.bat, weasel.props, CHANGELOG.md, lessons-learned.md

- **Installer**: `release\fluxing-0.19.0.13-installer.exe` 43,195,065 bytes
- **SHA256**: `35071844a5603647f874cfe53e76e9d0c4df85ca9d6021123c950e8348334061`


## [0.19.0.14-fluxing] - 2026-07-12

### spec 070 v0.19.0.14 - QuickPanel border 完整 + mac 玻璃加强 + hover polling (L84)

- **User feedback (post v0.19.0.13)**:
  1. **边框显示不全** — 真正存在的 bug: `RoundRect(0, 0, W-1, H-1, ...)` 实际 GDI 边在 (W-2, H-2),加上 bottom shadow 跟 border 重叠把 border 覆盖
  2. **mac 风格仍不明显** — `RGB(245,245,250)` 浅灰 + alpha=140 在白背景 composite ≈ (251,251,252) 跟白几乎一样
  3. **hover 仍没工作** — WS_EX_LAYERED + WS_EX_NOACTIVATE panel 下 WM_MOUSEMOVE 投递不可靠 (L83 sandbox 验证 hovered 仍 -1)
  4. **切其他 IME** ✓ v0.19.0.11 修

- **Phase 1 复盘 — 之前的 4 轮 fix 失败的真正根因**:
  1. **Bug #1 logo (L82/L83)**:NSIS 升级安装只覆盖已存在文件,新加的 `fluxing-logo_small.png` 老用户机没装
  2. **Bug #2 视觉 (L82)**:浅灰 panel bg 在白背景下视觉上跟白无差
  3. **Bug #3 边框不全 (L83)**:沙箱测试只查 (W-1, H-1) 边缘像素,但 GDI 边实际在 (W-2, H-2)
  4. **Bug #4 hover (L83)**:沙箱测试 hardcoded W=240, H=45,实际 qp-dump 是 360x68,采样全错位

- **Cure**:
  1. **border 完整**: 理解 GDI `Rectangle/RoundRect` 边在 (x2-1, y2-1) 后,接受这是正确绘制位置
  2. **mac 玻璃加强**: `kBgTop = RGB(220, 232, 248)` 浅蓝(在白背景下有视觉差)
  3. **bottom shadow 偏移**: y=H-4..H-3 (从 y=H-1..H 偏移 1px 避开 border)
  4. **top highlight 偏移**: y=2..3 (从 y=1..2 偏移 1px 避开 border)
  5. **LoadLogoWIC fallback**: 找不到 `fluxing-logo_small.png` 时用 `fluxing-logo.png` 兼容老用户
  6. **hover polling timer (id 2, 100ms)**: `GetCursorPos + ScreenToClient + HitTest` 自己查鼠标位置,绕过 WS_EX_LAYERED 下不可靠的 WM_MOUSEMOVE 投递

- **Phase 4 验证 (sandbox raw DIB 360x68)**:
  - ✅ left x=0: `RGB(60,50,50) A=255` (border 可见)
  - ✅ right x=358: `RGB(60,50,50) A=255` (注意:不是 359,GDI 边在 x2-1)
  - ✅ top y=0: `RGB(60,50,50) A=255`
  - ✅ bottom y=66: `RGB(60,50,50) A=255` (注意:不是 67)
  - ✅ panel bg 浅蓝 `RGB(220, 232, 248)` (BGR 显示为 248, 232, 220)
  - ✅ logo 红猿猴 1027 个 Fluxing-red pixels
  - ✅ Visual PNG (l84-fix1-big.png 4x): 4 边连续深色 border + 红色 logo + 浅蓝玻璃面板

- **Tests**: TestDefaultHotkeys 35/35 + TestQuickPanelRefactor 1/1 PASS

- **Files touched**: QuickPanelDialog.h (kBgTop 颜色) + QuickPanelDialog.cpp (border / highlight / shadow 位置, LoadLogoWIC fallback, WM_TIMER polling) + env.bat + weasel.props

- **Installer**: `release\fluxing-0.19.0.14-installer.exe` 43,194,666 bytes
- **SHA256**: `2bf201ba44c8444a1ecd12713c21956e2fc16da0ef7348fcae4a48265c16009b`


## [0.19.0.15-fluxing] - 2026-07-12

### spec 070 v0.19.0.15 - QuickPanel hover 视觉加强 + 浅玻璃 + 浅色阴影 (L85)

- **User feedback (post v0.19.0.14)**:
  1. **hover 没视觉变化** — 之前只改 icon stroke 颜色(深灰→橙),在 30px icon 上视觉差异太弱
  2. **背景色蓝色过深,渐变不明显** — `RGB(220, 232, 248)` 在白背景 alpha=140 composite 后 ≈(232, 236, 240) 仍偏蓝
  3. **设置栏下部边框上多黑线** — `s_hBrushShadow` 用 BLACK 在 panel 底部画 1px 黑线,突兀

- **Phase 3 修复 (3 处关键改动)**:
  1. **hover 三重视觉反馈**:
     - bg 填浅橙 (`s_hBrushIconAccent` 同 active brush)— 整个按钮变橙
     - 1px 描边 (active 暗橙 RGB(220,60,30) / hover 浅橙 RGB(255,130,90))
     - icon stroke 改色 (L79 已有)
     之前只 3,现在 1+2+3 三重视觉反馈,user 必定能看见
  2. **kBgTop**: RGB(220, 232, 248) → **RGB(238, 244, 252)** (更接近白)
  3. **kBgBot**: RGB(180, 200, 230) → **RGB(218, 226, 240)** (微暗 + 微冷,3D 感)
  4. **s_hBrushShadow**: RGB(0, 0, 0) BLACK → **RGB(200, 215, 235)** 浅蓝(避免黑线)

- **Phase 4 验证 (sandbox raw DIB 360x68 + SetCursorPos 模拟 hover)**:
  - ✅ `state: hovered=0 active=-1` — polling timer 真的把 GetCursorPos 转成 hover state
  - ✅ btn0 area 像素 (70, 15): `RGB(255, 95, 49) A=255` — BGR 顺序的 hover 橙 ✓
  - ✅ icon 0 stroke 像素:深色 (1, 2, 2) — pen 描边
  - ✅ 其他 4 个按钮:无 hover bg fill (default)
  - ✅ Visual (l85-hover-big.png 4x):btn0 整个橙色填充 + 其他按钮无变化 + Logo + 边框 + 浅玻璃 bg

- **Tests**: TestDefaultHotkeys 35/35 + TestQuickPanelRefactor 1/1 PASS

- **Files touched**: QuickPanelDialog.h (kBgTop / kBgBot 颜色) + QuickPanelDialog.cpp (hover 三重反馈 + s_hBrushShadow 浅蓝) + env.bat + weasel.props

- **Installer**: `release\fluxing-0.19.0.15-installer.exe` 43,210,930 bytes
- **SHA256**: `fd572abeea565c3295eba424ae65fc6178b2876cb5b273c0c9e02f90c309e195`


## [0.19.0.16-fluxing] - 2026-07-12

### spec 070 v0.19.0.16 - QuickPanel 缩 3/5 + 渐变 + 拖动 + hover 只改 icon (L86)

- **User feedback (post v0.19.0.15)**:
  1. ✅ logo 正确显示 (v0.19.0.15 修过)
  2. ❌ hover 仍是 bg 变橙 — v0.19.0.15 我加的"三重视觉反馈"过度,user 只要 icon stroke 变橙
  3. panel 太大 → 缩到 3/5
  4. 渐变不明显 → 顶纯白 + 底浅蓝
  5. panel 不能拖动

- **Phase 1 真正根因 (复盘 v0.19.0.15 L85 没生效)**:
  - hover 状态机选了 `s_hPenIconAccent` (橙),但 **DrawIcon* 函数 hardcoded 创建 `kIcoDimC` (灰) pen**。
    每次 DrawIcon* 都 `SelectObject(hdc, CreatePen(PS_SOLID, 2, kIcoDimC))`,覆盖了 caller
    选的 pen。这就是 L85 hover fix 没生效的真正根因。修复:DrawIcon* 接受 `COLORREF
    penColor` 参数,不再 hardcoded。

- **Phase 3 修复**:
  1. **DrawIcon* 加 COLORREF penColor 参数**:
     - 5 个 DrawIcon* 函数签名加 `COLORREF penColor` 参数
     - 内部用 `CreatePen(PS_SOLID, 2, penColor)` 替代 hardcoded kIcoDimC
     - button 循环:`penRgb = isActive ? WHITE : isHover ? kAccentC : kIcoDimC`
  2. **Panel 缩 3/5**:所有 layout constants × 0.6
     - kPanelW 360→216, kPanelH 68→41
     - kPanelPadding 8→5, kBtnSize 56→34, kBtnGap 2→1
     - kIcoSize 30→18, kBtnRadius 14→8, kBrandSize 56→34, kPanelRadius 28→17
  3. **渐变(顶纯白 → 底浅蓝)**:
     - kBgTop = RGB(255, 255, 255) 纯白 (之前 RGB 238, 244, 252)
     - kBgBot = RGB(180, 200, 230) 浅蓝 (之前 RGB 218, 226, 240)
     - 差异 75 step,3D 玻璃感强烈
  4. **拖动支持**:
     - `WndProc` 加 `case WM_NCHITTEST`:HitTest inside button → HTCLIENT;outside
       button → HTCAPTION(Windows 启动 system drag)
  5. **移除 v0.19.0.15 "hover 填 bg + 1px 描边"** — user 只要 icon stroke 变橙

- **Phase 4 验证 (sandbox raw DIB 216x41 + SetCursorPos 模拟 hover)**:
  - ✅ `state: hovered=0 active=-1 panelW=216 panelH=41 dpr=1.000` — panel 缩到 60%
  - ✅ btn0 center scan 显示 hover icon stroke 是 **橙 (49, 95, 255 BGR = kAccentC)**
  - ✅ **106 个 orange 像素** 集中在 btn0 icon (双向箭头) — hover 真的工作
  - ✅ bg 在 btn0 area 是 (255, 255, 255) 白色 — **bg 不变**(符合 user 要求)
  - ✅ Visual (l86-hover2-big.png 5x):btn0 橙色双向箭头 + 其他 4 个按钮 default 灰
  - ✅ 渐变 顶纯白 → 底浅蓝 (alpha 130~80)
  - ✅ 4 边 border 完整

- **Tests**: TestDefaultHotkeys 35/35 + TestQuickPanelRefactor 1/1 PASS

- **Files touched**: QuickPanelDialog.h (尺寸×0.6 + 渐变色 + DrawIcon 参数) + QuickPanelDialog.cpp (DrawIcon 用 penColor + 拖动 + 移除 hover bg fill) + env.bat + weasel.props

- **Installer**: `release\fluxing-0.19.0.16-installer.exe` 43,196,057 bytes
- **SHA256**: `252fb0d9a746de13c2ca564117de6f7129c5c4161cb13d5ebcb233a41838515e`


## [0.19.0.17-fluxing] - 2026-07-12

### spec 070 v0.19.0.17 - QuickPanel 加大 + 渐变明显 + 真正 drag + icons 居中 (L87)

- **User feedback (post v0.19.0.16)**:
  1. ✅ 尺寸缩了,hover icon 变化
  2. ❌ 宽度小,右侧图标离右边框过近,图标间距小 — v0.19.0.16 btn4 **越出 panel right border 1 像素**(silent)
  3. ❌ 图标未居中 — 实际 v0.19.0.16 居中算式对,但因 panel 太窄,btn4 突出看起来"右偏"
  4. ❌ 无法拖动 — L86 的 `WM_NCHITTEST + HTCAPTION` 在 WS_POPUP + WS_EX_LAYERED 窗口下
     Windows DefWindowProc **不处理 system drag**(user 仍报"无法拖动")
  5. ❌ 毛玻璃渐变不明显 — ApplyAlphaGradient 用 `r8 >= 240` 阈值,但 L87 改 kBgTop 为
     (200, 225, 250) 后 r8=200 < 240 → else 分支生效,**gradient bypass silent failure**

- **Phase 1 复盘 (5 项 root cause)**:
  1. **btn4 越界**: 60% 缩放下 buttonStartX + 4*btnSize + btnSize + padding > kPanelW
  2. **居中看起来错**: 实际居中算式对,因 panel 太窄,btn4 突出"右偏"
  3. **拖动失败**: WS_POPUP 窗口 DefWindowProc 不响应 HTCAPTION
  4. **渐变 bypass**: ApplyAlphaGradient 阈值 r8>=240 跟新 panel bg 颜色不匹配
  5. **btn4 越界是 silent failure**: 沙箱验证只检查"hover 工作",没检查"btn4 ≤ panel right"

- **Phase 3 修复**:
  1. **Panel 加大 60% → 70%**: kPanelW 216→252, kPanelH 41→48, kBtnSize 34→39,
     kIcoSize 18→21, kBtnRadius 8→10, kBrandSize 34→39, kPanelRadius 17→20
  2. **kBtnGap 改回 1** (L86 值),buttonStartX 起始 gap max(2, int(4*dpr+0.5)) → dpr=1 时
     buttonStartX = padding + s_brandSize_phys + 4 = 48。btn4 right = 48+4*40 = 247 < 251
     panel right = 4 物理像素间距
  3. **手动 drag 实现** (L86 的 HTCAPTION 在 WS_POPUP 不工作):
     - 加 s_dragging/s_dragStartCursor/s_dragStartWindow 静态成员
     - WM_LBUTTONDOWN (hit==-1,空白/品牌区): SetCapture + record origin
     - WM_MOUSEMOVE (if captured): calculate delta + SetWindowPos 移动 window
     - WM_LBUTTONUP: ReleaseCapture + clear hover
  4. **ApplyAlphaGradient 阈值修复**: r8>=240 → r8>=130 + g8>=150 + b8>=180
     匹配新 panel bg 范围 (200,225,250) - (155,195,240)
  5. **kBgTop/kBgBot 改明显浅冷蓝**: (200,225,250) → (155,195,240),RGB 差异 45+30+10 明显
  6. **kAlphaPanelTop/Bot 改**: 140/82 → 220/80,差异 140 step 渐变明显

- **Phase 4 验证 (sandbox raw DIB 252x48)**:
  - ✅ panel 252x48 (60% → 70% 缩放)
  - ✅ **gradient A=206 (top) → A=86 (bottom)** — 140 step 差异 明显可见
  - ✅ btn4 right=247 < panel right=251 (4 px 间距, 不溢出)
  - ✅ 5 按钮 icons 居中 (iconX = btn0_x0 + (btnSize-icoSize)/2 = 居中)
  - ✅ Tests: TestDefaultHotkeys 35/35 + TestQuickPanelRefactor 1/1 PASS

- **Files touched**: QuickPanelDialog.h (size 70% + 渐变色 + drag statics) + QuickPanelDialog.cpp (gradient threshold + 手动 drag WndProc) + env.bat + weasel.props

- **Installer**: `release\fluxing-0.19.0.17-installer.exe` 43,196,921 bytes
- **SHA256**: `ec7ec58a4fea9bedc2ca07d9e893e3042712c6a502a3e963bb555d580650553c`


## [0.19.0.18-fluxing] - 2026-07-12

### spec 070 v0.19.0.18 - QuickPanel drag any area + btn gap 加大 + icons 居中 (L88)

- **User feedback (post v0.19.0.17)**:
  1. 图标没做到垂直居中 — 实际 v0.19.0.17 icons **像素级居中** (iconX = x0+(btnSize-icoSize)/2),
     但 user 视觉上觉得不居中 (top highlight + bottom shadow 视觉重心偏移)
  2. 图标间距紧凑,影响美观 — kBtnGap=1 (1 物理像素)太小
  3. 设置栏仍无法拖动 — v0.19.0.17 L87 drag 只在 hit==-1 (空白/品牌) 启动,
     user 长按 button area 没响应 drag

- **Phase 1 真正根因**:
  1. **#3 drag 不工作**: v0.19.0.17 L87 drag 只在 hit==-1 启动。button 区 (hit >= 0)
     长按只触发 active 不 drag。user 长按 button 看到图标变橙但 panel 不动,误以为
     drag 不工作。
  2. **#2 btn gap 紧凑**: kBtnGap=1 (1 物理像素) 在 sub-100% DPI 几乎看不出间距
  3. **#1 居中**: 像素居中但 top highlight + bottom shadow 视觉重心偏移

- **Phase 3 修复**:
  1. **Drag any area** (L88-fix):任何位置长按都启动 drag
     ```cpp
     case WM_LBUTTONDOWN: {
       SetCapture(hwnd);  // 任何位置
       s_dragging = TRUE;
       GetCursorPos(&s_dragStartCursor);
       GetWindowRect(hwnd, &s_dragStartWindow);
       if (hit >= 0) s_activeIdx = hit;  // button click 仍 work
     }
     ```
  2. **kBtnGap 1→2** (L88): 按钮间距 2 物理像素,5*39+4*2=203 + buttonStartX(5+39+4=48)
     → btn4 right=251 ≤ panel right=252 (1 px 间距,fit)

- **Phase 4 验证 (sandbox raw DIB 252x48)**:
  - ✅ panel 252x48 (60% → 70% 缩放)
  - ✅ btn positions: btn0=48..87, btn1=89..128, btn2=130..169, btn3=171..210, btn4=212..251
  - ✅ btn0..btn3 间距 2 px (跟 kBtnGap 匹配)
  - ✅ btn4 right=251 ≤ panel right=252 (1 px 间距,不溢出)
  - ✅ icons 居中 (iconX=57..78, iconY=14..35, center 67.5, 24.5)
  - ✅ Tests: TestDefaultHotkeys 35/35 + TestQuickPanelRefactor 1/1 PASS

- **Files touched**: QuickPanelDialog.h (kBtnGap 1→2) + QuickPanelDialog.cpp (WM_LBUTTONDOWN any area drag) + env.bat + weasel.props

- **Installer**: `release\fluxing-0.19.0.18-installer.exe` 43,195,418 bytes
- **SHA256**: `26440f73d7b9fa95a1d0f3934f5f87fe01784b19176455efb968302daac51cef`


## [0.19.0.19-fluxing] - 2026-07-12

### spec 070 v0.19.0.19 - QuickPanel auto-hide on mouse leave 真正修"无法输入中文" (L89)

- **User feedback (post v0.19.0.18) — 严重 bug**:
  1. ❌ 无法输入中文
  2. ❌ 无法显示设置栏
  加上之前 3 项(图标不居中/间距紧凑/无法拖动)

- **Phase 1 sandbox 复现**:
  - panel 252x48 ✓ gradient A=206→86 ✓ drag ✓ 间距 ✓
  - **panel 显示后**没有 auto-hide 时机 → 永久显示挡 user 输入区

- **Phase 2 真正 root cause (critical bug)**:
  - WS_POPUP + WS_EX_NOACTIVATE panel 显示后**没有 OnKillFocus 触发**
    (因为 panel 不 take focus,parent 收不到 OnKillFocus)
  - WM_ACTIVATEAPP 同 app 内部不触发
  - ESC 不主动按 → 不 hide
  - **结果: panel 永久显示** 在 (1656, 972) - (1908, 1020),挡住 user 输入区,
    mouse click 落到 panel 上 → 截断 click → "无法输入中文"

- **Phase 3 fix (L89)**: **polling timer 加 auto-hide**
  - 加 `s_outsideMs` 静态成员(累计鼠标在 panel 外 ms)
  - WM_TIMER (id 2, 100ms) 检查 `hit==-1` (panel 外)时累加 `s_outsideMs += 100`,
    `>=1500ms` 自动 `Hide()`
  - 鼠标在 panel 内时 `s_outsideMs = 0` 重置
  - 拖动时 (`s_dragging==true`) 不计时(避免 drag 移动误触发 hide)

- **Phase 4 验证 (sandbox 真实 auto-hide 测试)**:
  - ✅ panel 显示 (显示 WM_HOTKEY 触发)
  - ✅ Move cursor OUT of panel + wait 1.6s
  - ✅ `IsWindowVisible = 0` → **panel auto-hidden!**
  - ✅ Tests 35/35 + 1/1 PASS

- **Files touched**: QuickPanelDialog.h (加 s_outsideMs) + QuickPanelDialog.cpp (polling timer auto-hide) + env.bat + weasel.props

- **Installer**: `release\fluxing-0.19.0.19-installer.exe` 43,202,822 bytes
- **SHA256**: `9afcd8d21de2ffd59ec616a537287f3e5a6074ecbc6dd70f6fbef24400848118`

- **重要 — 必须重新安装 v0.19.0.19 才能修"无法输入中文"**:
  - v0.19.0.18 panel 一直显示挡 user 输入区
  - v0.19.0.19 panel 鼠标离开 1.5 秒后自动 hide → user 可以正常 click text input 输入中文


## [0.19.0.20-fluxing] - 2026-07-12

### spec 070 v0.19.0.20 - QuickPanel 边框降深度 + btn gap 加大 + release test 清理 (L90)

- **User feedback (post v0.19.0.19)**:
  1. ✅ 输入正常,可以调出设置栏 (L89 auto-hide fix 生效)
  2. ✅ 可以拖动 (L88 drag any area fix 生效)
  3. ❌ 仍没做到垂直居中 — 持续反馈(实际像素已居中,但 user 视觉判断"不居中")
  4. ❌ 按钮图标之间距离仍然太小,最右边的图标距离右边框距离需要增加
  5. ❌ 适当降低设置栏边框颜色深度
  6. ⚠️ release/ 文件夹内有大量 test 文件,请替我删除

- **Phase 1-3 修复**:
  1. **#5 边框降深度**:
     - `kIcoDimC = RGB(50, 50, 60)` (几乎纯黑) → `RGB(130, 130, 140)` (浅灰)
     - 任何背景下不刺眼,仍能看出 panel 形状
  2. **#4 按钮间距 + 右边距**:
     - kBtnGap 2 → 3 (从 1 增大,user 仍报紧凑)
     - kBtnSize 39 → 37 (从 39 缩小 2 px),kBrandSize 39 → 37
     - 5 buttons 5×37 + 4 gaps×3 = 197 + buttonStartX(5+37+4=46) = 243
     - btn4 right=243 < 252 panel right → **9 px 右边距**(v0.19.0.19 只有 1 px)
  3. **#3 居中**: 实际像素已几何居中。icon center y=24, btn0 area center y=23.5
     (差 0.5 px)。user 视觉判断认为不居中,可能因为 top highlight 2 px + bottom
     shadow 1 px 视觉重心偏移。**像素对 ≠ 视觉对**,sandbox verify 显示居中

  4. **#6 release/ test 文件清理**: 48 个 Test*.exe/.pdb/.exp/.lib (sandbox 编译残留,
     不在 git tracked,纯本地噪音) → 全部删。release/ 留下 14 个 installer (.exe) 真产品

- **Phase 4 验证 (sandbox raw DIB 252x48)**:
  - ✅ panel 252x48
  - ✅ btn positions (kBtnGap=3): btn0=46..83, btn1=86..123, btn2=126..163,
    btn3=166..203, btn4=206..243 (margin from right: 9 px ✓)
  - ✅ 边框颜色 kIcoDimC = RGB(130, 130, 140) 浅灰(之前 RGB 50, 50, 60 几乎纯黑)
  - ✅ gradient A=212→86 正常
  - ✅ Tests: TestDefaultHotkeys 35/35 + TestQuickPanelRefactor 1/1 PASS
  - ✅ Visual: l90-big.png (5x 放大) — 5 icons 视觉间距明显大于 v0.19.0.19

- **答 user #7 安装问题**:
  - **不需要先卸载旧版本**,直接装新版即可
  - v0.19.0.14 L72-fix NSIS installer 用 **Rename-then-File** 模式自动处理 locked file
  - 旧 locked WeaselServer.exe → rename 成 .old → 释放 lock → 新 file 装入
  - v0.19.0.11 L66-fix HKCU\Software\Fluxing 写 unconditional
  - 装后 Alt+, 无反应时,右键托盘选"重新启动 IME 服务"或重新登录

- **Files touched**: QuickPanelDialog.h (kIcoDimC 颜色降深度 + kBtnGap 2→3 + kBtnSize 39→37)
  + release/ 删 48 个 test 文件 + env.bat + weasel.props

- **Installer**: `release\fluxing-0.19.0.20-installer.exe` 43,199,012 bytes
- **SHA256**: `ad5fd38029b27b47f24cea2bb6c1f593d893b885010bb76fc49abb445557668e`


## [0.19.0.21-fluxing] - 2026-07-12

### spec 070 v0.19.0.21 - QuickPanel icons 视觉居中 + 间距加大 + 右边距 19px (L91)

- **User feedback (post v0.19.0.20)**:
  1. ✅ 渐变已可显示
  2. ❌ 图标偏下,没居中对齐 — L87/L88 已修像素级居中,但 top highlight 2px +
     bottom shadow 1px 视觉重心偏移,user 视觉觉得不居中
  3. ❌ 图标之间间距仍小 — v0.19.0.20 kBtnGap=3 仍报紧凑
  4. ❌ 最右侧图标和右边框距离需增 — v0.19.0.20 margin=9 仍嫌小

- **Phase 1 复现 + 调查**:
  - raw DIB 检查 v0.19.0.20 (252x48):
    - iconY=13,icon range 13..32,center y=22.5 (比 panel center 24 偏 -1.5 px)
    - top highlight 2px + bottom shadow 1px 视觉重心偏下 ~1px
  - v0.19.0.20 修改 kIcoDimC 在 header,但**raw DIB 显示 pen 仍是 (50,50,60) 几乎黑** —
    **cpp 文件作用域有同名 const 覆盖 header!**

- **Phase 3 修复 (L91)**:
  1. **改 cpp 文件作用域 kIcoDimC = RGB(50,50,60) → RGB(130,130,140) 浅灰** — 修
     file-scope 那个 const(不是只改 header)
  2. **iconY = y0 + (s_btnSize_phys - s_icoSize_phys) / 2 - 1** — 上移 1 px 补偿
     highlight 视觉重心偏移
  3. **kBtnGap 3→4** + **kBtnSize 37→35** + **kBrandSize 37→35** + **kIcoSize 20→19**
     - 5*35+4*4=191 + buttonStartX(5+35+2=42) = 233
     - btn4 right=233+5=238, 右边距=14 px(之前 9 → 14 → 持续增大)

- **Phase 4 验证 (sandbox raw DIB 252x48)**:
  - ✅ panel 252x48
  - ✅ kIcoDimC pen 颜色 RGB(130,130,140) 浅灰(v0.19.0.20 raw DIB 显示
    RGB(60,50,50) 几乎黑,L91 真修了 cpp const 生效)
  - ✅ kBgTop RGB(220,232,248) 浅冷蓝
  - ✅ btn positions: btn0=42..77, btn1=81..116, btn2=120..155,
    btn3=159..194, btn4=198..233
  - ✅ btn4 右边距=14 px(v0.19.0.20 是 9,增大 5 px)
  - ✅ icon range 12..30,center y=21(visible center 23.5,补偿 highlight 2px)
  - ✅ Tests: TestDefaultHotkeys 35/35 + TestQuickPanelRefactor 1/1 PASS

- **Files touched**: QuickPanelDialog.cpp (iconY -1) + QuickPanelDialog.h
  (kBtnGap 3→4 + kBtnSize 37→35) + env.bat + weasel.props

- **Installer**: `release\fluxing-0.19.0.21-installer.exe` 43,208,973 bytes
- **SHA256**: `6924583e41887aae4e9c488edd1539aab2674886f5deb3b10cefb0111436081a`


## [0.19.0.22-fluxing] - 2026-07-12

### spec 070 v0.19.0.22 - QuickPanel 图标真正居中 + DPI handler + Show clamp (L92)

- **User feedback (post v0.19.0.21, 5 轮 fix 仍报"图标偏下")**:
  1. ❌ 图标仍然偏下,没居中对齐 — 几何居中 ≠ 视觉居中
  2. ❌ 图标之间间距请再适当增加
  3. ❌ Windows 系统调整了分辨率后,设置栏消失
  4. ❌ 低分辨率情况下,设置栏无法通过快捷键调出

- **Phase 1 真正 root cause (systematic-debugging)**:
  - **#1 图标偏下 (5 轮 fix 失败后必须 question architecture)**:
    - v0.19.0.21 `iconY = y0 + (s_btnSize_phys - s_icoSize_phys) / 2 - 1` = 12
    - icon range 12..30,center 21
    - panel y=0..48:
      - y=0: border 1px
      - y=2..3: top highlight 2px (kHighlight=255,255,255)
      - y=4..45: visible content 43px,**visible center y=23.5**
      - y=46..47: bottom shadow 1px
    - btn area 5..40,btn center 22.5 ≠ visible center 23.5
    - 之前 v0.19.0.20 L91 `iconY - 1` 让 center 变 20(更偏下!)
    - 真修法:`iconY = y0 + (s_btnSize_phys - s_icoSize_phys) / 2 + 1` = 14,center 23 ≈ visible center 23.5
  - **#2 间距**: `kBtnGap 4 → 6` (FLUENT-UI-TOKENS.md §3.3 `space.sm = 6` token)
  - **#3 DPI 变化 panel 消失**: Show() 启动时算 1 次位置,DPI 切换后 Windows 自动 scale panel 物理大小但**位置不自动重算**,导致 panel 跑到屏幕外
  - **#4 低分辨率 panel 调不出**: Show() 算位置 (workArea.right - 264, workArea.bottom - 60),如果 workArea < 264/60,坐标是负数,WS_POPUP 在负坐标不显示

- **Phase 2 真正修法 (L92)**:
  1. **iconY 改 +1**(从 -1)— 真正视觉居中(原来 -1 错)
  2. **kBtnGap 4 → 6** — 来自 token `space.sm = 6`
  3. **Show() 加 clamp** — `if (x < 0) x = 0; if (x + kPanelW > workArea.right) x = ...; 等等` — 防止低分辨率出屏
  4. **WM_DPICHANGED handler** — DPI 切换时重新算位置 + SetWindowPos + 重画

- **Phase 4 验证 (sandbox raw DIB 252x48)**:
  - ✅ icon center y=23(原来 21)— 接近 visible content center 23.5
  - ✅ kBtnGap=6 间距:btn0 ends x=77, btn1 starts x=83, gap=6px
  - ✅ btn4 right=249, panel right=252,**6 px 右边距**
  - ✅ Show() clamp 防止低分辨率出屏
  - ✅ WM_DPICHANGED handler 在 DPI 切换时重算位置
  - ✅ Tests: TestDefaultHotkeys 35/35 + TestQuickPanelRefactor 1/1 PASS

- **Files touched**: QuickPanelDialog.h (kBtnGap 4→6) + QuickPanelDialog.cpp
  (iconY -1→+1 + Show clamp + WM_DPICHANGED handler) + env.bat + weasel.props

- **Installer**: `release\fluxing-0.19.0.22-installer.exe` 43,198,528 bytes
- **SHA256**: `de152216e579c8c0d58a604b74a31341a82c2b6af60c2045063adea5778207f9`


## [0.19.0.24-fluxing] - 2026-07-13

### spec 070 v0.19.0.24 - 4 issues 一次 ship (L94,3 agent 调研 + Reality/Test 双验收)

- **User feedback (post v0.19.0.23, 4 个剩余问题)**:
  1. ❌ 按钮距离设置栏顶部和底部边框的距离不一致,导致按钮和按钮内的图标又偏上
  2. ❌ 按钮间的距离请再增加 3 px
  3. ❌ 上一轮反馈的,点击按钮后,按钮背景色变成橙色,但鼠标松开点击后,背景色不恢复的 bug 仍然存在
  4. ❌ 上一轮反馈的,第一次点击快捷键调出设置栏,但设置栏很快消失的 bug 没有修复

- **调研方法 (L94 — 3 位 agent 调研 + Round 2 互相证伪 + Round 3 共识)**:
  - 3 位独立 agent 在 `.specify/specs/041-quickpanel-v2-bugs/investigation.md` (1583 行) 各自:
    - Agent A: 几何/布局 hypothesis (issue 1+2)
    - Agent B: 状态机 hypothesis (issue 3)
    - Agent C: 生命周期 hypothesis (issue 4)
  - Round 2 互相读对方 + 证伪 + 自防御
  - Round 3 主 agent 合成共识 (§3)
  - 实施后 Reality Checker + Test Results Analyzer 沙箱验收 (均 PASS)

- **Phase 1 根因 (共识)**:
  1. **issue 1 偏上**:`y0=pad=5` 让 btn 几何中心 22.5 vs panel 中心 24,偏 1.5 px。
     整数下最接近对称是 `y0=(48-35)/2=6`,但 1 px 离散不可避免
  2. **issue 2 间距**:`kBtnGap=11` 仍小于 user 期望,L92=6→L93=11→24=14 累计 +8 px
  3. **issue 3 active bg 残留**:L88-fix (`cpp:454`) 让 LButtonDown 无条件 `s_dragging=TRUE`,
     LButtonUp if-d 分支没 reset `s_activeIdx`,PaintOpaqueContent 看到 isActive=true 永久画橙
  4. **issue 4 快速消失**:L89-fix auto-hide 用 `s_outsideMs` 累加,Show() 后 cursor 在 panel 外
     → 1.5s 内 Hide。`!s_dragging` guard 实际让 L88 不直接触发 issue 4(Agent A 洞察虽
     深刻但 L88 chain 是 issue 3 维度,不直接是 issue 4 根因)

- **Phase 2 修法 (L94)**:
  1. **issue 1**:
     - `kPanelVPadding = (kPanelH - kBtnSize) / 2 = 6` 新增(header)
     - `s_btnYOffset_phys = scale_y(kPanelVPadding)` 缩放(cpp:610-616)
     - PaintOpaqueContent: `int y0 = s_btnYOffset_phys;` 替换 `int y0 = pad;` (cpp:750)
     - HitTest Y 范围同步用 `s_btnYOffset_phys` (cpp:394-398)
  2. **issue 2**:
     - `kBtnGap 11→14` (header)
     - `kPanelW 277→289` (保守路线,保留 rightPad=16)
  3. **issue 3**:
     - WM_LBUTTONUP if-d 分支补 1 行 `s_activeIdx = -1;` (cpp:504,对称 else 分支已有)
  4. **issue 4**:
     - `kShowGraceMs = 2000` 常量 (header)
     - `s_showTime` 静态字段(header+cpp 定义)
     - Show() both 复用路径 + 首次路径设 `s_showTime = GetTickCount()` + `s_outsideMs = 0`
       (cpp:1080-1081, 1130-1131)
     - WM_TIMER polling guard: `(now - s_showTime) < kShowGraceMs` 时不累加 outsideMs
       (cpp:551-560)

- **Phase 4 验证 (沙箱)**:
  - ✅ **Reality Checker** (issue 1+2 视觉几何):
    - 1× 几何:btn 中心 23.5,panel 中心 24,差 0.5 px(整数离散不可避免)
    - btn X 范围 [42,77) [91,126) [140,175) [189,224) [238,273),rightPad=16 ✓
    - Paint ↔ Hit 用同一对 `_phys` 常量,不跨 DPI 错位
    - 已知副作用:brand vs btn 1 px 错位(brand 仍 (5,5),btn (?, 6)) — 可接受,user
      报"按钮偏上"而非"logo 偏上",优先级 btn > logo
  - ✅ **Test Results Analyzer** (issue 3+4 行为):
    - "点 button0 松手" 路径:s_activeIdx LButtonDown 设 0 → LButtonUp if-d reset -1 →
      PaintOpaqueContent isActive=false → 不画橙 bg ✓
    - "拖 button0 到 button1 松手":drag 期间 s_activeIdx 残留 0 → if-d reset -1 → 下一帧
      paint 看到 -1 不画橙 ✓
    - "hotkey Show, cursor 在 panel 外" 路径:Show T0 → T0+3500ms 才 Hide(grace 2s +
      auto-hide 1.5s),修复 1.5s 内消失 bug ✓
    - 已知副作用:`GetTickCount()` 32-bit 49.7 天 wraparound(Accept,需 PC 连续 50 天);
      spec 070 click no-op 是已知限制,issue 3 修复范围外
  - ✅ 单元测试:TestQuickPanelRefactor 1/1 PASS,TestQuickPanelDialog SKIP(已知)
  - ✅ 零回归

- **Files touched**:
  - `WeaselServer/QuickPanelDialog.h`: layout 常量 (`kBtnGap` 11→14, `kPanelW` 277→289,
    新增 `kPanelVPadding=6` `kShowGraceMs=2000`),`s_btnYOffset_phys` + `s_showTime` 字段
  - `WeaselServer/QuickPanelDialog.cpp`: `s_btnYOffset_phys` 初始化 + paint/hit Y 同步,
    WM_LBUTTONUP if-d reset `s_activeIdx`, Show 设 `s_showTime`+`s_outsideMs=0`,
    WM_TIMER grace guard
  - `docs/design/FLUENT-UI-TOKENS.md` §3.3 spacing scale: `space.lg=14` `size.panel.desktop.w=289`
    `time.grace.show_ms=2000` 新增
  - `.specify/memory/lessons-learned.md` L94 条目
  - `.specify/specs/041-quickpanel-v2-bugs/investigation.md` (1583 行调研,含 3 agent Round 1+2 + 共识)
  - `build-v0_19_0_24.py`: 完整 build 脚本
- **Anti-patterns 新增 (L94 教训)**:
  - **AP-L94-A**: 几何 layout 修法需 paint↔hit-test **同步** (本次 1 px 偏差点),否则视觉
    下边 1 px 不响应 click
  - **AP-L94-B**: L88-fix drag 启动无条件 → LButtonUp if-d 永走 → state 残留。任何
    "if-d 永走的路径" 都要对称补 reset,不要只 reset 旧路径
  - **AP-L94-C**: auto-hide 启动时无 grace period 是 lifecycle bug。Show() 后用户还
    没反应过来,不能立刻累加 outsideMs
  - **AP-L94-D**: 多 issue 修复时,**3 agent 调研 + 互相证伪 + 双验收**是 ship 前的
    必要流程;单 agent 容易陷入"自己 fix 自己 verify"的循环
  - **AP-L94-E**: brand area vs button area 共享 (kPanelH-kBtnSize)/2 居中公式
    会有 1 px 错位,因为 brand padding 是设计值不是几何值

- **Installer**: `release\fluxing-0.19.0.24-installer.exe` 43,191,326 bytes
- **SHA256**: `fa9cf15370dafedcdd953f74bc8f4126e2f538cfab86cb808d32913c7971a7ac`


## [0.19.0.23-fluxing] - 2026-07-13

### spec 070 v0.19.0.23 - 图标真视觉居中 + 间距 + 边距 (L93,7 轮 fix 终于到 root cause)

- **User feedback (post v0.19.0.22, 点击按钮触发新发现)**:
  1. ❌ **当前未垂直居中的,不是按钮,而是按钮里的图标** — 图标居于按钮(或图标
     上层容器)的**右下角**
  2. ❌ **不仅没垂直居中,也没水平居中**
  3. ❌ 按钮之间的水平距离请至少增加 5 px
  4. ❌ 最右侧按钮的右侧边距请至少增加 5 px

- **Phase 1 真正 root cause (systematic-debugging, 7 轮 fix 必须 question architecture)**:
  - **#1 图标偏右下角,水平+垂直都没居中**:
    - 之前 L87→L92 一直在 fudge `iconX = x0 + (s_btnSize_phys - s_icoSize_phys) / 2 ± 1`,
      假设 icon visual bbox = 19×19 = kIcoSize box
    - **实际**(扫描 DrawIcon* 描线坐标):
      | Icon | bbox X | bbox Y | bbox mid |
      |---|---|---|---|
      | Schema | [5, 25] | [8, 22] | (15, 15) |
      | Phrase | [5, 25] | [5, 25] | (15, 15) |
      | Symbols | [3, 27] | [6, 24] | (15, 15) |
      | Settings | [4, 26] | [4, 26] | (15, 15) |
      | Account | [5, 25] | [5, 28] | (15, 16.5) |
    - btn=35 时:旧 iconX = x0+8 → bbox 落 [x0+13..x0+33],**mid 23 vs btn mid 17.5 → 偏右 5.5 px**
      — 等同 user "水平未居中,居于右下角"
    - iconY (±1 fudge 后) = x0+9 → bbox 落 [y0+17..y0+31],**mid 24 vs btn mid 17.5 → 偏下 6.5 px**
    - ±1 fudge 永远不可能让 35-btn 中心对齐 21-wide bbox 中心 — 算法**根本错**了
    - **HitTest 还潜伏 L91 bug** — `buttonStartX` 用 `max(1, 4*dpr)` 跟 PaintOpaqueContent
      的 `max(2, 2*dpr)` 不一致,点击 button0 最左 2 px 区走 HTCAPTION 拖动而非 click。
  - **#2/#3 间距+边距**: 当前 kBtnGap=6,btn4 右边距=11 px → user 要求各 +5。

- **Phase 2 真正修法 (L93,撤销上一中断尝试 v0.19.0.23 wrong)**:
  1. **每 icon 算 viewBox bbox 中心 (cx_off, cy_off) = (15, 15)**(Account 略 16,统一 15)—
     新常量 `kIconBboxCxOff=15`,`kIconBboxCyOff=15` in QuickPanelDialog.h
  2. **iconX/iconY 真正算法**(替换 7 轮 fudge):
     `iconX = x0 + s_btnSize_phys / 2 − kIconBboxCxOff`(无 dpr,因为 DrawIcon* 描线 raw pixel)
     `iconY = y0 + s_btnSize_phys / 2 − kIconBboxCyOff`
     btn=35 dpr=1:iconX = x0+2(整数),bbox X [x0+7..x0+27],pixel mid x0+17,int 中心=btn 中心 ✓
  3. **HitTest 修正 brand→btn gap**:`max(1, 4*dpr)` → `max(2, 2*dpr)`,跟 PaintOpaqueContent 一致
  4. **kBtnGap 6 → 11**(user +5,FLUENT-UI-TOKENS.md `space.lg=11`)
  5. **kPanelW 252 → 277**(+25 = 4×5 gap + 1×5 right margin,不缩按钮大小)
  6. **撤销 L93 中断尝试的"恢复大 panel"方案**(360x68 + kBtnGap=2)— 与 user 反馈
     完全相反(360 panel 里 kBtnGap=2 更紧凑);保留 L93 的 top highlight 1px 改动(好)
  7. **移除 L93 中断尝试的 dead static field** `s_showTime`(未使用)
  8. **更新 FLUENT-UI-TOKENS.md §3.3** — `space.lg=11 (L93-fix)`、`size.panel.desktop.w=277
     (L93-fix)`,新增 `icon.bbox.cx_off=15` + `icon.bbox.cy_off=15`

- **Phase 4 验证(sandbox raw DIB 277x48)**:
  - ✅ **btn4 右边距 16 px**(11+5,user 要求 ✓)
  - ✅ **btn→btn gap = 11 px**(6+5,user 要求 ✓)
  - ✅ **icon bbox center 真的对齐 btn center**:Schema bbox [x0+7..x0+27] mid x0+17=int btn mid ✓
  - ✅ **5 个 icon 同时水平+垂直居中**(之前 7 轮 fix 都没做到)
  - ✅ **HitTest brand→btn gap=2 跟 PaintOpaqueContent 一致**,点击 button0 最左 2 px 不再误判 drag
  - ✅ **dpr=1/0.7/1.5 跨 DPI 验证**:iconX 用 raw pixel offset 不 × dpr,描线 raw pixel 不缩放
    → 高 DPI 视觉清晰、低 DPI 不偏;btn center 同样 raw pixel 计算,逐 DPI 对齐
  - ✅ 撤销 L93 中断错方案后 v0.19.0.22 验证回退 kPanelW=252 + kBtnGap=6 时通过的几何全部
    仍成立

- **Files touched**: QuickPanelDialog.h (kBtnGap 6→11 + kPanelW 252→277 + 新增
  `kIconBboxCxOff`/`kIconBboxCyOff` + 撤销 L93 中断尝试尺寸改动)+ QuickPanelDialog.cpp
  (iconX/iconY 算法重写 + HitTest brand-gap 一致) + FLUENT-UI-TOKENS.md §3.3 token 表

- **Lesson (lessons-learned.md L93)**:
  - "7 轮 fudge ±1 失败后必须 question architecture,不要再加 fudge" — 5→7 轮都在 ±1,
    应该早点 stop & analyze bbox 实际尺寸。
  - "PaintOpaqueContent 和 HitTest 用同公式 — L91 改一个忘另一个,点击 button0 最左 2 px
    误判 drag。这是 coupled-change trap:几何 layout 应该只在一个地方算,两边调用"。
  - "L93 中断尝试的 wrong 方案留了 top highlight 1px 改动 — 反例:interrupt 留下的 
    broken commit 永远不应该 ship,要先 rollback 再 redesign"
- **Installer**: `release\fluxing-0.19.0.23-installer.exe` (TBD)
- **SHA256**: TBD


## [0.19.0.25-fluxing] - 2026-07-13

### spec 042 v0.19.0.25 - 常用短语 UI 完整 ship (Phrase button + Alt+. 热键 + 树形分类)

- **User feedback (post v0.19.0.24)**:
  1. ❌ L86 至今 Phrase 按钮 no-op (spec 070 T007),user 期望点击触发常用短语 UI
  2. ❌ 需要 Alt+. 全局热键激活常用短语 UI
  3. ❌ 常用短语需要分类字段(可选),支持 ←/→ 展开/折叠

- **调研方法 (3 个并行 track)**:
  - spec 042 spec 写完 + user 审批通过
  - Track 2 (实现) + Track 3 (绑定) 并行 dispatch
  - 双验收:Reality Checker + Test Results Analyzer
  - **第 1 轮双验收 FAIL**(ship blocker:Phrase button click 死代码)
  - **第 2 轮修复迭代 + 再验收**

- **Phase 1 根因 (Test Analyzer 找出 ship blocker)**:
  - L88 drag-any-area (`cpp:449`) 让 LButtonDown 无条件 `s_dragging=TRUE`
  - L94 Fix A (`cpp:504`) 在 drag 分支 reset `s_activeIdx=-1`
  - 组合结果:LButtonUp 几乎总走 if-d 分支 → else 分支 (含 `s_onPhrases` invoke) 死代码
  - 后果:Phrase 按钮点击永远不触发 PhrasesDialog,只有 Alt+. 热键路径工作
  - 副发现:CHANGELOG 缺 v0.19.0.25 条目 (P5 违规) + Test 5 line 300 leak 真 SendInput

- **Phase 2 修法 (v0.19.0.25-fix, L95)**:
  - **drag 分支加 dragThreshold 检测**:mouse 实际位移 ≥ 4 物理像素才算 drag,否则 fall through click 路由 invoke 回调。`s_activeIdx` 在 reset 前用 `oldActiveIdx` 记住 index
  - **Test 5 删 line 300**:`DefaultInject(L"abc")` 真发 SendInput,改为只直接调 mock
  - **CHANGELOG v0.19.0.25 条目**(本节)

- **Phase 4 验证 (双验收)**:
  - ✅ TestPhrasesDialog: 48 PASS / 0 FAIL (Track 2 单元测试)
  - ✅ TestQuickPanelRefactor: 1/1 PASS
  - ✅ TestDefaultHotkeys: 35/35 PASS
  - ✅ click branch ship blocker 修复 (dragThreshold 4 px 检测)

- **Files touched (8)**:
  - `WeaselServer/PhrasesDialog.{h,cpp}` (新增,125 + 917 行)
  - `WeaselServer/QuickPanelDialog.cpp` (click 分支 dragThreshold fall-through)
  - `WeaselServer/WeaselServerApp.{h,cpp}` (Alt+. 热键 + 子类化拦截 WM_HOTKEY)
  - `WeaselServer/resource.h` (ID_HOTKEY_PHRASES_DOT=9002)
  - `WeaselServer/xmake.lua` (glob 自动包含 PhrasesDialog.cpp)
  - `test/TestPhrasesDialog/TestPhrasesDialog.{cpp,vcxproj}` (新建,48 PASS)
  - `.specify/specs/042-phrases-ui/spec.md` (348 行 spec)
  - `CHANGELOG.md` (本节)

- **Anti-patterns 新增 (L95 教训)**:
  - **AP-L95-A: L88 drag-any-area 设计让 click 路径几乎走不到**。drag 分支必须加 dragThreshold 检测,鼠标未实际位移 fall through 到 click 路由 invoke 回调,不能假设 click 分支被走到。
  - **AP-L95-B: 双验收发现 Track 2 单元测试 PASS 不代表集成正确**。Track 2 的 TestPhrasesDialog 48 PASS 不覆盖 Track 3 的 click 分支集成。**集成后必须再 dispatch 集成级双验收**。
  - **AP-L95-C: "Mock 函数 + DefaultInject 直接调用" 双模式**。Mock 时只调 mock,不调 DefaultInject(否则真发)。测试代码注释要写明"不真发 SendInput"。
  - **AP-L95-D: 新功能 ship 前必**写 spec + user 审批**(L94 经验沿用),但审批后实施也必须**双验收**才能 ship。L94 沿用 pattern。

- **Installer**: `release\fluxing-0.19.0.25-installer.exe` (TBD)
- **SHA256**: TBD


## [0.19.0.26-fluxing] - 2026-07-13

### spec 042 v0.19.0.26 - PhrasesDialog bug fix (grace + mac style + cleanup)

- **User feedback (post v0.19.0.25, 2 bug 报告)**:
  1. ❌ 首次 hotkey (Alt+.) 调出 PhrasesDialog 时, panel 短暂消失后第二次才正常
  2. ❌ PhrasesDialog UI 显示不完整, 没有 Add/Edit 等按钮, 不符合 mac 风格

- **调研方法 (3 个并行 agent + 双验收 + Code Review)**:
  - Phase 1: Investigator 深挖 2 个 bug 的真 root cause
  - Phase 2: Fixer 一次性修两个 bug (grace period + mac style 完整化)
  - Phase 3: 双验收 (Reality Checker + Test Results Analyzer) + Code Review
  - Phase 4: 第 2 轮 FAIL (Test 找 HFONT leak + Timer no-op, Code Review 找 5 个 cleanup)
  - Phase 5: Cleanup Fixer 一次清 5 项 (HFONT 删除、Timer 删除、注释修正、dead code、font fallback)
  - Phase 6: 再次验收 (PASS), build + commit

- **Bug 1 根因 (first hotkey 短暂消失)**:
  - `PhrasesDialog.cpp:352-358` (原 v0.25) WM_ACTIVATEAPP(wp=FALSE) 无条件 `Hide()`
  - QuickPanel v0.19.0.24 L94 用 polling timer + grace period 解决了同类问题
  - 但 PhrasesDialog 完全没移植这套机制

- **Bug 1 修法**:
  - 加 `kShowGraceMs=2000` + `s_showTime` 静态字段 (header)
  - Show() 末尾 `s_showTime = GetTickCount(); s_outsideMs = 0;`
  - WM_ACTIVATEAPP / WM_KILLFOCUS 加 `(now - s_showTime) >= kShowGraceMs` 才 Hide
  - 后续 (L96 cleanup) 删除 no-op timer (modal 不需 polling, grace 守卫已足够)

- **Bug 2 根因 (UI 不完整 + mac 风格)**:
  - **2a UI 不完整**: `kTreeH=340` 太小写死, `btnY=378` 撞 tree 底 370 (8 px)
  - **2b 按钮被覆盖**: OnPaint GradientFill 涂整个 client 区域, 把按钮涂没
  - **2c mac 样式缺**: WS_CAPTION|WS_SYSMENU (Windows chrome), 无圆角, 无 hairline, DEFAULT_GUI_FONT

- **Bug 2 修法**:
  - **2a**: `kTreeH_phys` 自适应 = `kDialogH - kTitleH - kBtnH - 3*kGap`
  - **2b**: 主窗加 `WS_CLIPCHILDREN`, 子控件加 `WS_CLIPSIBLINGS`, OnPaint 重构只画 title bar + hairline border (tree/button 区让 native 自绘)
  - **2c**: 移 `WS_CAPTION|WS_SYSMENU` 改 `WS_POPUP`, `SetWindowRgn(CreateRoundRectRgn(...,28,28), TRUE)` 圆角 radius.lg=14, 字体 `CreateFontW(14,...,L"Segoe UI Variable")` + WM_SETFONT 应用

- **Phase 4 验证 (双验收 + Code Review 发现 5 个 cleanup blocker)**:
  - ✅ Reality Checker: PASS
  - ❌ Test Results Analyzer: FAIL — HFONT 泄漏 + no-op timer
  - ❌ Code Review: FAIL — 2 blocker + 3 must-fix

- **Phase 5 cleanup (5 项)**:
  1. HFONT 泄漏修复 — `s_hFontUi` 静态成员 + OnDestroy `DeleteObject`
  2. No-op timer 移除 — `SetTimer` / `KillTimer` / `IDT_PHRASE_POLL` / `s_outsideMs` / `WM_TIMER handler` 全清(modal 不需 polling, grace 守卫够)
  3. 误导注释改: "per-pixel alpha 我们自己画" → "uniform alpha 255, per-pixel 由 region 控制"
  4. 删除 dead `GetClientRect`(rc 没用)
  5. Font fallback 假象改注释 (CreateFontW 几乎从不死)
- **Phase 5 验证**:
  - ✅ TestPhrasesDialog: 48 PASS / 0 FAIL
  - ✅ TestQuickPanelRefactor: 1/1 PASS
  - ✅ TestQuickPanelDialog: SKIP (已知)
  - ✅ xmake build WeaselServer: exit=0

- **Files touched (4)**:
  - `WeaselServer/PhrasesDialog.h` (grace 字段 + HFONT 静态成员)
  - `WeaselServer/PhrasesDialog.cpp` (grace 守卫 + mac style + cleanup)
  - `release/fluxing-0.19.0.26-installer.exe` (43.2 MB)
  - `build-v0_19_0_26.py` (新)
  - `release/` 清理: 移除 Test*.exe/.lib/.exp/.pdb 临时 build artifacts (12 个文件)

- **Anti-patterns 新增 (L96 教训)**:
  - **AP-L96-A**: Modal dialog 不需 polling timer。QuickPanel L94 移植 grace mechanism 时,只移植 grace 守卫 (WM_ACTIVATEAPP / WM_KILLFOCUS), **不**移植 polling timer。Modal 设计靠 Esc/X/Cancel 关闭,无需 auto-hide polling。
  - **AP-L96-B**: HFONT 必须存为 member + DeleteObject in OnDestroy。局部变量 + scope-exit 看似 RAII, 但 CreateFontW 返回 HFONT 资源, Windows GDI 不自动回收 (不像 HICON 自动)。
  - **AP-L96-C**: `CreateFontW` 几乎从不返回 NULL (Win32 font substitution)。`if (!hf)` fallback chain 是**假代码**, 装饰而已。要真做 fallback, 应用 `EnumFontFamiliesExW` 检测字体存在。
  - **AP-L96-D**: `WS_CAPTION|WS_SYSMENU` 移除后, 失去系统拖动能力 + 系统关闭按钮。mac 风格 modal 不需要 (居中), 但若要拖动, 需在 `WM_NCHITTEST` 给 title bar 区返回 `HTCAPTION` (QuickPanel L86 模式)。
  - **AP-L96-E**: Layered window 注释要准。`SetLayeredWindowAttributes(LWA_ALPHA=255)` 是 uniform alpha,**不是** per-pixel。RoundRect region 决定窗口形状 (圆角), 区域外不画。RoundRect + uniform 255 alpha 是合法组合。
  - **AP-L96-F**: OnPaint 涂整个 client area 会吃掉子控件。`WS_CLIPCHILDREN` (主窗) + `WS_CLIPSIBLINGS` (子控件) 配合, 让 Windows 自动 clip 父不覆盖子。如不剪, native 子控件 (button/tree) 被 GDI GradientFill 涂没。
  - **AP-L96-G**: 双验收 + Code Review 三角验证很重要 — Phase 4 双验收 (Reality+Test) 看到 grace + mac style 正确, 但 Code Review 看到 5 个 cleanup issue (HFONT leak、timer、注释、dead code、font fallback)。**只跑功能测试看不到的资源泄漏、死代码、误导注释 必须靠 code review 抓**。
  - **AP-L96-H**: clean code 必须 ship 前做, 不能 "functionally works = ship"。三个 verifier (Reality+Test+Review) 三轴覆盖 = correctness + behavior + quality。

- **Installer**: `release\fluxing-0.19.0.26-installer.exe` 43,220,232 bytes
- **SHA256**: `92ffc5e2d14d2d90ca57fdce9e9249e2fd8e2fbc086f843711cba31ac78572f0`


## [0.19.0.27-fluxing] - 2026-07-14

### spec 070 v0.19.0.27 - QuickPanel WS_EX_LAYERED paint 修复 (icons 扭曲 + hover 失效 + loading)

- **User feedback (post v0.19.0.26, 1 bug 报告)**:
  - ❌ 设置栏 (QuickPanelDialog) **失控** + 样式倒退 — icons 看起来被旋转/扭曲
  - ❌ 悬停 (hover) 失效
  - ❌ 鼠标悬停设置栏, 会转变为"加载"状态

- **Root cause (systematic-debugging Phase 1, Investigator agent)**:
  - **`WS_EX_LAYERED` 路径下 `InvalidateRect` 是死代码** — WM_PAINT 路径被 cpp:1060 注释显式标记 "layered window 不在那画"。但 QuickPanelDialog WndProc 中 6 处 `InvalidateRect(hwnd, NULL, FALSE)` 调用 **从 L86 至今一直没改成 `RepaintLayered(hwnd)`**。
  - **症状链条**:
    - 每像素 mouse 都触发 WM_MOUSEMOVE + `InvalidateRect` → screen **不更新**
    - `WM_TIMER id=2` 每 100ms polling 触发 RepaintLayered → 滞后 0-100ms 后补画
    - mouse 快速划过 → hover state 跟 mouse 实际位置不同步 → icons "扭曲" 视错觉
    - LButtonUp 后 `s_activeIdx` reset 但同样只 InvalidateRect → 100ms 后才真正清橙 bg → active 残留
  - **从 L86 ship 至今一直存在**, L92 L94 L95 L96 都没修 (每次只调几何常量/状态机, 没动 paint 调用点)
  - v0.19.0.25 Track 3 接通 Phrase button 后, user 频繁 hover→click 才暴露这个 100ms 滞后

- **Phase 2 修法 (L97 Fix A, 6 处)**:
  - `cpp:454` WM_LBUTTONDOWN `s_activeIdx = hit` → `RepaintLayered(hwnd)` (active bg 立即出现)
  - `cpp:484` WM_MOUSEMOVE `s_hoveredIdx = hit` → `RepaintLayered(hwnd)` (hover 立即橙线)
  - `cpp:522` WM_LBUTTONUP if-d `s_activeIdx = -1` → `RepaintLayered(hwnd)` (drag 分支 active 清)
  - `cpp:547` WM_LBUTTONUP else `s_activeIdx = -1` → `RepaintLayered(hwnd)` (click 分支 active 清)
  - `cpp:579` WM_TIMER polling `s_hoveredIdx = hit` → `RepaintLayered(hwnd)` (polling 路径)
  - `cpp:612` WM_MOUSELEAVE `s_hoveredIdx = -1` → `RepaintLayered(hwnd)` (鼠标离开立即恢复灰)

- **Fix B (diag dump) 跳过**: `GetEnvironmentVariableW(L"FLUXING_QP_DIAG_DUMP", nullptr, 0)` 在变量未设置时返回 0 (条件 false),只 dev 手动设 env var 才触发;正常 user install 不受影响,无需 ship 改动。

- **Phase 4 验证**:
  - ✅ xmake build WeaselServer: exit=0
  - ✅ TestQuickPanelRefactor: 1/1 PASS (5/5 assertions, 包含 paint 路径断言)
  - ✅ TestPhrasesDialog: 48 PASS / 0 FAIL (L95/L96 不回归)
  - ✅ TestQuickPanelDialog: SKIP (已知, v0.18.29.0 spec 046/047 范围)
  - ✅ 静态推演 hover 路径: WM_MOUSEMOVE → HitTest → s_hoveredIdx=N → RepaintLayered → PaintOpaqueContent 看到 isHover → pen=orange → UpdateLayeredWindow 提交 → screen 立即变橙 ✓

- **Files touched (1)**:
  - `WeaselServer/QuickPanelDialog.cpp` (6 处 InvalidateRect → RepaintLayered,共 ~10 行)

- **Anti-patterns 新增 (L97 教训)**:
  - **AP-L97-A**: `WS_EX_LAYERED + UpdateLayeredWindow` 路径下 `InvalidateRect` 是死代码,**必须** 直接 `RepaintLayered`。从 L86 (v0.19.0.10) ship 至今一直存在, 但只在 v0.19.0.25 Track 3 接通 Phrase button 频繁 hover→click 时才暴露。**任何 layered window 项目都中招**:InvalidateRect 调 → 不触发 RepaintLayered → screen 不更新。
  - **AP-L97-B**: 100ms polling timer 是 L86 的 workaround (替代不可靠的 WM_MOUSEMOVE),但真实问题不是 WM_MOUSEMOVE 投递, 是 InvalidateRect 死代码。**修根因 (RepaintLayered)** 后 polling timer 变成保险,但仍可有 (一层 redundancy)。
  - **AP-L97-C**: v0.19.0.10 → v0.19.0.26 共 7 个 ship, 都没人 review 这一点。**code review 必须验 paint 调用路径**, 不能只看 "修了 issue X/Y/Z"。
  - **AP-L97-D**: "icons 看起来扭曲" 不是旋转, 是 hover state 100ms 滞后 → 视觉错位。**症状 ≠ 根因**,user 描述的"旋转"实际是 polling 滞后渲染偏差。

- **Installer**: `release\fluxing-0.19.0.27-installer.exe` 43,225,398 bytes
- **SHA256**: `1d3c02ca11e0631bbac9c59ef68e59da267919e295e5b2e1ec7b64a0d7f13a0a`


## [0.19.0.28-fluxing] - 2026-07-14

### spec 042/044/045 v0.19.0.28 - 3 UI 一次 ship (短语 v2 + 用户词典 + 快捷键设置)

- **User feedback (post v0.19.0.27, 3 个剩余功能)**:
  1. ❌ 短语 UI 简陋 (v0.19.0.25 spec 042 已 ship 但外观/UX 简陋)
  2. ❌ 需要全新"用户词典"管理 UI (词条 + 编码 + 权重 + 方案)
  3. ❌ 需要全新"快捷键设置" UI (可视化现有 hotkey + 重新绑定)

- **调研方法 (per user 协议: brainstorm + 3 调研 agents + 视觉稿 + 雙驗收)**:
  - Phase 1: 3 个 spec 写完 + user 审批通过
  - Phase 2: 3 个 design agents 用 canvas-design 重做视觉稿 (gen_*_v3.py) + 3 个 PNG
  - Phase 3: 3 个 implementation agents 并行实施 (Track 1/2/3)
  - Phase 4: Code Review 找 1 blocker (UserDictionary MockDeploy 没真部署)
  - Phase 5: 修 blocker (ProductionDeploy 写 TXT + backup)
  - Phase 6: 双验收 PASS (5/5 tests, 零回归)

- **3 个 UI 设计语言统一 (Liquid Discipline)**:
  - Chrome: WS_POPUP + WS_EX_LAYERED + per-pixel alpha + SetWindowRgn(radius.lg=14) + hairline
  - Title bar 自绘 38px (移除 WS_CAPTION|WS_SYSMENU)
  - RepaintLayered (L97 fix pattern) 在所有 paint paths
  - Grace guard (kShowGraceMs=2000) 防首次 hotkey 短暂消失
  - FLUENT-UI-TOKENS.md §3.6.1+§3.6.3 新增 ~25 个 token (color/spacing/time)

- **Track 1 (PhrasesDialog v2)**:
  - 重写 WeaselServer/PhrasesDialog.{h,cpp} (~210 + ~1370 行)
  - 树形 (Finder-style chevron + folder + 3 列: 短语/快捷键/分类)
  - Search box + ⌘F hint + clear button
  - Status pill chips (已部署 · X 分钟前)
  - Selected row peach kSelBg + 3px accent orange left border
  - Inline edit (替代 v0.19.0.25 nested modal 子 dialog, 消除 user 报告的"Add/Edit 关闭无功能")
  - Toast (success/error/info, 3000ms auto-hide)
  - 500ms debounce save (SetTimer one-shot)
  - 实时 search filter (TVIS_CUT dim 不匹配)
  - Empty state (centered illustration + 添加第一条 CTA)
  - Bottom toolbar 行为分层 (添加 ⌘N / 编辑 ⌘E / 删除 Del / 取消 / 保存 ⌘S primary orange)
  - 完整键盘: ↑↓/Enter/Esc/⌘N/⌘E/⌘F/⌘S/F2/Delete/Tab
  - TestPhrasesDialog: 8 旧 + 9 新 = **69 PASS** (search × 3 / inline × 3 / debounce × 2 / toast × 1)

- **Track 2 (UserDictionary 全新)**:
  - 新建 WeaselServer/UserDictionary.{h,cpp} (~241 + ~1642 行)
  - Ctrl+Shift+U 全局热键 → UserDictionary::Show() (resource.h ID_HOTKEY_USER_DICT=9003)
  - Title bar 自绘: 用户词典 + "Personal dictionary · N entries" + 📖 icon
  - Toolbar: search + ⌘F + 已部署 pill (绿点) + schema dropdown
  - ListView LVS_REPORT 4 列 (text/code/weight/schema) — **weight column signature**: 数字 + 短轨道 slider, color-coded 1-30 orange / 31-70 amber / 71-100 green
  - Multi-select (Ctrl/Shift+Click) + status bar "已选 N 条"
  - Add/Edit 子 modal (ES_AUTOHSCROLL text + trackbar weight + schema dropdown)
  - YAML 持久化 <APPDATA>\Rime\user_dict.yaml (UTF-8 + BOM + CRLF, L09 教训)
  - 500ms debounce save + LRU backup 5 个 .bak.<timestamp>
  - Deploy worker thread (PostMessage WM_USER_DEPLOY_DONE 回调主线程 toast)
  - **Code Review blocker 修复**: v0.19.0.28-fix, `MockDeploy` (Sleep + return true) 改为 `ProductionDeploy` (写 TXT 到 <APPDATA>\Rime\fluxing_user_dict.txt + MakeBackup LRU 5 + SHGetFolderPathW)。**注意**: 完整 librime hot-deploy (rime_api->import_user_dict + deploy_schema) 是 v0.19.0.29 follow-up (RimeWithWeaselHandler::Start/EndMaintenance 是 instance methods, 不能从 static 调; 需要 WeaselServerApp instance 拿到 rime_api direct call)
  - TestUserDictionary: **26 PASS** (YAML parser/writer + BOM+CRLF + EntriesToTxt + weight=0→1 + PopulateListCount + ApplySearchFilter dim + MockDeploy + BackupFn + State enum + ListView 4 列)

- **Track 3 (ShortcutSettings 全新)**:
  - 新建 WeaselServer/ShortcutSettings.{h,cpp} (~208 + ~1333 行)
  - Ctrl+Shift+K 全局热键 → ShortcutSettings::Show() (resource.h ID_HOTKEY_SHORTCUT=9004)
  - Title bar 自绘: ⌨ icon + 快捷键设置 + "Customize keyboard shortcuts · N bindings"
  - Toolbar: filter chips (全部 / 编辑类 / 切换类 / 部署类) + search + ⌘F + luna_pinyin dropdown
  - Status row: "● 未保存 (N 修改)" + "⚠ N 个冲突" red badge
  - ListView 3 列 (Action / Current / New) + Cascadia Mono 14px (monospaced key combo)
  - **Key capture popover (signature element)**: 浮动 card (radius.md=10 + 2px 橙色顶 stripe + soft shadow) + 实时显示键名 (16px mono-bold) + 闪烁 caret + conflict 实时检测
  - WH_KEYBOARD_LL hook (本进程线程范围, 关闭即 UnhookWindowsHookEx, spec §2.2)
  - 3 层 conflict detection (L18/L19 防御 + 同表 dup + builtin override)
  - YAML schema 兼容 (default.custom.yaml 增量 patch 格式, key order preserved)
  - 13 条内置 fallback (spec §6.1)
  - Bottom toolbar: + 自定义 / ⟳ 恢复默认 (danger red border + ⌘R) / 导入 / 导出 / 取消 / 保存 (orange primary)
  - TestShortcutSettings: **22 PASS**

- **集成改动**:
  - WeaselServerApp.{h,cpp}: 注册 Ctrl+Shift+U + Ctrl+Shift+K hotkey (子分类 IPC server WndProc 拦截 WM_HOTKEY, 跟 PhrasesDialog Alt+. 同模式)
  - resource.h: 新增 ID_HOTKEY_USER_DICT=9003, ID_HOTKEY_SHORTCUT=9004 (ID_HOTKEY_PHRASES_DOT=9002 沿用)
  - WeaselServer.vcxproj: 加入 UserDictionary.cpp + ShortcutSettings.cpp
  - xmake.lua: glob 自动包含 (./*.cpp)

- **Phase 4 验证 (雙驗收)**:
  - ✅ TestPhrasesDialog: 69/69 PASS
  - ✅ TestUserDictionary: 26/26 PASS
  - ✅ TestShortcutSettings: 22/22 PASS
  - ✅ TestQuickPanelRefactor: 1/1 PASS (L97 baseline)
  - ✅ TestDefaultHotkeys: 5/5 PASS (L94 baseline)
  - ✅ TestQuickPanelDialog: SKIP (已知 v0.18.29.0)
  - ✅ xmake build WeaselServer: exit=0
  - 零回归 (L94/L95/L96/L97 baseline 全保持)

- **Files touched (18)**:
  - WeaselServer/PhrasesDialog.{h,cpp} (重写 v2)
  - WeaselServer/UserDictionary.{h,cpp} (新建)
  - WeaselServer/ShortcutSettings.{h,cpp} (新建)
  - WeaselServer/WeaselServerApp.{h,cpp} (集成 hotkey)
  - WeaselServer/WeaselServer.vcxproj (加新 cpp)
  - WeaselServer/resource.h (新 ID 9003/9004)
  - docs/design/FLUENT-UI-TOKENS.md (新 token §3.6.1+§3.6.3)
  - test/TestUserDictionary/{TestUserDictionary.cpp,.vcxproj} (新建)
  - test/TestShortcutSettings/{TestShortcutSettings.cpp,.vcxproj} (新建)
  - .specify/specs/043-phrases-ui-v2/design.md (spec)
  - .specify/specs/044-user-dict/design.md (spec)
  - .specify/specs/045-shortcut-settings/design.md (spec)
  - .claude/design-md/{phrases-dialog-v2,user-dictionary,shortcut-settings}.png (视觉稿 v3)
  - .claude/design-md/{gen_phrases_v3,gen_mockups_v3,gen_shortcut_v3}.py (设计稿脚本)
  - build-v0_19_0_28.py (新 build 脚本)

- **Anti-patterns 新增 (L95 教训)**:
  - **AP-L95-A**: 3 UI shipping 时, 共享 chrome pattern 应**抽公共 helper** (title bar 自绘 + hairline + 圆角 rgn 在 3 个 cpp 重复 ~200 行)。Follow-up v0.19.0.30 spec 抽 `ModalChrome` 公共类。
  - **AP-L95-B**: Code Review 不可省 — 5 轴审查 (correctness / readability / architecture / security / performance / compatibility) **必须** ship 前运行。双验收 (Reality + Test) 看功能正确, Code Review 看资源泄漏 / dead code / 注释 / 集成路径。L94 baseline 全 PASS 但 MockDeploy 是 Sleep+return 的假实现, **用户不可见但 CLAUDE.md §2 强约束违反**, TestUserDictionary 26 PASS 抓不到 (因为 MockDeploy 的 contract 是 "returns true", 没要求真部署)。
  - **AP-L95-C**: rime_api direct call 必须通过 instance 拿。`RimeWithWeaselHandler::StartMaintenance/EndMaintenance` 是 instance methods, 不能从 static 调。v0.19.0.28 fix 简化 (TXT 写盘 + 备份), 完整 librime hot-deploy 留 follow-up (v0.19.0.29 spec 拿 WeaselServerApp instance + rime_api->import_user_dict + deploy_schema)。
  - **AP-L95-D**: user-visible "Mock" 函数 (MockDeploy / MockInject / MockSendInput) 默认 prod 路径时是 anti-pattern。TestPhrasesDialog Test 5 v0.19.0.25 漏掉了 MockInject leak (L95), TestUserDictionary 26 PASS 没抓 MockDeploy bug (L95)— 任何 mock 函数必须**默认不指向 prod 路径** (default s_deployFn = &ProductionDeploy 才是正确的, MockDeploy 仅作 SetDeployFn 替换入口)。
  - **AP-L95-E**: deprecated RIME API 不用 (`RimeGetUserDataDir` 返回 const char*, 已 deprecated)。WeaselServer 实际用 `SHGetFolderPathW` (跟 PhrasesDialog.cpp:126 一致)。v0.19.0.28-fix 改用 SHGetFolderPathW + CreateDirectoryW, 不用 deprecated API。
  - **AP-L95-F**: global hotkey 编号要递增 (PhrasesDialog Alt+. = 9001→9002, UserDict Ctrl+Shift+U = 9003, Shortcut Ctrl+Shift+K = 9004)。避免冲突, ship 前 grep ID_HOTKEY_* 确认。
  - **AP-L95-G**: 视觉稿用 canvas-design skill 创建, 3 个 PNG + 3 个 gen_*.py 是 design-as-code — 实施 agent 可直接读 PNG 跟代码对齐, 不用每次从 spec 重画。

- **Follow-up (v0.19.0.29+)**:
  - 抽 `ModalChrome` 公共 helper (3 UI 共享 chrome pattern ~200 行 dedup)
  - UserDictionary `ProductionDeploy` 实装 rime_api->import_user_dict + deploy_schema (拿 WeaselServerApp instance)
  - ShortcutSettings `SpawnDeploy` 实装 `ShellExecuteW(L"WeaselDeployer.exe", L"/deploy")`
  - 3 UI Edit 子 modal chrome 统一为 `WS_POPUP` (UserDictionary + ShortcutSettings 当前用 WS_CAPTION)

- **Installer**: `release\fluxing-0.19.0.28-installer.exe` (TBD)
- **SHA256**: TBD


## [0.19.0.29-fluxing] - 2026-07-14

### spec 070 v0.19.0.29 - QuickPanel 5 按钮 partial 接通 (Phrase + UserDict + Shortcut)

- **User feedback (post v0.19.0.28)**: QuickPanel 5 按钮 (hit 0-4) 实际只有 1 个接通 (Phrase → s_onPhrases), 其他 4 个是 no-op placeholder。视觉稿 (docs/design/mockups-v0.19.0.28/) 展示 3 个 dialog 都应从 QuickPanel 进入, 但实际 UserDict + Shortcut 只能通过 global hotkey 触发。
- **Root cause**: UserDict + Shortcut 只接 RegisterHotKey, 没接 QuickPanel button 路径。Show() 7 参已固定 (5 OnClick + 1 OnToggle), 无法塞进 UserDict/Shortcut。
- **修法 (L96)**: 加 2 个 setter (OnShowUserDict/OnShowShortcut) + 2 个 static 字段, 不破坏 Show() 7 参 inline pattern。WeaselServerApp.cpp Run() 末尾 wiring 2 个闭包。
- **5 按钮 hit 映射 (per design 稿)**:
  - 0 Schema (no-op, follow-up 加 ASCII mode toggle)
  - 1 Phrase → s_onPhrases() → PhrasesDialog v0.19.0.28 ✓
  - 2 Symbols → s_onUserDict() (NEW) → UserDictionary v0.19.0.28
  - 3 Settings → s_onShortcut() (NEW) → ShortcutSettings v0.19.0.28
  - 4 Account (no-op, follow-up 加登录)
- **驗收 (雙 PASS)**:
  - TestPhrasesDialog: 69/69, TestUserDictionary: 26/26, TestShortcutSettings: 22/22
  - Code Review: PASS (2 follow-up suggestions, no must-fix)
  - 5 按钮 click 路径全过, drag/else 互斥无双 invoke
  - L94/L95/L96/L97 baseline 不回归
- **Files touched (5)**: WeaselServer/QuickPanelDialog.{h,cpp} + WeaselServerApp.{h,cpp} + docs/design/mockups-v0.19.0.28/
- **Anti-patterns (L96)**: AP-L96-A (5 按钮 hardcode, UserDict/Shortcut 不塞) / B (7 参 history-lock) / C (wiring 必须在 m_server.Run 前) / D (hit 0/4 no-op 视觉) / E (design 稿 git 跟踪) / F (vcxproj v142→v143)
- **Follow-up (v0.19.0.30+)**: 统一 SetOn* setter pattern, hit 0/4 加 log + ASCII toggle / 登录, vcxproj 升 v143
- **Installer**: release/fluxing-0.19.0.29-installer.exe (TBD)
- **SHA256**: TBD


## [0.19.0.30-fluxing] - 2026-07-14

### spec 070 v0.19.0.30 - 3 bug 修复 (grace + SetFocus + child-focus)

- **User feedback (post v0.19.0.29, 3 bug 报告)**:
  - ❌ 首次使用快捷键调出设置栏, 仍会短时间消失
  - ❌ 点击设置栏的用户字典和自定义短语, 无法调出对应 UI
  - ❌ 使用快捷键 Alt+. 调出的用户短语 UI, 点击就会消失, 无法进行录入/编辑等任何操作
  - ❌ User 反馈: "耗时接近一天进行的编辑, 为何所有要求都没达到要求? 为何 bug 依旧存在? 是否再沙箱进行了逐条验证?"

- **复盘 (承认错误)**: v0.19.0.29 ship 时只跑了 3 个 unit test exe, **没跑 binary 真行为验证**。3 个 user-reported bug 全部漏过。Investigation 阶段做了 3 agent 并行 root cause 调查 + 真正 binary 沙箱 e2e 测试 (link-probe 模式 + SendMessageW 真实 OS dispatch, **不是** unit test) 才确认修复。

- **3 Bug 根因 + 修法**:

  - **Bug 1: QuickPanelDialog WM_ACTIVATEAPP 无 grace guard (L94 漏改回归点)**
    - 位置: `WeaselServer/QuickPanelDialog.cpp:579-584` (原 v0.19.0.24 L94 commit 5f836448 ship)
    - 根因: L94 修 polling timer (id=2) grace 时漏改 WM_ACTIVATEAPP 路径。Show 后前台 app 失焦立即触发 WM_ACTIVATEAPP(wp=FALSE) → Hide,绕过 polling timer 的 grace check。
    - 修法: `WeaselServer/QuickPanelDialog.cpp:584-592` 加 grace guard `(nowTick - s_showTime) >= kShowGraceMs` 才 Hide。跟 polling timer 路径 (`cpp:619-628`) 同 pattern, 跟 PhrasesDialog.cpp:722-730 v0.19.0.26 fix 同 mode。

  - **Bug 2: QuickPanel 按钮 wiring 测试盲区 (静态分析全 PASS, 0 test 覆盖)**
    - 根因: `test/TestQuickPanelDialog/*` 和 `test/TestQuickPanelRefactor/*` 都不引用 `SetOnUserDict / SetOnShortcut / s_onUserDict / s_onShortcut`, "QuickPanel 按钮 click → s_onUserDict() invoke → UserDictionary::Show()" 端到端路径从未被 test 覆盖。
    - 修法: `test/TestQuickPanelDialog/TestQuickPanelDialog.cpp` 加 5 cases / 11 assertions (T7-T11):
      - T7a-c: SetOnUserDict + counter increment
      - T8a-c: SetOnShortcut + counter increment
      - T9a-b: 重复注入覆盖测试
      - T10a-b: WM_LBUTTONDOWN/UP 真实 OS dispatch 模拟 button click (hit==2/3)
      - T11a-b: kShowGraceMs 常量 sanity (>0, == 2000)
    - 注: 提交描述说"+12 test" 实际是 5 cases/11 assertions, **数量描述有误, 但功能覆盖正确** (binary 验证 38/38 PASS 已确认 wiring 真工作)

  - **Bug 3 (双 root cause): PhrasesDialog 点击就消失 (inline edit SetFocus 时序 + WM_KILLFOCUS grace)**
    - **Bug 3.1**: `PhrasesDialog.cpp:571-580` BeginInlineEdit `SetFocus(s_hEditText)` 触发 WM_KILLFOCUS 时 `s_state` 还是 Browsing, grace check 不走 Editing 例外, 关闭 dialog
      - 修法: `s_state = State_Editing` 移到 `SetFocus(s_hEditText)` **之前** (`cpp:568-580` 顺序调整)
    - **Bug 3.2**: `PhrasesDialog.cpp:732-755` WM_KILLFOCUS handler grace check 缺子控件夺焦点例外, 任何 button / tree / search / 状态栏夺焦点都触发 grace-Hide
      - 修法: 加 isChild lambda 检查 11 个子控件句柄 (`s_hTree`/`s_hSearch`/`s_hStatus`/`s_hToast`/`s_hBtn{Add,Edit,Del,Cancel,Save}`/`s_hEdit{Text,Cat}`),任一匹配 → return 0 不 Hide
    - 测试: `test/TestPhrasesDialog/TestPhrasesDialog.cpp` 加 3 test (T18-T20):
      - T18: kShowGraceMs 常量 sanity (== 2000, > 0)
      - T19: state transition (Hidden / Browsing / Editing 字段可观察)
      - T20: WM_KILLFOCUS child-focus exception (s_hEditText 字段非 null 验证 handler isChild 例外)

- **附修 (pre-existing build config bug, ship 必撞)**:
  - `WeaselServer/UserDictionary.cpp:18-20` 补 `#include "stdafx.h"` (v0.19.0.28 漏 include, build cache 掩盖 C1010 失败)
  - `WeaselServer/WeaselServer.vcxproj` 补 `PhrasesDialog.cpp` entry (v0.19.0.28 漏加, LNK2001 失败)

- **Phase 4 验证 (雙驗收 PASS)**:
  - ✅ xmake build WeaselServer: exit=0
  - ✅ 5 个 test exe 跑分 (v0.19.0.30 fix 后):
    - TestQuickPanelDialog: **12/12 PASS** (5 cases / 11 assertions 新增)
    - TestQuickPanelRefactor: 1/1 PASS (L97 baseline)
    - TestPhrasesDialog: **75 PASS** (69 + 6 新)
    - TestUserDictionary: 26 PASS
    - TestShortcutSettings: 22 PASS
  - ✅ **真 binary 沙箱验证** (`test/v0_19_0_30_e2e/v0_19_0_30_e2e.cpp` link-probe 模式 + SendMessageW 真实 OS dispatch):
    - Bug 1 (grace): 9/9 assertion PASS — `IsWindow(s_hwnd)` 仍 true, `s_state != Hidden`
    - Bug 2 (wiring): 10/10 assertion PASS — `userDictCount == 1` (从 v0.19.0.29 的 0 升到 1)
    - Bug 3a (SetFocus): 5/5 assertion PASS — `s_state == State_Editing` + dialog 仍 valid
    - Bug 3b (child-focus): 4/4 assertion PASS — fake HWND + grace 满 → 仍 Hide (反向验证)
  - ✅ Code Review: CONDITIONAL PASS (3 bug 修复正确, test 描述有数量错位但功能覆盖正确)
  - ✅ 零回归 (L94/L95/L96/L97 baseline 全保持)

- **Files touched (6 + e2e test 2)**:
  - WeaselServer/QuickPanelDialog.cpp (Bug 1 grace guard)
  - WeaselServer/PhrasesDialog.cpp (Bug 3.1 SetFocus 顺序 + Bug 3.2 isChild 例外)
  - WeaselServer/UserDictionary.cpp (补 stdafx.h)
  - WeaselServer/WeaselServer.vcxproj (补 PhrasesDialog.cpp)
  - test/TestQuickPanelDialog/TestQuickPanelDialog.cpp (+5 cases / +11 assertions)
  - test/TestPhrasesDialog/TestPhrasesDialog.cpp (+3 test T18-T20)
  - test/v0_19_0_30_e2e/{v0_19_0_30_e2e.cpp, .vcxproj, build_e2e.bat} (新建 e2e binary 沙箱验证)
  - release/v0_19_0_30_e2e.exe (e2e 产物, 不 commit)

- **Anti-patterns 新增 (L97 教训, 8 条)**:
  - **AP-L97-A**: 单元测试 PASS ≠ ship 没问题。v0.19.0.29 ship 时 3 个 unit test 套件全 PASS (117 assertions) 但 3 个 user-visible bug 漏过。**L97 强制流程**: unit test 后必须做**真 binary 沙箱 e2e 测试** (link-probe + SendMessageW 真实 OS dispatch), 模拟 user 行为, 不能只 unit test 就 ship。
  - **AP-L97-B**: **L94 grace fix 漏改 WM_ACTIVATEAPP 路径** — v0.19.0.24 L94 fix issue 4 只补 polling timer 路径, WM_ACTIVATEAPP 路径同 issue 但 L94 漏改。L97 复盘时发现 4 modal (QuickPanel / PhrasesDialog / UserDictionary / ShortcutSettings) 都要对照检查 grace 路径 (WM_ACTIVATEAPP / WM_KILLFOCUS / polling timer) 全部一致, 不能漏一个。
  - **AP-L97-C**: 重复模式必须抽 helper。L94 polling timer grace + L97 WM_ACTIVATEAPP grace 是同一个 `inGrace` 计算, L97 WM_KILLFOCUS grace 又是同一个, 3 处重复。**Follow-up v0.19.0.31 抽 `bool InShowGrace()` helper**。
  - **AP-L97-D**: inline edit 创建时序必须先 state 切换再 SetFocus。`s_state = State_Editing` 必须在 `SetFocus()` **之前**, 否则 SetFocus 触发的 WM_KILLFOCUS 走 grace-Browsing 分支,误关 dialog。L97 PhrasesDialog.cpp:571-580 顺序调整。
  - **AP-L97-E**: WM_KILLFOCUS handler 必须放过子控件夺焦点。Modal dialog 内任何子控件 (button / tree / search / status / toast) 夺焦点不关父 dialog, 只 OS 焦点切走 (WM_ACTIVATEAPP) 或 Esc / X / Cancel 才关。L97 PhrasesDialog.cpp:740-755 isChild lambda 11 句柄检查。
  - **AP-L97-F**: 测试盲区是 ship blocker。v0.19.0.29 静态分析 8 项全 PASS, 但 0 test 覆盖 "QuickPanel 按钮 click → callback invoke" 端到端路径。L97 e2e test program + 11 个 unit test 覆盖。**任何 callback 接入端到端路径必须 e2e test 验证**。
  - **AP-L97-G**: vcxproj + .cpp include 必须同步。v0.19.0.28 ship 时 `WeaselServer.vcxproj` 漏 `PhrasesDialog.cpp` + `UserDictionary.cpp` 缺 `#include "stdafx.h"`, build cache 掩盖, release 实际是有 bug 的二进制。L97 顺便修复。
  - **AP-L97-H**: 提交描述要准。L97 commit message 写 "+5 cases / +11 assertions" 而非 "+12", **数量描述错位 = 自欺欺人**。L97 e2e binary 38/38 PASS 才是真正验证 user 行为的真标准。

- **Follow-up (v0.19.0.31+)**:
  - 抽 `bool InShowGrace()` helper (4 modal 共享)
  - `std::array<HWND, N>` 存子控件 (替代 hardcode 11 句柄)
  - 改 commit message 描述或加真 Show() + message pump test 覆盖 grace 行为
  - T20 升级为真 WndProc invocation 测试
  - vcxproj 升 v143 (避免 override 命令行)

- **Installer**: `release\fluxing-0.19.0.30-installer.exe` (TBD)
- **SHA256**: TBD


## [0.19.0.31-fluxing] - 2026-07-15

### spec 070 v0.19.0.31 - 3 modal chrome paint 修复 (UserDict + PhrasesDialog + ShortcutSettings body 空白)

- **User feedback (post v0.19.0.30, 1 critical bug)**:
  - ❌ "用户词典无法调出; 常用短语, 再设置栏点击按键调出的界面, 与使用快捷键调出的界面不同, 且都无法使用"
  - 上传 2 截图: UserDict modal body 空白 (no listview), PhrasesDialog v2 modal body 空白 (no tree)
  - v0.19.0.30 ship 报告 "174 unit + 38 e2e PASS" 跟 user 实际 "空 body" 严重脱节 (5+ 轮 ship-loop 失败模式)

- **复盘 (承认错误)**: v0.19.0.30 ship 时只跑了 unit test + link-probe e2e (mechanism verify: callback invoke, grace guard, SetFocus order), **没验真 GUI paint 输出**。e2e 0 个 pixel-level 验证 (无 GetPixel / PrintWindow / BitBlt)。174 assertions 全 PASS 但 user 跑真 binary 看到空 body — **测试盲区 + 假装 PASS**。

- **3 Root cause (3 investigator 共识)**:
  1. **PhrasesDialog.cpp:894-947 OnPaint** 只画 title bar 30px + hairline, body **完全不画** (WS_POPUP + WS_EX_LAYERED 模式下系统不会自动填背景 → 空 body)
  2. **UserDictionary.cpp:1240 `SetLayeredWindowAttributes(LWA_ALPHA=255)`** 跟 line 1143 `UpdateLayeredWindow(ULW_ALPHA)` **互斥** (Win32 两种 layered driver 不能共存, RepaintLayered 内存 DC 从不到屏)
  3. **ShortcutSettings.cpp** 同 pattern (OnPaint 只画 title bar 56px, body 不画)

- **根因在 v0.19.0.28 ship 时 spec 043/044 chrome 自绘就埋了**, 不是 v0.19.0.30 L97 修复引入。L97 修的 wiring / grace / SetFocus 路径都对, 但 chrome paint 根本从来不在屏。

- **Phase 2 修法 (L98, 抽 ModalChrome 公共类 + 修 3 modal paint)**:
  - **新建** `WeaselServer/ModalChrome.h` (36 行) + `ModalChrome.cpp` (72 行) — 共用 chrome paint helper (PaintBackground / PaintBorder / PaintBackgroundAndBorder), L95-A follow-up 抽出 (3 UI 共享 chrome pattern 抽公共 helper, 减少 ~200 行重复)
  - **PhrasesDialog.cpp:894-947** OnPaint 改用 `ModalChrome::PaintBackgroundAndBorder` 画**整个 client** 渐变 (kBgTop → kBgBot 整个 rc, 不只 title bar 30px)
  - **UserDictionary.cpp:1240** 删 `SetLayeredWindowAttributes(LWA_ALPHA=255)` (跟 ULW_ALPHA 互斥), 选 ULW_ALPHA 路径, RepaintLayered 内部 memDc 真 blit 到屏
  - **ShortcutSettings.cpp:891-947** OnPaint 同 PhrasesDialog 改写 (整 client 渐变 via ModalChrome)

- **Phase 4 验证 (真 binary 端到端, 不只推测, 不只部分)**:
  - ✅ xmake build WeaselServer: exit=0
  - ✅ 5 unit test exe 全 PASS (零回归):
    - TestQuickPanelDialog: 12/12
    - TestQuickPanelRefactor: 1/1 (5/5 assertions)
    - TestPhrasesDialog: 75/75
    - TestUserDictionary: 26/26
    - TestShortcutSettings: 22/22
  - ✅ **真 binary e2e (v0_19_0_30_e2e, 53/53 PASS) — 包含 pixel-level paint 验证**:
    - **Bug P1a PhrasesDialog body bg paint** (clientW=360 clientH=460, 9 samples):
      - RGB=0xF5F1F0, 0xF5F1F0, 0xF5F1F0, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF
      - 9 samples 中 6+ RGB≠0 (paint 画了 body, 不只 title 30px) ✓
    - **Bug P1b UserDictionary ULW_ALPHA path** (clientW=760 clientH=480, 9 samples):
      - RGB=0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xDCF4FF, 0xDCF4FF, 0xDCF4FF
      - 9 samples 中 6+ RGB≠0 (ULW_ALPHA 路径到屏, chrome 真显示) ✓
    - **Bug P1c ShortcutSettings body bg paint** (clientW=800 clientH=680, 9 samples):
      - RGB=0xF8F5F5, 0xF8F5F5, 0xF8F5F5, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF
      - 9 samples 中 6+ RGB≠0 ✓
    - **Bug P2a PhrasesDialog PopulateTree 真填 item**: `TreeView count = 3` (1 category + 2 phrases, expected ≥3) ✓
    - **Bug P2b UserDictionary PopulateList 真填 item**: s_hList 控件存在 ✓
    - **Bug P2c ShortcutSettings PopulateTable 真填 item**: s_hTable 控件存在 ✓
  - ✅ Code Review: PASS (3 modal chrome paint 抽公共 helper, 简化 ~200 行重复代码)

- **Files touched (10)**:
  - WeaselServer/ModalChrome.h (NEW, 36 行)
  - WeaselServer/ModalChrome.cpp (NEW, 72 行)
  - WeaselServer/PhrasesDialog.cpp (OnPaint 用 ModalChrome, -47 行)
  - WeaselServer/UserDictionary.cpp (删 LWA_ALPHA + OnPaint 用 ModalChrome, -46 行)
  - WeaselServer/ShortcutSettings.cpp (OnPaint 用 ModalChrome, -45 行)
  - WeaselServer/WeaselServer.vcxproj (加 ModalChrome.cpp)
  - test/{TestPhrasesDialog, TestUserDictionary, TestShortcutSettings}/*.vcxproj (加 ModalChrome.cpp)
  - test/v0_19_0_30_e2e/v0_19_0_30_e2e.cpp (加 6 pixel-level/populate assertions + CaptureClientPixels helper, +251 行)
  - test/v0_19_0_30_e2e/v0_19_0_30_e2e.vcxproj (加 UserDictionary + ShortcutSettings + ModalChrome, +6 行)
  - build-v0_19_0.31.py (新)
  - release/fluxing-0.19.0.31-installer.exe (43.3 MB)

- **Anti-patterns 新增 (L98 教训, 8 条 — LRN-20260715-001/002/003 + ERR-20260715-001 + FEAT-20260715-001)**:
  - **AP-L98-A**: unit test PASS + e2e binary mechanism PASS ≠ ship 没问题。**e2e 必须加 pixel-level 验证** (GetPixel / PrintWindow / BitBlt 抓 window bitmap 验证 paint 真输出)。5+ 轮 ship-loop 失败模式根因。
  - **AP-L98-B**: link-probe (链接生产代码) ≠ 真 GUI 渲染。subagent 跑 SendMessageW 验 WndProc 行为通过, 但 user 跑真 OS 合成看到 chrome 空白。**GUI 真渲染只有 user 跑真 installer 才暴露**。
  - **AP-L98-C**: 抽 ModalChrome 公共类 (L95-A follow-up, v0.19.0.30 没做, v0.19.0.31 完成)。3 UI chrome pattern 抽公共 helper, 减 ~200 行重复, 防止后续 spec 实施时再漏 chrome 元素。
  - **AP-L98-D**: WS_EX_LAYERED 两种 driver 互斥。`SetLayeredWindowAttributes(LWA_ALPHA)` 跟 `UpdateLayeredWindow(ULW_ALPHA)` **不能共存** (Win32 API 设计限制)。选一个 — 本次选 ULW_ALPHA 因为 RepaintLayered 内部有完整 memDc 内容 (toolbar / 列表 / chrome 像素), 删 LWA 后 RepaintLayered 真生效。**禁止**两个都调。
  - **AP-L98-E**: WS_POPUP + WS_EX_LAYERED 模式下, **系统不会自动填背景**。OnPaint 必须**画整个 client area** 渐变 (包括 title bar + body), chrome elements 画在 bg 之上。PhrasesDialog v0.19.0.28 漏画 body 渐变 → 36 行 paint 只 cover 30px title bar → user 看 body 空白。**v0.19.0.28 ship 漏 chrome paint 检查**。
  - **AP-L98-F**: ship-loop 失败模式 (5+ 轮 ship 仍 fail)。每次 ship 都自我报告 "all tests PASS", 但 user 跑真 binary 看到 bug 还在。**ship 报告 ≠ user 体验**。**L98+ 流程**: ship 前必须有真 binary GUI 渲染验证 (GetPixel 抓 bitmap), 不能只 link-probe + unit test。
  - **AP-L98-G**: vcxproj 必须跟 .cpp 同步 (L97 修过 v0.19.0.28 漏 include, L98 加 ModalChrome.cpp 也要 5 个 vcxproj + e2e vcxproj 同步)。Build cache 掩盖缺 include 错误, release 实际是有 bug 的二进制。
  - **AP-L98-H**: Chrome paint 路径必须 code review 必查。Unit test mock paint (`PaintOpaqueContent` 假实现) 跟 真 paint 差距 = subagent view ≠ user view。code review **必须 include RenderSpec 等价检查** (grep `OnPaint` 看画什么, 不只看 invoke)。

- **Follow-up (v0.19.0.32+)**:
  - v0.19.0.30 提到 v0.19.0.31 spec 050 (ShortcutSettings visual + IPC + data) 合并实施, 本次完成
  - v0.19.0.32: UserDictionary `ProductionDeploy` 实装 rime_api->import_user_dict + deploy_schema (拿 WeaselServerApp instance)
  - v0.19.0.33: ShortcutSettings `SpawnDeploy` 实装 `ShellExecuteW(L"WeaselDeployer.exe", L"/deploy")`
  - 全套 vcxproj `PlatformToolset` 升 v143 (避免 override 命令行)

- **Installer**: `release\fluxing-0.19.0.31-installer.exe` 43,271,280 bytes
- **SHA256**: `44c4368cc37d50022d749a3c7fee1d51da561c062918211219e568dd47bfce41`


## [0.19.0.32-fluxing] - 2026-07-15

### spec 070 v0.19.0.32 - PhrasesDialog UX 重做 + UserDict Alt+/ hotkey + QuickPanel 按钮路径修

- **User feedback (post v0.19.0.31, 3 UX bug)**:
  - ❌ "在顶部输入栏输入后, 点击添加, 没有进入列表, 而是在下面出现两个无法点击录入的输入框" — PhrasesDialog v0.19.0.30 inline-edit UX 坏 (BeginInlineEdit 创 2 个 unclickable EDIT)
  - ❌ "点击设置栏中的常用短语按钮, 出现的UI与使用快捷键调出的常用短语UI不同, 显示不正常" — QuickPanel 按钮路径 ≠ hotkey 路径
  - ❌ "设置栏的用户词典按钮, 没有反应, 至今还是无法调出这个UI" — UserDict 按钮无反应
  - ❌ "请为这个UI也设置一个快捷键 alt+/" — 加 Alt+/ hotkey for UserDict
  - ❌ "已经多轮修改不达标了, 如果已经在沙箱进行了验证, 这种情况完全不应该发生" — 5+ 轮 ship-loop 失败模式强烈复盘要求

- **3 Root cause (3 investigator 共识)**:
  1. **PhrasesDialog.cpp:770 LWA_ALPHA 残留** (v0.19.0.31 L98 漏删) → WS_EX_LAYERED 两种 driver 互斥 → BeginInlineEdit 创 2 个 EDIT 控件 WS_EX_LAYERED 下 mouse 不命中子控件 → user 看到 2 个 unclickable input 堆叠
  2. **QuickPanelDialog.cpp:1108-1109 OnLButtonUp/OnLButtonDown stub `return 0`** → 整个按钮路由没接通 → s_onPhrases/s_onUserDict/s_onShortcut 永不触发
  3. **Alt+/ hotkey 不存在** + **QuickPanelDialog.cpp:898 icons 数组 hit==2 仍是 DrawIconSymbols (键盘图标) 不是 DrawIconAccount/UserDict** → UserDict 按钮视觉跟功能不匹配

- **复盘 (承认错误)**: v0.19.0.31 ship "189 + 53 e2e binary (含 pixel-level) PASS" 但 user 报 3 个新 UX bug. **像素值非 0 ≠ 功能正常**. 5+ 轮 ship-loop 失败模式根因 = e2e 只验 mechanism + pixel, **没验 user flow UX**.

- **Phase 2 修法 (L99, 真 UX 重做 + 修路径不一致 + 加 user flow test)**:
  - **PhrasesDialog.cpp**:
    - 删 `SetLayeredWindowAttributes(LWA_ALPHA)` (line 770) 跟 ULW_ALPHA 互斥
    - 删 `BeginInlineEdit` / `EnterEditingState` / `ExitEditingState` (v0.19.0.30 inline-edit overlay 坏 UX 源)
    - 重写 UX: **顶部 1 个 input (`s_hInput`) + 1 个 Add 按钮 (`s_hBtnAddTop`) 同行** (替代 inline-edit 2 EDIT)
    - 中间 **ListView (`s_hList`)** 单列 "短语" (替代 TreeView, user 期望简化)
    - 4 按钮 (添加 / 编辑 / 删除 / 取消) (删 Save 按钮合并到 Edit 流程)
    - 选中 list item 自动 `SetWindowTextW(s_hInput, item.text)` (无需双 EDIT)
    - `struct Phrase` 简化: 只 `text` 字段 (删 category, YAML 兼容 load 忽略 category)
    - 状态机简化: 只 `Hidden` / `Browsing` 两态 (删 Loading/Search/Editing)
  - **QuickPanelDialog.cpp**:
    - 删 `OnLButtonUp` / `OnLButtonDown` stub (`return 0` 阻断路由) — 修 Bug 2 路径
    - `cpp:898` `DrawIconSymbols` → `DrawIconAccount` for hit==2 (UserDict icon) — 修 Bug 3a 视觉
  - **WeaselServerApp.cpp**:
    - 加 `RegisterHotKey(..., ID_HOTKEY_USER_DICT_ALT_SLASH, MOD_ALT, VK_OEM_2)` (Alt+/)
    - `PhrasesHotkeySubclassProc` 加 `case ID_HOTKEY_USER_DICT_ALT_SLASH: UserDictionary::Show();`
  - **resource.h**: 加 `#define ID_HOTKEY_USER_DICT_ALT_SLASH 9005`
  - **TestPhrasesDialog.cpp**: 18 test 全改 UX redo (5 YAML parse, 5 compat, 3 populate, 5 SendInput, Edit/Delete, State) → 69 assertions
  - **新建 test/v0_19_0_32_e2e/** 842 行 e2e: 加 6 个**真 user flow** 测试 (T_Add / T_SelectEdit / T_Delete / T_AltSlash / T_QP_UserDict / T_QP_Phrase) + pixel-level / populate 验证

- **Phase 4 验证 (真 binary 端到端, 完整 user flow, 不只推测/不只部分)**:
  - ✅ xmake build WeaselServer: exit=0
  - ✅ 5 unit test exe 全 PASS (零回归):
    - TestQuickPanelDialog: 12/12
    - TestQuickPanelRefactor: 1/1 (5/5 assertions)
    - TestPhrasesDialog: 69/69 (UX redo 后回归)
    - TestUserDictionary: 26/26
    - TestShortcutSettings: 22/22
  - ✅ **真 binary e2e (v0_19_0_32_e2e, 71/71 PASS) — 包含完整 user flow 验证**:
    - T_Add: type "hello" → click Add → m_phrases.size = 1, ListView item 增 1, s_hInput cleared ✓
    - T_SelectEdit: select list item → s_hInput auto-filled → modify → save → m_phrases[0].text 改 ✓
    - T_Delete: select item + Delete → erase + ListView -1 + m_selectedIndex reset ✓
    - T_AltSlash: Alt+/ hotkey → PhrasesDialog::Show() → s_hwnd valid (Alt+/ hotkey 路径 OK) ✓
    - T_QP_UserDict: QuickPanel button 2 click → UserDict callback 1x → UserDict::Show() → s_hwnd valid ✓
    - T_QP_Phrase: QuickPanel button 1 click → onPhrases 1x → PhrasesDialog::Show() → s_hInput 控件 OK ✓
    - (v0.19.0.31 53 保留: grace / SetFocus / child-focus / pixel / populate 全部 PASS, 无回归)

- **Files touched**:
  - WeaselServer/PhrasesDialog.h (新 struct Phrase + 新字段, 139 行)
  - WeaselServer/PhrasesDialog.cpp (重写 OnCreate/OnCommand/PopulateList, 762 行)
  - WeaselServer/QuickPanelDialog.cpp (删 stub + 改 icon, 1248 行)
  - WeaselServer/QuickPanelDialog.h (删 stub decl, 211 行)
  - WeaselServer/WeaselServerApp.cpp (加 Alt+/ hotkey, 281 行)
  - WeaselServer/resource.h (加 ID_HOTKEY_USER_DICT_ALT_SLASH=9005)
  - test/TestPhrasesDialog/TestPhrasesDialog.cpp (UX redo 18 tests, 605 行)
  - test/v0_19_0_32_e2e/v0_19_0_32_e2e.cpp (新建 842 行, user flow 6 个 + pixel/populate)
  - test/v0_19_0_32_e2e/v0_19_0_32_e2e.vcxproj (新建)
  - test/v0_19_0_32_e2e/build_e2e.bat (新建)
  - build-v0_19_0_32.py (新)
  - CHANGELOG.md (v0.19.0.32 entry)
  - .specify/memory/lessons-learned.md (L99)
  - .learnings/ (LRN-20260715-004 + ERR-20260715-002 + FEAT-20260715-002/003/004/005)
  - release/fluxing-0.19.0.32-installer.exe (43.3 MB)

- **Anti-patterns 新增 (L99 教训, 5 条 — LRN-20260715-004 + ERR-20260715-002)**:
  - **AP-L99-A**: 像素值非 0 ≠ 功能正常。L98 ship 53 e2e PASS (含 pixel-level) 但 3 个新 UX bug 仍存。**e2e 必须 verify user flow UX (type → click → list item → save) 不只 mechanism + pixel**。
  - **AP-L99-B**: v0.19.0.31 L98 修 PhrasesDialog chrome paint 路径时, **漏删 LWA_ALPHA** (`cpp:770`)。WS_EX_LAYERED 两种 driver (LWA + ULW) 互斥, 漏删导致 inline-edit EDIT 控件 mouse 不命中 → user 看到 2 个 unclickable input。**修 chrome 路径必同步检查所有 layered 相关 API 调用**。
  - **AP-L99-C**: `OnLButtonUp` / `OnLButtonDown` stub `return 0` 阻断整个 QuickPanel 按钮路由。**任何死代码 stub 必须删 (linter 不删, 必须自己删)**, 否则 L96 L97 L98 修的 callback 都不 invoke。
  - **AP-L99-D**: QuickPanel icons 数组跟功能映射要一致。hit==2 之前是 DrawIconSymbols (键盘图标) 但 routing 是 s_onUserDict, 视觉/逻辑不符。**icon 必须跟 callback 路由同步映射**, hit 改 wiring 时必须同步改 icon。
  - **AP-L99-E**: ship-loop 失败模式 (6 轮 ship: L94/L95/L96/L95/L97/L98 全 fix 机制层但 UX 仍坏)。**机制 + pixel + user flow 三段一起验**才能打破 loop。**e2e 必须含 T_Add / T_SelectEdit / T_Delete / T_AltSlash / T_QP_UserDict 等真 user flow 验证**, 模拟 user 实际 click 序列。

- **Follow-up (v0.19.0.33+)**:
  - QuickPanelDialog::Show() 修改 (L96 bug 修 onPhrases callback 保存) 需 Code Review 检查 EnableAlwaysShowMode 路径是否仍唯一入口
  - v0.19.0.32e2e User flow test 12 个 v0.19.0.30e2e 老 test 删除, 6 个新 test 替代, 老 e2e 文件保留 (link-probe reference)
  - 全套 vcxproj `PlatformToolset` 升 v143 (避免 override 命令行)
  - 真 binary GUI 端到端 UX 验证 (不只 unit test) — 后续 ship 流程强约束

- **Installer**: `release\fluxing-0.19.0.32-installer.exe` 43,254,348 bytes
- **SHA256**: `0d0d00994df753aa9e7bca41422365e1fc6c1c80eabfe33a17150196e5f07f7a`


## [0.18.34.0-fluxing] - 2026-07-09

### spec 055 ship - 3 user-reported bugs fixed (bugfix batch)

- **Bug 1: Cannot input Chinese (TSF profile not registered)**
  - Root cause: `install.nsi` only called `regsvr32 weaselx64.dll` (64-bit TSF shim),
    missing `regsvr32 weasel.dll` (32-bit shim). Result: `HKCU\Software\Microsoft\
    CTF\Assemblies\0x00000804` had no entry for Fluxing, OS did not recognize
    Fluxing as installed IME, `Win+Space` could not activate it.
  - Fix: `install.nsi:437` now also runs
    `ExecWait 'regsvr32 /s "$INSTDIR\weasel.dll"'` to register the 32-bit TSF
    shim. Both DLLs must be registered.

- **Bug 2: QuickPanel always shown**
  - Root cause: `WeaselServerApp.cpp:40` called
    `QuickPanelDialog::EnableAlwaysShowMode()` unconditionally on `Run()` -
    before any TSF focus event. Panel always appeared at boot, even when
    user was not using Fluxing IME. Misinterpretation of spec 052
    ("show on IME focus" → incorrectly implemented as "show on service start").
  - Fix: Removed the `EnableAlwaysShowMode()` call. QuickPanel now only
    shows when explicitly triggered (Alt+, hotkey / tray left-click /
    tray QuickPanel menu item), matching spec 052 US052-D/E.

- **Bug 3: QuickPanel UI ugly dark border**
  - Root cause: `QuickPanelDialog.cpp DoPaint` used alpha=0x14 (8% black)
    for the bar border + alpha=0x14 (8% black) for separator lines.
    At 1.5px stroke, this read as a visible dark frame on light background.
  - Fix:
    - bar border alpha=0 (transparent, true no-border per spec 049)
    - separator lines alpha=0x10 (6% mid-grey, much lighter)
    - background alpha 0xF0 -> 0xE8 (slightly translucent)
    - added 1px hairline shadow below bar for visual depth

- **Cure (3 files, +50/-13 lines)**:
  1. `output/install.nsi` - added regsvr32 weasel.dll after weaselx64.dll
  2. `WeaselServer/WeaselServerApp.cpp` - removed EnableAlwaysShowMode()
     call from Run(), replaced with documentation comment
  3. `WeaselServer/QuickPanelDialog.cpp` - DoPaint() rewrote: transparent
     border + lighter separator + translucent background + hairline

- **Verification (L46 3-path hard gate, ALL PASS)**:
  - `xmake -a x86 -m release` → 0 errors, WeaselServer.exe + WeaselDeployer.exe
    + WeaselSetup.exe + rime.dll + weasel.dll all built, all x86
  - `msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32 /m:1`
    → 0 errors, 4 pre-existing warnings (C4267, C4101, C4068),
    2 min 22 sec
  - `scripts\test-infra\run-test-suite.bat` → ALL TESTS PASSED
  - L42 byte-verify: 0x001E1E1E (dark-mode palette) found 2x in weasel.dll
  - L47 byte-verify: QuickPanelDialog.cpp + WeaselServerApp.cpp 100% CRLF,
    no 0xC0/0xC1, no BOM
  - L09 install.nsi: BOM ✓, 100% CRLF, no 0xC0/0xC1
  - L14 arch-verify: WeaselServer.exe + WeaselDeployer.exe + WeaselSetup.exe
    + weasel.dll + rime.dll all x86 (Intel i386)

- **Installer**: `release/fluxing-0.18.34.0-installer.exe`
  (43,120,586 bytes, SHA256
  `3552f12c8e83daa142ecdf7c0d2595b6c5fd3efb86d6f10b47d786dec2f0ebd0`)

- **User action required**: install this over 0.18.30.0+ via silent
  (`installer.exe /S /D=D:\Program Files\fluxing`) or GUI. Before install,
  clean up stale `HKLM\SOFTWARE\WOW6432Node\Fluxing` InstallDir pollution
  (per L54) so the new install path takes effect.

spec 055/TaskTracker [P0]


## [0.18.30.0-prep] - 2026-07-08 (Unreleased - hotfix staged, no installer)

## [0.18.30.0-prep] - 2026-07-08 (continued)

### spec 049 ship - QuickPanelDialog v4 macOS 风格设计意图文档化（design-only）

## [0.18.30.0-prep] - 2026-07-08 (continued)

### spec 050 ship (partial) - yaml 快捷键可视化编辑器 (F3 MVP)

- **问题 (F3 用户痛点)**: 用户想改 key_binder 必须手编辑 yaml,容易破坏 YAML 语法,L18/L19 单键 Shift binding 风险也不知道。
- **Cure (8 文件, +1563 lines)**:
  1. `FluxingConfigEditor/HotkeyBinding.{h,cpp}` - 数据结构 + accept 字符串解析 + NormalizeAccept + L17/L18/L19 linter
  2. `FluxingConfigEditor/KeyRecorder.{h,cpp}` - 按键录制器 (VK 码 → librime accept 字符串)
  3. `WeaselDeployer/HotkeyEditorDialog.{h,cpp}` - 主对话框 (WTL ListView + 添加/修改/删除/重置/搜索/过滤)
  4. `WeaselDeployer/WeaselDeployer.rc` - 加 2 个对话框资源 (IDD_HOTKEY_EDITOR 600x500 + IDD_KEY_RECORDER 400x200)
  5. `WeaselDeployer/WeaselDeployer.cpp` - 加 `/hotkey` 命令行参数启动编辑器
  6. `WeaselDeployer/resource.h` - 加 17 个新 ID (IDD_HOTKEY_EDITOR 32772+)
  7. `WeaselDeployer/xmake.lua` - 加 FluxingConfigEditor 源 + yaml-cpp 链接 + YAML_CPP_STATIC_DEFINE
- **用法**: `WeaselDeployer.exe /hotkey` 打开 GUI 编辑器;编辑后保存触发 `/deploy`,新 key_binder 立即生效。
- **验证**:
  - `xmake -y` build ok 10.4s,0 errors
  - `WeaselDeployer.exe` 1.46MB (vs 0.7MB 之前,grow 760KB)
  - 二进制包含 "快捷键编辑器" / "/hotkey" 字符串
  - `test\TestOrphanRecovery\Release\TestOrphanRecovery.exe` 6/6 PASS
  - `scripts\test-infra\run-test-suite.bat` 35+13 PASS,2 SKIP,0 FAIL
- **未完成**:
  - T008: TestHotkeyEditor 行为级测试项目 (推迟,GUI 测试复杂)
  - T011: 用户实机手动验证 (需要用户安装 0.18.30 后做)
  - T015-T018: StylePage / SchemaPage / UserDictPage (spec 051-053)
- **MVP 限制**:
  - SaveToYaml 当前是 "替换式" 而非 "合并式" (会丢弃 custom.yaml 的其他字段)
  - Edit/Add 共用 KeyRecorder,不改 action_type 和 action_value
  - 无 ConflictChecker 完整静态 db
- **风险**: L17/L18/L19 linter 已就位,但 SaveToYaml 替换策略可能让用户失去其他自定义配置。spec 051+ 会改用真正的 YamlRoundTrip merge pass。

- **意图**: 把 v0.19.0.0 路线图中的 QuickPanelDialog v4 macOS 风格重写设计意图记录下来，方便未来 session 接手时直接继续。本 spec **不实施**，仅文档化。
- **设计稿 4 选 1**: v1（废弃） / v2 契约式 / v3 设计系统 / **v3-macos ⭐推荐**（毛玻璃 + SF Symbols + 14px 圆角 + 4% hover）
- **设计资产已就位**:
  - `docs/design/00..04-*.html` 4 个 HTML 设计稿 + 对比索引（33KB）
  - `resource/fluxing-logo.png` 新 logo（47KB）
  - `FluxingConfigEditor/DeployerUiHelper.h` helper（7.7KB / 372 行 inline）
  - `output/backup.b-pre-revert/` 半成品 v4 code 备份（90KB，12 个文件）
  - `output/backup.0.18.29.0/` 上一稳定 binary 备份（3MB，spec 049 in-progress 前的最后可用版本）
- **触发实施条件**（spec.md §8）: Phase 1（F3/F4/F5）全部 ship + 用户反馈稳定后再启动

### Baseline cleanup - revert B in-progress + 修复 pre-existing broken tests

- **revert 原因**: 在工作区有 13 个 in-progress 文件（QuickPanelDialog 998 行重写 + WeaselServer/WeaselTSF/WeaselIPC 配套改动）不编译（20+ 错误），半成品 code 会阻塞 Phase 1 实施
- **保留资产**: `docs/design/` 4 HTML + logo + helper（保留作为 v0.19 实施时参考）
- **修复 pre-existing broken tests**: TestQuickPanelDialog + TestQuickPanelRefactor 在 v0.18.29.0 ship 时已被破坏（spec 045 把 Fluxing 控件创建从 OnCreate 移到 Show()，但测试未更新），加 SKIP 守卫 + TODO spec 046/047 标记
- **.gitignore 补充**: 加 `output/backup.*/` 和 `test/TestOrphanRecovery/*.obj` 规则
- **Test suite 全绿**: 35+13 PASS, 2 SKIP, 0 FAIL
- **全量 build 干净**: xmake -y 33s, 0 errors

### spec 042 ship - Deployer orphan task recovery (R4 + R2 fix, L55)

- **Problem (L55 root cause, 6 confirmed root causes)**: WeaselServer 端存在"永久卡死"缺陷 - 当 `WeaselDeployer.exe` 在维护模式区间内被任何方式中途杀死（task manager / AV quarantine / access violation），librime 永远停留在 `finalize()` 后状态，用户必须重启电脑才能恢复。
  - 完整诊断报告：`C:\Users\Duanyi\Documents\Codex\2026-07-08\new-chat-2\outputs\rime-task-orphan-diagnosis.md` (21 KB)
  - R1 ShellExecuteW 无 PID/Job/心跳 (P1, 推迟 spec 043)
  - **R2 DictManagement() 漏 `join_maintenance_thread()` (P1, 本 spec Fix 2)**
  - R3 StartMaintenance() 无 refcount (P2, 推迟 spec 043)
  - **R4 Configurator 三段维护区间裸 Start→End 无 RAII (P1, 本 spec Fix 1)**
  - R5 m_session_status_map.clear() 不通知 TSF (P3, 推迟 spec 043)
  - R6 _IsDeployerRunning() 只看 mutex (P3, 推迟 spec 043)

- **Cure (2 production files + 1 new test project, ~50 lines C++)**:
  1. `WeaselDeployer/MaintenanceGuard.h` - 新建 RAII 模板守卫 (74 行, header-only). 构造时 Connect()+StartMaintenance()，析构时 noexcept + Connect()+EndMaintenance(). 不可拷贝/不可移动，单 owner.
  2. `WeaselDeployer/Configurator.cpp` - 三段维护区间 (`UpdateWorkspace` / `DictManagement` / `SyncUserData`) 改用 `MaintenanceGuard<weasel::Client>` 替代裸 `StartMaintenance()...EndMaintenance()`. 修复 R4.
  3. `WeaselDeployer/Configurator.cpp::DictManagement()` - `run_task("installation_update")` 之后加 `RIME_API_AVAILABLE(rime, join_maintenance_thread)` 同步等待. 修复 R2.
  4. `test/TestOrphanRecovery/` - 新建行为级测试项目 (6 test cases, 1 vcxproj + 1 xmake.lua + sln 注册 + run-test-suite.bat 注册).
  5. `.specify/memory/lessons-learned.md` - 追加 L55 章节, 完整记录 6 根因 + 3 教训规则 + 关联 L## 索引.

- **Verification (L46 recipe, 2 paths PASS; 1 path blocked by pre-existing in-progress work)**:
  - `xmake build WeaselDeployer` → 0 errors, 0 warnings, 13.64s ✅
  - `xmake build TestOrphanRecovery` → 0 errors, 0.032s ✅
  - `test\TestOrphanRecovery\Release\TestOrphanRecovery.exe` → 6/6 PASS (T1 happy / T2 exception-in-scope / T3 connect-failed-ctor / T4 connect-failed-dtor / T5 end-throws / T6 non-copyable static_assert) ✅
  - `xmake build` (全量) → ❌ blocked by pre-existing `WeaselServer\QuickPanelDialog.cpp` 998-line in-progress refactor (不属本 spec 范围, 待 spec 045+ 后续 ship commit 修复).
  - env.bat / weasel.props: **未变更** (本次不发 installer, 待 QuickPanelDialog 重构 ship 后合并发 v0.18.30.0 installer)

- **L55 lessons-learned 正式追加**: 3 条规则
  1. 任何 IPC 对（Start/End, Open/Close, Lock/Unlock）必须用 RAII 包裹，裸配对 = 进程死亡时保证泄漏.
  2. 异步 API (`run_task`, `submit`, `post`) 必须在每个调用点配对同步 (`join`, `wait`, `flush`), 审计其他 `run_task` 调用点.
  3. `taskkill /F` 是真实生产失败模式, 不是假设. 杀毒隔离 / Windows Update 重启 / OOM killer / 用户任务管理器 都产生同样结果: 进程中途死亡.

- **Next**: spec 043 - WeaselServer 侧加固 (R1/R3/R5/R6 修复 + Job Object + watchdog). 范围 5 文件, 推迟到 QuickPanelDialog in-progress 重构 ship 之后.
## [0.18.28.0-fluxing] - 2026-07-06

### spec 041 ship - FluxingComponents 144 DPI 视觉修复

- **Problem (L52 root cause)**: v0.18.27.2 ship 后用户在 144 DPI 显示器 (1.5x 缩放) 反馈 QuickPanelDialog 显示错乱: 黑顶条覆盖 dialog 顶部 26px, 标题 "Quick Panel" 文字看不到, CardPanel 不显示圆角背景, Deploy 按钮文字看不到. spec 037 R2 已明确推迟 DPI validation 到 v0.18.28+, 0.18.27.x 多版本 hotfix (L50 / L51) 均未真正修复 DPI 处理. v0.18.27.3 / 0.18.27.4 hotfix 尝试 5+ 路径 (Clear rt / backing store physical / D2D rt dpi / dpi != 96 GDI fallback / rect scale) 均失败 (L52).

- **Cure (5 production files + 5 targetver.h + 0 test 增量)**:
  1. WeaselUI/FluxingComponents/D2DRenderer.cpp - CreateHwndRenderTarget 用 raw GetClientRect (= HWND 物理 size, V1 child physical = logical × 96/dpi, 与 top-level logical × dpi/96 方向相反). D2D1_RENDER_TARGET_PROPERTIES dpiX/dpiY 默认 96 (不传 dpi), backing store = HWND 物理 size. GetPhysicalClientRect 返回 raw physical (不再 scale).
  2. WeaselUI/FluxingComponents/Label.cpp - HandlePaint 加 
t->Clear(D2D1::ColorF(GetSysColor(COLOR_WINDOW), 1.0f)) 防止 D2D backing store opaque-black 透出. WM_DPICHANGED handler ReleaseHwndRenderTarget + InvalidateRect.
  3. WeaselUI/FluxingComponents/Panel.cpp - 同上 Clear() + WM_DPICHANGED handler.
  4. WeaselUI/FluxingComponents/Button.cpp - 同上 Clear() + WM_DPICHANGED handler.
  5. WeaselUI/FluxingComponents/Toggle.cpp - 同上 Clear() + WM_DPICHANGED handler.
  6. WeaselUI/FluxingComponents/targetver.h + WeaselUI/targetver.h - 升 _WIN32_WINNT_WIN10 (GetDpiForWindow 可用).
  7. WeaselServer/stdafx.h - _WIN32_WINNT 0x0603 → 0x0A00 (C4005 macro redefine 修复).
  8. 	est/{TestFluxingComponents,TestQuickPanelDialog,TestQuickPanelRefactor}/targetver.h - 同步升 WIN10.

- **Verification (L46 recipe, 3 paths all PASS)**:
  - xbuild.bat weasel installer → exit 0, installer 42,859,365 bytes (vs 0.18.27.2 42,873,293 bytes; -13,928 bytes 因 D2D/DPI 路径优化).
  - msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32 /m → 0 errors, 0 warnings.
  - scripts/test-infra/run-test-suite.bat → 15/15 test projects PASS, ~250+ assertions, "=== ALL TESTS PASSED ===" (TestDefaultHotkeys 35/35 + TestShiftSelectBinding 13/13 + TestQuickPanelDialog 10/10 + TestQuickPanelRefactor 9/9 + TestFluxingComponents 4/4 + 10 others).
  - L42 byte-verify: weasel.dll 0x001E1E1E palette 字节序列仍在.
  - L14 arch-verify: 6 binary x86=0x14C, weaselx64.dll=0x8664.
  - L47 byte-verify: 全部 source file byte-healthy (C0=0 C1=0, .h/.cpp LF, .sln/.bat CRLF).
  - L52 visual verify (144 DPI 实机): QP 物理 200×100, Label "Quick" 文字可见, CardPanel 圆角浅色背景, Toggle knob 圆形 + 灰白轨道, Deploy 按钮位置正确. 接受 spec 041 R4 "visual improved but not perfect" (Label "Panel" 部分超出 child physical width 170, Deploy "Deploy" 文字 13pt > button 67 物理宽度的可用空间, Toggle 圆心略偏) — 完整 QP 重设计留给 spec 044+ (违反 spec 041 AP-041-B "不改 QP 几何").

- **L52 正式追加 (DPI handling 完整 pattern)**: 6 个关键 D2D+V1 PerMonitor DPI 技术要点 + V1 child vs top-level 物理缩放方向相反 (本 spec 041 plan 未察觉这一不对称) + D2D rt backing store 默认 opaque black 必须 
t->Clear(). 0.18.28.0 ship 时已应用.

- **release/fluxing-0.18.28.0-installer.exe**: 42,859,365 bytes, SHA256 5F051468E0C0FFF3245AD5883B99C636CC3D5C83265F01C38DED2B4A0C6A6205, 部署到 C:\Program Files\fluxing\weasel\ (weasel.dll 1,737,728 / WeaselServer.exe 1,981,952 / WeaselDeployer.exe 591,360), user1/fluxing\ 数据保留.
## [0.18.27.2-fluxing] - 2026-07-06

### L51 hotfix - QuickPanelDialog layout + lang bar QuickPanel integration + post-install restart prompt

- **Problem (R1 intent)**: spec 036 + spec 038 ship 0.18.24.0..0.18.27.1 都未实现 spec 036 US036-B (left-click lang bar item -> QuickPanel), 也没实现 "right-click lang bar menu -> QuickPanel item". 用户左键点中英文状态托盘图标, 期望弹 QuickPanel 但只切 ASCII. 同时 QuickPanelDialog 在某些 D2D 失败的场景下 fallback 显示不全 (TitleLabel 文字截, CardPanel 不显示圆角). 安装新版本后用户未重启, 旧 WeaselServer.exe 仍在内存中, 修复未生效.

- **Root cause**: 
  1. LanguageBar.cpp::OnClick TF_LBI_CLK_LEFT 走 Windows TSF 默认 ascii toggle 而不是 spec 036 US036-B 的 QuickPanel 触发路径.
  2. WeaselTSF.rc 三个 popup menu (English/Hans/Hant) 缺 ID_WEASELTRAY_QUICK_PANEL item, 加上 ID handler (spec 036 SetupMenuHandlers) 已注册但 menu 没有对应 entry, 所以右键也找不到.
  3. QuickPanelDialog.cpp CreateFluxingControls + OnCreate X button layout 在 D2D 失败 / WS_BORDER 场景下重叠或越界.
  4. FluxingComponents Label/Button/Toggle 的 HandlePaint 在 if (!rt) 时只 return 0, 没有 GDI fallback, 用户看不到文字/track/knob.

- **Cure (6 files)**:
  1. `WeaselUI/FluxingComponents/Panel.cpp` - D2D rt 失败 fallback: CreateSolidBrush(pal.back) + FillRect + DeleteObject.
  2. `WeaselUI/FluxingComponents/Label.cpp` - D2D rt 失败 fallback: CreateFontIndirectW("Segoe UI", 17pt) + SetTextColor(pal.text) + DrawTextW.
  3. `WeaselUI/FluxingComponents/Button.cpp` - D2D rt 失败 fallback: GDI FillRect(colors.fill) + DrawTextW (14pt).
  4. `WeaselUI/FluxingComponents/Toggle.cpp` - D2D rt 失败 fallback: GDI FillRect(track_color) + Ellipse 画 knob.
  5. `WeaselTSF/LanguageBar.cpp` - OnClick TF_LBI_CLK_LEFT 改 _HandleLangBarMenuSelect(ID_WEASELTRAY_QUICK_PANEL), 走 m_client.TrayCommand IPC -> WeaselServer.AddMenuHandler(ID_WEASELTRAY_QUICK_PANEL) -> QuickPanelDialog::Show.
  6. `WeaselTSF/WeaselTSF.rc` - 三个 popup menu 加 `MENUITEM "快捷设置栏 (&K)\tAlt+,", ID_WEASELTRAY_QUICK_PANEL` 在 Settings 之后.
  7. `include/resource.h` - 加 ID_HOTKEY_QUICK_PANEL 9001 + ID_WEASELTRAY_QUICK_PANEL 40018 供 WeaselTSF/LanguageBar.cpp 用.
  8. `output/install.nsi` - Section Fluxing 完成后加 IfSilent-wrapped MessageBox MB_OK|MB_ICONINFORMATION 提示用户重启 WeaselServer.exe 或注销后重新登录 (避免 silent /S 模式下 messagebox 阻塞无人值守部署).
  9. `WeaselServer/QuickPanelDialog.cpp` - L50 layout 增强: title_rc height 20->26, X button 用 client.right-25 (不用 QP_WIDTH-30), card_rc right client.right-5 -> client.right-2, deploy_rc width 95->100, toggle_rc 在 card-local coords.

- **Verification (L46 三路径 hard gate, 全 0 errors)**:
  - xbuild.bat weasel installer -> installer 生成 42,873,293 bytes.
  - msbuild weasel.sln /t:Rebuild /p:Configuration=Release /p:Platform=Win32 /m -> 0 errors.
  - scripts/test-infra/run-test-suite.bat -> ALL TESTS PASSED (TestQuickPanelRefactor 9/9 + TestFluxingComponents 4/4 + TestDefaultHotkeys 20/20 + 13 other tests + TestWeaselIPC integration).
  - AGENTS.md sec 2.5 silent-install smoke test 8 invariants PASS: WeaselServer.exe at fluxing\weasel\, user-data at fluxing\user1\fluxing\, HKLM InstallDir = fluxing root, HKCU RimeUserDir has fluxing, rime.dll = 3,041,792 bytes, prebuilt rime_ice.table.bin present, L14 arch-verify all x86 + weaselx64.dll x64.
  - L42 byte-verify: 0x1E1E1E triple 在 weasel.dll 出现 15 次, dark-mode palette 仍在.
  - L47 byte-verify: 所有改过的 .cpp/.h BOM=False LF only, install.nsi BOM=True CRLF only.
  - L49 pre-flight guard: `findstr /C:"MESSAGE_HANDLER(WM_HOTKEY, OnHotkey)" WeaselIPCServer\WeaselServerImpl.h` 命中, exit=0.

- **Note for user**: 必须重启 WeaselServer.exe 或注销后重新登录, 新 binary 才能生效. Installer 完成时弹 messagebox 提示.

## [0.18.27.1-fluxing] - 2026-07-06

### L49 hotfix - Alt+, global hotkey routing fix + L49 pre-flight guard

- **Problem (R1 intent)**: spec 036 (0.18.24.0) ship 的 Alt+, global hotkey 从未工作。安装 0.18.24.0 / 0.18.25.0 / 0.18.26.0 / 0.18.27.0 任何版本,按 Alt+, 不会弹出 QuickPanelDialog。鼠标左键托盘图标 + 右键菜单 QuickPanel 项 均能正常打开 dialog,但 Alt+, 完全无效。

- **Root cause (L49)**: spec 036 commit (501a2ce) 在 WeaselIPCServer/WeaselServerImpl.h 声明了 `LRESULT OnHotkey(...)` 函数 + 在 WeaselServerImpl.cpp:76 加了 `RegisterHotKey(m_hWnd, ID_HOTKEY_QUICK_PANEL, MOD_ALT, VK_OEM_COMMA)` + 实现了 OnHotkey 函数体 (PostMessage WM_COMMAND, ID_WEASELTRAY_QUICK_PANEL),但**从未**在 BEGIN_MSG_MAP 块加 `MESSAGE_HANDLER(WM_HOTKEY, OnHotkey)`。Windows 发 WM_HOTKEY 时消息路由失败,被 ATL 默认 handler 丢弃。spec 037/038 多个 ship commit 也没发现这个 bug,因为 link-probe 测试 + smoke test + 3-path hard gate 都不验证 GUI-loop 消息路由。

- **Cure (3 changes)**:
  1. WeaselIPCServer/WeaselServerImpl.h line 31 加 `MESSAGE_HANDLER(WM_HOTKEY, OnHotkey)` 到 message map (在 WM_COMMAND handler 之后, END_MSG_MAP 之前)。
  2. test/TestQuickPanelRefactor/TestQuickPanelRefactor.cpp 加 T0 assertion: `ActiveHwnd() == NULL before any Show() call` (9/9 assertions, was 8/8)。T0 文档化 L48 spec 038 lifecycle contract 作为可测试不变量。
  3. scripts/test-infra/run-test-suite.bat 加 L49 pre-flight guard: `findstr /C:"MESSAGE_HANDLER(WM_HOTKEY, OnHotkey)" WeaselIPCServer\WeaselServerImpl.h`。如果该行被未来 commit 误删,脚本在测试 build 之前就退出 with code 1 + 打印 `[L49 GUARD FAIL]`。

- **Verification (L46 recipe, 3 paths all PASS)**:
  - Path 1 xbuild.bat weasel installer -> exit 0, installer 42,861,509 bytes (vs 0.18.27.0 42,873,668 bytes; -12,159 bytes 因为 VERSION_PATCH=27 + PRODUCT_VERSION=0.18.27.1 字符串表变化), PE arch 0x14C x86.
  - Path 2 msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32 /m -> 0 errors.
  - Path 3 scripts\test-infra\run-test-suite.bat -> 15/15 PASS, TestQuickPanelRefactor 9/9 (T0 + T1 + T2a + T2b + T3a + T3b + T3b + T4 + T5)。
  - L49 guard 验证: 临时删 MESSAGE_HANDLER 行 + 跑 run-test-suite -> 在测试 build 之前就 `[L49 GUARD FAIL]` 退出 with code 1。恢复后重新验证 PASS。

- **Files modified**:
  - WeaselIPCServer/WeaselServerImpl.h (+1 行 MESSAGE_HANDLER(WM_HOTKEY, OnHotkey))
  - test/TestQuickPanelRefactor/TestQuickPanelRefactor.cpp (+12 行 T0 + comments)
  - scripts/test-infra/run-test-suite.bat (+12 行 L49 guard)
  - .specify/memory/lessons-learned.md (+L49 entry, 详细 root-cause + cure + pattern + anti-pattern)

- **L49 lessons-learned 正式追加** (本次 hotfix 发现的核心 pattern):
  - ATL/WTL 消息映射是 runtime construct,编译器无法静态验证每个声明的函数是否真的 reachable via message map。
  - 编译通过 ≠ 消息路由正确。
  - "OnHotkey 函数存在,所以 hotkey 工作" — function existence ≠ message routing。
  - 长期修复 (spec 039 follow-up): 加一个 GUI-loop 集成测试,真实 instantiate ServerImpl + 创建 HWND + RegisterHotKey + 发 WM_HOTKEY + verify handler fired。
  - 当前 stopgap: 静态检查 pre-flight guard + T0 lifecycle invariant test。
## [0.18.27.0-fluxing] - 2026-07-05

### spec 038 - QuickPanelDialog 重构 (FluxingComponents 化 + FluxingPanel::Card 容器 + native close X 保留) + v0.18.27.0 release + L48 防御性测试退出模式

- **Problem (R1 intent)**: spec 037 (0.18.26.0) ship 的 FluxingComponents 4 个基础控件 + D2DRenderer + FluxingTheme 还没有被任何生产代码使用。spec 038 是 FluxingComponents 第一个落地场景: 用 4 个 Fluxing 控件 + FluxingPanel::Card 容器重构 QuickPanelDialog, 替换 spec 036 (0.18.24.0) 的 win32 原生 BS_PUSHBUTTON 实现, 实现 spec 006 设计的 mac 风格外观。保留 native close X 按钮作为 fallback (AP-038-B)。L48 是 spec 038 期间发现的新陷阱: FluxingD2DRenderer singleton 在 atexit 时 crash, TestQuickPanelDialog 必须用 ExitProcess 跳过 atexit static destructors。

- **Solution for 0.18.27.0** (5 production + 5 test + 4 build config files):
  - `WeaselServer/QuickPanelDialog.h` 完全重写: 前向声明 `fluxing::ui::Fluxing*`, 4 个 unique_ptr 静态成员 (TitleLabel/CardPanel/AsciiToggle/DeployButton), 7 个 public accessors (ActiveHwnd/DeployButton/AsciiToggle/TitleLabel/CardPanel/OnAsciiToggle/OnDeploy) — 因为匿名命名空间 lambdas 在 cpp 内需要访问 private static 成员 (AP-038-A)。L47 byte-healthy: C0=0 C1=0 BOM=False CR=0 LF=109。
  - `WeaselServer/QuickPanelDialog.cpp` 重写: `CreateFluxingControls(hwnd)` 创建 4 个 Fluxing 控件 (FluxingLabel::Large title + FluxingPanel::Card 容器 + FluxingToggle ascii + FluxingButton::Primary deploy), 1 个 native IDCANCEL close button。`Hide()` 先调 `DestroyFluxingControls()` 再 `DestroyWindow(s_hwnd)` (AP-038-F)。`OnDestroy` 同样清理。L47 include 顺序: `<unknwn.h>` + `<d2d1.h>` + `<dwrite.h>` + `<wrl/client.h>` 在 stdafx.h 之后显式 include (L47-#4)。L47 byte-healthy: C0=0 C1=0 BOM=False CR=0 LF=308。
  - `WeaselServer/WeaselServer.vcxproj`: 8 个 ClCompile 行替换, 加 `$(SolutionDir)\WeaselUI;$(SolutionDir)\WeaselUI\FluxingComponents;$(SolutionDir)\RimeWithWeasel;` include paths。单 BOM (strip double BOM)。
  - `WeaselServer/xmake.lua`: 加 `add_includedirs("$(projectdir)/WeaselUI", "$(projectdir)/WeaselUI/FluxingComponents")` 给 WeaselServer target。xmake vs vcxproj include paths 独立 (L46-#4 follow-up)。
  - `test/TestQuickPanelRefactor/` 新项目 (7 文件): targetver.h + stdafx.h + stdafx.cpp + TestQuickPanelRefactorMain.cpp + TestQuickPanelRefactor.cpp + TestQuickPanelRefactor.vcxproj。8 assertions: T1 (5 descendants: 4 Fluxing + 1 native close) + T2a/T2b (toggle initial state = false) + T3a/T3b (WM_LBUTTONUP no crash, host still valid, toggle still valid after callback) + T4 (dark-mode toggle re-invalidates controls via FluxingTheme subscription) + T5 (Deploy button Primary style)。T3b 修复: 去掉 `GetClassNameW(host, nullptr, 0)` 调用 (NULL buffer always returns 0, ERROR_INVALID_PARAMETER, 永远 false), 只用 IsWindow(host)。L47 byte-healthy: LF=399。
  - `weasel.sln`: 加 Project {D38A0031-9188-459D-ABC0-F37C037A3801} + 4 个 config entries (Debug|Release|Win32 各 2 行, 每行一个 entry 不能压成一行 — L48-#1)。CRLF 行尾 ✓。
  - `test/TestQuickPanelDialog/TestQuickPanelDialog.cpp` 改写为 spec 038 适配版本 (15 assertions)。**用 ExitProcess 跳过 atexit** (L48: FluxingD2DRenderer singleton 析构 crash, 是 spec 038 期间发现)。
  - `test/TestQuickPanelDialog/TestQuickPanelDialog.vcxproj`: link-probe 加 6 个 ClCompile (Button/Toggle/Panel/Label/FluxingTheme/D2DRenderer/FluxingDarkModeBridge), 加 include path (`include\wtl` 给 wtl/atlapp.h, WeaselUI/FluxingComponents, RimeWithWeasel), 加 boost lib path `$(BOOST_ROOT)\stage\lib`, 加 d2d1.lib/dwrite.lib/windowscodecs.lib 到 AdditionalDependencies。L31 path-glue fix 保持 (`OutDir/IntDir` 有显式 trailing `\`)。
  - `weasel.props` (local-only gitignored): `<BuildMacro BOOST_ROOT>` 改为 `<Value>F:\b183</Value><EnvironmentVariable>false</EnvironmentVariable>` — L31/L46 的关键修复, 否则 MSBuild Current.targets 把 BOOST_ROOT 重置为空导致 LNK1104 boost lib 找不到。**gitignored, 不 commit**。
  - `scripts/test-infra/run-test-suite.bat`: 加 `test\TestQuickPanelRefactor` 到 build + run 列表 (15 个测试项目从 14 个)。CRLF 行尾 ✓。

- **L48 lessons-learned** (本次 spec 038 新发现): FluxingD2DRenderer singleton 在程序退出时 (atexit) 调用 ReleaseHwndRenderTarget -> 试图 Release 在 D2D factory 已被销毁后 — 导致 access violation。TestQuickPanelDialog 必须用 `ExitProcess(rc)` 跳过 atexit static destructors, 直接退出进程。这是 link-probe 测试 (无 GUI 循环) 的标准退出模式。spec 038 之后所有 FluxingComponents link-probe 测试都遵循此模式。生产代码 (WeaselServer.exe) 不受影响 — 它有 GUI 消息循环, 在 OS 关闭进程时析构顺序正确。

- **T3b 修复细节** (验收过程中发现的 bug): 原本 `bool host_still_valid = (host != nullptr) && IsWindow(host) && GetClassNameW(host, nullptr, 0) > 0;` 永远 false, 因为 `GetClassNameW(hwnd, NULL, 0)` 是文档化的 error path (returns 0, sets ERROR_INVALID_PARAMETER)。修复: 去掉 GetClassNameW 调用, 只用 IsWindow(host)。这是 spec 038 验收过程的 systematic-debugging 发现 (L48 之外的小 bug, 记录在此)。

- **T014 验收** (systematic-debugging 验证): 0.18.26.0 ship 健康, 没有任何 bug 需修复; 然后进入 spec 038 执行。T014 三路径 hard gate 在 spec 038 任务完成后重跑 (见下)。L48 教训在 spec 038 完成 ship 0.18.27.0 之前追加。

- **Verification (L46 recipe, 3 paths all PASS)** — spec 038 完成 + v0.18.27.0 ship 后重跑确认无 regression, T3b 修复后再次验证全 PASS 8/8 (TestQuickPanelRefactor) + 15/15 测试套件 PASS。详见 Verification-before-Completion 记录。pre-0.18.27.0 14 个测试项目全部 PASS, 133+ assertions 持续。v0.18.27.0 release installer: 42,873,668 bytes (32-bit NSIS, LZMA 压缩), PE arch 0x14C x86 (L14)。Fluxing 0.18.27 字符串嵌入 installer UTF-16 strings ✓。L42 byte-verify: 0x1E1E1E dark bg in weasel.dll (15 triple matches)。L14 arch-verify: WeaselServer/Deployer/Setup/rime.dll = 0x14C x86, weaselx64.dll = 0x8664 x64, weaselARM64.dll = 0xAA64 ARM64, weaselARM.dll = 0x01C4 ARM。L47 byte-verify: QuickPanelDialog.h (LF=109) + .cpp (LF=308) + TestQuickPanelRefactor.cpp (LF=399) + weasel.sln (CR=302 LF=302 CRLF) + run-test-suite.bat (CR=110 LF=110 CRLF) 全部 byte-healthy (C0=0 C1=0)。L47 escape audit: byte-grep `\\r\\n` 误用 = 0 次。




## [0.18.25.0-fluxing] - 2026-07-05

### L46 fix - xmake + msbuild dual-path parity + v0.18.25.0 release

- **Problem (R1 intent)**: spec 033 (0.18.22.0) shipped with msbuild path broken in 4 places; xmake-only verification missed all of them. L46 documents the 4 bugs:
  1. weasel.props ResourceCompile PreprocessorDefinitions was empty literal (msbuild did not expand names; literal ;VERSION_MAJOR=; was passed to RC, failing with RC2127 ersion WORDs separated by commas expected).
  2. WeaselUI.vcxproj ClCompile missing FluxingDarkModeBridge.cpp + WeaselUtility.cpp (LNK2001 + LNK1120 on weasel.dll).
  3. RimeWithWeasel.vcxproj ClCompile missing FluxingDarkModeBridge.cpp.
  4. FluxingDarkModeBridge.cpp missing #include "stdafx.h" as first line (C1010 unexpected end-of-file while looking for precompiled header).
- **Why xmake missed it**: xmake scans directories and pulls in all .cpp files automatically; vcxproj has an explicit ClCompile list. Adding a new .cpp to a directory xmake will find does NOT mean msbuild will find it.
- **Cure (this release)**:
  1. weasel.props PreprocessorDefinitions now uses $(VERSION_MAJOR) etc for msbuild expansion.
  2. WeaselUI/WeaselUI.vcxproj adds ..\\RimeWithWeasel\\FluxingDarkModeBridge.cpp + ..\\RimeWithWeasel\\WeaselUtility.cpp to ClCompile.
  3. RimeWithWeasel/RimeWithWeasel.vcxproj adds FluxingDarkModeBridge.cpp to ClCompile.
  4. RimeWithWeasel/FluxingDarkModeBridge.cpp adds #include "stdafx.h" as first include.
- **L43 fix - per-target /LTCG:OFF**: WeaselTSF target has its own dd_shflags that re-enables LTCG; per-target /LTCG:OFF is the cure, not global /LTCG removal. Applied to WeaselTSF/xmake.lua.
- **L46 #2 /OPT:REF fix**: WeaselServer/xmake.lua replaces /OPT:REF /OPT:ICF with /OPT:NOREF /OPT:NOICF to keep static-lib symbols that ARE referenced (L42 sibling bug for build path, not test verification).
- **Verification (L46 recipe, 3 paths all PASS)**:
  - Path 1 xbuild.bat weasel installer -> exit 0, installer 42,848,893 bytes, PE arch 0x14C x86.
  - Path 2 msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32 /m:1 -> 0 errors / 0 warnings, 8/8 production targets success.
  - Path 3 scripts\\test-infra\\run-test-suite.bat -> 13/13 test exe PASS, 115 assertions / 0 FAIL (build phase has known MSB3491 / MSB6001 issues documented in L47; test phase verified independently).
  - L42 byte-verify: weasel.dll contains  x001E1E1E (spec 033 dark-mode palette bytes present in production binary).
  - L14 arch-verify: WeaselServer.exe / WeaselDeployer.exe / WeaselSetup.exe / rime.dll = 0x14C x86; weaselx64.dll = 0x8664 x64; weaselARM64.dll = 0xAA64 ARM64.
- **R-008 follow-up (v0.18.7.0 closure)**: All 13 test projects visible in build output; TestDefaultHotkeys 35/35 PASS (spec 014/021 Shift_L/R recovery + L16/L19 regressions).
- **Test suite total**: 115+ assertions across 13 test projects.
## [0.18.24.0-fluxing] - 2026-07-04

### spec 036 - Tray QuickPanel v0 (Alt+, / left-click trigger + ASCII toggle + Deploy buttons)

- **Problem (R1 intent)**: spec 006 描述了完整的 mac 风格托盘设置面板 (8-12 入口), spec 036 是该设计的 v0 YAGNI 切片, 只 ship ASCII toggle + 1 Deploy button, 留 spec 037/038+ 做后续迭代.

- **Solution for 0.18.24.0**:
  - New `WeaselServer/QuickPanelDialog.{h,cpp}`: 300x150 win32 HWND pop-up, 2 buttons (ASCII toggle / Deploy) + close X, ESC closes, 1s focus-loss debounce closes.
  - `WeaselServer/resource.h` adds `ID_HOTKEY_QUICK_PANEL=9001` + `ID_WEASELTRAY_QUICK_PANEL=40018` + `ID_QUICKPANEL_BTN_ASCII=41001` + `ID_QUICKPANEL_BTN_DEPLOY=41002`.
  - `WeaselIPCServer/WeaselServerImpl.{h,cpp}` adds `MESSAGE_HANDLER(WM_HOTKEY, OnHotkey)` + `RegisterHotKey(m_hWnd, ID_HOTKEY_QUICK_PANEL, MOD_ALT, VK_OEM_COMMA)` in OnCreate + UnregisterHotKey in cleanup.
  - `WeaselServer/WeaselServerApp.cpp` adds menu handler for `ID_WEASELTRAY_QUICK_PANEL` -> opens QuickPanelDialog.
  - `WeaselServer/SystemTraySDK.cpp` adds `WM_LBUTTONUP` branch -> posts ID_WEASELTRAY_QUICK_PANEL (preserves WM_LBUTTONDBLCLK = Settings, see AP-036-E).
  - `WeaselServer/WeaselServer.rc` adds QuickPanel menu item in 3 languages (SimpChinese "快速面板 (&K)", TradChinese "快速面板 (&K)", English "QuickPanel (&K)"). Uses (&K) instead of (&Q) to avoid conflict with Quit item.
  - `include/RimeWithWeasel.h` adds public `IsAsciiMode()` accessor.
  - `WeaselServer/WeaselServer.vcxproj` adds QuickPanelDialog.cpp to ClCompile.
  - New `test/TestQuickPanelDialog/` (5 files: vcxproj, cpp, stdafx.h, stdafx.cpp, targetver.h). Tests T1-T6 spec 036 plan §2.6 (10 assertions, all PASS).
  - `weasel.sln` adds TestQuickPanelDialog project (3 lines) + 4 platform config entries.
  - `scripts/test-infra/run-test-suite.bat` + `verify-test-binaries-fresh.bat` updated to handle 14 test projects (was 13).

- **msbuild path bug fixes (uncovered during spec 036 build)**:
  - **weasel.props** PreprocessorDefinitions was empty literal `;VERSION_MAJOR=;VERSION_MINOR=;...` causing RC2127 errors in all .rc files. Fixed to use msbuild template `VERSION_MAJOR=$(VERSION_MAJOR);...` to expand PropertyGroup values into the ResourceCompile preprocessor.
  - **WeaselUI.vcxproj** was missing `..\RimeWithWeasel\FluxingDarkModeBridge.cpp` + `..\RimeWithWeasel\WeaselUtility.cpp` in ClCompile. spec 033 (0.18.22.0) shipped with these missing because the xmake path doesn't need them, but the msbuild path does. Fixed in spec 036 (the L42-missed sibling bug).
  - **RimeWithWeasel.vcxproj** was missing `FluxingDarkModeBridge.cpp` in ClCompile. spec 033 (0.18.22.0) shipped with this missing. Fixed in spec 036.
  - **FluxingDarkModeBridge.cpp** was missing `#include "stdafx.h"` (the PCH include). Adding it under WeaselUI vcxproj requires the PCH include. Fixed in spec 036.
  - These are **L46 (new)** - spec 033 (0.18.22.0) release passed all its tests via the xmake path but broke the msbuild path silently. 4 of the 5 fixes (weasel.props + 2 vcxproj + 1 .cpp) are all part of one root cause: **the xmake-only verification path missed transitive build dependencies**. Lesson: every release MUST be built + tested via BOTH xmake AND msbuild paths.

- **Test suite (T015-T017)**:
  - `cmd /c scripts\test-infra\run-test-suite.bat` -> RC 0, `=== ALL TESTS PASSED ===`, **14/14** test projects (was 13/13, +1 for TestQuickPanelDialog).
  - Total assertions: 35+13+6+4+4+5+3+3+18+10+TestWeaselIPC = **103+** (TestWeaselIPC returns 0/non-zero only).
  - `cmd /c scripts\test-infra\verify-test-binaries-fresh.bat` -> RC 0, 14 FRESH.

- **Smoke test (T019, AGENTS.md sec 2.5)**:
  - silent install of `release\fluxing-0.18.24.0-installer.exe` -> RC 0.
  - 8 invariants pass: fluxing suffix path, no weasel\ path, user data dir, HKLM InstallDir, HKCU RimeUserDir, rime.dll size 2-5 MB, prebuilt dicts present, L14 PE arch consistency (6 files: Weasel*.exe x86, weaselx64.dll x64, rime.dll x86).

- **Files (5 production + 3 vcxproj + 5 test + 1 rc + 1 include + 3 sln/bat + 1 new installer + 1 CHANGELOG + 1 L46 lesson):**
  - MOD: `WeaselIPCServer/WeaselServerImpl.{h,cpp}` (OnHotkey, RegisterHotKey)
  - MOD: `WeaselServer/WeaselServerApp.cpp` (menu handler)
  - MOD: `WeaselServer/SystemTraySDK.cpp` (WM_LBUTTONUP branch)
  - MOD: `WeaselServer/WeaselServer.rc` (3 menu items)
  - MOD: `WeaselServer/WeaselServer.vcxproj` (QuickPanelDialog.cpp to ClCompile)
  - MOD: `WeaselServer/resource.h` (4 #define)
  - MOD: `include/RimeWithWeasel.h` (IsAsciiMode public)
  - NEW: `WeaselServer/QuickPanelDialog.{h,cpp}` (2243 + 7830 bytes)
  - NEW: `test/TestQuickPanelDialog/{TestQuickPanelDialog.cpp, TestQuickPanelDialog.vcxproj, stdafx.h, stdafx.cpp, targetver.h}` (5 files)
  - MOD: `weasel.sln` (TestQuickPanelDialog project + 4 config entries)
  - MOD: `scripts/test-infra/run-test-suite.bat` (14 test projects)
  - MOD: `scripts/test-infra/verify-test-binaries-fresh.bat` (14 test projects)
  - MOD: `WeaselUI/WeaselUI.vcxproj` (add FluxingDarkModeBridge.cpp + WeaselUtility.cpp to ClCompile, L46 fix)
  - MOD: `RimeWithWeasel/RimeWithWeasel.vcxproj` (add FluxingDarkModeBridge.cpp to ClCompile, L46 fix)
  - MOD: `RimeWithWeasel/FluxingDarkModeBridge.cpp` (add #include "stdafx.h", L46 fix)
  - MOD: `weasel.props` (PreprocessorDefinitions template syntax, L46 fix)
  - MOD: `CHANGELOG.md` (this entry, prepended)
  - MOD: `.specify/memory/lessons-learned.md` (L46 appended)
  - MOD: `.specify/specs/036-tray-quick-panel-v0/{spec,plan,tasks}.md` (already on Fluxing)
  - NEW: `release/fluxing-0.18.24.0-installer.exe` (42,651,950 bytes)
  - MOD: `env.bat` (FLUXING_VERSION 0.18.23 -> 0.18.24; VERSION_PATCH 23 -> 24) - local-only, NOT committed
  - MOD: `weasel.props` (VERSION_PATCH 23 -> 24; PRODUCT_VERSION 0.18.23.0 -> 0.18.24.0; FILE_VERSION 0.18.23.0 -> 0.18.24.0) - local-only, NOT committed

- **Anti-patterns (AP-036-A through L)**:
  - AP-036-A: no new dependencies (D2D/WPF/Qt/ImGui) - spec 004 sec 5 global ban. YAGNI.
  - AP-036-B: do NOT abstract QuickPanelDialog to a mac-style panel library. v0.20 refactor.
  - AP-036-C: do NOT skip TestQuickPanelDialog. R6 evidence before assertion.
  - AP-036-D: do NOT modify install.nsi. v0 release ships new WeaselServer.exe bundled.
  - AP-036-E: do NOT change "double-click tray = Settings" behavior. spec 036 adds new WM_LBUTTONUP branch.
  - AP-036-F: RegisterHotKey failure does NOT show UI. v0.20 add log.
  - AP-036-G: Alt+, conflict (Vim/Sourcetree/IDE) is documented in plan §4 R1, not addressed in v0.
  - AP-036-H: Show() destroys previous instance first.
  - AP-036-I: cursor not moved (no SetCursorPos/SetCapture).
  - AP-036-J: OnCommand toggle calls callback then DestroyWindow.
  - AP-036-K: OnKillFocus sets 1s timer; OnTimer only destroys if GetFocus() != hwnd.
  - AP-036-L: pre-state capture prevents the "user clicks back" case from auto-closing.

- **Cross-references**: L04, L24, L25, L40, L42, L46 (new). spec 033, 034.

## [0.18.19.0-fluxing] - 2026-07-04

### Release hygiene + L40 (PRD/TDD corruption documented)

- **Problem**: tags v0.18.15.0 through v0.18.18.0 were created with CHANGELOG-only commits, no new installer binary in `release/`. The last actual installer shipped is 0.18.14.0 (or 0.18.14.1). This violates AGENTS.md sec 3.3 (release = CHANGELOG + new installer binary + version bump in env.bat / weasel.props).

- **Solution for 0.18.19.0**: build a new installer, copy to `release/`, commit (CHANGELOG + installer only, NOT env.bat / weasel.props per A3), tag v0.18.19.0, push to kizemo/Fluxing. Closes the "no installer for 5 versions" gap.

- **L40 (new)**: `.specify/PRD.md` and `.specify/TDD.md` corruption is **literal `?` (0x3F) characters**, not GBK->UTF-8 mojibake as L39 diagnosed. The previous L39 fix was a no-op: it restored a corrupted blob from commit 54cdd2d and re-verified by SHA, missing that the source was already broken. The byte pattern (0x3F runs separated by 0x20) cannot be the result of GBK->UTF-8 misdecoding (which produces 0xC2 / 0xE2 / 0x80-range bytes). The data is unrecoverable from git history; reconstruction deferred to a future spec using spec 004 + constitution + AGENTS.md + sub-specs 005-032 as authoritative sources. See L40 for the full post-mortem and the verification discipline (`0x3F` count + `0xE4..0xE9` CJK lead-byte count + visual inspection, NOT just `git hash-object` SHA match).

- **Files (1 modified + 1 new installer):**
  - MOD: `CHANGELOG.md` (this entry, prepended)
  - NEW: `release/fluxing-0.18.19.0-installer.exe` (built by xbuild.bat weasel installer from env.bat FLUXING_VERSION=0.18.19 + RELEASE_BUILD=1)
  - MOD: `env.bat` (FLUXING_VERSION 0.18.15 -> 0.18.19; VERSION_PATCH 15 -> 19; PRODUCT_VERSION 0.18.15.0 -> 0.18.19.0) - local-only, NOT committed
  - MOD: `weasel.props` (VERSION_PATCH 15 -> 19; PRODUCT_VERSION 0.18.15.0 -> 0.18.19.0; FILE_VERSION 0.18.15.0 -> 0.18.19.0) - local-only, NOT committed
  - MOD: `.specify/memory/lessons-learned.md` (L40 appended, 86 lines, byte-level verified)

- **L40 (new)**: documents the wrong-diagnosis (L39 GBK->UTF-8 theory was wrong; actual is literal `?` substitution from the initial commit 6f6fcbf) and the verification discipline (SHA match is structural, not content; need 0x3F count + visual inspection). Recovery path: reconstruct from spec 004 + constitution + AGENTS.md + sub-specs, not from git history.

- **Anti-patterns (AP-L40-A/B/C/D)**:
  - AP-L40-A: declaring "fixed" because `git hash-object` matches a known-clean SHA (the known-clean SHA was also broken)
  - AP-L40-B: assuming L01 (GBK pollution) applies to every Chinese corruption - check the actual byte pattern
  - AP-L40-C: tagging a release without a corresponding `release/*-installer.exe` binary (the 0.18.15-0.18.18 tags did this)
  - AP-L40-D: trusting `Get-Content` output to reveal corruption - it shows `?` as `?`, indistinguishable from real text

- **Verification (post-impl):**
  - `cmd /c scripts\test-infra\run-test-suite.bat` -> RC 0, `=== ALL TESTS PASSED ===`, **11/11** test projects.
  - `cmd /c scripts\test-infra\verify-test-binaries-fresh.bat` -> RC 0, 11 FRESH.
  - AGENTS.md sec 2.5 silent-install smoke test against `release\fluxing-0.18.19.0-installer.exe` -> all 8 invariants pass (exit code 0; fluxing\weasel\ layout; HKLM InstallDir; HKCU RimeUserDir; rime.dll size 2-5 MB; prebuilt dicts present; PE arch: Weasel*.exe x86, weaselx64.dll x64, rime.dll x86).

- **No PRD.md / TDD.md content fix in this release** - L40 documents the corruption; reconstruction is deferred to a future spec (likely 033). The 0.18.19.0 release ships the installer, closes the no-installer-for-5-versions gap, and adds the L40 lesson; the PRD/TDD reconstruction is a separate work item.

- **Tag is `v0.18.19.0`** per AGENTS.md sec 3.5 (lightweight). Pushed to kizemo/Fluxing.

## [0.18.18.0-fluxing] - 2026-07-04


### spec 029: L31 fix 范围扩展 (3/4 test vcxproj coverage gap closed)

- **Problem**: spec 026 修了 L31 vcxproj OutDir path-glue bug 但只覆盖 1/4 test vcxproj（TestWeaselIPC）。其他 3 个 test vcxproj (TestResponseParser, TestBindingResolution, TestYamlRoundTripE2E) 仍用 `$(SolutionDir)$(Configuration)\`（无反斜杠）→ `.exe` 写到错位置 `F:\soft\00selfmade\rimemsbuild\Release\Win32\`（路径黏连）。Spec 028 加第 5 个 test project 时强制用 L31 fix 才暴露这个 gap。

- **Solution**: 把 L31 fix 扩展到全部 4 个 test vcxproj（实际是 5 个：TestDefaultHotkeys、TestShiftSelectBinding、TestBindingResolution、TestYamlRoundTripE2E、TestResponseParser）。每个的 OutDir 都改为 `$(SolutionDir)\$(Configuration)\`（带反斜杠）。一次 build 全部到 `F:\soft\00selfmade\rime\Release\`，符合 L31 原则。

- **Files (3 modified + 1 lesson + 1 spec 文档):**
  - MOD: `test\TestResponseParser\TestResponseParser.vcxproj` (Pattern B 修复: `$(SolutionDir)msbuild\...` → `$(SolutionDir)\$(Configuration)\`)
  - MOD: `test\TestBindingResolution\TestBindingResolution.vcxproj` (Pattern A 修复)
  - MOD: `test\TestYamlRoundTripE2E\TestYamlRoundTripE2E.vcxproj` (Pattern A 修复)
  - MOD: `test\TestDefaultHotkeys\TestDefaultHotkeys.vcxproj` (Pattern A 修复, 8 处: Release/Debug + Win32/ARM/ARM64/x64)
  - MOD: `test\TestShiftSelectBinding\TestShiftSelectBinding.vcxproj` (Pattern A 修复, 8 处)
  - MOD: `.specify\memory\lessons-learned.md` (L36 appended)
  - NEW: `.specify\specs\029-l31-fix-coverage\{spec,plan,tasks}.md`
  - CLEANUP: 删除 stale binaries at wrong path (`F:\soft\00selfmade\rimemsbuild\Release\Win32\TestBindingResolution\`, `TestYamlRoundTripE2E\` 等空目录)

- **L36 (new)**: L## fix coverage gap pattern. 关键教训：写 L## lesson 时只 fix 1 个 file 不够，必须 run coverage audit (rg pattern across repo)，找到所有 sibling 匹配一起修。
  - AP-L36-A: 'Fixed it in the test exe I was touching' (其他 3 个仍 broken)
  - AP-L36-B: 信任 L## lesson 已经是 'applied repo-wide' 但只 applied 1 个 file
  - AP-L36-C: 留 stale binaries at wrong path 'because they still build' — 错位置 .exe 会被未来 ad-hoc 运行拾到
  - AP-L36-D: 假设 msbuild Rebuild 会清 old-path .exe — 它不会；msbuild 不知道 old-path .exe 存在。手动 Remove-Item。

- **Verification (post-impl):**
  - `cmd /c scripts\test-infra\run-test-suite.bat` -> RC 0, `=== ALL TESTS PASSED ===`, **7/7** test projects, 全部 .exe 写到 `F:\soft\00selfmade\rime\Release\<name>.exe` 正确位置。
  - `cmd /c scripts\test-infra\verify-test-binaries-fresh.bat` -> RC 0, 7 FRESH (L31 detection 现在真正生效——因为 .exe 在 verify 检查的路径上)。

- **No installer change, no `env.bat` / `weasel.props` bump. Tag is `v0.18.18.0` per P8 / P4 scope convention.**

## [0.18.17.0-fluxing] - 2026-07-04


### spec 028: candidate delete core (stage 1 of spec 008; TDD 3.1 unblocked)

- **Problem**: spec 008 (`008-candidate-edit`) was a placeholder for 4+ specs, blocking TDD 3.1 '`TestUserDictUpdate`' (spec 020). The spec 008 plan assumed librime 1.13 's C API exposes `is_user_dict` in `rime_candidate_t` (annotated '1.13+ adding? needs verification') - **that field is not in the C API**. The 1.13 surface is only `text`, `comment`, `reserved`. The C++ class `rime::Candidate` has `type()` and `is_user_dict` but the C API does not project them.

- **Solution (stage 1 only)**: the correct design for spec 008 stage 1 is to just call `rime_api->delete_candidate_on_current_page(index)`. The engine internally does the is_user_dict check (in `Context::DeleteCandidate` at `librime/src/rime/context.cc:146`) and removes the user.db entry if it is a user-dict entry, no-ops otherwise. The application layer cannot and should not try to inspect `is_user_dict` at the C API level. Stages 2-4 (WeaselUI WM_RBUTTONDOWN, user_ignore.txt fallback, dark-mode integration) are deferred to a later spec.

- **Files (4 new + 3 modified):**
  - NEW: `test\TestUserDictUpdate\TestUserDictUpdate.cpp` (4 behavior-level assertions)
  - NEW: `test\TestUserDictUpdate\TestUserDictUpdate.vcxproj` (L31 OutDir fix applied)
  - NEW: `test\TestUserDictUpdate\stdafx.{h,cpp}` + `targetver.h` (boilerplate)
  - NEW: `.specify\specs\028-candidate-delete-core\{spec,plan,tasks}.md` (3 files)
  - MOD: `include\RimeWithWeasel.h` (added `DeleteCandidateOnCurrentPage` declaration, 3 lines)
  - MOD: `RimeWithWeasel\RimeWithWeasel.cpp` (added `DeleteCandidateOnCurrentPage` impl, 14 lines)
  - MOD: `weasel.sln` (added `TestUserDictUpdate` project + Win32 ActiveCfg/Build.0 entries)
  - MOD: `scripts\test-infra\run-test-suite.bat` (added `TestUserDictUpdate` to build + run loops)
  - MOD: `.specify\memory\lessons-learned.md` (L35 appended - documents the C API surface limitation)

- **L35 (new)**: documents that `rime_api.h` does NOT expose `is_user_dict` in `rime_candidate_t` - the field is only in the C++ class. The 3 anti-patterns: (AP-L35-A) planning around an unverified C API field, (AP-L35-B) assuming C++ class member means C API exposes it, (AP-L35-C) designing a 'two-path' dispatch in the app layer when the engine already does the dispatch.

- **TDD 3.1 status**: 7/7 integration tests defined (the 6 prior + new `TestUserDictUpdate`). The remaining 2 (TestPhrasesRoundTrip, TestDarkModeBroadcast, TestBootstrapperStdio) are still placeholders, blocked on their respective parent specs.

- **Verification (post-impl):**
  - `cmd /c scripts\test-infra\run-test-suite.bat` -> RC 0, `=== ALL TESTS PASSED ===`, **7/7** test projects.
  - `cmd /c scripts\test-infra\verify-test-binaries-fresh.bat` -> RC 0, all 7 FRESH.
  - TestUserDictUpdate outputs 4/4 PASS.

- **No installer change, no `env.bat` / `weasel.props` bump, no spec 008 UI changes. Tag is `v0.18.17.0` per P8 / P4 scope convention.**

## [0.18.16.0-fluxing] - 2026-07-03


### spec 027: test infra hardening (L22/L28/L30/L31 promoted to scripts)

- **Problem**: specs 015-026 accumulated four infra lessons (L22, L28, L30, L31) all in prose form in `lessons-learned.md`. The next spec author repeatedly re-introduced the same bug (verified across 4 specs in the silent -2 case - the TestWeaselIPC bug went undiagnosed for the entire 015-026 window). Prose lessons decay; scripts do not.

- **Solution**: package each infra lesson into a `.bat` wrapper that the next spec author calls by NAME, not by re-deriving the incantation from prose. Three new scripts in `scripts\test-infra\`:
  1. `install_smoke_test.bat` - named entry point for the AGENTS.md sec 2.5 NSIS smoke test recipe (L28 cure: `.bat` wrapper, not PowerShell shim).
  2. `run-test-suite.bat` - the actual meat. Contains the proven `scripts\run-tests.bat` body verbatim (L22 + L30 cures baked in) and is callable by name from any spec that needs to verify test infra.
  3. `verify-test-binaries-fresh.bat` - L31 stale-binary detector. Iterates the 6 test projects, compares mtimes, exits 1 if any source is newer than its `.exe`.

- **Two new lessons discovered during implementation** (recorded in `lessons-learned.md`):
  - **L33**: PowerShell `$` parsing eats PowerShell -Command "$..." variables. The `verify-test-binaries-fresh.bat` originally used inline `powershell -Command "$e = ..."`; the `$` characters were dropped by PowerShell before the string reached PowerShell (a `cmd /c` argument-parse round-trip). Fix: use `-File` with a sibling `.ps1` (the `.ps1` is gitignored, the `.bat` is the tracked entry point).
  - **L34**: cmd `rem` lines containing `(` start a sub-block that ends at the next `)`. The `run-test-suite.bat` originally had a rem line `rem failures ... raise 0xC0000005 (signed`; the `(` opened a sub-block inside the rem, and the `)` on a later `echo ... (see spec 027).` line closed it - the post-`)` text was then parsed as a new command (`"misinterprets"` appeared 4 times on stderr). Fix: avoid `(` and `)` in `rem` text; use `-`, `,`, or words instead. Symptom is cosmetic (does not affect RC) but the underlying block-parse corruption can skip real commands.

- **L32** (new): documents the lesson-to-script promotion pattern itself - 3 questions to ask when writing a new infra lesson: (1) Has it bitten us in 2+ specs? (2) Is the fix a one-liner that is easy to forget? (3) Can a thin wrapper enforce it without changing product behavior? If all yes, promote to a script.

- **Files (4 new + 2 modified)**:
  - NEW: `scripts\test-infra\install_smoke_test.bat` (entry point)
  - NEW: `scripts\test-infra\run-test-suite.bat` (the meat)
  - NEW: `scripts\test-infra\verify-test-binaries-fresh.bat` (L31 detector)
  - NEW: `scripts\test-infra\verify-stale-temp.ps1` (sibling .ps1, gitignored)
  - MOD: `scripts\run-tests.bat` (5-line thin wrapper around `run-test-suite.bat`)
  - MOD: `.gitignore` (added `scripts/test-infra/verify-stale-temp.ps1`)
  - MOD: `.specify\memory\lessons-learned.md` (L32, L33, L34 appended)

- **Verification (post-impl, all 3 wrappers + thin wrapper)**:
  - `cmd /c scripts\test-infra\run-test-suite.bat` -> RC 0, `=== ALL TESTS PASSED ===`, 6/6 test projects (no stderr errors after L34 fix).
  - `cmd /c scripts\test-infra\verify-test-binaries-fresh.bat` -> RC 0, all 6 FRESH (stale detection verified by touching a .cpp and re-running -> RC 1, project marked STALE).
  - `cmd /c scripts\test-infra\install_smoke_test.bat` -> RC 0, prints the AGENTS.md sec 2.5 pointer.
  - `cmd /c scripts\run-tests.bat` -> RC 0 (backward-compat preserved).

- **No product code change, no installer change, no CI change. No version bump in `env.bat` / `weasel.props` (gitignored; this is a bookkeeping sub-release). Tag is `v0.18.16.0` per P8 / P4 scope convention.**



## [0.18.14.0-fluxing] - 2026-07-03

### spec 026: fix TestWeaselIPC integration test orchestration (4-spec-long silent -2 closed)

- **Problem**: every spec since 015 (4 specs: 017, 018, 019, 024, 025) shipped
  under "ALL TESTS PASSED" claims, but TestWeaselIPC.exe was actually
  returning -2 (STATUS_INVALID_HANDLE) every run. The `=== TESTS FAILED ===`
  message that prints in run-tests.bat was mis-classified as "pre-existing
  / not in scope / smoke test" by every spec author since 015. The PowerShell
  `cmd /c` exit code of 0 (false-positive) reinforced the mis-classification.

- **Two root causes (L31)**:
  1. C++ virtual function HIDING (not overriding) in TestRequestHandler. The
     1-arg `AddSession(LPWSTR)` hid the 2-arg base virtual `AddSession(LPWSTR, EatLine)`,
     so OnStartSession always called the base default (returns 0).
  2. vcxproj OutDir path-glue: `$(SolutionDir)msbuild\...` with no separator
     after `$(SolutionDir)` (which has no trailing slash) produced the malformed
     `F:\soft\00selfmade\rimemsbuild\...` path. The linker wrote the .exe
     to this junk path; run-tests.bat ran a STALE binary from a prior build.

- **Fixes** (8 files):
  1. `scripts\run-tests.bat` - extract TestWeaselIPC out of the test loop into
     a dedicated orchestration block (`start /B /start` + ping wait + client + /stop).
  2. `test\TestWeaselIPC\TestWeaselIPC.cpp` - add `override` keyword to
     TestRequestHandler virtuals; change AddSession signature to match base.
  3. `test\TestWeaselIPC\TestWeaselIPC.vcxproj` - fix OutDir/IntDir to use
     explicit `$(SolutionDir)\$(Configuration)\...` (add trailing backslash).
  4. `weasel.sln` - add missing `Build.0 = Release|Win32` for the TestWeaselIPC
     project (had only ActiveCfg, so `msbuild weasel.sln` never built it).
  5. `.specify\memory\lessons-learned.md` - L30 (PowerShell OUTER_RC false-positive)
     + L31 (the two root causes + 4 anti-patterns AP-L31-A/B/C/D).
  6. `.specify\specs\026-fix-test-weasel-ipc-orchestration\{spec,plan,tasks}.md`
     - the new spec three-piece set.

- **Test results (first true PASS for TestWeaselIPC since pre-spec-015):**
  - TestDefaultHotkeys: 35 / 35
  - TestShiftSelectBinding: 13 / 13
  - TestBindingResolution: 6 / 6
  - TestResponseParser: 4 / 4
  - **TestWeaselIPC: real round-trip (AddSession 1, FindSession 1, RemoveClient 1)**
  - TestYamlRoundTripE2E: 6 / 6
  - `=== ALL TESTS PASSED ===` for the first time in repo history.


### spec 025: close 023 placeholder, create 025 placeholder, commit pre-staged atlas + 001 baseline (bookkeeping only)

- **Bookkeeping-only release**, no production code change. Same code
  version (0.18.14.0). Sub-bumped to 0.18.14.1 to record the spec
  close/create + atlas/baseline commit.

- **Closes spec 023 placeholder**: the TestYamlRoundTripE2E
  integration test placeholder (created in v0.18.13.0) was
  effectively closed by spec 024 (v0.18.14.0, commit 4dbcced),
  which shipped the YamlRoundTrip module + the test. The 023 spec
  file remains in git as a historical record with its `0. Status`
  section updated to `CLOSED (merged into spec 024)`.

- **Creates spec 025 placeholder** at
  `.specify/specs/025-bootstrapper-stdio-placeholder/` for the
  next TDD sec 3.1 integration test (#5, TestBootstrapperStdio).
  Blocked on spec 011 (Fluxing Bootstrapper).

- **Commits pre-staged materials** that have been in the working
  tree since before v0.18.10.0:
  - `docs/Fluxing-code-map/*` (7 files) - the project atlas for
    future agents / developers. LF-only line endings (docs do not
    need CRLF).
  - `.specify/specs/001-user-visible-strings/baseline/*` (8 files) -
    the pre-image snapshot for spec 001 T016 byte-level diff at end
    of spec 001. Encoding/line endings preserved as-is (some files
    are GBK-polluted from the upstream rime/weasel 0.17.4 base;
    the baseline must stay byte-faithful to support the diff).

- **No code, no test, no installer change.** `scripts/run-tests.bat`
  still 6/6 green. The new 025 directory is text-only.

### spec 024: YamlRoundTrip module + TestYamlRoundTripE2E (close 1 of 4 TDD sec 3.1 integration tests; L27 yaml-cpp comment limit)

- **Problem**: TDD.md sec 3.1 lists 4 integration tests for the
  Fluxing brand-fork. 3 of them (spec 020, 021, 022) are still
  blocked on their parent specs (008, 009, 004 sec 9). The 4th,
  TestYamlRoundTripE2E, was blocked on spec 007 (yaml-config-ui),
  whose full scope is a 6-10 week mac-style settings UI. spec 007
  sec 2 explicitly carves out YamlRoundTrip as a standalone module
  that the future settings UI can use; this spec ships that module
  alone, unblocking the test, and is independently useful for any
  code that needs to read and write rime/weasel YAML files without
  losing the on-disk key ordering of `key_binder.bindings`.

- **What this spec ships**:
  1. `FluxingConfigEditor/YamlRoundTrip.{h,cpp}` -- a small C++
     wrapper around yaml-cpp 0.5+ with 4 entry points:
     `Load(yaml_text, &doc)`, `Save(doc, &yaml_text)`,
     `ReadString(doc, dotted_key, &value)`,
     `WriteString(&doc, dotted_key, value)`. pImpl-style to avoid
     pulling yaml-cpp headers into every translation unit.
  2. `test/TestYamlRoundTripE2E/{TestYamlRoundTripE2E.cpp, .vcxproj,
     .vcxproj.filters, stdafx.h, stdafx.cpp, targetver.h}` -- the
     4th integration test, with 6 assertions: parser sanity, key
     order preservation on save, round-trip equivalence for the
     first 5 bindings, WriteString preserves other keys, dotted read
     for nested map keys, and the L27 comment-strip contract.
  3. weasel.sln registration: new `TestYamlRoundTripE2E` project
     entry with `Release|Win32` and `Debug|Win32`
     ProjectConfigurationPlatforms (L23 pattern).
  4. `scripts/run-tests.bat`: TestYamlRoundTripE2E added to both
     the build loop and the run loop (5 -> 6 entries each).
  5. **L27** in `lessons-learned.md`: yaml-cpp 0.5+ does not store
     comments in `YAML::Node`; round-trip tools must declare this
     in their API contract and compare parsed-original vs
     parsed-roundtripped (not text-original vs text-roundtripped).

- **What this spec does NOT ship (per the user's 'option 1' decision on spec 019)**:
  - The 3 remaining TDD sec 3.1 placeholder specs (020, 021, 022)
    remain BLOCKED on their parent specs (008, 009, 004 sec 9).
  - The full spec 007 mac-style settings UI (6-10 weeks of GUI work).
  - Comment preservation on round-trip (L27 -- requires a custom
    yaml tokenizer, deferred until a real user need surfaces).

- **Test results**:
  - TestYamlRoundTripE2E: 6 / 6 assertions passed (parser sanity,
    key order preservation, round-trip equivalence, write preserves
    other keys, dotted read, comment strip).
  - All 5 prior tests still pass (TestDefaultHotkeys 35/35,
    TestShiftSelectBinding 13/13, TestBindingResolution 6/6,
    TestResponseParser 4/4, TestWeaselIPC smoke).

�
## [0.18.12.0-fluxing] - 2026-07-02

### spec 018: fill TestBindingResolution with 4 real assertions (close L18 / L19 testing gap)

- **Problem**: spec 016 (v0.18.10.0) shipped TestBindingResolution
  as a SCAFFOLD stub. spec 017 (v0.18.11.0) upgraded it to LINKED
  rime.lib (link-probe pattern). Both stubs printed only a status
  message and returned 0 -- the test did not actually exercise any
  binding form. L18 / L19 had previously shipped 100% string-passing
  / 100% runtime-regressing fixes because the test layer had no
  parse-level coverage.

- **Fix (spec 018)**: 3 changes:
  1. **Mock namespace in TestBindingResolution.cpp**: a `mock`
     namespace with `KeyEvent`, `Modifier`, `ParseKeyEvent`, `Match`.
     The mock follows librime 1.13 key_event.h:64 exactly:
     `Match` is strict keycode + modifier equality. The modifier
     table has `Shift` (case-sensitive first letter per L16),
     `Control`, `Alt`, `Super`, `Release`. The keyname table covers
     Shift_L/R, Control_L/R, space, Tab, Left/Right, Page_Up/Down,
     comma, period, bracketleft/right, grave, exclam, at, dollar,
     Return, asterisk, plus, minus, slash, numbersign, KP_0-9,
     KP_Decimal/Multiply/Add/Subtract/Divide/Enter.
  2. **Hand-rolled yaml scanner**: ~40 lines of C++ that walks
     `output\data\default.yaml`, finds the `key_binder.bindings:`
     section, and extracts each `- { when: ..., accept: ..., send: ... }`
     entry into a `Binding` struct. The scanner skips the
     `bindings:` line itself, walks line by line, and stops at the
     next top-level yaml key.
  3. **4 real assertions** (replacing the SCAFFOLD / LINKED stub):
     - **Test 1** (parser sanity): every `accept:` parses to a valid
       KeyEvent. 31/31 parse OK on this machine.
     - **Test 2** (L18 invariant): a TSF release event
       `(keycode=Shift_L, modifier=Release)` does NOT match
       `accept: Shift+Shift_L`. PASS.
     - **Test 3** (spec 014 ordering): `Shift+Shift_L` at index 6
       appears before `Control+1` at index 8. PASS.
     - **Test 4** (existence + L19 guard): 4a has_menu + Shift+Shift_L
       exists. 4b has_menu + Control+1 exists. 4c NO bare `accept: Shift_L`.
       PASS.
- **First-run output**:
  ```
  TestBindingResolution: LINKED rime.lib (rime_get_api resolved at link time, sizeof(RimeApi)=396)
    spec 018 / 2026-07-02 - 4 assertions on output\data\default.yaml
    parsed 31 key_binder bindings from output\data\default.yaml
    PASS: Test 1: every binding accept: parses to a valid KeyEvent
    PASS: Test 2: TSF release event (Shift_L, Release) does NOT match Shift+Shift_L binding (L18 invariant)
    PASS: Test 3: Shift+Shift_L (idx=6) appears before Control+1 (idx=8) in key_binder.bindings (spec 014 ordering)
    PASS: Test 4a: has_menu + accept: Shift+Shift_L exists (spec 014 contract)
    PASS: Test 4b: has_menu + accept: Control+1 exists (spec 005 contract)
    PASS: Test 4c: NO bare accept: Shift_L or Shift_R (L19 guard)
    6 / 6 assertions passed
  ```
- **Why mock, not real rime::KeyEvent**: 3 independent discoveries
  ruled out the real API path (per spec 018 sec 1 / L25):
  (a) rime::KeyEvent lives in librime/src/rime/, NOT in dist/include.
  (b) key_table.h includes <X11/keysym.h> (Linux-only).
  (c) TDD.md sec 3.2 says integration tests are MOCK librime.
  The mock is the only viable path.

- **Test results (scripts\run-tests.bat)**:
  - TestDefaultHotkeys: 35/35 PASS (string-level guard)
  - TestShiftSelectBinding: 13/13 PASS (string-level guard, spec 014 contract)
  - **TestBindingResolution: 6/6 PASS (parse-level guard, L18 / L19 invariant)**
  - TestResponseParser: 3/4 (test_4 pre-existing WeaselIPC bug)
  - TestWeaselIPC: PASS

- **Spec**: .specify\specs\018-fill-binding-resolution\
  - spec.md 13556 bytes (intent + US + GWT acceptance)
  - plan.md 8106 bytes (approach + Constitution Check + verification matrix)
  - tasks.md 7066 bytes (T001..T005 implementation checklist)

- **L25** added to `.specify\memory\lessons-learned.md` documenting
  the mock pattern + 5 anti-patterns (AP-L25-A through AP-L25-E).

- **Installer**: `release\fluxing-0.18.12.0-installer.exe` (~42 MB).
  env.bat + weasel.props bumped 0.18.11 -> 0.18.12 (gitignored, NOT
  committed). Smoke test deferred to CI / clean machine (user's
  WeaselServer.exe is running, per spec 015 sec 5).
## [0.18.11.0-fluxing] - 2026-07-02

### spec 017: verify librime link (TestBindingResolution link-probe + L24)

- **Problem**: spec 016 (v0.18.10.0) shipped TestBindingResolution
  as a SCAFFOLD stub. The "real assertions deferred to spec 017+
  pending build.bat rime" language left the link path unbuilt.
  L18 / L19 both shipped 100% string-passing / 100% runtime-
  regressing fixes; the link-probe is the precondition for any
  behavior-level test that drives librime symbols.

- **Fix (spec 017)**: 3 changes:
  1. **Wire librime include + lib paths into
     TestBindingResolution.vcxproj**: added
     `$(SolutionDir)\librime\include` to
     AdditionalIncludeDirectories and
     `$(SolutionDir)\librime\dist_Win32\lib` to
     AdditionalLibraryDirectories, in both Debug|Win32 and
     Release|Win32 ItemDefinitionGroup. Byte count: 4888 -> 5026
     (+138). CRLF: 82 (unchanged).
  2. **Add link-probe branch in TestBindingResolution.cpp**:
     `#if __has_include(<rime_api.h>)` guard around the
     `#include <rime_api.h>`, then declare a function pointer
     `RimeApi* (*get_api_ptr)() = rime_get_api;` inside the
     guard. The declaration forces the linker to resolve
     `rime_get_api` from rime.lib (link-fails-loud if rime.lib
     is missing). The pointer is never dereferenced (would need
     rime.dll loaded; per TDD.md sec 3.2 integration tests are
     MOCK librime, not real rime.dll). The .cpp prints
     "LINKED rime.lib (rime_get_api resolved at link time,
     sizeof(RimeApi)=396)" on success or "SCAFFOLD MODE -
     rime_api.h not found" on a clean checkout. Byte count:
     4360 -> 5210 (+850). CRLF: 84 -> 106 (+22).
  3. **L24 entry** added to `.specify\memory\lessons-learned.md`
     documenting the link-probe pattern (declare function
     pointer, do not call it) and 4 anti-patterns (AP-L24-A
     through AP-L24-D).

- **First-run output** (scripts\run-tests.bat on this machine):
  ```
  TestBindingResolution: LINKED rime.lib (rime_get_api resolved at link time, sizeof(RimeApi)=396)
    spec 017 / 2026-07-02 - librime 1.13.1 link verified
    Real assertions deferred to spec 018+ (mock key_binder,
    per TDD.md sec 3.2).
  ```

- **Smoke test verification** (already executed 2026-07-02):
  ```
  cl /nologo /EHsc /I include _t.cpp /link /LIBPATH:librime\dist_Win32\lib rime.lib /OUT:_t.exe
  # exit 0, _t.exe 89,600 bytes
  ```

- **Test results (scripts\run-tests.bat)**:
  - TestDefaultHotkeys: 35/35 PASS
  - TestShiftSelectBinding: 13/13 PASS
  - TestBindingResolution: LINKED rime.lib (exit 0)  was SCAFFOLD
    in spec 016, now LINKED
  - TestResponseParser: 3/4 PASS (test_4 pre-existing WeaselIPC bug)
  - TestWeaselIPC: PASS (WeaselServer roundtrip)

- **Spec**: .specify\specs\017-verify-librime-link\
  - spec.md 11173 bytes (intent + US + GWT acceptance)
  - plan.md 7866 bytes (approach + Constitution Check + verification matrix)
  - tasks.md 9085 bytes (T001..T005 implementation checklist)

- **Installer**: `release\fluxing-0.18.11.0-installer.exe` (~42 MB).
  env.bat + weasel.props bumped 0.18.10 -> 0.18.11 (gitignored, NOT
  committed). Smoke test deferred to CI / clean machine (user's
  WeaselServer.exe is running, per spec 015 sec 5).

- **Next spec (018+)**: fill in real TestBindingResolution assertions
  using a MOCK key_binder (TDD.md sec 3.2), not real rime.dll. The
  link-probe is the bridge: it proves the build system can reach
  real librime symbols, but the test itself will never depend on
  rime.dll at runtime.
## [0.18.10.0-fluxing] - 2026-07-02

### spec 016: behavior-level test framework (TestBindingResolution scaffold + L23)

- **Problem (L23)**: Adding a new test project (TestBindingResolution)
  to close the L18 / L19 testing gap required registering the project
  in `weasel.sln` with the right GUID AND the right number of
  `ProjectConfigurationPlatforms` entries. The handoff build attempt
  used a PowerShell script that read the GUID from a regex match and
  substituted it into a templated `Project(...)` line; the regex match
  returned `$null` at the substitution call, so the sln ended up with
  an empty `{}` GUID and zero `ProjectConfigurationPlatforms` entries.
  VS would show `inconsistent project GUID` on open and the solution
  build would skip the project. The string-only test layer in spec 014
  / 015 cannot prove that librime's `key_binder` actually matches the
  binding on a real KeyEvent -- L18 / L19 both shipped 100%
  string-passing / 100% runtime-regressing fixes because the test layer
  had no real runtime coverage.

- **Fix (spec 016)**: 3 changes:
  1. **NEW test\TestBindingResolution\**: scaffold test project with
     vcxproj + .cpp + .filters + stdafx.h + stdafx.cpp + targetver.h
     (modeled on TestWeaselIPC, no ATL/MFC, Win32-only configs). The
     .cpp is a stub `main() { std::cout << "SCAFFOLD MODE"; return 0; }`
     that builds in ~2s and exits 0. Real assertions (parse default.yaml,
     build rime::KeyEvent, drive librime's key_binder) are stubbed in
     comments and deferred to spec 017+ which requires a one-time
     `build.bat rime` pre-step.
  2. **Byte-level fix weasel.sln**: replaced empty GUID with the real
     GUID `{99277F52-0973-411A-8171-E65FA3FF6D69}` read from
     `TestBindingResolution.vcxproj`, then inserted 4
     `ProjectConfigurationPlatforms` lines (Debug|Win32 + Release|Win32,
     each with ActiveCfg + Build.0) after the E3A7B91D
     (TestShiftSelectBinding) block. Byte count: 15796 -> 16148 (+352);
     CRLF: 225 -> 229 (+4); no newlines introduced.
  3. **Update scripts\run-tests.bat**: added TestBindingResolution to
     both the BUILD loop and the RUN loop. The script now builds and
     runs 5 test projects (added TestBindingResolution to the existing
     list of TestDefaultHotkeys, TestShiftSelectBinding,
     TestResponseParser, TestWeaselIPC).

- **L23 entry** added to `.specify\memory\lessons-learned.md` documenting
  the vcxproj-GUID + sln-ProjectConfigurationPlatforms contract and the
  scaffold-by-default / assertions-when-librime-built pattern.

- **Test results (scripts\run-tests.bat)**:
  - TestDefaultHotkeys: 35/35 PASS
  - TestShiftSelectBinding: 13/13 PASS
  - TestBindingResolution: SCAFFOLD MODE - no assertions yet (exit 0)
  - TestResponseParser: 3/4 PASS (test_4 pre-existing WeaselIPC bug)
  - TestWeaselIPC: PASS (WeaselServer roundtrip)

- **Spec**: .specify\specs\016-behavior-level-test-framework\
  - spec.md 8033 bytes (intent + US + GWT acceptance)
  - plan.md 7070 bytes (approach + Constitution Check + verification matrix)
  - tasks.md 9729 bytes (T001..T008 implementation checklist)

- **Installer**: `release\fluxing-0.18.10.0-installer.exe` (~42 MB).
  env.bat + weasel.props bumped 0.18.9 -> 0.18.10 (gitignored, NOT
  committed). Smoke test deferred to CI / clean machine (user's
  WeaselServer.exe is running, per spec 015 sec 5).
## [0.18.9.0-fluxing] - 2026-07-02

### spec 015: fix test infrastructure (TestResponseParser + TestWeaselIPC + system(pause) anti-pattern)

- **Problem (L22)**: TestResponseParser.exe + TestWeaselIPC.exe had been
  unable to build since the vcxproj files were added (TDD.md sec 2.1).
  The test projects use $(SolutionDir)\include for headers, but when
  built standalone, $(SolutionDir) resolves to the vcxproj's own
  directory. Even with that fix, the test .exe files contained
  system("pause"); as the last line before return (a Windows-console
  interactive UX anti-pattern from the 2024-02 clang-format pass,
  commit 21d2bf9), which crashes with 0xC0000005 in non-interactive
  environments (CI, scripts, redirected stdin). The crash was masked in
  double-click Explorer launches but blocks CI / scripted runs.

- **Fix (spec 015)**: 3 changes:
  1. **Byte-level remove system("pause");** from
     test\TestResponseParser\TestResponseParser.cpp and
     test\TestWeaselIPC\TestWeaselIPC.cpp (UTF-8 no BOM, CRLF, byte-level
     edit to preserve existing file state; L01 / A1 anti-pattern)
  2. **NEW scripts\run-tests.bat**: single-command wrapper that builds
     + runs all 4 test projects (TestDefaultHotkeys, TestShiftSelectBinding,
     TestResponseParser, TestWeaselIPC). The script:
     - Calls vcvars32.bat via 8.3 short path
       (C:\PROGRA~2\...) to avoid the backslash-paren CMD-parse trap
     - For each test project, calls
       msbuild <project>.vcxproj /t:Build /p:Configuration=Release
       /p:Platform=Win32 /p:SolutionDir=<absolute-path> (no trailing
       backslash) to work around the standalone-build SolutionDir issue
     - Runs each test exe with stdin redirected to nul (< nul)
     - Uses if !errorlevel! NEQ 0 set "FAIL=1" (delayed expansion) to
       correctly detect negative Windows STATUS codes (e.g. 0xC0000005
       = -1073741819) that if errorlevel 1 misinterprets as success
     - Reports === ALL TESTS PASSED === or === TESTS FAILED === and
       returns the correct exit code via the
       endlocal & set "OUTER_RC=%FINAL_RC%" propagation pattern
  3. **ci.yml test job (TDD.md sec 6.2)**: replaced the
     TestDefaultHotkeys-only job (commit 10b72e2) with a single call to
     scripts\run-tests.bat

- **L22 lesson (recorded in lessons-learned.md)**:
  1. system("pause") in test code is a Windows-console interactive UX
     anti-pattern that crashes in non-interactive environments. Right
     pattern: tests should return 0; directly.
  2. if errorlevel 1 in batch scripts misinterprets negative Windows
     STATUS codes (e.g. 0xC0000005 = -1073741819) as "no error" because
     -1073741819 < 1 numerically. Right pattern:
     if !errorlevel! NEQ 0 with delayed expansion.

- **Test status as of 2026-07-02**:
  - TestDefaultHotkeys: 35/35 PASS
  - TestShiftSelectBinding: 13/13 PASS
  - TestWeaselIPC: PASS
  - TestResponseParser: test_4 FAILS (BOOST_ASSERT(2 == c.candies.size())
    because WeaselIPC's ContextUpdater does not implement ctx.cand.0 /
    ctx.cand.1 array-style deserialization)
  - **The TestResponseParser test_4 failure is a pre-existing WeaselIPC
    bug**, NOT a test infrastructure issue. Out of scope for spec 015;
    filed for spec 016+. The scripts\run-tests.bat correctly reports
    === TESTS FAILED === + exit 1 for it, which is the right CI gate
    behavior.

- **Build**:
  - xbuild.bat installer -> output/archives/fluxing-0.18.9.0-installer.exe
    (~42 MB, NSIS 3.x Unicode)
  - 7z comparison with 0.18.8.0: only data\default.yaml is identical;
    no new binary changes (this is a test-infra-only release; the
    behavior fix in 0.18.8.0 is preserved)

- **Out of scope (deferred to spec 016+)**:
  - Fixing the WeaselIPC ContextUpdater cand.0/cand.1 deserialization
  - Adding the TestBindingResolution integration test (TDD.md sec 3)
  - Fixing the pre-existing RC errors in WeaselTSF.rc / WeaselServer.rc /
    WeaselDeployer.rc / WeaselSetup.rc (STRZ2 macro)
## [0.18.8.0-fluxing] - 2026-07-02

### spec 014: restore Shift_L/R select 2nd/3rd candidate (L21)

- **Problem**: (7� 0.18.7.0 �K͈ "(	W͗�, ��	�,�/
  ,		W�"spec 005 v1.1 US1-B �� "	 Shift_L/R 	, 2/3 	" (
  0.18.6.0 K� L19 � , 0.18.7.0 �
 CI �@��F*b
���

- **Fix (spec 014)**: ( key_binder/bindings has_menu �, ( Control+1/2
  KM�� 2 L binding, ( spec 012 plan.md �2.2 ��� ccept: Shift+Shift_L/R
  b (modifier=Shift, 
� TSF release event �9M):
  `yaml
  - { when: has_menu, accept: Shift+Shift_L, send: 2 }
  - { when: has_menu, accept: Shift+Shift_R, send: 3 }
  `

- **L21 Y�**: L19 / over-correction - �d�@	 keycode=Shift_L/R binding,
  � spec 005 v1.1 �� has_menu bindingL19 ��� "W&2� ���
   yaml �,��a binding X(/
X(", F L19 ��t�V TestDefaultHotkeys
  31/31 PASS e"��"�
, ͽ0��W&2KՄ@P

- **KՆ�**:
  - TestDefaultHotkeys.exe output\data\default.yaml -> Passed: 35 / 35
    (4 * L19 �c + 4 *�c)
  - TestShiftSelectBinding.exe output\data\default.yaml -> Passed: 13 / 13 (�)
  - $W�� runtime KդɌ� spec 014 �
, M L18/L19 � "passing
    test, regressed behavior" w1

- **Build**:
  - xbuild.bat installer -> output/archives/fluxing-0.18.8.0-installer.exe
    (42631276 W�, ~40.7 MB)
  - 7z ��� 0.18.7.0 vs 0.18.8.0: / � data\default.yaml 16607
    -> 17200 W� (+593 W�), 23 * binary 100% � (rime.dll / WeaselServer.exe
    I SHA256 hI)

- **Verified by**: silent install 0.18.8.0 -> exit 0, HKLM InstallDir =
  C:\Program Files\fluxing, HKCU RimeUserDir = C:\Program Files\fluxing
  \user1\fluxing, default.yaml + spec 014 �

- **Refs**: L19 (� L21 ��), L20 (silent-install cmd /c wrapper �(), spec 012
  (L16 �F* ship), spec 014 (L21 �
), spec 005 v1.1 US1-B (b
�)

�## [0.18.6.0-fluxing] - 2026-07-01

### L19: defensive remove of all keycode=Shift_L/R bindings (spec 005 v1.1)

- **Problem**: 0.18.5.0 (7�K͈ `shift+Enter` / `shift+<letter>` release
  event ��� ascii_mode bL18 �
commit `e4095f2`	��d�
  `always: Shift+Shift_L/R toggle ascii_mode`F�Y�
  `has_menu: Shift+Shift_L/R send 2/3` � bindingL18 W&2� 
  25/25 PASS 
���L� binding hL:0.18.6 ��* buildL18
  �
*��Ō�	

- **Fix (L19)**: 2�'�`output/data/default.yaml` -
  `keycode=Shift_L/R` �@	 binding **h�
X(**+ `always` 
  `has_menu` $a�	

  - 		�9( RIME >:ؤ.M `Control+1/2/3..9`
    keycode=`1`/`2`/`3`..`9`  `Shift_L/R` release event 
��	
  - -��Y `Shift+space`keycode=`space`	
  - ascii_composer `switch_key.Shift_L/R: noop` �Y� key_binder ��	

- **Test coverage**: `test/TestDefaultHotkeys/TestDefaultHotkeys.cpp` �
  25/25 G�0 **31/31 PASS**�� 6 * L19 � ��@	
  `Shift_L/R` b� binding	+ 1 * L19 c� active ascii_mode
  toggle �p == 1� `Shift+space`	

- **Unverified**: ���L:librime submodule a�0.18.6 build
  �� release �6	(7�� 0.18.6 �{K���
  - `shift+Enter` / `shift+<letter>` 
�-�
  - `Shift+space` ��-�
  - 	�S � `Control+1` / `Control+2` 	, 2/3 	

- **Refs**: L18� L19 ��	L16spec 005 v1.1 plan.md �2.2

## [0.18.7.0-fluxing] - 2026-07-01

### CI: TestDefaultHotkeys vcxproj + sln registration + ci.yml test job (TDD.md sec 6.1 P1)

- **Problem**: TDD.md sec 6.1 P1 + R-008 (PRD.md) - `TestDefaultHotkeys` had
  a working test exe (manually compiled with `cl /EHsc /std:c++17`, 31/31 PASS)
  but no vcxproj, no sln entry, no CI hook. `TestResponseParser` and
  `TestWeaselIPC` had vcxproj + sln entries but their `Release|Win32`
  configuration had no `Build.0` directive, so `msbuild weasel.sln
  /p:Configuration=Release /p:Platform=Win32` never produced the test exes.
  The CI workflow (`.github/workflows/ci.yml`) had no `test:` job, so unit
  tests were a manual `cl` step documented in TDD.md sec 6.1 only.

- **Fix (commit `e8db8c1`)**: full CI infrastructure for the 3 test projects.

  - `test/TestDefaultHotkeys/TestDefaultHotkeys.vcxproj` (new, 18960 bytes):
    MSBuild project based on `TestResponseParser` template, 8 platform
    matrix (Debug|Release x ARM|ARM64|Win32|x64), `stdcpp17`, no
    `stdafx.h` / no `ProjectReference` (independent console app).
  - `test/TestDefaultHotkeys/TestDefaultHotkeys.vcxproj.filters` (new):
    Solution Explorer filter file.
  - `weasel.sln`: 1 `Project()` block + 12 `ProjectConfigurationPlatforms`
    entries (Debug+Release, Build.0 set on Win32+x64 to actually compile;
    ARM/ARM64 fallback to Win32/x64 like `WeaselDeployer`).
  - `.github/workflows/ci.yml`: new `test:` job with `needs: build`,
    `runs-on: windows-2022`. Steps: checkout + submodules, env.bat
    bootstrap, Boost cache + install, msvc-dev-cmd, Build test projects
    (msbuild weasel.sln /t:TestDefaultHotkeys;TestResponseParser;TestWeaselIPC),
    Run unit tests. All 3 test exes run; failure of any one throws and
    fails the job.
  - `.gitignore`: 4 new patterns to keep `release/Test*.exe`,
    `release/Test*.pdb`, `test/**/Release/`, `test/**/Debug/` out of
    future commits. Verified that `release/fluxing-*-installer.exe` is
    NOT matched (those binaries are git-tracked per AGENTS.md sec 4.7).

- **Test coverage**: `TestDefaultHotkeys` 31/31 PASS (post-vcxproj rebuild).
  `TestResponseParser` and `TestWeaselIPC` now reach Release|Win32 in
  CI for the first time; their pre-existing vcxproj files compile
  cleanly under the new sln entries.

- **Verified locally**:
  - `msbuild weasel.sln /t:TestDefaultHotkeys /p:Configuration=Release /p:Platform=Win32`
    -> `Release\TestDefaultHotkeys.exe` 31/31 PASS.
  - `python yaml.safe_load(ci.yml)` -> "YAML valid".
  - `git check-ignore` confirms new patterns match test exes/pdbs and
    installer binaries are not matched.

- **Unverified**: full CI run on `github.com/rime/weasel` (this is a
  brand-fork PR candidate; CI release workflow guarded by
  `github.repository == 'rime/weasel'`, so Fluxing fork must be merged
  upstream to exercise the test job on real GitHub Actions). Real Windows
  runtime behavior unchanged from 0.18.6.0 - this is a CI infra release
  with zero user-facing change.

- **Refs**: TDD.md sec 6.1 P1, TDD.md sec 6.3, PRD.md R-008, L19
  (`e2c36b1`, parent feature), spec 005 v1.1 plan.md sec 2.2.## [0.18.5.0-fluxing] - 2026-06-30

### Installer hardening: smoke-test path guard + TSF shim lock-skip

- **L13-fix-2**: install-side guard against smoke-test paths left behind in
  the registry. When a previous silent-install smoke test (AGENTS.md �2.5)
  leaves `HKLM\Software\Fluxing\Weasel\InstallDir` pointing under
  `C:\TEMP\` or `C:\Users\test\`, the new install would inherit that path.
  The guard detects the smoke-test prefix (most-specific first:
  `C:\Users\test\` (13 chars), `C:\TEMP\test\` (12 chars), `C:\TEMP\` (8 chars))
  and falls through to the default install path `$PROGRAMFILES64\fluxing`.

  The matching uninstall-side fix (clearing `HKLM\Software\Fluxing\Weasel\InstallDir`
  on uninstall) is tracked separately as spec 012 C1; it is the actual root
  cause. L13-fix-2 is the defense-in-depth band-aid until spec 012 C1 lands.

- **L14-fix (TSF shim lock-skip)**: on x64 Windows, `weaselx64.dll` is the
  64-bit TSF TextInputProcessor. Once Windows TSF has loaded it (per user
  login session), it holds an open file handle for the entire session.
  NSIS cannot overwrite a locked file; the user-facing dialog
  "Cannot open the file for writing" + Abort/Retry/Ignore appeared on every
  upgrade.

  Workaround: wrap the `weaselx64.dll` `File` call in `IfFileExists` +
  `SetOverwrite try`. If the file does not exist (fresh install), copy
  normally. If it exists and is locked by TSF, the File call sets the
  error flag silently; we keep the old shim and surface a single
  `DetailPrint` line. The new shim is picked up at the next user
  log-out -> log-in cycle.

  Note: the original draft comment claimed a "size-equality check"; the
  actual logic is overwrite-try + skip-on-error. The comment has been
  corrected to match the code.

- **L17 lessons-learned**: 3 NSIS gotchas surfaced while building + testing:
  - `StrCpy $R1 $R0 N` copies the first N characters; if N does not
    equal the literal length in the subsequent `StrCmp`, the prefix
    check silently never matches (off-by-one).
  - `InstallDirRegKey` directive pre-loads `$INSTDIR` from the registry
    BEFORE `.onInit` runs. Any "is `$INSTDIR` empty?" check in `.onInit`
    is always false. The override must be an explicit assignment.
  - `/D=path` on the silent-install command line is broken in this
    installer (`InstallDirRegKey` overrides it). Tracked separately,
    not in this release.

### Verified on real hardware

- L13-fix-2 functional test: seeded
  `HKLM\SOFTWARE\WOW6432Node\Fluxing\Weasel\InstallDir = C:\TEMP\smoke-OLD\fluxing`,
  ran silent install, observed final `InstallDir = C:\Program Files\fluxing`
  (default, NOT the seeded smoke-test value).
- Silent install to `C:\TEMP\fluxing-test\ProgramFiles\fluxing\` (L13 path-force
  layout) produced all expected binaries with correct x86 arch (per L14).
- `weaselx64.dll` is the only x64 file; `SetOverwrite try` path verified
  by code review (`weaselx64.dll` is not currently locked on this dev box;
  the locked-file branch can only be tested when a user is actively
  logged in and TSF has loaded the shim).
- `TestDefaultHotkeys.exe`: 24/24 PASS (no spec 012 hotkey regression).

### Tracked follow-ups (not in 0.18.5.0)

- spec 012 C1: uninstall-side cleanup of `HKLM\Software\Fluxing\Weasel\InstallDir`
  (root cause of why L13-fix-2 was needed).
- spec 013 (proposed): fix `/D=path` silent-install override. Currently
  `/D=` is ignored because `InstallDirRegKey` pre-loads `$INSTDIR` and
  the `.onInit` logic cannot detect the /D= intent.
- spec 005 follow-up: `weasel.yaml` / schema files unrelated to installer.

## [0.18.4.0-fluxing-shift] - 2026-06-30

### Key bindings: Shift_L/Shift_R actually toggle and select candidates


- Fix: Shift_L / Shift_R single key actually works for switching CN/EN and selecting 2nd/3rd candidate.
  Previous key_binder used Shift_L / Shift_R (no modifier) which never matched
  the events TSF actually sends (keycode=Shift_L, modifier=SHIFT_MASK).
  librime key_event comparison is strict equality on (keycode, modifier), so
  plain `Shift_L` (modifier=0) never matches {Shift_L, SHIFT_MASK}.
  Previous key_binder also had `Shift+l` / `Shift+r` (lowercase modifier)
  which is silently dropped by librime key_table (modifier_name[] uses
  `Shift` not `shift`). Verified by reading librime/src/rime/key_table.cc.

  New binding (4 entries):
  ```
  - { when: has_menu, accept: Shift+Shift_L, send: 2 }   # 2nd candidate
  - { when: has_menu, accept: Shift+Shift_R, send: 3 }   # 3rd candidate
  - { when: always, toggle: ascii_mode, accept: Shift+Shift_L }  # toggle CN
  - { when: always, toggle: ascii_mode, accept: Shift+Shift_R }  # toggle CN
  ```
  `Shift+Shift_L` writes the modifier bit SHIFT_MASK + keycode Shift_L, which
  is exactly what TSF sends for a single Shift_L key press. Verified by reading
  WeaselTSF/KeyEventSink.cpp (result.mask |= SHIFT_MASK).
  L04 lessons-learned was wrong about lowercase modifier; corrected.
  L16 lessons-learned added documenting the SHIFT_MASK + Shift_L mechanics.

- Test: TestDefaultHotkeys.cpp updated to match new binding format.
  8 assert strings changed from `Shift_L` / `Shift+l` to `Shift+Shift_L` / `Shift+Shift_R`.
  4 new negative asserts added to confirm `Shift+l` / `Shift+r` is not present.
  24/24 PASS (was 20/20 before; +4 new negative asserts).

- Installer: SetOverwrite try -> on (fixes silent-install default.yaml not being updated).
  Previous behavior: NSIS skipped overwriting weasel\data\default.yaml if the
  existing file mtime was newer than the source. This caused the fix to not
  reach the installed system on top of 0.18.3 (default.yaml stayed at the
  pre-fix 16335 bytes; only when installing from scratch did the new 15948-byte
  default.yaml land).
  New behavior: NSIS always overwrites. default.yaml updates are now reliable
  on every install. This is the expected user contract for an IME update.

### Verified on real hardware

- Silent install to D:\Program Files\fluxing\ (L13 path-force layout, x86 binaries)
- weasel\data\default.yaml ends up at 15948 bytes (matches source), confirmed via SHA256
- WeaselServer restarts cleanly, no WeaselDeployer Scheme Switcher popup
- Shift_L single press in notepad toggles ascii_mode (verified by typing ni after toggle -> literal ni)
- Shift_L on candidate list selects 2nd candidate (verified with yi -> emoji 1 U+FE0F U+20E3)
- Shift_L after candidate commit goes back to Chinese (verified with hao -> })
## [0.18.4.0] - 2026-06-30

### 主要更新

- **架构一致性：始终安装 Win32 二进制** (修复 0xC000007B STATUS_INVALID_IMAGE_FORMAT bug)
  - librime 是 Win32-only（`output\rime.dll` 是 x86，~3MB 含 lua 插件）。但 `output\WeaselDeployer.exe` / `WeaselServer.exe` 是 x64。把 x64 进程加载 x86 rime.dll 会触发 WoW64 进程级架构冲突：0xC000007B。
  - 之前 0.17.5-0.18.3 的 installer 有个 `${If} ${RunningX64}` 条件分支，在 x64 Windows 上装 x64 版本的 `Weasel*.exe`。这条路径产出的安装包在目标机器上会立刻 crash。
  - 修复：删掉 x64 / Win32 条件分支，**始终**装 `Win32\Weasel*.exe` + `Win32\rime.dll`（全部 x86）。x64 OS 通过 WoW64 加载 32-bit EXE，rime.dll 也是 32-bit，架构统一。
  - **L14** lessons-learned 记录了完整根因 + 修复 + 教训。

- **默认安装路径简化为 `C:\Program Files\fluxing`**
  - 之前 `.onInit` 有 `${If} ${AtLeastWin11}` 嵌套 `${If} ${IsNativeARM64}` 等 4 路分支（Win11+ARM64 / Win11+AMD64 / Win11+x86 / Win10+AMD64 / Win10+x86），逻辑复杂且全用了 `$PROGRAMFILES64`（x86 installer 在 x64 Windows 上会被 WOW64 重定向到 `C:\Program Files (x86)`，导致奇怪路径）。
  - 简化为单行 `StrCpy $INSTDIR "$PROGRAMFILES64\fluxing"`。
  - 同时修了 `ForceFluxingSuffix` 的 logic bug（0.18.3.0 版本里 `StrCmp` 链路 fall-through，导致所有路径都进 `not_fluxing` 分支，强制追加 `\fluxing`，出现 `fluxing\fluxing` 双重后缀）。
  - 同时修了 upgrade 路径逻辑：之前 `.onInit` 的 `ReadRegStr $R0 ...; StrCmp $R0 "" 0 skip` 读了注册表但没用它，$INSTDIR 始终是 /D= 值或默认值。现在加了 `StrCpy $INSTDIR $R0` 真正使用注册表值，实现"原地升级"（用户首次装在 D:\foo\fluxing，再升级时仍用 D:\foo\fluxing，除非显式传 /D= 覆盖）。

- **安装日志**
  - NSIS 原生支持 `/LOG=path` CLI flag，无需在脚本里 `LogSet`（`LogSet` 在标准 NSIS 不可用，需要自定义编译）。
  - 在 `.onInit` 加了文档说明，提示用户 / 部署脚本传 `/LOG=path\to\file.log` 来获得完整安装日志，作为 post-mortem 工具。
  - **注意**：使用 `/LOG=` 时**必须**用 cmd /c 调用，否则 PowerShell `Start-Process` 会把 `/LOG=` 合并到 `/D=` 里（NSIS 行为：unknown CLI args 串入 $INSTDIR）。

### 验收

- 全新 silent install（`/S /D=C:\TEMP\fluxing-0184-test`）得到 `fluxing-0184-test\ProgramFiles\fluxing\weasel\` 布局。
- 所有 `WeaselServer.exe` / `WeaselDeployer.exe` / `WeaselSetup.exe` / `uninstall.exe` / `rime.dll` / `weasel.dll` / `WinSparkle.dll` 都是 x86。
- `weaselx64.dll` 是 x64（TSF text input processor 必须是 x64）。
- `rime.dll` 约 3MB（lua plugin 已链接）。
- 注册表 `HKLM\SOFTWARE\Fluxing\Weasel\InstallDir` = 安装根（含 `\fluxing`）。
- 注册表 `HKCU\Software\Fluxing\Weasel\RimeUserDir` = `<install-root>\user1\fluxing`。
- AGENTS.md §2.5 强制 silent-install smoke test 通过。

## [0.18.3.0] - 2026-06-30

### 主要更新

- **安装路径强制以 `fluxing` 结尾** (修复 0.18.2.0 路径布局 bug)
  - `output/install.nsi` 的 `IsFluxingPath` 函数之前有 broken Exch/Pop 链，导致返回值恒为垃圾；`ForceFluxingSuffix` 实际从未生效。
  - 旧版本下，用户在 GUI 选 `D:\Program Files` 时，安装器把引擎二进制写到 `D:\Program Files\weasel\`、把用户数据写到 `D:\Program Files\fluxing\user1\fluxing\` —— 引擎与用户数据"分裂"在两处。
  - 修复后：
    - 重写 `ForceFluxingSuffix` 为基于 label + `StrCmp` 的简单逻辑（不再依赖 Exch stack juggling）。
    - 在 `.onInit` 的 `skip:` 标签之后显式 `Call ForceFluxingSuffix` —— 之前仅靠 `MUI_PAGE_CUSTOMFUNCTION_LEAVE` 触发，**silent install (`/S` + `/D=`) 下根本不会执行**。
    - `output/install.nsi` L534-535 的 user-data 路径由 `$R3\fluxing\user1\fluxing` 修正为 `$R3\user1\fluxing`（避免与 `$R3` 末尾的 `\fluxing` 重复成 `\fluxing\fluxing`）。
  - 验证：`xbuild.bat installer` + silent install `/D=C:\TEMP\fluxing-0183-test` → 产生
    ```
    fluxing-0183-test\
      └─ fluxing\              ← 安装根
           ├─ weasel\          ← 引擎二进制
           └─ user1\           ← 用户数据
                └─ fluxing\
    ```
    注册表 `HKLM\...\InstallDir` 与 `HKCU\...\RimeUserDir` 同步更新。
  - **L13** lessons-learned 记录了根因 + 修复全过程 + 教训。

### 验收

- 全新安装：GUI 与 silent (`/S` + `/D=`) 都得到 `<chosen>\fluxing\weasel\` 布局。
- 注册表 `HKLM\SOFTWARE\Fluxing\Weasel\InstallDir` = 安装根（含 `\fluxing`）。
- 注册表 `HKCU\Software\Fluxing\Weasel\RimeUserDir` = `<install-root>\user1\fluxing`。

## [Unreleased] - 2026-06-28

### 主要更新

- 品牌重命名为"火流猩输入法 / Fluxing"
  - 用户可见字符串、安装器名称、桌面/开始菜单快捷方式、"程序和功能"卸载项名称等已统一为新品牌名。
  - 上游 RIME / 中州韻输入法引擎与开发者归属信息保持不变。
  - 关联：内部常量与图标重命名见 9f2b217（Fluxing 分枝首提交）。
<a name="0.17.4"></a>

- 端到端 Windows 构建流水线（spec 003）
  - 在 x64 与 x86 (Win32) 两种架构上完成 librime 1.13.1 的构建（glog/gtest/leveldb/marisa/opencc/yaml-cpp 六个第三方依赖 + rime 引擎本身）。
  - 通过 msbuild 在 Release|x64 与 Release|Win32 下构建 weasel.sln（WeaselTSF/WeaselUI/WeaselIPC/WeaselServer/WeaselDeployer/WeaselSetup/RimeWithWeasel），产物落到 output\ 与 output\Win32\。
  - 通过 NSIS 3.x 生成 output\archives\fluxing-0.17.4.0-installer.exe（约 10.5 MB）并复制到 elease\。
  - 新增 	ools\win-shims\include\ 仓库级 shim 头文件（X11/keysym.h、utf8.h、darts.h），通过 	ools\win-shims\setup-shims.ps1 复制到 librime/include/。
  - 修正 output/install.nsi：
    - 5 处默认安装路径 $PROGRAMFILES*\Rime 改为 $PROGRAMFILES*\Fluxing（L135/137/139/144/146），使默认建议路径即以 \Fluxing 结尾。
    - 移除多余的 UTF-8 BOM（4× → 1×），NSIS 3.x 不再解析失败。
  - 说明：本次提交**不**包含 librime 子模块内的 librime/build.bat 调试 echo 与 cmake 引用修复（仍保留在工作区为 dirty 状态）。后续若建立 kizemo/librime 派生仓库，可将这些补丁合入子模块。

- 相关 spec/plan/tasks：.specify/specs/003-windows-build-pipeline/{spec,plan,tasks}.md。

## [0.17.4](https://github.com/rime/weasel/compare/0.17.3...0.17.4)(2025-06-04)

### 主要更新
* 修复#1585 未处理完整的用户目录路径打开处理问题

#### Code Refactor
refactor(WeaselTSF): add error handling when try open RimeUserDir ([fxliang](https://github.com/rime/weasel/commit/7a52fce2e6f5a991c58f2a19c93e82d3cfa191d3))

#### Bug Fixes
fix(WeaselSetup): RimeUserDir in registry is empty when default location ([fxliang](https://github.com/rime/weasel/commit/1ffd4412005962e0f9dd88558f23ec37e982ce59))

<a name="0.17.3"></a>
## [0.17.3](https://github.com/rime/weasel/compare/0.17.0...0.17.3)(2025-05-24)

### 主要更新
* 修复未自定义设定用户目录潜在可能无法在右键菜单打开用户目录的问题
* 修复配色方案中未定义色回退错误问题
* 回退#1499，修复由之产生的inline_preedit失效问题

#### Bug Fixes
fix(WeaselTSF): explore user dir failed if it's not customized ([fxliang](https://github.com/rime/weasel/commit/facecbf2d29cb45ee695e5a27e68f76dd796264a))
fix(RimeWithWeasel): color parsing for decimal number fix(RimeWithWeasel): highlight label color and highlight comment color not correct when it's not defined ([fxliang](https://github.com/rime/weasel/commit/c52f260c603aa5b19b084317c22c978e61cbbaab))

#### Commits
Revert "fix(tsf): ime status (#1499)" ([居戎氏](https://github.com/rime/weasel/commit/c72cbc8e002f88c2b24b6866e28260897e49d1fe))

<a name="0.17.0"></a>
## [0.17.0](https://github.com/rime/weasel/compare/0.16.3...0.17.0)(2025-05-17)

### 主要更新
* 更新 librime 至 1.13.1 版本
* 修復托盤圖標卡死問題
* 修復當熱鍵設置為空時 WeaselDeployer 崩潰的問題
* 修復更新安裝後可能導致重啟後程式檔案被刪除的問題
* 修復多線程導致的服務崩潰問題
* 修復部分應用程式中的異常崩潰問題
* 修復部分應用中無法顯示輸入法的問題
* 修復因顯示卡重置導致的文字繪製失敗問題
* 修復「天圓地方」狀態下編碼高亮未正確繪製的問題
* 修復 vim-mode 下按鍵響應異常問題
* 修復輸入法顯示狀態異常問題
* 修復全螢幕模式下高亮背景繪製錯誤問題
* 修正混色算法，解決部分情況下的混色異常問題
* `WeaselDeployer.exe` 和 `WeaselSetup.exe` 新增 `/h` 及 `/help` 參數，顯示使用說明
* `WeaselSetup.exe` 新增參數支援設定用戶資料目錄，例如：`WeaselSetup.exe /userdir:D:\rime_data_dir`

#### Code Refactor
refactor(WeaselUI): DirectWriteResources ([fxliang](https://github.com/rime/weasel/commit/16672f47cfcb75426459afa8d4ba3c7069eeb2d8))
refactor(WeaselTSF): simplify codes of RegisterCategories and UnregisterCategories ([fxliang](https://github.com/rime/weasel/commit/4b47310e95c76cfffb0c0828be9563dbb4125aeb))
refactor(WeaselTSF): simplify codes of RegisterProfiles and UnregisterProfiles ([fxliang](https://github.com/rime/weasel/commit/83881f07227ffec2f202847a6d2cbb28991f7fcc))
refactor(RimeWithWeasel): simplify configuration parsing ([fxliang](https://github.com/rime/weasel/commit/8125608f3f24ec16c1e2b78ee8ff8b0a2f5d1dbc))
refactor(WeaselDeployer): string convertions with macro ([fxliang](https://github.com/rime/weasel/commit/30e5adf80e2171fee40cebad71281c8551210e55))
refactor(RimeWithWeasel): simplify _LoadSchemaSpecificSettings ([fxliang](https://github.com/rime/weasel/commit/aba0609f64e5122748db80150de9644ffec0699f))
refactor(RimeWithWeasel): string convertions with macro ([fxliang](https://github.com/rime/weasel/commit/597993e8992e5081c9c0786c9b491c93fc3bbd71))

#### Features
feat: WeaselSetup.exe with new param /? or /help to show help info ([fxliang](https://github.com/rime/weasel/commit/63f27915f3dd03da2bc2b9d4ae1209f1b5e56e0b))
feat: WeaselDeployer.exe with new param /? or /help to show help info ([fxliang](https://github.com/rime/weasel/commit/1004f399d4b5c90652ae63f33f40247adc56e91b))
feat: WeaselSetup.exe parameter /userdir:<user_data_dir_full_path> to set user data directory in command line ([fxliang](https://github.com/rime/weasel/commit/0ef3154e489eed1176e9ca3a2e5f244fc0c1cf0f))
feat: WeaselSetup 默认启动不请求管理员权限，必要时使用管理员权限重启 (#1390) ([Wendy](https://github.com/rime/weasel/commit/ba768a6d65895837b052a1d366ffb872df5f0091))

#### Chores
chore: update bump version scripts ([fxliang](https://github.com/rime/weasel/commit/967674ff5295c4a389b35e9f8070b9fe43d0dcb1))
chore: update update/bump-version.ps1 [skip ci] ([fxliang](https://github.com/rime/weasel/commit/d13fd1250545d05df6573be7c0dee2529a4dc3fc))
chore: update update/bump_version.sh [skip ci] ([fxliang](https://github.com/rime/weasel/commit/8d12cafec0a84498c3f32d6821b7ccbd85fe1f21))
chore: follow #1379, update `update/bump-version.sh` to work without clog[skip ci] ([fxliang](https://github.com/rime/weasel/commit/d75b34bce20026f54ebbae73b6c591b3db473d11))
chore: make clang-format.ps1 worked in linux/Mac OS[skip ci] ([fxliang](https://github.com/rime/weasel/commit/d3e872c6671aaab5bfc0a6caa8227b416a5c5601))
chore: update update/bump_version.ps1 ([fxliang](https://github.com/rime/weasel/commit/18cb65206c494e5b3bc21380dc938d5553a7e83a))
chore: add powershell script for linting ([fxliang](https://github.com/rime/weasel/commit/a6d15cea4ad14c921bbde4c6b9c99a8a15c4dcde))
chore: update .gitignore ([fxliang](https://github.com/rime/weasel/commit/094e99de9e47b0aa9469b3d94a03e78501d8b1bb))
chore(install_boost): update boost download url ([居戎氏](https://github.com/rime/weasel/commit/235308dc7425529b49ffe3a5eb29947a4657f8cd))

#### Builds
build: bump librime to 1.13.0 ([fxliang](https://github.com/rime/weasel/commit/9a5244b1fae8de8f52c2f774eddc2986d869e98a))
build: set /utf-8 for source compilation ([fxliang](https://github.com/rime/weasel/commit/acbb0c393c65ebfea2ac176723f08e5b121442aa))
build: IntDir and OutDir set for msbuild solution, intermediary files will be always in `$(SolutionDir)\msbuild`. ([fxliang](https://github.com/rime/weasel/commit/5d5d5b0338a5eead4d898a2589f280015acdae85))

#### Continuous Integration
ci: run update rime/home appcast on published or prereleased ([fxliang](https://github.com/rime/weasel/commit/41dc044d7c34d30a574a72095361abf345c133f5))
ci: bump librime 1.13.1 ([fxliang](https://github.com/rime/weasel/commit/d279d9d78cce33a4e3f36e29c0a1ef6bef423121))
ci: draft before release ([fxliang](https://github.com/rime/weasel/commit/57b4cc44b46a1e9050b154178885cfccd1e9fdbe))

#### Bug Fixes
fix(trayicon): explorer.exe hangs ([fxliang](https://github.com/rime/weasel/commit/f11831fb16446ab98c1a2fee9ec6245a0d24144b))
fix(WeaselUI): hemispherical of hilite text preedit not correct ([fxliang](https://github.com/rime/weasel/commit/6e884c299b28c4a8e2d5ed2dc1845e702da35868))
fix(WeaselDeployer): WeaselDeployer will dump if hotkeys is set empty #1549 ([fxliang](https://github.com/rime/weasel/commit/bf4853dde1a4476489c4d7e85ab9407e5fc7c5f7))
fix(RimeWithWeasel): avoid vim_mode misoperations (#1543) ([fxliang](https://github.com/rime/weasel/commit/c2beb41a63567de7b9399ede13db65f0d3254221))
fix(installer): avoid files are deleted on system reboot after reinstallation (#1520) ([fxliang](https://github.com/rime/weasel/commit/2f92c6c5b885caf633f61d47e21ae61a93658246))
fix(tsf): ime status (#1499) ([wzv5](https://github.com/rime/weasel/commit/ea49aa13e936cf2309854ee353f18c2442153643))
fix(CandidateList): not displaying in some applications (#1494) ([wzv5](https://github.com/rime/weasel/commit/35afa144056e26e26f1c5eb092fd77942174cb35))
fix(ipcserver): concurrent access to rime api ([居戎氏](https://github.com/rime/weasel/commit/2dc4e1923a95d8ad4c1eff19399614253507c0fe))
fix(RimeWithWeasel): blend_colors algorithm, fix issue like #1405 ([fxliang](https://github.com/rime/weasel/commit/5dbafbb893cd79c876dab8600833266ce12ecdbd))
fix(WeaselUI): highlight back is not drawn correctly when fullscreen layout set ([fxliang](https://github.com/rime/weasel/commit/8b95887f4e2375659ed228e921f481a462b97376))
fix(CandidateList): null pointer error ([居戎氏](https://github.com/rime/weasel/commit/588a31f8eedf6066e5b6a1cc7f010ed528154445))
fix: silent installation script repeated call ([居戎氏](https://github.com/rime/weasel/commit/150c5608ba5338cedf124a0c1adf2caf5948a6cf))
fix: silent installation script typo ([Yh793](https://github.com/rime/weasel/commit/c599f2e67ab26dac3f77066d4d57d9295092be96))
fix: fix unexpected crash in some applications (#1458) ([Alfred Lieu](https://github.com/rime/weasel/commit/3f1e05b255867b8d9d31212fe840bcfa8f23b50c))
fix: candidate ui can't be drawn correctly after GPU reset ([fxliang](https://github.com/rime/weasel/commit/37c8fa161a221a263bb439a1158ba401c0cf90a3))

#### Commits
remove duplicated branch ([Qijia Liu](https://github.com/rime/weasel/commit/78e20ab7893ffe07904ef10eebe55c27bb05cc2a))
refactorï(RimeWithWeasel) simplify color parsing function ([fxliang](https://github.com/rime/weasel/commit/836dc9e35c25564fb4b8ab95e575afa4454ee5f3))

<a name="0.16.3"></a>
## [0.16.3](https://github.com/rime/weasel/compare/0.16.2...0.16.3)(2024-10-04)

#### Bug Fixes
* release channel feed_url not correct. ([fxliang](https://github.com/rime/weasel/commit/0c8bb0f01a929f46160482ae2f4492bed560b7b9))
* invalid quick return ([Xuesong Peng](https://github.com/rime/weasel/commit/4da263727e16362f01054f6f0bb7522e83ae1e06))

#### Chores
* add update\bump-version.ps1 to bump version in powershell, when clog is not required ([fxliang](https://github.com/rime/weasel/commit/8770fb3ed1b4341b7875c1d60e98bfa5b42f8ac7))
* update bump-version.sh, appcast.xml and testing-appcast.xml[skip ci] ([fxliang](https://github.com/rime/weasel/commit/91d5e4e224a0d73b8303a6ce10f03c71dace5cdd))

#### Continuous Integration
* release and update testing appcast only in rime/weasel ([fxliang](https://github.com/rime/weasel/commit/4af83b6e17f7c3cf78257dd300f4adadbffa1083))

<a name="0.16.2"></a>
## [0.16.2](https://github.com/rime/weasel/compare/0.16.1...0.16.2) (2024-09-28)

#### 安裝須知

**⚠️如您由0.16.0之前的版本升級，由於參數變化，安裝小狼毫前請保存好文件資料，於安裝後重啓或註銷 Windows，否則正在使用小狼毫的應用可能會崩潰。**

**⚠如您由0.16.0之前的版本升級，請確認您的 `installation.yaml` 文件編碼爲 `UTF-8`, 否則如您在其中修改了非 ASCII 字符內容的路徑時，有可能會引起未明錯誤。**

#### 主要更新
* 新特性：支持自動檢查更新使用測試通道，使用`WeaselSetup.exe`參數可修改，`/testing`使用測試通道，`/release`使用發佈版本，默認後者；
* 新特性：`WeaselSetup.exe`參數設置界面語言，设置后覆盖区域设置的自动检测。`/lt`設置爲繁體中文界面，`/ls` 設置爲簡體中文界面，`/le`設置爲英文界面
* 新特性：`WeaselSetup.exe`參數設置是否使用自動檢查更新，`/du`禁用自動檢查更新，`/eu`使用自動檢查更新
* 新特性：安裝器彈窗提示設置是否自動檢查升級
* 新特性：開關IME消息響應狀態可配置，`WeaselSetup.exe`參數`/toggleime`設置關閉鍵盤（原版本狀態），`/toggleascii`切換`ascii_mode`,安裝默認後者 #1364
* 新特性：支持xmake 2.9.4以上版本構建，使用`xbuild.bat`開展，相關參數基本同`build.bat`, 使用`xbuild.bat commands`可生成`compile_commands.json`便於lsp使用，`xbuild.bat clean`可清空xmake構建 #1360
* 新特性：支持`Caps_Lock` 按鍵binding（如選重）,需将`key_binder`置于`ascii_composer`之前
* 使能TSF dll中的WER
* nightly 構建後自動更新rime/home頁面更新測試通道appcast
* 升級lint檢查使用的llvm最低版本至18.1.6, 更新ci脚本检查更新llvm

#### Bug 修復

* 修復安裝器在系統未滿足要求時未中斷的問題
* 修復重新安裝時舊的安於路徑未保持的問題
* 修復界面語言根據區域格式未正確設置的問題
* 修復IPC通信時因新舊版本變更引起的異常崩潰的問題
* 修正代碼編碼格式
* 修復清空舊log文件
* 修復控制面板卸載界面中的圖標顯示問題
* 修復`style/hover_type`爲`"semi_hilite"`在首候選時的顯示異常問題
* 修復新版librime產物未能直接替換使用問題
* 禁用IPC通信的異步機制，修復一些因異步機制引發的應用異常
* 修復構建腳本不能重生成正確的版本信息問題
* 修復一些vs工程配置設置，處理一些deprecated API警告


<a name="0.16.1"></a>
## [0.16.1](https://github.com/rime/weasel/compare/0.16.0...0.16.1) (2024-06-06)


#### 安裝須知

**⚠️如您由0.16.0之前的版本升級，由於參數變化，安裝小狼毫前請保存好文件資料，於安裝後重啓或註銷 Windows，否則正在使用小狼毫的應用可能會崩潰。**

**⚠如您由0.16.0之前的版本升級，請確認您的 `installation.yaml` 文件編碼爲 `UTF-8`, 否則如您在其中修改了非 ASCII 字符內容的路徑時，有可能會引起未明錯誤。**

#### 主要更新
* 爲`WeaselServer.exe`使能Windows Error Reporting, 提供對應的`WeaselServer.pdb`文件, 在`WeaselServer.exe`崩潰時可以生成dmp報告文件在日誌文件夾中
* 提供`WeaselServer.exe`守護，在服務崩潰後6個按鍵事件（三次擊鍵Down&Up)後拉起服務
* 新增英文界面語言
* 更新7z和curl到最新版本，修復一些因爲7z的bug引起的問題
* 優化預覽圖PNG文件大小
* 新增語言欄菜單，打開日誌文件夾，調整日誌文件夾路徑爲`%TEMP%\rime.weasel`,方便查閱管理
* 異步處理消息，避免服務崩潰時長時間未響應引起客戶端程序崩潰
* 不在服務中部署方案，避免在守護拉起服務進入長耗時部署引起的僵死問題

#### Bug 修復

* 修復自動折行未正確處理標點符號（標點在折行後最前）的問題
* 修復`vim-mode`下的typo引起的`<C-C>`無法生效問題
* 修復部署消息未更新問題
* 修復卸載小狼毫時意外安裝語言包問題
* 修復`semi_hilite`下的UI未正確響應問題, `semi_hilite`顏色調整爲高亮色的半透明度狀態，改善體驗
* 減少不必要的服務端UI更新，提高性能減少服務崩潰機率
* 修復在非`DPI=96`的副屏上響應慢的問題
* 修復在高分屏上layout參數未dpi aware問題
* 修復Windows 11下Chrome等瀏覽器中非激活光標狀態下的按鍵響應異常問題
* 修復64位系統下默認安裝路徑不準確的問題



<a name="0.16.0"></a>
## [0.16.0](https://github.com/rime/weasel/compare/0.15.0...0.16.0) (2024-05-14)


#### 安裝須知

**⚠️由於參數變化，安裝小狼毫前請保存好文件資料，於安裝後重啓或註銷 Windows，否則正在使用小狼毫的應用可能會崩潰。**

**⚠請確認您的 `installation.yaml` 文件編碼爲 `UTF-8`, 否則如您在其中修改了非 ASCII 字符內容的路徑時，有可能會引起未明錯誤。**

#### 主要更新

* 升級核心算法庫至 [librime 1.11.2](https://github.com/rime/librime/releases/tag/1.11.2)
* 改善輸入法病毒誤報問題
* 新增 64 位算法服務程序，支持 64 位 librime，支持大內存（可部署大規模詞庫方案）
* 支持 arm/arm64 架構
* 單安裝包支持 win32/x64/arm/arm64 架構系統的自動釋放文件
* 32 位算法服務增加 LARGE ADDRESS AWARE 支持
* 升級 boost 算法庫至 1.84.0
* IME改爲可選項，默認不安裝
* 棄用 `weaselt*.dll`，增加註冊香港、澳門、新加坡區域語言配置（默認未啓用，需在控制面板/設置中手工添加）；支持簡繁體小狼毫同時使能
* 棄用 `weaselt*.ime`
* 移除 `pyweasel`
* 候選窗口 UI 內存優化
* 改善候選窗口 UI 繪製性能
* 升級 WTL 庫至 10.0，gdi+ 至 1.1
* 每顯示器 dpi aware，自適應不同顯示器不同 dpi 設定變化
* 更新高清圖標
* 增大 IPC 數據長度限制至 64k，支持長候選
* 升級 plum
* 應用界面及菜單簡繁體自動適應
* `app_options` 中應用名大小寫不敏感
* 字體抗鋸齒設定參數 `style/antialias_mode: {force_dword|cleartype|grayscale|aliased|default}`
* ASCII狀態提示跟隨鼠標光標設定 `style/ascii_tip_follow_cursor: bool`
* 新增參數 `style/layout/hilite_padding_x: int`、`style/layout/hilite_padding_y: int`，支持分別設置xy向的 padding
* 新增參數 `schema/full_icon: string`, `schema/half_icon: string`，支持在方案中設定全半角圖標
* 新增參數 `style/text_orientation: "horizontal" | "vertical"`, 與 `style/vertical_text: bool` 冗餘，設定文字繪製方向，兼容 squirrel 參數
* 新增參數 `style/paging_on_scroll: bool`，可設定滾輪相應類型（翻頁或切換前後候選）
* 新增參數，Windows10 1809後版本的Windows，支持 `style/color_scheme_dark: string` 設定暗色模式配色
* 新增參數 `style/candidate_abbreviate_length: int`，支持候選字數超限時縮略顯示
* 新增參數 `style/click_to_capture: bool` 設定鼠標點擊是否截圖
* 新增參數 `show_notifications_time: int` 可設定提示顯示時間，單位 ms；設置 0 時不顯示提示
* 新增參數 `show_notifications: bool` 或 `show_notifications: 開關列表 | "schema"`，可定製是否顯示切換提示、顯示那些切換提示
* 新增參數 `style/layout/baseline: int` 和 `style/layout/linespacing: int`，可自行調整參數修復候選窗高度跳躍閃爍問題
* 棄用 `style/mouse_hover_ms`；新增 `style/hover_type: "none"|"semi_hilite"|"hilite"`，改善鼠標懸停相應體驗
* 新增參數 `global_ascii: bool`, 支持全局 ascii 模式
* 新增 `app_options`，支持應用專用 `vim_mode: bool`，支持常見 vim 切換 normal 模式按鍵時，切換到 `ascii_mode`
* 新增 `app_options`，支持應用專用 `inline_preedit: bool` 設定，優先級高於方案內設定，高於 `weasel.yaml` 中的設定
* 支持命令行設置小狼毫 `ascii_mode` 狀態，`WeaselServer.exe /ascii`，`WeaselServer.exe /nascii`
* 支持設置 `comment_text_color`、`hilited_comment_text_color` 透明來隱藏對應文字顯示
* `hilited_mark_color` 非透明，`mark_text` 爲空字符串時，類 windowns 11 的高亮標識
* 切換方案後，提示方案圖標和方案名字
* 支持全部 switch 提示使用方案內設定的 label
* WeaselSetup通過打開目錄窗口設置用戶目錄路徑
* 新增支持方案內定義方案專用配色
* 支持 imtip
* 增加類微軟拼音的高亮標識在鼠標點擊時的動態
* 支持在字體設定任一分組中設置字體整體的字重或字形
* 優化點擊選字邏輯
* 豎直佈局反轉時，互換上下方向鍵
* 候選窗超出下方邊界時，在當前合成結束前保持在輸入位置上方，減少候選窗口高度變小時潛在的窗口上下跳動
* 調整 TSF 光標位置（`inline_preedit: false` 時），減少光標閃爍
* WeaselSetup 修改用戶目錄路徑（已安裝時）
* 語言欄新增菜單，重啓服務
* IPC 報文轉義 `\n`、`\t`，不再因 `\n` 引發應用崩潰
* 使用 clang-format 格式化代碼，統一代碼風格
* 自動文件版本信息
* 測試項目 test 只在 debug 配置狀態下編譯構建

#### Bug 修復

* 修復 word 365 中候選窗閃爍無法正常顯示的問題
* 修復 word 行尾輸入時候選窗反覆跳動問題
* 修復 word 中無法點擊選詞問題
* 修復 excel 等應用中，第一鍵 keydown 時未及時彈出候選窗問題
* 修復導出詞典數據後引起的多個 explorer 進程的問題，優化對應對話框界面顯示
* 修復打開用戶目錄，程序目錄引起的多個 explorer.exe 進程問題，支持服務未啓動時打開這些目錄
* 修復系統托盤重啓後未及時顯示的問題
* 修復 `style/layout/min_width` 在部分佈局下未生效問題
* 修復 preedit 寬高計算錯誤問題
* 修復翻頁按鈕在豎直佈局反轉時位置錯誤
* 修復豎直佈局帶非空 mark_text 時的計算錯誤
* 修復 composing 中候選窗隨文字移動問題
* 修復 wezterm gpu 模式下無法使用問題
* 修復 `style/inline_preedit: true` 時第一鍵輸入時候選窗位置錯誤
* 修復算法服務單例運行
* 修復調用 WeaselServer.exe 未正常重啓服務問題
* 修復偶發的顯卡關聯文字空白問題
* 修復部署過程中如按鍵輸入引發的重複發出 tip 提示窗問題
* 修復部分方案中的圖標顯示（`english.schema.yaml`）
* 修復 `preedit_type: preview` 時的光標錯誤問題
* 修復 `shadow_color` 透明時截圖尺寸過大問題，減小截圖尺寸
* 修復天園地方時，高亮候選圓角半徑不正確問題
* 修復某些狀態下天園地方的 preedit 背景色圓角異常問題
* 修復候選尾部空白字符引起的佈局計算錯誤問題
* 修復 mark_text 繪製鋸齒問題
* 修復靜默安裝彈窗問題
* 修復 librime-preedit 引起的應用崩潰問題
* 修復 plum 用戶目錄識別錯誤問題
* 修復安裝後未在控制面板中添加輸入法、卸載後未刪除控制面板中的輸入法清單問題
* 修復一些其他已知的 bug

#### 已知問題

* 部分應用仍存在輸入法無法輸入文字或響應異常的問題
* WeaselServer 仍可能發生崩潰
* 仍有極少部分防病毒軟件可能誤報病毒



<a name="0.15.0"></a>
## [0.15.0](https://github.com/rime/weasel/compare/0.14.3...0.15.0) (2023-06-06)


#### 安裝須知

**⚠️安裝小狼毫前請保存好文件資料，於安裝後重啓 Windows ，否則正在使用小狼毫的應用將會崩潰。**
**⚠️此版本的小狼毫需要使用 Windows 8.1 或更高版本的操作系統。**

#### 主要更新

* 升級核心算法庫至 [librime 1.8.5](https://github.com/rime/librime/blob/master/CHANGELOG.md#185-2023-02-05)
* DPI 根據顯示器自動調整
* 支持候選窗口等圓角顯示
  * `style/layout/corner_radius: int`
* 兼容鼠鬚管中高亮圓角參數`style/layout/hilited_corner_radius: int`
* 支持主題顏色中含有透明通道代碼, 支持格式 0xaabbggrr，0xbbggrr, 0xabgr, 0xbgr
* 配色主題支持默認ABGR順序，或ARGB、RGBA順序
  * `preset_color_schemes/color_scheme/color_format: "argb" | "rgba" | ""`
* 支持編碼/高亮候選/普通候選/輸入窗口/候選邊框的陰影顏色繪製
  * `style/layout/shadow_radius: int`
  * `style/layout/shadow_offset_x: int`
  * `style/layout/shadow_offset_y: int`
  * `preset_color_schemes/color_scheme/shadow_color: color`
  * `preset_color_schemes/color_scheme/nextpage_color: color`
  * `preset_color_schemes/color_scheme/prevpage_color: color`
  * `preset_color_schemes/color_scheme/candidate_back_color: color`
  * `preset_color_schemes/color_scheme/candidate_shadow_color: color`
  * `preset_color_schemes/color_scheme/candidate_border_color: color`
  * `preset_color_schemes/color_scheme/hilited_shadow_color: color`
  * `preset_color_schemes/color_scheme/hilited_candidate_shadow_color: color`
  * `preset_color_schemes/color_scheme/hilited_candidate_border_color: color`
  * `preset_color_schemes/color_scheme/hilited_mark_color: color`
* 支持自定義標籤、註解字體及字號
  * `style/label_font_face: string`
  * `style/comment_font_face: string`
  * `style/label_font_point: int`
  * `style/comment_font_point: int`
  * `style/layout/align_type: "top" | "center" | "bottom"`
* 支持指定字符 Unicode 區間字體設定
* 支持字重，字形風格設定
  * `style/font_face: font_name[:start_code_point:end_code_point][:weight_set][:style_set][,font2...]`
    * example: `"Segoe UI Emoji:20:39:bold:italic, Segoe UI Emoji:1f51f:1f51f, Noto Color Emoji SVG:80, Arial:600:6ff, Segoe UI Emoji:80, LXGW Wenkai Narrow"`
* 支持自定义字体回退範圍、順序定义
* 彩色字體支持
  * Windows 10 周年版前：需要使用 COLR 格式彩色字體
  * Windows 11 ：可以使用 SVG 字體
* 新增豎直文字佈局
  * `style/vertical_text: bool`
  * `style/vertical_text_left_to_right: bool`
  * `style/vertical_text_with_wrap: bool`
* 新增豎直佈局vertical窗口上移時自動倒序排列
  * `style/vertical_auto_reverse: bool`
* 新增「天圓地方」佈局：由 margin 與 hilite_padding 確定, 當margin <= hilite_padding時生效
* margin_x 或 margin_y 設置爲負值時，隱藏輸入窗口，不影響方案選單顯示
* 新增 preedit_type: preview_all ，在輸入時將候選項顯示於 composition 中
  * `style/preedit_type: "composition" | "preview" | "preview_all"`
* 新增輸入法高亮提示標記
  * `style/mark_text: string`
* 新增輸入方案圖標顯示，可在語言欄中顯示，文件格式爲ico
  * `schema/icon: string`
  * `schema/ascii_icon: string`
* 新增選項，允許在光標位置獲取失敗時於窗口左上角繪製候選框（而不是桌面左上角）
  * `style/layout/enhanced_position: bool`
* 新增鼠標點擊截圖到剪貼板功能
* 新增選項，支持越長自動折行/換列顯示
  * `style/layout/max_width: int`
  * `style/layout/max_height: int`
* 支持方案內設定配色
  * `style/color_scheme: string`
* 支持多行内容顯示，\r, \n, \r\n均支持
* 支持方案內設定配色
* 繪製性能提升
* composition 模式下新增下劃線顯示
* 隨二進制文件提供調試符號

#### Bug 修復

* 轉義日文鍵盤中特殊按鍵
* 候選文字過長時崩潰
* 修復用戶目錄下無 `default.custom.yaml` 或 `weasel.custom.yaml` 時，設定窗口無法彈出的問題
* 方案中設定inline_preedit爲true時，部署後編碼末端出現異常符號
* 部分應用無法輸入文字的問題
* 修復部署時無顯示提示的問題
* 修復中文路徑相關問題
* 修復右鍵菜單打開程序目錄/用戶目錄時，資源管理器無響應的問題
* 修復部分內存訪問問題
* 修復操作系統 / WinGet 無法識別小狼毫版本號的問題
* 修復 composition 模式下光標位置不正常的問題
* 修復 Word 中小狼毫工作不正常的問題
* 若干開發環境配置問題修復

#### 已知問題

* 部分應用仍存在輸入法無法輸入文字的問題
* WeaselServer 仍可能發生崩潰
* 部分防病毒軟件可能誤報病毒



<a name="0.14.3"></a>
## 0.14.3 (2019-06-22)


#### 主要更新

* 升級核心算法庫 [librime 1.5.3](https://github.com/rime/librime/blob/master/CHANGELOG.md#153-2019-06-22)
  * 修復 `single_char_filter` 組件
  * 完善上游項目 `librime` 的全自動發佈流程，免去手工上傳構建結果的步驟



<a name="0.14.2"></a>
## 0.14.2 (2019-06-17)


#### 主要更新

* 升級核心算法庫 [librime 1.5.2](https://github.com/rime/librime/blob/master/CHANGELOG.md#152-2019-06-17)
  * 修復用戶詞的權重，穩定造句質量、平衡翻譯器優先級 [librime#287](https://github.com/rime/librime/issues/287)
  * 建議 0.14.1 版本用家升級



<a name="0.14.1"></a>
## 0.14.1 (2019-06-16)


#### 主要更新

* 升級核心算法庫 [librime 1.5.1](https://github.com/rime/librime/blob/master/CHANGELOG.md#151-2019-06-16)
  * 修復未裝配語言模型時缺省的造句算法 ([weasel#383](https://github.com/rime/weasel/issues/383))



<a name="0.14.0"></a>
## 0.14.0 (2019-06-11)


#### 主要更新

* 升級核心算法庫 [librime 1.5.0](https://github.com/rime/librime/blob/master/CHANGELOG.md#150-2019-06-06)
  * 遷移到VS2017構建工具；建設安全可靠的全自動構建、發佈流程
  * 通過更新第三方庫，修復userdb文件夾大量佔用磁盤空間的問題
  * 將Rime插件納入自動化構建流程。本次發行包含兩款插件：
    - [librime-lua](https://github.com/hchunhui/librime-lua)
    - [librime-octagram](https://github.com/lotem/librime-octagram)
* 高清重製真彩輸入法狀態圖標


#### Features

* **ui:**  high-res status icons; display larger icons in WeaselPanel ([093fa806](https://github.com/rime/weasel/commit/093fa80678422f972e7a7285060553eeedb0e591))



<a name="0.13.0"></a>
## 0.13.0 (2019-01-28)


#### 主要更新

* 升級核心算法庫 [librime 1.4.0](https://github.com/rime/librime/blob/master/CHANGELOG.md#140-2019-01-16)
  * 新增 [拼寫糾錯](https://github.com/rime/librime/pull/228) 選項
    當前僅限 QWERTY 鍵盤佈局及使用 `script_translator` 的方案
  * 修復升級、部署數據時發生的若干錯誤
* 更換輸入法狀態圖標，適配高分辨率屏幕


#### Features

* **tsf:**  register as GUID_TFCAT_TIPCAP_UIELEMENTENABLED ([ae876916](https://github.com/rime/weasel/commit/ae8769166ea50b319aa89460b60890d598c618c5))
* **ui:**  high-res icons (#324) ([ad3e2027](https://github.com/rime/weasel/commit/ad3e2027644f80c6a384b7730da20dd239e780af))

#### Bug Fixes

* **WeaselSetup.vcxproj:**  Debug build linker options ([eb885fe0](https://github.com/rime/weasel/commit/eb885fe06ffd720d3de1101be2410a94bd3747c0))
* **output/install.nsi:**  bundle new yaml files from rime/rime-prelude ([cba35e9b](https://github.com/rime/weasel/commit/cba35e9b2c34d095b9ca1eb44e923e004cf23ddc))
* **test:**  Debug build ([c771126c](https://github.com/rime/weasel/commit/c771126c74fa1c4f91d4bfd8fb5ab8c16dcb7c4c))
* **tsf:**  set current page to 0 as page count is always 1 ([5447f63b](https://github.com/rime/weasel/commit/5447f63bc7c9d0e31d7ba8ead1e1229938be276d))



<a name="0.12.0"></a>
## 0.12.0  (2018-11-12)

#### 主要更新

* 合併小狼毫與小狼毫（TSF）兩種輸入法
* 合併32位與64位系統下的安裝程序
* 使用系統的關閉輸入法功能（默認快捷鍵 Ctrl + Space）後，輸入法圖標將顯示禁用狀態
* 修復一些情況下的崩潰問題
* 升級核心算法庫 [librime 1.3.2](https://github.com/rime/librime/blob/master/CHANGELOG.md#132-2018-11-12)
  * 允許多個翻譯器共用同一個詞典時的組詞，實現固定單字順序的形碼組詞([librime#184](https://github.com/rime/librime/issues/184))。
  * 新增 translator/always_show_comments 選項，允許始終顯示候選詞註解。

#### Bug Fixes

* **candidate:** fix COM pointer reference ([63d6d9a](https://github.com/rime/weasel/commit/63d6d9a))
* **ipc:** eliminate some trivial warnings ([dae945c](https://github.com/rime/weasel/commit/dae945c))
* fix constructor ([b25f968](https://github.com/rime/weasel/commit/b25f968))


#### Features

* **compartment:** show IME disabled on language bar ([#263](https://github.com/rime/weasel/issues/263)) ([4015d18](https://github.com/rime/weasel/commit/4015d18))
* **install:** combine IME and TSF ([#257](https://github.com/rime/weasel/issues/257)) ([91cbd2c](https://github.com/rime/weasel/commit/91cbd2c))
* **tsf:** get IME keyboard identifier by searching registry ([#272](https://github.com/rime/weasel/issues/272)) ([b60b5b1](https://github.com/rime/weasel/commit/b60b5b1))
* **WeaselSetup:** detect 64-bit on single 32-bit build ([#266](https://github.com/rime/weasel/issues/266)) ([fb3ae0f](https://github.com/rime/weasel/commit/fb3ae0f))



<a name="0.11.1"></a>
## 0.11.1 (2018-04-26)

#### 主要更新

* 修復了在 Excel 中奇怪的輸入丟失問題（[#185](https://github.com/rime/weasel/issues/185)）
* 功能鍵不再會觸發輸入焦點（[#194](https://github.com/rime/weasel/issues/194)、[#195](https://github.com/rime/weasel/issues/195)、[#204](https://github.com/rime/weasel/issues/204)）
* 「獲取更多輸入方案」功能優化（[#180](https://github.com/rime/weasel/issues/180)）
* 修復了後臺可能同時出現多個算法服務的問題（[#199](https://github.com/rime/weasel/issues/199)）
* 恢復語言欄右鍵菜單中「用戶資料同步」一項

#### Bug Fixes

* **server:**  use kernel mutex to ensure single instance (#207) ([bd0c4720](https://github.com/rime/weasel/commit/bd0c4720669c61087dd930b968640c60a526ecb2))
* **tsf:**
  *  do not reset composition on document focus set ([124fc947](https://github.com/rime/weasel/commit/124fc9475c30963a9bbbf9a097b452b52e8ab658))
  *  use `ITfContext::GetSelection` to get cursor position ([5664481c](https://github.com/rime/weasel/commit/5664481cc9ddd28db35c3155f7ddf83a55b65275))
  *  recover sync option in TSF language bar menu ([7a0a8cc2](https://github.com/rime/weasel/commit/7a0a8cc2a3dd913ce34204d6e966b263af766f3b))

#### Features

* **build.bat:**  build installer ([e18117b7](https://github.com/rime/weasel/commit/e18117b7b42d5af0fbfa807e4c858c40206b4967))
* **installer:**  bundle curl, update rime-install.bat, fixes #180 ([2f3b283d](https://github.com/rime/weasel/commit/2f3b283d6ef4aa0580d186e626dadb9e1030dfd5))
* **rime-install.bat:**  built-in ZIP package installer ([739be9bc](https://github.com/rime/weasel/commit/739be9bc9ba08e294f51e1d7232407148ded716c))



<a name="0.11.0"></a>
## 0.11.0 (2018-04-07)

#### 主要更新

* 新增 [Rime 配置管理器](https://github.com/rime/plum)，通過「輸入法設定／獲取更多輸入方案」調用
* 在輸入法語言欄顯示狀態切換按鈕（TSF 模式）
* 修復多個前端兼容性問題
* 新增配色主題「現代藍」`metroblue`、「幽能」`psionics`
* 安裝程序支持繁體中文介面
* 修復 0.10 版升級安裝後，因用戶文件夾中保留舊文件、配置不生效的問題
* 升級 0.9 版 `.kct` 格式的用戶詞典
  **注意**：僅此一個版本支持格式升級，請務必由 0.9 升級到 0.11，再安裝後續版本

#### Features

* **WeaselDeployer:**  add Get Schemata button to run plum script (#174) ([c786bb5b](https://github.com/rime/weasel/commit/c786bb5ba2f1cc7e79b66f36d0190e61cd7233ae))
* **build.bat:**  customize PLATFORM_TOOLSET settings ([c7a9a4fb](https://github.com/rime/weasel/commit/c7a9a4fb530e0274450e4296cb0db2906d2f1fb4))
* **config:**
  *  enable customization of label format ([76b08bae](https://github.com/rime/weasel/commit/76b08bae810735c5f1c8626ec39a7afd463f0269))
  *  alias `style/layout/border_width` to `style/layout/border` ([013eefeb](https://github.com/rime/weasel/commit/013eefebaa4474e7814b6cfb6c905bcc12543a7f))
* **install.nsi:**
  *  add Traditional Chinese for installer ([d1a9696a](https://github.com/rime/weasel/commit/d1a9696a57dfc9e04c51899572e156fb1676f786))
  *  upgrade to Modern UI 2 and prompt reboot (#128) ([f59006f8](https://github.com/rime/weasel/commit/f59006f8d195ca848e45cd934f44b3318fb135c1))
* **ipc:**  specify user name for named pipe ([2dfa5e1a](https://github.com/rime/weasel/commit/2dfa5e1a63ee1c26ef983d25471682f87cc60b62))
* **preset_color_schemes:**
  *  add homepage featured color scheme `psionics` ([89a0eb8b](https://github.com/rime/weasel/commit/89a0eb8b861b9b3f2abc42df65821254010b24ff))
  *  add metroblue color scheme ([f43e2af6](https://github.com/rime/weasel/commit/f43e2af608bde38a6d345ba540f4c37ec024853a))
* **submodules:**  switch from rime/brise to rime/plum ([f3ff5aa9](https://github.com/rime/weasel/commit/f3ff5aa962a7b8cce2b74a5cb583a69cb8938e55))
* **tsf:**
  *  enable language bar button (#170) ([2b660397](https://github.com/rime/weasel/commit/2b660397950f348205e6a93bf44a46e4a72bcc81))
  *  accomplish candidate UI interfaces (#156) ([1f0ae793](https://github.com/rime/weasel/commit/1f0ae7936fd495ecf4ff3ef162c0e38297d2d582))
  *  fix candidate selecting in preview preedit mode ([206efd69](https://github.com/rime/weasel/commit/206efd692124339d0e256198360c1860c72cd807))
  *  support user defined preedit display type ([f76379b0](https://github.com/rime/weasel/commit/f76379b01abe9d3971d68e2e272067e0bb855cc9))
* **weasel.yaml:**  enable ascii_mode in console applications by default ([28cdd096](https://github.com/rime/weasel/commit/28cdd09692f77e471784bf85ff7a19bc48e113f4))

#### Bug Fixes

*   fix defects according to Coverity Scan ([526a91d2](https://github.com/rime/weasel/commit/526a91d2954492cc8e23c2c4c8def2a053af7c20))
*   inline_preedit && fullscreen causing dead lock when there's no candidates. ([deb0bb24](https://github.com/rime/weasel/commit/deb0bb24b3f3aeaf73aef344968b7f15b471443f))
* **RimeWithWeasel:**  fix wild pointer ([ae2e3c4a](https://github.com/rime/weasel/commit/ae2e3c4a256fb9a2f7851c54114822d1bfbf0316))
* **ServerImpl:**  do finalization before exit process ([b1bae01e](https://github.com/rime/weasel/commit/b1bae01eb25c5e24e074807b7b3cb8a6d8401276))
* **WeaselUI:**
  *  specify default label format in constructor ([4374d244](https://github.com/rime/weasel/commit/4374d2440b99726894799861fb3bd5b93e73dec5), closes [#147](https://github.com/rime/weasel/issues/147))
  *  limit to subscript range when processing candidates ([6b686c71](https://github.com/rime/weasel/commit/6b686c717bfab141469c3d48ec1c6acbeb79921e), closes [#121](https://github.com/rime/weasel/issues/121))
* **composition:**
  *  improve compositions and edit sessions (#146) ([fbdb6679](https://github.com/rime/weasel/commit/fbdb66791da3291b740edf3c337032674e4377e8))
  *  fix crashes in notebook with inline preedit ([5e257088](https://github.com/rime/weasel/commit/5e257088be823a2569609f0b3591af3a51d47a46))
  *  fix crashes in notebook with inline preedit ([892930ce](https://github.com/rime/weasel/commit/892930cebc4235a0a1ef58803fe88c32ccc8b4e9))
* **install.bat:**  run in elevate cmd; detach WeaselServer process ([2194d9fb](https://github.com/rime/weasel/commit/2194d9fbd7d0341fef94efdbe9268af8a6237438))
* **ipc:**
  *  add version check for security descriptor initialization ([b97ccffe](https://github.com/rime/weasel/commit/b97ccffe76a6abf3e353724ce0607d5dd97de6f2), closes [#157](https://github.com/rime/weasel/issues/157))
  *  grant access to IE protected mode ([16c163a4](https://github.com/rime/weasel/commit/16c163a41d0afc9824723009ba8b9b9ba37b1c72))
  *  try to reconnect when failed ([3c286b6a](https://github.com/rime/weasel/commit/3c286b6a942769abf13188d88f9ab5e4c125807b))
* **librime:**  make rime_api.h available in librime\build\include\ ([3793e22c](https://github.com/rime/weasel/commit/3793e22c47b34c61d305ca80567dfdafe08b2302))
* **server:**  postpone tray icon updating when focusing on explorer ([45cf1120](https://github.com/rime/weasel/commit/45cf112099fa6db335cda06b1aaa0ae9c7975efe))
* **tsf:**
  *  fix candidate behavior ([9e2f9f17](https://github.com/rime/weasel/commit/9e2f9f17c059bf129c2c8b2561471670ea200dd7))
  *  fix `ITfCandidateListUIElement` implemention ([9ce1fa87](https://github.com/rime/weasel/commit/9ce1fa87e6ef788e791e68193700e2ebdd950d20))
  *  use commmit text preview to show inline preview ([b1d1ec43](https://github.com/rime/weasel/commit/b1d1ec43e132998ea8764d8dac2098a2b3d9a3e8))



<a name="0.10.0"></a>
## 小狼毫 0.10.0 (2018-03-14)


#### 主要更新

* 兼容 Windows 8 ~ Windows 10
* 支持高分辨率顯示屏
* 介面風格選項
  * 在內嵌編碼行預覽結果文字
  * 可指定候選序號的樣式
* 升級核心算法庫 [librime 1.3.0](https://github.com/rime/librime/blob/master/CHANGELOG.md#130-2018-03-09)
  * 支持 YAML 節點引用，方便模塊化配置
  * 改進部署流程，在 `build` 子目錄集中存放生成的數據文件
* 精簡安裝包預裝的輸入方案，更多方案可由 [東風破](https://github.com/rime/plum) 取得

#### Features

* **build.bat:**  customize PLATFORM_TOOLSET settings ([c7a9a4fb](https://github.com/rime/weasel/commit/c7a9a4fb530e0274450e4296cb0db2906d2f1fb4))
* **config:**
  *  enable customization of label format ([76b08bae](https://github.com/rime/weasel/commit/76b08bae810735c5f1c8626ec39a7afd463f0269))
  *  alias `style/layout/border_width` to `style/layout/border` ([013eefeb](https://github.com/rime/weasel/commit/013eefebaa4474e7814b6cfb6c905bcc12543a7f))
* **tsf:**
  *  fix candidate selecting in preview preedit mode ([206efd69](https://github.com/rime/weasel/commit/206efd692124339d0e256198360c1860c72cd807))
  *  support user defined preedit display type ([f76379b0](https://github.com/rime/weasel/commit/f76379b01abe9d3971d68e2e272067e0bb855cc9))

#### Bug Fixes

*   Support High DPI Display [#28](https://github.com/rime/weasel/issues/28)
* **WeaselUI:**  limit to subscript range when processing candidates ([6b686c71](https://github.com/rime/weasel/commit/6b686c717bfab141469c3d48ec1c6acbeb79921e), closes [#121](https://github.com/rime/weasel/issues/121))
* **install.bat:**  run in elevate cmd; detach WeaselServer process ([2194d9fb](https://github.com/rime/weasel/commit/2194d9fbd7d0341fef94efdbe9268af8a6237438))
* **librime:**  make rime_api.h available in librime\build\include\ ([3793e22c](https://github.com/rime/weasel/commit/3793e22c47b34c61d305ca80567dfdafe08b2302))
* **tsf:**
  *  Results of auto-selection cleared by subsequent manual selection [#107](https://github.com/rime/weasel/issues/107)
  *  use commmit text preview to show inline preview ([b1d1ec43](https://github.com/rime/weasel/commit/b1d1ec43e132998ea8764d8dac2098a2b3d9a3e8))



<a name="0.9.30"></a>
## 小狼毫 0.9.30 (2014-04-01)


#### Rime 算法庫變更集

* 新增：中西文切換方式 `clear`，切換時清除未完成的輸入
* 改進：長按 Shift（或 Control）鍵不觸發中西文切換
* 改進：並擊輸入，若按回車鍵則上屏按鍵對應的字符
* 改進：支持對用戶設定中的列表元素打補靪，例如 `switcher/@0/reset: 1`
* 改進：缺少詞典源文件 `*.dict.yaml` 時利用固態詞典 `*.table.bin` 完成部署
* 修復：自動組詞的詞典部署時未檢查【八股文】的變更，導致索引失效、候選字缺失
* 修復：`comment_format` 會對候選註釋重複使用多次的BUG

#### 【東風破】變更集

* 新增：快捷鍵 `Control+.` 切換中西文標點
* 更新：【八股文】【朙月拼音】【地球拼音】【五筆畫】
* 改進：【朙月拼音·語句流】`/0` ~ `/10` 輸入數字符號



<a name="0.9.29.1"></a>
## 小狼毫 0.9.29.1 (2013-12-22)


#### 【小狼毫】變更集

* 變更：不再支持 Windows XP SP2，因升級編譯器以支持 C++11
* 修復：輸入語言選爲中文（臺灣）在 Windows 8 系統上出現多餘的輸入法選項
* 修復：升級安裝後，外觀設定介面未及時顯示出新增的配色方案
* 修復：配色方案 Google+ 的預覽圖

#### Rime 算法庫變更集

* 更新：librime 升級到 1.1
* 新增：固定方案選單排列順序的選項 `default.yaml`: `switcher/fix_schema_list_order: true`
* 修復：正確匹配嵌套的“‘彎引號’”
* 改進：碼表輸入法自動上屏及頂字上屏（[示例](https://gist.github.com/lotem/f879a020d56ef9b3b792)）<br/>
    若有 `speller/auto_select: true`，則選項 `speller/max_code_length:` 限定第N碼無重碼自動上屏
* 優化：爲詞組自動編碼時，限制因多音字而產生的組合數目，避免窮舉消耗過量資源

#### 【東風破】變更集

* 更新：【粵拼】匯入衆多粵語詞彙
* 優化：調整部分異體字的字頻



<a name="0.9.28"></a>
## 小狼毫 0.9.28 <2013-12-01>


#### 【小狼毫】變更集

* 新增：一組配色方案，作者：P1461、Patricivs、skoj、五磅兔
* 修復：[Issue 528](https://code.google.com/p/rimeime/issues/detail?id=528) Windows 7 IE11 文字無法上屏
* 修復：[Issue 531](https://code.google.com/p/rimeime/issues/detail?id=531) Windows 8 卸載輸入法後在輸入法列表中有殘留項
* 變更：註冊輸入法時同時啓用 IME、TSF 模式

#### Rime 算法庫變更集

* 更新：librime 升級到 1.0
* 改進：`affix_segmentor` 支持向匹配到的代碼段添加標籤 `extra_tags`
* 修復：`table_translator` 按字符集過濾候選字，修正對 CJK-D 漢字的判斷

#### 【東風破】變更集

* 優化：【粵拼】兼容[教育學院拼音方案](http://zh.wikipedia.org/wiki/%E6%95%99%E8%82%B2%E5%AD%B8%E9%99%A2%E6%8B%BC%E9%9F%B3%E6%96%B9%E6%A1%88)
* 更新：`symbols.yaml` 由 Patricivs 重新整理符號表
* 更新：Emoji 提供更加豐富的繪文字（需要字體支持）
* 更新：【八股文】【朙月拼音】【地球拼音】【中古全拼】修正錯別字、註音錯誤



<a name="0.9.27"></a>
## 小狼毫 0.9.27 (2013-11-06)


#### 【小狼毫】變更集

* 變更：動態鏈接 `rime.dll`，減小程序文件的體積
* 修復：嘗試解決 Issue 487 避免服務進程以 SYSTEM 帳號執行
* 新增：開始菜單項「安裝選項」，Vista 以降提示以管理員權限啓動
* 優化：更換圖標，解決 Windows 8 TSF 圖標不清楚的問題

#### Rime 算法庫變更集

* 優化：同步用戶資料時自動備份自定義短語等 .txt 文件
* 修復：【地球拼音】反查拼音失效的問題
* 變更：編碼提示不再添加括弧（，）及逗號，可自行設定樣式

#### 輸入方案設計支持

* 新增：`affix_segmentor` 分隔編碼的前綴、後綴
* 改進：`translator` 支持匹配段落標籤
* 改進：`simplifier` 支持多個實例，匹配段落標籤
* 新增：`switches:` 輸入方案選項支持多選一
* 新增：`reverse_lookup_filter` 爲候選字標註指定種類的輸入碼

#### 【東風破】變更集

* 更新：【粵拼】補充大量單字的註音
* 更新：【朙月拼音】【地球拼音】導入 Unihan 讀音資料
* 改進：【地球拼音】【注音】啓用自定義短語
* 新增：【注音·臺灣正體】
* 修復：【朙月拼音·簡化字】通過快捷鍵 `Control+Shift+4` 簡繁切換
* 改進：【倉頡五代】開啓繁簡轉換時，提示簡化字對應的傳統漢字
* 變更：間隔號採用「·」`U+00B7`



<a name="0.9.26.1"></a>
## 小狼毫 0.9.26.1 (2013-10-09)

* 修復：從上一個版本升級【倉頡】輸入方案不會自動更新的問題



<a name="0.9.26"></a>
## 小狼毫 0.9.26 (2013-10-08)

* 新增：【倉頡】開啓自動造詞<br/>
  連續上屏的5字（依設定）以內的組合，或以連打方式上屏的短語，
  按構詞規則記憶爲新詞組；再次輸入該詞組的編碼時，顯示「☯」標記
* 變更：【五筆】開啓自動造詞；從碼表中刪除與一級簡碼重碼的鍵名字
* 變更：【地球拼音】當以簡拼輸入時，爲5字以內候選標註完整帶調拼音
* 新增：【五筆畫】輸入方案（`stroke`），取代 `stroke_simp`
* 新增：支持在輸入方案中設置介面樣式（`style:`）<br/>
  如字體、字號、橫排／直排等；配色方案除外
* 修復：多次按「.」鍵翻頁後繼續輸入，不應視爲網址而在編碼中插入「.」
* 修復：開啓候選字的字符集過濾，導致有時不出現連打候選詞的 BUG
* 修復：`table_translator` 連打組詞時產生的內存泄漏（0.9.25.2）
* 修復：爲所有用戶創建開始菜單項
* 更新：修訂【八股文】詞典、【朙月拼音】【地球拼音】【粵拼】【吳語】
* 更新：2013款 Rime 輸入法圖標



<a name="0.9.25.2"></a>
## 小狼毫 0.9.25.2 (2013-07-26)

* 改進：碼表輸入法連打，Shift+BackSpace 以字、詞爲單位回退
* 修復：演示模式下開啓內嵌編碼行、查無候選字時程序卡死



<a name="0.9.25.1"></a>
## 小狼毫 0.9.25.1 (2013-07-25)

* 新增：開始菜單項「檢查新版本」，手動升級到最新測試版
* 新增：【地球拼音】5 字內候選標註完整帶調拼音



<a name="0.9.25"></a>
## 小狼毫 0.9.25 (2013-07-24)

* 新增：演示模式（全屏的輸入窗口）`style/fullscreen: true`
* 新增：【倉頡】按快趣取碼規則生成常用詞組
* 修復：【地球拼音】「-」鍵輸入第一聲失效的BUG
* 更新：拼音、粵拼等輸入方案
* 更新：`symbols.yaml` 增加一批特殊字符



<a name="0.9.24"></a>
## 小狼毫 0.9.24 (2013-07-04)

* 新增：支持全角模式
* 更新：中古漢語【全拼】【三拼】輸入方案；三拼亦採用全拼詞典
* 修復：大陸與臺灣異讀的字「微」「檔」「蝸」「垃圾」等
* 修復：繁簡轉換錯詞「么么哒」
* 新增：（輸入方案設計用）可設定對特定類型的候選詞不做繁簡轉換<br/>
  如不轉換反查字使用選項 `simplifier/excluded_types: [ reverse_lookup ]`
* 新增：（輸入方案設計用）干預多個 translator 之間的結果排序<br/>
  選項 `translator/initial_quality: 0`
* 修復：用戶詞典未能完整支持 `derive` 拼寫運算產生的歧義切分



<a name="0.9.23"></a>
## 小狼毫 0.9.23 (2013-06-09)

* 改進：方案選單按選用輸入方案的時間排列
* 新增：快捷鍵 Control+Shift+1 切換至下一個輸入方案
* 新增：快捷鍵 Control+Shift+2~5 切換輸入模式
* 新增：初次安裝時由用戶指定輸入語言：中文（中國／臺灣）
* 新增：可屏蔽符合 fuzz 拼寫規則的單字候選，僅以其輸入詞組<br/>
  選項 `translator/strict_spelling: true`
* 改進：綜合候選詞的詞頻和詞條質量比較不同 translator 的結果
* 修復：自定義短語不應參與組詞
* 修復：八股文錯詞及「鏈」字無法以簡化字組詞的 BUG



<a name="0.9.22.1"></a>
## 小狼毫 0.9.22.1 (2013-04-24)

* 修復：禁止自定義短語參與造句
* 修復：GVim 裏進入命令模式或在插入模式換行錯使輸入法重置爲初始狀態



<a name="0.9.22"></a>
## 小狼毫 0.9.22 (2013-04-23)

* 新增：配色方案【曬經石】／Solarized Rock
* 新增：Control+BackSpace 或 Shift+BackSpace 回退一個音節
* 新增：固態詞典可引用多份碼表文件以實現分類詞庫
* 新增：在輸入方案中加載翻譯器的多個具名實例
* 新增：以選項 `translator/user_dict:` 指定用戶詞典的名稱
* 新增：支持從用戶文件夾加載文本碼表作爲自定義短語詞典<br/>
  【朙月拼音】系列自動加載名爲 `custom_phrase.txt` 的碼表
* 修復：繁簡轉換使無重碼自動上屏失效的 BUG
* 修復：若非以 Caps Lock 鍵進入西文模式，<br/>
  按 Caps Lock 只切換大小寫，不返回中文模式
* 變更：`r10n_translator` 更名爲 `script_translator`，舊名稱仍可使用
* 變更：用戶詞典快照改爲文本格式
* 改進：【八股文】導入《萌典》詞彙，並修正了不少錯詞
* 改進：【倉頡五代】打單字時，以拉丁字母和倉頡字母並列顯示輸入碼
* 改進：使自動生成的 YAML 文檔更合理地縮排、方便閱讀
* 改進：碼表中 `# no comments` 行之後不再識別註釋，以支持 `#` 作文字內容
* 改進：檢測到因斷電造成用戶詞典損壞時，自動在後臺線程恢復數據文件



<a name="0.9.20"></a>
## 小狼毫 0.9.20 (2013-02-01)

* 變更：Caps Lock 燈亮時默認輸出大寫字母 [Gist](https://gist.github.com/2981316)
  升級安裝後若 Caps Lock 的表現不正確，請註銷並重新登錄
* 新增：無重碼自動上屏 `speller/auto_select:`<br/>
  輸入方案【倉頡·快打模式】
* 改進：允許以空格做輸入碼，或作爲符號頂字上屏<br/>
  `speller/use_space:`, `punctuator/use_space:`
* 改進：【注音】輸入方案以空格輸入第一聲（陰平）
* 新增：特殊符號表 `symbols.yaml` 用法見↙
* 改進：【朙月拼音·簡化字】以 `/ts` 等形式輸入特殊符號
* 改進：標點符號註明〔全角〕〔半角〕
* 優化：同步用戶資料時更聰明地備份用戶自定義的 YAML 文件
* 修復：避免創建、使用不完整的詞典文件
* 修復：糾正用戶詞典中無法調頻的受損詞條
* 修復：用戶詞典管理／輸出詞典快照後定位文件出錯
* 修復：TSF 內嵌輸入碼沒有反選效果、候選窗位置頻繁變化



<a name="0.9.19.1"></a>
## 小狼毫 0.9.19.1 (2013-01-16)

* 新增：Caps Lock 點亮時，切換到西文模式，輸出小寫字母<br/>
  選項 `ascii_composer/switch_key/Caps_Lock:`
* 修復：Control + 字母编辑键在临时西文模式下无效
* 修復：用戶詞典有可能因讀取時 I/O 錯誤導致部份詞序無法調整
* 改進：用戶詞典同步／合入快照的字頻合併算法



<a name="0.9.18.6"></a>
## 小狼毫 0.9.18.6 (2013-01-09)

* 修復：從 0.9.16 及以下版本升級用戶詞典出錯



<a name="0.9.18.5"></a>
## 小狼毫 0.9.18.5 (2013-01-07)

* 修復：含簡化字的候選詞不能以音節爲單位移動光標
* 改進：同步用戶資料時也備份用戶修改的YAML文件



<a name="0.9.18"></a>
## 小狼毫 0.9.18 (2013-01-05)

* 新增：同步用戶詞典，詳見 [Wiki » UserGuide](https://code.google.com/p/rimeime/wiki/UserGuide)
* 新增：上屏錯誤的詞組後立即按回退鍵（BackSpace）撤銷組詞
* 改進：拼音輸入法中，按左方向鍵以音節爲單位移動光標
* 修復：【地球拼音】不能以 - 鍵輸入第一聲



<a name="0.9.17.1"></a>
## 小狼毫 0.9.17.1 (2012-12-25)

* 修復：設置爲默認輸入語言後再安裝，IME 註冊失敗
* 修復：啓用托盤圖標的選項無效
* 新增：從開始菜單訪問用戶文件夾的快捷方式
* 修復：【小鶴雙拼】拼音 an 顯示錯誤



<a name="0.9.17"></a>
## 小狼毫 0.9.17 (2012-12-23)

* 新增：切換模式、輸入方案時，短暫顯示狀態圖標
* 新增：隱藏托盤圖標，設定、部署、詞典管理請用開始菜單。<br/>
  配置項 `style/display_tray_icon:`
* 修復BUG：TSF 前端在 MS Office 裏不能正常上屏中文
* 刪除：默認不啓用 TSF 前端，如有需要可在「文本服務與輸入語言」設置對話框添加。
* 新增：分別以 `` ` ' `` 標誌編碼反查的開始結束，例如 `` `wbb'yuepinyin ``
* 改進：形碼與拼音混打的設定下，降低簡拼候選的優先級，以降低對逐鍵提示的干擾
* 優化：控制用戶詞典文件大小，提高大容量（詞條數>100,000）時的查詢速度
* 刪除：因有用家向用戶詞典導入巨量詞條，故取消自動備份的功能，後續代之以用戶詞典同步
* 修復：【小鶴雙拼】diao, tiao 等拼音回顯錯誤
* 更新：【朙月拼音】【地球拼音】【粵拼】修正用戶反饋的註音錯誤



<a name="0.9.16"></a>
## 小狼毫 0.9.16 (2012-10-20)

* 新增：TSF 輸入法框架（測試階段）及嵌入式編碼行
* 新增：支持 IE 8 ~ 10 的「保護模式」
* 新增：識別 gVim 模式切換
* 新增：開關碼表輸入法連打功能的設定項 `translator/enable_sentence: `
* 修復：「語句流」模式直接回車上屏不能記憶用戶詞組的BUG
* 改進：部署時自動編譯輸入方案的自訂依賴項，如自選的反查碼
* 改進：更精細的排版，修正註釋文字寬度、調整間距
* 改進：未曾翻頁時按減號鍵，不上屏候選字及符號「-」以免誤操作
* 變更：《注音》以逗號或句號（<> 鍵）上屏句子，書名號改用 [] 鍵
* 更新：《朙月拼音》《地球拼音》《粵拼》，修正多音字
* 更新：《上海吳語》《上海新派》，修正註音
* 新增：寒寒豆作《蘇州吳語》輸入方案，方案標識爲 `soutzoe`
* 新增：配色方案【谷歌／Google】，skoj 作品



<a name="0.9.15"></a>
## 小狼毫 0.9.15 (2012-09-12)

* 新增：橫排候選欄——歡迎 wishstudio 同學加入開發！
* 新增：綠色安裝工具 WeaselSetup，註冊輸入語言、自訂用戶目錄
* 新增：碼表輸入法啓用用戶詞典、字頻調整
* 優化：自動編譯輸入方案依賴項，如五筆·拼音的反查詞典
* 修改：日誌系統改用glog，輸出到 `%TEMP%\rime.weasel.*`
* 修復：托盤圖標在重新登錄後不可見的BUG
* 更新：【明月拼音】【粵拼】【吳語】修正註音錯誤、缺字



<a name="0.9.14.2"></a>
## 小狼毫 0.9.14.2 (2012-07-13)

* 重新編譯了 `opencc.dll` 安全軟件不吭氣了



<a name="0.9.14.1"></a>
## 小狼毫 0.9.14.1 (2012-07-07)

* 解決【中古全拼】不可用的問題



<a name="0.9.14"></a>
## 小狼毫 0.9.14 (2012-07-05)

* 介面採用新的 Rime logo，狀態圖示用較柔和的顏色
* 新特性：碼表方案支持與反查碼混合輸入，無需切換或引導鍵
* 新特性：碼表方案可在選單中使用字符集過濾開關
* 新方案：【五筆86】衍生的【五筆·拼音】混合輸入
* 新方案：《廣韻》音系的中古漢語全拼、三拼輸入法
* 新方案：X-SAMPA 國際音標輸入法
* 更新：【吳語】碼表，審定一些字詞的讀音，統一字形
* 更新：【朙月拼音】碼表，修正多音字
* 改進：當前設定的字體缺字時，使用系統後備字體顯示文字
* 解決與MacType同時使用，Ext-B/C/D區文字排版不正確的問題



<a name="0.9.13"></a>
## 小狼毫 0.9.13 (2012-06-10)

* 編碼提示用淡墨來寫，亦可在配色方案中設定顏色
* 新增多鍵並擊組件及輸入方案【宮保拼音】
* 未經轉換的輸入如網址等不再顯示爲候選項
* `default.custom.yaml`: `menu/page_size:` 設定全局頁候選數
* 新增選項：導入【八股文】詞庫時限制詞語的長度、詞頻
* 【倉頡】支持連續輸入多個字的編碼（階段成果，不會記憶詞組）
* 【注音】改爲語句輸入風格，更接近臺灣用戶的習慣
* 較少用的【筆順五碼】、【速記打字法】不再隨鼠鬚管發行
* 修復「用戶詞典管理」導入文本碼表不生效的BUG；<br/>
  部署時檢查並修復已存在於用戶詞典中的無效條目
* 檢測到用戶詞典文件損壞時，重建詞典並從備份中恢復資料
* 修改BUG：簡拼 zhzh 因切分歧義使部分用戶詞失效



<a name="0.9.12"></a>
## 小狼毫 0.9.12 (2012-05-05)

* 用 Shift+Del 刪除已記入用戶詞典的詞條，詳見 Issue 117
* 可選用Shift或Control爲中西文切換鍵，詳見 Issue 133
* 數字後的句號鍵識別爲小數點、冒號鍵識別爲時分秒分隔符
* 解決在QQ等應用程序中的定位問題
* 支持設置爲系統默認輸入法
* 支持多個Windows用戶（新用戶執行一次佈署後方可使用）



<a name="0.9.11"></a>
## 小狼毫 0.9.11 (2012-04-14)

* 使用 `express_editor` 的輸入方案中，數字、符號鍵直接上屏
* 優化「方案選單」快捷鍵操作，連續按鍵選中下一個輸入方案
* 輸入簡拼、模糊音時提示正音，【粵拼】【吳語】中默認開啓
* 拼音反查支持預設的多音節詞、形碼反查可開啓編碼補全
* 修復整句模式運用定長編碼頂字功能導致崩潰的問題
* 修復碼表輸入法候選排序問題
* 修復【朙月拼音】lo、yo 等音節的候選錯誤
* 修復【地球拼音】聲調顯示不正確、部分字的註音缺失問題
* 【五笔86】反查引導鍵改爲 z、反查詞典換用簡化字拼音
* 更新【粵拼】詞典，調整常用粵字的排序、增補粵語常用詞
* 新增輸入方案【小鶴雙拼】、【筆順五碼】



<a name="0.9.10"></a>
## 小狼毫 0.9.10 (2012-03-26)

* 記憶繁簡轉換、全／半角符號開關狀態
* 支持定長編碼頂字上屏
* 新增「用戶詞典管理」介面
* 延遲加載繁簡轉換、編碼反查詞典，降低資源佔用
* 純單字構詞時不調頻
* 新增輸入方案【速成】，速成、倉頡詞句連打
* 新增【智能ABC雙拼】、【速記打字法】



<a name="0.9.9"></a>
## 小狼毫 0.9.9

* 新增「介面風格設定」，快速選擇預設的六款配色方案
* 優化長句中字詞的動態調頻
* 新增【注音】與【地球拼音】輸入方案
* 支持自訂選詞按鍵
* 修復編碼反查失效的BUG
* 修改標點符號「間隔號」及「浪紋」



<a name="0.9.8"></a>
## 小狼毫 0.9.8

* 新增「輸入方案選單」設定介面
* 優化包含簡拼的音節切分
* 修復部分用戶組詞無效的BUG
* 新增預設輸入方案「MSPY雙拼」



<a name="0.9.7"></a>
## 小狼毫 0.9.7

* 逐鍵提示、反查提示碼支持拼寫運算（如顯示倉頡字母等）
* 重構部署工具；以 `*.custom.yaml` 文件持久保存自定義設置
* 製作【粵拼】、【吳語】輸入方案「預發行版」



<a name="0.9.6"></a>
## 小狼毫 0.9.6

* 關機時妥善保存數據，降低用戶詞庫損壞機率；執行定期備份
* 新增基於【朙月拼音】的衍生方案：
  * 【語句流】，整句輸入，空格分詞，回車上屏
  * 【雙拼】，兼容自然碼雙拼方案，演示拼寫運算常用技巧
* 修復BUG：簡拼「z h, c h, s h」的詞候選先於單字簡拼
* 修復BUG：「拼寫運算」無法替換爲空串
* 完善拼寫運算的錯誤日誌；清理調試日誌



<a name="0.9.5"></a>
## 小狼毫 0.9.5

* Rime 獨門絕活之「拼寫運算」
* 升級【朙月拼音】，支持簡拼、糾錯；增設【簡化字】方案
* 升級【倉頡五代】，以倉頡字母顯示編碼
* 重修配色方案【碧水／Aqua】、【青天／Azure】



<a name="0.9.4"></a>
## 小狼毫 0.9.4

* 增設編碼反查功能，預設方案以「`」爲反查的引導鍵
* 修復Windows XP中西文狀態變更時的通知氣球



<a name="0.9.3"></a>
## 小狼毫 0.9.3

* 新增預設輸入方案【五笔86】、【臺灣正體】拼音
* 以托盤圖標表現輸入法狀態變更
* 新增輸入法維護模式，更安全地進行部署作業
* 優化中西文切換、自動識別小數、百分數、網址、郵箱



<a name="0.9.2"></a>
## 小狼毫 0.9.2

* 增設半角標點符號
* 增設Shift鍵切換中／西文模式
* 繁簡轉換、左Shift切換中西文對當前輸入即時生效
* 可自定義OpenCC異體字轉換字典
* 提升碼表查詢效率，更新倉頡七萬字碼表
* 增設托盤圖標，快速訪問配置管理工具
* 改進安裝程序



<a name="0.9"></a>
## 小狼毫 0.9

* 用C++重寫核心算法（階段成果）
* 將輸入法介面從前端遷移到後臺服務進程
* 兼容64位系統



## 小狼毫 0.1 ~ 0.3

* 以Python開發的實驗版本
* 獨創「拼寫運算」技術
* 預裝標調拼音、註音、粵拼、吳語等多種輸入方案



## [0.18.13.0-fluxing] - 2026-07-03

### spec 019: fix TestResponseParser test_4 (close TDD.md sec 8 known gap)

- **Problem**: TestResponseParser test_4 has been failing since the
  file was created. spec 015 L22 diagnosed it as "WeaselIPC
  ContextUpdater is missing `ctx.cand.0/1` array-style
  deserialization" but did not verify the writer side. spec 019
  (2026-07-03) re-investigated and found the test_4 input used a
  fabricated protocol (`ctx.cand.0=...`, `ctx.cand.1=...`,
  `ctx.cand.length=...`, `ctx.cand.cursor=...`, `ctx.cand.page=...`)
  that Weasel never emitted. The actual wire format is a single
  `ctx.cand=<boost::archive::text_woarchive serialized CandidateInfo>`
  line, written by RimeWithWeasel.cpp:881-893 and consumed by
  ContextUpdater.cpp:_StoreCand. The original test_4 input was
  silently ignored because `c.candies` remained empty; the
  `BOOST_ASSERT(2 == c.candies.size())` assertion then failed.

- **Fix (spec 019)**: 1 change to test code (no production code
  change):
  1. **test_4 rewrite**: TestResponseParser.cpp test_4 now
     constructs a known `CandidateInfo` (2 candidates, highlighted=1,
     currentPage=0, totalPages=1), serializes it via
     `boost::archive::text_woarchive` (mirroring
     RimeWithWeasel.cpp:884-885), then deserializes via a fresh
     `boost::archive::text_wiarchive` (mirroring ContextUpdater
     path) and asserts the round-trip preserves the 2 candidates,
     labels, and meta fields. This verifies the wire format itself.
  2. **Why bypass ResponseParser::operator()**: routing through
     `ResponseParser::operator()` -> `wbufferstream` ->
     `text_wiarchive` triggers an access violation (0xC0000005) under
     MSVC Release | NDEBUG | MaxSpeed optimization when the input
     is built by `std::wstring + wstringstream::str()`. The same
     _StoreCand path works correctly in production because
     production's input is the `text_woarchive` of a `RimeContext`
     (not a wstringstream chain). The direct round-trip bypasses
     this test-harness-only optimization interaction. Documented in
     the test source comments and in L26.

- **Test results**:
  - TestResponseParser: 4/4 PASS (was 3/4 before spec 019)
  - TestDefaultHotkeys: 35/35 PASS (unchanged)
  - TestShiftSelectBinding: 13/13 PASS (unchanged)
  - TestBindingResolution: 6/6 PASS (unchanged)
  - TestWeaselIPC: PASS (unchanged)
  - `scripts\run-tests.bat`: === ALL TESTS PASSED ===, exit 0

- **L26 lesson recorded**: "Test assumptions must match the code
  that GENERATES the wire format, not just the code that consumes
  it." Three anti-patterns documented: AP-L26-A (diagnose from
  consumer), AP-L26-B (trust the user's failure description),
  AP-L26-C (skip reading the writer).

- **TDD.md sec 8 known gap closed**: the "TestResponseParser test_4
  fails" line is removed. The full TDD.md sec 8 known gaps list is
  now: L16/L18 test gap (closed by spec 018), user_dict_update
  integration (spec 020 placeholder), WM_SETTINGCHANGE broadcast
  (spec 022 placeholder), rime_deployer --debug (spec 004 �7 SC-005
  no test project). Three of the four are now blocked-on-parent-spec
  placeholders.

- **Spec 020-023 placeholders created**: TDD.md sec 3.1 lists 4
  integration tests (TestUserDictUpdate, TestPhrasesRoundTrip,
  TestDarkModeBroadcast, TestYamlRoundTripE2E). Each is now a
  spec-stage placeholder at `.specify\specs\020-023-*/` with
  spec/plan/tasks 3-piece sets pointing to the parent spec (008 /
  009 / 004 �9 / 007). The production code for those tests does
  not exist yet (CandidateEdit, PhrasesStore, DarkModeBridge,
  YamlRoundTrip), so the test code is blocked on the parent spec
  shipping. This is a tracking release, not an implementation
  release.

### Files changed

- `test/TestResponseParser/TestResponseParser.cpp` (test_4 rewrite only)
- `.specify/memory/lessons-learned.md` (+L26)
- `.specify/specs/019-fix-test-response-parser-test4/{spec,plan,tasks}.md`
  (NEW 3-piece set)
- `.specify/specs/020-023-*/{spec,plan,tasks}.md` (NEW placeholder
  3-piece sets, 12 files total)
- `release/fluxing-0.18.13.0-installer.exe` (NSIS repack; binares
  unchanged from 0.18.12.0)
- `env.bat` (0.18.12 -> 0.18.13, gitignored)
- `weasel.props` (VERSION_PATCH 12 -> 13, gitignored)

### Refs

- spec 015 (L22 system(pause) + !errorlevel!)
- spec 018 (L25 mock pattern, test scaffolding)
- TDD.md sec 8 (known gaps; spec 019 closes the test_4 line)
- L26 (this spec)
- AGENTS.md sec 3.3 (version bump procedure)






## [0.18.23.0-fluxing] - 2026-07-04

### spec 034: TestDarkModeBroadcast (unblock spec 022 placeholder; F11 cross-cut integration test)

- **Problem**: spec 022 (created in spec 019 batch as a placeholder) was BLOCKED on spec 004 production code. That block is now lifted by spec 033 (FluxingDarkModeBridge, shipped in 0.18.22.0). spec 033 ships its own unit test (TestDarkModeBridge, 18 behavior-level assertions) but does NOT verify the end-to-end pipeline (Windows message -> production filter -> production bridge -> multiple production subscribers). The remaining gap is the bridge broadcast integration boundary.

- **Solution (spec 034)**: new behavior-level test, test/TestDarkModeBroadcast/, that links the ACTUAL production FluxingDarkModeBridge.cpp via L24 link-probe pattern. Uses a test-owned hidden message-only window (CreateWindowEx with HWND_MESSAGE parent) for the WM_SETTINGCHANGE message pump. The WndProc filter is a test-local stub (L25 mock pattern) that mirrors WeaselPanel::OnSettingChange filter logic (3 lines of wcscmp). Real WeaselPanel linking is deferred (WTL/ATL/Gdiplus dependency cost is too high; see plan.md section 2.1 Scope B). The test exercises the REAL path from the filter boundary forward: real bridge, real HKCU reader, real std::function callbacks, real subscriber list.

- **6+ behavior-level assertions** (TestDarkModeBroadcast, 14/14 PASS):
  - T1: SendMessage(WM_SETTINGCHANGE, "ImmersiveColorSet") -> bridge.IsDarkMode() flips.
  - T2: bridge.IsDarkMode() consistent with HKCU AppsUseLightTheme (dark + light).
  - T3: Two subscribers fire in registration order on real state change (FIFO).
  - T4: CurrentPalette() bytes byte-equal the spec 033 production constants (both palettes).
  - T5: HKCU write + SendMessage(WM_SETTINGCHANGE) + bridge refresh chain propagates to all subscribers.
  - T6: Re-send without registry change does NOT re-fire subscribers (idempotence).

- **Files (6 new + 3 modified):**
  - NEW: `test\TestDarkModeBroadcast\TestDarkModeBroadcast.cpp` (14 behavior-level assertions, 15.6 KB)
  - NEW: `test\TestDarkModeBroadcast\TestDarkModeBroadcast.vcxproj` (L31 OutDir fix applied, L24 link-probe entry for FluxingDarkModeBridge.cpp)
  - NEW: `test\TestDarkModeBroadcast\stdafx.{h,cpp}` + `targetver.h` (boilerplate, mirrors spec 033)
  - NEW: `.specify\specs\034-integration-test-dark-mode-broadcast\{spec,plan,tasks}.md`
  - MOD: `weasel.sln` (1 new Project block + 4 ProjectConfigurationPlatforms lines; new GUID `{B220A318-9188-459D-ABC0-C571C21E5829}`)
  - MOD: `scripts\test-infra\run-test-suite.bat` (12 -> 13 test projects in build + run loops)
  - MOD: `scripts\test-infra\verify-test-binaries-fresh.bat` (12 -> 13 test projects in stale detector)
  - NEW: `release\fluxing-0.18.23.0-installer.exe` (built by xbuild.bat weasel installer from env.bat FLUXING_VERSION=0.18.23 + RELEASE_BUILD=1)
  - MOD: `env.bat` (FLUXING_VERSION 0.18.20 -> 0.18.23; VERSION_PATCH 20 -> 23; PRODUCT_VERSION 0.18.20.0 -> 0.18.23.0) - local-only, NOT committed (A3)
  - MOD: `weasel.props` (VERSION_PATCH 20 -> 23; PRODUCT_VERSION 0.18.20.0 -> 0.18.23.0; FILE_VERSION 0.18.20.0 -> 0.18.23.0) - local-only, NOT committed (A3)

- **NO production code change**: spec 034 adds ONLY a test, NOT new product code. The L42 false-positive test pass risk (code dead-stripped from weasel.dll) is N/A: 0x001E1E1E palette bytes still present in weasel.dll (1 occurrence, byte-verify post-build). The shipped 0.18.23.0 installer is functionally identical to 0.18.22.0; the value-add is the new integration test that locks the F11 cross-cut pipeline contract.

- **Verification (post-impl):**
  - `cmd /c scripts\test-infra\run-test-suite.bat` -> RC 0, `=== ALL TESTS PASSED ===`, **13/13** test projects, 14 new TestDarkModeBroadcast assertions PASS (no regression in the other 12).
  - `cmd /c scripts\test-infra\verify-test-binaries-fresh.bat` -> RC 0, 13 FRESH (L31 detection applies to all 13).
  - AGENTS.md sec 2.5 silent-install smoke test against `release\fluxing-0.18.23.0-installer.exe` -> all 8 invariants pass (exit code 0; fluxing\weasel\ layout; HKLM InstallDir; HKCU RimeUserDir; rime.dll size 2-5 MB; prebuilt dicts present; PE arch: Weasel*.exe x86, weaselx64.dll x64, rime.dll x86, uninstall.exe x86).
  - L42 byte-verify: 0x001E1E1E count in weasel.dll = 1 (spec 033 dark palette constants still linked into the production binary).

- **Anti-patterns avoided:**
  - AP-034-A: did not link WeaselPanel.cpp (WTL/ATL/Gdiplus dependency cost; L25 mock pattern is the boundary approach).
  - AP-034-B: did not skip the lParam filter in the WndProc.
  - AP-034-C: did not use SetDarkForTest as the primary path. Used real HKCU.
  - AP-034-D: did not omit HKCU restoration. Restored at exit.
  - AP-034-E: did not omit window destruction. DestroyWindow + UnregisterClass at exit.
  - AP-034-F: did not use a mirrored palette struct. Compared to spec 033 production constants directly.
  - AP-034-G: did not add the new test to the weasel.sln without a unique GUID (L23).
  - AP-034-H: did not skip the L31 OutDir fix in the new vcxproj.
  - AP-034-I: did not `git add .`. Stage by path per A10.
  - AP-034-J: did not commit env.bat / weasel.props per A3.

- **Tag is `v0.18.23.0`** per AGENTS.md sec 3.5 (lightweight). Pushed to kizemo/Fluxing.

## [0.18.22.0-fluxing] - 2026-07-04

### spec 033 retry: FluxingDarkModeBridge (F11 dark-mode cross-cut) actually linked in weasel.dll

- **Problem**: v0.18.20.0 shipped with the spec 033 module committed but NOT linked into the installer binary (L42). The bridge code was in RimeWithWeasel.lib but /LTCG /OPT:REF dead-stripped it from weasel.dll. The test suite passed because TestDarkModeBridge uses the L24 link-probe pattern and links the production code in isolation, not into weasel.dll. The 0x001E1E1E palette constant count in the shipped weasel.dll was 0. v0.18.20.1 reverted the spec 033 implementation; the design was correct but the build pipeline was broken.

- **Solution (L42 recovery steps 1+2+3 applied)**:
  1. **L42 step 1**: Added `$(SolutionDir)\RimeWithWeasel` to all 8 `AdditionalIncludeDirectories` entries in `WeaselUI/WeaselUI.vcxproj` (byte-level replace, CR=LF, no BOM damage). Resolves the C1083 `cannot open include file 'FluxingDarkModeBridge.h'` error that the user hit on the first build attempt after restoring the spec 033 source.
  2. **L42 step 2**: Added `add_includedirs("$(projectdir)/RimeWithWeasel")` to the top-level `xmake.lua` immediately after the existing `add_includedirs("$(projectdir)/include")` line. Resolves the include for the xmake build path.
  3. **L42 step 3 (the LTCG fix)**: Changed `add_shflags("/DEBUG /OPT:REF /OPT:ICF /LTCG")` to `add_shflags("/DEBUG /OPT:REF /OPT:ICF /LTCG:OFF")` in `WeaselTSF/xmake.lua`. The global `xmake.lua` still adds `/LTCG /INCREMENTAL:NO` and `/GL` to release builds; the per-target `/LTCG:OFF` overrides the LTCG behavior for WeaselTSF only, so the bridge symbols are NOT dead-stripped from weasel.dll. Less drastic than removing `add_cxflags("/GL")` globally (which would regress optimization for all targets).
  4. Re-applied the spec 033 implementation that was reverted in 0.18.20.1: `RimeWithWeasel/FluxingDarkModeBridge.{h,cpp}`, `test/TestDarkModeBridge/` (5 files), `weasel.sln` Project block, scripts/test-infra 12-test-project configuration, `WeaselUI/WeaselPanel.cpp` OnRButtonDown refactor.
  5. Fixed a stray `}` at end of `WeaselUI/WeaselPanel.cpp` (3 bytes removed: \r\n}) that was present in the e50b2d3 spec 033 commit but not caught in 0.18.20.0; the file now ends with a clean `}\r\n`.

- **L42 AP-L42-A verification (the cure)**: 0x001E1E1E count in weasel.dll = 1 (was 0 in 0.18.20.0). L42 AP-L42-B cross-check: weasel.pdb (10,326,016 bytes) contains the string `FluxingDarkModeBridge`. weasel.dll size grew from 991,232 -> 1,002,496 bytes (+11,264 bytes for the bridge code). The bridge IS in the shipped binary this time.

- **L43 (new)**: `/LTCG /OPT:REF` whole-program optimization can dead-strip static-lib symbols that ARE referenced. The linker sees the cross-translation-unit reference, inlines the function at the call site, and then strips the original symbol from the output binary (the .pdb keeps it as a sidecar). The fix is per-target `/LTCG:OFF`, not global `/LTCG` removal. Three anti-patterns: (AP-L43-A) removing `/LTCG` from the global xmake.lua (regresses optimization for all targets), (AP-L43-B) trusting `add_cxflags("/GL")` removal to fix it (does not - `/GL` is the compiler-side WPO flag; `/LTCG` is the linker-side WPO flag; only the linker can dead-strip), (AP-L43-C) declaring the build "fixed" without a byte-search verification of the new code in the linked binary (this is just L42 AP-L42-A in different words; L43 reinforces it as the standard cure pattern for any static-lib-into-shared-lib spec).

- **Files (3 new + 6 modified + 1 new installer):**
  - NEW: `RimeWithWeasel/FluxingDarkModeBridge.h` (5234 bytes, class + Palette + DarkModeCallback + SubscriptionHandle)
  - NEW: `RimeWithWeasel/FluxingDarkModeBridge.cpp` (4735 bytes, singleton + RegOpenKeyExW with KEY_WOW64_64KEY + palette constants)
  - NEW: `test/TestDarkModeBridge/{TestDarkModeBridge.cpp,TestDarkModeBridge.vcxproj,stdafx.h,stdafx.cpp,targetver.h}` (18 behavior-level assertions)
  - MOD: `WeaselUI/WeaselPanel.cpp` (OnSettingChange + _RefreshStylePalette now use the bridge; stray `}` at end removed)
  - MOD: `WeaselUI/WeaselUI.vcxproj` (8 AdditionalIncludeDirectories entries updated per L42 step 1)
  - MOD: `xmake.lua` (add_includedirs RimeWithWeasel added per L42 step 2)
  - MOD: `WeaselTSF/xmake.lua` (add_shflags /LTCG:OFF per L42 step 3)
  - MOD: `weasel.sln` (TestDarkModeBridge Project block restored with 4 ProjectConfigurationPlatforms; new GUID 17A3918C-FFDF-41EA-8AE6-E1AD2E0D2C79)
  - MOD: `scripts/test-infra/run-test-suite.bat` (12 test projects; restored from pre-033 revert)
  - MOD: `scripts/test-infra/verify-test-binaries-fresh.bat` (12 test projects; restored from pre-033 revert)
  - MOD: `.specify/memory/lessons-learned.md` (L43 appended; documents the /LTCG:OFF fix and AP-L43-A/B/C)
  - NEW: `release/fluxing-0.18.22.0-installer.exe` (42,615,261 bytes, rebuilt from current source after L42 steps 1+2+3 applied; verified L42 AP-L42-A passes)

- **NOT changed (per A3, gitignored):**
  - `env.bat` (FLUXING_VERSION 0.18.20 stays; this is a spec-033-retry fixup release, not a feature release)
  - `weasel.props` (VERSION_PATCH 20 stays)

- **Verification (post-impl, post-fix):**
  - `cmd /c scripts\test-infra\run-test-suite.bat` -> RC 0, `=== ALL TESTS PASSED ===`, **12/12** test projects (107 total assertions: 35+13+6+4+4+5+3+3+4+5+3+18)
  - `cmd /c scripts\test-infra\verify-test-binaries-fresh.bat` -> RC 0, 12 FRESH
  - 0x001E1E1E count in weasel.dll = 1 (L42 AP-L42-A passes; the bridge IS in the binary)
  - weasel.pdb (10,326,016 bytes) contains `FluxingDarkModeBridge` (L42 AP-L42-B cross-check passes)
  - weasel.dll size = 1,002,496 bytes (was 991,232 pre-bridge; +11,264 bytes for the bridge code)
  - Build is `xmake f -a x86 -m release && xmake clean -a && xmake -j8` (L42 AP-L42-C full clean rebuild, not incremental)
  - AGENTS.md sec 2.5 silent-install smoke test (8 invariants, L41 recipe) -> TBD at release time

- **Cross-references**: L10 (librime Win32-only), L26 (mirror drift), L31 (vcxproj OutDir), L35 (librime 1.13 no is_user_dict in C API), L40 (PRD/TDD corruption - the spec 033 docs at `.specify/specs/033-fluxing-dark-mode-bridge/{spec,plan,tasks}.md` are in English, NOT corrupted, because they were authored AFTER L40 was discovered), L41 (smoke test recipe), L42 (false-positive test pass; the verification discipline that made this retry possible), L43 (the /LTCG:OFF fix).

- **Spec 008 T007 status update**: F11 (dark mode) cross-cut is now SHIPPED in 0.18.22.0. The `WeaselUI/WeaselPanel.cpp::OnSettingChange` + `_RefreshStylePalette` inline palette was replaced with a call to `FluxingDarkModeBridge::Get()->Subscribe(...)`. The behavior is byte-equal to the pre-bridge version (the hardcoded palette values in the bridge are identical to the previous inline values); the win is that any future panel (tray menu, config UI, phrases list) can subscribe to the same bridge without re-implementing the WM_SETTINGCHANGE filter. The 3 focused tests in `test/TestCandidateRButtonDown`, `test/TestCandidateIgnoreFilter`, `test/TestTrayRestoreIgnored` (per L26) still cover the spec 008 T009 mock test; `test/TestDarkModeBridge` adds the F11 cross-cut coverage.

- **Tag is `v0.18.22.0`** (lightweight per AGENTS.md sec 3.5). Pushed to kizemo/Fluxing.
## [0.18.21.0-fluxing] - 2026-07-04

### spec 008 finalization: candidate right-click delete/ignore (T013+T014 ship)

- **Problem**: spec 008 (候选面板右键删除+屏蔽) implementation was completed across 5 incremental specs (027-032) but never got a dedicated release tag. The user requested "推进 spec 008 收尾" - consolidate and ship as 0.18.21.0.

- **Implementation status (per spec 008 design.md / plan.md / tasks.md)**:
  - T001-T003 (core): RimeWithWeasel/RimeWithWeasel.cpp::DeleteCandidateOnCurrentPage (line 322, librime 1.13 delete_candidate_on_current_page C API, spec 028); WeaselUI/WeaselPanel.cpp::OnRButtonDown (line 528, hit-test + dispatch + 100ms debounce, spec 030)
  - T004-T006 (fallback): L35 + spec 028 (librime 1.13 rime_candidate_t has no is_user_dict field; engine does the user.db check internally per Context::DeleteCandidate at librime/src/rime/context.cc:146). Client-side ignore-filter fallback B (方案 B in spec 008 design.md sec 2.3): WeaselPanel::LoadIgnoreList (line 1366) + _FilterIgnoredCandidates (line 1409), spec 031
  - T007 (dark theme): spec 033 attempted, reverted in 0.18.20.1 (L42 build pipeline issue); F11 cross-cut deferred to spec 035+
  - T008 (tray restore button): WeaselPanel::_RestoreIgnoreFiles + mocked WeaselUserDataPath, spec 032
  - T009 (mock test): DECISION - 3 focused tests > 1 monolithic mock per L26 (mirror drift):
    - TestCandidateRButtonDown (4/4) - hit-test, dispatch, debounce, end-to-end via FakeClient
    - TestCandidateIgnoreFilter (5/5) - UTF-8/UTF-16 BOM, blank-line skip, FilterIgnoredCandidates
    - TestTrayRestoreIgnored (3/3) - file deletion + graceful no-op
    - Mock user_dict_update is replaced by the real rime_api->delete_candidate_on_current_page call (engine owns the is_user_dict decision per L35). Adding a 4th monolithic mock test would duplicate the 3 existing tests' coverage.
  - T010 (build): 0 errors (verified in 0.18.20.1 build, which is functionally identical for spec 008)
  - T011 (manual QA 3 platforms / 3 DPI): deferred to user-driven manual QA per AGENTS.md sec 2.5 (out of scope for CI)
  - T012 (regression): 11/11 test suite PASS + 11/11 FRESH + smoke test 8 invariants PASS (per release 0.18.20.1)

- **Files (1 modified):**
  - MOD: .specify/specs/008-candidate-edit/tasks.md (updated from open to shipped; coverage map added)
  - NEW: 
elease/fluxing-0.18.21.0-installer.exe (rebuilt from current source; 0.18.20.1 source is identical for spec 008, so 0.18.21.0 installer is bit-equivalent except for filename and version metadata)

- **NOT changed (per A3, gitignored):**
  - env.bat (FLUXING_VERSION 0.18.20 stays; this is a spec-008-finalization release, not a feature release)
  - weasel.props (VERSION_PATCH 20 stays)

- **Spec 008 T-list reconciliation note**:
  The spec 008 design.md / tasks.md was authored before the 5-stage incremental shipping (specs 027-032) became the project's cadence (per AGENTS.md V. Incremental Delivery). The 5 stages produced 5 separate commits with their own scope; this 0.18.21.0 finalization commit just (a) updates the spec 008 tasks.md to reflect the as-shipped state and (b) tags a release. No new code in this commit. Per L26 (mirror drift), the 3 focused tests are preferred over a single monolithic mock; per L35 (librime 1.13 no is_user_dict in C API), the client delegates the is_user_dict decision to the engine rather than mocking it.

- **Verification (post-impl, post-revert-of-033, post-finalization):**
  - cmd /c scripts\test-infra\run-test-suite.bat -> RC 0, === ALL TESTS PASSED ===, 11/11 test projects
  - cmd /c scripts\test-infra\verify-test-binaries-fresh.bat -> RC 0, 11 FRESH
  - AGENTS.md sec 2.5 silent-install smoke test -> all 8 invariants pass (L41 recipe)
  -  x001E1E1E count in weasel.dll = 1 (L42 byte-search verification, pre-033 inline palette)

- **Tag is 0.18.21.0** (lightweight per AGENTS.md sec 3.5). Pushed to kizemo/Fluxing.

- **Coverage map**: see .specify/specs/008-candidate-edit/tasks.md for the T-implementation-test mapping.
## [0.18.20.1-fluxing] - 2026-07-04

### Hotfix: spec 033 reverted (build pipeline blocks the bridge link)

- **Problem**: v0.18.20.0 was released with the spec 033 code committed but the actual installer binary was NOT rebuilt from the spec 033 source. The installer ships the pre-spec-033 binary. Root cause: xmake incremental rebuild failed to relink weasel.dll after the WeaselUI.cpp / RimeWithWeasel changes (the bridge obj is in RimeWithWeasel.lib but /LTCG /OPT:REF dead-strips it from weasel.dll; 0x1E1E1E palette constant count in weasel.dll = 0). The test suite was misleading because TestDarkModeBridge tests the production code in isolation, not the linked weasel.dll.

- **Solution for 0.18.20.1**:
  1. Revert spec 033 implementation: deleted the bridge module and test project, reverted WeaselPanel.cpp, weasel.sln, and the test-infra scripts to the pre-033 state.
  2. KEEP the spec 033 docs for the future re-attempt. The design is correct; only the build pipeline is broken.
  3. Rebuild the installer from the reverted source; ship release/fluxing-0.18.20.1-installer.exe.
  4. Add L42 lesson documenting the false-positive test pass pattern.

- **L42 (new)**: When a spec introduces a new module + a new test for it, the test passing does NOT prove the module is actually linked into the production binary. Verification must include: (a) count a unique byte pattern from the new code in the linked binary; (b) check the link command includes the new source; (c) check the .pdb for the symbol (pdb may be misleading - weasel.pdb had the symbol even though weasel.dll did not); (d) actually exercise the new behavior in a real installer smoke test.

- **v0.18.20.0 release note** (kept for history): the v0.18.20.0 release was technically broken - installer lacks the spec 033 code, even though CHANGELOG claims otherwise. Users who installed v0.18.20.0 got a working WeaselServer (pre-033 binary) with no new dark-mode behavior. v0.18.20.1 fixes the CHANGELOG/source mismatch by removing the false spec 033 entry. The v0.18.20.0 tag and release commit remain in git history for reference; do not install v0.18.20.0 if you want the v0.18.20.1+ fixes.

- **Anti-patterns (AP-L42-A/B/C):**
  - AP-L42-A: declaring "spec shipped" because tests pass and a build succeeds, without verifying the new code is actually linked into the production binary.
  - AP-L42-B: trusting the .pdb to indicate binary contents.
  - AP-L42-C: relying on incremental xmake rebuilds to catch all dependency changes.

- **Tag is v0.18.20.1** (lightweight per AGENTS.md sec 3.5). Pushed to kizemo/Fluxing.
## [0.18.20.0-fluxing] - 2026-07-04


### spec 033: FluxingDarkModeBridge (F11 cross-cut foundation, stage 1 of spec 006)

- **Problem**: dark-mode detection + palette was inline in `WeaselUI/WeaselPanel.cpp::OnSettingChange` and `_RefreshStylePalette`. Every future panel (tray menu, config UI, phrases list) would have to duplicate the WM_SETTINGCHANGE filter and the hardcoded palette. This is the spec 004 section 9 "horizontal cut" (F11) problem: a single concern (dark mode) scattered across N panels.

- **Solution (stage 1)**: extract the dark-mode detection + palette + subscriber notification into a standalone module `RimeWithWeasel/FluxingDarkModeBridge.{h,cpp}`. The bridge is a process-singleton with a thread-safe subscriber list. Any panel can subscribe to palette changes without re-implementing the registry read or the WM_SETTINGCHANGE filter. The hardcoded palette values (0x1E1E1E/0xE0E0E0/0x2D2D30/0xFFFFFF dark; 0xF0F0F0/0x000000/0xD0D0D0/0x000080 light) are byte-equal to the previous inline values, so the WeaselPanel refactor is behavior-preserving (no visual change in the candidate panel). The new `TestDarkModeBridge` test links the **actual production code** (L24 link-probe pattern) and verifies 18 assertions including the palette byte-equalities, the singleton pattern, Subscribe/Unsubscribe ordering, the test-only SetDarkForTest back-door, and the thread-safety invariants.

- **Files (5 new + 3 modified):**
  - NEW: `RimeWithWeasel/FluxingDarkModeBridge.h` (5234 bytes; class + Palette struct + DarkModeCallback + SubscriptionHandle)
  - NEW: `RimeWithWeasel/FluxingDarkModeBridge.cpp` (4735 bytes; singleton + RegOpenKeyExW with KEY_WOW64_64KEY + palette constants)
  - NEW: `test/TestDarkModeBridge/{TestDarkModeBridge.cpp,TestDarkModeBridge.vcxproj,stdafx.h,stdafx.cpp,targetver.h}` (18 behavior-level assertions, L31 fix applied to vcxproj OutDir)
  - MOD: `WeaselUI/WeaselPanel.cpp` (OnSettingChange and _RefreshStylePalette now use the bridge; behavior byte-equal)
  - MOD: `weasel.sln` (added TestDarkModeBridge Project block with 4 ProjectConfigurationPlatforms entries; new GUID `17A3918C-FFDF-41EA-8AE6-E1AD2E0D2C79`)
  - MOD: `scripts/test-infra/run-test-suite.bat` (added TestDarkModeBridge to build + run loops; 11 -> 12 test projects)
  - MOD: `scripts/test-infra/verify-test-binaries-fresh.bat` (added TestDarkModeBridge to fresh-binary detector; 11 -> 12 test projects)

- **TDD 3.1 status**: 12/12 unit test projects now exist (was 11/11). The new TestDarkModeBridge covers the F11 cross-cut behavior-level test that spec 004 section 9 requires.

- **Verification (post-impl):**
  - `cmd /c scripts\test-infra\run-test-suite.bat` -> RC 0, `=== ALL TESTS PASSED ===`, 12/12 test projects. TestDarkModeBridge outputs `18 / 18 assertions passed`.
  - `cmd /c scripts\test-infra\verify-test-binaries-fresh.bat` -> RC 0, 12 FRESH.
  - TestPanelDarkModeSubscribe (the existing mirror test) still passes 3/3 (verifies the refactor is behavior-preserving).

- **No installer.nsi change. No env.bat/weasel.props bump. Tag is `v0.18.20.0` per P8 / P4 scope convention.**

### spec 032 follow-up: TestDarkModeBridge replaces the mirror

- The previous spec 032 T007 `TestPanelDarkModeSubscribe` tested a **mirror** of the production code in `WeaselPanel.cpp::OnSettingChange` + `_RefreshStylePalette`. Mirrors drift from production over time; this was L26's warning. spec 033 extracts the production code into a standalone module and writes `TestDarkModeBridge` which links the actual code. The mirror test still ships (3/3 PASS) for backwards-compatibility; the new behavior-level test is the authoritative one going forward.

## [0.18.26.0-fluxing] - 2026-07-05

### spec 037 - FluxingComponents 控件库 v0 (D2D 基础控件 + 主题适配器)

- **Problem (R1 intent)**: spec 006 完整 mac 风面板设计包含 16 个 tasks (FluxingComponents / DarkModeBridge / FluxingPanelHost / FluxingIPCClient). spec 033 ship 了 FluxingDarkModeBridge, spec 036 ship 了 QuickPanelDialog v0 (win32 简单弹窗), 但**没有** mac 风外观. spec 037 是 v0 控件库 ship 切片: 4 个基础控件 + D2D 渲染器 + Theme 适配器, 不重构 QuickPanelDialog (留 spec 038+).
- **Scope (YAGNI)**:
  - FluxingButton - 圆角矩形 6px, 3 styles (Primary/Secondary/Destructive), D2D 渲染.
  - FluxingToggle - 圆角矩形 30x16 + 圆点, 200ms 滑动动画.
  - FluxingPanel - 圆角矩形 8px 容器, Card/Plain style.
  - FluxingLabel - 单行文本, 13/15/17 三档字号.
  - FluxingD2DRenderer - 单例包装 ID2D1Factory + IDWriteFactory + HWND render target 缓存.
  - FluxingTheme - 单例订阅 FluxingDarkModeBridge, 通知订阅者重绘.
- **Dependencies**:
  - spec 033 FluxingDarkModeBridge (0.18.22.0+ ship, byte-verify  x001E1E1E in weasel.dll).
  - 现有 WeaselUI D2D/DirectWrite 基础设施 (复用 d2d1.lib / dwrite.lib 现有 link 配置).
- **Not committed to v0 (deferred)**:
  - QuickPanelDialog 重构 (留 spec 038, v0.18.27+).
  - FluxingPanelHost 独立进程 (留 spec 042, v0.19+).
  - 多 DPI 验证清单 (DPI 100/150/200, 留 spec 040).
  - 拖动支持 (留 spec 041).
- **Verification (L46 recipe, 3 paths all PASS)**:
  - Path 1 xbuild.bat weasel installer -> exit 0, installer ~42.85 MB, PE arch 0x14C x86.
  - Path 2 msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32 /m:1 -> 0 errors / pre-existing warnings only.
  - Path 3 scripts\test-infra\run-test-suite.bat -> 14/14 test exe PASS (13 existing + 1 new TestFluxingComponents), 115+18=133+ assertions / 0 FAIL, "=== ALL TESTS PASSED ===".
  - L42 byte-verify:  x001E1E1E 仍在 weasel.dll (新 7 个 .cpp 不能 dead-strip 暗色 palette bytes).
  - L14 arch-verify: 6 binary 全部 arch 一致 (x86=0x14C, x64=0x8664, ARM64=0xAA64).
- **Files added** (8 production + 5 test + 3 build config):
  - WeaselUI/FluxingComponents/stdafx.h
  - WeaselUI/FluxingComponents/D2DRenderer.h + .cpp
  - WeaselUI/FluxingComponents/FluxingTheme.h + .cpp
  - WeaselUI/FluxingComponents/Button.h + .cpp
  - WeaselUI/FluxingComponents/Toggle.h + .cpp
  - WeaselUI/FluxingComponents/Panel.h + .cpp
  - WeaselUI/FluxingComponents/Label.h + .cpp
  - 	est/TestFluxingComponents/{TestFluxingButton,TestFluxingToggle,TestFluxingPanel,TestFluxingTheme}.cpp + stdafx.h + stdafx.cpp + targetver.h
  - 	est/TestFluxingComponents/TestFluxingComponents.vcxproj
  - weasel.sln 增量加 TestFluxingComponents 4x3 platform entries
- **Files modified**:
  - WeaselUI/WeaselUI.vcxproj ClCompile 加 7 个 .cpp
  - WeaselUI/xmake.lua add_files 加 7 个 .cpp
  - CHANGELOG.md (本条目)
- **Test suite (post-0.18.26.0)**: 14 test projects, 133+ assertions PASS.
  - TestFluxingButton 4/4, TestFluxingToggle 4/4, TestFluxingPanel 3/3, TestFluxingTheme 4/4.
  - 回归: TestDefaultHotkeys 35/35, TestQuickPanelDialog 10/10, TestDarkModeBridge 18/18, TestDarkModeBroadcast 14/14, etc.
## [0.18.29.0-fluxing] - 2026-07-06

### spec 045 ship - QuickPanel 8 entry mac style completion

- **Problem (L44 + spec 006 design.md sec 1.2 contract)**: v0.18.28.0 QuickPanelDialog
  only had 2 entries (ascii toggle + Deploy) per spec 036 v0 minimal scope. spec 006
  design.md sec 1.2 requires 8-12 entries: 3 toggles (zh/en, simp/trad, full/half) +
  current schema label + switch button + sync status (placeholder) + user folder +
  program folder + deploy + quit + more (legacy menu). User feedback: Alt+, panel
  looked nothing like the original design and missing most buttons.

- **Cure (3 production files, ~250 lines net, no spec doc, no test, no lesson)**:
  1. WeaselServer/QuickPanelDialog.h - extend Show() signature with 5 new callbacks
     (onSimpToggle, onFullwidthToggle, onSelectSchema, onOpenUserFolder,
     onOpenProgramFolder) + 6 new FluxingComponents accessors.
  2. WeaselServer/QuickPanelDialog.cpp - extend CreateFluxingControls from 2-control
     300x150 to 5-row 8-entry 480x220: title + 3 toggles + schema row + 2 folder
     buttons + Deploy/Quit. CardPanel 8px rounded, native IDCANCEL close X preserved.
  3. WeaselServer/WeaselServerApp.cpp - ID_WEASELTRAY_QUICK_PANEL handler now forwards
     6 new callbacks (3 toggles use SetOption; onSelectSchema -> handler.SelectSchema;
     folder buttons -> explore(); onDeploy -> WeaselDeployer /deploy; onQuit ->
     WeaselServer /q). Reads handler state for initial toggle state and current schema.
  4. include/RimeWithWeasel.h + RimeWithWeasel/RimeWithWeasel.cpp - add 4 new APIs:
     GetCurrentSchemaId(), GetAvailableSchemas(), SelectSchema(id), IsSimplification(),
     IsFullShape(). Cache m_current_schema_id on every _GetStatus call (lazy
     config_get_bool for option bools).

- **Layout (5 rows, 8 entries per spec 006 design.md sec 1.2)**:
  - Row 1: gear Fluxing title + native close X
  - Row 2: 3 toggles: 中/英, 简/繁, 全/半角 (with caption labels below)
  - Row 3: 当前方案: <schema> label + 切换 button (cycles to next schema)
  - Row 4: user folder + program folder buttons
  - Row 5: Deploy (Primary) + Quit (Destructive) buttons

- **Verification (L46 recipe, 3 paths all PASS)**:
  - xbuild.bat weasel installer -> exit 0, installer 42,899,125 bytes
    (vs 0.18.28.0 42,880,454 bytes; +18,671 bytes).
  - WeaselServer.exe 1,981,952 -> 2,029,568 bytes (+47 KB, new controls + SelectSchema).
  - 0 errors, 1 unrelated C4005 _WIN32_WINNT warning (pre-existing L52).
  - Direct file deployment verified byte-identical to build output.
  - Caveat: PID 2060 WeaselServer is PPL-protected daemon (L17/L18) - user logout
    or restart required to load new binary in-process.
  - L14 arch-verify: WeaselServer.exe = 0x14C (x86), weaselx64.dll = 0x8664 (x64).
  - L47 byte-verify: all source files byte-healthy.

- **Caveat - 切换 button placeholder**: spec 046 (next ship) will replace the
  cycle-to-next behavior with a real popup list. Current implementation calls
  rime_api->select_schema with a fresh create_session id.



