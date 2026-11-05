# Handoff · 快捷设置栏槽位说明 + 候选删除（屏蔽）缺陷分析（2026-11-05 第三棒）

- **仓库**：`F:\soft\00selfmade\rime_claude`（Fluxing / 火流猩，WeaselServer / WeaselTSF 分支）
- **分支**：`Fluxing...kizemo/Fluxing`，本棒提交 `11327e3`（spec 099 P0-1/P0-2 收口 + 阶段交接）
- **本棒性质**：**只读分析 + 文档**。**零生产代码改动**，未重跑单测（基线 `release\TestQuickPanelDialog.exe` 190/190 沿用上一棒）
- **本棒提交**：本文 + 接力 prompt 一次提交（模块前缀 `docs(fluxing)`）

---

## 一、本棒用户指令原文

> 1. 请查看项目规划，快捷设置栏，品牌图标的右侧四个按钮，第二个是用户短语，另外三个是什么？
> 2. 在有候选窗的时候，使用快捷键 shift+ctrl+数字，以及右键点击对应候选，仍然无法删除候选。针对这个bug已经进行过多次修复。当前的分析结果如何？有什么修复计划？
> 3. 在回答完上述问题后，更新完整的handoff文档，和一句话prompt，用于新开会话接力任务。

**注意**：本棒开始时注入的交接卡 `E:\Users\Duanyi\.minimax\.minimax\maintenance\task-state\LATEST.md`
写的是「任务栏固定持久化 + 图片旋转翻转右键菜单」，交付物路径为
`F:\soft\00selfmade\filemanager\sigma-file-manager\...`（Alpha File Manager，session id
`mvs_abc537e91fe94dea80c4d87fb2ec4235`）—— **属另一项目，按 lessons L185 不采信**。

---

## 二、问题一：快捷设置栏品牌图标右侧 4 槽 = ？

**答案（已验证 · 源码级）**：4 个槽依次是

| 槽 | 名称（tooltip 首段） | 当前态（tooltip 第二段） | 实际行为 |
|---:|---|---|---|
| 0 | **切换到下一个输入方案** | `当前:<输入方案名>` | 切换到下一个 schema（ADR 0034） |
| 1 | **用户短语** | `当前输入:中/英` | 打开用户短语窗口 |
| 2 | **快捷键设置** | `常驻显示:开/关` | 快捷键设置页 |
| 3 | **关于 / 账号说明** | `版本:<x.y.z>` | 关于 / 账号说明页 |

⇒ 用户说的「第二个是用户短语」与实现一致；**另外三个是「切换到下一个输入方案」「快捷键设置」「关于 / 账号说明」**。

**证据**：
- `include/FluxingPanelTooltip.h:26-39`（`SlotName`，逐槽名称，用户可见简体中文）
- `include/FluxingPanelTooltip.h:42-63`（`SlotStateText`，逐槽当前态）
- `include/FluxingPanelTooltip.h:66-75`（`BuildTooltip` = 名称 + ` · ` + 当前态）
- 槽数常量：`WeaselServer/QuickPanelDialog.h:352`（`kButtonCount = 4`）、品牌图标命中 `-2`（`QuickPanelDialog.cpp:998-1000`）
- 行为细节：`docs/adr/0034-quickpanel-about-dialog-and-slot0-schema-switch.md`

**与搜狗的对照**：搜狗状态栏是 7 槽（含分隔线）且**无二级面板**；我方按 spec 099 `gap-matrix.md`
的结论**不引入二级面板**，`kButtonCount = 4` 保持不变。

---

## 三、问题二：「删除候选」缺陷的当前分析结果

### 3.1 已有 4 轮修复（全部在仓库源码内，已落地）

