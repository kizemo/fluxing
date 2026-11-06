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

---

## 十、本棒（2026-11-05 下午 · 段 1 落地 + 生产编译 + 1.2.0 打包）

### 10.1 段 0 判据结果（决定后续一切）

- 两处用户数据目录 `D:\Program Files\fluxing\user1\fluxing`（= `HKCU:\Software\Fluxing\Weasel` 的 `RimeUserDir`）与 `%APPDATA%\FluxingUserData\user1\fluxing` **均无任何 `*.user_ignore.txt`**（历史上也从未出现过）⇒ 按 §4 判定表：断点在**客户端 → 服务端之间**，不进服务端。
- **复验时两个目录都要查**，只查一处会误判「没生效」。

### 10.2 真实根因（已验证，逐行；已记 L186）

`WeaselUI/WeaselPanel.cpp`：

1. `OnMouseMove` 首行 `if (m_style.hover_type == UIStyle::NONE) return 0;`（HEAD 行号 493）⇒ `hover_type: none` 时 `m_hoverIndex` **永不赋值**；
2. `OnMouseLeave`（:544-552）复位 `-1`；初值 `int m_hoverIndex = -1;`（`WeaselPanel.h:187`）；
3. `OnRButtonDown` 旧守卫（:558）`if (hide_candidates || m_hoverIndex < 0) { bHandled = true; return 0; }` ⇒ **静默 return**，`m_deleteCallback` 与 IPC 一次都不发。

`hover_type` 默认三处一致为 NONE：`include/WeaselIPCData.h:205`（`enum HoverType { NONE, SEMI_HILITE, HILITE }`）、`:309 hover_type(NONE)`、`output/data/weasel.yaml:85` = `hover_type: none`；本机生效产物 `D:\Program Files\fluxing\user1\fluxing\build\weasel.yaml:523` 亦 `none`（用户 `weasel.custom.yaml` 只 patch 了 `style/color_scheme: flypy`）。
⇒ **默认配置下必然失效**，与用户机器、与 DLL 版本无关。

### 10.3 已排除的两个假说（别再翻）

- **「装机二进制过期」不成立**（verifier `bg_12029257-2533-43a6-bada-1d0b888fe54a` succeeded）：64-bit view 的 `CLSID\{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}\InprocServer32` = `weaselx64.1.1.9.0.2.dll`（hash `C401266DA8C37EF1`），`weaselx64.dll`(1.1.7.0) 只是残留；1.1.8↔1.1.9 `.text` 差 0 字节；三棵源码树删词链路 blob 全同（`WeaselPanel.h 553755d3` / `WeaselPanel.cpp 1be23df6` / `FluxingCandidateDeleteShortcut.h f5cacfc6`）。
- **「服务端 UI 未创建所以无回调」不成立**：全仓 `ShowUI|showUI|m_showUI` 只命中 `WeaselTSF.h:140` / `ThreadMgrEventSink.cpp:23` / `CandidateList.cpp:370` ⇒ `Status` 里根本没有 showUI 字段；可见候选窗由 TSF 在应用进程内创建（`CandidateList.cpp:313-316`、`:353-356`）。

### 10.4 本棒改动（4 文件）

| 文件 | 改动 |
|---|---|
| `include/FluxingCandidateHitTest.h` | 新增（2764 B，LF+BOM，与 `include/FluxingCandidateIgnore.h` 同风格）。`fluxing::CandidateHitRect{RECT rect; int offsetY;}` + `inline int HitTestCandidateIndex(items, pt, istorepos, padX, padY)`；逐字复刻 `OnMouseMove` 口径 `OffsetRect(0,offsetY)`（istorepos 时）→ `InflateRect(padX,padY)` → `PtInRect`；只依赖 `<windows.h>`，不碰 ATL/IPC |
| `WeaselUI/WeaselPanel.h` | `void _IgnoreCurrentCandidate();` 之后新增 `int _HitTestCandidateIndex(CPoint point);`（CRLF 无 BOM 保持不变） |
| `WeaselUI/WeaselPanel.cpp` | 顶部新增 `#include <vector>` / `#include <FluxingCandidateHitTest.h>`；实现 `_HitTestCandidateIndex`（`if (!m_layout) return -1;` + `for (size_t i = 0; i < m_candidateCount && i < MAX_CANDIDATES_COUNT; ++i)` + `item.rect = m_layout->GetCandidateRect((int)i);` + `item.offsetY = m_istorepos ? m_offsetys[i] : 0;` + `DPI_SCALE(m_style.hilite_padding_x/y)`）；重写 `OnRButtonDown`：`hide_candidates` 单独早退 → `index = m_hoverIndex; if (index < 0) index = _HitTestCandidateIndex(point);` → `index < 0` 早退 → 同 index 100 ms 去抖 → `m_deleteCallback(index)`；`bHandled = true; return 0;` 保持。**未**接回 UI 侧过滤（ADR 0023 D9 仍成立），**未**改 IPC 线格式（ADR 0025） |
| `test/TestCandidateRButtonDown/TestCandidateRButtonDown.cpp` | `FakePanel` 升级为「hover 优先、否则坐标命中」双源；T2 语义改为「既没 hover 也没命中 ⇒ 不派回调不发 IPC」；新增 **T10**（`m_hoverIndex=-1` + 点第 3 格 ⇒ 回调 1 次 index=2；点空白 ⇒ 仍 1 次；**修复前必挂**）与 **T11**（padding 扩张 / reposition 偏移 / `PtInRect` 右开下开 / 空列表恒 -1）；末行 `11 / 11 assertions passed` |

文档：`.specify/memory/lessons-learned.md` 新增 **L186**（+2890 B）；`CHANGELOG.md` 在 `## [Unreleased]` 下新增 `### fix(WeaselUI): 右键删候选在 hover_type: none 下静默失效`（+1645 B）。

### 10.5 编译与测试证据（已验证）

- 单测：`cmd /c "F:\soft\00selfmade\rime_claude\.superpowers\sdd\build_test_rbutton.cmd"` ⇒ `11 / 11 assertions passed` / `No errors detected` / `TESTRUN_EXIT=0`；exe 落在 `release\TestCandidateRButtonDown.exe`（**不是** `output\`）。
- 生产编译：`\.superpowers\sdd\build_weaselui.cmd`（vcvars64 + `msbuild WeaselUI\WeaselUI.vcxproj /t:ClCompile /p:Platform=x64 /p:BuildProjectReferences=false /p:BOOST_ROOT=F:\b183 /p:SolutionDir=%R%\`）⇒ `WEASELUI_EXIT=0`，唯一告警 `WeaselPanel.cpp(167,22) C4267 size_t→BYTE` 是**既有**代码（HEAD 165 行 `m_candidateCount = min(...size(), MAX_CANDIDATES_COUNT)`，被我新增的 2 行 include 下移）。
- 整包 + 安装器：`cmd /c build.bat weasel installer data`，日志 `\.superpowers\sdd\build_120.log`；14:46 已进入 makensis，产物 `output\archives\fluxing-1.2.0-installer.exe`。

### 10.6 版本号 1.1.9 → 1.2.0（L107 同版本重装会撞车，必须先 bump）

三处同步：本地 `env.bat`（**gitignored**：`FLUXING_VERSION` / `PRODUCT_VERSION` / `FILE_VERSION` / `VERSION_MAJOR|MINOR|PATCH`，`RELEASE_BUILD=1` 保持）、tracked `build.bat` 兜底数字（L158 要求同步，否则 CI/新克隆产出旧版本）、`version.txt`。`weasel.props` 是 `build.bat :render_props` 用 `cscript render.js` 生成的（.gitignore:32），**不要手改、不要提交**。门禁 `scripts/gate/L0-static.ps1:167`（RC7）与 `L3-mutation.ps1:161`（M03）会查这四者一致性。

### 10.7 下一棒要做的事（按序）

1. **装机端到端复验**（这是「链路通了」的唯一判据，纯函数绿不算）。装 1.2.0：先 `cmd /c "taskkill /F /IM WeaselServer.exe"`，再清 `HKLM:\SOFTWARE\WOW6432Node\Fluxing` 与 `HKCU:\Software\Fluxing`，再 `installer.exe /S /D=D:\Program Files\fluxing`（`/D=` 必须最后且不带引号）。验：① 右键点候选 → 该候选消失且 `build\..\ <schema_id>.user_ignore.txt` 出现；② 单独按 `Ctrl+Shift+1` 的 32/64 位两条路径；③ 托盘「恢复已忽略的候选项」能撤销。
2. **`Ctrl+Shift+数字` 入口仍只有源码在场证据**（`WeaselTSF/KeyEventSink.cpp:90-99` 拦截块 + `CandidateList.h:73-77 GetVisibleCandidateCount()`，`!_ui || !_ui->IsShown()` ⇒ 0 ⇒ 同样「什么都不发生」）；用户至今只回报跑了右键。若真机复验仍不通，**先取 `IsShown()` 运行态**。
3. **快捷设置栏对标搜狗（spec 099）—— 本棒未动，且状态比上一棒 handoff 记录的好**：`CHANGELOG.md` 的 `## [Unreleased]` 已有 3 条 feat（`L18` P0-7 位置持久化、`L32` 4 槽全部有确定效果 / 槽 0 切方案 + 槽 3 关于、`L2` P0-4/5/6 状态回显 + 键盘可达 + tooltip + DPI），说明这条线**大部分已落地并 commit**。 ⇒ 开工前**必须**先读 `.specify/specs/099-quickpanel-sogou-benchmark/tasks.md`（18432 B）与 `gap-matrix.md`（20129 B）核对剩余缺口，别按 §8 的旧清单重做已完成项。
4. commit 前跑 `git status -sb` + `git diff --stat`（`env.bat` / `weasel.props` / `*.obj` / `.superpowers/` 不得入库），按 Conventional Commits + 模块前缀 `WeaselUI`。

### 10.8 本棒新增工具坑（实测）

