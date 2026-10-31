# 092 · Plan — 让「测试全绿」名副其实；补齐发布载荷与文档索引

## 背景与取证

### 清单来源

交接文档 `docs/archive/handoffs/handoff-2026-10-31-fluxing-1.1.6-spec091-delivery.md` §6.2
列出「系统性问题清单」**七项**：

| # | 清单项（原文） | 本轮处置 |
|---|---|---|
| 1 | 测试 `.vcxproj` 统一加 `/utf-8` | ✅ 已做（27 个工程改动，全库 28 个现已全部含 `/utf-8`） |
| 2 | `TestCandidateIgnoreFilter` 的 PASS 打印是无条件的（探针无判别性） | ✅ 已做（连同同目录的 `TestServerSideIgnoreMapping`） |
| 3 | `TestQuickPanelRefactor` 仍是 SKIP | ✅ 已做（改为实跑） |
| 4 | `release/` 仍缺 1.1.1 / 1.1.2 两个安装包 | ✅ 已做（从 `output/archives/` 原件回填） |
| 5 | `__mavis_probe.tmp` 残留 | ✅ 已做（受信回收器删除，已复查不存在） |
| 6 | `CHANGELOG` 的 `[Unreleased]` 段仍留着 087/088 的旧内容 | ✅ 已做（并新增 `[1.1.0]` / `[1.1.1]` 两段承接） |
| 7 | `docs/adr/README.md` 无 ADR 编号索引 | ✅ 已做（19 篇全列） |

### 本轮新查出的第 8 项

清单只列了 `TestQuickPanelRefactor`。本轮逐个核对 `test/` 下所有带 `SKIP` 的输出时，
发现 **`test/TestQuickPanelDialog/TestQuickPanelDialog.cpp` 是同一个病的第二个实例**：

- `:116-118` 打印 `SKIP: TestQuickPanelDialog (broken in v0.18.29.0; ...)`；
- `:444-565` 是一整段**永不可达**的死代码（`ExitProcess` 之后还有 `return 0` + 6 组老断言）；
- 那 6 组老断言期待的「5 个子控件」布局，在现行生产代码里**两代之前就不存在了**。

⇒ 它同样是「跳过 + 假绿」。本轮一并处置（见 092-3）。

### 根因取证：两代设计漂移

| 世代 | 变更 | 对测试的影响 |
|---|---|---|
| spec 038（v0.18.27.0） | 面板用 4 个 Fluxing 子控件 + 1 个原生关闭 X | 老断言 T1–T6 按这个写 |
| spec 045（`3eab3f5`，v0.18.29.0） | 控件创建从 `OnCreate` 移到 `Show()`；老断言全部失效 | 文件当时没跟进，被整段 `return 0` 短路 |
| GDI v3-rev3 重写 | 面板改为**自绘**：4 个按钮全部 GDI 直绘，**零子窗口**，类名 `FluxingQuickPanel_v3` | 「11 Fluxing + 1 native close」的预期（TODO spec 046/047）**也是两代之前的设计** |

⇒ **不能沿用 TODO spec 046/047 的预期重写**。本轮一律按**现行生产契约**写断言。

### 现行契约取证（`WeaselServer/QuickPanelDialog.cpp` / `.h`）

| 契约 | 位置 | 事实 |
|---|---|---|
| 窗口类名 | `.cpp:37` `static const wchar_t kWindowClassName[]` | `FluxingQuickPanel_v3` |
| 窗口样式 | `.cpp:1345-1440` `Show()` | `WS_POPUP` + `WS_EX_NOACTIVATE|TOOLWINDOW|LAYERED|TOPMOST` |
| 零子控件 | `.cpp:780+` `OnCreate()` | 只算几何与 DPI，**不创建任何子窗口** |
| `Hide()` 不销毁 | `.cpp:1442-1453` | 只 `KillTimer(1/2)` + `ShowWindow(SW_HIDE)`，**不清 `s_hwnd`** |
| `Show()` 复用 | `.cpp:1345-1440` | `s_hwnd` 存在 ⇒ `ShowWindow(SW_SHOWNOACTIVATE)` + `SetWindowPos(HWND_TOPMOST)`，**不重建** |
| 命中测试 | `.cpp:448-487` `HitTest()` | brand 区 ⇒ `-2`；按钮 ⇒ 索引；其余/越界 ⇒ `-1` |
| 点击路由 | `.cpp:599-668` | `hit==1 → s_onPhrases`；`hit==2 → s_onShortcut`；`hit 0/3` no-op |

