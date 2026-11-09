# 批次 Q · spec 045 T104 + T105（「+ 自定义」子 modal / 验证指引文案）

> 生成时间：2026-11-09 · 会话 `mvs_35234ea9f81048a1a1856fbd7134c838`
> 上一批：[批次 P · T111 真机验收闭环](handoff-2026-11-09-batch-p-t111-acceptance.md)
> 本批状态：**T104 + T105 功能闭环**。代码 `17e238e` / 文档 `2a5635d` 已提交；真机验收探针 v3
> **14/14 PASS**（2026-11-09 14:03）；期间修掉一个「只有装机才暴露」的下拉框空白缺陷，
> 修复已提交 `3342fb2`（见 §6.1）。

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

> 上表是「首次实现」的清单。真机验收时又在 `ShortcutSettings.cpp` 内修掉一个
> **下拉框从未被 `CB_ADDSTRING` 填充**的缺陷（+44/−5），详见 §6.1 —— 那是同一个文件的第二批改动。

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
- [x] 构建 / 单测 / L0 门禁 → 结果见 §6
- [x] 提交（代码 `17e238e` + 文档 `2a5635d`）
- [x] **真机验收**（2026-11-09 14:03 探针 v3 **14/14 PASS**，见 §6.2）
  - [x] 装机（`output\archives\fluxing-1.2.1-installer.exe` 原地升级，已核对装机副本 size+mtime 真替换）
  - [x] **修掉真机才暴露的缺陷**：「+ 自定义」两个下拉框空白（见 §6.1）
  - [x] `select` + `9` + `paging` → 确定 → 表格 36→37 行（UI 通路真机证伪通过）
  - [x] 取消不加行 / 关窗不落盘（`user1\fluxing` 全部 yaml sha16 前后逐字一致）
- [x] 提交本轮修复（`WeaselServer: ...`）+ 本 handoff 回填 —— 本轮完成

## 5. 下阶段（本轮已全部执行完毕；保留作方法参考）

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

> 口径：下面的「已实测」都是本机跑出来的结论，各条都带日志/证据文件路径；
> **标「刻意不做」的项不算失败，但也不得当成已通过**。

### 6.1 只有装到机器上才暴露的产品缺陷与修复（`3342fb2`）

**症状**（装 v1.2.1 后人工点击复现）：点「+ 自定义」，modal 正常弹出，但「动作类型」「触发条件」
两个下拉框**全空白**；点「确定」恒弹中文错误「请先选择动作类型与触发条件」
⇒ **UI 通路产不出任何自定义行**（纯函数 `BuildCustomRow` 本身没问题）。

**根因**（`WeaselServer\ShortcutSettings.cpp`）：`ShowAddModal()` 用 `makeCombo` 建了两个
`CBS_DROPDOWNLIST`（`ID_MODAL_ACTION=2140` / `ID_MODAL_WHEN=2142`），但全文**没有任何
`CB_ADDSTRING` / `CB_SETCURSEL`**；`AddModalProc` 的 `WM_COMMAND` 里另有一份局地 `static`
字符串数组，与 UI 并不共享 ⇒ `CB_GETCURSEL` 恒 `-1` ⇒ 必然走「未选择」分支。
单测只覆盖纯函数 `BuildCustomRow`（不碰 win32 控件），所以 `PASS=222` 全绿也发现不了这类缺陷。

**修复**（3 处，+44/−5，同一文件）：

1. 匿名 namespace 末尾（cpp:2505-2526）新增单一真源
   `struct ModalChoice { const wchar_t* key; const wchar_t* label; };`
   + `kActionChoices[]`（`send` / `toggle` / `select`）
   + `kWhenChoices[]`（`always` / `composing` / `has_menu` / `paging`）
   + `kActionChoiceCount` / `kWhenChoiceCount`。