| # | 出处 | 修的是什么 | 根因 |
|---|---|---|---|
| 1 | ADR 0025 / spec 094（2026-11-01） | ① `m_deleteCallback` 由**值**改**引用**；② 新增 `include/FluxingCandidateDeleteShortcut.h`，按 **VK 码 + GetKeyboardState** 判定 `Ctrl+Shift+1..9`；③ DELETE 命令**回 body**（`EatLine eat`），客户端立刻 `_UpdateUI` | ① 值成员恒空 ⇒ **右键链路从未上线**；② `ConvertKeyEvent` 只给「产生的字符」（`Ctrl+Shift+1`→`!`），反推不出数字键；③ 不回 body ⇒ 客户端读旧缓冲 ⇒ 「点了没反应」 |
| 2 | ADR 0023 D5 / spec 090 | 服务端做「可见页内索引 → 引擎页内索引」映射；右键 = **无条件写屏蔽名单** + 照常调引擎删除 | librime C API **无 `is_user_dict`**；`Context::DeleteCandidate` 恒 `return true`（上游注释原文 "CAVEAT: this doesn't mean anything is deleted for sure"）⇒ 无法按返回值分流 |
| 3 | spec 097 / lessons L163（2026-11-01） | `ascii_punct` → `Control+period`、`traditionalization` → `Control+slash`，**腾空 `Ctrl+Shift+<数字>` 整族** | 两条无条件 toggle 抢占数字族；`ascii_punct` 无 reset ⇒ 一次误触变成**持续**故障 |
| 4 | spec 094 补强 | Ctrl 按住时**吞掉 Shift down/up**（`_shiftSuppressed`）；服务端补 `interveningKey` 标记 | 服务端 Shift 状态机会把 Ctrl 组合里的 Shift 当「选词 Shift」，菜单打开时 `commit_composition` 把候选误上屏 |

**配置侧已核对**：`output/data/default.yaml:272-273` 已是 `Control+period` / `Control+slash`（第 3 轮修复确实落到 yaml）。

### 3.2 本轮逐行核对的 4 段链路（**已验证 · 源码级**）

| 段 | 位置 | 事实 |
|---|---|---|
| A · 快捷键拦截 | `WeaselTSF/KeyEventSink.cpp:90-97` | `MatchCandidateDeleteShortcut(wParam, ctrl_down, shift_down, true)` → `ShouldInterceptCandidateDelete(number, _cand->GetVisibleCandidateCount())` → `_DeleteCandidateOnCurrentPage(number-1)` → `*pfEaten = TRUE` |
| B · 右键回调 | `WeaselTSF/CandidateList.cpp:309-310` → `:399-412` | `SetDeleteCandidateCallback([this](size_t){ _tsf->HandleDeleteCandidate(index); })` → `_DeleteCandidateOnCurrentPage` → `m_client.DeleteCandidateOnCurrentPage(index)` + 立刻读 body + `_UpdateUI(ctx, _status)` |
| C · 回调绑定 | `WeaselUI/WeaselPanel.h:184` / `:73-75` / `WeaselPanel.cpp:96` / `:571-573` | `std::function<void(size_t)>& m_deleteCallback;`（**引用**，非值拷贝）⇒ 构造时绑 `ui.deleteCandidateCallback()`；`if (m_deleteCallback) m_deleteCallback(static_cast<size_t>(m_hoverIndex));` |
| D · 服务端 | `RimeWithWeasel/RimeWithWeasel.cpp:439-500` | 取候选原文 → `AppendToIgnoreList(user_data, schema, text)` → 重置 `m_ignoreStamp` → `delete_candidate_on_current_page(session, engine_index)` → 置 `interveningKey` → `_Respond(ipc_id, eat)` + `_UpdateUI(ipc_id)` |

⇒ **源码侧没有缺口，四段全通。**

### 3.3 关键结论（当前分析结果）

1. **源码侧已无可修的「明显 bug」** —— 4 轮修复全部落地且互不冲突。
2. **你复现时跑的就是带修复的代码**（**已验证**）：
   - 已装 `D:\Program Files\fluxing\weasel\weasel.dll` mtime **2026-11-03 20:33**、`WeaselServer.exe` mtime **2026-11-03 20:34**，版本均 **1.1.9.0**；
   - 修复提交在 **2026-11-01 / 11-02**（`3de27e6`、`83d3fd5`、`135a5b1`）⇒ **早于**已装构建。
   - ⇒ **不是「装的是旧版」**，确有残留缺陷。
