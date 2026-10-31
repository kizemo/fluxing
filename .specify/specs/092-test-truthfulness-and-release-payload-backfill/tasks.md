# 092 · Tasks — 让「测试全绿」名副其实；补齐发布载荷与文档索引

> 优先级：**P1** = 本轮必做；**P2** = 本轮应做；**P3** = 可延后。
> 用户故事标签：`[US092-A]` 测试可信度；`[US092-B]` 载荷仓补齐；
> `[US092-C]` 变更记录归位；`[US092-D]` 决策记录索引。

---

## T001 [P1] [US092-A] 全部测试工程统一加 `/utf-8`

- **输入**：`test/` 下 27 个 `.vcxproj`（`TestPipeProtocol` 原本已有，不动）
- **输出**：
  - 既有 `<AdditionalOptions>` 者：内容**前置**插入 `/utf-8 `；
  - 无该节点者（`TestPipeChannelRace` / `TestShiftIPCStateMachine` / `TestShiftIPCWire`）：
    在 `</LanguageStandard>` 之后**独立成行**新建节点（行尾按文件实际）
- **约束**：字节级 API（`ReadAllBytes` / `WriteAllBytes`），**不动行尾**；幂等（已含则跳过）
- **验收**：`test/` 下 28 个 `.vcxproj` **全部**含 `/utf-8`；
  `git diff --numstat` 与 `git diff --ignore-cr-at-eol --numstat` **完全一致**
  （`EOL_ONLY_DIFF=False`）；差异行形态去重后**只有 `/utf-8` 插入**

## T002 [P1] [US092-A] `TestCandidateIgnoreFilter` 探针判别性

- **输入**：`test/TestCandidateIgnoreFilter/TestCandidateIgnoreFilter.cpp`
- **输出**：
  - 新增 `AssertionScope`（进入时快照 `boost::detail::test_errors()`，退出时再快照）
  - 新增 `ReportCase()`；5 个用例按差值决定 PASS/FAIL
  - 汇总行**只在全过时**打印 `5 / 5 assertions passed`，否则打印真实计数 + 失败断言数
- **约束**：退出码仍由 `boost::report_errors()` 决定（语义不变）；
  `boost::detail::test_errors()` 来自 `boost/detail/lightweight_test.hpp`（转发头）⇒ 可直接用
- **验收**：故意改坏一条断言 ⇒ 该用例报 FAIL、退出码非 0

## T003 [P1] [US092-A] `TestServerSideIgnoreMapping` 探针判别性

- **输入**：`test/TestCandidateIgnoreFilter/TestServerSideIgnoreMapping.cpp`
- **输出**：与 T002 同构的 `AssertionScope` / `ReportCase`（各自匿名命名空间，无符号冲突）；
  14 个用例改为判别性 PASS；汇总行条件化输出 `14 / 14 assertions passed`
- **验收**：故意改坏一条断言 ⇒ 报 FAIL、退出码非 0

## T004 [P1] [US092-A] `TestQuickPanelRefactor` 从 SKIP 改为实跑

- **输入**：`test/TestQuickPanelRefactor/TestQuickPanelRefactor.cpp`（+ `TestQuickPanelRefactorMain.cpp`）
- **输出**：删除 SKIP + `return 0` 死代码，改为 **14 组（≈30 条）现行契约实跑断言**：
  - T1–T4 窗口创建 / 类名 / 顶层 `WS_POPUP` / 四个扩展样式
  - T5–T6 client rect 与 `*_phys` / dpr 同步、**子窗口数 == 0**
  - T7–T9 `Hide()` 不销毁、`Show()` 复用同一 HWND、`ToggleMode()`
  - T10 `HitTest`（4 个按钮中心 / brand 区 `-2` / 空白 `-1` / 越界 `-1`）
  - T11 `Hide()` 复位 hover/active
  - T12 点击路由（`hit1 → s_onPhrases` / `hit2 → s_onShortcut` / `0,3` no-op）
  - T13 残留契约：`Show(true)` 后 `IsFullwidth()` 仍为 false
  - T14 `Show` 保存 / 清空 6 个回调