2. `ShowAddModal()` 内新增**无捕获 lambda** `fillCombo`（cpp:2678-2687：循环 `CB_ADDSTRING` 后
   `CB_SETCURSEL 0`），对 `hAct` / `hWhen` 各调一次。
3. `AddModalProc` 的 `WM_COMMAND`（cpp:2757-2795）删掉局地数组，改「越界判定 +
   `BuildCustomRow(kActionChoices[ai].key, buf, kWhenChoices[wi].key, &row)`」。

⇒ 填充与回读**共用同一数组**，索引漂移这一类缺陷从结构上消失（原缺陷的成因正是两份数据没同步）。
**未加**「空值即拒」等新规则：`IsYamlUnsafeValue` / `IsOptionName` 对空串本就返回 false，
再加一层会牵动已绿的 222 条断言，收益为零。

**构建 / 打包 / 装机（实证）**

- 脚本 `F:\soft\00selfmade\rime_claude\tmp\t105-fix-build.cmd`（纯 ASCII，4 步：
  `vcvarsall.bat x86` → `xbuild.bat weasel` → `tmp\rebuild-installer.cmd` →
  `taskkill` + `scripts\install\silent-install.ps1` + 起 `WeaselServer.exe`），
  日志 `F:\soft\00selfmade\rime_claude\tmp\build-t105-fix.log`。
- `BUILD_EXIT=0`：日志含 `compiling.release WeaselServer\ShortcutSettings.cpp` 与
  `linking.release WeaselServer.exe`，`build ok, spent 17.234s`。
- `MAKENSIS_EXIT=0` → `output\archives\fluxing-1.2.1-installer.exe`（8 条 `warning 7010`，已知无害）；
  `INSTALL_EXIT=0`，`[G3] upgrading in place`，`RimeUserDir = D:\Program Files\fluxing\user1\fluxing`，
  acceptance-check `RESULT: ALL HARD CHECKS PASSED`。
- **装机副本确为真替换**（不是「退出码 0 但没覆盖」）：`output\Win32\WeaselServer.exe` 与
  `D:\Program Files\fluxing\weasel\WeaselServer.exe` 均为
  `2441728 B / 2026-11-09 13:48:24`（MATCH=True）；server pid 265892，启动 13:50:35。
- TSF `weaselx64.1.2.1.2.0.dll` 未变（1988096 B / 2026-11-08 21:47:58）——本轮只改 server UI，无需重启 TSF。
- ⚠️ 已提交的 `PASS=222 FAIL=0` 与 L0 15/15 是**修复前代码**的结论。修复只动 UI 填充与
  回读取值，未触及 `BuildCustomRow` 或任何纯函数，故单测结论仍然成立；
  按用户 2026-11-09 指示**不重复验证**。

### 6.2 真机探针 v3：**14/14 PASS**（2026-11-09 14:03:55–14:03:56）

探针 `F:\soft\00selfmade\rime_claude\tmp\acceptance-t105\probe-t105-v3.ps1`（日志同名 `.log`），
`SESSION=UNLOCKED`，`EXIT=0`，末行 `SUMMARY pass=14/14 VERDICT=PASS`。

判定口径：UIA 只用于**定位**（名字 + ControlType，不硬编码坐标）；**不读任何文本**（跨 64/32 位
文本读取不可靠），写入只用 `WM_GETTEXTLENGTH` / `WM_GETTEXT` **自证**，其余判据全部是
`CB_GETCOUNT` / `CB_GETCURSEL`、模态存活性、表格行数增量、yaml 字节一致性。