- **锚点唯一性救了一次**：`## [Unreleased]` 在 CHANGELOG 里命中 **2 次**（L1、L5498）⇒ 用带下一行的长锚点 `## [Unreleased]\r\n### feat(WeaselServer)`（命中 1 次）。throw 在写盘前，文件没坏（铁律 05 / 23）。
- **保行尾最省事的做法**：纯 ASCII 替换用 `[Text.Encoding]::GetEncoding(28591)`（Latin-1，字节 1:1）读→`Replace`→同编码写字节，任何编码/行尾都不会被改坏；含中文的文件用 `UTF8.GetString` + `New-Object Text.UTF8Encoding($bom)` 写 bytes。
- `Get-ChildItem -Include` 与 `-Path` 混用时我写成 `Get-ChildItem $R -Include ... -Path ...` 会静默无输出，别依赖它判进度；判构建进度直接看 `Get-Process makensis/msbuild/cl` + `output\archives\` 新文件 + 日志 tail。
- `Select-String` 无 `-Recurse`；`foreach {} | Select-Object -First N` 报「不允许空管道元素」；脚本内 `Set-Location` 被硬策略拒绝。
- PowerShell 里给字符串补零对齐用 `'{0,4}: {1}' -f $i,$_` 必须写成 `(Get-Content ...)[a..b] | ForEach-Object { ... }`，我上面 `{ '{0,4}...' -f ... }` 的花括号把脚本块当对象打印出来了 —— 输出是 `'{0,4}: {1}' -f $_` 原文时就是这个坑。

### 10.5 装机（1.2.0）已完成 —— 端到端复验的前置与判据

1. **已静默装上本机**：`cmd /c .superpowers\sdd\install_120.cmd` ⇒ `INSTALL_EXIT=0`（配方：直接调用安装包 + `"/S" "/D=$INSTDIR"` 作**裸尾参**，`/D=` 不带引号；见 `project-knowledge.md §11.1`）。装机前快照 `\.superpowers\sdd\pre_install_120_snapshot.txt`（42 文件 name|size|SHA256 前 16 位）留作回滚参照。**本棒没有删任何注册表键** —— 版本已 bump，不需要 L107 那套清理，也避免动到 `RimeUserDir` 指向的用户数据。
2. **装机产物已取证**：`WeaselServer.exe` / `weaselx64.dll` / `weasel.dll` / `WeaselDeployer.exe` 全部 fv=pv=`1.2.0.0`；装机 `weaselx64.dll` 与 `output\weaselx64.dll` **SHA256 相同**；`output\weaselx64.pdb` 内含 `_HitTestCandidateIndex` ⇒ 新代码确实在出货二进制里。安装器覆盖的是**规范名** `weaselx64.dll` / `weasel.dll`（未生成 `weaselx64.1.2.*` 版本化 shim），残留 `.old.tmp` 已清；TSF 注册表 64-bit view 与 WOW6432Node 里 fluxing 的 `InprocServer32` 只剩这两条路径。WeaselServer 由安装器自启（新 exe）。
3. **复验前必须重启目标应用**（否则测的是旧客户端，会得出假阴性）：装机不会替换已在运行进程里已映射的 DLL —— 实测 14 处仍映射 `weaselx64.1.1.9.0.2.dll`（explorer / Excel / Edge / 飞书 / 钉钉 / 微信 / MiniMax Code / ToDesk 等）；注销或重启最彻底。目标应用 32 位加载 `weasel.dll`、64 位加载 `weaselx64.dll`，两条都已带修复。
4. **生效配置仍是 `hover_type: none`**（`D:\Program Files\fluxing\user1\fluxing\build\weasel.yaml`，mtime 2026-11-03，未被本次装机改写）⇒ 复验正好落在修复路径上，**不要**先把 hover_type 改成 `mouse` 再测。忽略名单基线 = 两处用户数据目录共 **0** 个 `*user_ignore*`。
5. **同批次交接文档**（`handoff-2026-11-05-fluxing-quickpanel-ui-acceptance.md`，同项目同批次 ⇒ 采信）记录 1.1.9 真机 3 条与本条线同源的症状：右键点候选「候选窗直接消失、不弹菜单」；`Ctrl+Shift+1..9` **与** `Ctrl+Alt+1..9` 全无反应；候选窗 hover 无高亮。前两者与 hover 无高亮都由 `hover_type: none` + 旧 `m_hoverIndex` 守卫解释。**`Ctrl+Shift+<数字>` 是独立的第二处断点，本棒未修** —— 唯一剩余判据：重启过的目标应用里、候选窗已弹出时单按 `Ctrl+Shift+1`；若仍无反应，先取 TSF 侧 `WeaselPanel::IsShown()` 运行态（`CandidateList.h:73-77` 在 `!_ui || !_ui->IsShown()` 时返回 0 ⇒ 与本次缺陷同构的静默失效），**别改代码**。用户已授权：若与其它快捷键冲突可改 `Ctrl+Alt+数字`（本棒判定数字族已腾空 ⇒ 未动键位）。
6. 更正一处误记：`weasel.user.yaml` **在本机两个用户数据目录都不存在**，源码 `findstr` 亦无引用 ⇒ 装机没有删用户数据；`weasel.custom.yaml`(264 B/10-22) / `default.custom.yaml` / `rime_ice.custom.yaml` / `double_pinyin_sogou.custom.yaml` / `radical_pinyin.custom.yaml` / `phrases.yaml` / `installation.yaml` mtime 全部早于装机，原样保留（`user.yaml` 被部署器 14:56 重写，内容与 APPDATA 副本同长 146 B）。
7. commit 前唯一待核事实：`git status` 未列 `version.txt`（本棒写入 `1.2.0`）⇒ 确认它是 gitignore 还是已进 ahead 3 的某个 commit。

## 十一、本棒（2026-11-06 凌晨 · 版本同步收口 + 门禁全绿 + 真机 E2E 被锁屏挡住）

### 11.1 已做（可提交的部分）

1. **版本号三处同步补齐**：`version.txt` 仍是 `1.1.9`（§10.7-7 那条「待核事实」**已坐实为真缺陷**）→ 改成 `1.2.0`（5 字节、无 BOM、无换行，与 gate 的形状要求一致）；`build.bat` 的 `VERSION_MINOR/PATCH` 已是 `2/0`；本地 `env.bat` 已是 `1.2.0`（gitignored，不入库）。
2. **`CHANGELOG.md` 缺 1.2.0 发布段** → 按仓库惯例切出 `## [1.2.0] - 2026-11-05`（gate 的 `version-consistency` 取的是**首个数字版本头**，原先是 `1.1.9` ⇒ 判红）。
3. **`.specify/memory/lessons-learned.md` 丢了 UTF-8 BOM**（L186 那次写入的后果）→ 字节级前置 `EF BB BF`，14000 处 CRLF 与 904850 字节正文一字未动。
4. **格式化**：CI 的 lint 门禁「只检查本次 push 触及的文件」⇒ 用 `git clang-format --force HEAD` 只重排本次改动行（测试文件 T10/T11 段）。`WeaselPanel.cpp/.h` 的 84/18 条违规是 **HEAD 既有欠账**（改动前后条数相同），不做无关重排。
5. **门禁全绿**：`L0-static -Mode all` 12/12（`nsis-compile` 在完整跑那次 PASS）、`L1` 15/15、`L3` 变异 **7/7 全杀**（M03 现在会连 CHANGELOG 顶部版本一起判红）。
6. **单测**：`release\TestDefaultHotkeys.exe` **91/91 PASS**（exit 0）；`release\TestCandidateRButtonDown.exe` **11/11 PASS**（exit 0，含 T10「修复前必挂」）。

### 11.2 真机 E2E：被「桌面已锁屏」挡住（**不是代码问题**）

- **现象链**：`GetForegroundWindow()` 返回 **0**、`Graphics.CopyFromScreen` 抛「句柄无效」、枚举却能看见记事本/Edge/微信等真实窗口；`LogonUI` 进程在跑 ⇒ **会话处于锁屏态**。锁屏时 Windows 拒绝输入注入与屏幕捕获 ⇒ 「右键删候选」这条判据在**无人解锁时物理上无法自动完成**。
- **中途两次「假失败」已排除，别再翻**：
  - Tk 的 `Text` 是**自绘控件，不吃 IME** ⇒ 没有候选窗是正常现象，不是缺陷复现。
  - 打包版记事本的窗口**不在 `notepad.exe` 的 PID 里** ⇒ 必须全进程枚举按类名 `Notepad` 找，按启动进程 PID 找必然扑空。
  - `ctypes` 侧：`GetForegroundWindow` 不声明 `restype=HWND` 会把 64 位句柄**截断成 0**（误判成「抢不到焦点」）；`EnumWindows` 的回调必须是 `WINFUNCTYPE` 实例且要给 `argtypes`。
- **复验怎么做（解锁后一条命令）**：
  `F:\soft\00selfmade\rime_claude\.superpowers\sdd\verify_candidate_delete_manual.cmd`
  它会起一个记事本、提示输入 `nihao` 并**右键第一个候选**，随后自动监视两处用户数据目录里新出现的 `*user_ignore*` 文件，打印 PASS/FAIL 与文件内容。
- **复验前必做：重启目标应用**（装机不替换已运行进程里已映射的 DLL）。本机实测映射情况：
  - 仍映射**旧** `weaselx64.1.1.9.0.2.dll`：`explorer` / `msedge` / `msedgewebview2` / `MiniMax Code` / `Feishu` / `DingTalk` / `ToDesk` / `Claude Code Haha` / `ShadowBot.Shell`；
  - 映射**规范名** `weaselx64.dll`（= 1.2.0，**修复在位**）：`Notepad`（00:16:43 启动，晚于 11-05 14:44 装机）/ `EXCEL` / `powershell` / `SearchHost` / `Doubao` / `ApplicationFrameHost`。
- **其它环境事实**：本机已排每日 **03:00 自动关机**计划任务 `Fluxing-Daily-Shutdown-0300`（下次 2026/11/6 03:00，`shutdown /s /t 60`）；`D:\Program Files\fluxing\user1\fluxing\build\weasel.yaml` 的 `hover_type` 仍是 `none`（**不要**先改成 `mouse` 再测）；两处用户数据目录的 `*user_ignore*` 基线仍是 0。

### 11.3 本棒新增工具坑（实测）

- `ctypes.cast(int, c_void_p)` 直接抛「Don't know how to convert parameter 1」——回调里的 HWND 已是 int，用 `int(hwnd)`。
- `EnumWindows` 不给 `argtypes` 就传回调 → 「expected WinFunctionType instance instead of function」；给了 `argtypes` 后又必须把回调包成 `WINFUNCTYPE` 实例。
- `GetForegroundWindow` / `SetForegroundWindow` / `SetFocus` / `AttachThreadInput` 等 64 位 HWND API **必须**显式声明 `restype/argtypes`，否则句柄被截断成 0。
- 打包应用（记事本）**窗口不在启动进程 PID 里**；按 PID 枚举必然扑空。
- 自绘控件（Tk Text）不吃 IME；验证 IME 链路必须用**原生 EDIT 控件**。
- 锁屏会话下输入注入与屏幕捕获**静默失败**：`GetForegroundWindow()==0` + `CopyFromScreen`「句柄无效」就是锁屏指纹（枚举窗口仍然正常，别被误导成「看不见桌面」）。
- `taskkill /F /IM python.exe` 会连**别的项目的** python 一起杀（本机一次干掉 8 个）——要按 PID 精确杀。
- 长驻子进程会持有父进程的 stdout 管道 ⇒ `| Select-Object -Last N` 永远等不到 EOF（子进程退出前管道不关）。把输出重定向到文件，或让子进程自己脱离。
- `git clang-format` 拒绝在有未暂存改动时运行，要加 `--force`。