3. **测试只覆盖纯函数，从不驱动真实链路**（**已验证**）：`test/TestCandidateRButtonDown/` 测的是
   `MatchCandidateDeleteShortcut` / `ShouldInterceptCandidateDelete`；
   `test/TestCandidateIgnoreFilter/` 测的是名单解析与可见→引擎映射。
   **没有任何一个测试真的走「右键 → 回调 → IPC → 服务端 → 写名单」**。
   ⇒ **这正是「多次修复仍然不能删」的结构性原因：每一轮都只证明了纯函数正确，从没证明链路通。**
   任何一环接线断掉，单测全绿、真机照样零反应。
4. **唯一可观测产物 = `<用户数据目录>\<schema_id>.user_ignore.txt`**（**已验证**）：
   服务端每次删词**无条件**往名单写一行（`FluxingCandidateIgnore.h:216-237`，UTF-8 + CRLF，追加）。
   ⇒ **判「到底走到服务端没有」不用猜，看这个文件在不在、行数涨没涨。**

### 3.4 三个最可能的残留断点（**推断**，按可能性排序）

> ⚠️ 这三条是**源码级推断**，本轮**没有真机复现**（当前活动输入法是搜狗，不是火流猩）。每条都给了可判定的检验方法。

- **(a) 右键命中用了「hover 状态」而不是「命中测试」**（最可疑）
  - `WeaselPanel.cpp:572` 用的是 `m_hoverIndex`（`int`，初值 **-1**，见 `WeaselPanel.h:187`）；
    `static_cast<size_t>(-1)` = **SIZE_MAX**。
  - 而 **CHANGELOG.md:496-497** 记的原始语义是「`index` 来自 `m_layout.candies.indexFromPoint(pt)`」——
    **当前实现与原始设计语义不一致**（点命中 → 悬停态）。
  - 失败路径：**候选窗刚出现、鼠标还没产生过 WM_MOUSEMOVE** 就右键 ⇒ `m_hoverIndex = -1`
    ⇒ 发出 **SIZE_MAX** ⇒ 服务端 `_MapVisibleIndexToEngine` 越界 ⇒ 直接 `return`（但**照样 `_Respond` + `_UpdateUI`**）
    ⇒ **用户看到「点了没反应」，而代码路径确实执行过了**。
  - 检验：读 `WeaselUI/WeaselPanel.cpp:540-576` 全文，确认 `m_hoverIndex` 何时被赋值、`-1` 时有无守卫。
- **(b) 快捷键入口被「可见性判据」静默放行**
  - `WeaselTSF/CandidateList.h:73-76`：`GetVisibleCandidateCount()` 先判 `_ui->IsShown()`，
    不显示就返回 **0**；`ShouldInterceptCandidateDelete(number, 0)` 恒假 ⇒ **不拦截** ⇒ 按键放行给引擎。
  - L163 之后 `Ctrl+Shift+<数字>` 已**没有任何 Rime 绑定兜底** ⇒ 放行 = **纯静默、零反应**。
  - 失败路径：TSF 侧 `IsShown()` 与用户肉眼看到的候选窗**不一致**（时序 / 另一窗口句柄）⇒ 按了等于没按。
  - 检验：在拦截分支加一条 `DLOG`（含 `number` / `visible_count` / `IsShown()`），看是否进得来。
- **(c) 响应体时序**：客户端 `m_client.DeleteCandidateOnCurrentPage(index)` 之后立刻 `GetResponseData` + `_UpdateUI`
  （`CandidateList.cpp:399-408`）。若读到的是**上一份**管道缓冲，候选窗不变 ⇒ 也表现为「像没删」。

---

## 四、修复计划（段 0 → 段 3，按序执行；段 1/2 取决于段 0 的判定结果）

### 段 0 · 先取判别性证据（**不写代码**，一次操作定性）
在**已装 v1.1.9.0 的机器**上、用**火流猩**（不是搜狗）：
1. 输入拼音 → 出候选窗；
2. **右键**点第 1 个候选；
3. 立刻检查用户数据目录里有没有 `<schema_id>.user_ignore.txt`，内容是不是那个词。

