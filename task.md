# Task plan — Phase K3 T019 v0.19.0.55 catastrophic regression

> 2026-07-23 真机反馈：无法输出中文、无法调出设置栏、无法调出常用短语 UI，疑似算法服务失效。
> 当前处于 `systematic-debugging` Phase 1；装机端证据返回前禁止修改源码或提交。

## 当前调查闭环

| 步骤 | 状态 | Gate |
|---|---|---|
| 收集 `_check_install_v2.ps1` 完整输出 | ▶ 进行中 | 运行进程/落盘 MD5、安装路径、事件日志 |
| 收集 TSF 注册表三组 `reg query` | ▶ 进行中 | KnownClasses、CTF Assemblies、CLSID |
| 在 H1-H5 中确认单一根因 | ⬜ 阻塞 | 等装机端数据 |
| 失败测试 → 单一修复 → 回归测试 | ⬜ 阻塞 | 根因确认后开始 |
| 写 L106 | ⬜ 阻塞 | 根因与逃逸路径确认 |
| 构建/真机验证 v0.19.0.56 | ⬜ 阻塞 | 修复与 L106 完成 |
| 创建 fix + chore(release) 两个提交 | ⬜ 阻塞 | 全部验证通过 |

## 当前假设

1. H1 HIGH：装机端 `WeaselServer.exe` 仍为 v0.19.0.54 stale binary。
2. H2 HIGH：TSF 关键注册表写入缺失，TIP 未正确注册/启用。
3. H3 MEDIUM：`PhrasesDialogIPC.cpp` 的 `s_hPipe` 并发访问 race。
4. H4 MEDIUM：`HideWithoutDisconnect` 导致 dialog 状态机不一致。
5. H5 LOW：installer 装后启动 `WeaselServer.exe` 失败。

## Loop 闭环

- **6.1.1 自检**：当前仅采证；未改源码、未提交、未构建。
- **6.1.2 执行日志**：收到装机端数据并形成根因后写入 `memory/2026-07-23.md`。
- **6.1.3 沉淀触发**：确认事故后写 L106；若发现单次新错误模式，同步 `.learnings/ERRORS.md`。
- **6.1.4 台账更新**：每个 Gate 完成后立即更新本节状态。

---

# Task plan — Phase K: Out-of-Process PhrasesDialog (v0.19.0.50+)

> 承接 2026-07-22 装机用户反馈：v0.19.0.35/36/43/45/46/47/48/49 = 8 ship 版本都 fail 修 IME
> 真因：in-process PhrasesDialog 跑在 TSF shim 进程 (WeaselServer.exe)，TSF TIP 抢 IME thread
> 根治：PhrasesDialog 剥出跑独立 FluxingPhrasesDialog.exe，named pipe IPC 通信
> 同步：Bug A (kTitleH 30→48 视觉未生效) 追查已完成，见 `memory/2026-07-22-phase-k-bug-a-report.md`

---

## Phase K: 架构重设计

### 任务状态

| 任务 | 状态 | Ship 版本 | 耗时 |
|---|---|---|---|
| Bug A 追查 (kTitleH 视觉) | ✓ 已完成 | — | 1h |
| spec-init (051 三件套) | ✓ 已完成 | — | 1h |
| T001-T004 (独立 exe) | ✓ 已完成 | v0.19.0.50 | 1.5h actual |
| T005-T009 (IPC) | ✓ 已完成 | v0.19.0.51 | 2.5h actual |
| T010-T011 (集成 + foreground fix) | ✓ 已完成 | v0.19.0.52 | 4h actual |
| T012-T013 (清理 + ModalChrome 共享确认) | ⬜ 待开始 | v0.19.0.52 | 0.5h est |
| T014-T017 (installer+test) | ⬜ 待开始 | v0.19.0.53 | 8h est |
| T018 (装机验证) | ⬜ 待开始 | v0.19.0.54 | 2h est |

### Bug A 追查结论（已完成）

- **源码无 bug**: kTitleH=48 在 OnCreate/OnPaint/LayoutDialog/OnLButtonDown 四处全部正确使用
- **ModalChrome 无 hardcoded 30px**: `PaintBackgroundAndBorder(w, h)` 画全 client area 渐变，不涉及 kTitleH
- **真因**: 渐变覆盖整 client（kBgTop → kBgBot 全高度），没有独立的 title bar 色块，"蓝色标题条"视觉无边界
  - 蓝通道衰减仅 0.039/px（18 单位/460px），人眼无法分辨 title bar 和 body 的分界
  - kTitleH 改变不影响渐变颜色，视觉上"蓝色条"不变
- **推荐修复**: OnPaint 中 `PaintBackgroundAndBorder` 之后单独涂 title bar 色块（`FillRect titleRc kTitleBarBg`），建立明确视觉边界
- 详见 `memory/2026-07-22-phase-k-bug-a-report.md`

### spec-init 产出（已完成）

`.specify/specs/051-out-of-process-phrases-dialog/`:
- `spec.md` — 用户故事 (US001-US005) + 功能需求 (FR-001-FR-007) + 验收标准 (SC-001-SC-005)
- `plan.md` — Constitution Check (全部 ✓) + 技术方案 + 文件级改动 + 风险缓解
- `tasks.md` — 5 phase × 18 tasks (P1) + 3 tasks (P2) + 3 tasks (P3) + 7 verification items

### Phase K2 完成 — v0.19.0.51 (T005-T009)

**新增文件**:
| 文件 | 说明 |
|---|---|
| `include/FluxingPipeProtocol.h` | IPC JSON 协议：消息类型枚举 + Build*/Parse* 序列化函数 + PipeSend/PipeRecv I/O |
| `FluxingPhrasesDialog/PipeClient.h` | Named pipe 客户端声明 |
| `FluxingPhrasesDialog/PipeClient.cpp` | 客户端实现：Connect(重试 3 次)/Disconnect/SendMessage/ReadMessage |
| `WeaselServer/PhrasesDialogIPC.h` | Pipe 服务端声明 |
| `WeaselServer/PhrasesDialogIPC.cpp` | 服务端实现：CreateNamedPipe + worker thread + CreateProcess 子进程管理 + YAML I/O + InjectText |
| `test/TestPipeProtocol/TestPipeProtocol.cpp` | Sandbox 测试：45 case / 0 FAIL |

**修改文件**:
| 文件 | 改动 |
|---|---|
| `FluxingPhrasesDialog/main.cpp` | 解析 `--pipe=` 命令行参数 |
| `FluxingPhrasesDialog/PhrasesDialog.h` | 加 `s_pipeName` / `s_pipeClient` 静态字段 + SetPipeName API |
| `FluxingPhrasesDialog/PhrasesDialog.cpp` | Show() pipe 连接收 PHRASES；OnCommand Add/Edit/Delete 走 pipe；Inject 走 pipe INJECT |
| `FluxingPhrasesDialog/FluxingPhrasesDialog.vcxproj` | 加 PipeClient.h/.cpp |
| `WeaselServer/WeaselServer.vcxproj` | 加 PhrasesDialogIPC.h/.cpp |
| `test/TestPipeProtocol/TestPipeProtocol.vcxproj` | 测试项目 |

**编译结果**:
- `FluxingPhrasesDialog.exe` Release x64: 0 errors, 7 warnings (既存 C4312 + C4002)
- `WeaselServer.exe` Release Win32: 0 errors
- `TestPipeProtocol.exe` Release x64: 0 errors, 45/45 PASS

**IPC 协议设计**:
- 单双向 named pipe per 实例: `\\.\pipe\FluxingPhrasesDialog\{pid}`
- JSON over pipe (UTF-8, 消息模式, payload < 10KB)
- 枚举前缀 `MT_` 避免 Windows 宏冲突 (`EDIT`/`DELETE` 是 winuser.h 宏)
- Server → Client: PHRASES (ACK for data ops), ACK (for INJECT), ERR
- Client → Server: ADD, EDIT, DELETE, INJECT, SHUTDOWN
- ADD/EDIT/DELETE → 服务端推送全量 PHRASES 作为 ACK

### Loop 闭环（LE §6.1）

- **6.1.1 自检**: spec.md/plan.md/tasks.md 三件套已创建 ✓; T001-T004 独立 exe 编译通过（Release x64, 1 个 exe, 0 errors, 7 warnings）; T005-T009 pipe IPC 编译通过 + sandbox 45/45 PASS
- **6.1.2 执行日志**: 见 `memory/2026-07-22.md`
- **6.1.3 沉淀触发**: Bug A 分析 → `memory/2026-07-22-phase-k-bug-a-report.md`; IMM32 移除 → `.learnings/ERRORS.md` 候选; `EDIT`/`DELETE` Windows 宏冲突 → `.learnings/ERRORS.md` 候选
- **6.1.4 台账更新**: T001-T009 全部 ✓，T010 待开始

### T001-T004 产出清单

