# tasks 096 — 焦点进出的对称位标志协议表

> 优先级定义：**P1** = 阻塞其它任务；**P2** = 重要；**P3** = 可延后。
> 用户故事标签：`[US1]` = 维护者不再需要靠人眼保证两侧对称；
> `[US2]` = 焦点进入/离开两侧具备对称的判定口径。

## Phase 1 — 协议表与共享纯函数

- [x] **T1** `[US1]` **P1** — 在 `include/WeaselIPC.h` 增加 `struct FocusFlagSlot`
      （`in_bit` / `out_bit` / `dimension`）与
      `inline const FocusFlagSlot* FocusFlagProtocolTable(size_t& count)`。
      输入：现有两个位标志常量。输出：一张编译期常量表（**无动态分配**），
      槽位 0 = 输入法激活状态，槽位 1 = 应用窗口焦点。
- [x] **T2** `[US1]` **P1** — 补齐焦点进入侧的正向位标志：
      `WEASEL_IPC_FOCUS_IN_FLAG_APP_FOCUS_GAINED = 1u << 1`，
      与已有的 `WEASEL_IPC_FOCUS_OUT_FLAG_APP_FOCUS_LOST = 1u << 1` 成对。
      输入：ADR 0027 的协议表设计。输出：两侧 bit1 含义一致。
- [x] **T3** `[US2]` **P1** — 增加对称的分类器
      `ClassifyFocusIn` / `ClassifyFocusOut` 与
      `enum class FocusInKind` / `enum class FocusOutKind`，
      以及 `ShouldShowQuickPanelOnFocusIn`（与既有
      `ShouldHideQuickPanelOnFocusOut` 同构）。
      输入：T1/T2 的常量。输出：两侧互为镜像的纯函数，
      未知位落进 `kUnknown` ⇒ 行为与今天一致。

## Phase 2 — 生产代码消费

- [x] **T4** `[US1]` **P2** — `WeaselTSF/KeyEventSink.cpp` 的 `OnSetFocus`
      改为携带 `APP_FOCUS_GAINED` / `APP_FOCUS_LOST`。
      输入：T2 的常量。输出：普通窗口焦点事件不再发 `wParam = 0`。
      **约束**：`WeaselTSF/WeaselTSF.cpp` 的 `_Reconnect()` 里的
      `FocusIn()` / `FocusOut()` **保持 0**（连接事件不是焦点事件）。
- [x] **T5** `[US2]` **P2** — `RimeWithWeasel/RimeWithWeasel.cpp` 的 `FocusIn`
      改用 `ClassifyFocusIn` + `ShouldShowQuickPanelOnFocusIn`，
      不再自己写位与运算；活动会话门禁与 early-return 保持不变。
      输入：T3 的纯函数。输出：服务端与协议表同源。

## Phase 3 — 判别性验证

- [x] **T6** `[US1]` **P2** — `test/TestWeaselIPC/TestWeaselIPC.cpp`：
      mock handler 记录每次焦点进入的原始 `wParam` 到 `focusin_param.txt`
      （与既有的 `focusout_param.txt` 同构）。
      输入：既有 `focusout_record_path()` 的写法。输出：线上可读回的记录。
- [x] **T7** `[US1]` **P2** — 新增 `/verify-focusin-flags`：
      - 纯函数断言：表恰 2 槽位、两侧位掩码相等且为单比特、维度名非空且互异、
        **bit0 取值与既有常量逐字相等**（线格式零变化的回归保护）、
        分类器四类输入的预期结果、profile 位优先；
      - 行为不变断言：「应用失焦尊重常驻开关 / 切走输入法必收起」两条与加标志前一致；
      - 线上断言：两种焦点进入事件携带**不同**的标志值，且值分别等于对应常量。
      输入：T1–T3。输出：PASS / FAIL 与失败原因明细。
- [x] **T8** `[US1]` **P2** — `scripts/test-infra/run-test-suite.bat` 接线
      `/verify-focusin-flags`（插在 `/verify-focusout-flags` 之后、`/stop` 之前
      —— 它必须在 mock server 还活着时跑）。
      输入：T7。输出：套件会执行该模式。

## Phase 4 — 文档与交付

- [x] **T9** `[US2]` **P2** — 写 `docs/adr/0027-focusin-focusout-symmetric-flag-protocol-table.md`
      （MADR 四段式：背景 / 决策 / 被否决的替代方案 / 后果），
      并在 `docs/adr/README.md` 补索引行。
- [x] **T10** `[US2]` **P2** — `CHANGELOG.md` 的 `[Unreleased]` 补条目：
      协议表、焦点进入侧正向标志、两侧判定对称化、线格式零变化、deployer 豁免。
- [x] **T11** `[US2]` **P3** — 写本目录三件套（`spec.md` / `plan.md` / `tasks.md`），
      plan.md 含 Constitution Check（P1–P8 逐条）+ P8 三问。

## Phase 5 — 验证与提交

- [x] **T12** `[US2]` **P1** — 构建：`cmd /c "build.bat weasel"` ⇒ 零错误。
- [x] **T13** `[US2]` **P1** — 全量套件：`scripts/test-infra/run-test-suite.bat`
      ⇒ `=== ALL TESTS PASSED ===`，日志含 `[FOCUSIN] PASS`。
      **前置**：跑之前必须先 `WeaselServer.exe /quit` 停服务（PPL 进程不得 taskkill），
      跑完再重启装机版服务端。
- [x] **T14** `[US2]` **P2** — 分模块小提交（Conventional Commits + 单模块 scope）：
      `feat(WeaselIPC)`（协议表）/ `feat(WeaselTSF)`（前端打标志）/
      `feat(RimeWithWeasel)`（服务端消费）/ `test(fluxing)`（用例 + 套件接线）/
      `docs(fluxing)`（ADR / CHANGELOG / 三件套）。
      提交前 `git status -sb` + `git diff --stat` 复核，
      暂存区**不得**包含 `librime` 与构建产物。

## 未纳入本 spec 的后续项

- **[US2]** **P3** 未来若出现第三个语义维度（如「远程桌面会话切换」），
  按协议表规则**加槽位**而不是单侧加标志；届时需要新增两侧常量与一行断言。
- **[US2]** **P3** `WeaselTSF::_Reconnect()` 的连接事件目前仍发 `wParam = 0`。
  若将来需要区分「首次连接」与「重连」，应**加槽位**而不是复用 bit1。
