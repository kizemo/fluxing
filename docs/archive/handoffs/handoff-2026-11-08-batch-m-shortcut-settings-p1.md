# 批次 M · spec 045 快捷键设置界面 P1 收口（T101 / T102 / T103 / T106）

- 日期：2026-11-08
- 分支：`Fluxing`（ahead 10，**本批改动尚未提交**）
- 前序交接：[handoff-2026-11-08-batch-l-shortcut-settings-menu.md](handoff-2026-11-08-batch-l-shortcut-settings-menu.md)
- 规格：`.specify/specs/045-shortcut-settings/`（`spec.md` / `design.md` / `tasks.md`）
- 设计条款：AP-045-B（`WH_KEYBOARD_LL` hook 内不得 `SendInput` / IPC / 同步 I/O）

---

## 1. 本轮目标

按批次 L 交接的「下阶段计划」，把 spec 045 的 **P1 缺口**（实现已存在但零调用点 / 写盘会毁配置）全部接线完成：

| 任务 | 原文病灶 | 本轮结论 |
|---|---|---|
| T101 | `CheckConflict` / `GetBuiltinBindings` 实现完整但**全仓无调用点** ⇒ 冲突 UI 从未生效 | 已接线 |
| T102 | 状态行只在窗口创建时写死一次，编辑后不刷新 | 已接线 |
| T103 | `ApplySearchFilter` 算了 `match` 但**不隐藏行** | 已改为按匹配重建列表 |
| T106 | 写盘是**全量** patch；`action_yaml` 为空的行写出**无动作的坏 binding** | 已改增量托管块 |

---

## 2. 本轮完成（全部落盘）

### 2.1 T101 冲突检测接线

- `ShortcutSettings.h`：`SetCaptureHint` 后新增 `SetPopoverHint`；`CheckConflict` 后新增
  `ConflictHintText` / `UpdateCaptureConflict` / `RefreshStatus`。
- `ShortcutSettings.cpp`：
  - `LowLevelKeyboardProc` 改调 `UpdateCaptureConflict()`（纯内存 + `SetWindowTextW`，**无 I/O**，符合 AP-045-B）。
  - `EnterCapturing` 尾部写提示；`ExitCapturing` 对 **L18 / L19 硬错误**（`Shift_L` / `Shift_R` 单键）拦截并**留在捕获态**。
  - 同表重复冲突**不拦**，只提示（design §4.5 / §6.3 允许）。
- 配色复用既有 token `kWarningText`，**未新增硬编码颜色**（守 `ShortcutSettings.h` 注释 + FLUENT-UI-TOKENS §3.6）。

### 2.2 T102 状态行实时刷新

- 新增 `RefreshStatus()`，在 `PopulateTable()` 末尾**统一收口**调用；`s_statusConflict` 记冲突态，
  `OnCtlColor` 据此选 `kWarningText`。
- 所有增删改 / 搜索 / 捕获结束路径都经 `PopulateTable()` ⇒ 状态行天然同步，无需逐处补调用。

### 2.3 T103 搜索框真正过滤

- 从「重画但不隐藏」改为**按匹配重建列表**（不用 `LVIS_CUT` 假隐藏）。
- 行号 → `m_hotkeys` 下标的映射存进 `lParam`；新增文件作用域 `TableItemData(HWND, int)` 读取。
- **编译阻塞根因**：SDK 10.0.26100 中 `ListView_GetItemData` / `ListView_SetItemData` / `LVM_GETITEMDATA`
  **全都不存在** ⇒ 只能用 `ListView_GetItem(_SetItem)` + `LVIF_PARAM` 自行取 `lParam`。
- `OnNotify` 两处按数据下标取行，避免过滤后行号错位。

### 2.4 T106 增量 patch 落盘（本批最大改动）

`ShortcutSettings.cpp` 结构（改后行号）：

| 行号 | 内容 |
|---|---|
| 588–638 | 匿名 `namespace { struct SaveStats{written, skipped_no_action, backed_up, refused}; SaveStats g_saveStats; const char kManagedBegin[] / kManagedEnd[]; HasTopLevelPatch(); BackupFile(); }` |
| 640–约 770 | `SaveHotkeys(path, data, bool overwrite_file)` |
| 1731 起 | `SaveAndDeploy()` |
| 1769 起 | `ExportYamlPath()` |

写盘语义（定稿）：

