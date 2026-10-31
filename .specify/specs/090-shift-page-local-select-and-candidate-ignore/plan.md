# 090 · Plan — Shift 翻页后按页内次序选词，以及右键屏蔽候选词

> 配套 [spec.md](spec.md)（WHAT 层）与 [tasks.md](tasks.md)（执行层）。本文件是 HOW 层。

## 背景与取证

| 编号 | 取证结论 | 证据 |
| --- | --- | --- |
| E1 | Shift 选词用的是**全局绝对次序**，这就是翻页错位的直接根因 | `RimeWithWeasel/RimeWithWeasel.cpp:1879-1882`：`case ShiftUpAction::kSelectCandidate: { size_t idx = s.lastIsLeft ? 1 : 2; rime_api->select_candidate(to_session_id(ipc_id), idx); ... }`。对照 `:382-391` `SelectCandidateOnCurrentPage()` 走的是 `select_candidate_on_current_page` —— 数字键走页内语义、Shift 走全局语义，两者不一致 |
| E2 | `librime` 两个 API 的语义差已在头文件注释里写明 | `librime/include/rime_api.h:467-469`：`select_candidate` 注释 "Select a candidate from the current page."（**注释与实现不符**，实现是全局次序）；`:471-473` `select_candidate_on_current_page` 注释 "Select a candidate from the current page." |
| E3 | 选词点上已经拿不到分页信息 —— 上下文被提前释放 | `RimeWithWeasel.cpp:1867-1876`：`get_context(&shift_ctx)` 取到 `menu_open` 后**立刻** `rime_api->free_context(&shift_ctx)`，而 `kSelectCandidate` 分支在 `:1879` 之后 ⇒ `page_no` / `page_size` 在选词点已失效。任何「换算全局索引」的方案都 MUST 先把页信息缓存下来或把 `free_context` 后移 |
| E4 | 功能开关关闭时，Shift 会在**没有候选菜单**的情况下也走选词分支 | `include/ShiftStateMachine.h:152-163` `DecideShiftUpAction()`：`if (!feature_enabled) return ShiftUpAction::kSelectCandidate;` 排在 `menu_open` 判断**之前**。改动 MUST NOT 让这条路径去访问已释放的上下文 |
| E5 | 屏蔽名单的三段逻辑都已存在，但**没有任何调用者** | `WeaselUI/WeaselPanel.cpp:1391-1432` `LoadIgnoreList()`、`:1434-1442` `_FilterIgnoredCandidates()`、`:1444-1468` `_IgnoreCurrentCandidate()` 均有完整实现；全仓 grep 显示 `_IgnoreCurrentCandidate` 与 `_FilterIgnoredCandidates` **零调用点**。`git log -S` 证实：从无对应 spec、无按键绑定、无进程间命令、无处理入口 ⇒ 是「写了没接线」，**不是被删、也不是被开关关掉** |
| E6 | 右键按下时只有「删除」一条分支 | `WeaselUI/WeaselPanel.cpp:553-575` `OnRButtonDown()`：`:557` `hide_candidates \|\| m_hoverIndex<0` 早退；`:561-569` 100 ms 去抖（`static DWORD s_lastTick` / `static int s_lastIndex`）；`:570-572` **只调** `m_deleteCallback`。没有任何「删不掉就屏蔽」的分流 |
| E7 | 引擎侧删除 API 的返回值被丢弃，而它正是「是否自造词」的唯一判据 | `RimeWithWeasel.cpp:393-409` `DeleteCandidateOnCurrentPage()` 明确丢弃 `rime_api->delete_candidate_on_current_page` 的返回值，代码注释写着「The engine does the is_user_dict check internally; the C API does NOT expose is_user_dict」⇒ 界面侧无法自行判断是否自造词。**收尾修订**：该返回值已被证伪为不可用判据（`librime/src/rime/context.cc:146-154` 恒真、`rime_api_impl.h:982-1000` 的 `false` 分支全是参数/状态校验失败、C API 无 `is_user_dict`），改法转为「无条件写屏蔽名单 + 同时尝试引擎剔除」，详见 ADR 0023 D5 修订 |
| E8 | 候选信息下发只有一个汇聚点，且它拿得到会话标识与方案标识 | `RimeWithWeasel.cpp:599-622` `_GetCandidateInfo(weasel::CandidateInfo&, RimeContext&)`，调用点在 `:1066`（`_Respond`）与 `:1769` 一带（`_GetContext`）。它当前只设 `highlighted / currentPage(=page_no) / is_last_page`，**不设** `totalPages` |
| E9 | 界面层不做分页切片，`candies` 是什么就显示什么 | `WeaselUI/Layout.cpp:12-15` 构造函数直接引用 `cinfo.candies / comments / labels / highlighted` ⇒ 只要服务端把过滤后的列表放进 `candies`，界面自然显示过滤结果，无需改界面布局 |
| E10 | 结构体里有 `totalPages` 字段但无人填写 | `include/WeaselIPCData.h:181-229` `CandidateInfo` 含 `currentPage / is_last_page / totalPages / highlighted / candies / comments / labels`，**没有** `page_size`；`totalPages` 全仓无写入点 |
| E11 | 名单文件路径与编码规则已经定好，直接复用 | `WeaselPanel.cpp:1391-1432` `LoadIgnoreList()` 用 `WeaselUserDataPath()`，路径 `<userDir>\<schema_id>.user_ignore.txt`，支持 UTF-8 BOM / UTF-16LE BOM / 裸 UTF-8；`_IgnoreCurrentCandidate()`（`:1444-1468`）用 `FILE_APPEND_DATA` + `OPEN_ALWAYS` 追加，UTF-8 + `\r\n`，写前 `m_ignoreList.insert(text)`，末尾 `RedrawWindow()` |
| E12 | `Shift+Ctrl+数字` 在本仓**从未存在** | 全仓 grep + `git log -S` 双向确认：无 spec 目录、无 `output/data/**/*.yaml` 的 `key_binder` 绑定、无 `include/WeaselIPC.h` 命令、无处理入口。⇒ 用户记忆中的这个快捷键不来自本项目任何历史版本，它是**新功能**（FR-016） |
| E13 | 两个既有测试工程可直接承接新纯函数 | `test/TestCandidateIgnoreFilter/TestCandidateIgnoreFilter.vcxproj:42,62` 的 include 路径含 `$(SolutionDir)\include`，`:80` 的 `ClCompile` 只有测试文件本身 ⇒ 新增 `include/*.h` **零 vcxproj 改动**即可被测；`test/TestCandidateRButtonDown/TestCandidateRButtonDown.cpp`（123 行）用 `FakePanel` / `FakeClient` 复刻右键分流，不链接 `WeaselPanel.cpp`，是接 US2 分流的现成夹具 |
| E14 | Shift 选词后的 flush 判据已有 ADR 约束 | ADR `docs/adr/0018-always-show-idle-alpha-and-partial-selection-flush.md` 定义了 `ShouldCommitAfterSelect`；`RimeWithWeasel.cpp:1896-1906` 是其落地点 ⇒ 本轮 MUST 保持该判据不变（FR-004） |

