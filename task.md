# Task plan — v0.19.0.33 Three-bug fix

> User feedback 2026-07-17 装机 v0.19.0.32 binary:
> 1. QuickPanel 按钮 2 (Phrase button) → 打开文件夹 D:\Program Files\fluxing\weasel
> 2. QuickPanel 按钮 3 (UserDict button) → 图标和最右 account 一样 + 调出空白 UserDict UI
> 3. Alt+. 不能调出常用短语 UI
>
> User directive: "建议第一步，先完善和实现常用短语模块的所有功能，用户词典/快捷键等功能，推到下一环节执行"

## Scope

按 user 指示分阶段:
- **Phase A (本环节)**: 完善常用短语模块 (Phrase button 错路由 + Alt+. hotkey) + DrawIconUserDict 占位
- **Phase B (下一环节)**: UserDictionary 完整功能 (icon 唯一化 + 列表 populate + first-paint)
- **Phase C (下一环节)**: ShortcutSettings 验证 + 视觉 polish

---

## Phase A: 常用短语模块 — 修复 + 完善

### A1. QuickPanel::Show lambda 第 4 个 callback 修复 (Bug 1 root cause)

**文件**: `WeaselServer/WeaselServerApp.cpp` L275-290

**问题**: `onPhrases` (第 4 个 callback) 接成 `explore(install_dir())` — 打开 D:\Program Files\fluxing\weasel。
应该是 `PhrasesDialog::Show()`。

**修改**:
```cpp
// L282 改为:
[this]() {
  fs::path deployer = install_dir() / L"WeaselDeployer.exe";
  ShellExecuteW(NULL, NULL, deployer.c_str(), L"/hotkey", NULL, SW_SHOWNORMAL);
},
// ↑ 这是 onSymbols (第 6 参数), 现在变成 deployer /hotkey — 错位!
// ↑ 正确 onPhrases 应该是 PhrasesDialog::Show()
```

Actually — 让我重新核对 QuickPanel::Show signature 跟 WeaselServerApp.cpp 调用:
- Show(bool currentFullwidth, OnClick onSchema, OnClick onUserFolder, OnClick onPhrases, OnToggle onFullwidth, OnClick onSymbols, OnClick onLogin)
- arg 1: currentFullwidth (ToggleMode 用)
- arg 2: onSchema
- arg 3: onUserFolder
- arg 4: onPhrases
- arg 5: onFullwidth
- arg 6: onSymbols
- arg 7: onLogin

WeaselServerApp.cpp L275-290 当前:
```
1. deployer /hotkey       → arg 2 onSchema
2. explore(user_data)     → arg 3 onUserFolder  ← 正确 (打开 RimeUserDir)
3. explore(install_dir)   → arg 4 onPhrases     ← ★ 错! 应该是 PhrasesDialog::Show()
4. fullwidth toggle       → arg 5 onFullwidth  ← 正确
5. deployer /deploy       → arg 6 onSymbols    ← ★ 错! 应该是 UserDictionary::Show()
6. noop                   → arg 7 onLogin     ← 正确 (no-op)
```

但 SetQuickPanelUserDictCallback / SetQuickPanelShortcutCallback (Run() 末尾 L198-199) **覆盖** s_onUserDict / s_onShortcut。所以 arg 4 / arg 6 setter 覆盖, 实际 callback 是 setter 注入的.

但 arg 4 是 onPhrases **没** setter 覆盖 → 仍用 WeaselServerApp L282 的 lambda `explore(install_dir)` → **打开文件夹 = Bug 1**.

**Phase A.1 修改**:
```cpp
// L282 (第 4 个 lambda) 改为:
[this]() { PhrasesDialog::Show(); }
```

### A2. alt+. hotkey 失败诊断 + UI fallback (Bug 3 root cause)

**文件**: `WeaselServer/WeaselServerApp.cpp` L80-130

**问题**: RegisterHotKey(Alt+.) 失败仅 log stderr warning (service 进程 stderr 不可见). 如果其他 app 抢占 Alt+. (常见: 中文输入法候选翻页), WeaselServer 静默失败.

**修改**:
- Phase A 内: stderr log 增强 + tray icon tooltip 加上 "Alt+. 状态: 已注册/失败"
- Phase B 时机: 加 GUI notification (Phrase button 高亮提示)

**Phase A.2 修改**:
```cpp
// L98-102 改为 (增强 log):
if (!::RegisterHotKey(hwndServer, ID_HOTKEY_PHRASES_DOT, MOD_ALT, VK_OEM_PERIOD)) {
  DWORD err = ::GetLastError();
  std::wcerr << L"[WeaselServerApp] WARN: RegisterHotKey(Alt+.) failed, err="
             << err << L" (ERROR_HOTKEY_ALREADY_REGISTERED=" << err == 1409 << L")"
             << std::endl;
  // Phase B: tray icon tooltip 反映状态
}
```