### 探针无判别性的取证

`test/TestCandidateIgnoreFilter/` 下两个文件原来在断言之后**无条件**打印 `PASS:`。
修复手段不是「把打印语句挪位置」，而是引入**可判别的计数口径**：

- boost 断言计数 API：`F:\b183\boost\core\lightweight_test.hpp:88` → `inline int& test_errors()`，
  位于 `namespace boost { namespace detail {` 内；
- `F:\b183\boost\detail\lightweight_test.hpp` 是**转发头**（include `<boost/core/lightweight_test.hpp>`）
  ⇒ `boost::detail::test_errors()` 可直接用；
- `boost::report_errors()` 在 `boost/core/lightweight_test.hpp:499`，返回错误数（<256 时原样返回）。

做法：每个用例进入时快照 `test_errors()`，退出时再快照一次；
**差值 > 0 ⇒ 该用例 FAIL**，否则 PASS。汇总行只在全过时打印 `N / N assertions passed`，
否则打印真实计数 + 失败断言数。退出码仍由 `boost::report_errors()` 决定（语义不变）。

### `/utf-8` 的取证

- `test/` 下 **28** 个 `.vcxproj`，本轮**改动 27** 个（`TestPipeProtocol` 原本就有）；
  24 个在既有 `<AdditionalOptions>` 前置插入，3 个（`TestPipeChannelRace` /
  `TestShiftIPCStateMachine` / `TestShiftIPCWire`）**没有** `AdditionalOptions` 节点，
  在 `</LanguageStandard>` 之后**独立成行**新建节点；
- 差异纯度：`git diff -- 'test'` 去重后**只有 3 种变更行形态，全部是 `/utf-8` 插入**；
  `git diff --numstat` 与 `git diff --ignore-cr-at-eol --numstat` **完全一致**
  （`EOL_ONLY_DIFF=False`）⇒ **无行尾残留 churn**；
- 改完后 `test/` 下 28 个 `.vcxproj` **全部**含 `/utf-8`。

### 发布载荷回填的取证

- `git log --diff-filter=D -- release/` **无输出** ⇒ 1.1.1 / 1.1.2 两个安装包**从未进过 git 历史**；
- 全盘也搜不到 ⇒ 只能从 `output/archives/` 的**原件**回填，**不是重新构建**；
- 回填手段：`cmd /c copy /Y "<绝对路径>" "<绝对路径>"`（绝不带 `cd`）；
- 双比对：

  | 文件 | 字节数 | SHA256 |
  |---|---|---|
  | `release/fluxing-1.1.1-installer.exe` | 42,753,471 | `211BBA1B91A1F4646BB2D72758B4F1D278917B9921FBBD096D02F678C51F94B9` |
  | `release/fluxing-1.1.2-installer.exe` | 42,763,583 | `4238124C8EEFC5B86785D37640F1CF99C61004124302406AFC5A750CEBAAEC36` |

### ADR 数量的更正

任务书按「24 篇 ADR」下达，但 `docs/adr/` 目录与 `git ls-tree HEAD docs/adr/` 均为 **19 篇**
（0001、0007–0024）；`git log --diff-filter=D -- docs/adr/` 无输出 ⇒ `0002`–`0006`
**从来没有被用过**，不存在被删的 ADR。索引按 **19 篇**建，并在注记里写明这一点。

### 变更记录归属表（证据：`git log --diff-filter=A -- docs/adr/*.md` + 安装包构建时间 + 交接卡）