1. **只写需要 override 的行**：`edited`（`accept_new` 非空且与 `accept` 不同）或 `!is_default`。
2. `accept` 为空 ⇒ 跳过；`action_yaml` 为空 ⇒ 跳过并 `++g_saveStats.skipped_no_action`（**不静默吞**，状态行报账）。
3. 零行时**不写空 `patch:` / `bindings:`**（只写托管块注释）。
4. 托管块用**纯 ASCII 标记行** `kManagedBegin` / `kManagedEnd` 包裹；二次保存只替换块内，块外内容原样存活。
5. `overwrite_file || existing.empty()` ⇒ 整文件覆写；否则定位托管块整体替换（无尾标记则吃到最后）。
6. 旧文件**已有顶层 `patch:` 且无托管块** ⇒ `refused = true; return false;`（**拒绝写入**，避免 YAML 重复键毁配置）。
7. 首次接管先 `BackupFile()`，**失败即不写**；成功置 `backed_up`。
8. 统一写 `"\xEF\xBB\xBF"` + 正文（保 BOM）。
9. 读旧文件时剥掉 UTF-8 BOM，避免二次保存出现双 BOM。

配套修正：

- `HasTopLevelPatch` **原有 bug**：`a == 0` 只在首行成立 ⇒ 改存 `lineStart`，只认**行首无缩进**的 `patch:`
  （缩进的 `patch:` 是别的段子键，追加不冲突）。
- `LoadHotkeys` 补 `else if (!key.empty() && !val.empty()) hk.action_yaml = key + L": " + val;`
  —— 修「本界面写出的 `custom:` 行读回即丢」。
- `SaveAndDeploy()` 按 `g_saveStats.refused` 分文案：
  `✗ 未写入：default.custom.yaml 已有 patch 段，请手工合并后再保存` /
  `✗ 保存失败：无法写入 default.custom.yaml`；成功后追加 `· N 行缺 YAML 动作已跳过` 与 `· 原文件已备份`。
- 导出调用点改 `SaveHotkeys(path, m_hotkeys, true)`（导出=整文件覆写）。
- `.h`：`SaveHotkeys` 三参声明（`bool overwrite_file = false`）放**public 区**（单测要直接调），
  且必须位于 `struct Hotkey` **之后** —— 类内成员声明的参数类型必须已声明，放 `SetYamlPath` 之后会 `C2065 Hotkey 未声明`。

### 2.5 新增 4 条落盘单测

`test/TestShortcutSettings/TestShortcutSettings.cpp`（插入 `}  // namespace test` 之前），
新增辅助 `WriteRaw` / `CountOf` / `MakeHk`：

| 用例 | 断言（全部打在落盘字节上） |
|---|---|
| `TestSaveHotkeysWritesOnlyEdited` | 只写改过的行；BOM 正确；两个标记各 1 次；未改默认行不出现 |
| `TestSaveHotkeysPreservesOutsideBlock` | 块外 `schema_list:` 存活；二次保存仍只有 1 个托管块；旧键被换掉 |
| `TestSaveHotkeysRefusesExistingPatch` | 旧文件有顶层 `patch:` 无托管块 ⇒ 返回 false **且字节完全不变** |
| `TestSaveHotkeysSkipsNoAction` | 无 `action_yaml` 不写盘、**不写空 `patch:`** |

### 2.6 测试目标链接修复（真 bug，非本轮功能引入）

`TestShortcutSettings.vcxproj` 链接 `ShortcutSettings.obj`，而后者引用 `WeaselUserDataPath()`
（定义在 `RimeWithWeasel/WeaselUtility.cpp`，测试工程不链接该 TU）：

```
ShortcutSettings.obj : error LNK2001: 无法解析的外部符号
  "class std::filesystem::path __cdecl WeaselUserDataPath(void)"
```

修法：在测试 TU 里提供**测试本地最小实现**（同 `test/TestTrayRestoreIgnored` 思路），返回
`%TEMP%\FluxingTestUserData`；用例一律用 `SetYamlPath` 显式指定路径，谁都不读这个目录。

---

## 3. 验证状态

### 已验证（有日志 / 有输出）

| 项 | 命令 | 结果 | 证据 |
|---|---|---|---|
| `weasel` 目标构建 | `tmp\build-batchM.cmd`（`xbuild.bat weasel`） | `BUILD_EXIT=0`，`[100%]: build ok, spent 18.734s` | `tmp\build-batchM-6.log` |
| 测试工程构建 | `tmp\build-test-shortcut.cmd`（MSBuild 直编） | `BUILD_TEST_EXIT=0` | `tmp\build-test-2.log` |
| 单测 | `release\TestShortcutSettings.exe` | **`PASS=80 FAIL=0`**，`TEST_EXIT=0` | `tmp\test-batchM-3.log` |
| L0 静态门禁 | `scripts\gate\L0-static.ps1 -Mode source` | **`L0 RESULT mode=source checks=14 pass=14 fail=0 skip=0`**，`GATE_EXIT=0` | `tmp\gate-batchM-3.log` |