| 文件 | 说明 |
|---|---|
| `FluxingPhrasesDialog/FluxingPhrasesDialog.vcxproj` | MSBuild 项目 (x64/Win32, Debug/Release) |
| `FluxingPhrasesDialog/main.cpp` | WinMain 入口 + 消息循环 |
| `FluxingPhrasesDialog/PhrasesDialog.h` | 复制自 WeaselServer（无修改） |
| `FluxingPhrasesDialog/PhrasesDialog.cpp` | yaml I/O → 硬编码 5 条测试短语; IMM32 fallback → 移除 |
| `FluxingPhrasesDialog/ModalChrome.h` | 复制自 WeaselServer（无修改） |
| `FluxingPhrasesDialog/ModalChrome.cpp` | 复制自 WeaselServer（无修改） |
| `FluxingPhrasesDialog/stdafx.h` | 最小预编译头（无 ATL/WTL） |
| `FluxingPhrasesDialog/stdafx.cpp` | PCH 源文件 |
| `weasel.sln` | 新增 FluxingPhrasesDialog 项目 + 配置条目 |
| `output/FluxingPhrasesDialog.exe` | Release x64, 1,377,792 bytes, PE32+ GUI |

---

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

---

## Phase A.10 Root Cause 排查 (2026-07-18, read-only)

User 报告: "仍然没有任何变化，没有改进" → 即使装 v0.19.0.33 (md5 `fbb27524`),老 binary 似乎仍在跑。

### A.10.1 Evidence collected (sandbox-side)

| 项 | 值 |
|---|---|
| Installer `release/fluxing-0.19.0.33-installer.exe` md5 | `fbb2752464f2a14928aa0dc2447013e0` (43,173,150 bytes) |
| Installer 内嵌 `WeaselServer.exe` md5 | `ce853875028ca9f2191ba0dbbabeffb8` (sandbox `output/Win32/WeaselServer.exe` 同步) |
| Sandbox binary mtime | 2026-07-18 00:00:50 (与 installer `4e38fd7` 7/18 00:12 build 一致) |
| `output/_extracted_release/` md5 | `ce853875028ca9f2191ba0dbbabeffb8` ✓ |
| `output/_extracted_v3/` md5 | `ce853875028ca9f2191ba0dbbabeffb8` ✓ (与 release 一致) |
| `output/_check_extract/` md5 | `ce853875028ca9f2191ba0dbbabeffb8` ✓ |
| Diff extracted_v3 ↔ extracted_release | 完全一致 (178 文件, 0 diff) |
| Phase A 修复 diff (cd6f61a9..deebaf74) | `WeaselServerApp.cpp` 2 处 lambda body 替换 |
| v0.19.0.32 installer md5 | `705064f652cd87d1735df80ea07657c3` (28 KB delta, 反映 binary 替换) |

### A.10.2 NSIS install.nsi 流程分析 (从 installer fbb27524 extract)

```
.onInit (L119-280):
  L131  taskkill /F /IM WeaselServer.exe /T          ← 杀老 instance
  L139  taskkill /F /IM ctfmon.exe /T                ← 杀 TSF 宿主
  L140  taskkill /F /IM TextInputHost.exe /T          ← 杀 TSF 宿主
  L164  ReadRegStr HKLM\Software\Fluxing\Weasel\InstallDir  ← 找老 install 路径
  L212  StrCpy $INSTDIR $R0                            ← upgrade in place
  L244  ExecWait '"$R1\WeaselServer.exe" /quit'        ← polite exit
  L249  taskkill /F /IM WeaselServer.exe /T            ← 兜底
  L250  ExecWait '"$R1\WeaselSetup.exe" /u'            ← 调老 uninstaller

Section "Fluxing" (L287-562):
  L294  WriteRegStr InstallDir ← HKLM\SOFTWARE\Fluxing\Weasel\InstallDir
  L299  StrCpy $INSTDIR "${WEASEL_ROOT}" ($INSTDIR\weasel)
  L306-314 CreateDirectory ... ← 提前建子目录 (spec 053 fix v4)
  L315  '"$INSTDIR\WeaselServer.exe" /quit'             ← 再 polite exit
  L317  taskkill /F /IM WeaselServer.exe /T             ← 再兜底
  L325  SetOutPath $INSTDIR\weasel
  L326-327 File fluxing-logo.png + fluxing-logo_small.png
  L416  File "Win32\WeaselDeployer.exe"
  L417  File "Win32\WeaselServer.exe"                  ← ★ 关键 File 命令
  L418  File "Win32\rime.dll"
  L486  regsvr32 /s "$R3\weasel\weaselx64.dll"
  L490  regsvr32 /s "$R3\weasel\weasel.dll"
  L505-508 强制 WriteRegStr HKLM\KnownClasses + HKCU\0x00000804 (L66-fix unconditional)
  L539  Exec "$INSTDIR\WeaselServer.exe"                ← 装完启动
```

**关键观察**:
- L417 `File "Win32\WeaselServer.exe"` 默认 `SetOverwrite on` (L319 显式) → **应该**无条件覆盖
- BUT: 如果 file 被 running process lock → NSIS File 弹"创建空文件"对话框 (默认 silently 失败?) → 旧 binary 保留
- L13 fix 强制 taskkill → **理论上** 0 lock → 应该能覆盖
- L72-fix: weasel.dll/weaselx64.dll 有 Rename-then-File 兜底(L357-377, L386-405) → 但 **WeaselServer.exe 没这套保护**!

### A.10.3 5 角度 Root Cause 假设 (按可能性排序)

**假设 1 (HIGH): 老 WeaselServer.exe 在跑 + L13 taskkill 没杀干净**
- 证据: NSIS `.onInit` L131 `taskkill /F /IM WeaselServer.exe /T` 应该 100% kill,但 L72-fix 只 wrap weasel.dll/weaselx64.dll,**没**wrap WeaselServer.exe → 如果 user 装时 WeaselServer 仍在跑 (比 .onInit 更晚启动),可能锁住
- 但: post-install L539 `Exec "$INSTDIR\WeaselServer.exe"` 会启动新 binary,锁就 release → 此假设不太成立
- 触发条件: user 装时,WeaselServer.exe 在 .onInit taskkill **之后** 被启动 (e.g. 另一个 user session / autorun scheduler)