## 技术方案

### 090-1 Shift 选词改页内语义

**采用「页内 API」而不是「换算全局索引」**，理由：

- E3 表明选词点上分页信息已被释放，走换算方案必须移动 `free_context` 或额外缓存，改动面更大；
- 数字键本来就页内语义（E1），改成同一个 API 后两个入口语义天然一致（P1 单一事实源）；
- 越界行为由引擎决定且与数字键一致，不需要我们自己维护 `page_no * page_size + idx` 的边界。

具体改动（`RimeWithWeasel.cpp:1877-1882`）：

```
case ShiftUpAction::kSelectCandidate: {
  const size_t local_index = s.lastIsLeft ? 1 : 2;   // 页内第 2 / 第 3 个
  if (shift_menu_open &&
      fluxing::IsPageLocalIndexValid(local_index, shift_candidate_count)) {
    rime_api->select_candidate_on_current_page(to_session_id(ipc_id), local_index);
  }
  break;
}
```

- `shift_menu_open` 与 `shift_candidate_count` 在 `:1867-1876` 取上下文时**一并缓存成局部变量**，
  然后再 `free_context` ⇒ 解决 E3，且不改变释放时机（P2 友好）。
- `IsPageLocalIndexValid` 是新增的 header-only 纯函数（见下），把 FR-002 的「越界不选词、不 fallback」
  显式化。
- E4 的「功能开关关闭 + 无候选菜单」路径由 `shift_menu_open` 守卫直接短路，不会去碰已释放的上下文。

**明确偏离 §24 的建议**：§24 提出「越界时 fallback 到全局 API」。本 plan **不采纳**，因为该 fallback
会选中**别的页**的词 —— 对用户来说「上屏一个我没看见的词」比「什么都不上屏」更错（FR-002 / SC-002）。
偏离理由写入 ADR 0023。

**新增纯函数**（放 `include/ShiftStateMachine.h`，与既有 Shift 判定同处，避免再开一个 header）：

