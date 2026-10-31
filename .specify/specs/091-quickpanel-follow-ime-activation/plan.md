# 091 · Plan — 快捷设置栏显隐跟随输入法激活状态；语言栏菜单去除「快速面板常驻」

## 背景与取证

### 真机验收结果（v1.1.5，用户未重启状态下）

| # | 反馈 | 结论 |
|---|---|---|
| 1 | 面板默认常驻，鼠标移开不消失 | ✅ 符合预期 |
| 2 | `Alt+,` 或托盘右键能取消常驻 | ✅ 符合预期（**取消入口必须保留**） |
| 3 | 语言栏勾选态仍不回显 | ⚠️ **用户改口**：改为「从菜单中去除『快捷面板常驻』」 |
| 4 | 不悬停约 85% 不透明、悬停 100%、观感变平 | ✅ 符合预期（ADR 0022 D1–D3） |
| 5 | 翻页后 Shift 选页内第 2/3 个、越界不动作 | ✅ 符合预期（spec 090） |
| 6 | 右键屏蔽 → 名单落盘 → 托盘撤销 | ⏳ 未成功，**待重启后复验**（非本轮范围） |
| 7 | Shift 中英切换、Esc | ✅ 符合预期 |

### 新缺陷的根因链（源码级取证）

**现象**：切到其他输入法后快捷设置栏不消失。

**第一步 —— 面板在常驻态下没有任何隐藏路径。**
`WeaselServer/QuickPanelDialog.cpp` 的两条自动隐藏分支都被常驻守卫包住：

- `:686` 轮询超时隐藏：`if (!AlwaysShowMode() && (nowTick - s_showTime) >= kShowGraceMs)`
- `:744` 鼠标移出隐藏：`if (!AlwaysShowMode() && hit == -1 && !s_dragging && !inGrace)`

⇒ `AlwaysShowMode() == true` 时**面板不可能被隐藏**。

**第二步 —— 「切到别的输入法」这个事件确实到了服务端，但被同一个守卫挡掉。**

| 环节 | 位置 | 事实 |
|---|---|---|
| 事件源 | `WeaselTSF/WeaselTSF.cpp:414-446` `OnActivated(clsid, guidProfile, isActivated)` | `ITfActiveLanguageProfileNotifySink` 已真正 advise（spec 080 / ADR 0008 修好的） |
| 判定 | `WeaselTSF/WeaselTSF.cpp:443-444` | `isActivated == false` ⇒ `_PostProfileNotify(kProfileNotifyDeactivated)` |
| 投递 | `WeaselTSF/WeaselTSF.cpp:404-407` | 通知线程调 `m_client.FocusOut()`（**不带任何区分信息**） |
| 线格式 | `WeaselIPC/WeaselClientImpl.cpp:178-180` | `_SendMessage(WEASEL_IPC_FOCUS_OUT, 0, session_id)` ⇒ `wParam` **恒为 0** |
| 派发 | `WeaselIPCServer/WeaselServerImpl.cpp:272-279` | `m_pRequestHandler->FocusOut(wParam, lParam)` ⇒ **`wParam` 被透传** |
| 服务端 | `RimeWithWeasel/RimeWithWeasel.cpp:531-559` | `if (!QuickPanelDialog::AlwaysShowMode()) QuickPanelDialog::Hide();` |

**第三步 —— 同一个 `FocusOut` 承载了两种语义不同的事件。**

| 事件 | 触发点 | 语义 |
|---|---|---|
| 输入焦点离开（应用失焦 / 切窗口） | `WeaselTSF/KeyEventSink.cpp:87-93` `OnSetFocus(fForeground=false)` | 「我暂时不在前台了，面板**可以**留着」（ADR 0016 D2） |
| 用户切到别的输入法 | `WeaselTSF/WeaselTSF.cpp:404-407` profile deactivated | 「这个输入法**不在场**了，面板**必须**走」（用户本轮要求） |

两者线格式完全相同（`wParam` 都是 0）⇒ 服务端**无法区分** ⇒ 只能用一条
`!AlwaysShowMode()` 一刀切 ⇒ **常驻态下两条都不隐藏** ⇒ 切输入法后面板残留。

**第四步 —— 这一刀是 ADR 0016 有意加的，不是笔误。**
`docs/adr/0016-quickpanel-focusout-guard-and-x64-swap.md` D1/D2 明确写了
「守卫加在调用点，不动 `Hide()` 本体」「只对『输入焦点离开』这一条路径设防」——
**但它当时无法区分两种事件**，于是把「切输入法」一并挡掉了。
`RimeWithWeasel.cpp:542-546` 的注释里还留着当年的原始意图：
「**当用户切换到 en-US IME 时** … 这里 `Hide()` panel … 这是最干净的 IME switch path」。

