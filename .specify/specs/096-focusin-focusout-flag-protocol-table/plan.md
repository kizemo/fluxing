# plan 096 — 焦点进出的对称位标志协议表

> 状态：已实现、已构建、已通过全量检查套件（2026-11-01）
> 对应 ADR：`docs/adr/0027-focusin-focusout-symmetric-flag-protocol-table.md`
> 上游依据：spec 091 / ADR 0024（焦点离开侧标志）、spec 094 / ADR 0025（共享判定纯函数）

## 上下文

spec 096 的问题陈述是「两侧的位标志是分别长出来的」：

- `WEASEL_IPC_FOCUS_IN_FLAG_PROFILE_ACTIVATED = 1u << 0`（ADR 0008 / spec 080 加的）
- `WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED = 1u << 0`（ADR 0024 / spec 091 加的）

两个常量值相同、语义对称，但写在两个独立的匿名 `enum` 里，彼此**没有任何机器可验证的约束**。
同时 `include/WeaselIPC.h` 顶部已有三段各说各话的判定逻辑：

```
ShouldHideQuickPanelOnFocusOut(param, always_show)   // 焦点离开侧，有共享口径
DecideProfileNotify(is_self_clsid, is_activated)      // TSF 侧，有共享口径
（焦点进入侧的判定散在 RimeWithWeaselHandler::FocusIn 里）  // 没有共享口径
```

技术方案（详见 ADR 0027）：

1. 在 `include/WeaselIPC.h` 增加 `struct FocusFlagSlot` 与
   `inline const FocusFlagSlot* FocusFlagProtocolTable(size_t& count)`。
   槽位含 `in_bit` / `out_bit` / `dimension`；**`in_bit` 与 `out_bit` 必须相等**
   （同一 bit 编号 = 同一语义维度）。表是编译期常量数组，**无动态分配**。
2. 增加 `WEASEL_IPC_FOCUS_IN_FLAG_APP_FOCUS_GAINED = 1u << 1`
   与已有的 `WEASEL_IPC_FOCUS_OUT_FLAG_APP_FOCUS_LOST = 1u << 1` 成对。
3. 增加对称的分类器 `ClassifyFocusIn` / `ClassifyFocusOut`
   与 `enum class FocusInKind` / `enum class FocusOutKind`，
   以及与 `ShouldHideQuickPanelOnFocusOut` 同构的 `ShouldShowQuickPanelOnFocusIn`。
4. `WeaselTSF/KeyEventSink.cpp` 的 `OnSetFocus` 改为带正向标志
   （`APP_FOCUS_GAINED` / `APP_FOCUS_LOST`），不再发 `wParam = 0`。
   `WeaselTSF/WeaselTSF.cpp` 的 `_Reconnect()` 里的 `FocusIn()` / `FocusOut()`
   **刻意保持 0** —— 那是「客户端刚连上 / 没拿到响应」的连接事件，不是焦点事件。
5. `RimeWithWeasel/RimeWithWeasel.cpp` 的 `FocusIn` 改用共享分类器与共享判定函数，
   不再自己写位与运算。
6. `test/TestWeaselIPC` 增加 `/verify-focusin-flags`：线上断言两类焦点进入事件
   携带不同的标志值；纯函数断言协议表逐行对称、bit0 取值不变、分类器优先级、
   以及「应用失焦尊重常驻开关 / 切走输入法必收起」的行为不变。

**deployer 端豁免**：`WeaselDeployer` 没有 `RequestHandler` 实现
（`grep "RequestHandler" WeaselDeployer/` 无匹配），不消费焦点进入/离开通知，
故无需同步改动（与 ADR 0025 D 段一致）。

## Constitution Check