### 11.4 下一棒（按序）

1. 解锁桌面 → 跑 `verify_candidate_delete_manual.cmd` → 右键第一个候选 → 看 PASS/FAIL。
2. PASS 后：把结果回填到本文件 §十一，并把接力 prompt 改成「已复验」版本。
3. 未 PASS：**先取 TSF 侧运行态**（`WeaselPanel::IsShown()`），**别改代码**（与本缺陷同构的静默失效，§10.7-2 已写明）。
4. `Ctrl+Shift+<数字>` 那条独立断点**仍未修**（§10.7-2），用户已授权：若冲突可改 `Ctrl+Alt+数字`。
5. spec 099 快捷设置栏那条线**另棒继续**：按 `.specify/specs/099-quickpanel-sogou-benchmark/{spec.md,tasks.md,gap-matrix.md}` 与同批次 `docs/archive/handoffs/handoff-2026-11-05-fluxing-quickpanel-ui-acceptance.md` 核对剩余缺口，**勿重做已完成项**。

### 11.5 本棒 commit 划分（scope 单一，避免 P4 判红）

- `fix(WeaselUI)`：`WeaselUI/WeaselPanel.cpp` + `.h` + `include/FluxingCandidateHitTest.h` + `test/TestCandidateRButtonDown/TestCandidateRButtonDown.cpp` + `.specify/memory/lessons-learned.md` + `CHANGELOG.md` 的修复条目 + 本 handoff + 接力 prompt。
- `chore(installer)`：`version.txt` + `build.bat` + `CHANGELOG.md` 的 `## [1.2.0] - 2026-11-05` 发布段。

不入库：`env.bat` / `weasel.props`（gitignored）、`librime`（子模块只有本地构建脏文件，**指针未变**，别 `git add librime`）、`.superpowers/`（工具与日志）。

## 十二、本棒（2026-11-06 上午 · 真机端到端复验 **通过**）

### 12.1 结论：判据已满足

§10.7-① 的唯一可信判据（候选消失 + `<schema_id>.user_ignore.txt` 出现）**已满足**：在 `hover_type: none` 生效配置下右键第一个候选 ⇒ 该候选被忽略 ⇒ 真实用户数据目录生成 `double_pinyin_sogou.user_ignore.txt`。

- 复验窗口：2026-11-06 07:29–07:38（桌面已解锁，`LogonUI` 进程数 = 0；03:00 计划关机已执行过，机器重启后所有进程都是新进程）。
- 探针状态：**连续 3 次 `PASS_IGNORE_FILE_CREATED`**，每次都往 `user_ignore.txt` 追加一个真实候选词：

  | 次序 | 候选面板 rect（屏幕坐标） | 右击点 | 追加的忽略词 |
  |---|---|---|---|
  | 1 | 214,248,544,297 | (240,272) | `你哈哦` |
  | 2 | 214,248,448,297 | (240,272) | `你` |
  | 3 | 214,248,486,297 | (240,272) | `拟哈哦` |

  面板宽度随被忽略的候选变化（330 → 234 → 272）⇒ 候选列表真的在刷新，不是假写盘。
- 右击后截屏（`evidence/e2e_candidate_after.png`）显示候选栏区域已空 ⇒ **候选确实从界面消失**，不只是名单写盘。
- 生效二进制：`HKLM\SOFTWARE\Classes\CLSID\{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}\InprocServer32` = `D:\Program Files\fluxing\weasel\weaselx64.dll`，该 DLL `FileVersion = 1.2.0.0`（`weasel.dll` 同为 1.2.0.0）；候选面板 HWND 属于**目标应用进程 PID** ⇒ 该进程经 TSF 加载的就是这版 DLL。

### 12.2 探测方法的三处更正（前几次「失败」全是探针写错，不是缺陷）

1. **候选窗类名不含 `weasel`**：`WeaselPanel` 继承 `CWindowImpl<WeaselPanel, CWindow, CWeaselPanelTraits>`，`CWeaselPanelTraits` = `WS_POPUP | WS_CLIPSIBLINGS | WS_DISABLED` + `WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED` ⇒ ATL 默认类名是 `ATL:xxxxxxxx`。按 `weasel` / `fluxing` 关键字匹配的探针**必然扑空** —— §11.2 报的 `NO_CANDIDATE_WINDOW` 就是这个原因。
   **正确做法**：枚举**目标进程 PID 的顶层窗口**，挑 `cls` 以 `ATL:` 开头、可见、宽 > 高的横向条 ⇒ 即候选面板。
2. **现代记事本（WinUI）不吃 TSF**：`Microsoft.WindowsNotepad` 里打 `nihao` **不弹候选栏**（截图确认：只有文本，无候选条）⇒ 复验目标必须是**原生 EDIT 控件**（本棒用 WinForms `TextBox`）。与 §11.3「Tk Text 不吃 IME」同源，但**记事本与 Tk 表现不同，别混为一谈**。
3. **PowerShell `@($null).Count` 返回 1（不是 0）** ⇒ 用它判「文件个数」会把「一个文件都没有」读成「1 个文件」。本棒第一次因此误报 `ignore_after=1`，**差点把「没删成」读成「已 PASS」**。必须先判 `$null`：`if ($null -eq $r.x) { 0 } else { @($r.x).Count }`。

### 12.3 本棒新增工具坑（实测）

