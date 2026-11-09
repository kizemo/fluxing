# 批次 Q · spec 045 T104 + T105（「+ 自定义」子 modal / 验证指引文案）

> 生成时间：2026-11-09 · 会话 `mvs_35234ea9f81048a1a1856fbd7134c838`
> 上一批：[批次 P · T111 真机验收闭环](handoff-2026-11-09-batch-p-t111-acceptance.md)
> 本批状态：**代码 + 单测已落盘，构建/单测/门禁/提交结果见文末「交付与验证」；真机验收未做**

---

## 1. 背景

spec 045 = 快捷键设置 UI（`WeaselServer/ShortcutSettings.{h,cpp}`，GDI 分层自绘面板）。
批次 P 已闭环 T111（应用级两行只读、永不写托管块）。本批做 tasks.md 里剩下的两项：

- **T104** — design §7.3「立即验证」：原方案设想用 `SendInput` 注入新快捷键以证明生效。
  用户明确要求 **必须先过 P2 / AP-045-B 审查** 再动手。
- **T105** — design §7.3「+ 自定义」按钮：现状是「直接加一个空行」，用户要的是
  **能真正构造一条自定义 binding**。

## 2. 目标与本轮决策（ADR 0046）

ADR：[`docs/adr/0046-spec045-verify-no-sendinput-and-add-custom-modal.md`](../../adr/0046-spec045-verify-no-sendinput-and-add-custom-modal.md)
三个决策，全部有实测依据：

1. **否决 SendInput 注入验证**。理由：① 违反 constitution P2 / AP-045-B；
   ② LL hook 回调内禁 SendInput/IPC/同步 I/O；③ 非前台进程的注入会被前台锁吞掉
   （批次 P 真机已复现「点击未送达」类时序坑）；④ 即便注入成功也无法证明是新 binding 生效。
   → 改为**保存后状态栏文案指引**：`已触发部署，请在候选窗自按新快捷键验证`。
2. **modal 用主窗的 `WS_CHILD` 子窗**，不用 owned 顶层窗。理由：顶层窗会让分层面板
   走失焦 / `ACTIVATEAPP` 隐藏路径，真机症状 = 「一点自定义面板就消失」。
3. **不在 modal 内嵌键捕获**，复用已上线的 `EnterCapturing` popover；且
   **必须先 `PopulateTable()` 再 `EnterCapturing(m_hotkeys.size()-1)`**
   —— 否则新行下标与表格行号不同步，捕获的键会写到别的行。

## 3. 改动清单（本批全部文件）

| 文件 | 内容 |
|---|---|
| `WeaselServer/ShortcutSettings.h` | modal 控件 ID 段 `ID_MODAL_ACTION/VALUE/WHEN/OK/CANCEL/ERR = 2140..2145`；private 声明 `ShowAddModal()` / `AddModalProc()` / `static HWND s_hModal`；public `static bool BuildCustomRow(const wchar_t* actionType, const wchar_t* actionValue, const wchar_t* when, Hotkey* out)` |
| `WeaselServer/ShortcutSettings.cpp` | ① `ID_BTN_ADD` case → `ShowAddModal(); return 0;`（不再直接加空行）② T104 状态栏文案（仅 deployed 分支）③ 文件尾约 200 行：匿名 ns helper（`IsYamlUnsafeValue` / `IsOptionName` / `TrimWs` / `SetModalError`）+ `s_hModal` 定义 + `BuildCustomRow` + `ShowAddModal`（一次性注册 `FluxingShortcutAddModal` 类、420×286 `WS_CHILD`、`ModalChrome` 同款背景、两个 `CBS_DROPDOWNLIST` + 一个 `EDIT`、确定/取消）+ `AddModalProc` |
| `test/TestShortcutSettings/TestShortcutSettings.cpp` | 新增 `TestT105BuildCustomRow` + `TestT105CustomRowPersistence`，并在 `wmain` 注册 |
| `.specify/specs/045-shortcut-settings/tasks.md` | T104 / T105 勾选（**若本轮构建或单测最终失败必须回退**） |
| `CLAUDE.md` | 本批交接指针行 |
| `docs/adr/0046-*.md` | 新增 |