- **约束**：**不改**生产代码；断言按**现行**契约写，**不沿用** TODO spec 046/047 的
  「11 Fluxing + 1 native close」预期
- **验收**：输出无 `SKIP` 字样；退出码 0

## T005 [P1] [US092-A] `TestQuickPanelDialog` 删 SKIP + 删死代码 + 补结构锚点

- **输入**：`test/TestQuickPanelDialog/TestQuickPanelDialog.cpp`
- **输出**：
  - 删 `:116-118` 的 SKIP 打印 + `:444-565` 的不可达死代码
    （`ExitProcess` 后的 `return 0` + T1–T6 老断言）
  - 随之失去调用方的 `CreateTestHost` / `CountByClassName` 一并删除
    （`CountAllDescendants` 保留，供 T18 使用）
  - 移除只被死代码使用的 `#include "../../WeaselServer/resource.h"`
  - 补 **T17–T19**：T17 窗口类名 == `FluxingQuickPanel_v3`；
    T18 子窗口数 == 0；T19 `Hide()` 不销毁 + `Show()` 复用同一 HWND
  - 保留 T8 / T11–T16（状态与持久化）
- **约束**：`#if 0` 的 T7 / T9 / T10 **保留**（它们是「已下线功能」的显式历史记录，
  有注释说明下线原因，与「意外死代码」性质不同）
- **验收**：输出无 `SKIP` 字样；`=== N / N assertions passed ===` 中 N 为实测值；退出码 0

## T006 [P2] [US092-A] 保留的残留接口钉住 + 披露

- **输入**：`test/TestQuickPanelRefactor/TestQuickPanelRefactor.cpp`（T13）
- **输出**：把「`Show(true)` 后 `IsFullwidth()` 仍为 false」钉成断言；
  在交付报告里向用户披露 `s_fullwidth` / `currentFullwidth` / `s_onFullwidth` 是**残留 API**
- **约束**：**不改**生产代码、**不删**参数（属 API 变更，需另开 ADR）

## T007 [P1] [US092-B] `release/` 载荷仓回填 1.1.1 / 1.1.2

- **输入**：`output/archives/fluxing-1.1.1-installer.exe`、`fluxing-1.1.2-installer.exe`
- **输出**：`release/fluxing-1.1.1-installer.exe`（42,753,471 B）、
  `release/fluxing-1.1.2-installer.exe`（42,763,583 B）
- **约束**：`cmd /c copy /Y "<绝对路径>" "<绝对路径>"`，**绝不带 `cd`**；
  **不是重新构建**
- **验收**：SHA256 分别为 `211BBA1B…C51F94B9` 与 `4238124C…BAAEC36`，与原件一致；
  `git log --diff-filter=D -- release/` 无输出（从未进过 git）

## T008 [P1] [US092-C] 变更记录归位

- **输入**：`CHANGELOG.md`（无 BOM / 全 CRLF，448,999 B）
- **输出**：
  - `[Unreleased]` 段只留标题 + 「本段暂无待发布条目。」+ spec 092 条目
  - 新增 `## [1.1.1] - 2026-10-29`（L25）与 `## [1.1.0] - 2026-10-28`（L74）
  - `[1.1.5]` 两处**追加**订正（原句均保留）
- **约束**：字节级编辑（解码 → 变换 → 重编码 → `WriteAllBytes`），保持全 CRLF、无 BOM；
  **不篡改已发布段落**
- **验收**：原 `[Unreleased]` 非空行丢失 **0**；CRLF 计数与 LF-only = 0

## T009 [P1] [US092-D] `docs/adr/README.md` 补 ADR 编号索引

- **输入**：`docs/adr/README.md`（无 BOM / 全 LF，5,265 B）
- **输出**：在 L18 插入 `## ADR index`（19 行表格 + 2 条注记）：
  编号 / 标题 / 文件 / 状态
- **约束**：`Status` 列**逐字取各 ADR 自带值**，不编造、不归一；
  注记写明 `0002`–`0006` **从未使用**（`git log --diff-filter=D` 无输出）