- `D:\Program Files\` 属受保护路径，**删除操作被硬安全策略拦下** ⇒ 复验残留的 `user_ignore.txt` 删不掉，需用户手工清理（§12.4）。
- `Start-Process` 起目标后若目标进程提前退出（用户手关窗口等），事后 `Get-Process -Id … -Modules` 会报「找不到进程标识符」⇒ **模块取证必须在输入之后立刻做**，并预设目标可能已死。
- 候选面板虽是 `WS_POPUP` 顶层窗口，但**只在宿主进程 PID 内枚举才找得到**；全屏枚举时它只是众多窗口之一，且类名匹配不上。
- `Get-Process -Modules` 查询 64 位进程时，查询器也须是 64 位（用 `powershell.exe` 而非 32 位宿主）。

### 12.4 需要用户手工处理的一件事（复验副作用）

复验在真实用户数据里留下 3 个被忽略的候选词（`double_pinyin_sogou.user_ignore.txt`，27 B）：

```
你哈哦
你
拟哈哦
```

其中 **`你`** 是常用词，会影响日常输入。删除路径受保护、agent 删不掉，请手工删除该文件：
`D:\Program Files\fluxing\user1\fluxing\double_pinyin_sogou.user_ignore.txt`
（内容副本已留在 `.superpowers\sdd\evidence\user_ignore_final.txt`，便于核对。）

### 12.5 仍未验证 / 未修（别当已解决）

- **`Ctrl+Shift+<数字>` 第二处断点未复验也未修**（§10.7-2 / §11.4-4）：本棒只验了右键路径。复验须在**候选窗已弹出**的状态下单按 `Ctrl+Shift+1`；若仍无反应，先取 `WeaselPanel::IsShown()` 运行态，**别改代码**。
- **托盘「恢复已忽略的候选项」撤销未验**（§10.7-①③）。
- **模块级取证缺一格**：目标进程在事后枚举前已退出 ⇒ 没能直接打印「目标进程内 `weaselx64.dll` FileVersion=1.2.0.0」。证据链（CLSID → DLL 路径 → 文件版本 1.2.0.0 + 候选面板属于目标 PID + 修复后的行为）已闭合，但这一格属于**推断**。

### 12.6 复验脚本与证据（均在 `.superpowers/sdd/`，不入库）

| 文件 | 作用 |
|---|---|
| `e2e_probe3.py` | **最终探针**：WinForms 原生 EDIT 目标 → 强制前台 → 打 `nihao` → 定位 `ATL:` 候选面板 → 右击第一个候选 → 查 `*user_ignore*` → 前后截屏 |
| `e2e_target_winforms.ps1` | 起原生 EDIT 目标窗（写 `e2e_target.json`） |
| `e2e_candidate_delete.py` / `e2e_probe2.py` | 前两版探针（按类名匹配 / 全屏枚举），**留作反面教材**：都因类名假设扑空 |
| `verify_candidate_delete_manual.ps1` / `.cmd` | 纯人工复验脚本（用户双击即可，自动监测名单生成） |
| `evidence/e2e_probe3_result.json` | PASS 的完整结果（面板 rect、右击点、名单内容） |
| `evidence/e2e_candidate_before.png` / `e2e_candidate_after.png` | 右击前 / 右击后截屏（after 显示候选栏已空） |
| `evidence/user_ignore_final.txt` | 复验残留的名单内容副本（供用户核对） |

### 12.7 本棒 commit 划分

- `docs(fluxing)`：本 handoff 的 §十二 + 接力 prompt 改成「已复验」版本。**代码一行没再动**（上一棒的 `fix(WeaselUI)` = `065383e`、`chore(installer)` = `7181f56`）。
- 分支 `Fluxing...kizemo/Fluxing` 本棒前 **ahead 5**，加本棒 1 笔 ⇒ **ahead 6**，**仍未 push**（等用户点头）。

---

## 十三、本棒（2026-11-06 上午 · 用户真机反馈「仍无法删除」⇒ **任务未完成**，§十二 结论收窄）

### 13.0 用户反馈原文（最高优先级运行时证据）

2026-11-06 07:54 用户：

> 当前使用 ctrl+shift+数字，以及右键，仍然无法删除候选窗的候选词。

**判定：缺陷在用户真实场景下未修复 ⇒ 本线任务未完成。**

### 13.1 对 §十二「复验通过」的结论收窄（修订声明）

§十二 的 PASS **不推翻**，但**适用范围收窄**为：

| 维度 | §十二 实际覆盖 | 未覆盖 |
|---|---|---|
| 目标程序 | WinForms 原生 `EDIT` 控件（自建 `e2e_target_winforms.ps1`） | 微信 / 钉钉 / Edge / 用户真实编辑场景 |
| 操作 | 鼠标**右键** | `Ctrl+Shift+<数字>` |
| 落点 | 面板**左起第 26px**（约第一个候选左边缘） | 候选**文字中心** |
| 配置 | `hover_type: none` + `hilite_padding: 8` | 用户其它 `weasel.yaml` 差异 |
| 会话 | 桌面已解锁、目标程序前台 | 用户真实使用节奏 |

⇒ **不得对外宣称「缺陷已修复」**。§十二 只能证明：**在该目标 / 该落点 / 该配置下，删词链路（右键 → 写 `user_ignore.txt`）是通的**。

### 13.2 方法论缺陷（必须写明，否则下一棒会误用 §十二 的 PASS）

§十二 的 PASS **没有做对照组**：

- 未证明「装 1.1.9（修复前版本）时同一探针会失败」；
- 未证明「把命中守卫改回旧写法时同一探针会失败」。

⇒ 该 PASS **不能单独证明修复有效**，只能证明「在该目标 / 该落点下删词链路通」。
下一棒**必须补对照组**（靶子自己造，见 13.5-③），否则「探针有牙」这件事从未被证明。

### 13.3 本轮排除性取证（全部已定案，勿重复劳动）

| # | 假说 | 取证方式 | 结论 |
|---|---|---|---|
| 1 | 用户进程仍加载 **1.1.9 旧 shim** | 全进程扫描 + `Get-Process -Modules`，读 `weaselx64.dll` 路径与 FileVersion | **排除**：加载的全是 1.2.0 规范名 `D:\Program Files\fluxing\weasel\weaselx64.dll`（`weasel.dll` FileVersion `1.2.0.0`）。已见进程：Notepad(57788)、DingTalk、Weixin、explorer、WeaselServer、Claude Code Haha、MiniMax Code、WindowsTerminal、electron、aboboo、FluxingPhrasesDialog |
| 2 | 生效配置里 `hover_type` 不是 `none`（⇒ 走了别的分支） | 读生效配置 `D:\Program Files\fluxing\user1\fluxing\build\weasel.yaml:523`（mtime 11-03） | **排除**：`:523` = `hover_type: none`，`:534` = `hilite_padding: 8`；仓库默认 `output/data/weasel.yaml:85` 亦为 none ⇒ 落在修复路径上 |
| 3 | `m_layout` 为空导致 `if (!m_layout) return -1` | 读 `WeaselPanel.cpp` 调用链 | **排除**：`_CreateLayout()` 在 `UpdateUI` 路径 `WeaselPanel.cpp:198` 被调用 ⇒ 布局非空 |
| 4 | 右键消息根本没进处理函数 | 读消息映射 | **排除**：`WeaselPanel.h:43` `MESSAGE_HANDLER(WM_RBUTTONDOWN, OnRButtonDown)` 存在 |
| 5 | 命中判定与 hover 高亮**坐标系错位** | 对比 `OnMouseMove`(:507-527) 与 `_HitTestCandidateIndex`(:561-574) | **排除**：两者都用**客户区坐标** `GET_X_LPARAM(lParam)` + `m_layout->GetCandidateRect(i)` + `m_istorepos` 纵向偏移 + `DPI_SCALE(hilite_padding_x/y)` 扩张 + `PtInRect`。**但**：二者共享同一几何 ⇒ 若几何本身偏，则 hover 高亮与命中**同时**失灵（与同批次记录的「候选窗 hover 无高亮」症状同源，见 13.4-③） |

### 13.4 尚未排除的出口（下一棒重点）

- **① `OnRButtonDown` 首行静默早退（最高嫌疑）**
  `WeaselPanel.cpp:580-583`：`if (hide_candidates) { bHandled = true; return 0; }`
  `hide_candidates` 唯一赋值点 `WeaselPanel.cpp:191-192`：
  `inline_no_candidates || (margin_negative && !show_tips && !show_schema_menu)`
  初值 `false`（`:95`）。
  ⇒ **下一棒必须核对这两个布尔量在用户真实场景下的取值**（必要时加临时日志 / 调试钩子）。

- **② `GetCandidateRect` 与渲染文字矩形不是同一个**
  `StandardLayout.h:37` `GetCandidateRect` 返回 `_candidateRects[id]`（布局坐标）；渲染文字另用 `GetCandidateTextRect`。
  ⇒ 命中用前者 ⇒ **可能偏宽 / 偏窄**，需实测定。

- **③ hover 高亮是否真的生效（与 ① 同源）**
  用户若反馈「候选窗 hover 无高亮」，则 ① / ② 嫌疑大幅上升。

### 13.5 下一棒执行顺序（按此序走，不要跳）

1. **先问用户三件事**（缺一不可，直接决定后面怎么走）：
   - 在**哪个应用**里测的？（微信 / 钉钉 / Edge / 记事本 / 其它）
     ⚠️ **现代记事本是 WinUI，不吃 TSF，候选窗根本不弹** ⇒ 若用户答案是记事本，本身就解释得通「无效」。
   - 右击之后候选窗**是否消失**？有没有任何提示？
   - **hover 时候选是否高亮**？
2. **在用户指定应用里复跑 `e2e_probe3.py`**，右击点**精确到候选文字中心**（从截图量坐标），不再用「面板左起 26px」。
3. **补对照组**：证明同一探针在「修复前 / 旧守卫」下会失败 ⇒ 证明探针有牙。
4. 核对 `hide_candidates` 两个分量 + 右键消息是否真到达 `OnRButtonDown`（必要时临时日志）。
5. 若命中几何确实偏 ⇒ 补偿 `m_layout` 偏移，并用**真实 Layout** 写单测。
   ⚠️ **L186 教训：别抄 FakePanel** —— 用假 Layout 写的测试会与真实几何不一致，给出假绿。
6. `Ctrl+Shift+<数字>`：先取 `WeaselPanel::IsShown()` 运行态，再决定改键位还是修路径。

### 13.6 仍未验证 / 未修（继续有效）

- **`Ctrl+Shift+<数字>` 断点从未修**：`WeaselTSF/KeyEventSink.cpp:90-99` + `CandidateList.h:73-77` 的 `!_ui || !_ui->IsShown()` 守卫。用户 2026-11-06 再次确认无效，与 §10.7-2 / §12.5 一致 ⇒ **该断点确认为真断点，非探测误差**。
- **托盘「恢复已忽略的候选项」撤销未验**（§10.7-①③ / §12.5）。
- **残留用户数据仍在**：`D:\Program Files\fluxing\user1\fluxing\double_pinyin_sogou.user_ignore.txt`（27 B，内容 `你哈哦 / 你 / 拟哈哦`，含常用词「你」）—— 删除路径受保护、agent 删不掉，**仍需用户手工删除**。

### 13.7 本棒 commit 划分

- `docs(fluxing)`：本 handoff §十三 + 接力 prompt 改写为「任务未完成」版本。**代码本棒一行未改**，也未做新实验。
- 分支 `Fluxing...kizemo/Fluxing` 本棒前 **ahead 6**，加本棒 1 笔 ⇒ **ahead 7**，**仍未 push**（等用户点头）。
- 本棒新增证据文件（均在 `.superpowers/sdd/`，不入库）：`user_report_probe.log`（用户反馈复现探针日志）、`code_probe_hitbox.log`（命中几何代码取证日志）。

---

## 十四、崩溃根因闭环 + 修复落地（2026-11-06 会话）

> 本棒把 §13.4 #2 / §13.5 #2「右键崩」这条**从假设做成已验证事实，并当场落盘修复 + 造出能咬的回归测试**。
> 探索按 CLAUDE.md §2 应先走 `codebase-memory-mcp`（`F-soft-00selfmade-rime-claude`，`search_graph` → `trace_path` → …），
> 但本会话 `mcp_invoke` 以 `tool_name: "search_graph"` 调用返回 `Unknown tool_name`（图谱工具名未被解析），
> 已按 `.claude/rules/codebase-graph.md` 降级到 grep/read。**下一棒若图谱工具恢复，请改回图谱优先。**

### 14.1 已验证：崩溃链路（符号级 + 源码级 + 构建级三重证据）

**两次独立 WER 报告的偏移完全一致**（`weaselx64.dll` @ base `0x7FF9583E0000`，崩溃偏移 **`0x428ba`**）：

| 报告 | 应用 | 时间 |
|---|---|---|
| `d7bdb876-a248-334e-2480-c6199447f596` | MiniMax Code 3.1.10.203 | 2026-11-06 **07:49:39** |
| `43190ee7-cc65-36f8-a358-4a8717766c06` | Feishu 8.4.0.18 | 2026-11-06 **09:12:18**（用户答完三问后又崩了一次） |

符号化（`.superpowers/sdd/symprobe.py`，用 `Release\Win32` 下同名 PDB）：
```
0x428ba  Configurator::Store + 0xaa   WeaselIPC\Configurator.cpp:21
         函数入口 RVA 0x42810 = public symbol "Configurator::Store"（ClassParent 修正）