**结论（根因）**：`FocusOut` 缺少「事件种类」信息 ⇒ 服务端把「切输入法」误判成「切窗口」⇒
常驻守卫下不隐藏。不是「面板忘了隐藏」，而是**「该隐藏」这个信号从未送到**。

### 语言栏菜单的证据

`WeaselTSF/WeaselTSF.rc` 的三个弹出菜单块各有 1 条「快速面板常驻」项：

| 块 | 行 | 文案 |
|---|---|---|
| `IDR_MENU_POPUP`（英） | `:72` | `MENUITEM "快速面板常驻 (&A)", ID_QUICKPANEL_ALWAYS_SHOW` |
| `IDR_MENU_POPUP_HANT`（繁） | `:97` | `MENUITEM "快速面板常駐 (&A)", ID_QUICKPANEL_ALWAYS_SHOW` |
| `IDR_MENU_POPUP_HANS`（简） | `:122` | `MENUITEM "快速面板常驻 (&A)", ID_QUICKPANEL_ALWAYS_SHOW` |

托盘菜单（`WeaselServer/WeaselServer.rc` `:164` / `:258` / `:352`）**保留** ——
用户验收第 2 条认可的取消入口在那里。

---

## 技术方案

### 091-1 用既有 `wParam` 槽位携带「事件种类」，不新增消息类型

**线格式零变化**：`weasel::PipeMessage` 的 `wParam` 字段对每条消息都存在，
`WEASEL_IPC_FOCUS_OUT` 早就在用它，只是**一直传 0**。

| 改动 | 文件 | 内容 |
|---|---|---|
| 标志位 | `include/WeaselIPC.h` | 新增 `WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED = 1u << 0`（与既有 `WEASEL_IPC_FOCUS_IN_FLAG_PROFILE_ACTIVATED` 对称） |
| 客户端 | `include/WeaselIPC.h` / `WeaselIPC/WeaselClientImpl.h` / `.cpp` | `FocusOut(DWORD client_caps = 0)` ⇒ `_SendMessage(WEASEL_IPC_FOCUS_OUT, client_caps, session_id)` |
| 前端 | `WeaselTSF/WeaselTSF.cpp:406` | profile-deactivated 分支改传 `WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED` |
| 服务端 | `RimeWithWeasel/RimeWithWeasel.cpp` | `FocusOut` 按 `param` 分流 |

**为什么不用新增消息类型**：P2 要求「不要凭空发明新消息类型而不更新两端」；
复用既有槽位比新增消息类型**更省**（不动线格式、不动部署端、不动服务端派发表），
且与既有 `FOCUS_IN` 标志位的做法完全同构 ⇒ 读代码的人一眼能认出来。

### 091-2 判定逻辑：纯函数 + 守卫加在调用点

`include/WeaselIPC.h` 新增一个**纯函数**（无副作用、可单测）：

```
ShouldHideQuickPanelOnFocusOut(param, always_show)
  ├─ param 带 PROFILE_DEACTIVATED  ⇒ true  （切输入法 ⇒ 必须收起，不受常驻约束）
  └─ 否则                            ⇒ !always_show （切窗口：常驻态保持可见）
```

`RimeWithWeasel.cpp` 的 `FocusOut` 改成：

```
if (ShouldHideQuickPanelOnFocusOut(param, QuickPanelDialog::AlwaysShowMode()))
  QuickPanelDialog::Hide();
```

**为什么守卫仍在调用点**（沿用 ADR 0016 D1 的理由）：
`Hide()` 同时服务于「用户明确手势」的路径（`Alt+,` 收起 / `ESC` / 点词库按钮）；
把守卫放进 `Hide()` 内部会让这些路径在常驻态下**全部失效**。
**守卫属于「这次收起是不是用户要的」这个判断，不属于「怎么隐藏」这个动作。**

**为什么判定放共享头**：生产代码与测试共用**同一份**口径 ⇒ 不会出现
「代码按 A 判、测试按 B 测」的双事实源（宪法 R7）。

### 091-3 「切到火流猩 ⇒ 显示」沿用既有通路，不改