```
// 页内次序是否落在当前页实际候选数之内。local_index 从 0 起算。
constexpr bool IsPageLocalIndexValid(size_t local_index, size_t candidate_count_on_page);
```

### 090-2 候选屏蔽：服务端过滤 + 服务端统一映射

**过滤点唯一且放在服务端**（FR-005）：`_GetCandidateInfo()`（E8）是候选信息下发的唯一汇聚点，
在这里过滤可以保证「界面看到的次序」与「服务端持有的映射」天然同源。若改回界面侧过滤，
Shift / 数字键的页内次序会与服务端的引擎次序错开，与 US1 直接冲突。

新增 header-only 单元 `include/FluxingCandidateIgnore.h`（`namespace fluxing`）：

- `LoadIgnoreListFrom(user_data_dir, schema_id, std::unordered_set<std::wstring>* out)`
  复用 E11 的路径与三种编码兼容规则（与 `WeaselPanel::LoadIgnoreList` 同语义，但服务端可用）。
- `FilterCandidatesAndBuildMap(const std::vector<std::wstring>& engine_candidates,
  const std::unordered_set<std::wstring>& ignore,
  std::vector<std::wstring>* out_visible,
  std::vector<size_t>* out_visible_to_engine)`
  **纯函数**：一次遍历同时产出「过滤后的可见列表」与「可见次序 → 引擎次序」映射，
  保证两者永不脱节（这是 US3 的核心不变量）。
- `MapVisibleIndexToEngine(const std::vector<size_t>& map, size_t visible_index, size_t* out_engine)`
  越界 ⇒ `false`，调用方 MUST 不做任何选词/删除动作。

`RimeWithWeasel` 侧：

- `include/RimeWithWeasel.h` 的 private 成员区新增 per-session 状态：
  `std::vector<size_t> m_visibleToEngineIdx;` 与 `std::unordered_set<std::wstring> m_ignoreList;`
  及 `std::wstring m_ignoreSchemaId;`（方案变了要重读名单）。
- `_GetCandidateInfo(weasel::CandidateInfo&, RimeContext&)` 增加会话标识入参（调用点 `:1066` 与
  `:1769` 一带同步），流程改为：取引擎候选 → 方案变化则重读名单 → `FilterCandidatesAndBuildMap`
  → 把 `out_visible` 写进 `cinfo.candies`（`comments` / `labels` 按同一映射同步裁剪）
  → 把 `out_visible_to_engine` 存进成员。
- 所有「页内次序」入口在下发引擎前先过映射：`SelectCandidateOnCurrentPage`（`:382-391`）、
  `DeleteCandidateOnCurrentPage`（`:393-409`）、`HighlightCandidateOnCurrentPage`（`:411-422`）、
  `ShiftUp` 的 `kSelectCandidate` 分支（`:1879-1882`）。映射失败 ⇒ 无操作（FR-002 / FR-006）。
- `DeleteCandidateOnCurrentPage` 改为**捕获**返回值（E7）：`false` ⇒ 非自造词 ⇒ 服务端取
  `ctx.menu.candidates[engine_index].text` 与当前 `schema_id`，追加写
  `<用户数据目录>\<schema>.user_ignore.txt`（复用 E11 的追加+编码规则），然后重算过滤与映射。
  **不新增进程间命令**（FR-013）：整条链路仍走既有的
  `WEASEL_IPC_DELETE_CANDIDATE_ON_CURRENT_PAGE`。

`WeaselUI/WeaselPanel.cpp` 侧：

- `OnRButtonDown()`（`:553-575`）按 E6 的现状只有 delete 分支 —— 保持**只调** `m_deleteCallback`。
  「删不掉就屏蔽」的分流落在**服务端**（上一条），因为「是否自造词」只有引擎知道（E7）。
  这样界面侧不需要第二个回调，也不需要猜。
- `_FilterIgnoredCandidates()`（`:1434-1442`）**不再被界面调用**（FR-005：服务端已过滤，
  界面二次过滤会错位）。`LoadIgnoreList()` / `_IgnoreCurrentCandidate()` 保留给
  「恢复已忽略的候选项」与服务端复用同语义时的参考实现；若确认无调用者，标注为服务端职责迁移，
  在 ADR 里记录，不做无谓删除（P3 增量）。

### 090-3 `Shift+Ctrl+数字`：本轮不做，记录为后续需求