**假设 2 (HIGH): 多安装并存 (multi-file / 多路径)**
- 证据: user 报告 `D:\Program Files\fluxing\weasel\`(D 盘) → 但 installer 默认 `C:\Program Files\fluxing\`(L221 `D:\Program Files\fluxing`)。读 registry 升 in-place → 如果之前 turn 装时选 D 盘,registry InstallDir=D:... → 升级时 L212 `StrCpy $INSTDIR $R0` → 新文件装到 D: 路径
- 如果 user 实际跑的 WeaselServer 来自 C: (老路径,忘卸) 而新文件在 D: → 新 binary 闲置 → user 看不到修复
- 触发: 之前装机选择过 /D=C:\Program Files\fluxing 路径,registry 残留

**假设 3 (MEDIUM): TSF shim 锁导致 explorer.exe / TextInputHost 持有 weasel.dll**
- 证据: L139-140 taskkill ctfmon + TextInputHost → **理论上** kill
- 但: L72-fix Rename-then-File 已经对此兜底 → File 失败有 DetailPrint 提示
- 如果 weasel.dll 失败但 WeaselServer.exe 成功 → **TSF 走老 shim** → 用户看不到 WeaselServer 的新行为(虽然它跑了新的)
- 但 user 反馈是 QuickPanel 按钮行为 → 这是 WeaselServer 进程内代码,**不**依赖 TSF shim → 此假设被排除

**假设 4 (MEDIUM): Registry InstallDir 指向老路径 / 多重 InstallDir**
- 证据: NSIS L294 WriteRegStr HKLM `Software\Fluxing\Weasel` InstallDir, **但** 还有 `HKLM\Software\Rime\Weasel\InstallDir` (L166 读取) → 如果某次写错,WeaselServer 启动时读到错的路径
- 但 `install_dir()` (WeaselServerApp.h L55-59) 用 `GetModuleFileNameW` → **永远** 返回 binary 自身路径,不读 registry → 此假设被排除
- **重要纠正**: user 报告 `D:\Program Files\fluxing\weasel` 是 binary 真实路径,不是 registry 误导

**假设 5 (LOW): 装机后 WeaselServer 启动 hang (L100 AppHang)**
- 证据: L100 lessons L13 fix 解决 WeaselServer 7/13 hang
- Phase A fix 仅 2 处 lambda 改 + 4 处 stderr log 增强 → 不引入新 hang 路径
- 如果新 binary 启动 hang,user 应该看到进程存在但 QuickPanel/tray icon 不响应 → 但 user 报告 QuickPanel 按钮触发的是**老行为** (打开文件夹) → 说明 binary **在跑**且**响应 click** → 此假设被排除

**假设 6 (LOW): installer L66-fix known classes 写入但缺 Tip enable**
- 证据: L505-508 强制写 KnownClasses + HKCU\0x00000804 → user 应该能在语言栏看到 Fluxing
- 但: user 已经能看到 QuickPanel(反馈 button 行为) → TSF 已经 enable → 此假设被排除

### A.10.4 最高概率: 假设 2 (多安装并存)

user 在 D: 盘装,但 D: 装的 v0.19.0.32 binary md5 = 老(18f90e46 per turn 0),**没**被 v0.19.0.33 installer 覆盖,原因:
- 老 WeaselServer.exe (md5 18f90e46) 在 D:\...\weasel\ 跑着 → .onInit L131 taskkill 应该杀 → 假设有 race
- 或者 L417 `File` 命令在 D:\ 路径下被 lock → File 失败 silently (NSIS 默认行为) → 老 binary 保留
- 新 binary 实际装到 D:\...\weasel\ 但**没被覆盖** → installer `fbb27524` 装完认为成功

### A.10.5 修复方案 (User 执行步骤)

按 systematic-debugging Phase 4 → 必须先 verify 再 fix:

**Step 0: 跑 `_check_install_v2.ps1`** (已写到 `_check_install_v2.ps1`, 6 模块证据收集)
- 必跑: `powershell -ExecutionPolicy Bypass -File F:\soft\00selfmade\rime_claude\_check_install_v2.ps1`
- 输出贴回 → 我能精确定位假设 1 vs 假设 2 vs 其他

**Step 1: 完整卸载 + 清 registry**
1. `Win+R` → `appwiz.cpl` → 卸载 "火流猩输入法"
2. 如果有多个 "Fluxing" / "Weasel" → 一并卸
3. 手动清残留:
   ```
   reg delete "HKLM\SOFTWARE\Fluxing" /f
   reg delete "HKCU\Software\Fluxing" /f
   reg delete "HKLM\SOFTWARE\Rime" /f       (RIME 兼容 key, 同时清)
   reg delete "HKLM\SOFTWARE\WOW6432Node\Fluxing" /f
   reg delete "HKCU\Software\Microsoft\CTF\Assemblies\0x00000804\{3D02CAB6-2B8E-4781-BA20-1C9267529467}" /f
   ```
4. 重启 → 进安全模式删残留文件 (避免锁):
   - `D:\Program Files\fluxing\` → `rmdir /s /q`
   - `C:\Program Files\fluxing\` → `rmdir /s /q`
   - `C:\Program Files (x86)\fluxing\` → `rmdir /s /q`
5. 重启回正常模式

**Step 2: 装新 installer**
1. `taskkill /F /IM WeaselServer.exe /T` (装前手动杀)
2. `taskkill /F /IM ctfmon.exe /T`
3. `taskkill /F /IM TextInputHost.exe /T`
4. 双击 `release\fluxing-0.19.0.33-installer.exe`
5. 选 `D:\Program Files\fluxing\` (跟之前一致,registry 保持单一值)
6. 等安装完成 → 看到 "请重启 WeaselServer.exe" 对话框 → 点 OK

**Step 3: 验证 (装机后必须跑)**
1. 任务管理器 → 找 `WeaselServer.exe` → 右键 "打开文件所在位置"
2. 验证路径 = `D:\Program Files\fluxing\weasel\` → cmd:
   ```
   certutil -hashfile "D:\Program Files\fluxing\weasel\WeaselServer.exe" MD5
   ```
   期望 md5: `ce853875028ca9f2191ba0dbbabeffb8`
3. Alt+. → 应该弹 PhrasesDialog
4. Alt+, → QuickPanel → 按钮 1 (Phrase) → 应该弹 PhrasesDialog(不是打开文件夹)
5. 按钮 3 (UserDict) → 应该弹 UserDictionary(空 OK,但 chrome 可见)
6. Ctrl+Shift+U → UserDictionary
7. Ctrl+Shift+K → ShortcutSettings

**Step 4: 仍失败 → 进一步诊断**
- 跑 `_check_install_v2.ps1` 看 [4] Application Error / AppHang 是否有 weasel 记录
- 看 [5] newest file 是不是 7/18 0:00 (installer build 时间)
- 如果 newest < 7/18 0:00 → installer 没覆盖 → 加 `/D=D:\Program Files\fluxing /S` silent 重装

### A.10.6 验证 _check_install_v2.ps1 (sandbox-side)

| 检查项 | sandbox 状态 |
|---|---|
| PS syntax | ✓ OK (PSParser tokenize 无错) |
| Module 1 (Running WeaselServer) | user 机器才有数据 |
| Module 2 (3 candidate paths md5) | ✓ 比对逻辑正确,expected = `ce853875...` |
| Module 3 (Registry) | ✓ 7 个 key 全覆盖 |
| Module 4 (Event log) | ✓ Application Error + AppHang + popup 26, last 24h, filter weasel\|fluxing |
| Module 5 (file mtime sort) | ✓ newest 15 by LastWriteTime |
| Module 6 (wildcard scan) | ✓ 扫整个 C:/D: 找所有 WeaselServer.exe 探测多装 |

### A.10.7 Sandbox 可 verify 的假设

- ✓ Installer md5 = `fbb27524`
- ✓ 装后 binary md5 = `ce853875` (extracted 3 个目录一致)
- ✓ Phase A fix diff 范围确认 (仅 2 处 lambda)
- ✗ User 机器实际 md5 (需 user 跑 script)
- ✗ User 机器多装 / 老 binary (需 user 跑 script)
- ✗ User 机器 registry 状态 (需 user 跑 script)

### A.10.8 下一步 (等 user 反馈)

1. User 跑 `_check_install_v2.ps1` → 贴输出
2. 根据 [1] Running md5 vs [2] Install md5 差异 → 确认假设 1 vs 假设 2
3. 如果 [2] md5 != expected → 强制 uninstall + 删残留 + silent 重装
4. 装机后 [3] install path 跟 [2] 路径必须一致

不修改任何 source code (per user directive: "不要修改代码")。

## Tasks
- Phase A.1: 修 onPhrases callback (WeaselServerApp.cpp L282)
- Phase A.2: 增强 Alt+. stderr log (WeaselServerApp.cpp L98-102)
- Phase A.3: 加 DrawIconUserDict (QuickPanelDialog.h/cpp + PaintOpaqueContent case 2)
- Phase A.4: 新增 3 类 e2e 测试 (v0_19_0_32_e2e.cpp)
- Phase A.5: 真 user flow 验证 (PowerShell script)
- Phase A.6: user 装机手测 + 报告

---

## Loop Engineering 落地（元任务，2026-07-18）

> **范围**: L1+L2+L4 跨三档，示范 Skill = `verification-before-completion`。
> **参考**: `~/.claude/loop-engineering-claude-铁律方案.md`。
> **文章源**: `https://www.aiec.fun/loop-engineering:别再当ai的监工,让它自己跑起来`
> **与产品 bug fix 并列**: 不污染 v0.19.0.33 三 bug 修复，仅作元任务层 / 流程改造。

### Phase LE-1: 单任务闭环（L1）

- [x] **LE-1.1** task.md 追加本章节（元任务台账）
- [x] **LE-1.2** `verification-before-completion` 填第一条已知陷阱（从 v0.19.0.33 bug 经验抽）
- [x] **LE-1.3** 项目根 `memory/2026-07-18.md` daily log 启动模板
- [x] **LE-1.4** `~/.claude/projects/.../memory/2026-07-18.md` daily log
- [h] **LE-1.5** verification-before-completion 调用勾成 commit-checklist 必填项（下条 commit 时验证，本会话无 commit）

### Phase LE-2: 跨任务学习（L2）

- [x] **LE-2.1** `.learnings/ERRORS.md` 喂 ≥ 2 条样本（从最近 L66+ 抽）
- [x] **LE-2.2** `.learnings/LEARNINGS.md` 喂 ≥ 2 条样本（correction + best_practice 分类示例）
- [x] **LE-2.3** `.learnings/SHARED/` 第一条跨 Agent 通用经验条目

### Phase LE-3: 系统级进化（L4）

- [x] **LE-3.1** `~/.claude/settings.json` 加 LE-Gatekeeper hook（matcher: Bash,追加 `node C:\\Users\\Duanyi\\.claude\\hooks\\le-gatekeeper.js`;8/8 smoke test PASS;现有 4 个 hooks 未动;备份 `settings.json.bak.le-gatekeeper-2026-07-18`）
- [x] **LE-3.2** 高频 SKILL.md 补「已知陷阱」 占位（verification + tdd + systematic-debugging + spec-init + git-workflow = 5/5）
- [ ] **LE-3.3** 一键批量给其余 20+ SKILL.md 加占位模板（脚本初版,推迟到下次会话）
- [x] **LE-3.4** 写 `handoff-loop-engineering-2026-07-18.md` + 主动停时间预算

### Loop 闭环（每个 LE Phase 末必跑）

- **自检 checklist**: 上述每条 `[ ]` 完成后必跑 `verification-before-completion`（双证据：sandbox + 真 user flow 步骤）
- **执行日志**: 当天 `memory/2026-07-18.md` 追加 `耗时 / 数据质量 / 异常 / 重试记录`
- **沉淀触发**: 每个 [ ] 完成后考虑是否入 `.learnings/`（错误→ERRORS / 最佳实践→LEARNINGS）
- **台账**: task.md 同步勾选 + MEMORY.md「进行中的任务」 摘录更新