| 条目 / ADR | 归属 | git 证据 |
|---|---|---|
| ADR 0011 抓键按产出字符（含 3 个 refactor/test 小节） | **1.1.0** | ADR 文档 `20aa324` 10-28 00:29；实现 12:59–17:44 |
| ADR 0013 长显模式 | **1.1.0** | `ef6360f` 10-28 19:42；载荷 1.1.0 首建 10-28 21:00 |
| ADR 0014 CapsLock | **1.1.0** | `57004c9` 10-28 20:59 |
| ADR 0015 Shift 约定 + 弹出菜单 | **1.1.0** | `bcc7daf` 10-28 22:44；1.1.0 第 2 次打包 22:43 |
| ADR 0016 FocusOut 守卫 + 64 位换装 | **1.1.1** | 文档 `99aeac0` 10-29 09:48；实现 `c55abfa` 09:31（**例外已标注**：该实现随 09:47 重打的 1.1.0 载荷先出货） |
| ADR 0017 扫描码判定 + 「先取事实」 | **1.1.1** | `e8c5789` 10-29 11:23；1.1.1 载荷 15:06 |
| ADR 0018 常驻不被点没 / alpha 60% / 部分选词 | **1.1.1** | `549cf1b` 10-29 15:01；1.1.1 载荷 15:06 |
| 安装包载荷重编 / `release/` 载荷仓补齐（1.0.0+0.21.0.9） | **1.1.0（标「归属待考」）** | 交接卡 §13.2/§14 + `eedad70` 10-28 19:06 ⇒ 实际随 **1.0.0** 出货 |

---

## 技术方案

### 092-1 `/utf-8` 批量应用（幂等 + 字节级）

- 既有 `<AdditionalOptions>` 者：在其内容**前置**插入 `/utf-8 `；
- 无该节点者：在 `</LanguageStandard>` 之后**独立成行**新建节点（行尾按文件实际）；
- 全程字节级 API（`ReadAllBytes` / `WriteAllBytes`），**不动行尾**；
- 幂等判定：`/utf-8` 已存在则跳过 ⇒ 第二次跑无 diff。

### 092-2 探针判别性：计数快照（不改退出码语义）

`AssertionScope` 快照 `boost::detail::test_errors()`；
`ReportCase()` 按差值决定 PASS/FAIL；
汇总行**只在全过时**打印 `N / N assertions passed`，否则打印真实计数 + 失败数。
退出码仍由 `boost::report_errors()` 决定 ⇒ 语义与原来一致，只是标签终于可信。

### 092-3 两个快捷面板测试的分工（避免重复断言）

| 测试 | 覆盖面 | 本轮动作 |
|---|---|---|
| `TestQuickPanelRefactor` | **窗口结构与命中契约**：类名、样式、几何/DPI、零子窗口、`Hide()` 不销毁、`Show()` 复用、`HitTest`、点击路由、回调存取 | 从 SKIP 改为 **14 组实跑断言** |
| `TestQuickPanelDialog` | **状态与持久化**：`SetOn*` 接线、alpha 演算、常驻态持久化、ESC 语义、`quickpanel.state` 落点 | 删 SKIP + 删不可达死代码 + 补 **T17–T19 结构锚点** |

**为什么保留少量重叠**：两个测试的入口不同（一个走 `Show()` 全链路，一个走状态面），
结构锚点各留 3 条，属**防御性冗余**而非重复劳动。

### 092-4 删掉的死代码

`TestQuickPanelDialog.cpp:444-565`（`ExitProcess` 之后的 `return 0` + T1–T6 老断言）
与随之失去调用方的 `CreateTestHost` / `CountByClassName` 两个辅助函数。
同时移除只被死代码使用的 `#include "../../WeaselServer/resource.h"`。

### 092-5 残留接口的处理（不改生产代码）

`QuickPanelDialog::s_fullwidth` 全仓只在 `QuickPanelDialog.cpp:196` 静态初始化处被赋值，
`Show()` 明写 `(void)currentFullwidth;`，`s_onFullwidth` **无任何调用点**，
4 按钮布局里也没有全角/ASCII 按钮 ⇒ `IsFullwidth()` **恒为 false**。
调用方 `WeaselServerApp.cpp:409-412` 仍在传 `m_handler->IsFullShape()`。

定性：**残留 API（死参数），不是用户可见功能缺陷**。
处置：**不改生产代码、不删参数**（删参数属 API 变更，需另开 ADR），
只在测试里把现状**钉住**（T13），并在交付报告里向用户披露。

