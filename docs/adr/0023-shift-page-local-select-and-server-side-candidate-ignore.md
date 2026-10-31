# 0023. Shift 选词改走页内索引 + 候选屏蔽的过滤点上移到服务端

- **状态**: Accepted
- **日期**: 2026-10-30
- **关联**: ADR 0010 / 0012 / 0015 / 0017 / 0018 D3 · spec 031（屏蔽名单格式与落点）· spec 087 · spec 090 · 交接卡 §24 第 3/6/7 条
- **相关**: `include/ShiftStateMachine.h` · `include/FluxingCandidateIgnore.h`（新） · `include/RimeWithWeasel.h` · `RimeWithWeasel/RimeWithWeasel.cpp` · `WeaselUI/WeaselPanel.cpp`（仅加注释）

## 背景（Context）

### 1. Shift 选词用的是**全局索引** API，数字键用的是**页内索引** API

`RimeWithWeasel/RimeWithWeasel.cpp:1877-1882`：

```cpp
size_t idx = s.lastIsLeft ? 1 : 2;
rime_api->select_candidate(to_session_id(ipc_id), idx);
```

而 `SelectCandidateOnCurrentPage`（`:382-391`）与数字键分支走的是
`select_candidate_on_current_page`。两个 API 在 `librime/include/rime_api.h:467-473`
的注释**完全相同**（都写 "Select a candidate from the current page."），但实现不同：
`select_candidate` 收**全局次序**，`select_candidate_on_current_page` 收**页内次序**。

⇒ 翻页后按 Shift，上屏的是**第一页**的第 2 / 3 个候选。与用户第 3 条反馈逐字吻合。
**上游注释不可采信**，本 ADR 采信实现（`librime/src/rime/rime_api_impl.cc`）。

### 2. 用户第 4 条「之前已实现、当前丢失」——取证结论是**一半从未存在，一半写了没接线**

| 半边 | 事实 |
|---|---|
| `Shift+Ctrl+数字` 按次序移除 / 隐藏 | **本仓从未存在**：无 spec、`output/data/**/*.yaml` 无绑定、`include/WeaselIPC.h` 无对应命令、无 handler；`git log -S` 双向确认 |
| 右键点候选屏蔽 | **代码存在但从未被调用**：`WeaselUI/WeaselPanel.cpp:1391-1432 LoadIgnoreList`、`:1434-1442 _FilterIgnoredCandidates`、`:1444-1468 _IgnoreCurrentCandidate` 三个函数**有定义、无调用点**；`OnRButtonDown`（`:553-575`）只有 delete 分支（`:570-572` 调 `m_deleteCallback`） |

⇒ 不是「恢复」，而是「接线 + 补新功能」。这条澄清必须写进 CHANGELOG 与最终回复，
否则用户会以为 1.1.5 又漏做了一半。

### 3. 屏蔽一旦生效就会**改变页内索引**，与用户第 3 条反馈直接耦合

若过滤留在 UI 侧（`_FilterIgnoredCandidates` 的原设计位置），服务端看到的候选序列
仍是引擎原始序列 ⇒ UI 上的「第 2 个」在引擎里可能是「第 3 个」。
此时 Shift / 数字键按页内索引选词会**选错词**。

⇒ §24 的第 6/7 条必须与第 3 条**一起**改，且过滤点必须上移。

### 4. `DeleteCandidateOnCurrentPage` 丢弃了引擎返回值

`:393-409` 的注释明写 "The engine does the is_user_dict check internally;
the C API does NOT expose is_user_dict" ⇒ **前端无法预知**一个候选是自造词还是
系统词典词，只能事后按返回值分流。

### 5. IPC 侧没有承载分页信息的字段

`include/WeaselIPCData.h:181-229` 的 `CandidateInfo` **没有 `page_size`**；
`_GetCandidateInfo`（`:599-622`）只设 `highlighted` / `currentPage`(= `page_no`) /
`is_last_page`，**不设 `totalPages`**（全仓无写入点）；
`WeaselUI/Layout.cpp:12-15` 直接引用 `candies/comments/labels/highlighted`，**不做分页切片**。

## 决策（Decision）

### D1 Shift 选词改用 `select_candidate_on_current_page`，**不做**索引换算

与数字键同路径 ⇒ 「Shift 选词」与「数字键选词」在结构上不可能再分叉（P1 单一事实源）。

**被否决**：读 `page_no × page_size` 把页内索引换算成全局索引再调 `select_candidate` ——
需要前端知道 `page_size`，而背景 §5 已证它根本不在 IPC 载荷里；
换算还会把「引擎分页语义」这条本不该前端知道的细节复制到前端。

### D2 **不采纳**交接卡 §24 的「越界时 fallback 到全局 API」建议（显式偏离，理由在此）

§24 建议「页内索引越界时退回全局 API」。**不采纳**。