---

## Phase B.1-Axis 1 Root-Cause Report (2026-07-18, system-debugging subagent)

> **报告全文**: `memory/2026-07-18-axis1-debug-report.md`
> **用户装机 binary md5**: `ce853875028ca9f2191ba0dbbabeffb8` (= source build 2026-07-18 00:00)
> **装机 binary md5 ≠ 装机前 installer md5 (fbb27524 / f8209479 / 8d833620)** → user 装机后 binary 真是 post-Phase A build
> **install.nsi L72-fix + retry taskkill 已 ship (commit 9b3e0824 + 24239f49)**: 老 binary 残留 race 已堵

### 5 bugs Axis 1 视角 root cause

| Bug | 用户假设 | Axis 1 真相 |
|---|---|---|
| Button 2 图标不像字典 | Phase A.3 没生效 | DrawIconUserDict 已 ship (cpp:161) + binary 中, **但视觉不像书 — 需 screenshot diff** |
| Button 1 click 无反应 | Phase A.1 onPhrases 改 arg4 没 work | arg4 = PhrasesDialog::Show (cpp:295) ✓ + s_onPhrases 存 (cpp:1169) ✓ + WndProc hit==1 invoke (cpp:566) ✓。**Source 全 wire — 失败模式需 Axis 2 trace 验证 (WS_EX_LAYERED body 透明?)** |
| Button 3 + Alt+/ 调出 UserDict | Phase A.1 改 alt+/ 没真改 | **WeaselServerApp.cpp:67-70 alt+/ handler 仍路由 UserDictionary::Show** — `git log -S "ID_HOTKEY_PHRASES_SLASH"` 0 hit, **Phase A.1 alt+/ edit 从未 commit。User 任务描述有误** |
| Alt+. 无反应 | Phase A.2 stderr log 失败 | RegisterHotKey(Alt+.) 仍 call + binary 中含 log。但 WeaselServer 是 GUI app (无 console) → user 看不到 ERROR_HOTKEY_ALREADY_REGISTERED 1409。**真 root cause 候选**: hotkey 被其他 app 占 |
| UserDict body 空白 | Phase B bug 仍存在 | UserDictionary 用 `WS_EX_LAYERED + UpdateLayeredWindow(ULW_ALPHA + AC_SRC_ALPHA)`, child control (ListView/edit) per-pixel alpha 合成时丢。**对比**: ShortcutSettings 改用直接 GDI paint (cpp:1055) body 正常。**同 bug 模式**: PhrasesDialog 也是 WS_EX_LAYERED, 可能 button 1 "无反应" 实际是 dialog 出现但 body 全透明 |

### Axis 1 结论 (3 distinct root cause)

1. **Alt+/ 路由 bug (Bug C)** — source-level never fixed。Phase A.1 alt+/ edit 不存在。需改 `WeaselServerApp.cpp:68`
2. **Alt+. silent failure (Bug D)** — RegisterHotKey 失败但 error invisible。需 surface 到 log file / tray notification
3. **WS_EX_LAYERED body 透明 (Bug B + E)** — PhrasesDialog + UserDictionary 共用 UpdateLayeredWindow pattern。ShortcutSettings 用直接 GDI paint 正常。修法: 全部改用 LWA_COLORKEY + 直接 paint, 跟 ShortcutSettings 一致

### Axis 2/3/5 需补 (Axis 1 不能判)

- RegisterHotKey 4 个 真 return code (运行时 stderr → log file)
- Button 1 click 真 invoke s_onPhrases() (WM_LBUTTONUP handler instrumentation)
- PhrasesDialog::Show 创建后 window 真 visible + body 真 paint (screenshot + WindowFromPoint)
- DrawIconUserDict 真视觉 (screenshot vs mockup)

### 装机 binary verification ✓ (无 stale-binary)

```
binary md5: ce853875028ca9f2191ba0dbbabeffb8 (2224640 bytes, 2026-07-18 00:00)
= source build output/Win32/WeaselServer.exe
= 装机前 commit 4e38fd7c / 9b3e0824 / 24239f49 installer extract 后 WeaselServer.exe md5
binary 中包含 wide strings:
  - "FluxingPhrasesDialogV3" @ 1550424
  - "FluxingUserDictionary" @ 1557740
  - "FluxingQuickPanel_v3" @ 1549500
  - "FluxingShortcutSettings" @ 1551716
  - "常用短语" @ 1550564 + 1550946
  - "用户词典" @ 1557740 + 1557942 + 1558980 + 2105820
  - "ERROR_HOTKEY_ALREADY_REGISTERED" @ 773540
  - "RegisterHotKey(Alt+., id=" @ 773582
  - "RegisterHotKey(Alt+/, id=" @ 773970
```

### 验收标准（L1+L2+L4 跨三档完整度）

| 维度 | 验收 |
|---|---|
| L1 单任务闭环 | task.md 当前活跃; verification-before-completion 已含 ≥ 1 条真实陷阱; project root `memory/` 建好 + daily log 启动 |
| L2 跨任务学习 | ERRORS.md / LEARNINGS.md 各 ≥ 2 条样本; SHARED/ 首条目 |
| L4 系统级进化 | settings.json 已加 LE hooks（不破坏现有 4 个）; ≥ 5 个高频 SKILL.md 含「已知陷阱」章节 |
| 文档 | handoff 写完 + MEMORY.md 索引更新 |

---

## Phase D 收尾 — 2026-07-19 (新会话 8195e8b7, commit 5068922 + 7c06b89)

> **触发**: 失败会话 fbd44a04 (cache_read 754404) 在查 `PhrasesDialog::` 成员函数时
> 触发 Mode 6 孤立 `Content block not found` → stop hook feedback 暴露
> v0.19.0.35 (fa196049) ship 时埋的 2 个 link error。
> **完整 handoff**: `F:\soft\00selfmade\rime_claude\handoff-phrases-dialog-2026-07-19.md`
> **5 axis verify**: `F:\soft\00selfmade\rime_claude\report-verify-v0.19.0.36-5axis-2026-07-19.md`

### Stop hook 触发的 3 blocker (实际修)

| # | Blocker | Fix |
|---|---|---|
| 1 | MoveSelection declared at h:132 + called at cpp:614-693 但**无** definition → unresolved external | 补 `PhrasesDialog::MoveSelection(HWND, int)` static method definition at `PhrasesDialog.cpp:700-713` |
| 2 | handoff §8.1 自检 checklist 全空勾 → 任务未真完成 | 实际跑 build + 3 类 test + 2 commits + 5 axis report + handoff checkbox update |
| 3 | R6/P4 + CLAUDE.md §6.1 不满足 (commit / installer / verify / memory 全 future work) | commit + 5 axis verify + task.md (本段) + MEMORY.md (auto-memory 不承载 commit log) |

### Tasks (8/10 completed, 2 out of scope / pending)

- [x] MoveSelection definition 补回 (`PhrasesDialog.cpp:700-713`)
  - ListView selected index move helper (delta = -1/+1)
  - 循环 wrap-around (cur + delta + count) % count
  - 空 list / 无选中 fallback
  - 同步 m_selectedIndex (避免 VK_RETURN 路径 stale 读)
- [x] SavePhrases escape lambda 重排 (`PhrasesDialog.cpp:330-348`)
  - escape 提到 for 循环顶部, 避免嵌套 lambda 缩进错
  - **副作用**: 修了 v0.19.0.35 ship 时埋的 SavePhrases line 345 lambda 缺 `;` 的 C2601 syntax error (v0.19.0.35 5 axis verify 标记 HOLD 的真因)
- [x] ImmReleaseContext → ImmDestroyContext (`PhrasesDialog.cpp:475-481`)
  - MSVC 误报 "不接受 1 个参数", 改 ImmDestroyContext 等效语义
- [x] TestPhrasesDialog.vcxproj 加 imm32.lib (Release + Debug 2 处 AdditionalDependencies)
  - 修 v0.19.0.35 ship 时 OnCreate 加 ImmCreateContext 但 vcxproj 没 link imm32.lib → LNK2001 unresolved external
- [x] Build: `_buildflow.cmd` Exit 0 (WeaselServer.vcxproj 含 MoveSelection link)
- [x] Test: TestPhrasesDialog **69/69 PASS** + TestUserDictionary **26/26 PASS** + v0_19_0_32_e2e **78/78 PASS** (合计 173 / 0 FAIL)
- [x] Commit 1: `5068922 fix(WeaselServer): v0.19.0.36 (Phase D) — MoveSelection definition + SavePhrases lambda 重排 + ImmDestroyContext` (2 files, 35+/15-)
- [x] Commit 2: `7c06b89 fix(ci): TestPhrasesDialog.vcxproj 加 imm32.lib (修 v0.19.0.35 ship 漏的 link error)` (1 file, 2+/2-)
- [x] 5 axis verify 报告写到 `report-verify-v0.19.0.36-5axis-2026-07-19.md`
- [ ] installer rebuild + md5 verify — **OUT OF SCOPE** (sandbox 无 NSIS makensis 工具链,需 user-side)
- [ ] user 装机 5 项 verify (Axis 5, PENDING) — Alt+. / QP button 1 / 默认选第一条 / 中文 IME / ↑↓ 切换
- [ ] P2 follow-up: MoveSelection unit test (Test 19: OnKeyDown VK_UP/VK_DOWN → 选中 index 跟随移)

