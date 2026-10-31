# 0024. 快捷设置栏的显隐跟随输入法激活状态（并从语言栏菜单移除「快速面板常驻」）

- **状态**: Proposed
- **日期**: 2026-10-31
- **关联**: spec 091（任务总账本轮条目）
- **相关**: ADR 0008 / spec 080（切到火流猩时弹面板）、
  ADR 0013 / spec 084（常驻模式）、
  ADR 0016（FocusOut 焦点守卫 —— 本条是它**漏掉的一条分支**）、
  ADR 0019（常驻勾选态持久化）、
  ADR 0022（默认常驻 + 扁平 idle alpha）

## 背景（Context）

v1.1.5 装机后的真机验收（2026-10-30，用户未重启状态下）带回一条**新缺陷**：

> **从火流猩输入法切换到其他输入法，快捷设置栏仍然显示。**
> 正确的应该和其他输入法逻辑相同：切换到火流猩输入法，显示快捷设置栏；
> 切换到其他输入法，设置栏不显示。

同时，用户对验收第 3 条（「语言栏勾选态仍不回显」）**改口**：
不再要求补回显，改为**从菜单里去掉「快捷面板常驻」这一项**。

### 根因（逐环取证）

1. `AlwaysShowMode() == true` 时，面板内部两条自动隐藏路径
   （`QuickPanelDialog.cpp:686` 轮询超时、`:744` 鼠标移出）都被 `!AlwaysShowMode()` 挡住
   ⇒ 常驻态下面板**没有任何隐藏路径**。
2. 「用户切到别的输入法」这个事件**确实到达了服务端**：
   `WeaselTSF/WeaselTSF.cpp:414-446` 的 `ITfActiveLanguageProfileNotifySink::OnActivated`
   在 `isActivated == false` 时 ⇒ `_PostProfileNotify(kProfileNotifyDeactivated)`
   ⇒ 通知线程 `m_client.FocusOut()`（`:404-407`）。
3. 但 `FocusOut` 的线格式里 `wParam` **恒为 0**
   （`WeaselClientImpl.cpp:178-180`）⇒ 服务端**分不出**用户是在
   「切输入法」还是「切窗口」。
4. 服务端 `RimeWithWeasel/RimeWithWeasel.cpp:558` 只有一条一刀切：
   `if (!QuickPanelDialog::AlwaysShowMode()) QuickPanelDialog::Hide();`
   ⇒ 常驻态下**两条都不隐藏** ⇒ 切输入法后面板残留。

**这不是「面板忘了隐藏」，而是「该隐藏」这个信号从未送到。**

### 这一刀是怎么来的

ADR 0016 D1/D2 明确写了「守卫加在调用点，不动 `Hide()` 本体」「只对『输入焦点离开』这一条路径设防」。
**它当时的意图是对的**（用户勾了「常驻」就不该因为切窗口而消失），
**但它无法区分两种事件**（因为线格式不带信息），于是把「切输入法必须消失」一并挡掉了。
`RimeWithWeasel.cpp:542-546` 的注释至今还留着当年的原始意图：
「**当用户切换到 en-US IME 时** … 这里 `Hide()` panel … 这是最干净的 IME switch path」。

⇒ 教训：**「总开关」按发起点逐条列**（ADR 0016 自己的结论）在本轮再次成立 ——
这次漏的不是「另一条隐藏路径」，而是「另一条隐藏路径的**触发条件**」。

## 决策（Decision）

- **D1 —— 复用既有 `FocusOut` 的 `wParam` 槽位，不新增 IPC 消息类型。**
  `weasel::PipeMessage` 的 `wParam` 字段对每条消息都存在，`WEASEL_IPC_FOCUS_OUT`
  早就在用它，只是**一直传 0**。本轮新增位标志
  `WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED = 1u << 0`，
  与既有的 `WEASEL_IPC_FOCUS_IN_FLAG_PROFILE_ACTIVATED`（spec 080 / ADR 0008）**完全同构**。
  理由：P2 要求「不要凭空发明新消息类型而不更新两端」；
  复用既有槽位**线格式零变化**，且旧客户端发 0 = 旧语义 ⇒ **向后兼容、安全降级**。
  服务端派发本就透传 `wParam`（`WeaselServerImpl.cpp:277`）⇒ 服务端**零协议改动**。

- **D2 —— 判定逻辑放共享头，生产代码与测试共用同一份口径。**
  `include/WeaselIPC.h` 新增纯函数
  `ShouldHideQuickPanelOnFocusOut(DWORD param, bool always_show)`：
  - 带 `PROFILE_DEACTIVATED` ⇒ `true`（**切输入法 ⇒ 必须收起，不受常驻约束**）
  - 否则 ⇒ `!always_show`（切窗口：常驻态保持可见，瞬时态收起）
  理由：宪法 R7（一个概念一个事实源）——
  「代码按 A 判、测试按 B 测」的双事实源会让这个缺陷第二次复现。

- **D3 —— 守卫仍然加在调用点，`Hide()` 本体不动。**
  延续 ADR 0016 D1 的理由：`Hide()` 同时服务于「需要用户明确手势」的路径
  （`Alt+,` 收起 / `ESC` / 点词库按钮），把守卫放进 `Hide()` 内部会让这些路径在常驻态下**全部失效**。
  **守卫属于「这次收起是不是用户要的」这个判断，不属于「怎么隐藏」这个动作。**