### A3. DrawIconUserDict 提前实现 (Bug 2a 占位, Phase B 完整化)

**文件**: `WeaselServer/QuickPanelDialog.cpp` + `WeaselServer/QuickPanelDialog.h`

**说明**: 这是 UserDict 模块的 icon, 但 Phase A 先实现占位 (让 cd6f61a9 的"用 Account icon 代替"撤回), Phase B 完整 polish (icon 设计 + 视觉验证)。

**Phase A.3 修改**:
- `QuickPanelDialog.h` 加 `static void DrawIconUserDict(HDC hdc, int x, int y, COLORREF penColor);`
- `QuickPanelDialog.cpp` 实现简单 BookIcon (用 LineTo/MoveTo 画一本开着的书, 类似 macOS Contacts 图标简化版)
- PaintOpaqueContent L903 改 `case 2: DrawIconUserDict(...)` (替换 DrawIconAccount)
- 这样 button 2 (UserDict) 跟 button 4 (Account) 视觉不再冲突

### A4. 测试覆盖补全 (Phase A 必须)

**文件**: `test/v0_19_0_32_e2e/v0_19_0_32_e2e.cpp`

需要新加的 e2e 测试 (Phase A ship gate):
1. **T_QP_Phrase_RealCallback**: mock QuickPanelDialog::Show 调真 WeaselServerApp lambda chain — 验证 button 1 click → PhrasesDialog::Show() (而非 explore)
2. **T_AltDot_Register_Path**: mock RegisterHotKey return success → verify subclass handler dispatch → PhrasesDialog::Show()
3. **T_QP_Icon_Unique**: paint each button → check pixel diff (UserDict icon ≠ Account icon)

**真 user flow 验证** (Phase A ship 前必跑, 不只 sandbox):
- 启动 WeaselServer + 真 SendMessage WM_HOTKEY(9002) (Alt+.) — 看 PhrasesDialog 是否 visible
- 启动 + 真 SendMessage WM_LBUTTONDOWN+UP 给 QuickPanel button (1/2/3) — 看对应 dialog 是否 visible
- QuickPanel paint 时 用 EnumChildWindows 看所有 button bitmap — 验证 UserDict icon ≠ Account

### A5. Ship gate (Phase A 必须全过)

1. **Build**: `msbuild weasel.sln /p:Configuration=Release /p:Platform=Win32` Exit 0
2. **Unit tests**: TestPhrasesDialog 69/69 + TestQuickPanelDialog 12/12 + TestDefaultHotkeys 35/35 + TestDarkModeBridge 18/18 + TestDarkModeBroadcast 14/14 (全 PASS)
3. **e2e**: T_QP_Phrase_RealCallback + T_AltDot_Register_Path + T_QP_Icon_Unique + 现有 158 个全 PASS
4. **PE arch**: WeaselServer.exe / Deployer.exe / Setup.exe 全部 0x014C x86 + weaselx64.dll 0x8664
5. **真 user flow** (PowerShell + SendMessage WM_HOTKEY + EnumChildWindows):
   - Alt+. → PhrasesDialog visible + 9 children visible
   - Phrase button click → PhrasesDialog visible + 9 children visible (新测)
6. **user 装机手测**: 必须 user 报告 "Phrase 按钮能调出 UI" + "Alt+. 也能调出 UI"

---

## Phase B: 用户词典模块 — 下一环节 (User directive)

### B1. UserDict icon 与其他 button 视觉差异保留

Phase A.3 已经把 case 2 从 DrawIconAccount 改成 DrawIconUserDict。Phase B 只需 polish icon 设计 (加 hover 状态、active 状态、字号调优等)。

### B2. UserDictionary main window 显示完整功能 (Bug 2b root cause)

**问题**: UserDict 主窗口 chrome 画成功但 list control 没 populate, 整个 body 空白。

**调查** (Phase B 开始时):
1. UserDictionary.cpp OnCreate L1232 `PopulateListImpl(s_hList)` 调用 — 看 s_yamlPath + LoadYaml 路径
2. UserDictionary.cpp s_yamlPath 默认 `<APPDATA>\Rime\user_dict.yaml` — 检查装机后 YAML 是否存在 + 内容是否合法
3. PopulateListImpl 是否处理 empty YAML (没 entries 时不显示? 还是显示空 list?)
4. UserDict::Show() 是否在 WS_EX_LAYERED 上有 first-paint 路径缺失