E12 已证其从未存在 ⇒ 这是新功能，需要：① 界面/文本处理层拦截 `Ctrl+Shift+Digit`；
② 一个新的进程间命令（触发 `include/WeaselIPCData.h` 的四边同步：server / TSF / deployer +
一个 ADR + 一个 `test/TestWeaselIPC/*` 测试）；③ 该拦截点落在 `ITfTextInputProcessor::*` 回调族内，
按 P2 MUST `PostMessage` 弹到 UI 线程，不能在回调里做文件写入。验收成本与本轮两条缺陷不在同一量级。

⇒ 本轮交付右键路径（它已覆盖「删除自造词 / 隐藏系统词」的全部效果），并在最终回复中明确告知用户：
`Shift+Ctrl+数字` 不是「丢失」，本项目从未有过；如需该快捷键，作为独立需求排期。

### 测试策略

- `test/TestCandidateIgnoreFilter/TestCandidateIgnoreFilter.cpp` 扩充：改为 include
  `include/FluxingCandidateIgnore.h` 的**真实实现**（当前是 test-local 镜像，见该文件头注释），
  覆盖 ① 过滤正确性 ② 映射与过滤结果一一对应 ③ 空名单 ⇒ 映射为恒等 ④ 全部被屏蔽 ⇒ 可见列表为空
  ⑤ 映射越界 ⇒ `false` ⑥ 名单文件三种编码都能读 ⑦ 方案切换 ⇒ 名单重读。
- `test/TestCandidateRButtonDown/TestCandidateRButtonDown.cpp` 扩充 `FakeClient`：
  ① 删除成功 ⇒ 不写名单 ② 删除失败 ⇒ 写名单且内容含该词 ③ 追加不丢已有内容
  ④ 鼠标未命中候选 ⇒ 无操作 ⑤ 去抖阈值内连点 ⇒ 只处理一次。
- `include/ShiftStateMachine.h` 的 `IsPageLocalIndexValid` 由 `test/TestShiftStateMachine`
  （若不存在则由 `TestHotkeyBindings` 承接）断言：`count=3` 时 `local_index` 0/1/2 有效、3 无效；
  `count=0` 时全部无效；`count=2` 时 2 无效（对应 SC-002）。

## Constitution Check

| 原则 | 判定 | 依据 |
| --- | --- | --- |
| I. Intent Before Implementation | ✅ | 两条反馈的用户原文与心智模型已在 spec.md Intent 复述；E1–E14 全部是源码/提交历史级取证。**关键澄清**：用户以为「右键屏蔽曾经有过、现在丢了」，E5 + E12 证明它从未接线、`Shift+Ctrl+数字` 从未存在 ⇒ 这一事实必须写进 ADR 0023 与 CHANGELOG，并在最终回复里明确告知用户，不能顺着「恢复」的说法含糊过去 |
| II. Test-Backed Change | ✅ | 三个新增/修改的纯函数（`IsPageLocalIndexValid`、`FilterCandidatesAndBuildMap`、`MapVisibleIndexToEngine`）全部脱机断言；E13 证明两个既有测试工程零 vcxproj 改动即可承接；越界语义（FR-002）与「无条件屏蔽 + 同时尝试剔除」（FR-007，收尾修订后）各有专门断言 |
| III. Spec-Artifact Discipline | ✅ | 三件套齐备；spec.md 无技术栈词（R2）；tasks.md 每条带 P1/P2/P3 与 `[USx]`（R3）；「过滤点放服务端」「不采纳 §24 的 fallback 建议」「`Shift+Ctrl+数字` 延后」三个决策立 ADR 0023 |
| IV. Structured Clarification | ✅ | 无待澄清项。用户第 4 问「是否可以恢复」的答案已由 E5/E12 给出确定结论（右键那半可接线、快捷键那半是新功能），不需要再问；本轮范围按此结论锁定 |
| V. Incremental Delivery | ✅ | 090-1（Shift 页内语义）可独立发布与回滚，不依赖屏蔽；090-2 依赖 090-1 的映射入口但自身可整体回滚；090-3 明确不在本轮。三片各自有验收命令 |

### P 规则逐条