| # | 检查项 | 实测输出 |
|---|---|---|
| C0 | 设置对话框在跑 | `dialog handle=4719980 pid=265892` |
| C1 | 表格基线行数 | `table handle=7606068 rows0=36` |
| C2a | 点「+ 自定义」→ modal 出现 | `FluxingShortcutAddModal` 出现 |
| C2b | modal 控件齐全 | `act=ComboBox val=Edit when=ComboBox ok=Button err=Static` |
| C3 | **下拉框已填充**（§6.1 缺陷的正面证据） | `act items=3 cur=0` / `when items=4 cur=0`，默认 0/0 |
| C4 | 程序化选值 + 写值自证 | `CB_SETCURSEL` → `act=2 when=3`；`WM_SETTEXT "9"` → `ret=1 valLen=1 text=[9]` |
| C5 | 确定 → 行真的出现 | `modalClosed=True rows=37`（基线 36，**+1**） |
| C6 | 取消 → 不加行 | 重开 → 取消 → `cancelClosed=True rows=37`（未变） |
| C7 | 关窗 → 对话框关闭 | `WM_CLOSE → dialog closed=True` |
| C8 | **关窗零落盘** | `user1\fluxing` 全部 `*.yaml` 的 `名字:sha16:size` 串前后**逐字相同**（`yaml unchanged=True`） |
| C9 | server 存活 | `server alive pid=265892` |

- C5 输出里的 `errLen=-999` 是「modal 已销毁、`SendMessage` 打不通」的**哨兵值，不是错误**。
- C3 是本轮最关键的一条：**修复前**同一探针（v2）在这里拿不到项，修复后 `items=3/4`。
- 探针演进留档：v1 `pass=6/10`（枚举竞态，探针缺陷）→ v2 `pass=9/10`（仅 C6 红，根因是写入方式）
  → v3 `pass=14/14`。**只改探针、不改生产代码**；C6 的 39 字错误串与「空值」失败同长，
  故不能用 C5 自证写入生效，必须另做写值诊断（见 §8 第 14 条）。

### 6.3 构建 / 单测 / L0 门禁（修复前代码 `17e238e`）

- 构建日志 `F:\soft\00selfmade\rime_claude\tmp\build-batchQ.log` ⇒ **`BUILD_EXIT=0`**（Release\|Win32 全量）
- 单测：`msbuild /t:Rebuild test\TestShortcutSettings\TestShortcutSettings.vcxproj` ⇒ `TESTBUILD_EXIT=0`；
  跑 `test\TestShortcutSettings\Release\TestShortcutSettings.exe`（393216 B，2026-11-09 13:32:11）
  ⇒ **`PASS=222 FAIL=0`**，`TEST_EXIT=0`；日志 `tmp\test-batchQ.log`。
  批次 P 基线 186 → 222，**+36 = 本轮新增 `EXPECT` 条数**（逐条数过），确认新用例真执行了。