```
异常码 **`0xc000041d` = `STATUS_FATAL_USER_CALLBACK_EXCEPTION`** —— 它本身的含义就是
「异常从 OS 派发给用户态的回调里逃了出来」，与下面的通路定性互相印证；
首次异常是 `WeaselServer.exe` 侧记录的 `0xc0000005`（`STATUS_ACCESS_VIOLATION`，§13.1）。

**完整链路（每一环都有源码行号为证）**：
1. **两条入口共用同一条删除通路**：
   `WeaselTSF/KeyEventSink.cpp:90-99`（`Ctrl+Shift+<数字>`，位于 `CKeyEventSink::OnKeyDown` 内）
   与 `WeaselTSF/CandidateList.cpp:410-412` `HandleDeleteCandidate`（右键菜单 → `WeaselUI/WeaselPanel.cpp` 回调）
   ⇒ 都调 `WeaselTSF/CandidateList.cpp:399-408` `_DeleteCandidateOnCurrentPage`。
2. `CandidateList.cpp:405` `ResponseParser parser(NULL, &ctx, &_status, NULL, &_cand->style())`
   ⇒ 第 4 个实参使 `p_config == NULL`。
3. `RimeWithWeasel/RimeWithWeasel.cpp:1470-1474` 对**每一条**响应**无条件**追加 config：
   ```cpp
   // 无论是否存在配置变更, 都返回 inline_preedit 状态
   w << "config.inline_preedit" << "=" << (double(status.inline_preedit) ? "1" : "0") << "\n";
   actions.push_back("config");
   ```
   ⇒ 线上报头必含 `action=...,config,...`，`ResponseParser.cpp` 按名单逐个 `Require(L"config", pTarget)`，
   `Deserializer.cpp:24,48-49` 从工厂表取出 `Configurator` 实例。
4. 原 `Configurator::Store` 只判 `p_context`，下一句 `m_pTarget->p_config->inline_preedit = bool_value`
   解引用 NULL ⇒ `0xc0000005` ⇒ 在宿主进程内不可恢复 ⇒ **宿主被带走**。
   ⇒ **确定性崩溃，不是竞态**：只要走这条路必崩。

**线程定性（重要，别写错）**：快捷键那条 `KeyEventSink.cpp:95` 的同步删除**确实跑在 TSF
`ITfKeyEventSink::OnKeyDown` 回调里**，右键那条跑在 `WeaselPanel` 的 WndProc 里 ——
两者都是宿主 UI 线程。故本缺陷**不是**「某个非 TSF 通路」的例外。
遗留风险（本棒**不动**，交下一棒评估）：这条「OnKeyDown 内同步 IPC」本身与 constitution **P2**
（TSF 回调内禁止阻塞 I/O）的精神相悖，崩溃修掉之后仍值得单独开一条 spec 评估。

**同类 deserializer 审计（四对一）**：`Styler.cpp:13-14`、`ContextUpdater.cpp:97-98`、`Committer.cpp:13`、
`StatusUpdater.cpp:13` **都判了自己那个 target 指针** ⇒ 唯独 `Configurator` 漏判 `p_config` —— 这是**缺陷**，
不是「契约要求非 NULL」。故修复选在解引用点补判空，而**不是**给每个 NULL 调用者塞一个用完即弃的局部 `Config`：
后者只遮症状，且挡不住未来任何新的 NULL 调用点（`WeaselTSF.cpp:227/:505`、`Compartment.cpp:271` 本来就传 NULL）。

**为什么 spec 094 之前没发现**：`065383e` 把删除改成「同步 IPC 调用」之前，这条通路不走
`ResponseParser(NULL,…)`；重构之后，`RimeWithWeasel` 侧那条无条件 `action=config` 又刚好命中唯一漏判的 deserializer。

### 14.2 已落地修复（未 commit，在工作树）

`WeaselIPC/Configurator.cpp` —— 中文事故注释在 `:17-25`，守卫在 `:26-27`：
```cpp
if (!m_pTarget->p_context || !m_pTarget->p_config || key.size() < 2)
  return;
```
注释里**不写会漂移的硬编码行号**（只描述本函数内部逻辑）。
文件归一化为 **LF**（`size=1477 CR=0 LF=32`，index 口径 `i/lf`）。
`git diff --numstat` 当前：`WeaselIPC/Configurator.cpp` **10/1**、
`test/TestResponseParser/TestResponseParser.cpp` **66/0**、`.specify/memory/lessons-learned.md` **49/0**；
`librime` 子模块脏（` m`）**不得入库**。

### 14.3 本轮踩到的两个「会给出假结论」的陷阱（务必先读）

**陷阱 1 —— 假探针（载荷写法不合法，测试根本没执行到被测代码）**
`ResponseParser` 的 deserializer 图**默认只注册 `action`**（`Deserializer.cpp:27`），
`ResponseParser.cpp:59-64` 按 `key[0]` 查图，**查不到就整行丢弃**；只有 `ActionLoader::Store`
按报头 `action=` 名单逐个 `Require` 才会把 `config`/`status` 挂进图。
第一版新用例载荷写成 `action=ctx`，⇒ `config.*` / `status.*` 两行**从未派发** ⇒
「没崩」是平凡真，**永远咬不到 bug**。已把载荷改成 `action=config,status` / `action=config` 并补注释。
⇒ **判据：新用例「不崩」本身不算证据，必须做「去掉修复必须崩」的反向对照。**

**陷阱 2 —— 构建时间戳造成的假失败**
`.bak` 用 `copy` 还原源文件时带着**旧的 LastWriteTime**，MSBuild `/t:Build` 判 up-to-date、
**没有重编译 `Configurator.cpp`**，于是拿 Phase B 的陈旧 `.obj` 重链 ⇒ 还原了修复却照样崩（exit `-1073741819`）。
判据 = **构建日志里有没有目标源文件的编译行**（`-> xxx.lib` / `-> xxx.exe` 这类链接行在
「什么都没重编译」时也会打印，不能当证据）。
⇒ 凡「还原文件后重建」的对照组一律用 `/t:Rebuild`（或显式刷新 LastWriteTime）。

**附带：编辑工具把 LF 文件写成 CRLF**（本仓库 index 行尾不统一：`Configurator.cpp` 是 `i/lf`、
`TestResponseParser.cpp` 是 `i/crlf`、`lessons-learned.md` 是 `i/crlf` + BOM、handoff 是 `i/lf`），
造成 22 行「整文件重写」假 diff。已按字节还原为 LF；`weasel.props` / `env.bat` 未被改动。
**改任何文本文件前先 `git ls-files --eol -- <path>` 看该文件的 index 口径。**

### 14.4 已验证：三阶段「有牙」证明（`.superpowers/sdd/evidence/verify_configurator_guard.log`）

| 阶段 | 树状态 | 构建 | `TestResponseParser.exe` 退出码 | 结论 |
|---|---|---|---|---|
| A | 修复态 | `Configurator.cpp` 重编译，`No errors detected.` | **0** | 用例通过 |
| B | `git checkout HEAD -- Configurator.cpp`（去掉守卫） | `Configurator.cpp` 重编译 | **-1073741819 = 0xC0000005** | **用例确实咬到被测代码** |
| C | `copy` 还原（未重编译，假失败） | 日志无编译行 | -1073741819 | 陷阱 2，作废 |
| D | `/t:Rebuild` 强制重编译 | 有 `Configurator.cpp` 编译行，`No errors detected.` | **0** `TEST_EXIT=0` | **最终树状态是绿的** |

新增用例 `test_5` / `test_6`（`test/TestResponseParser/TestResponseParser.cpp:115-177`，`_tmain` 在 `:184-185` 调用）
是**成对的**：
- `test_5` 复刻 `CandidateList.cpp:405` 的真实形参组合（`p_config == NULL`）⇒ 既验「不崩」，
  也验**同一响应的后续 `status.*` 行仍被解析**（守卫不能把整条响应哑掉）。
- `test_6` 传**真实** `Config` ⇒ 守卫没把正常 config 通道关死。**专门防「过度修复」这类静默回归。**

### 14.5 已验证：全量单测（`scripts/run-tests.bat` → `.superpowers/sdd/evidence/runtests_final.log`，850 行）

**构建零错误**（`error C` / `error LNK` / `error MSB` 命中数 **0**），改过的 `WeaselIPC` 被整套一起重建。
关键套件全绿：
```
TestResponseParser        No errors detected.   exit 0     ← 含新增 test_5/test_6
TestUserDictUpdate        4/4
TestCandidateRButtonDown  T1.. 全 PASS
TestShiftIPC            PASS=49 FAIL=0
TestShiftIPCWire          PASS=10 FAIL=0
TestPipeChannelRace       12 passed, 0 failed
TestShortcutSettings      PASS=52 FAIL=0
MAPPER-SUMMARY failures=0 / RECORDER-SUMMARY failures=0
=== ALL TESTS PASSED === x2  (TestWeaselTSF / TestRimeWithWeasel)
```
唯一 FAIL 是**环境前置条件**（`runtests_final.log:845`）：
`[ERROR] A WeaselServer.exe or TestWeaselIPC.exe process is already running.` ⇒
本机输入法常驻，`TestWeaselIPC` 不能与生产 server 并存。**与本次改动无关，不是代码失败。**
`LNK4020` 经查是既有噪音：官方套件同样出现，来自中间目录 `rime_claudemsbuild` 的 PDB 类型记录
（`scripts/test-infra/run-test-suite.bat:27-29` 的 `SOL_DIR` 不带尾反斜杠），非本次引入。

### 14.6 状态分层

- **已验证**：崩溃点符号 / 无条件 `action=config` 使该崩溃**确定**而非竞态 / 四个同类 deserializer 都判空 /
  修复后 `TestResponseParser` 退出码 0 且去掉守卫后 0xC0000005 / 全量单测构建与套件（除环境前置）全绿。
- **推断**：装机后飞书 / MiniMax Code 的右键崩溃会消失（同一段代码、同一条通路、缺陷已消除）。
- **未验证**：**真机复验未做**（需 `build.bat` Release x64 出包 + 重启宿主，重放崩溃会再杀 MiniMax Code ——
  它就是本宿主）。⇒ 对外**不得宣称「缺陷已修」**，只能说「已定位并修复，单测已验证，待装机复验」。
- **仍未定性**：`Ctrl+Shift+<数字>`「候选窗消失」的两条互斥机制：
  - **A（门没开）**：`KeyEventSink.cpp:93-94` 的拦截门要 `ShouldInterceptCandidateDelete(number, _cand->GetVisibleCandidateCount())`，
    而 `WeaselTSF/CandidateList.h:73-77` 的 `GetVisibleCandidateCount()` 在 `!_ui || !_ui->IsShown()` 时返回 0
    ⇒ 门关 ⇒ 按键落到 `:101 m_client.ProcessKeyEvent(ke)` ⇒ Electron 把 `Ctrl+Shift+<n>` 当切标签页加速器吃掉。
  - **B（门开了但崩了）**：门过 ⇒ 进 `_DeleteCandidateOnCurrentPage` ⇒ 同一空指针崩溃 ⇒
    Electron 把「渲染进程死 + 标签重载」表现成「候选窗消失」。
  机制 A 成立时「右键崩 + 快捷键吞窗」两条症状同时自洽；机制 B 也自洽。**只能由运行态区分**。
  ⇒ **本棒的 `p_config` 修复装机后，若该现象一并消失 ⇒ B；若仍在 ⇒ A（且需再查宿主加速器）。**
  这条判别实验**无需再改代码**。
  > **§十五 更新（2026-11-06 探针轮）**：机制 B **基本排除** —— 用户答「什么都没发生、只是窗没了」，
  > 而该崩溃**每次必崩**（§14.2）；且 A 里的子假设「`ConvertKeyEvent` 返回 false 使主键盘 chord 不可达」
  > **已被实测证伪**（拦截门静态可达，见 15.2 #1/#2）。A 收窄为「运行时 `GetVisibleCandidateCount()` 是否为 0」，
  > 并新增一条与之并列的出口 ②「Windows `Layout Hotkey=2` 系统抢键」（15.4/15.5）。
- **不得再用作区分性证据**：「hover 无高亮」= 生效配置 `hover_type: none` + `WeaselPanel::OnMouseMove` 首行
  `return 0` 的**预期行为**（§13.3）；用户答的第三问已证实它是配置所致。

### 14.7 下一棒请按此顺序

1. **取得用户点头**再动：① `build.bat` Release x64 + 装机复验（会重启/可能杀掉宿主，含 MiniMax Code 本身）
   ② commit（`WeaselIPC/Configurator.cpp` + `test/TestResponseParser/TestResponseParser.cpp` +
   本 handoff + `.specify/memory/lessons-learned.md` L187/L188；排除脏 `librime`、`.superpowers/`、
   `_ts_yaml_fixture.yaml`、`env.bat`、`weasel.props`）③ push（当前 `ahead 7` 未 push）。
   commit 前必须 `git status -sb` + `git diff --stat`，格式 Conventional Commits + 模块前缀（建议 `WeaselIPC: fix ...`）。
   > **§十五 更新**：本条 ①②③ **均已完成** —— 包已出（`output/archives/fluxing-1.2.0-installer.exe`，
   > 2026-11-06 11:39:57，42789863 B，含修复），代码与文档已提交（`fde34a1` / `b9d5f24`）并已 push；
   > 分支现与 `kizemo/Fluxing` 同步。**尚未做的只剩「装机复验」**。
2. 装机复验判据：在**飞书**与**自建 WinForms 原生 EDIT 目标**（`.superpowers/sdd/e2e_target_winforms.ps1`）
   里各做「右键删词」+「`Ctrl+Shift+<数字>`」；
   **先确认进程里实际映射的是新 `weaselx64.dll`**（`install.nsi` 的「DLL 被 TSF 锁住 ⇒ 改装版本化 shim」机制
   可能让宿主继续用旧 binary，光看文件时间戳不算证据；装机目录残留十余个 `weaselx64.*.dll`）。
   重放崩溃会杀掉所测宿主，**别拿 MiniMax Code 裸做实验**（本棒已崩过两次，属既有行为）。
3. 若装机后 `Ctrl+Shift+<数字>` **仍**不删词**且不崩** ⇒ **先跑 15.5 的 30 秒人工判据**
   （只按 `Ctrl+Shift` 看不切不切输入法），再决定是否走机制 A 的注入探针：
   - 指示器**变** ⇒ 出口 ② 成立，与代码无关，修法属产品层 chord 决策（需 ADR），**别去插桩**。
   - 指示器**不变** ⇒ 才走机制 A：自建**非 Electron 原生 EDIT 宿主**补**对照组**
     （那里能删 ⇒ Electron 宿主加速器成立；那里也不删 ⇒ 才需要 `GetVisibleCandidateCount()` 运行态插桩）。
   - 另外：**若用户按的是小键盘，现象已被 15.2 #3 完整解释，不必做任何运行态实验。**
4. §十二 的 PASS 仍**严格限域**：WinForms 原生 EDIT + 右键 + 面板左起 26px 落点 + `hover_type: none`，
   **不推翻但不得外推**。
5. 顺手项（低优先，本棒未做）：`TestWeaselIPC` 与常驻 `WeaselServer.exe` 的前置冲突，可让脚本自己拉起/关闭
   测试 server，否则本机永远跑不出全绿。
6. 残留待用户手工删除：`D:/Program Files/fluxing/user1/fluxing/double_pinyin_sogou.user_ignore.txt`
   （27 B，含常用词「你」；副本 `.superpowers/sdd/evidence/user_ignore_final.txt`）。agent 删不掉受保护路径。

### 14.8 本棒新增/更新的资产（`.superpowers/` 不入库）

| 文件 | 说明 |
|---|---|
| `sdd/verify_configurator_guard.bat` | A/B/C 三阶段有牙验证驱动（**C 用 copy 还原 ⇒ 已判定不可信**） |
| `sdd/rebuild_phaseC.bat` | 修正版：`WeaselIPC.vcxproj /t:Rebuild` + 重跑测试（**Phase D 用它**） |
| `sdd/Configurator_fixed.bak` | 修复版源文件备份（与工作树等价，仅编码/行尾不同） |
| `sdd/symprobe.py` | DbHlp 符号化探针（已调通；纯 ASCII + `PYTHONIOENCODING=utf-8`） |
| `sdd/evidence/verify_configurator_guard.log` | 112 行，含 A/B/C 三段退出码 |
| `sdd/evidence/phase_D_final.log` | 33 行，最终态绿 |
| `sdd/evidence/runtests_final.log` | 850 行全量单测（唯一 FAIL = 环境前置） |
| `sdd/evidence/runtests_after_fix.log` | 956 行，**假载荷那一版**的记录（保留作反面教材） |

---

## 十五、`Ctrl+Shift+<数字>` 静态/离线探针轮（2026-11-06）

> 本轮性质：**纯取证，零代码改动、零提交**。分支已与 `kizemo/Fluxing` 同步
> （`git status -sb` 只剩禁入项 ` m librime` / `?? .superpowers/` / `?? _ts_yaml_fixture.yaml`）。
> 产出 = 把 §13.2 / §14.7 的「未排除出口」收窄到 2 个，并**证伪两条**曾挂在账上的假设。

### 15.1 探针 `sdd/tochar_probe2.ps1`（已自检，输出可用）

复刻口径 = 生产代码链路 `ConvertKeyEvent` → `ResolveCharacter` → `ToUnicodeEx`
（`include/KeyNameMapper.h:586-606`、`WeaselTSF/KeyEvent.cpp:54`），关键点：

- `UINT(kinfo)` 是**整条 `WM_KEYDOWN` 的 lParam**（位域视图见 `include/KeyEvent.h:3-15`），
  scan code 在 bit16-23、repeat 在 bit0-15 —— **不是裸 scan code**（本轮前我曾凭记忆写错）。
- 调用前复制 keyState 并**清掉 `VK_CONTROL` / `VK_MENU`**，`wReserved` 传 0，`hkl` 依次试
  `NULL` / 当前线程布局 / 英文布局。
- **自检先行**：`Shift+A → ret=1 code=0x41` 通过才允许采信全表。

⚠ 弃用 `sdd/tmp/tochar_probe.ps1`（v1）：`Add-Type` 的 C# 里裸写 `StringBuilder` ⇒ CS0246 编译失败
⇒ 整表 `code=0x00`、`HKL=0` 全是**工具自身报错的产物**。教训：**别把工具自身报错的产物当测量值**。

### 15.2 实测结论（日志 `sdd/evidence/tochar_probe2.log`）

| # | 事实 | 证据 | 定级 |
|---|---|---|---|
| 1 | 主键盘 `Ctrl+Shift+1..9`（VK `0x31..0x39`，scan `0x02..0x0A`）在三种 `hkl` 口径下**全部** `ret=1`、`code = ! @ # $ % ^ & * (` | 探针全表 | **已验证** |
| 2 | ⇒ `ConvertKeyEvent` 返回 true ⇒ `KeyEventSink.cpp:90-94` **拦截门可达**（主键盘行） | 由 #1 + `KeyEvent.cpp:60-61` | **已验证**（仅「可达」） |
| 3 | 小键盘 `Numpad1/2/3`（VK `0x61..0x63`）`ToUnicodeEx ret=0` ⇒ `ConvertKeyEvent` FALSE ⇒ 门**永不达**（`KeyEvent.cpp:60-61` 是唯一 false 出口）；且 `IsCandidateDeleteDigitVk` 只认 `0x31..0x39` ⇒ **两重确定性死路** | 探针 + `include/FluxingCandidateDeleteShortcut.h` | **已验证（源码+实测）** |
| 4 | `Ctrl+Shift+grave`（VK `0xC0`，scan `0x29`）`ret=1 code=0x7E '~'` —— 与 spec 097 / L163「该 chord 已移出删除候选词家族」自洽 | 探针 | **已验证** |