越界只可能发生在「当前页候选数 < 3」（典型是末页只剩 1–2 个候选）。
此时 fallback 到 `select_candidate(2)` 会选中**别的页**的第 3 个候选 ——
用户按右 Shift 想选「本页第 3 个」，结果上屏了一个他在屏幕上**根本没看见**的词。
这比「什么都不做」错得更严重，且不可稳定复现（依赖翻页深度与词频）。

⇒ 越界一律**不选词、不上屏**，`DecideShiftUpAction`（`ShiftStateMachine.h:152-163`）
的返回语义不变。若将来用户反馈「按了没反应」，正确修法是把「不可选」做成**可见反馈**，
而不是回退到 fallback。

### D3 新增 `constexpr IsPageLocalIndexValid(local_index, candidates_on_page)` 承载 D2 的判定

置于 `include/ShiftStateMachine.h` 文件尾（`:189`）。放这里而非 `RimeWithWeasel.cpp` 的理由：
Constitution II 要求判定逻辑可单测；`ShiftStateMachine.h` 已是 header-only 纯函数集
（`:152-163 DecideShiftUpAction`、`:176-188 DecideAsciiModeScope`），
测试工程零链接改动即可覆盖，也便于造**坏数据负对照**（越界 / 空页）。

注意 `DecideShiftUpAction` 的 `!feature_enabled` 早退排在 `menu_open` 之前
（`:152-163`），新判定必须插在 `menu_open` 之后、`select` 之前，不得改变既有早退顺序。

### D4 候选屏蔽的**唯一**过滤点 = 服务端 `_GetCandidateInfo`

`RimeWithWeasel::_GetCandidateInfo`（`:599-622`，被 `:1063` 与 `:1769` 两处调用）
是候选序列离开服务端的**唯一汇聚点**。在这里过滤 ⇒ UI、Shift、数字键、高亮、删除
看到的都是同一个「过滤后序列」。服务端同时维护 visible → engine 的索引映射。

**所有按页内索引进入的入口都必须走这个映射**（漏一个就出现「屏蔽后选错词」）：

| 入口 | 位置 |
|---|---|
| `SelectCandidateOnCurrentPage` | `RimeWithWeasel.cpp:382-391` |
| `DeleteCandidateOnCurrentPage` | `:393-409` |
| `HighlightCandidateOnCurrentPage` | `:411-422` |
| Shift 页内选词（D1 新改处） | `:1877-1882` |

**被否决**：在 UI 侧过滤（原死代码的设计位置）—— 见背景 §3。

### D5 「删不掉就屏蔽」的分流落在**服务端**，不在 UI

> **2026-10-30 收尾修订**：本节原文写的是「以引擎返回值分流：成功 ⇒ 已剔除、不写名单；
> 失败 ⇒ 系统词典词、写名单」。**该分流方案已作废**，实现改为「无条件写名单 + 同时照常
> 尝试引擎剔除」。理由如下。

`DeleteCandidateOnCurrentPage` 的实际改法：由 visible 索引经 `_MapVisibleIndexToEngine`
取得 `engine_index` 与候选文本后，**先无条件追加写屏蔽名单**，再照常调
`rime_api->delete_candidate_on_current_page`；其返回值**不再参与任何分流**。

作废原方案的取证（三条，均取自 `librime/` 只读源码）：

1. `librime/src/rime/context.cc:146-154` 的对应分支在正常调用姿势下**恒为真** ⇒
   「返回 false」几乎只出现在调用姿势错误（会话失效、索引越界）时，
   而**不是**「这是系统词典词」的信号。
2. `librime/src/rime/rime_api_impl.h:982-1000` 里所有 `return false` 分支都是参数 /
   状态校验失败，没有一条表达「该候选不可从用户词典剔除」。
3. C API 里**不存在** `is_user_dict`（背景 §4 已证）⇒ 前端与服务端都无法预判词源。

⇒ 以返回值分流会得到「系统词典词永远进不了名单」的结果，用户第 4 条反馈的后半段
（隐藏系统词）等于没做。无条件写名单后：自造词既被引擎剔除、也进名单（名单条目此后
不再出现在候选序列中，语义一致）；系统词典词进名单后被过滤，用户观感即「这个词以后
不出现了」。撤销入口沿用既有托盘项「恢复已忽略的候选项」
（`ID_WEASELTRAY_RESTORE_IGNORED`，`WeaselServer.rc:332`），清空名单即恢复。

分流（现在是「名单写入 + 索引映射」）仍必须在**服务端**：见背景 §4，UI 侧拿不到
`is_user_dict`，且背景 §3 已证 UI 侧过滤会让页内索引错位。

`WeaselUI/WeaselPanel.cpp` 的 `OnRButtonDown`（`:553-575`）**保持只调 `m_deleteCallback`**，
一行逻辑都不改 ⇒ 右键手势的 100 ms 去抖（`:561-569`）与
`test/TestCandidateRButtonDown` 的既有 4 条断言全部沿用。

### D6 屏蔽名单沿用 spec 031 的既有格式与落点，**不新造格式**