### `BuildCustomRow` 契约（唯一业务出口，modal 本身不含逻辑）

| type | value 要求 | 产物 |
|---|---|---|
| `select` | 单个 `1`-`9` | `action = Action_SelectN`，`action_yaml = "send: N"` |
| `send` | 非空、无 YAML 结构字符/空白（允许 `+`） | `action = Action_CustomUser`，`action_yaml = "send: V"` |
| `toggle` | 仅 `[A-Za-z0-9_.]` | `action = Action_CustomUser`，`action_yaml = "toggle: V"` |

行属性统一：`is_custom=true` / `is_default=false` / `baseline_index=-1` / `read_only=false`
/ `accept` 与 `accept_new` 留空（待捕获）/ `yaml_raw = action_yaml`。
空 `when` → `always`；type/when 大小写不敏感；value 首尾空白被裁。
非法输入一律 `return false`，modal 只在错误 static 里给中文提示，不落行。

## 4. 进度

- [x] T104 审查 + ADR 0046
- [x] T104 文案落地
- [x] T105 生产代码（h + cpp）
- [x] T105 单测（2 个用例 + wmain 注册）
- [ ] 构建 / 单测 / L0 门禁 → 结果见 §6
- [ ] 提交（分两笔）
- [ ] **真机验收（本批唯一硬缺口）**

## 5. 下阶段（接力者按此顺序做）

1. 若 §6 有红灯：先修到 `TestShortcutSettings` 全绿，再提交；不要带着红灯 commit。
2. 提交两笔（Conventional Commits + 模块前缀，见 AGENTS.md §4）：
   - `docs: ADR 0046 spec045 T104/T105 审查结论与 tasks 勾选`（ADR + tasks + CLAUDE.md + 本 handoff）
   - `WeaselServer: spec045 T105 自定义子 modal + BuildCustomRow, T104 验证指引文案`（h + cpp + test）
   - commit 前必须 `git status -sb` + `git diff --stat`；**排除 `librime` 脏子模块**、
     `weasel.props` / `env.bat` / `*.obj`。
3. **真机验收**：按用户 2026-10-19 偏好，需要装机就**直接打包 exe**，不要等用户要求。
   探针要点（UIA 定位用名字+ControlType，别硬编码坐标；锁屏时只有 `InvokePattern` 可用）：
   - 点「+ 自定义」→ **面板不消失**、出现 modal（子窗，`WS_CHILD`）；
   - 选 `select` + 值 `9` + 条件 `paging` → 确定 → **行出现且立即进入捕获 popover**；
   - 按 `Control+Alt+K` → 行内显示新键 → 保存；
   - `default.custom.yaml` 托管块内出现 `send: 9` 且带 `when: paging`；
   - 取消 → 不加行；非法值（如 `select` 填 `0`）→ 有中文错误提示且不落行；
   - 回归：应用级两行仍只读、不泄漏；`weasel.yaml` 未被写；面板重绘无残影（L97 需 `RepaintLayered`）。
4. 若真机发现 modal 布局/裁切问题：改 `ShowAddModal` 的尺寸常量即可，业务逻辑无需动。

## 6. 交付与验证

> 本节由本轮构建/测试结束后填写；**空着 = 未验证**，不要当成已通过。

- 构建日志：`F:\soft\00selfmade\rime_claude\tmp\build-batchQ.log` ⇒ **`BUILD_EXIT=0`**（Release|Win32 全量）
- 单测：`msbuild /t:Rebuild test\TestShortcutSettings\TestShortcutSettings.vcxproj` ⇒
  `TESTBUILD_EXIT=0`，跑 `F:\soft\00selfmade\rime_claude\test\TestShortcutSettings\Release\TestShortcutSettings.exe`
  （393216 B，2026-11-09 13:32:11）⇒ **`PASS=222 FAIL=0`**，`TEST_EXIT=0`；
  日志 `F:\soft\00selfmade\rime_claude\tmp\test-batchQ.log`。
  批次 P 基线 186 → 222，**+36 = 本轮新增 `EXPECT` 条数**（逐条数过），新用例确实执行了。