**边界（务必别越）**：#2 只证明「门在静态上可达」，**不**证明运行时真删词。

**两条作废假设**（不要再挂账）：
- ~~「Shift+数字被解析成 `!@#` 所以永不命中」~~ —— 判定只看 **VK**（`MatchCandidateDeleteShortcut(int vkey, bool ctrl_down, bool shift_down, bool key_down)`，4 参形式，见 `test/TestCandidateRButtonDown/TestCandidateRButtonDown.cpp:236-284` 的 Test 7/8/9），keycode 是什么与命中无关。
- ~~「`ConvertKeyEvent` 返回 false 使主键盘 chord 不可达」~~ —— 由 #1 直接证伪。

### 15.3 「功能被某个开关默认关闭」—— 证伪

全仓 `delete_candidate` 检索（`*.{h,cpp,yaml,custom.yaml,md}`）没有任何 bool 开关/option 读取点；命中的只有 IPC 命令（`include/WeaselIPC.h:35`）、librime API（`include/rime_api.h:483/485`）、各 schema 的 `Control+Delete: delete_candidate`、以及文档/ADR/spec 叙述。⇒ 客户端拦截门**不受任何开关控制**。

### 15.4 本机 Windows 抢键配置（候选解释，尚未证实）

实测注册表与语言列表：

```
HKCU\Keyboard Layout\Preload : 1=00000804  2=00000409
HKCU\Keyboard Layout\Toggle  : Language Hotkey=1  Layout Hotkey=2
Get-WinUserLanguageList      : zh-Hans-CN, en-US
```

`Toggle` 两值含义按注册表口径（1 = Left Alt+Shift，2 = Ctrl+Shift，3 = 未分配）⇒
**本机 Ctrl+Shift 被 Windows 系统级绑成「切换同一语言的键盘布局」**。这用户户描述「按下去候选窗消失、什么也没删」高度自洽，但仍是**推断**：注册表值只证明热键**处于开启**，不证明它在本机真的会切走（Windows 通常只在当前语言下有多于一个布局时才切）。

### 15.5 下一棒第一步（零风险、用户 30 秒，优先于任何注入探针）

在记事本里用 Fluxing 打出 `ni` 让候选窗出现，然后**只按 `Ctrl+Shift`**（不按数字），看任务栏输入法指示器 / 语言栏是否变化：

- **变** ⇒ 出口 ②（系统抢键）成立。修法方向是产品层规避（例如改 chord，或按 `default.yaml:37` 那条 `Control+Shift+grave` 的既有先例另择不冲突的组合），属需 ADR 的架构决策，**不在本轮范围**。
- **不变** ⇒ 该假设不成立，再走 15.6 的注入探针。

### 15.6 收窄后的两个出口

| 出口 | 内容 | 状态 | 怎么判 |
|---|---|---|---|
| ① 运行时 | `ShouldInterceptCandidateDelete(number, _cand->GetVisibleCandidateCount())` 因 `GetVisibleCandidateCount()==0` 不拦（`CandidateList.h:73-77` 需 `_ui->IsShown()` 为真） | **未证实/未证伪** | 注入探针 + 运行态插桩 |
| ② 环境 | Windows `Layout Hotkey=2` 在 chord 到达 TSF sink 前抢走 | **有配置证据，无行为证据** | 15.5，用户 30 秒 |