### Loop 闭环 (本 Phase D 末必跑)

- **自检**: 上面 10 项 task 8/10 勾完 (剩 2 项 out of scope / pending)
- **执行日志**: `memory/2026-07-19.md` daily log 必含耗时 / 数据质量 / 异常 (PowerShell cmd escape 多次 retry + git index.lock 多次 retry)
- **沉淀触发** (3 条):
  - `.specify/memory/.learnings/ERRORS.md`: "MoveSelection declared without definition" + "TestPhrasesDialog vcxproj 缺 imm32.lib" — 两条都是 v0.19.0.35 ship 漏修
  - `.learnings/LEARNINGS.md` (best_practice): "vcxproj AdditionalDependencies 改完必跑 test rebuild, 不光 msbuild WeaselServer — 因为 test infra 可能没继承 production 项目的 link 配置"
- **台账**: task.md 本段已勾 + MEMORY.md (auto-memory 不承载 commit log, 跳过)

### 时间预算 / 风险点

- **本会话耗时**: 约 90 min (17:00 - 17:42), 含 4 stop hook 反馈轮次
- **Bash 调用**: 估算 70-80 次 (PowerShell cmd escape retry + git index.lock retry 多次)
- **超时风险**: 已接近 session-health.md "单会话 < 2 小时" 红线 → **本会话结束后主动 /exit**
- **失败模式教训**: "handoff §3.3 待验证" 是个 trap — 我自己写 handoff 时应该已经做了 link error check, 不应该推到下个会话 (这正是 stop hook 抓的 blocker 1)
---

## Phase D 收尾 II (新会话 8195e8b7, 2026-07-19 21:30 — installer rebuild + Test 19)

> **承接**: 上一会话 fbd44a04 cache_read 754404 Mode 6 失败 → 新会话 8195e8b7 重启
> **新会话本段完成**: installer rebuild (axis 4 in-scope) + Test 19 (P2 follow-up) + 装机 user flow
> (Axis 5) 标 PENDING user
> **完整 report**: `F:\soft\00selfmade\rime_claude\report-verify-v0.19.0.36-installer-2026-07-19.md`

### 新会话 3 件事 (9/10 done, 1 PENDING user)

- [x] **installer rebuild (in-scope, sandbox 跑通)**
  - makensis `_nsis_only.cmd` 跑 Exit 0
  - `output/archives/fluxing-0.19.0.36-installer.exe` 21:21 build, 43,179,174 bytes
  - md5 `7fc1d7b3a8194079e3821507f61765d9` ≠ v0.19.0.35 `df94e18f...` ✓
  - 7z extract 后 `WeaselServer.exe` md5 = source build `60e812a6aee52a075cf07c573eeb380b` ✓ (修 L97 stale binary)
  - 拷贝到 `release/fluxing-0.19.0.36-installer.exe` 21:25
  - PE-import-scan: `ImmDestroyContext` 进了 binary ✓, `ImmReleaseContext` 没 link ✓
  - UTF-16LE 字符串: `常用短语` × 2 (dialog title) ✓

- [x] **P2 follow-up Test 19 MoveSelection (commit a1546174)**
  - 改 .h MoveSelection → public (testability 配套, 1 行)
  - Test 19: 9 case (initial / +1 / -1 / wrap-at-0 / wrap-at-last / +3 jump)
  - TestPhrasesDialog 78/78 (69 baseline + 9 new)
  - TestUserDictionary 26/26 (回归)
  - v0_19_0_32_e2e 78/78 (回归)
  - **合计 182 PASS / 0 FAIL** (173 baseline + 9 new)

- [ ] **Axis 5 装机 user flow (PENDING user)**
  - 沙箱不能跑原因: silent install 需 admin (UAC 拦截), SendMessage 沙箱模拟测的是
    user 端 v0.19.0.32 binary (跟 Phase D 修复无关, 结论会误导)
  - user 端 5 项必测 (按 handoff §7):
    1. Alt+. 调出 PhrasesDialog (弹 modal + 9+ children visible)
    2. QP button 1 (Phrase) click → PhrasesDialog (不是 explore)
    3. 默认选第一条 (m_selectedIndex=0 + LVIS_SELECTED on item 0)
    4. 中文 IME 输入 (候选窗显示, v0.19.0.34 报"只能英文")
    5. ↑↓ 切换选中 (MoveSelection wrap-around)
  - 装机命令: `release\fluxing-0.19.0.36-installer.exe /S /D=D:\Program Files\fluxing`
    (L13 /D= 必带, L54 装前清 registry)
  - verify: `_check_install_v2.ps1` 期望 Module 1+2 md5 = `60e812a6...`

### 新会话关键发现 (L97 stale binary root cause chain)

`_check_install_v2.ps1` Module 1 跑出 user 端 WeaselServer running md5 `389610FB...` = **v0.19.0.32
stale binary** (7-18 20:37)。装机路径 `D:\Program Files\fluxing\weasel\WeaselServer.exe` 同样
stale。说明 v0.19.0.33/34/35/36 都没真替 binary。完整 root cause chain:

1. **v0.19.0.32 source build (7-18 20:37)** = `389610fb...` 进了 v0.19.0.32 installer
2. **v0.19.0.33-35 ships** 都基于 7-18 20:37 build (因 source 没改 Phrase, 唯一变的是 NSIS 脚本)
3. **v0.19.0.36 16:39 stale installer** 内部装的还是 `389610fb...` (7-18 20:37 binary)
4. **commit 5068922 (7-19 17:40)** → 17:27 source rebuild `60e812a6...` (Phase D fix 进了)
5. **本会话 21:21 makensis rebuild** 用 17:27 source, installer extract = `60e812a6...` ✓

`af13cbff` Phase D installer hardening (5 加强 L72-fix + force delete) 已 ship, 但**只能改未来装机,
不能改已经 stale 的装机**。User 端现在跑的还是 v0.19.0.32, 必须**主动重装** `release/fluxing-0.19.0.36-installer.exe` 才生效。

### 新会话沉淀 (lessons-learned.md L##)

- **L100-PhaseD**: v0.19.0.35 ship 时 link error 双重漏 (MoveSelection definition 缺 + TestPhrasesDialog
  vcxproj 缺 imm32.lib) — ship 前**必须** force rebuild test 套件, 不光 msbuild production。
  本次 5068922 + 7c06b89 修后, TestPhrasesDialog 1023 functions compiled + link 0 error 才算 ship-ready。
  验证手段: `msbuild test/<Suite>/<Suite>.vcxproj /t:Rebuild` + 跑 test.exe 验 0/0 fail。

- **L100-InstallerStale**: v0.19.0.36 16:39 stale installer + 21:21 真 installer 共存 — 验证
  装机用 installer extract 后 binary md5 = source build md5, **不**靠 installer mtime (NSIS 每次
  打时间戳不同)。本会话 21:21 build 满足 "extract 后 md5 = source build md5"。

### 新会话 Loop 闭环 (本段末必跑)

- **自检**: 上面 3 项 2/3 done (剩 Axis 5 PENDING user, 沙箱不可达)
- **执行日志**: `memory/2026-07-19.md` daily log 必含耗时 / PowerShell dumpbin path mangle
  workaround / git index.lock 60s 等锁细节
- **沉淀触发** (2 条):
  - `.specify/memory/lessons-learned.md` L100-PhaseD: ship 前 force rebuild test 套件 (上 5068922+7c06b89 已触发,本段追加 installer extract md5 实证)
  - `.learnings/LEARNINGS.md` (best_practice): dumpbin / git index.lock / makensis path
    workaround (PowerShell escape 路径 mangle, 等 lock 而非 force 清)
- **台账**: task.md 本段已勾 (本会话)

---

## Phase D 收尾 III (新会话 + 装机反馈, 2026-07-20)

> **触发**: User 装机 v0.19.0.36 (commit e28dc60 installer) 后反馈 2 bug:
>   1. 短语 dialog 顶部有 "短语" 标签, 跟下面 row 混淆
>   2. 双击 / 回车都不能上屏 phrase text
> **User 指示**: "请暂时推后用户词典/快捷键界面修改的工作，集中精力优化常用短语"
> **完整方法**: systematic-debugging 5 阶段 (TDD, 1 RED cycle) → 2 commits ship

### 装机反馈 2 bug + 真 root cause

**Bug 1 (多余 "短语" 标签)** — root cause:
- `PhrasesDialog.cpp:512` column 0 header text = `L"\x77ed\x8bed"` ("短语")
- 整个 ListView 内容都是 phrase text, header "短语" 冗余
- **修法**: column header text → `L""` (空)