| 规则 | 判定 | 依据 |
| --- | --- | --- |
| P1 单一事实源 | ✅ | 过滤点唯一（服务端 `_GetCandidateInfo`）；「可见次序 → 引擎次序」映射由同一个纯函数与过滤结果一次产出，不可能脱节；Shift 与数字键改用同一个页内 API |
| P2 TSF 不做阻塞 I/O | ✅ | 屏蔽名单的读/写全部发生在**服务端**进程；界面层只发既有命令。名单文件写入不在 `ITfTextInputProcessor::*` 回调链路内。`ShiftUp` 分支只做局部变量判断，无新增 I/O |
| P3 不碰 librime | ✅ | 零 `librime/` 改动。E2 揭示的 `select_candidate` 头文件注释与实现不符，只在 ADR 里记录，**不改上游头文件** |
| P4 提交规范 | ✅ | 中文提交信息 + `RimeWithWeasel \| WeaselUI \| docs` 模块前缀，走 `tmp/commit-N.txt` + `git commit -F` |
| P5 用户可见字符串简体中文 | ✅ | 本轮无新增菜单项；若屏蔽成功需要界面提示，文案为简体中文 |
| P6 门禁 | ✅ | `scripts/test-infra/run-test-suite.bat`（21 工程构建 + 19 exe 运行）+ `build.bat weasel installer` |
| P7 状态外置 | ✅ | 测试与构建日志落 `tmp/suite-090.log`、`tmp/build-115.log` |
| P8 披露 | ✅ | 见下方三问 |

### P8 必披露项

1. **是否触及旧用户数据路径？** 写入只落**用户数据目录**（`<用户数据目录>\<方案标识>.user_ignore.txt`，
   与 E11 既有规则一致）。服务端读取名单时 MUST NOT 创建任何目录，MUST NOT 触碰 `%APPDATA%\Rime`。
2. **是否引入新的进程间通信消息类型？** 否（FR-013）。屏蔽复用既有的
   `WEASEL_IPC_DELETE_CANDIDATE_ON_CURRENT_PAGE`：界面发同一个命令，服务端根据引擎返回值决定
   「已删除」还是「转为屏蔽」。⇒ 不触发 IPC 四边同步。
3. **CHANGELOG 条目**：`[1.1.5]` 三条 —— 修复 Shift 翻页后仍上屏第一页候选的缺陷（改为页内次序，
   页内不足时不选词）；接通右键屏蔽候选词（自造词删除、系统词典词写入方案级屏蔽名单，
   下次输入不再出现）；**披露**「右键屏蔽」此前是写好但从未接线的死代码、`Shift+Ctrl+数字`
   从未实现过，本轮只交付右键路径。

## 风险与回滚

| 风险 | 概率 | 缓解 | 回滚 |
| --- | --- | --- | --- |
| `select_candidate_on_current_page` 与 `select_candidate` 在部分方案下行为不一致 | 低 | 数字键本来就页内语义（E1），Shift 改成同一路径反而消除不一致；`run-test-suite.bat` + 真机 §24.5 第 6 条验收 | `RimeWithWeasel.cpp` 单点 revert 回 `select_candidate` |
| 缓存页信息后 `free_context` 时机被误改，造成悬垂指针 | 中 | 只在 `free_context` **之前**把 `menu_open` 与候选数拷进局部变量，释放语句位置不动；代码评审时逐行核对 `:1867-1882` | 同上单点 revert |
| 服务端过滤改变了 `comments` / `labels` 与 `candies` 的对应关系 | 中 | `FilterCandidatesAndBuildMap` 只产出一个映射，`comments` / `labels` 用**同一个**映射裁剪；测试里断言三者长度一致且逐项对应 | 关闭过滤（名单为空即恒等映射），行为退回 1.1.4 |
| ~~`DeleteCandidateOnCurrentPage` 返回值语义在引擎升级后反转~~ **该风险已消除** | — | **收尾修订**：实现不再依赖返回值（改为无条件写名单 + 同时尝试引擎剔除），因此返回值语义无论如何变化都不影响行为。原风险的前提（E7 注释可信）已被源码取证推翻：`context.cc:146-154` 恒真 | 无需缓解；见 ADR 0023 D5 修订 |
| 屏蔽名单被误当成用户数据被安装器覆盖 | 低 | 名单在用户数据目录（`D:\Program Files\fluxing\user1\fluxing\`），不在安装目录；本轮**不碰** `output/install.nsi` 与 `output/data/user-custom/**` | 不适用 |
| 界面层遗留的 `_FilterIgnoredCandidates` 被后人误接回去造成双重过滤 | 中 | 在该函数上方加注释指明「过滤已迁至服务端，界面层不得调用」（FR-005），并在 ADR 记录 | 不适用 |

## 复杂度跟踪

新增一个 header-only 纯函数单元（`include/FluxingCandidateIgnore.h`）与 `RimeWithWeasel` 的
per-session 映射状态；`ShiftStateMachine.h` 增加一个 `constexpr` 谓词。净效果是把原本分散在界面层的
三段死代码（E5）收敛成服务端一处过滤 + 一处映射，**减少**了事实源数量。
090-3 的 `Shift+Ctrl+数字` 被显式推迟，不在本轮引入新的进程间命令，避免了 IPC 四边同步的复杂度。