| 条款 | 本次如何满足 |
| --- | --- |
| **I. Intent Before Implementation** | spec 096 的问题陈述 / 目标 / 非目标 / 验收标准即 What；Why 是「两侧判不对称 + 无法正向表达窗口焦点 + 对称性无人保证」；验收见 spec §验收标准 1–8，证据见本次的构建与套件输出。 |
| **II. Test-Backed Change** | 新增 `/verify-focusin-flags`（线上 + 纯函数两组断言），并接入 `scripts/test-infra/run-test-suite.bat`；既有 `TestWeaselIPC` 断言未改仍全绿。协议表与分类器属纯逻辑，可完全自动化，不需人工验证兜底。 |
| **III. Spec-Artifact Discipline** | 改动 ≥ 3 文件 ⇒ 本目录 `spec.md` + `plan.md` + `tasks.md` 三件套。spec.md 为 WHAT 层（不含技术栈词，符合 R2 的 grep 检查）。 |
| **IV. Structured Clarification** | 无未澄清项：交接卡 §6.3 已给出明确要求（补对称位标志协议表）。 |
| **V. Incremental Delivery** | 协议表 / 分类器 / 前端打标志 / 服务端消费 / 检查用例五步，每步独立可编译可验证；单次提交内保持可构建。 |
| **R1** | 本 plan + spec 即「What / Why / 如何验证」的书面形态；聊天回复同步给出。 |
| **R2** | `spec.md` 不含 `WTL` / `Boost` / `C++` / `MFC` / 具体文件名；技术名只出现在 plan.md 与 tasks.md。 |
| **R3** | `tasks.md` 每个任务带单一优先级 P1/P2/P3 与用户故事标签 `[US1]` / `[US2]`。 |
| **R4** | 本节即 Constitution Check，逐条覆盖 I–V 与 P1–P8。 |
| **R5** | 每个任务 ≤ 4 小时、1–3 个文件、输入输出明确（见 `tasks.md`）。 |
| **R6** | 完成时贴全量套件输出（`=== ALL TESTS PASSED ===` + 各用例 PASS 计数 + `[FOCUSIN] PASS` 行），并给出真机人工验证步骤。 |
| **R7** | 协议表与分类器是**唯一真源**：生产代码（`WeaselTSF` / `RimeWithWeasel`）与检查用例（`TestWeaselIPC`）引用同一份 `include/WeaselIPC.h` 实现，不各写一份。 |
| **R8** | 三件套随本次变更入库（`git log .specify/specs/` 可见）。 |
| **R9** | 改动前已读 `include/WeaselIPC.h`、`WeaselIPC/WeaselClientImpl.cpp`、`WeaselIPCServer/WeaselServerImpl.cpp` 的相关段落，未凭记忆假设。 |
| **P1 Code Style** | 2 空格缩进（项目 `.clang-format` 实为 Chromium + `SortIncludes: false` + 2 空格，与宪法文字里的「tabs」不符，以仓库实际配置为准）；新符号命名符合 `PascalCase` / `kCamelCase` 约定；include 不排序。 |
| **P2 Windows-Specific Constraints** | 无 `std::wcout` / `printf` 用户可见串；无新增用户可见字符串（本次不动任何界面文案）；**TSF 回调内无阻塞 I/O**（`OnSetFocus` 只发一条既有 IPC）；**不新增消息类型**，只复用既有 `WEASEL_IPC_FOCUS_IN` / `WEASEL_IPC_FOCUS_OUT` 的 `wParam` 位标志槽位，接收侧（`ServerImpl::OnFocusIn` / `OnFocusOut`）本来就透传 `wParam`，无需改 `WeaselIPCData.h`。 |
| **P3 Build & Dependencies** | 未引入任何第三方依赖；未触碰 `librime/` 子模块；`weasel.sln` 是工程成员的唯一真相，未改工程成员。 |
| **P4 Commit & PR** | Conventional Commits + 单模块 scope（本次用 `WeaselIPC` / `WeaselTSF` / `RimeWithWeasel` / `fluxing`）；用户可感知行为**无变化**，但本次新增了「设置面板界面分组」这一可感知变化（spec 095），已在 `CHANGELOG.md` 的 `[Unreleased]` 补条目。 |
| **P5 UX / 用户体验** | 不改默认快捷键、不改输入方案行为、不改用户可见字符串。 |
| **P6 Backwards Compatibility** | **不触碰用户数据路径**（不读、不写、不迁移任何用户数据目录）；协议线格式零变化 ⇒ 新旧端双向安全降级。 |
| **P7 Subprocess Lifecycle** | 不新增任何常驻进程；沿用既有 `WeaselServer` 主管 + 命名管道协议。 |
| **P8 Brand Fork** | 见下节「P8 三问」。 |