另：**若用户按的是小键盘 ⇒ 现象已被 #3 完整解释**，无需任何运行态实验。

### 15.7 本轮新增/更新的资产（`.superpowers/` 不入库）

| 文件 | 说明 |
|---|---|
| `sdd/tochar_probe2.ps1` | 7586 B，自检版 ToUnicodeEx 探针，**本轮唯一可信探针** |
| `sdd/tmp/tochar_probe.ps1` | v1，**坏**（CS0246 静默失败），保留作反面教材 |
| `sdd/evidence/tochar_probe2.log` | 三布局全表 + Preload/Toggle/LangList 实测 |

**本轮工具坑（追加）**：① `Add-Type` 的 C# MemberDefinition 里裸 `StringBuilder` ⇒ CS0246（须全限定或 `-UsingNamespace`）；② 探针里那行「gate REACHED」的判据写成 `code` 落在 `0x31..0x39`，与真实判定（看 VK）不符 ⇒ 该诊断行**永不亮**，只当 `ret`/`code` 原始值看；③ `Get-WinUserLanguageList` 的 `TipInstances` 段打印为空 ⇒ 「zh-Hans-CN 下到底挂了几个 TIP」这个事实**仍未量到**，而下棒判 15.5 时正好需要它。

### 15.8 装机复验的第一号前置（本轮实测，硬事实）

**当前机器上跑的还是 11/5 的旧 DLL，崩溃修复根本没生效。** 取证（SHA256 前 16 位）：

| 文件 | 哈希前 16 | 大小 | 时间 |
|---|---|---|---|
| `D:\Program Files\fluxing\weasel\weaselx64.dll`（装机） | `1E9821AA26528BB0` | 1,989,632 B | 2026/11/5 14:44:40 |
| `output\weaselx64.dll`（含修复的新产物） | `AD6A7A7922E67362` | 1,989,632 B | 2026/11/6 11:36:11 |
| `D:\Program Files\fluxing\weasel\weasel.dll`（装机） | `3F8F21844B66FE73` | 1,678,336 B | 2026/11/5 14:44:56 |
| `output\weasel.dll`（新产物） | `15BF689ECC3770F8` | 1,678,336 B | 2026/11/6 11:36:25 |

**大小完全相同、哈希不同 ⇒ 不能靠大小或文件名判断新旧，必须比哈希。** 修复提交 `fde34a1` 于 11:32:50，新 DLL 构建于 11:36:11 ⇒ 新产物确实含修复；新包 `output\archives\fluxing-1.2.0-installer.exe`（11:39:57）由它打出。

**因此**：用户 11/6 07:49 / 09:12 的两次崩溃都由**旧 DLL** 造成，与「修了但没用」无关 —— 右键那条线**至今还没有被真机检验过一次**。装机目录里还留着十余个历史 shim（`weaselx64.0.21.0.5.dll` 一直到 `weaselx64.1.1.9.0.2.dll`），装完后要按 §14.7 的教训核对**进程里实际映射的文件路径与哈希**，别只看 `weaselx64.dll` 的时间戳。

**本轮附带事实**：`HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Sharing Engines\Server`（含 `WOW6432Node` 同路径）在本机**不存在** ⇒ 「从注册表读 TSF server 注册路径」这条抓手作废；要拿装机路径，直接读运行中 `WeaselServer.exe` 进程的 `.Path`（本机为 `D:\Program Files\fluxing\weasel\WeaselServer.exe`，PID 46560，启动于 11/6 7:28:37）。另：我第一轮凭记忆写死 Sharing Engines 的 CLSID 子键去 `Get-ItemProperty`，直接报「路径不存在」—— 这类 GUID 一律先枚举再取，不要凭记忆写。

**修正 §15.6 / §14.7 的复验顺序第 1 步**：原文「先确认进程里实际映射的是新 `weaselx64.dll`」应理解为 **「先装新包 `output\archives\fluxing-1.2.0-installer.exe`，再核对映射的 DLL 哈希」**；在装新包之前做的任何真机复验（右键、`Ctrl+Shift+<数字>`）都只是在重复测旧 binary，结论无效。

## 十六、本棒（2026-11-06 下午 · `Ctrl+Shift+<数字>` 顺序敏感**定性为真实缺陷** + 右键修复装机真机验收 **PASS**）

> 本棒用户指令：接手本线；用户 07:54 反馈「`ctrl+shift+数字` 与右键仍无法删除候选词」，并回答三问 ——
> ① 测于**飞书 / MiniMax Code**，「有候选窗时按 shift+ctrl+数字，候选窗会消失」；
> ② 「右键点击候选窗会导致宿主软件一起被杀死」；③ 「hover 时候选没有高亮」。
> 前三棒（§十三 / §十四 / §十五）分别记录了：结论收窄、崩溃根因与修复、静态/离线探针。**本棒以 §十五 为起点，只读 §15.6 的复验顺序**（勿重跑 §十五 已作废的判据）。

### 16.1 环境前提（已实测核对，勿重复取证）

| 项 | 实测值 | 判据来源 |
|---|---|---|
| 装机 image | `D:\Program Files\fluxing\weasel\weaselx64.dll` = `1989632 B / mtime 2026-11-06 11:36:10 / sha256_16 AD6A7A7922E67362` ⇒ **含 `fde34a1` 空值守卫的新产物** | `chord_rba_result.json.installed_dll` |
| 安装时刻 | 约 `2026-11-06 11:55:12`（`fluxing-1.2.0-installer.exe`，42789863 B @11:39:57） | §十五 表 + 注册表/文件核对 |
| 靶进程 | powershell WinForms `pid 88156`，`proc_start 2026-11-06 14:33:49` ⇒ **晚于安装**，映射必为新 image | `chord_rba_result.json.target` |
| WeaselServer | `pid 74356`（安装后新起） | `chord_rba.log` |
| 锁屏 | `LogonUI` 未运行 ⇒ 注入与截屏可用 | `session_health()` / `chord_rba.log:1` |

> **口径限定**：本棒全部结论**仅对「安装 1.2.0 之后新启动的宿主进程」成立**。安装前启动的 explorer(7:12) / DingTalk / Weixin / TextInputHost / `Claude Code Haha`(9:09) / **`MiniMax Code`(9:13)** 仍持旧 image；其中 `MiniMax Code` 就是当前宿主 ⇒ **不可**在本机重放右键崩溃做正对照。

### 16.2 判别矩阵 exp3：顺序敏感是真实缺陷，两个伪因当场证伪

资产 `.superpowers/sdd/chord_exp3.py`（结果 `chord_exp3_result.json` 22324 B @14:52:48；日志 `chord_exp3.log` + `evidence/exp3_stdout.log`；运行标记 `evidence/exp3_run.txt` = `RUN_START 14:52:12.83 / EXIT=0 / RUN_END 14:52:48.21`，35 s 属正常 —— 5 变体 + 被跳过的 H 门很快）。

| 变体 | 顺序 | 每事件间隔 | 删词（ignore 文件变化） | 数字键时刻面板 |
|---|---|---|---|---|
| R1 | Ctrl 先 → Shift 后 → `1` | 0.12 s | **无** | `[]`（已消失） |
| R2 | **Shift 先 → Ctrl 后** → `1` | 0.12 s | **成功**（新增 `嘚嘚`；29 B `5A23503366C8FA31`→37 B `401288373BCE9DFC`） | 非空 |
| R3 | Ctrl 先 → Shift 后 → `1` | **0.60 s**（人类慢时序） | **无** | `[]` |
| R4 | Shift 先 → Ctrl 后 → `1` | 0.60 s | **成功**（新增 `得得得得`；→51 B `96526A0E8C8A0744`） | 非空 |
| R5 | Ctrl 先 → **右 Shift** → `1` | 0.12 s | **无** | `[]` |

- **H2「注入时序伪影」被证伪**：R3 把每个事件放慢到 0.60 s（比人手更慢）仍然失败 ⇒ 撤销 §十五「先排除注入时序再决定是否报缺陷」的保留，**直接定性为「Ctrl-first 手势下删词功能失效」**。
- **H3「左/右 Shift 分叉」不支持**：R5 右 Shift 与 R1 左 Shift 同败。
- **决定性观测**：五个变体在数字键时刻的异步键状态**全部**显示修饰键真按住（`shift_generic=true / ctrl_generic=true`，R5 为 `rshift=true`），但**失败变体的面板 `panel_before_digit=[]`** —— 面板在「按住第二个修饰键那一瞬」就消失；成功变体在 `after_hold_shift.panel` / `after_hold_ctrl.panel` 全程可见。**用户自述的「候选窗会消失」与失败变体逐字吻合 ⇒ 用户观察不是错觉，是本缺陷的第一手现象。**
- 其余恒定值：`hkl` 变体前后 `0x08040804`、`foreground=854074`（靶窗口）、`target_alive=True`、`wer_new=[]`、`text_after=''`（**没有上屏**）、WER 基线 102 → 末 102。

### 16.3 代码层根因：Ctrl-first 路径上有**两个独立阻断点**（勿写成单一根因）

读源码切片（`WeaselTSF/KeyEventSink.cpp` 全 174+ 行，实测读到）：

- `:26` `GetKeyboardState(_lpbKeyState)`；`:27-31` `ctrl_down / shift_down = fluxing::IsKeyStateDown(...)` —— **读的是宿主线程的键盘状态表**。
- `:50-78` Shift 分支：`:58-61` `if (ctrl_down) { _shiftSuppressed = TRUE; *pfEaten = TRUE; return; }` —— **Ctrl 已按下时 TSF 自己吞掉 Shift down**（注释 spec 094 明写目的：不让服务端 Shift 状态机把它当成「选词 Shift」而 `commit_composition`，见 spec 077 / ADR 0023）；`:63-65` 仅在 **Ctrl 未按下**时才 `m_client.ShiftDown()`；`:67-71` 吞配对的 up。
- `:90-98` 删词门：`if (!keyCountToSimulate && fluxing::IsKeyDownLParam(lParam))` → `MatchCandidateDeleteShortcut(wParam, ctrl_down, shift_down, true)` → `ShouldInterceptCandidateDelete(number, _cand->GetVisibleCandidateCount())` → `_DeleteCandidateOnCurrentPage(number-1)`。
- `include/FluxingCandidateDeleteShortcut.h:31-71`（header-only，73 行，UTF-8+BOM）：`MatchCandidateDeleteShortcut` 在 `!ctrl_down || !shift_down` 时**直接返回 0**；`ShouldInterceptCandidateDelete` 在 `visible_count==0` 时返回 false。
- `WeaselTSF/CandidateList.h:73-77`：`GetVisibleCandidateCount()` **第一句就是 `if (!_ui || !_ui->IsShown()) return 0;`** ⇒ 面板不可见 ⇒ 门必然不命中。