### 092-6 变更记录：不是只清空，而是**各归其位**

`[Unreleased]` 段原内容分属两个**已发布**版本（087 / 088）⇒ 只清空会让读者看不到历史。
处置：新增 `[1.1.1] - 2026-10-29` 与 `[1.1.0] - 2026-10-28` 两段承接，
**不修改任何已发布段落的正文**（除两处**追加**订正说明，原句均保留）。

### 092-7 ADR 索引：只抄不编

索引表的 `Status` 列**逐字取各 ADR 自带的 status 值**，不编造、不归一。
三代标签形态不同（0001 用 `## Status` 段；0007–0013 用 `- **Status**:`；0014–0024 用 `- **状态**:`）
⇒ 在注记里写明「以 ADR 文件自身为准」。

---

## 测试策略

| 测试 | 断言组 | 判别性保证 |
|---|---|---|
| `TestCandidateIgnoreFilter` | 5 组 | 每组前后快照 `test_errors()`；**故意改坏一条 ⇒ 必须变红** |
| `TestServerSideIgnoreMapping` | 14 组 | 同上 |
| `TestQuickPanelRefactor` | 14 组（≈30 条） | 全部实跑；含 `HitTest` 四个按钮中心 / brand 区 `-2` / 空白 `-1` / 越界 `-1` |
| `TestQuickPanelDialog` | T8 / T11–T16（保留）+ T17–T19（新增） | T17 类名、T18 零子窗口、T19 `Hide()` 不销毁 + `Show()` 复用 |

**点击模拟的已知残余风险**：`WM_LBUTTONDOWN` 无条件置 `s_dragging=TRUE`，
`WM_LBUTTONUP` 靠两次 `GetCursorPos` 位移 < 4px 判 click；
若测试执行期间用户移动鼠标，可能被判成 drag。
既有 `test/v0_19_0_32_e2e/v0_19_0_32_e2e.cpp:744-762` 用同一手法且通过 ⇒ 可接受。

---

## Constitution Check

| 原则 / 规则 | 判定 | 依据 |
|---|---|---|
| **I. Intent Before Implementation** | ✅ | spec §1–§3 写明 What / Why / 验收 |
| **II. Test-Backed Change** | ✅ | 本轮**就是**测试修复；21 个工程全量重编 + 完整套件 |
| **III. Spec-Artifact Discipline** | ✅ | ≥3 文件 ⇒ 本目录三件套 |
| **IV. Structured Clarification** | ✅ | 无未决产品决策；三个候选方向按 §6.3 **不自行开工**，最后问用户 |
| **V. Incremental Delivery** | ✅ | 092-1…092-7 各自可独立验证 |
| **R1** 意图 + 验收前置 | ✅ | spec §3（AC-1…AC-6） |
| **R2** spec 无技术栈词 | ✅ | spec.md 全程用户语言（「实跑 / 探针无判别性 / 载荷仓」皆为项目词汇，非栈词） |
| **R3** 任务带优先级 + 用户故事标签 | ✅ | tasks.md（P1/P2/P3 + US092-A…D） |
| **R4** plan 含 Constitution Check | ✅ | 本节 |
| **R5** 任务粒度 ≤4h / 1-3 文件 | ✅ | tasks.md（单任务最多 3 个文件） |
| **R6** 「完成」须贴测试输出 | ✅ | 交付前贴完整套件输出 |
| **R7** 单一事实源 | ✅ | 判别性口径取 boost 官方 `test_errors()`，不另造计数 |
| **R8** 规格进 git | ✅ | 随本轮提交入库 |
| **R9** 查证优先于记忆 | ✅ | 每条结论附源码行号 / git 证据 / 字节数 |
| **P1** 代码风格 | ✅ | 2 空格 / Chromium（`.clang-format`）；改动行走 `git-clang-format` |
| **P2** Windows 约束 | ✅ | 用户可见文案简体中文；**零协议改动**；TSF 线程未触碰，无阻塞 I/O |
| **P3** 构建与依赖 | ✅ | 无新依赖；`librime/` 不动 |
| **P4** 提交与 PR | ✅ | Conventional Commits + 模块前缀；CHANGELOG 有 `[Unreleased]` 条目 |
| **P5** UX | ✅ | 本轮无用户可见功能变更；文档类变更已入 CHANGELOG |
| **P6** 向后兼容 | ✅ | 产品行为零改动；`quickpanel.state` 落点与格式一字未动 |
| **P7** 子进程生命周期 | ✅ | 不新增长驻进程；测试进程行为不变 |
| **P8** 品牌分支 | ✅ | 见下方必披露项 |