### P8 三问（必答披露）

1. **是否触碰旧用户数据路径？调用了哪条 P6 豁免？**
   **不触碰。** 本次既不读也不写 `%AppData%\Rime`（旧路径），也不读 `%AppData%\Fluxing`
   （fork 路径）。改的只是进程间消息里一个已有整数槽位的**取值含义**，
   不涉及任何文件路径。**未调用任何 P6 豁免条款。**
   （对照：spec 095 的常驻开关落盘走 fork 用户数据目录，那是 ADR 0020 已定的 primary 路径，
   旧路径只读兜底 —— 与本次无关。）
2. **是否引入新的 IPC 消息类型？**
   **没有。** 没有新增命令号、没有改 `PipeMessage` 字段布局、没有改 `WeaselIPCData.h`。
   焦点进入/离开两个既有命令的 `wParam` 从「只有 bit0 有定义」扩展到「bit0 + bit1 有定义」，
   属于**位标志槽位的语义补全**，线格式（命令号 / 字段布局）**零变化**。
   按 P2 的要求，接收侧已同步：`WeaselIPCServer/WeaselServerImpl.cpp` 的
   `OnFocusIn` / `OnFocusOut` 本来就原样透传 `wParam` 给 handler，
   无需改动；**deployer 端豁免**（无 `RequestHandler` 实现，不消费这两条消息）。
3. **是否有用户可见字符串 / 默认值 / 路径变更？**
   本次**没有**。协议表不产生任何用户可见差异；用户可感知行为逐条比对后**完全一致**
   （切到输入法显示面板、切走输入法收起面板、应用失焦尊重常驻开关，全部不变）。
   本次**同一提交批次**里的 spec 095 有用户可见变化（设置面板新增「界面」分组与
   「快捷面板常驻」复选框），已在 `CHANGELOG.md` 的 `[Unreleased]` 补条目。

## 改动清单

| 文件 | 改动 |
| --- | --- |
| `include/WeaselIPC.h` | 协议表 + 两个分类器 + `ShouldShowQuickPanelOnFocusIn` + bit1 常量（位标志本身由并行任务先行加入） |
| `WeaselTSF/KeyEventSink.cpp` | `OnSetFocus` 携带 `APP_FOCUS_GAINED` / `APP_FOCUS_LOST` |
| `RimeWithWeasel/RimeWithWeasel.cpp` | `FocusIn` 改用共享分类器与判定函数 |
| `test/TestWeaselIPC/TestWeaselIPC.cpp` | mock handler 记录焦点进入的 `wParam` + `/verify-focusin-flags` + 主程序分发 |
| `scripts/test-infra/run-test-suite.bat` | 接线 `/verify-focusin-flags` |
| `docs/adr/0027-*.md` | 架构决策记录 |
| `docs/adr/README.md` | 索引 |
| `CHANGELOG.md` | `[Unreleased]` 条目 |
| `.specify/specs/096-*/` | 三件套 |
| `.specify/memory/lessons-learned.md` | 不新增（本次无新事故；spec 095 的编码事故记为 L162） |

## 验证计划

1. `cmd /c "build.bat weasel"` ⇒ 零错误。
2. `scripts/test-infra/run-test-suite.bat` ⇒ `=== ALL TESTS PASSED ===`，
   且日志含 `[FOCUSIN] PASS: the FocusIn event kinds are distinguishable on the wire
   and the flag protocol table is symmetric`。
3. `git diff --stat` + `git status -sb` 复核暂存区不含 `librime` 与构建产物。