> **用户数据目录落点未知**：`%APPDATA%\Rime` **不存在**（本棒实测）。真实落点由 `WeaselUserDataPath()` 决定
> （spec 088「weasel-install-root-resolution」）；先按该规则定位，或直接在 `D:\Program Files\fluxing\weasel\` 下找 `*.user_ignore.txt`。
> 定位命令（ASCII）：`Get-ChildItem -Path 'D:\Program Files\fluxing' -Recurse -Filter '*.user_ignore.txt' -EA SilentlyContinue`

**判定表**：

| 段 0 结果 | 结论 | 下一步 |
|---|---|---|
| 名单文件**出现**且含该词 | 服务端**已收到** ⇒ 断点在**客户端 / UI 刷新** | 走 **段 2** |
| 名单文件**不出现** | 断点在**客户端 → 服务端之间**（右键回调 / 索引 / IPC） | 走 **段 1** |
| 右键有效、Ctrl+Shift+数字无效 | 快捷键入口的**可见性判据**问题 | 走 **段 2** 的 (b) |
| Ctrl+Shift+数字有效、右键无效 | 右键命中 / hover 索引问题 | 走 **段 1** 的 (a) |

> 同时对 **Ctrl+Shift+1** 单独做一次上述判定 —— 两条入口分别定性，不要混着看。

### 段 1 · 修「右键命中 + 负索引」（若段 0 = 名单没写）
1. `WeaselPanel.cpp` 的 `OnRButtonDown`：加 `if (m_hoverIndex < 0) return 0;` **守卫**，杜绝 SIZE_MAX 静默越界；
2. 命中索引改回**点命中**语义：`m_layout.candies.indexFromPoint(pt)`（与 CHANGELOG 记录的原始语义对齐），
   `m_hoverIndex` 仅作 fallback；
3. 补**判别性单测**：构造 `m_hoverIndex = -1`，断言**不触发**回调、**不发** IPC。
4. 若段 0 显示「Ctrl+Shift+数字也没写名单」⇒ 一并查段 2(b)。

### 段 2 · 修「静默放行 + 刷新」（若段 0 = 名单写了但界面不变）
1. **拦截判据**（b）：把 `ShouldInterceptCandidateDelete` 的 `visible_count` 来源从
   `_ui->IsShown() ? ctx().candies.size() : 0` 改成**服务端最近一次 `_GetCandidateInfo` 的可见候选数**
   （UI 可见性与引擎可见性不再混用）；先加 `DLOG` 取证再改。
2. **刷新**（c）：核对 `GetResponseData` 的时序 —— 删词后必须等**本次**响应；
   必要时改用「发命令 → 等回包 → 用回包里的 ctx 刷新」而不是「发命令 → 立刻读缓冲」。
3. 两者都要有**判别性单测**（mock：可见数=0 时不拦截；回包体与上一份不同时必须用新的一份）。

### 段 3 · 补端到端门禁（**根治「多次修复仍然假绿」**）
在 `test/TestWeaselIPC` 增加一条**真链路**断言：
`WeaselPanel` 右键命中 → 回调被调 → IPC 送达 → 服务端 handler 执行 → **名单文件新增一行**。
（现有 `TestWeaselIPC` 已有 `/verify-delete-body` 一类断言，可在其上扩展。）

> **没有这一条，后续任何修复都会继续「单测全绿、真机不通」。** 这是本缺陷反复修不好的**结构性根因**。

---

## 五、已验证 / 推断 / 未验证（严格三分）

### 已验证（本轮读源码 / 读文件 / 实测文件系统）
1. 快捷设置栏 4 槽的名称与当前态（`include/FluxingPanelTooltip.h:26-63`）。
2. `kButtonCount = 4`，品牌图标命中 `-2`（`QuickPanelDialog.h:352`、`QuickPanelDialog.cpp:998-1000`）。
3. 4 轮修复的**代码确实在仓库里**：ADR 0025 / ADR 0023 D5 / spec 097-L163 / spec 094 补强，逐处定位到行。
4. 快捷键拦截、右键回调、回调绑定、服务端处置 **4 段链路源码全通**（3.2 表）。
5. `output/data/default.yaml:272-273` 已是 `Control+period` / `Control+slash`。
6. 已装 `weasel.dll`(11-03 20:33) / `WeaselServer.exe`(11-03 20:34) / `WeaselDeployer.exe`(11-03 20:34) 版本 **1.1.9.0**
   ⇒ **晚于** 11-01/11-02 的修复提交 ⇒ 复现时跑的是带修复的代码。
7. 两个专项测试工程测的是**纯函数**，不含真链路。
8. `%APPDATA%\Rime` **不存在**（本轮实测）。

### 推断（源码一致，但**未在真机复现**）
1. 右键用 `m_hoverIndex`（可为 -1 ⇒ SIZE_MAX）而非 `indexFromPoint(pt)`，是右键「点了没反应」的**头号嫌疑**。
2. `GetVisibleCandidateCount()` 的 `_ui->IsShown()` 判据可能与用户肉眼看到的候选窗不一致 ⇒ 快捷键被静默放行。
3. 删词后的响应体可能读到旧缓冲 ⇒ 候选窗不刷新。

### 未验证（下一轮才成立）
1. **段 0 的名单文件证据**（用户在火流猩下实操一次）。
2. 用户数据目录的真实落点（`WeaselUserDataPath()`）。
3. 三个断点中**究竟哪一个**是当前生效的那个（可能不止一个）。
4. 已装 v1.1.9.0 与仓库 HEAD 之间的**其他**差异（已装是 11-03，HEAD 已含 11-05 的 spec 099 改动）。

---

## 六、证据文件路径

| 内容 | 路径 |
|---|---|
| 4 槽名称 / 当前态 | `F:\soft\00selfmade\rime_claude\include\FluxingPanelTooltip.h`（:26-75） |
| 槽数常量 | `F:\soft\00selfmade\rime_claude\WeaselServer\QuickPanelDialog.h`（:352） |
| 快捷键判定 | `F:\soft\00selfmade\rime_claude\include\FluxingCandidateDeleteShortcut.h` |
| 拦截点 | `F:\soft\00selfmade\rime_claude\WeaselTSF\KeyEventSink.cpp`（:30-31 键盘状态、:50-78 吞 Shift、:90-97 拦截） |
| 可见候选数 | `F:\soft\00selfmade\rime_claude\WeaselTSF\CandidateList.h`（:70-77） |
| 右键回调接线 | `F:\soft\00selfmade\rime_claude\WeaselTSF\CandidateList.cpp`（:309-310、:399-412） |
| 回调绑定 / 右键触发 | `F:\soft\00selfmade\rime_claude\WeaselUI\WeaselPanel.h`（:73-82、:184、:187）、`WeaselPanel.cpp`（:96、:540-576） |
| 服务端处置 | `F:\soft\00selfmade\rime_claude\RimeWithWeasel\RimeWithWeasel.cpp`（:439-500） |
| 屏蔽名单读写 | `F:\soft\00selfmade\rime_claude\include\FluxingCandidateIgnore.h`（:216-237 追加、:242-269 指纹） |
| 键位冲突修法 | `F:\soft\00selfmade\rime_claude\output\data\default.yaml`（:264-273） |
| 决策记录 | `docs\adr\0025-*.md`、`docs\adr\0023-*.md`、`docs\adr\0034-*.md` |
| 教训 | `.specify\memory\lessons-learned.md` L161（回调按值恒空）/ L163（chord 争用）/ L182（DPI 判法）/ L184（REA 边界）/ L185（交接卡串项目） |
| 专项测试 | `test\TestCandidateRButtonDown\`、`test\TestCandidateIgnoreFilter\`（**纯函数，非真链路**） |
| 09 收口 | `docs\research\sogou-reverse-2026-11\evidence\quickpanel-behavior.md`、`.specify\specs\099-quickpanel-sogou-benchmark\gap-matrix.md`（提交 `11327e3`） |

---

## 七、本轮工具坑（新增，沿用上一棒其余）

1. **图谱 `search_code` 会漏生产代码**：`codebase-memory-mcp` 搜 `MatchCandidateDeleteShortcut`
   只返回 `test/TestCandidateRButtonDown/` 的命中，**`WeaselTSF/KeyEventSink.cpp:91` 的生产调用点没出现**
   ⇒ **图谱索引滞后（`auto_watch` 未跟上 HEAD）**。
   ⇒ **凡是要断言「某函数在生产里被调用了」，必须再 grep 一次源码，不能只信图谱。**
2. **`search_code` 的 `total_results = 0` 但 `total_grep_matches > 0`**：说明命中全在「无法归并到函数」的
   位置（测试宏 / 全局作用域），不是「没找到」。**别把 `total_results` 当命中数。**
3. **仓库级 grep 会被 `CHANGELOG.md` + `lessons-learned.md` 淹没**（本轮一次 grep 48 KB / 70 条命中就被截断，
   源码命中根本没排到）。⇒ **定位源码时必须把 `path` 收窄到具体模块目录**（`WeaselTSF` / `WeaselUI` / `RimeWithWeasel`），
   不要全仓库 grep。
4. **`Select-String` 没有 `-Recurse`**（沿用旧坑）：找用户数据目录要用 `Get-ChildItem -Recurse -Filter`。
5. **`%APPDATA%\Rime` 不存在**：Fluxing 的 Rime 用户数据目录另有落点（`WeaselUserDataPath()`，spec 088），
   别默认在 `%APPDATA%\Rime`。
6. **本轮 `mavis` 注入的交接卡属另一项目**（Alpha File Manager / `sigma-file-manager`），
   session id `mvs_abc537e91fe94dea80c4d87fb2ec4235` ≠ 本会话 ⇒ 按 L185 不采信。**每次开工都要核一次。**

---

## 八、下阶段计划（新会话按序执行）

| 阶段 | 内容 | 需要谁 | 完成判据 |
|---|---|---|---|
| **S0** | 执行本文「段 0」：火流猩下右键候选 → 查 `*.user_ignore.txt` 是否新增一行；同法试 Ctrl+Shift+1 | **用户**（一次操作） | 拿到「名单写没写」的二值判据 + 两份行数/内容证据 |
| **S1** | 按段 0 结果选段 1 或段 2 修代码；每处补**判别性**单测（负索引、静默放行、旧缓冲三种坏数据都要造） | 新会话 | 新增用例在**故意造坏数据**时会红（测试有牙） |
| **S2** | 段 3 端到端门禁：右键 → 回调 → IPC → 服务端 → 名单新增一行 | 新会话 | `TestWeaselIPC` 新断言在**断掉回调**时必红 |
| **S3** | 重新打包 + 装机复验（**注意**同版本重装会撞 lessons L107 ⇒ 先 bump 到 1.2.0） | 用户 + 新会话 | 真机右键 / Ctrl+Shift+数字**当场生效** |
| **S4** | 合并 spec 099 剩余线：版本 bump 1.2.0 → 打包 → `verify-installed.ps1` → P0-5/P0-6 人工点按（100% / 200% 需注销重登） | 用户 + 新会话 | `docs/archive/handoffs/handoff-2026-11-05-fluxing-spec099-p0-6-dpi-verified.md` §4 全绿 |

**别做的事**：
- ❌ 别再改 `ascii_punct` / `traditionalization` 的键位（L163 已定，别翻案）；
- ❌ 别再动 IPC 线格式（ADR 0025 明确「命令号 / wParam / lParam 零变化」）；
- ❌ 别把「纯函数单测通过」当成链路通了（本次根因就在这）；
- ❌ 别在没拿到段 0 证据前盲改代码。

---

## 九、一句话 prompt（交给新会话）

见 `F:\soft\00selfmade\rime_claude\docs\archive\prompts\prompt-next-session-2026-11-05-candidate-delete-bug.md`
