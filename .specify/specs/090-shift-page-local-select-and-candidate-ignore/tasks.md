# 090 · Tasks — Shift 翻页后按页内次序选词，以及右键屏蔽候选词

> 单一垂直切片。优先级 P1/P2/P3，用户故事标签 [US1] [US2] [US3] [US4]。
> 状态标记：[ ] 未开始 · [~] 进行中 · [x] 完成。

## T1 — 页内次序有效性谓词 [P1] [US1]

- **输入**：`include/ShiftStateMachine.h:146-189`（`ShiftUpAction` / `DecideShiftUpAction` / 文件尾）。
- **输出**：`include/ShiftStateMachine.h` 修改。
- **做什么**：新增 `constexpr bool IsPageLocalIndexValid(size_t local_index, size_t candidate_count_on_page)`，
  语义 `local_index < candidate_count_on_page`。附注释说明「越界 MUST 不选词、MUST NOT fallback 到
  全局次序 API」（偏离 §24 建议的理由见 ADR 0023）。
- **验收**：脱机断言全通过 —— `count=3` 时 `0/1/2` 为真、`3` 为假；`count=2` 时 `2` 为假（对应 SC-002）；
  `count=0` 时全假。断言落在 `test/TestShiftStateMachine`（不存在则落 `test/TestHotkeyBindings`）。

## T2 — Shift 选词改页内语义 [P1] [US1]

- **输入**：T1；`RimeWithWeasel/RimeWithWeasel.cpp:1855-1926`（`ShiftUp` 全段，含 `:1867-1876` 取上下文、
  `:1877-1882` 选词分支、`:1896-1906` flush）；`:382-391` `SelectCandidateOnCurrentPage`；
  ADR 0018 的 `ShouldCommitAfterSelect` 判据。
- **输出**：`RimeWithWeasel/RimeWithWeasel.cpp` 修改。
- **做什么**：① 在 `:1867-1876` 取上下文处，把 `menu_open` 与「当前页候选数」一并拷进局部变量，
  **`free_context` 的位置与调用次数不动**；② `kSelectCandidate` 分支改为
  `local_index = s.lastIsLeft ? 1 : 2`，用 `shift_menu_open && IsPageLocalIndexValid(local_index, count)`
  守卫，通过后调 `rime_api->select_candidate_on_current_page(...)`，否则不做任何事；
  ③ flush 判据与 ascii 分支（`:1914-1924`）、`s = ShiftState{}`（`:1925`）全部不动；
  ④ 注意 `DecideShiftUpAction` 在 `!feature_enabled` 时会先于 `menu_open` 返回 `kSelectCandidate`
  （取证 E4），守卫必须能短路这条路径。
- **验收**：`build.bat weasel` 通过；真机按 §24.5 第 6 条验收（翻到第 2 页后左/右 Shift 上屏第 2 页
  第 2/第 3 个）；最后一页只剩 2 个候选时按右 Shift 无上屏、无报错、窗口不关；
  Shift 双击切英文与 CapsLock 关闭态回归项不破。

## T3 — 屏蔽名单读取 + 过滤映射纯函数 [P1] [US3] [US4]

- **输入**：`WeaselUI/WeaselPanel.cpp:1391-1432`（`LoadIgnoreList` 的路径与三种编码兼容规则）、
  `include/FluxingUserPaths.h`、`test/TestCandidateIgnoreFilter/TestCandidateIgnoreFilter.cpp`（现为
  test-local 镜像）与其 `.vcxproj:42,62,80`。
- **输出**：新文件 `include/FluxingCandidateIgnore.h`（header-only，`namespace fluxing`）；
  `test/TestCandidateIgnoreFilter/TestCandidateIgnoreFilter.cpp` 改为 include 真实实现。
- **做什么**：实现 `LoadIgnoreListFrom(user_data_dir, schema_id, out_set)`、
  `FilterCandidatesAndBuildMap(engine_candidates, ignore, out_visible, out_visible_to_engine)`、
  `MapVisibleIndexToEngine(map, visible_index, out_engine)`。过滤与映射 MUST 由同一次遍历产出。
  读取只 `open` 不 `create`，MUST NOT 建目录、MUST NOT 触碰 `%APPDATA%\Rime`。