- L0 门禁：**仓库内** `F:\soft\00selfmade\rime_claude\scripts\gate\L0-static.ps1`（不是仓库外的
  `sandbox-verify\`，那是需要管理员 + NSIS 安装包的整机复验）⇒
  日志 `F:\soft\00selfmade\rime_claude\tmp\gate-L0-batchQ.log`，**`L0_EXIT=0`，checks=15 pass=15 fail=0 skip=0**
- 提交：代码 `17e238e`（`WeaselServer: spec 045 T105「+ 自定义」子 modal + T104 验证指引文案`，
  h + cpp + test，已排除脏子模块 `librime`）；文档 = 含本 handoff 的那一笔
  （`git log --oneline -1` 即 docs 提交，ADR 0046 + tasks.md T104/T105 勾选 + CLAUDE.md 指针）。
- **仍未验证**：真机装机交互（modal 显示 / 焦点 / 层级 / 重绘 / 保存后的 yaml 结果）

## 7. 证据与前提分级

| 级别 | 内容 |
|---|---|
| **已实测** | `EnterCapturing(int row)` 的 `row` 是 `m_hotkeys` 下标（cpp:1453 有越界守卫，cpp:2147 调用点传数据下标）；popover 是 `CreateWindowExW(WS_EX_TOPMOST\|WS_EX_TOOLWINDOW, L"STATIC", ..., WS_POPUP)` 独立 popup，非子窗；动作 yaml 合法形态只有 `send:` / `toggle:`（cpp:556/558 解析、cpp:619-628 描述、cpp:645-664 反查、内置表 cpp:383-436）；`RenderRow(eff, h.when, h.action_yaml)` 把 `action_yaml` **原样**拼进 inline dict（cpp:1064/1092）⇒ 坏字符会打碎 patch；批次 P 基线 `PASS=186` / run11 14/14 / 装机 sha256 `B7D04408650A71FC81D2AD45C15A44739C43FDF5E841184FE98D41ADF5A5F7DA` |
| **已实测（本轮新增）** | 生产代码 `BUILD_EXIT=0`；`TestShortcutSettings` `PASS=222 FAIL=0`；L0 静态门禁 15/15。⇒ modal 用到的符号（`kUiFontSize` / `kBtnW` / `kDialogH` / `kBgTop` / `kBgBot` / `kDlgRadius` / `kBorderColor` / `kWarningText` / `ModalChrome::PaintBackgroundAndBorder` 参数形态 / `ToLower` / 私有 helper 可见性）经编译器证伪为**可用**，`BuildCustomRow` 的三型产物（select→`send: N`+`Action_SelectN`、toggle/send→`Action_CustomUser`）与 10 条非法值反例全部按断言通过 |
| **推断（待真机证伪）** | modal 的**布局与层级**（420×286 子窗在 DPI 缩放下是否裁切、`WS_CHILD` 是否真的不让分层主面板走失焦）、`EnterCapturing` 接在新行下标上能否正确写键、保存后 `default.custom.yaml` 的 patch 是否被 librime 正常部署 |
| **未验证** | 真机交互全链路（modal 显示/焦点/重绘、取消不加行、非法值中文提示、应用级两行仍只读、`weasel.yaml` 未被写） |

## 8. 本轮工具坑（接力者免踩）

1. **交接卡串台**：`E:\Users\Duanyi\.minimax\.minimax\maintenance\task-state\LATEST.md`
   内容是 **Alpha File Manager**（3.13.1 打包 / 登录 harvest 死凭据），与本仓库无关 ⇒ 忽略，
   按 Fluxing 上下文走（同 agent memory #38）。
2. **图谱降级已实证**：`codebase-memory-mcp` 索引过期 ⇒ 按 `.claude/rules/codebase-graph.md`
   降级 Read/Grep，本批全程如此。
3. **路径雷**：`include\ShortcutSettings.h` / `WeaselIME\ShortcutSettings.cpp` /
   `test\TestShortcutSettings\Main.cpp` 都**不存在**，正确路径是 `WeaselServer\` 与
   `test\TestShortcutSettings\TestShortcutSettings.cpp` ⇒ 先 `git ls-files '*ShortcutSettings*'`。
4. **bash 权限门**：`Set-Location` + 写日志会被拦（「write/delete targets cannot be verified
   safely after a command working directory change」）⇒ msbuild 一律用**绝对路径**、不要 `cd`。
5. **`edit` 只吃唯一匹配**：`}  // namespace test` 前的锚点必须带上文（`EXPECT(p.y == 0);` 三行），
   否则 `Could not find the exact text`。一次调用只传一组 `old_string`/`new_string`（memory #40）。