`FocusIn` + `WEASEL_IPC_FOCUS_IN_FLAG_PROFILE_ACTIVATED`
→ `RimeWithWeasel.cpp:504-513` → `m_onShowQuickPanel()` → 服务端界面线程
`ShowQuickPanel()`（spec 080 / ADR 0008 已实现并被 `/verify-quickpanel` 覆盖）。

本轮**只补「切走」这一半**，不重写「切来」那一半。

### 091-4 语言栏菜单去除「快速面板常驻」项

- `WeaselTSF/WeaselTSF.rc`：删掉 3 条 `MENUITEM ... ID_QUICKPANEL_ALWAYS_SHOW`
  （**英文块 / 繁体块 / 简体块各 1 条**），其余菜单项与顺序**一字不动**。
- 该文件是 **UTF-16LE + BOM**，改写必须保编码、保行尾。
- `include/resource.h` 的 `#define ID_QUICKPANEL_ALWAYS_SHOW 40019` **保留**
  （服务端托盘菜单仍在用同一个编号，两份头必须取值相等），只更新注释说明现状。

### 测试策略

新增 `test/TestWeaselIPC` 的 `/verify-focusout-flags` 模式（**端到端 + 纯判定**两部分）：

| # | 断言 | 牙齿在哪 |
|---|---|---|
| T1 | 标志位存在、非 0、且是**单 bit** | 防止写成 `1u << 1` 之类的多 bit 或 0 |
| T2 | 与 `FOCUS_IN` 的标志位**不重叠** | 防止两个方向撞同一个 bit |
| T3 | `ShouldHideQuickPanelOnFocusOut(FLAG, 常驻态) == true` | **本缺陷的反向断言**：退回旧写法即红 |
| T4 | `ShouldHideQuickPanelOnFocusOut(FLAG, 瞬时态) == true` | 瞬时态行为不变 |
| T5 | `ShouldHideQuickPanelOnFocusOut(0, 常驻态) == false` | **ADR 0016 D1 的反向断言**：无条件隐藏即红 |
| T6 | `ShouldHideQuickPanelOnFocusOut(0, 瞬时态) == true` | 瞬时态行为不变 |
| T7 | **线格式往返**：客户端 `FocusOut(FLAG)` ⇒ mock 服务器收到并落盘 `param` ⇒ 客户端读回断言该 bit 为 1 | 防止「客户端没把标志送上线」或「服务端没透传」 |
| T8 | 再发一次 `FocusOut(0)` ⇒ 落盘 `param == 0` | 证明参数**是透传**的，不是写死 |

T7/T8 复用 `TestWeaselIPC` 既有的「mock 服务器 + 客户端」编排：
mock 的 `TestRequestHandler` 新增 `FocusOut` 重写，把收到的 `param`
写进**与 exe 同目录**的 `focusout_param.txt`（客户端读同一个文件）。
mock 服务器与客户端是**同一个 exe** ⇒ 路径天然一致，不依赖工作目录。

---

## Constitution Check

| 原则 / 规则 | 判定 | 依据 |
|---|---|---|
| **I. Intent Before Implementation** | ✅ | 本 spec §1/§2 写明 What / Why / 验收；用户已给出明确期望语义 |
| **II. Test-Backed Change** | ✅ | 新增 `test/TestWeaselIPC` 的 `/verify-focusout-flags`（8 条断言，含线格式往返） |
| **III. Spec-Artifact Discipline** | ✅ | ≥3 文件 ⇒ 本目录三件套 |
| **IV. Structured Clarification** | ✅ | 无未决项（用户已裁定「菜单」= 语言栏菜单；托盘项保留） |
| **V. Incremental Delivery** | ✅ | 091-1/091-2/091-3 各自可独立编译验证 |
| **R1** 意图 + 验收前置 | ✅ | spec §3 |
| **R2** spec 无技术栈词 | ✅ | spec.md 全程用户语言 |
| **R3** 任务带优先级 + 用户故事标签 | ✅ | tasks.md |
| **R4** plan 含 Constitution Check | ✅ | 本节 |
| **R5** 任务粒度 ≤4h / 1-3 文件 | ✅ | tasks.md |
| **R6** 「完成」须贴测试输出 | ✅ | 交付前贴完整套件输出 |
| **R7** 单一事实源 | ✅ | 判定函数放共享头，生产与测试同源 |
| **R8** 规格进 git | ✅ | 随本轮提交入库 |
| **R9** 查证优先于记忆 | ✅ | 根因链逐环有源码行号取证 |
| **P1** 代码风格 | ✅ | 2 空格 / Chromium（`.clang-format`），改动行走 `git-clang-format` |
| **P2** Windows 约束 | ✅ | 用户可见文案简体中文；**不新增 IPC 消息类型**（复用既有 `wParam` 槽位）；TSF 线程不做阻塞 I/O（通知线程已存在，路径不变） |
| **P3** 构建与依赖 | ✅ | 无新依赖；`librime/` 不动 |
| **P4** 提交与 PR | ✅ | Conventional Commits + 模块前缀；CHANGELOG 补 `[1.1.6]` |
| **P5** UX | ✅ | 菜单项移除属用户可见变更 ⇒ CHANGELOG 条目 |
| **P6** 向后兼容 | ✅ | 用户数据路径不变；`always_show` 配置文件格式不变 |
| **P7** 子进程生命周期 | ✅ | 不新增长驻进程；仍走既有命名管道 |
| **P8** 品牌分支 | ✅ | 见下方必披露项 |