### 未验证（必须诚实记为缺口）

- **真机界面零验收**：未启动快捷键设置窗口；T101 冲突提示、T102 状态行、T103 搜索过滤的实际像素与交互**均未看过**。
- **未验证 `default.custom.yaml` 真实落盘**：只在单测里对临时文件断言过；真实用户目录写入路径未跑。
- **`WeaselUserDataPath()` 实际返回值未确认**：`E:\Users\Duanyi\AppData\Roaming\Fluxing` 目测不存在，
  真机验收前必须先查清它到底解析到哪。

---

## 4. 关键工具坑（下一会话别再踩）

1. **`xbuild.bat weasel` 不重建测试工程**。测试是独立 vcxproj，必须 MSBuild 直编：
   ```
   msbuild test\TestShortcutSettings\TestShortcutSettings.vcxproj /t:Build ^
     /p:Configuration=Release /p:Platform=Win32 ^
     /p:SolutionDir="F:\soft\00selfmade\rime_claude" /m:1 /nologo /v:minimal
   ```
   ⇒ **`release\*.exe` 单测产物可能是过期二进制**。本批就发生了：上一轮报 `PASS=52`，
   其实是加入测试前的旧 exe；重建后变 `PASS=80`。判 PASS 前先确认二进制 mtime。
2. `SolutionDir` 必须是**不带尾反斜杠**的绝对路径（否则 OutDir 会嵌到 `test\<name>\Release\`，见 L31）。
3. `cmd /c "F:\...\tmp\build-test-shortcut.cmd"` 形态可用；`cd /f/... && cmd /c ...` 会被安全策略拦。
4. PowerShell 里 `"$i: $_"` 会报「变量引用无效」，要写 `${i}`。
5. `ShortcutSettings.h` 注释明确：**不在 `ShortcutSettings` 内写硬编码颜色**，一律复用 `namespace {}` 常量。

---

## 5. 下阶段计划（按优先级）

1. **T107 正文级真合并**（新增，本批产物）：现在「旧文件已有顶层 `patch:` 且无托管块」是**拒写**。
   要接管老用户配置，必须按 key 递归合并而不是拒写 —— 否则老用户点保存只会看到「未写入」。
2. **真机界面验收**（T101 / T102 / T103 / T106 全都要）：起窗口 → 改一行 → 看状态行/冲突提示 →
   保存 → 看 `default.custom.yaml` 字节 → 重新打开看能否读回。注意锁屏时原始输入注入与抓屏会失败，
   但 UIA `InvokePattern.Invoke()` 仍可用。
3. **T104「立即验证」按钮**：涉及 `SendInput` 注入，必须**先过 P2 审查**（AP-045-B），本批未做。
4. **T105 自定义快捷键子 modal**：现在「+ 自定义」只 push 一行 placeholder（`accept=""` / `custom: user_added`）。
5. **P2 T201–T203**、**测试缺口 T301–T304**（见 `tasks.md`）。

---

## 6. 文件清单（本批改动）

已跟踪、已改：

- `WeaselServer/ShortcutSettings.cpp`
- `WeaselServer/ShortcutSettings.h`
- `test/TestShortcutSettings/TestShortcutSettings.cpp`
- `.specify/specs/045-shortcut-settings/tasks.md`（T101/T102/T103/T106 勾选 + 新增 T107）

批次 L 遗留未提交（同分支同一批，勿单独提交）：

- `WeaselServer/WeaselServer.rc`、`WeaselServer/WeaselServerApp.cpp`、`WeaselServer/resource.h`
- `build.bat`、`CHANGELOG.md`、`CLAUDE.md`、`.specify/memory/lessons-learned.md`、`docs/adr/README.md`
- 未跟踪：`.specify/specs/045-shortcut-settings/tasks.md`、`docs/adr/0045-*.md`、批次 L handoff

**提交前必做**：`git status -sb` + `git diff --stat`，确认没有 `weasel.props` / `env.bat` / `*.obj` 混入。
提交信息用 Conventional Commits + 模块前缀（`WeaselServer:` / `docs:`）。

## 7. 残留物待清

- `tmp\` 下本批日志：`build-batchM-6.log`、`build-test-1.log`、`build-test-2.log`、`test-batchM-3.log`、`gate-batchM-3.log`
- `%TEMP%` 下 `default.custom.yaml.bak-<时间戳>`（单测走 BackupFile 分支留下，仅污染临时目录）
- `weaselx64.dll.bak0044`、`weaselx64.dll.old.tmp`（批次 L 遗留，被 17 个进程占用，`CLEAN_FAIL`）