路径 `<WeaselUserDataPath()>\<schema_id>.user_ignore.txt`，UTF-8 + `\r\n`；
读取兼容 UTF-8 BOM / UTF-16LE BOM / 裸 UTF-8（`WeaselPanel.cpp:1391-1432` 已实现这三种）。

⇒ 托盘「恢复已忽略的候选项」（`WeaselServerApp.cpp:324-342`）**无需改动**
即可清空服务端写出的名单；名单落在用户数据目录
（`D:\Program Files\fluxing\user1\fluxing\`），不产生 `%APPDATA%\Rime`（ADR 0020 / 0021）。

新增 `include/FluxingCandidateIgnore.h`（header-only 三个纯函数：解析名单一行 /
判定是否被屏蔽 / 由 visible 索引反查 engine 索引）。两个测试工程
（`TestCandidateIgnoreFilter.vcxproj:42,62`、`TestQuickPanelDialog.vcxproj:52,72`）
都已把 `$(SolutionDir)\include` 放进 include 路径 ⇒ 零 vcxproj 改动可测。

### D7 **不新增** IPC 消息类型 ⇒ 不触发四边同步

右键屏蔽复用既有 `WEASEL_IPC_DELETE_CANDIDATE_ON_CURRENT_PAGE`；
`include/WeaselIPC.h:18-40` 的命令枚举不新增任何值（无 refresh、无 ignore）。
⇒ 不触发 CLAUDE.md §2 的「IPC 四边同步」硬规则（server / TSF / deployer + ADR + `test/TestWeaselIPC/*`）。

### D8 `Shift+Ctrl+数字` 本轮**不做**，记录为后续独立需求

它需要：新增 IPC 命令（⇒ 四边同步）+ TSF 侧按键拦截（`Ctrl+Shift+数字`
目前会被当普通组合键送进引擎，而 ADR 0009 / 0011 已把「前端发字符不发键码」定为契约，
拦截它要先论证不破坏该契约）+ P2 绕行论证 + 独立验收。

⇒ 记为 spec 090 FR-016 / T9，并在 CHANGELOG 与最终回复中**明确告知用户**
「这一半从未实现过，不是丢失；本轮实现的是右键那半」。

### D9 UI 侧三个死函数**保留**，但加禁用注释

`_FilterIgnoredCandidates` / `_IgnoreCurrentCandidate` / `LoadIgnoreList` **不删**
（`LoadIgnoreList` 是 D6 名单格式的实现依据；删了会丢掉三种 BOM 兼容的既有代码）。
在前两个函数上方加中文注释、点名 spec 090 与本 ADR D4，
**禁止后人把 UI 侧过滤接回去**。

## 后果（Consequences）

- (+) 翻页后 Shift 选词与屏幕上看到的次序一致，用户第 3 条反馈闭环。
- (+) Shift 与数字键选词共用同一 API ⇒ 结构上不再可能分叉。
- (+) 右键屏蔽从死代码变成可用功能；「自造词删除 / 系统词屏蔽」的分流由**一律写入屏蔽名单**（见 D5 收尾修订），
  不靠前端猜（背景 §4）。
- (+) 过滤点唯一 ⇒ 屏蔽后所有按次序入口（Shift / 数字键 / 高亮 / 删除）不会错位。
- (−) 服务端多了一份 visible → engine 映射状态，其**失效时机**必须与候选序列刷新严格同步；
  今后任何新增的按页内索引入口都要记得走映射（D4 的表即检查表）。
- (−) `librime/include/rime_api.h:467-473` 的注释与实现不符。本 ADR 采信实现；
  上游注释若将来被修正，结论不变，但本 ADR 引用的行号会失效。
- (−) 末页候选数 < 3 时 Shift **静默不选词**（D2 的有意选择）。
- (−) `Shift+Ctrl+数字` 未实现 ⇒ 用户第 4 条反馈只闭合了一半（D8）。
- (−) `WeaselUI/WeaselPanel.cpp` 里留着三个不再被调用的函数（D9），
  静态分析工具会继续报「未使用」；靠注释而非删除来防止误接回。
- **P2 论证**：屏蔽名单的文件读写发生在**服务端** `RimeWithWeasel`
  （WeaselServer 进程的工作线程），不在 `ITfTextInputProcessor::*` 回调内
  ⇒ 不违反 P2「TSF 内禁止阻塞 I/O」。前端右键手势仍只发一条既有 IPC 命令，无新增 I/O。
- 回归测试：`test/TestCandidateIgnoreFilter`（原 spec 031 的 5 断言 test-local 镜像）
  扩充覆盖三个纯函数，含**坏数据负对照**（非法索引 / 空名单 / 越界 visible 索引）
  以证明断言有牙；`test/TestCandidateRButtonDown` 的 4 条断言保持通过；
  `IsPageLocalIndexValid` 由 `ShiftStateMachine` 的既有测试工程覆盖。