### P 规则逐条要点

- **P2（协议）**：**不新增消息类型**。`WEASEL_IPC_FOCUS_OUT` 的线格式（命令号 / 字段布局）**一字未改**，
  只是把原本恒为 0 的 `wParam` 用起来 ⇒ 前后端协议**向后兼容**（旧客户端发 0 = 旧语义）。
- **P5**：菜单项移除是用户可见变更 ⇒ `CHANGELOG.md` 必须有 `### 主要更新` 条目。
- **P7**：没有新进程；`WeaselServer` 仍是唯一监管者。

### P8 必披露项（三问）

1. **是否触碰旧用户数据路径？** —— **否**。本轮不读写任何用户数据目录，
   `always_show` 的落盘位置与格式**一字未动**；不触发 P6 豁免的任何条款。
2. **是否引入新的 IPC 消息类型？** —— **否**。复用既有 `WEASEL_IPC_FOCUS_OUT` 的
   `wParam` 槽位，新增的是一个**位标志常量**（`WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED`），
   与既有 `WEASEL_IPC_FOCUS_IN_FLAG_PROFILE_ACTIVATED` 同构。
   ⇒ P2 的「两端都更新」要求仍然满足：客户端（`WeaselIPC` + `WeaselTSF`）、
   服务端（`RimeWithWeasel`）、部署端（`WeaselDeployer` 复用同一份 `include/WeaselIPC.h`，
   默认参数保证其现有调用点不受影响）三端一并重编。
3. **CHANGELOG 是否有用户可见条目？** —— **有**：`[1.1.6]` 下记两条
   （① 切到其他输入法时快捷设置栏消失；② 语言栏菜单移除「快速面板常驻」）。

---

## 风险与回滚

| 风险 | 可能性 | 缓解 |
|---|---|---|
| 「切输入法」判定漏发 ⇒ 缺陷复现 | 低 | T7/T8 端到端断言 + 真机 AC-1 |
| 误伤「切窗口保持可见」⇒ ADR 0016 回归 | 中 | T5 反向断言 + 真机 AC-3 |
| 旧客户端（未重编的 `weasel.dll`）与服务端不一致 | 中 | `wParam` 旧值恒 0 ⇒ 旧语义（切窗口）⇒ **安全降级**，不崩 |
| 常驻态下切走输入法 ⇒ 面板消失 ⇒ 用户以为「常驻失效」 | 中 | 切回输入法即重新出现（AC-2）；CHANGELOG 明确写「常驻 = 不因鼠标/切窗口消失」 |
| 菜单项删除后用户找不到取消入口 | 低 | 托盘菜单保留同名项 + `Alt+,`（验收第 2 条已确认这两条有效） |

**回滚**：本轮是一次提交，可整体 `git revert`；已发布版本 1.1.5 的安装包不受影响。

---

## 复杂度跟踪

| 偏差 | 理由 |
|---|---|
| 不新增独立 spec（与菜单移除合并在 091） | 两者同属「v1.1.5 真机验收处置」同一轮，且共享「面板显隐」语境；拆开会重复取证 |
| 判定函数放在 `include/WeaselIPC.h` 而非新建头文件 | 减少文件数与工程配置改动；该头已是协议与焦点语义的事实源 |
| `include/resource.h` 的 `ID_QUICKPANEL_ALWAYS_SHOW` 保留不删 | 服务端托盘菜单仍在用；两份头取值必须相等，删一边会造成不对称 |
| 未处理验收第 6 条（右键屏蔽 / 名单落盘） | 用户已明示「待重启后复验」，属真机复验项而非代码缺陷 |