### P 规则逐条要点

- **P2（协议）**：本轮**不触碰任何 IPC 协议** ⇒ server / TSF / deployer 三端同步要求**不触发**。
- **P4**：`CHANGELOG.md` 的 `[Unreleased]` 段保留本轮条目；新增 `[1.1.0]` / `[1.1.1]` 两段**只承接旧内容**，不篡改已发布段落。
- **P6**：不读写用户数据目录 ⇒ `quickpanel.state` 落点与格式不变。
- **P7**：不新增长驻进程；`release/` 只增加两个静态安装包。

### P8 必披露项（三问）

1. **是否触碰旧用户数据路径？** —— **否**。本轮不读写任何用户数据目录；
   `quickpanel.state` 的落点、文件名、内容格式**一字未动**；测试里的临时落点照旧指向 `%TEMP%`。
2. **是否引入新的 IPC 消息类型？** —— **否**。本轮**零协议改动**，
   `include/WeaselIPC.h` 未被触碰 ⇒ 三端同步要求不触发。
3. **CHANGELOG 是否有用户可见条目？** —— **有**：`[Unreleased]` 段记本轮 spec 092 条目
   （测试可信度修复 / 载荷仓补齐 / ADR 索引 / 变更记录归位）。

---

## 风险与回滚

| 风险 | 可能性 | 缓解 |
|---|---|---|
| 把测试改成「认当前行为」掩盖了真缺陷 | 中 | 新断言钉的是**客观事实**（类名 / 样式 / 几何 / 零子窗口 / `Hide()` 不销毁 / `Show()` 复用 / 命中路由），不是恒真断言；判别性由 `test_errors()` 差值保证 |
| 删死代码误伤仍在用的辅助函数 | 低 | 编译期即暴露（未引用静态函数在 `/W3` 下也不报错，但链接期与调用点 grep 双向核对过） |
| `/utf-8` 插错位置导致编译炸 | 低 | 21 个工程全量重编通过 |
| release 回填的其实是「重新构建的伪原件」 | 低 | `cmd /c copy /Y` 原件复制 + SHA256 + 字节数双比对 |
| CHANGELOG 归属判错 | 中 | 段首已标「归属待考」与例外说明；**不静默改已发布段落** |
| ADR 索引状态列不可机器读 | 低 | 只抄各文件自带值，不编造；三种标签形态在注记里说明 |

**回滚**：本轮一次提交，可整体 `git revert`；已发布版本的安装包不受影响。

---

## 复杂度跟踪

| 偏差 | 理由 |
|---|---|
| 清单 7 项 → 实际做了 8 项 | 第 8 项（`TestQuickPanelDialog` 同样是 SKIP + 不可达死代码）由逐个核对 `test/` 下所有 `SKIP` 输出查出；同一根因（两代设计漂移）⇒ 合并进本 spec，不另开 |
| 两个快捷面板测试保留少量重叠断言 | 入口不同（`Show()` 全链路 vs 状态面），属防御性冗余 |
| 不删 `s_fullwidth` / `currentFullwidth` / `s_onFullwidth` | 属**接口变更**，按决策分流规则需另开 ADR；本轮只钉现状并披露 |
| CHANGELOG 新增 `[1.1.0]` / `[1.1.1]` 而非只清 `[Unreleased]` | 087/088 的旧内容**本就属于已发布版本**；只清空会造成历史丢失 |
| ADR 索引表头用英文 | `docs/adr/README.md` 通篇英文；ADR 标题逐字保留中英混排 |
| 真机旧版嵌套目录（`weasel\fluxing\weasel\` 内的 0.21.0.0）只报告不清理 | 独立于本清单，清理属不可逆操作 ⇒ 交用户决定 |