- L0 门禁：**仓库内** `scripts\gate\L0-static.ps1`（不是仓库外 `sandbox-verify\`，那需要管理员 +
  NSIS 安装包做整机复验）⇒ 日志 `tmp\gate-L0-batchQ.log`，
  **`L0_EXIT=0`，checks=15 pass=15 fail=0 skip=0**。
- 提交：代码 `17e238e`（`WeaselServer: spec 045 T105「+ 自定义」子 modal + T104 验证指引文案`，
  h + cpp + test，已排除脏子模块 `librime`）；文档 `2a5635d`（ADR 0046 + tasks.md T104/T105 勾选 +
  CLAUDE.md 指针 + 本 handoff 初版）。

### 6.4 仍未覆盖（**刻意不做**，不是失败）

- **不真按 `Control+Alt+K` 录键、不点「保存」** ⇒ 因此**没有** `default.custom.yaml` 托管块内
  `send: 9` + `when: paging` 的落地证据。理由：会真改用户配置并触发部署。
  探针收尾改用 `keybd_event(VK_ESCAPE)` 退捕获 + `WM_CLOSE` 关窗，**零落盘**（C8 已证）。
- 应用级两行仍只读、面板重绘无残影（L97 需 `RepaintLayered`）：批次 P run11 已验，本轮未重跑。
- DPI 缩放下 modal 是否裁切：只在本机当前缩放比例验过一次，**未做多比例矩阵**。
- 保存失败/部署失败路径的中文提示：本轮未触发。

## 7. 证据与前提分级

| 级别 | 内容 |
|---|---|
| **已实测（历史基线，批次 P 及更早）** | `EnterCapturing(int row)` 的 `row` 是 `m_hotkeys` 下标（cpp:1453 有越界守卫，cpp:2147 调用点传数据下标）；popover 是 `CreateWindowExW(WS_EX_TOPMOST\|WS_EX_TOOLWINDOW, L"STATIC", ..., WS_POPUP)` 独立 popup，非子窗；动作 yaml 合法形态只有 `send:` / `toggle:`（cpp:556/558 解析、cpp:619-628 描述、cpp:645-664 反查、内置表 cpp:383-436）；`RenderRow(eff, h.when, h.action_yaml)` 把 `action_yaml` **原样**拼进 inline dict（cpp:1064/1092）⇒ 坏字符会打碎 patch；批次 P 基线 `PASS=186` / run11 14/14 / 装机 sha256 `B7D04408650A71FC81D2AD45C15A44739C43FDF5E841184FE98D41ADF5A5F7DA` |
| **已实测（本轮，编译期）** | `BUILD_EXIT=0`；`TestShortcutSettings` `PASS=222 FAIL=0`；L0 静态门禁 15/15。⇒ modal 用到的符号（`kUiFontSize` / `kBtnW` / `kDialogH` / `kBgTop` / `kBgBot` / `kDlgRadius` / `kBorderColor` / `kWarningText` / `ModalChrome::PaintBackgroundAndBorder` 参数形态 / `ToLower` / 私有 helper 可见性）经编译器证伪为**可用**；`BuildCustomRow` 三型产物（select→`send: N`+`Action_SelectN`、toggle/send→`Action_CustomUser`）与 10 条非法值反例全部按断言通过 |
| **已实测（真机，本轮 14/14）** | ① modal 用 `WS_CHILD` 后**主面板不消失**（C2a，v1/v2/v3 三轮都成立）；② 两个下拉框**确实被填充**且默认停在 0（C3 `items=3/4`、`cur=0/0`）——这条直接证伪了 §6.1 的缺陷；③ 选 `select`+`9`+`paging` 点确定 ⇒ 表格 **36→37**，UI 通路端到端成立（C5）；④ 取消不加行（C6）；⑤ `WM_CLOSE` 关窗后 `OnDestroy`（cpp:2014-2025）**只解键盘钩子、无落盘分支**，实测 `user1\fluxing` 全部 yaml 的 sha16+size 前后逐字一致（C8）；⑥ 装机副本 size+mtime 与 `output\Win32\` 一致，排除「退出码 0 但没覆盖」（§6.1） |
| **推断（仍未证伪）** | `EnterCapturing` 接在新行下标（`rows-1`）后能否正确写键——本轮**不按键**，此路径只有静态依据（cpp:1453 越界守卫 + cpp:2147 传数据下标）；保存后 `default.custom.yaml` 托管块的 patch 是否被 librime 正常部署（没点保存，无证据）；DPI 多缩放比例下 modal 是否裁切（只在本机当前比例验过） |
| **未验证（刻意不做，见 §6.4）** | 应用级两行仍只读、面板重绘无残影、非法值中文提示的真机观感、`weasel.yaml` 未被写 |

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

以下 11–18 条是**本轮真机验收新增**的坑，接力者做桌面自动化时几乎必踩。

11. **探针里包装 `SendMessage` 的函数必须给可选参数默认值**：
    `SM($h, $msg, $wp)` 若只声明两参而调用点写 `SM($h, $msg)`，PowerShell 报
    「无法将 null 转换为 IntPtr」⇒ 签名写 `SM($h,$msg,$wp=[IntPtr]::Zero)`。
    同源：`SendMessageTimeout` 返回值语义是「0 = 超时/失败」，要单独映射成 `-999` 哨兵，
    否则 0 会被当成真实结果参与判定。

12. **`SetWindowTextW` 对**32 位进程**的 `EDIT` 返回 `TRUE` 但文本不落盘**：
    实测 `ret=True lastErr=0`，紧跟 `WM_GETTEXTLENGTH` 仍是 0。必须
    `SendMessageW(hEdit, WM_SETTEXT=0x000C, 0, text)`（`ret=1`，`valLen` 立刻变 1）。
    跨 64/32 位边界时**永远不要信 `SetWindowText` 的布尔返回值**，要用 `WM_GETTEXTLENGTH` 自证。

13. **P/Invoke 自定义方法名必须显式写 `EntryPoint`**：`static extern IntPtr SendMessageStrW(...)`
    会报 `Unable to find an entry point named 'SendMessageStrW' in DLL 'user32.dll'`。
    写法：`[DllImport("user32", EntryPoint="SendMessageW", CharSet=Unicode)]`。

14. **`WM_CHAR` 是「追加」不是「替换」**：先 `WM_SETTEXT "7"` 再 `WM_CHAR '9'` ⇒ 内容变 `"97"`
    （`valLen=2`），产品按设计报 39 字「动作值不合法」。**看起来像产品 bug，实际是探针自己写坏了。**
    两条实用推论：① 探针要有「写入后 `valLen` 是否等于预期」的**硬门禁**，不等就 `exit 2` 并标注
    `probe defect, cannot judge product`，不要把探针失败算成产品失败；② 「错误串长度」不能当判据
    ——「值不合法」与「未选择」的错误串一样长（都是 39 字），必须另外取证（返回码 +
    `GetLastWin32Error` + `valLen`，本轮 `tmp\acceptance-t105\diag-t105-c6b.ps1` 就是干这个的）。

15. **枚举 modal 有竞态，必须「等到子控件就绪」才算出现**：`ShowAddModal()` 先
    `CreateWindowExW` 建窗（cpp:2614-2618）再建 6 个子控件（cpp:2640-2710）；
    `EnumChildWindows` 一命中就去 `Child-ById` ⇒ 4 条假失败（v1 `pass=6/10` 就是这么来的）。
    正解：`Wait-Modal` 循环直到 `class=='FluxingShortcutAddModal' && visible && Child-ById(2143)!=0`。

16. **PowerShell 三个会打断脚本的名字/调用**：① 函数名别叫 `Cls`（撞 `Clear-Host` 别名，
    报「设置 CursorPosition 时出现异常：句柄无效」并中断）；② 别用 `& '<script>.ps1'` 直接调，
    会撞 profile 的 `$RawUI.CursorPosition` ⇒ 一律 `pwsh -NoProfile -File <abs.ps1>`；
    ③ **PS 5.1 的 `Add-Type` 没有 `-CompilerOptions`** ⇒ `NamedParameterNotFound`、日志都不生成
    ——探针必须用 **pwsh 7**。

17. **`Get-FileHash` 在「`.cmd` 里 `powershell -File`」调起的脚本里不可用**：
    仓库内 `scripts\install\acceptance-check.ps1` 因此**只比了 size，sha256 比对是假 PASS**。
    本机改用 `[System.Security.Cryptography.SHA256]::Create()` 逐文件算。
    ⇒ **安装器「ALL HARD CHECKS PASSED」不能当成「文件内容正确」**，要另做 hash 核对。

18. **`weaselx64.dll` 被在跑进程占用时，静默安装会「退出码 0 但覆盖失败」**（与 memory #41 同源）。
    只改 server 的批次可以避开：改完 `WeaselServer.exe` 就够，不必动 TSF。
    但凡涉及 TSF 的批次，装完必须核对装机副本的 **size + mtime**（本轮即用
    `2441728 B / 2026-11-09 13:48:24` 三者对齐来证明真替换）。