- **验收**：索引 19 行；`git diff` 只 +33 行 / −0 行；ADR 行标题逐字未改

## T010 [P1] 探针残留清理

- **输入**：`D:\Program Files\fluxing\user1\fluxing\__mavis_probe.tmp`
- **输出**：用受信回收器（`mavis-trash.js` + `ELECTRON_RUN_AS_NODE=1`）删除
- **约束**：可恢复删除；**不**永久删除；**不**用 `Remove-Item`
- **验收**：删除后复查文件不存在

## T011 [P1] 完整套件取证

- **输入**：本轮全部测试改动
- **输出**：`scripts/run-tests.bat`（→ `run-test-suite.bat`）完整输出 + 退出码
- **约束**：跑前确认**无** `WeaselServer.exe` / `TestWeaselIPC.exe` 在跑
  （squat 检查会误报）；跑完用 `start_service.bat` 恢复服务
- **验收**：套件退出码 0；**无** `SKIP` 字样；两个 QuickPanel 检查的汇总行 N = 实测总数

## T012 [P1] 提交 + 推送

- **输入**：本轮全部改动
- **输出**：提交（Conventional Commits + 模块前缀 `fluxing` / `docs`）⇒ 推 `kizemo/Fluxing`
- **约束**：提交前 `git status -sb` + `git diff --stat` 复核，
  挡住 `weasel.props` / `env.bat` / `*.obj` / `librime` 误提交；
  提交信息写 `tmp/commit-092.txt` 再 `git commit -F`
- **验收**：`git rev-list --left-right --count kizemo/Fluxing...HEAD` = `0 0`

## T013 [P1] 交付 + 取下一轮优先级

- **输入**：本轮全部成果
- **输出**：交付报告（结论 + 证据 + 未完成项）+ 就 §6.3 三个候选方向**问用户优先级**
- **约束**：**不自行开工** §6.3 的三个方向；
  §6.1 验收第 8 条如实报告「本会话无法完成」及其原因
- **验收**：用户拿到优先级选择 + 真机复验安排 + 旧版嵌套目录清理授权

---

## 执行结果（2026-10-31）

| 任务 | 状态 | 证据 |
|---|---|---|
| T001 | ✅ | `test/` 下 **28** 个 `.vcxproj` 全部含 `/utf-8`；`git diff --numstat` 与 `--ignore-cr-at-eol` 结果**完全一致**（`EOL_ONLY_DIFF=False`） |
| T002 | ✅ | `TestCandidateIgnoreFilter` **5/5**；PASS/FAIL 由 `boost::detail::test_errors()` 前后快照差决定 |
| T003 | ✅ | `TestServerSideIgnoreMapping` **14/14**（同构判别性） |
| T004 | ✅ | `TestQuickPanelRefactor` **32/32** 实跑，日志无 `SKIP` |
| T005 | ✅ | `TestQuickPanelDialog` **50/50**（含新增 T17–T19），日志无 `SKIP` |
| T006 | ✅ | T13 钉住「`Show(true)` 后 `IsFullwidth()` 仍 false」；残留 API 已于交付报告披露 |
| T007 | ✅ | `release/fluxing-1.1.1-installer.exe` / `-1.1.2-` SHA256 与 `output/archives/` 原件一致 |
| T008 | ✅ | CHANGELOG 段序恢复降序；`[Unreleased]` 矛盾句已删；全 CRLF / 0 裸 LF / 无 BOM |
| T009 | ✅ | `docs/adr/README.md` +33 行 / −0 行，19 行索引 |
| T010 | ✅ | `__mavis_probe.tmp` 已用受信回收器删除并复查不存在 |
| T011 | ✅ | `scripts/run-tests.bat` → `SUITE_EXIT=0`，0 个 FAIL；日志 2 处 `SKIP` 为测试名/计数器误命中 |
| T012 | ⏳ | 提交 + 推送 `kizemo/Fluxing`（本表落盘后执行） |
| T013 | ⏳ | 交付报告 + 向用户取 §6.3 优先级（交付时执行） |