6. **调用预算**：本轮 harness 在 121→133 次连续告警「立即落盘汇报」。下一轮**禁止再探索**，
   只做「构建 → 单测 → 门禁 → 提交 → 打包 → 探针」。
7. **`vswhere -latest` 在本机返回空 ⇒ 构建静默不跑**（`$msbuild` 为空串，
   `& $msbuild ...` 报「管道元素中 '&' 后面的表达式生成了无效对象」）。
   原因：本机只有 **BuildTools**（`Microsoft Visual Studio\18\BuildTools` 与
   `2022\BuildTools`），BuildTools 不参与 `-latest` 的默认 product 过滤。
   正确姿势：`-products * -find 'MSBuild\**\Bin\MSBuild.exe'`，或直接用绝对路径
   `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe`。
   另外 CLAUDE.md 写的 `msbuild weasel.sln` 假定 msbuild 在 PATH —— 本机**不在**，
   必须先定位。**教训：后台构建任务 `status=failed` 且日志文件根本不存在时，
   先查「命令是否真的跑起来了」，不要去查编译错误。**
8. **`weasel.sln` 构建不会重编 `TestShortcutSettings` ⇒ 陈旧二进制会给出假绿**：
   本轮 `BUILD_EXIT=0` 之后跑根目录 `release\TestShortcutSettings.exe` 得
   `PASS=186 FAIL=0`，与批次 P 基线**一模一样**，看 mtime 才发现它是 11:39 的旧产物
   （新代码根本没进去）。⇒ 单测必须**直接** `msbuild test\TestShortcutSettings\TestShortcutSettings.vcxproj /t:Rebuild`，
   产物落在 `test\TestShortcutSettings\Release\`（直接编工程时 `$(SolutionDir)` 未定义，
   OutDir 退化成 `$(ProjectDir)Release\`，与 sln 路径**不同**）。
   判据：报 PASS 数**恰好等于旧基线** ⇒ 先查 exe 的 mtime/size，再查代码。
9. **`ShortcutSettings` 是 `class`（全静态），不是命名空间**：`namespace SS = ShortcutSettings;`
   直接 `C2879`；写 `using SS = ShortcutSettings;` 才有 `SS::Hotkey` / `SS::BuildCustomRow`。
   动作枚举里**没有** `Action_ToggleOption` —— toggle 型自定义行的 `action` 是
   `Action_CustomUser`（真·内置 toggle 才有 `Action_ToggleChinese` 等）。
10. **static 局部量不能被 lambda 简单捕获**：modal 里 `static HFONT hfUi` + `[hfUi]` ⇒
    `C3495 简单捕获必须是含自动存储持续时间的变量`，并级联出 6 条 `C2064/C2440`。
    改成无捕获 lambda `auto setFont = [](HWND e, HFONT f){...}` + 调用点传参。