⇒ **(a)** Shift keydown 被 TSF 吞掉 ⇒ 宿主线程 `GetKeyboardState` 可能从未记录 `VK_SHIFT` ⇒ `MatchCandidateDeleteShortcut` 返回 0（代码推出，**进程外无法读取宿主线程键盘状态表，尚未坐实**）。
⇒ **(b)** 实测面板在按住第二个修饰键那一瞬已消失 ⇒ `GetVisibleCandidateCount()==0` ⇒ 门不命中（**直接观测到的事实**）。
**(b) 单独就足以解释全部失败**；因此真正的上游问题是「**谁在 Ctrl-first 时隐藏了面板**」，(a) 在 (b) 成立时是 moot 的 —— 下一棒必须先答 (b)。
已排除：服务端 ShiftUp 状态机不是隐藏者（`RimeWithWeasel/RimeWithWeasel.cpp:2205-2258` 复核，`ShiftUp` 只在 `kSelectCandidate` / `kToggleAsciiMode` 动作下做事，`:2257 s = ShiftState{}` 无条件复位；而 Ctrl-first 下客户端根本没把 Shift 转发到服务端）。
未排除：系统「Layout Hotkey = 2」（Ctrl+Shift 切换输入语言）触发切换器导致 TSF 结束 composition —— 本机 `Preload = 1:00000804 / 2:00000409` 确实启用了该热键；本棒 `hkl` **只在变体前后采样**，中间态采到的 `hkl_probe_thread=134481924` 是**探针线程自己的** hkl，对靶线程无意义 ⇒ 瞬态布局切换**未被证伪**（但「R2/R4 同 chord 却成功」这条不对称削弱它）。

### 16.4 作废 §十五 的 H 变体「同 hwnd 因果门」

`chord_probe.py:510-515` 要求「Escape 使面板消失后、重打必须**同一个 hwnd** 再现」才允许下右键。14:52 实测 `gone=True rearm=[214,248,448,297]` 而待点是 `0xEC0D1C@[214,248,541,297]` ⇒ 判 `skipped_panel_not_causal`，H **从未执行**。
**原因（新认知）**：候选面板是**宿主进程内**由 WeaselTSF 自建的 ATL 窗（类名 `ATL:00007FFB…`，归属靶 pid），每次 composition 结束即 `Dispose`、重开重建 ⇒ **hwnd 必变** ⇒ 「同 hwnd」门永远过不了。
**新口径**（已写进 `chord_rba.py` 并跑通）：`Escape 使面板消失` + `重新打字后同类名面板再现` ⇒ 面板因果成立，可下右键。

### 16.5 右键线：1.2.0 修复的**装机真机验收 PASS**（本线第一次真机通过）

资产 `.superpowers/sdd/chord_rba.py`（7751 B）+ `run_rba.cmd`（606 B，纯 ASCII，先 `del` 再写，自带 `RUN_START/EXIT=/RUN_END` 标记）。启动方式（实测可用）：

```powershell
cmd /c "F:\soft\00selfmade\rime_claude\.superpowers\sdd\run_rba.cmd"   # run_in_background=true
```

结果（`chord_rba_result.json` 2865 B / `chord_rba.log` 974 B / `evidence/rba_run.txt` = `RUN_START 14:59:26.78 / RUN_END 14:59:38.50`）：

- `verdict = "PASS 右键删词成功且宿主存活、无新 WER ⇒ 1.2.0 修复装机真机验收通过"`
- 面板 `0x90848 rect=[214,248,448,297]`，输入 `de`；因果门 `gone=True rearm=[214,248,448,297]` 同类名再现。
- 落点 `(240,272)`（候选文字中心，非 §十二 的「左起 26px」粗口径）。
- **主判据三取齐**：`ignore_changed` 非空（`double_pinyin_sogou.user_ignore.txt` 51 B `96526A0E8C8A0744` → 56 B `4B2C4EFC1930D689`，新增 `得`）**且** `target_alive=true` **且** `wer_new=[]`。
- `text_after=''`（未误上屏）。

⇒ §十三 里「右键会导致宿主被杀」的现象，在 **1.2.0 + 安装后新起的宿主**上已不复现；根因（`WeaselIPC/Configurator.cpp` `Store` 空指针解引用）与修复（一行空值守卫）+ 单测三阶段有牙验证（§十四）+ 真机 PASS 形成完整闭环。

### 16.6 用户反馈③「hover 不高亮」：**复现**（像素级证据）

同一次 exp4 里在右键前采集三帧全屏截图（`.superpowers/sdd/rba_hover_on_candidate.png` / `rba_cursor_away.png` / `rba_hover_again.png`，2517147 B）：

```
rba_hover_on_candidate.png 784b4bc9538c1ea0
rba_cursor_away.png        784b4bc9538c1ea0
rba_hover_again.png        784b4bc9538c1ea0
rba_final.png              dcd3d737ecba7241   # 删词后面板消失 ⇒ 不同
```

三帧**同哈希 ⇒ 像素完全一致**：光标从候选文字中心移到 (40,40) 再指回，面板没有任何重绘。`CopyFromScreen` 不含光标图像本身，所以差异只可能来自候选高亮 —— 没有差异。
**口径**：这证明「合成光标移动（`SetCursorPos`）下无高亮」；物理鼠标是否走另一条消息路径**未验证**，但该现象与用户描述一致，可判为缺陷复现，交由 spec 层排查 `WeaselUI/WeaselPanel.cpp` 的 mouse-move → highlight 链路。

### 16.7 残留用户数据污染（**必须请用户手工清理**，agent 删不动受保护路径）

`D:/Program Files/fluxing/user1/fluxing/double_pinyin_sogou.user_ignore.txt`（当前 56 B，`4B2C4EFC1930D689`，mtime 14:59:35）内容 = 探针删词记录：
`策士 / 中过 / 中果 / 的 / 嘚嘚 / 得得得得 / 得`
**其中 `的` 与 `得` 是超常用字** —— 不删直接影响日常打字（输入 `de` 不再给出「的/得」候选）。清理命令（管理员 PowerShell）：

```powershell
Copy-Item 'D:\Program Files\fluxing\user1\fluxing\double_pinyin_sogou.user_ignore.txt' "$env:USERPROFILE\Desktop\user_ignore.backup.txt" -Force
Remove-Item 'D:\Program Files\fluxing\user1\fluxing\double_pinyin_sogou.user_ignore.txt' -Force
```

删完必须**重新部署**（托盘图标右键 → 重新部署，或退出 WeaselServer 再启动），因为 librime 把 ignore 列表读进了内存，只删文件不重新部署不生效。本棒**没有**代用户写这个文件（避免在服务持有内存态时造成写回冲突）。

### 16.8 下一棒顺序（按序，勿并行）

1. **答 16.3 的 (b)「谁隐藏了面板」**：`Release` 下 `DLOG(INFO)` 可能被 `NDEBUG` 整段编不出（**尚未坐实**）⇒ 改用 `OutputDebugStringA`，配一个 ~40 行的 `DBWIN_BUFFER` / `DBWIN_BUFFER_READY` 共享内存 Python 监听器（无需管理员）。仪表化三处：`KeyEventSink.cpp:58` 进入吞 Shift 分支时、`:90-98` 门口打印 `ctrl_down/shift_down/_shiftSuppressed/number/visible_count`、`CandidateList::Hide()` 的调用栈来源。
2. 若 (b) 答案是「系统 Ctrl+Shift 布局热键」⇒ 修复方向变成**换 chord**（例如 `Ctrl+Alt+<数字>`）或在 `OnTestKeyDown` 提前识别 chord 并阻止框架走语言切换；若答案是「TSF 自身吞 Shift 的副作用」⇒ 才轮到改判定入参。
3. **候选一行修复（本棒未提交，机制未坐实）**：`KeyEventSink.cpp:92` 传 `shift_down || _shiftSuppressed`。它**不会**重新引入 spec 094 的上屏问题（只改客户端拦截判定，不改变转发给服务端的内容），且在 (b) 成立时**单独无效**。坐实 (a) 之后再提交，走完整 fix 链（构建 + `TestResponseParser` + CHANGELOG + Conventional Commit `fix(WeaselTSF): …`）。
4. **请用户做物理按键复测**（注入阴性不可外推）：前提**先完全退出并重开飞书 / MiniMax Code**（两者 11:55 安装前启动，仍持旧 image）。步骤 ① `Ctrl` 先按住 → 再 `Shift` → 再主键盘 `1/2/3`；② `Shift` 先按住 → 再 `Ctrl` → 再 `1/2/3`；记录候选窗是否在按下第二个修饰键时消失、是否切换了输入法语言、是否上屏、用的左还是右 Shift。
5. spec 099 快捷设置栏那条线**另棒继续，本棒未触碰**。

### 16.9 本棒工具坑（新增）

- 抽 `chord_exp3_result.json` 摘要时对 `variants` **末条 H 记录** `print(v['dwell'])` ⇒ `KeyError: 'dwell'`，脚本 `exit code 1`，但**数据其实已全打出来** —— H 记录字段集与其它变体不同。**判据**：取数前先按 `variant` 名分支或用 `.get()`；非零退出码不必然意味着测量失败，先看已打印内容。
- `edit` 工具参数名必须 `file_path`（写成 `path` 报 `root: must not have additional properties`）。
- 后台长时探针**唯一正确读法**：先看 `evidence/*_run.txt` 的 `RUN_START / EXIT= / RUN_END` 三行与时间差，再看日志；`task status=succeeded` 不代表真跑过（本棒 exp3 的 35 s 需要靠时间戳解释，不能靠「比预估短」推断）。
- 同一份「探针原语」要复用时**另立新脚本**（`chord_rba.py`）而不是在旧脚本上打补丁：`--only=H` 复用旧门必然再次跳过，且同文件多次写是冲突写。

**关联**：§十三（结论收窄）/ §十四（崩溃根因与修复）/ §十五（静态离线探针，其 15.6 复验顺序已被本棒 §16.4 修正）；lessons **L190**（吞修饰键 vs 依赖线程键盘状态的自相矛盾）、**L191**（进程内面板 hwnd 每次 composition 重建 ⇒ 同 hwnd 因果门永不成立）；ADR 0023 / spec 094 / spec 097 / L163。