- **验收**：`TestCandidateIgnoreFilter` 全通过 —— ① 过滤结果不含名单词 ② 映射与过滤结果逐项对应
  ③ 空名单 ⇒ 映射为恒等 ④ 全部被屏蔽 ⇒ 可见列表为空且映射为空 ⑤ `MapVisibleIndexToEngine` 越界
  ⇒ `false` ⑥ UTF-8 BOM / UTF-16LE BOM / 裸 UTF-8 三种名单文件都能读 ⑦ 追加写入不丢已有内容。

## T4 — 服务端过滤接入候选信息下发 [P1] [US3]

- **输入**：T3；`RimeWithWeasel.cpp:599-622`（`_GetCandidateInfo`）、调用点 `:1061-1067`（`_Respond`）
  与 `:1750-1780`（`_GetContext`）；`include/RimeWithWeasel.h:118-200`（private 区与成员声明尾部）；
  `include/WeaselIPCData.h:181-229`（`CandidateInfo`）；`WeaselUI/Layout.cpp:12-15`。
- **输出**：`include/RimeWithWeasel.h`、`RimeWithWeasel/RimeWithWeasel.cpp` 修改。
- **做什么**：① 头文件 private 成员区新增 `std::vector<size_t> m_visibleToEngineIdx;`、
  `std::unordered_set<std::wstring> m_ignoreList;`、`std::wstring m_ignoreSchemaId;`；
  ② `_GetCandidateInfo` 增加会话标识入参，两个调用点同步；③ 流程改为「取引擎候选 → 方案变化则
  重读名单 → `FilterCandidatesAndBuildMap` → `candies` 写可见列表、`comments`/`labels` 用**同一映射**
  裁剪 → 映射存成员」；④ 界面层不做第二次过滤（`Layout.cpp` 不改）。
- **验收**：`build.bat weasel` 通过；无屏蔽名单时候选显示与 1.1.4 完全一致（恒等映射）；
  手工在用户数据目录放一个 `<方案>.user_ignore.txt` 后重新输入，该词不出现，且 `candies` /
  `comments` / `labels` 三者长度一致、逐项对应。

## T5 — 页内次序入口统一走映射 [P1] [US1] [US3]

- **输入**：T3、T4；`RimeWithWeasel.cpp:382-391`（`SelectCandidateOnCurrentPage`）、`:393-409`
  （`DeleteCandidateOnCurrentPage`）、`:411-422`（`HighlightCandidateOnCurrentPage`）、
  `:1879-1882`（Shift 分支）。
- **输出**：`RimeWithWeasel/RimeWithWeasel.cpp` 修改。
- **做什么**：四个入口在下发引擎前一律先 `MapVisibleIndexToEngine`；映射失败 ⇒ 直接返回、不做任何
  引擎调用。Shift 分支复用 T2 的守卫。
- **验收**：屏蔽掉当前页第 1 个词后，数字键 `1`、左 Shift、右键三个入口命中的都是界面上显示的那个词
  （SC-005）；映射越界时无引擎调用（用日志或断点核对）。

## T6 — 右键屏蔽：无条件写名单 + 同时尝试引擎剔除 [P1] [US2] [US4]

- **输入**：T5；`RimeWithWeasel.cpp:393-409`（返回值当前被丢弃，取证 E7；**该返回值已被证伪为不可用判据**，见 ADR 0023 D5 修订）；
  `WeaselUI/WeaselPanel.cpp:553-575`（`OnRButtonDown`，取证 E6）、`:1444-1468`
  （`_IgnoreCurrentCandidate` 的追加写入与编码规则，取证 E11）；
  `include/WeaselIPC.h:18-40`（确认复用既有命令，不新增）。
