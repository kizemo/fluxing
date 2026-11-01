# plan 095 — 「快捷面板常驻」开关收进统一设置面板

## 0. 一句话

把常驻开关做成设置面板里的一个复选框；为了让两个模块共用同一份状态，
把「运行期状态 + 落盘 + 读盘策略」下沉成共享头 `include/FluxingAlwaysShow.h`，
并删掉 `QuickPanelDialog::s_mode` 这个第二真源。

## 1. Constitution Check

| 条目 | 结论 | 依据 |
|---|---|---|
| **P1 简体中文** | ✅ | 新增用户可见字符串全中文：「界面」「快捷面板常驻」「设置」「快捷键与界面设置 · N 条快捷键」。 |
| **P2 TSF 回调内不做阻塞 I/O** | ✅ | 本次不触碰 `ITfTextInputProcessor::*` 回调路径；勾选框落盘发生在 `WM_COMMAND`（UI 线程消息），不是 TSF 回调。 |
| **P3 IPC 协议变更需三端同步** | ✅ 不适用 | 本 spec 不改 `include/WeaselIPC.h` 的消息语义，IPC 线格式零变化。 |
| **P4 Conventional Commits + 模块前缀** | ✅ | 计划提交：`feat(WeaselServer)`、`feat(WeaselUI)`、`test(fluxing)`、`docs(fluxing)`，scope 均在白名单内。 |
| **P5 ADR 决策落 `docs/adr/`** | ✅ | ADR 0026（MADR 四段式）+ `docs/adr/README.md` 索引。 |
| **P6 事故/bugfix 落 `lessons-learned.md`** | ✅ | L162（编码事故：`\xNN` 字节转义在宽字面量里不被重组）。 |
| **P7 变更 ≥3 文件 ⇒ 建 spec 三件套** | ✅ | 本目录 `spec.md` / `plan.md` / `tasks.md`。 |
| **P8 必答三问** | ✅ | 见下方。 |

### P8 三问（Required plan.md disclosures）

1. **这次改动最可能在哪里悄悄坏掉？**
   在「读盘语义」和「布局公式」两处。
   - 读盘：primary / legacy 的优先级、以及「读到但内容不认识 ⇒ `false`」
     这条旧语义一旦被「优化」成 nullopt 回落，就会把**坏文件**从「关」变成
     「常驻」。防护：`test/TestQuickPanelDialog` 既有 T13h / T16a–e **一行不改**，
     外加 `TestShortcutSettings` 的 T-A4。
   - 布局：`kTableH_phys` 与 `kBtnY_phys` 是两个耦合公式，插入 `kPrefsH`
     只改一个必然让表格压住按钮行或按钮行溢出对话框。防护：两个公式同步改，
     并在交付报告里给出推算值（表格 428px / 按钮行 y=606）。

2. **哪些改动会跨模块边界、需要 ADR？**
   「把常驻状态从 `QuickPanelDialog` 的私有静态成员，提到两个模块共享的
   共享头」是**跨模块边界的架构改动**（`WeaselServer` 内部两个子模块 +
   测试工程），需要 ADR 0026。
   「给设置弹窗加一个复选框」是纯 UI 增量，不单独需要 ADR。

3. **如果这次改动要回滚，最小回滚单元是什么？**
   按提交回滚。计划拆成 4 个提交（`feat(WeaselServer)` / `feat(WeaselUI)` /
   `test(fluxing)` / `docs(fluxing)`），其中 `feat(WeaselUI)` 只含共享头与
   `QuickPanelDialog` 的下沉，**不依赖** `feat(WeaselServer)` 的 UI 增量即可
   单独回滚（回滚后 `s_mode` 回来，设置面板里的复选框失效，托盘行为不变）。

### 决策分流（CLAUDE.md §3）

| 决策 | 落点 |
|---|---|
| 常驻状态下沉共享头、删除第二真源 | `docs/adr/0026-quickpanel-always-show-in-settings-panel.md`（Accepted） |
| `\xNN` 字节转义导致装机乱码 | `.specify/memory/lessons-learned.md` L162 |

## 2. 实施步骤

### 2.1 共享头（宪法 R7：生产代码与测试同源）

新建 `include/FluxingAlwaysShow.h`（UTF-8 BOM + LF，header-only，`namespace fluxing::always_show`）：