**修复方向** (Phase B 时定):
- YAML 文件不存在时自动创建 (with default empty entry)
- PopulateListImpl 处理 empty list — 显示 "No entries yet" placeholder
- Show() 路径强制 first RepaintLayered call

### B3. UserDict 列表 populate 完整功能 (Phase B 后期)

- 4 列 text/code/weight/schema 完整显示
- 搜索框过滤
- 添加 / 删除 / 导入 / 导出 / 部署 按钮全部 wired
- Weight 滑块 + schema 下拉框

---

## Phase C: 快捷键模块 — 下一环节 (User directive)

### C1. ShortcutSettings 已基本 work (cd6f61a9 接通), 只需验证 + polish

- 验证 Ctrl+Shift+K → ShortcutSettings 调出
- ShortcutSettings::Show() 路径完整
- 表格 5 列显示 (按键 / 模式 / action / 中文描述 / 修改状态)
- 搜索框 / 添加按钮 / 保存按钮 wired

### C2. Ctrl+Shift+U / Alt+/ / Alt+. 三 hotkey 整体验证

- Ctrl+Shift+U → UserDictionary (Phase B 后修)
- Alt+/ → UserDictionary (cd6f61a9 自己确认 "Bug 3b" - 错的, 应是 Phrase; Phase A 顺便修这个错路由)
- Alt+. → Phrase (Phase A 修)

---

## 文件改动总览

| Phase | 文件 | 改动 |
|---|---|---|
| A1 | WeaselServer/WeaselServerApp.cpp | L282 lambda `explore(install_dir)` → `PhrasesDialog::Show()` |
| A2 | WeaselServer/WeaselServerApp.cpp | stderr log 增强 (warning 时机记录); Phase B 才加 GUI |
| A3 | WeaselServer/QuickPanelDialog.{h,cpp} | 加 `DrawIconUserDict` 声明 + 实现 + PaintOpaqueContent case 2 改用 |
| A4 | test/v0_19_0_32_e2e/v0_19_0_32_e2e.cpp | 新增 3 类测试 (T_QP_Phrase_RealCallback / T_AltDot_Register_Path / T_QP_Icon_Unique) |
| A5 | release/fluxing-0.19.0.33-installer.exe | rebuild with v0.19.0.33 binary (= post-Phase-A source) |
| B2 | WeaselServer/UserDictionary.cpp | empty YAML handling + first-paint 路径 |
| B3 | WeaselServer/UserDictionary.cpp | 4 列 + 搜索 + 按钮 + weight slider 完整 |
| C1 | WeaselServer/ShortcutSettings.cpp | 验证 + polish |

---

## 关键文件引用

- `F:\soft\00selfmade\rime_claude\WeaselServer\WeaselServerApp.cpp` L268-293 (Show lambda)
- `F:\soft\00selfmade\rime_claude\WeaselServer\QuickPanelDialog.cpp` L547-567 (click routing), L893-905 (icon switch), L1143-1230 (Show())
- `F:\soft\00selfmade\rime_claude\WeaselServer\QuickPanelDialog.h` L230-237 (DrawIcon declarations, 仅 5 个)
- `F:\soft\00selfmade\rime_claude\WeaselServer\resource.h` L37-48 (ID_HOTKEY_* 定义)

---

## Verification philosophy

按 L100+ lessons learned: 不能"4 项 sandbox PASS = ship 成功"。

每个 Phase ship 前必须:
1. sandbox e2e 全 PASS
2. **真 user flow 测试** (启动 WeaselServer + SendMessage WM_HOTKEY + EnumChildWindows)
3. **user 装机手测** 一次 + 报告 OK

UserDict "界面空白" 这类 e2e sandbox 不能完全覆盖的 bug, 必须 Phase B 装机手测 + e2e 加 YAML 解析断言.

---

## 状态

- **不执行代码修改** (按 user directive)
- task.md plan 已写
- 等 user 确认 Phase A 范围 + 顺序

## Tasks
- Phase A.1: 修 onPhrases callback (WeaselServerApp.cpp L282)
- Phase A.2: 增强 Alt+. stderr log (WeaselServerApp.cpp L98-102)
- Phase A.3: 加 DrawIconUserDict (QuickPanelDialog.h/cpp + PaintOpaqueContent case 2)
- Phase A.4: 新增 3 类 e2e 测试 (v0_19_0_32_e2e.cpp)
- Phase A.5: 真 user flow 验证 (PowerShell script)
- Phase A.6: user 装机手测 + 报告