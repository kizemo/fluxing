# 0025. 切走输入法的判定表抽成共享纯函数；删词 IPC 回 body；前端按物理键位拦截 Ctrl+Shift+数字

- **状态**: Accepted
- **日期**: 2026-11-01
- **决策者**: Fluxing maintainers
- **相关**: spec 094、ADR 0024、ADR 0023、ADR 0012、lessons L161

## 背景

用户在 1.1.6 装机实测中报了两个缺陷：切到别的输入法后快捷设置栏不消失；
有候选窗时右键候选词或 `Ctrl+Shift+数字` 都删不掉候选词。

排查后确认这不是两处独立的笔误，而是**三类语义缺口**同时暴露：

1. **TSF 激活通知的语义被读错了。**
   `ITfActiveLanguageProfileNotifySink::OnActivated` 投递的 `clsid` 是
   「**新的上位者是谁**」，不是「自己还在不在」。用户切到别的输入法时，
   本进程收到的是 `(别的输入法 clsid, isActivated=TRUE)`。
   旧代码首行就 `if (!IsEqualCLSID(clsid, c_clsidTextService)) return S_OK;`
   ⇒ 「切走」信号被整条丢弃。加上 `Deactivate()` 先 `EndSession()`、
   通知循环里又有 `HasSession()` 门禁，三处叠加导致快捷设置栏永远收不起来。

2. **UI 回调按值存进成员 ⇒ 回调恒为空。**
   `WeaselPanel::m_deleteCallback` 是**值** `std::function<void(size_t)>`，
   构造时没赋值 ⇒ 空 ⇒ `OnRButtonDown` 里的 `if (m_deleteCallback)` 永假。
   右键删词从未真正工作过。

3. **「产生的字符」不足以判定物理按键。**
   `ConvertKeyEvent` 产出的是字符（`Ctrl+Shift+1` → `!`），无法反推数字键；
   而 `Ctrl+Shift+3/4` 在 Rime 默认配置里已绑定 `ascii_punct` /
   `traditionalization`，抢占需要有明确边界。

同时还有一个**时序缺口**：服务端 DELETE 命令不回 body，客户端删词后读到
的仍是上一次的管道缓冲，候选窗要等下一次按键才刷新 —— 用户看到的是
「点了没反应」。

## 决策

**我们把「切走」的判定抽成共享纯函数 `DecideProfileNotify`，放在
`include/WeaselIPC.h`（与 `ShouldHideQuickPanelOnFocusOut` 并列）。**

```
自身 clsid + activated   → kNotifyActivated
自身 clsid + deactivated → kNotifyDeactivated
别的 clsid + activated   → kNotifyDeactivated   // 用户切走了
别的 clsid + deactivated → kIgnore
```

生产代码（`WeaselTSF::OnActivated`）与测试（`TestWeaselIPC` 的
`/verify-delete-body`）引用**同一份实现**（宪法 R7）。修法采用「两种都覆盖」：
`Deactivate()` 在 `EndSession()` **之前**主动投递 Deactivated，通知循环里
去掉 `HasSession()` 门禁，`OnActivated` 改用判定表。**服务端不变，IPC 线格式零变化。**

**我们让删词命令回 body。** `RequestHandler::DeleteCandidateOnCurrentPage`
增加 `EatLine eat` 形参（默认 `0`，向后兼容），`WeaselServerImpl` 建 `eat`
lambda 把它接到管道，`RimeWithWeaselHandler` 在成功路径 `_Respond(ipc_id, eat)`。
客户端 `WeaselTSF::_DeleteCandidateOnCurrentPage` 发命令后立刻
`GetResponseData` + `_UpdateUI` 刷新候选窗。**命令号 / wParam / lParam 零变化。**