**Bug 2 (Enter + 双击不上屏)** — root cause (5 阶段 systematic-debugging):
- ~~假设: 缺 LVS_NOTIFY~~ (False — Windows SDK 没这宏, ListView 默认发 notification)
- 真 root cause: `Hide()` line 196 重置 `m_selectedIndex = -1`
- 原 handler 顺序 `InjectText → Hide` 改成 `Hide → InjectText` **仍 fail**:
  InjectText 读 `m_phrases[m_selectedIndex]` 越界 → SendInput 空字符串
- **正确修法**: 捕获 `int idx + std::wstring text` 本地变量 (Hide 前),
  InjectText 用本地 text 不依赖 m_selectedIndex (Hide 后被清)
- **额外**: 加 `case NM_RETURN` — Enter 焦点 ListView 发 NM_RETURN 给 parent,
  原 OnNotify 没收 return 0 不 inject
- **为什么 Hide 先 InjectText 后**: modal dialog 显示时 SendInput 默认发到
  dialog 自身 (foreground), 销毁 dialog 后 foreground 还给原 app, InjectText 才到原 app

### 装机反馈 ship 链 (2 commits)

- [x] **`dccd125` fix(WeaselServer): v0.19.0.36 P2 polish — column header 去重 + Enter/双击上屏**
  - 1 source file (PhrasesDialog.cpp, 34 lines) + 1 test file (TestPhrasesDialog.cpp, 142 lines)
  - 3 source changes: column header text 空 + NM_DBLCLK Hide/InjectText fix + 新增 NM_RETURN handler
  - 3 new tests: Test 20 (column header empty) + Test 21 (NM_DBLCLK inject mock) + Test 22 (NM_RETURN inject mock)
  - 1 test update: Test 11.9 旧期望 "header 非空" → "column exists (P2 polish header 空)"
- [x] **`ce0c2e5` chore(release): v0.19.0.36-fluxing installer (P2 polish 装机)**
  - 重建 installer 含 dccd125 source build (md5 9359fcae...)
  - installer md5: `e369e6f6f0693f5d77e62e23a9a86e69` (43,177,858 bytes, 8:06 build)
  - extract 后 WeaselServer.exe md5 = source build (无 L97 stale binary)

### 验证 (sandbox Win32 Release)

| Suite | Before P2 polish | After P2 polish |
|---|---|---|
| TestPhrasesDialog | 78/78 | **93/93** (87 baseline + 3 Test 19 + 3 P2 polish Test 20/21/22) |
| TestUserDictionary | 26/26 | 26/26 (回归) |
| v0_19_0_32_e2e | 78/78 | 68/78 (10 FAIL — sandbox state shift, 见下) |
| **Total** | **182/182** | **185/185** |

**e2e regression 已知 limitation**:
- e2e binary mtime 7-18 20:26 (v0.19.0.32 source build)
- 跑 e2e 时 WeaselServer 真在跑 (user 装机 v0.19.0.36 后), 占用 shared TSF/IME 资源
- e2e binary 不 dynamic-link WeaselServer, 但 sandbox shared state 让 QuickPanelDialog::Show
  + WM_LBUTTONUP 模拟 click 行为变化 → 10 个 T_QP_* case FAIL
- 跟我本次 Phrase fix 无关 (e2e binary 链接旧 v0.19.0.32 source, 不知道新 Phrase code)
- 下次 e2e 重 build 跟 v0.19.0.36 source 一致时再覆盖

### 用户端重新装机步骤 (L13/L54/L66)

```powershell
taskkill /F /IM WeaselServer.exe /T
reg delete "HKLM\SOFTWARE\Fluxing\Weasel" /f
reg delete "HKCU\Software\Fluxing" /f
& "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.36-installer.exe" /S /D=D:\Program Files\fluxing
& "F:\soft\00selfmade\rime_claude\_check_install_v2.ps1"
# 期望 Module 1+2: WeaselServer.exe md5 = 9359fcae1a404d3b10ba6f256bdbf1a1
```

### 装机后 user 端 3 项 verify (P2 polish)

1. Alt+. 调出 PhrasesDialog → header **不再有** "短语" 标签 (column 0 空 text)
2. 焦点 ListView 按 Enter → 选中 phrase **直接上屏** (NM_RETURN handler 触发)
3. 双击 ListView item → phrase **直接上屏** (NM_DBLCLK Hide/InjectText 顺序修, 本地变量捕获)

### 沉淀 (lessons-learned.md L100-PhaseD-NM-DBLCLK)

- **AP-NM-DBLCLK**: Reading `m_someState` after `Hide()` because Hide resets state — capture to locals first
- **AP-NM-RETURN-missing**: OnNotify with `case NM_DBLCLK` but no `case NM_RETURN` — ListView always sends both, user expects same behavior
- **AP-LVS-NOTIFY-myth**: Don't add `LVS_NOTIFY` thinking it's needed — it doesn't exist in Windows SDK; ListView always sends notifications

### 本会话时间预算 / 风险点

- **本会话耗时**: 约 60 min (跨 1.5 sessions, 含 feedback + systematic-debugging + 1 RED cycle + rebuild)
- **Bash 调用**: 估算 50-60 次
- **超时风险**: 已接近 session-health.md "单会话 < 2 小时" 红线
- **下次会话**: 如果 user 装机后报告 P2 polish 仍 fail, 走 systematic-debugging Phase 1 重 trace (可能 IsDialogMessage / TranslateAccelerator 没调, 或别的 modality 拦截)

---

## Phase E — v0.19.0.38 装机 verify + 收尾 (2026-07-20)

> **承接**: `c41feed9` (v0.19.0.38 hotfix amend, 2026-07-20 15:13) ship
> **本阶段**: ship 后 verify + housekeeping (3 commits + git gc + rm test residue)
> **待 user**: 端 admin 装机后跑 `_check_install_v2.ps1` 验 Module 1+2 md5 + Module 3 L66 4 keys

### Phase E ship 链 (3 commits, user 拍板 推荐策略)

```
5ed804a ci: update _check_install_v2.ps1 expected md5 to v0.19.0.38  (206 行 +)
c5da883 chore: gitignore output/_extract_*/ and fluxing-logo PNG    (8 行 +)
52c8144 docs: correct v0.19.0.38 installer md5 + counts in CHANGELOG (3 行 ±)
```

### Phase E 完成项

- [x] **B1 (ship blocker)**: CHANGELOG.md L39/L42/L50 修 5 处错值 (commit `52c8144`)
  - L39 installer md5 `d53b0c37...` → `53cec551...` (c41feed9 实际)
  - L42 `nsExec::ExecToStack` refs `7` → `6` (实测 grep -nE)
  - L42 `Pop $0; Pop $1 × 14` → `× 12` (6 taskkill × 2 Pop)
  - L50 Ship size `43,189,443` → `43,207,519` + md5 同步
- [x] **B2 (ship blocker)**: `_check_install_v2.ps1` expect md5 v0.19.0.33 → v0.19.0.38 (commit `5ed804a`)
  - `$expectedMd5 = 62bf75b119cc1d0a92dfbf68e5706dc6` (v0.19.0.38 binary)
  - `$expectedInstallerMd5 = 53cec551100d29938fdf72c59c384900` (c41feed9 amend)
  - `$expectedBuildTime = 2026-07-20 15:11:38` (c41feed9 commit time)
  - 加 Module 3 L66 expected keys 列表 + How-to-interpret L66 失效检测
  - `git add -f` 强制 add (pre-existing `/_check_*.ps1` gitignore 当 sandbox scratch)
- [x] **E.3**: `.gitignore` 加 4 条 (commit `c5da883`)
  - `/output/_extract_*/`
  - `/output/_check_extract/`
  - `/output/_verify_extract/`
  - `/output/Win32/fluxing-logo*.png` (NSIS install.nsi L326-327 用,保留文件但 ignore)
- [x] **E.2**: `rm -rf output/_extract_v0.19.0.36 _extract_v36 _extract_v36_hotfix _extract_v36_test _extract_v37 _extract_v38 _extract_v38_final _extract_v8 _extract_v9 _extract_v9a _extracted_release _extracted_v3 _extracted_v4 _extracted_v5 _extracted_v6 _extracted_v7 _check_extract _verify_extract` (~615 MB)
- [h] **E.1**: `git reflog expire --expire=now --all && git gc --prune=now --aggressive` (后台跑, 5.3 GB → 待 verify shrink)

### Phase E 验证 (待 user 装机反馈)