- **D4 —— 「切到火流猩 ⇒ 显示」沿用既有通路，不重写。**
  `FocusIn` + `WEASEL_IPC_FOCUS_IN_FLAG_PROFILE_ACTIVATED`
  → `RimeWithWeasel.cpp:504-513` → `m_onShowQuickPanel()` → 服务端界面线程 `ShowQuickPanel()`。
  本轮**只补「切走」这一半**，不碰「切来」那一半。

- **D5 —— 「输入焦点离开」那条路径的 `wParam` 保持 0，一个字节都不改。**
  `WeaselTSF/KeyEventSink.cpp:91`（`OnSetFocus(fForeground=false)`）与
  `WeaselTSF.cpp:464`（重连失败）继续发**无标志**的 `FocusOut`。
  理由：这两条是 ADR 0016 D1 明确保护过的行为，任何改动都是回归。

- **D6 —— 语言栏弹出菜单移除「快速面板常驻」项；托盘菜单保留。**
  `WeaselTSF/WeaselTSF.rc` 的三个语言块各删 1 条 `MENUITEM`；
  `WeaselServer/WeaselServer.rc` 的三处托盘菜单项**保留** ——
  那是用户验收第 2 条明确认可的取消入口。
  `include/resource.h` 的 `ID_QUICKPANEL_ALWAYS_SHOW 40019` **保留**
  （两份头取值必须相等），只更新注释。

- **D7 —— 「常驻」的含义不变，但补一句口径。**
  常驻 = 面板**不因鼠标移开 / 切窗口 / 点击**而消失；
  「切到别的输入法」属于**输入法本身不在场**，不属于常驻要保护的范围。
  该口径写进 `CHANGELOG.md`，避免用户把「切输入法即消失」误读成「常驻失效」。

## 后果（Consequences）

- **修好了什么。** 常驻态下切到其他输入法 ⇒ 快捷设置栏消失；切回火流猩 ⇒ 重新出现。
  瞬时态下行为**完全不变**。常驻态下「切窗口仍在」「鼠标移开仍在」**仍然成立**。
- **用户可能察觉到的变化。** 托盘菜单里那个项还在；语言栏弹出菜单里**少了一项**。
  切输入法时面板会消失（这是用户要的）。
- **刻意不修的东西。** 托盘菜单勾选态回显（本来就正常）；
  验收第 6 条（右键屏蔽 / 名单落盘 / 托盘撤销）—— 用户已明示「待重启后复验」；
  `AlwaysShowMode()` 的**持久化语义**（ADR 0019）不动。
- **本条自身变更的风险。** 小而有限：一个标志位 + 一个纯函数 + 四个调用点；
  线格式不变；`Hide()` 本体不动；输入引擎未动（宪法 P3）。
- **本条引入的风险。** 若某台机器上前端与服务端版本不一致（前端旧、服务端新），
  旧前端发 `wParam=0` ⇒ 服务端按「切窗口」处理 ⇒ **缺陷复现但不崩**（安全降级）。
  装机时前端与服务端是同一个安装包同时替换 ⇒ 不会出现混版。

## 证据（Evidence）

- 根因链逐环的行号取证：`WeaselTSF/WeaselTSF.cpp:404-407 / :414-446`、
  `WeaselIPC/WeaselClientImpl.cpp:178-180`、
  `WeaselIPCServer/WeaselServerImpl.cpp:272-279`、
  `RimeWithWeasel/RimeWithWeasel.cpp:531-559`、
  `WeaselServer/QuickPanelDialog.cpp:686 / :744`。
- 「切输入法」与「切窗口」两条路径的**唯一**区别就是 `wParam` 的那个 bit ——
  两条都发 `WEASEL_IPC_FOCUS_OUT`，除此以外**没有任何**可区分信息（这是本条的立论基础）。
- 协议侧：`include/WeaselIPC.h:52-54` 已有 `WEASEL_IPC_FOCUS_IN_FLAG_PROFILE_ACTIVATED`
  的**同构先例**；`WEASEL_IPC_FOCUS_OUT` 的命令号与字段布局**未改**。
- 菜单侧：`WeaselTSF/WeaselTSF.rc:72 / :97 / :122`（三个语言块各一条）vs
  `WeaselServer/WeaselServer.rc:164 / :258 / :352`（托盘三块各一条）。
- 测试侧：`test/TestWeaselIPC` 新增 `/verify-focusout-flags`，含
  **线格式往返断言**（客户端发出的 `wParam` 被 mock 服务器落盘并读回比对）
  与 **双向反向断言**（T3 挡住退回旧写法，T5 挡住无条件隐藏）。

## 未决问题（Open questions）

- **Q-091-1**：切到**繁体/简体**两个不同的 TSF 活动配置时，
  `OnActivated` 会先 `false` 再 `true` ⇒ 面板可能**闪一下**。
  本轮不额外处理（闪一下比残留更符合直觉）；若用户反馈刺眼，下轮用「同一 TIP 内的
  配置切换不触发隐藏」来收窄。
- **Q-091-2**：多进程场景下，`OnActivated` 是**按进程**订阅的 ⇒ 切走时前台与后台进程
  都会发 `FocusOut` ⇒ 面板会被隐藏多次（幂等，无害）。是否需要按 `m_active_session`
  再收窄一层，留待真机观察。
- **Q-091-3**：验收第 6 条（右键屏蔽 → 名单落盘 → 托盘撤销）**待重启后复验**，
  本条不涉及。