**我们在前端按物理键位拦截 `Ctrl+Shift+<1..9>`，判定逻辑抽成共享纯函数。**
新增 `include/FluxingCandidateDeleteShortcut.h`（header-only，只依赖
`windows.h` + STL）：`MatchCandidateDeleteShortcut(vkey, ctrl_down,
shift_down, key_down)` 只看 **VK 码 + `GetKeyboardState` 的 Ctrl/Shift 位 +
lParam bit31**，不看 `ConvertKeyEvent` 产出的字符；
`ShouldInterceptCandidateDelete(number, visible_count)` 保证**只在候选窗可见
且序号落在当前可见页范围内时**才拦截，让位给 Rime 默认的
`Ctrl+Shift+3/4`。命中后复用既有的
`WEASEL_IPC_DELETE_CANDIDATE_ON_CURRENT_PAGE` 命令，不新增命令。

**我们把 `m_deleteCallback` 改成引用绑定**，与同文件已生效的 `_UICallback`
同构：`std::function<void(size_t)>& m_deleteCallback;`，构造时绑
`ui.deleteCandidateCallback()`。这样不依赖 setter 与面板创建的先后顺序。

**我们让删词组合里的 Shift 不原样转发。** 前端在 Shift 分支里当 Ctrl 按住时
吞掉 Shift down（记 `_shiftSuppressed`），配对的 up 也吞；服务端 DELETE 处置
另把该会话的 `interveningKey` 置 true 作兜底。否则服务端 Shift 状态机会把它
当成「选词 Shift」，在菜单打开时 `commit_composition` 把候选上屏。

**deployer 端不受影响**：`WeaselDeployer` 没有 `RequestHandler` 实现
（`grep "RequestHandler|DeleteCandidate" WeaselDeployer/` 无匹配），本次
也不改 IPC 线格式，故无需同步 deployer 改动。

### 被否决的替代方案

- **只改服务端，让服务端自己判断「切走」**：服务端无法区分「切走到别的输入法」
  与「编辑框失焦」——`FocusOut` 的 `wParam` 位标志是 ADR 0024 已定的协议，
  换判据要动协议，得不偿失。
- **用 `ProcessKeyEvent(0)` 空按键强制刷新候选窗**（Option X）：能刷新，
  但绕过了服务端「删词后回 body」的语义，且会在引擎里留下一次无意义的事件。
  选 Option Y（回 body）。
- **前端把 `Ctrl+Shift+数字` 转成字符再发给引擎**：与 ADR 0009/0011
  「前端发字符不发键码」的契约冲突，且字符 `!` 无法反推数字键。
- **新增一条独立的删词 IPC 命令**：命令号与字段布局会变动，收益为零
  ——现有 DELETE 命令的语义完全够用，只是没回 body。

## 后果

- ✅ 快捷设置栏的显隐判定与「切走」的语义**只有一处真相**（`DecideProfileNotify`），
  前端与测试不会漂移。
- ✅ 删词链路（右键 / `Ctrl+Shift+数字`）共用同一条 IPC 与同一套
  「可见位置 → 引擎位置」换算（ADR 0023 D5）。
- ✅ 删词后候选窗**立即**刷新，用户不再看到「点了没反应」。
- ✅ IPC 线格式零变化 ⇒ 老客户端 / 老服务端仍可互通。
- ⚠️ `Ctrl+Shift+3/4` 在**候选窗可见**时不再触发 Rime 的
  `ascii_punct` / `traditionalization`。这是有意取舍：候选窗可见时用户更可能
  想删词。边界由 `ShouldInterceptCandidateDelete` 收紧 —— 候选窗不可见、或序号
  超出可见页范围时一律不拦截。
- ⚠️ `RequestHandler::DeleteCandidateOnCurrentPage` 的虚函数签名变了
  （多了 `EatLine eat = 0`）。默认值保持向后兼容，但**下游若有自定义
  `RequestHandler` 实现，必须同步**。这是 IPC 铁律要求一次性提交 server /
  TSF / deployer 三端的原因。
- ⚠️ TSF 到底投递 `(别的 clsid, TRUE)` 还是 `(自己 clsid, FALSE)`，
  取决于系统与前台进程。本决策**两种都覆盖**，因此不依赖该事实成立；
  判定表本身由共享纯函数逐条断言，真机行为列人工验证步骤。
- ↩️ 本决策**可逆**：`DecideProfileNotify` 是纯函数，改判定表即可；
  `EatLine` 形参带默认值，改回不需要同步下游。