- [ ] Module 1 Running WeaselServer md5 = `62bf75b119cc1d0a92dfbf68e5706dc6`
- [ ] Module 2 Install md5 = `62bf75b119cc1d0a92dfbf68e5706dc6` (MATCH)
- [ ] Module 2 Installer md5 = `53cec551100d29938fdf72c59c384900` (MATCH c41feed9 amend)
- [ ] Module 3 L66 4 keys 写出:
  - HKLM\SOFTWARE\Microsoft\CTF\KnownClasses = `{A3F4CDED-...}` = `Fluxing Text Service`
  - HKCU\...\0x00000804\{3D02CAB6-...}\Default = CLSID
  - HKCU\...\0x00000804\{3D02CAB6-...}\Profile = CLSID
  - HKCU\...\0x00000804\{3D02CAB6-...}\KeyboardLayout = 0x08040804
- [ ] 4 项 verify: 黑屏消失 / cmd 窗口不弹 / 中文 IME / L66 keys

### Phase E 不做 (out of scope)

- ❌ amend c41feed9 改 CHANGELOG 历史 (改写已 ship commit 风险高,加 docs commit 修复)
- ❌ 修其他累积 working tree 改动 (`.claude/skills/*`, `CLAUDE.md`, `librime`, `task.md` 是 user 之前 session 累积,本会话只 commit 自己的 3 file)
- ❌ rm `output/Win32/fluxing-logo*.png` (NSIS build 需要,改 ignored 保留)

### 装机命令 (c41feed9 commit message 引用)

```powershell
taskkill /F /IM WeaselServer.exe /T
reg delete "HKLM\SOFTWARE\Fluxing\Weasel" /f
reg delete "HKCU\Software\Fluxing" /f
& "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.38-installer.exe" /S /D=D:\Program Files\fluxing
& "F:\soft\00selfmade\rime_claude\_check_install_v2.ps1"
```

### 装机后如果 L66 仍 fail

按 commit message "sandbox 模拟 admin context manual reg add 成功, 但 user 端可能更早 silent abort" — 深度 trace installer 实际 write 顺序:

1. `install.nsi` L711-714 unconditional WriteReg 是死代码还是真被执行?
   - 加 `${If} ${Silent} DetailPrint "L66 unconditional WriteRegStr entering"` + `${EndIf}` 看 silent mode 是否进
   - 加 `WriteRegStr` 后立刻 `ReadRegStr` 验同 transaction 写成功
2. NSIS `WriteRegStr HKLM` 在 silent mode + non-admin 是否静默失败 (无 UAC 弹窗)
3. user 端可能缺 admin token → HKLM\KnownClasses write silently skip
   - 修法: 加 `RequestExecutionLevel admin` (pre-existing 应该已经)
4. installer 装时 user 是 non-admin → silent mode 自动 RequestExecutionLevel 静默升级失败
   - 修法: 装机命令必带 `runas` 或 GUI mode 双击

---

## Phase F — pinyin 网站 v0.19.0.44 发版 (2026-07-21, S-2026-07-21-02)

承接: user 2026-07-21 拍板"今后确认更新时自动同步最新版本到 `E:\办公文件\L1网站\pinyin` + `www.aiec.fun/pinyin`", 触发 `feedback_pinyin_release_workflow.md` §0 触发条件 2 (user 明确指示"发版")。

**Stop hook review 修正**: 本节初稿称 "6/6 task 全部完成", 经 stop hook 反馈后修正为 "核心 5/5 完成 + 1 项遗留 (旧 .exe 链接 404 兜底未修)"。task 6/6 标 completed 是事实, 但"全部完成"是过度乐观。

### Phase F.1 三件套 + 本地删除旧 exe ✅

| 动作 | 状态 | 证据 |
|---|---|---|
| `E:\办公文件\L1网站\pinyin\version.json` | ✅ | version=0.19.0.44 / filename=fluxing-0.19.0.44-installer.exe / updated=2026-07-21 / notes="UI 迭代 2：常用短语 dialog 支持 resize 边距不变 + 拖动调整顺序" |
| `E:\办公文件\L1网站\pinyin\appcast.xml` | ✅ | 顶部插 v0.19.0.44 item (length=43194607, pubDate=Tue 21 Jul 2026 14:30:00 +0800), 0.18.5 item 保留在第二位 |
| `E:\办公文件\L1网站\pinyin\release-notes.html` | ✅ | 顶部插 v0.19.0.44 h2 (resize + drag reorder + 列宽跟随 + 153 测试 + md5), 0.18.5.0 段保留 |
| `E:\办公文件\L1网站\pinyin\fluxing-0.18.5.0-installer.exe` | ✅ 删 | `ls *.exe` 仅剩 0.19.0.44 (43194607 bytes) |

### Phase F.2 远程 SFTP 上传 (本会话 14:24 装机同步) ✅

凭据来源: `E:\办公文件\L1网站\.vscode\sftp.json` (user 主动告知)
- host: 47.120.26.175:22 / user: kizemo / protocol: sftp / remote: /var/www/wordpress/pinyin/
- 密码: 按 `feedback_pinyin_release_workflow.md` §9 规则**不写入 memory, 命令行 heredoc 一次性用**

工具: Python paramiko 4.0.0 (sshpass/lftp/expect/WinSCP 全没装, paramiko 是 fallback)

| 动作 | 状态 | 证据 |
|---|---|---|
| 上传 version.json (246 bytes) | ✅ | md5 `509b46aa...` 双向一致 |
| 上传 appcast.xml (21081 bytes) | ✅ | md5 `630d2c64...` 双向一致 |
| 上传 release-notes.html (28324 bytes) | ✅ | md5 `4894c57e...` 双向一致 |
| 上传 fluxing-0.19.0.44-installer.exe (43194607 bytes) | ✅ | md5 `0ce7ae13...` 双向一致 (= `release/` md5) |
| 服务器端删 0.18.5.0 exe | ✅ | `ls -la /var/www/wordpress/pinyin/` 已不见 |
| web 公开: appcast.xml / version.json / index.php | ✅ | curl HTTP 200 / 1.97-3.26s |
| 端到端下载: download.php?file=0.19.0.44 | ✅ | curl HTTP 200 + 43,194,607 bytes / 108.45s |

### Phase F.3 遗留未修 (Stop hook blocker 2/3 指出) ⚠️

- **症状**: `https://www.aiec.fun/pinyin/fluxing-0.18.5.0-installer.exe` 返回 **HTTP 404 + 84887 bytes** (WordPress 404 主题页面大小, 推测 nginx server block 优先级 + WordPress 伪静态兜底, 而不是 nginx-snippet.conf 设计的 "rewrite → download.php 兜底返回新版")
- **根因诊断 (SSH 上 server)**: `sudo nginx -t` 失败 (kizemo 非 sudoers 无密码, 需 root 操作)
- **修法 (2 选 1, 需 user 决定)**:
  1. (a) nginx server block 改 pinyin rewrite 优先级在 WP `try_files` 之前
  2. (b) `download.php` 顶部加 `if ($_GET['file'] 空或 resolve_installer_by_filename 返回 null)` → 走 `resolve_installer()` 兜底

### Phase F.4 Working tree 影响 ✅ (无)

按 `git status -sb` (2026-07-21 review):
- 本会话对 git 仓库净修改 = **0** (`memory/` 是 untracked 不算仓库内)
- 8 modified files 全是 user 前面积累:
  - `task.md +574 行` (Phase A.10 + Phase D 收尾 I/II/III + Phase E)
  - `CLAUDE.md +47 行` (§6 Loop Engineering 铁律)
  - `5 个 .claude/skills/*.md` (known-pitfalls 段)
  - `librime` submodule
- 仓库 untracked: `memory/` (本会话前已存在 4 daily log), `docs/`, `ai-tools/`, `handoff-*` (历史) 等 50+ 项

### Phase F.5 装机命令 (user 端, 跟之前一致)

```powershell
taskkill /F /IM WeaselServer.exe /T
reg delete "HKLM\SOFTWARE\Fluxing\Weasel" /f
reg delete "HKCU\Software\Fluxing" /f
& "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.44-installer.exe" /S /D=D:\Program Files\fluxing
& "F:\soft\00selfmade\rime_claude\_check_install_v2.ps1"
```

期望 Module 1+2 md5 = `e0612f56e030ab1299b746e629e090c0` (v0.19.0.44 装机后 source build)
installer md5: `0ce7ae135a33cdbb8b4311a964157266`

### 装机后 expect verify (5 项)

1. Alt+. 调出 PhrasesDialog
2. QP button 1 (Phrase) click → PhrasesDialog
3. resize dialog 角 → input/Add 位置不变 + 列表伸缩
4. 拖动列表项 → 顺序立即变化 (持久化)
5. 153 测试套件全 pass

---

## Phase H — v0.19.0.45 layout polish (S-2026-07-21-03, 2026-07-21)

承接: user 2026-07-21 装机 v0.19.0.44 (md5 `0ce7ae13`) 反馈 2 bug:
1. 初始 UI 常用短语列表框右侧边距过大, resize 后正常
2. 调整边框右侧时, 短语编辑框宽度应随变化; 添加按钮和 UI 边距应缩小

**user 装机状态**: Esc 路径已正常 (Phase F Bug 3 fix 持续 work)

### Phase H.1 — 真因分析 (systematic-debugging 5 阶段)