| 符号 | 职责 |
|---|---|
| `AlwaysShowFlagRef()` | 运行期状态唯一真源（函数内 `static bool`，跨 TU 共享一份） |
| `SetAlwaysShowFlag(bool)` | 直接写状态（**不落盘、不通知**）—— 只给「从磁盘恢复」用 |
| `ModeChangedCallback` / `SetModeChangedCallback` / `NotifyModeChanged` | 视觉副作用观察者（POD 函数指针，不拖 `<functional>`） |
| `FormatStateLine` / `ParseStateLine` | 单行 ASCII `always_show=0\|1\n` 的格式化与解析 |
| `StateReadStatus` / `ReadStateLine` | **三态**读：缺失 / 空文件 / 读到一行 |
| `ReadStateFile` | 读 + 解析 ⇒ `optional<bool>` |
| `ResolveAlwaysShowFromFiles(primary, legacy, default)` | **读盘策略纯函数**，与 ADR 0020 旧实现逐条等价 |
| `WriteStateFile(path, enabled)` | 写盘 + **回读自证**（不一致就删掉坏文件） |
| `StatePathOverrideRef()` / `HasStatePathOverride()` / `ResolveStatePath()` | 落点注入点与实际落点解析 |
| `ApplyAlwaysShowTo` / `ApplyAlwaysShow` | 置位 + 落盘 + 通知观察者（没变就不写盘、不通知） |

依赖面刻意收窄到 `<filesystem> + <fstream> + <optional> + <string>` +
`FluxingUserPaths.h`：**不引入 shell32 / COM / D2D**（`RegOpenKey` 走
`cl.exe` 默认链接的 advapi32），这样 `TestShortcutSettings` 才链得动。

### 2.2 `QuickPanelDialog` 下沉

- 删 `static Mode s_mode;` 及其定义；`CurrentMode()` / `AlwaysShowMode()`
  改读共享头。
- `WriteQuickPanelState` / `ReadAlwaysShowStateLine` / `QuickPanelStatePath`
  三个函数删除，逻辑并入共享头。
- `LoadPersistedAlwaysShowModeFrom` / `LoadPersistedAlwaysShowMode` 改为调用
  `ResolveAlwaysShowFromFiles`；`SetStatePath` 改为写 `StatePathOverrideRef()`。
- `RestoreAlwaysShowMode` 改为 `SetAlwaysShowFlag(LoadPersistedAlwaysShowMode())`
  —— **不落盘、不 auto-show 面板**（spec 055 不变量保留）。
- 新增静态成员 `OnAlwaysShowModeChanged(bool)`：关 ⇒ alpha 复位为满值；
  开关都 ⇒ 面板可见时 `RepaintLayered`。由文件作用域静态对象在**进程启动时**
  注册观察者（早于任何一次 `SetAlwaysShowMode`）。
- **legacy 落点（`%APPDATA%\Rime\quickpanel.state`）留在 `QuickPanelDialog`**：
  它要 `SHGetFolderPathW`（shell32），共享头不引入。

### 2.3 `ShortcutSettings` 增量

- 窗口标题 / 面板标题改「设置」；副标题改中文。
- 新增常量 `kPrefsH = 30`（分组行高）、`kPrefsY = kTitleH + kToolbarH`。
- `kTableH_phys` 减 `kPrefsH`、`kBtnY_phys` 加 `kPrefsH`（两式同步）。
- `OnCreate` 在工具栏行之后、状态行之前插入「界面」分组：
  自绘分组标题 + 分隔线（`OnPaint`），复选框用真实 `BUTTON` 控件
  （`BS_AUTOCHECKBOX`，ID `2130`），初始勾选态取 `AlwaysShowChecked()`。
- `OnCommand` 增 `ID_CHK_ALWAYS_SHOW` 分支（`BN_CLICKED` ⇒
  `SetAlwaysShowChecked(checked)`）。
- 新增公开 API `AlwaysShowChecked()` / `SetAlwaysShowChecked(bool)` ——
  不需要建窗口就能读写 ⇒ 单元测试可直接断言。

### 2.4 判别性回归测试

- **T-A（共享头纯函数）**：`TestShortcutSettings` 内新增 4 组 ——
  格式/解析、三态读、写盘回读自证、主旧落点优先级。
- **T-B（生产 API）**：`SetAlwaysShowChecked(true/false)` ⇒ 运行期状态 +
  落盘内容逐字节断言；同值重复设置不重复写盘。
- **既有断言零改动**：`TestQuickPanelDialog` 的 T12/T13/T16 全部保留 ——
  它们就是 FR-009「读盘语义逐条等价」的机器证据。
- **套件接线**：`TestShortcutSettings` 同时进入 build 列表（:107）与
  run 列表（:119）—— 此前两边都没有它，等于这个工程从未被 CI 编译过。

## 3. Quality Gates（提交前）

1. `build.bat weasel` ⇒ 0 错误。
2. `scripts/test-infra/run-test-suite.bat` ⇒ `=== ALL TESTS PASSED ===`。
3. `git status -sb` + `git diff --stat` 复核：无 `weasel.props` / `env.bat` /
   `*.obj` / `librime` 误入暂存区（`librime` 的脏状态是既有的，不可提交）。
4. 编码自检：`ShortcutSettings.cpp` 里 2 位十六进制字节转义计数 == 0。
5. CHANGELOG `[Unreleased]` 有对应条目（用户可感知的行为变化）。

## 4. 回滚

见 P8 三问第 3 问：按提交回滚，`feat(WeaselUI)` 可单独回滚而不影响托盘行为。