- **输出**：`RimeWithWeasel/RimeWithWeasel.cpp` 修改；`WeaselUI/WeaselPanel.cpp` 只加注释。
- **做什么**（**2026-10-30 收尾修订**：原「以返回值分流」方案已作废，取证见 ADR 0023 D5 修订）：
  ① `DeleteCandidateOnCurrentPage` 把可见索引经 `_MapVisibleIndexToEngine` 翻成引擎索引；
  ② **无条件**取 `ctx.menu.candidates[engine_index].text` 与当前 `schema_id`，按 E11 的规则
  （`FILE_APPEND_DATA` + `OPEN_ALWAYS`、UTF-8 + `\r\n`）追加写
  `<用户数据目录>\<schema>.user_ignore.txt`，写前更新内存名单；
  ③ **同时**照常调 `rime_api->delete_candidate_on_current_page` 尝试引擎剔除（自造词能被真删掉），
  但**返回值不参与任何分流** —— 已取证它在候选存在时恒为 `true`，无法区分自造词与系统词；
  ④ 写完重算过滤与映射；⑤ `WeaselPanel::OnRButtonDown` **保持只调** `m_deleteCallback`
  （名单写入与索引映射都在服务端，界面不需要第二个回调）；⑥ 在 `_FilterIgnoredCandidates`
  与 `_IgnoreCurrentCandidate` 上方加注释「过滤已迁至服务端，界面层不得调用，否则次序错位」。
  **不新增进程间命令**（FR-013）。
- **实际落地**：①–⑥ 全部完成。新增 `include/FluxingCandidateIgnore.h`（header-only，
  `LoadIgnoreListFrom` / `ParseIgnoreListText` / `AppendToIgnoreList`）承载 E11 的编码规则；
  撤销入口沿用既有托盘「恢复已忽略的候选项」（`ID_WEASELTRAY_RESTORE_IGNORED`）。
- **验收**：`TestCandidateRButtonDown` 6/6、`TestCandidateIgnoreFilter` 5/5 + 14/14 全通过 ——
  右键命中 ⇒ 无条件写名单且含该词；追加不丢已有内容；解析函数二次调用不累积（本轮修掉的真 bug）；
  鼠标未命中 ⇒ 无操作；去抖阈值内连点 ⇒ 只处理一次。**原「删除成功 ⇒ 不写名单」这条断言已随方案作废移除。**
  真机按 §24.5 第 7 条验收。

## T7 — 「恢复已忽略的候选项」联通 [P2] [US2]

- **输入**：`WeaselServer/WeaselServerApp.cpp:324-342`（`ID_WEASELTRAY_RESTORE_IGNORED` 处理器）；
  T4 的服务端名单缓存。
- **输出**：视取证结果决定是否改 `WeaselServerApp.cpp` / `RimeWithWeasel.cpp`。
- **做什么**：确认清空名单文件后，服务端的 `m_ignoreList` / `m_ignoreSchemaId` 会失效重读
  （T4 已按「方案变化则重读」实现，此处需要额外覆盖「文件被外部清空」的情形 —— 最简做法是
  在每次 `_GetCandidateInfo` 时按文件修改时间或每次都重读，取代价更低者并在 ADR 记录）。
- **验收**：屏蔽若干词 → 从托盘菜单点「恢复已忽略的候选项」→ 重新输入同一拼音，被屏蔽的词全部恢复
  （SC-006）；名单文件被清空。

## T8 — 门禁 [P1]

- **输入**：T1–T7 全部改动。
- **输出**：`tmp/suite-090.log`、`tmp/build-115.log`、`tmp/fmt-090.log`。
- **做什么**：① `.\clang-format.ps1 -i` 过改动的 C++ 文件；② `Stop-Process -Name WeaselServer -Force`
  后跑 `scripts\test-infra\run-test-suite.bat`；③ `build.bat weasel`；④ 跑完用
  `D:\Program Files\fluxing\weasel\start_service.bat` 恢复服务。
- **验收**：`SUITE_EXIT=0` 且日志出现 `=== ALL TESTS PASSED ===`；`BUILD_EXIT=0`；
  `git diff --numstat` 无 `librime/` / `weasel.props` / `env.bat` / `*.obj` 误入。

## T9 — 记录 `Shift+Ctrl+数字` 为后续独立需求 [P3]

- **输入**：取证 E12；spec.md FR-016。
- **输出**：ADR 0023 的一节 + CHANGELOG 的披露条目 + 最终回复中的明确告知。
- **做什么**：写清三件事 —— ① 本项目从未实现该快捷键（无 spec、无按键绑定、无进程间命令、无处理入口）；
  ② 「右键屏蔽」是写好但从未接线的死代码，本轮接通；③ 该快捷键若要实现，需要新增按键拦截 +
  新的进程间命令（触发四边同步）+ 绕开输入处理回调的阻塞限制，作为独立需求排期。
- **验收**：ADR 0023 含该节；CHANGELOG `[1.1.5]` 含披露条目；最终回复明确回答用户第 4 问。