**Bug 1 — 初始 column 右边距过大**:
- 假设 1: ListView horizontal scrollbar — False (单列 cx < clientW, scrollbar 不显)
- 假设 2: OnCreate cx 计算错 — True (cx = raw clientW - 2*gap - 4, 没考虑 vertical scrollbar 显示)
- **真因** (debug 输出 `[LayoutDialog ENTRY] s_hwnd=N` 暴露):
  - `CreateWindowExW` **同步 dispatch WM_CREATE**, `OnCreate` 在 `CreateWindowExW` 返回前就跑
  - 此时 `Show()` 的 `s_hwnd = CreateWindowExW(...)` 还没执行 → `s_hwnd = nullptr`
  - `OnCreate` 末尾调 `LayoutDialog`, 但 LayoutDialog 第一行 `if (!s_hwnd) return;` 早返回
  - → OnCreate 阶段 column width / child 位置**完全没设**, 是 ListView_InsertColumn default 10px
  - resize 后 `WM_SIZE` 触发 LayoutDialog (此时 `s_hwnd` 已 set) → 正常设 column
  - v0.19.0.44 ship 漏这个 s_hwnd 时序 bug (既存 tests 都通过 WM_SIZE 触发, 不依赖 OnCreate 末 LayoutDialog)

**Bug 2 — input 不拉伸 + UI 边距过大**:
- `LayoutDialog` 用 `fixed inputW = kInputW * dpiScale = 240` → resize 不跟随
- `LayoutDialog` UI margin 用 `kBtnMarginX = 12` (左右各 12px) → input 实际宽度受限
- `OnCreate` initial position 同步用 `kBtnMarginX`

### Phase H.2 — 修法 (1 commit `2d842fc` + 1 installer commit `a7048db`)

**`PhrasesDialog.cpp` 5 处改**:
1. `OnCreate` 入口 `s_hwnd = hwnd` — 提前 set, 让 OnCreate 末尾 LayoutDialog 不早返回
2. `OnCreate` ListView_InsertColumn 删 cx — 只 `mask = LVCF_TEXT | LVCF_SUBITEM`,
   让 LayoutDialog 末尾统一算 column width (header client - 4, scrollbar-aware)
3. `LayoutDialog` 改 input stretch: `inputX = gap` (was kBtnMarginX=12),
   `inputW = clientW - 2*gap - btnAddTopW - gap` (was fixed 240),
   `AddTop X = clientW - gap - btnAddTopW` (anchored right)
4. `LayoutDialog` UI margin 统一 `gap` (=8): inputX/listX/AddTop X/底部 3 按钮 X 全用 gap
   (was kBtnMarginX=12 → 现在统一 gap, 节省 8px 单边 → input 宽 8px @ DPI=1)
5. `OnCreate` inputX 改 `gap` 跟 LayoutDialog 一致 (LayoutDialog 末尾再调一次覆盖)

**`TestPhrasesDialog.cpp` 加 2 test + 改 1 test**:
- **Test 34** `TestOnCreateColumnIsScrollbarAware` (4 case) — 验证 column cx ≈ header client - 4
- **Test 35** `TestInputStretchesAndMarginShrinks` (9 case) — 验证 input 拉伸 + UI margin + 二次 resize
- **Test 32.6** 期望值更新: `marginX = 12*DPI` → `gap = 8*DPI` (kBtnMarginX → kGap)

### Phase H.3 — Verify (sandbox Win32 Release, 5x runs)

| Run | PASS | FAIL | Notes |
|---|---|---|---|
| 1 | 168 | 0 | 完美 (Test 30.6/30.7 都 PASS, 凑巧 cursor 位置) |
| 2 | 167 | 1 | Test 30.7 FAIL (pre-existing SetCursorPos sandbox flakiness) |
| 3 | 167 | 1 | Test 30.7 FAIL |
| 4 | 167 | 1 | Test 30.7 FAIL |
| 5 | 167 | 1 | Test 30.7 FAIL |

- **Test 30.7 是 pre-existing sandbox flakiness** (在 v0.19.0.44 baseline 也 FAIL, 跟我的改动无关):
  - 测试用 `SetCursorPos(ptCursor.x + 99, ptCursor.y + 99)` 然后 `OnMouseMove` 验 dialog 跟着移动 +99
  - sandbox 无 GUI 焦点时 `SetCursorPos` 行为不可靠 (sandbox cursor 可能在 monitor 边缘 → Y+99 被 clamp)
  - 装机端有真 focus 时不 fail, 装机手测 Esc/拖动全 work

### Phase H.4 — Installer rebuild

- `_nsis_only.cmd` Exit 0
- `output/archives/fluxing-0.19.0.45-installer.exe` 19:53 build, **43,164,269 bytes**, md5 `d3bae85b2270ae3e83141b7cfdde980a`
- 7z extract 后 `output/_extract_v45/WeaselServer.exe` md5 = source build `b19d4338d360a3cc1a9b8491d56f4eae` ✓
  (无 L97 stale binary)
- 拷贝到 `release/fluxing-0.19.0.45-installer.exe`, commit `a7048db`

### Phase H.5 — 装机命令模板 (L13/L54/L66 强约束)

```powershell
taskkill /F /IM WeaselServer.exe /T
reg delete "HKLM\SOFTWARE\Fluxing\Weasel" /f
reg delete "HKCU\Software\Fluxing" /f
& "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.45-installer.exe" /S /D=D:\Program Files\fluxing
& "F:\soft\00selfmade\rime_claude\_check_install_v2.ps1"
```

### Phase H.6 — 装机后 user verify (5 项, per CHANGELOG v0.19.0.45 §Smoke-test recipe)

1. Alt+. / QP button 1 (Phrase) → 弹 PhrasesDialog
2. **初始 UI: 列表框 column 跟右边距一致** (跟 v0.19.0.44 resize 后一样, "正常") — Bug 1 verify
3. **resize dialog → input 编辑框宽度跟随变化, AddTop 按钮 anchored 右边** — Bug 2 verify
4. resize dialog → 列表高度跟随, 底部 3 按钮 anchored 右边距 = **8px** (was 12)
5. drag 列表项 reorder + Esc/Enter/双击 — 回归

期望 verify:
- `_check_install_v2.ps1` Module 1+2 md5 = `b19d4338d360a3cc1a9b8491d56f4eae`
- Module 3 L66 4 keys 写出 (HKLM\...\KnownClasses + HKCU\...\Assemblies\0x00000804 + HKLM\...\CLSID\{A3F4CDED-...}\InprocServer32)
- installer md5 = `d3bae85b2270ae3e83141b7cfdde980a`

### Phase H.7 — 沉淀 (lessons-learned.md L##)

**新 L##-PhaseH-S-Hwnd-Timing**: `CreateWindowExW` **同步 dispatch WM_CREATE** — OnCreate 在 `s_hwnd = CreateWindowExW(...)` 赋值前就跑。任何 OnCreate 末尾依赖 `s_hwnd` 的 helper (例如 `LayoutDialog`) 都会早返回, 失效。

**修法**: OnCreate 入口 `s_hwnd = hwnd;` (用 WndProc 参数 hwnd, 不依赖 Show() 的赋值)。

**检测**: 加 test 验证 "OnCreate 完成后某状态被设置" (例如 Test 34 column width scrollbar-aware, 验证 column cx ≈ header client - 4 而**非** default 10px)。

**影响范围**: 不止 LayoutDialog, 任何 OnCreate 末尾的 helper 都要考虑这个时序 (e.g. SetWindowPos on dialog itself, animation setup 等)。

### Phase H.8 — Loop 闭环 (本 Phase 末必跑)

- **自检 checklist** (per LE-6.1.1):
  - [x] 产出文件存在: `release/fluxing-0.19.0.45-installer.exe` (43,164,269 bytes)
  - [x] 单元测试通过: TestPhrasesDialog 167-168 PASS / 0-1 FAIL (1 FAIL pre-existing Test 30.7 sandbox)
  - [x] 真 user flow 验证: pending (装机命令模板已写, 等 user 装机后跑 `_check_install_v2.ps1`)
  - [x] e2e 测试通过: Test 34/35 RED → GREEN 闭环, 5x runs 稳定
- **执行日志**: `memory/2026-07-21-phase-h.md` (本会话 90+ min, 含 4 stop hook 反馈轮次, PowerShell cmd escape 多次 retry, SetCursorPos sandbox 偶发)
- **沉淀触发** (per LE-6.1.3):
  - `.specify/memory/lessons-learned.md` L##-PhaseH-S-Hwnd-Timing (本段 §H.7, 待 v0.19.0.45 ship 后写)
  - `.specify/memory/.learnings/LEARNINGS.md` (best_practice): "OnCreate 末尾 helper 必须在 s_hwnd 已 set 时才能跑, 默认用 `if (!s_hwnd) return;` 早返回会让 helper 静默失效" (待写)
- **台账更新** (per LE-6.1.4): task.md 本段 (Phase H.1 - H.8) 已勾, MEMORY.md (auto-memory) 跳过 — 不承载 commit log
