# 0032. 自家 UI 进程用「客户端身份」而不是「焦点顺序」与「用户切走输入法」区分

- **Status**: Accepted
- **Date**: 2026-11-03
- **Deciders**: Fluxing maintainers
- **Related**: lessons L171, L172; spec 091, spec 094, spec 097; ADR 0024, ADR 0025, ADR 0027, ADR 0029

## Context

「用户短语」窗口是独立进程 `FluxingPhrasesDialog.exe`(spec 051),它自己加载
`weasel.dll`(TSF tip)。因此它的一次「窗口销毁」会顺着 tip 生命周期发出

```
focus_in(profile=present) -> remove_session(active) -> focus_out(param=1)
```

这与「用户真的切到别的输入法」的事件序列**完全同形**(lessons L171 用真机探针
逐条对过时间线)。两条链路的命令、参数、到达顺序都一样,只有**发起者**不同。

于是「按 Esc 关闭短语窗口 ⇒ 快捷设置栏跟着消失」这个 bug 无法靠顺序判据修:
任何「先来后到」的规则都必然在两条链路上同时生效,修了 bug 就会破坏
spec 091/094 的反向口径(用户真切切走输入法时,快捷设置栏**必须**收起)。

已有的三个动作都只管住了链路的一半:

- ADR 0019 的「ESC 不收起」只在 `QuickPanelDialog::OnKeyDown` 里吞掉**面板自己**
  收到的 `Esc`;管不到「别的窗口收到 Esc → 焦点/profile 事件 → 服务端隐藏面板」。
- ADR 0024/0027 把「切走输入法」与「窗口失焦」用位标志分开,解决了**用户应用**
  内部的歧义,但自家 UI 的 profile 往返仍然带**同一个** `PROFILE_DEACTIVATED` 位。
- ADR 0029 让「切走必隐」的通知必达、同线程定序,保证隐藏**一定生效** ——
  于是自家 UI 的伪「切走」也一定生效,bug 反而更稳。

协议层面只有 `PROFILE_ACTIVATED` / `PROFILE_DEACTIVATED` 两个 profile 位
(spec 091/094),没有第三个态可以塞「这是自家窗口」。

但服务端**已经掌握**每个会话的客户端身份:`StartSession` 时客户端通过
`_WriteClientInfo()` 自报 `session.client_app=<宿主 exe 名,小写>`,服务端在
`_ReadClientInfo()` 里解析并 `set_property(session_id, "client_app", ...)`。
既有先例:`_RefreshTrayIcon` 就是按 `client_app == "explorer.exe"` 特判托盘图标。
也就是说「这条会话来自自家 UI 进程」是**已知事实**,不需要新增协议字段。

## Decision

1. **判据 = IPC 会话的客户端身份**。服务端按 `ipc_id` 记住会话归属
   (`fluxing::IpcClientKind`),归属来自 `session.client_app`。
   自家 UI 进程 = `FluxingPhrasesDialog.exe`(判定大小写不敏感)。
2. **自家 UI 进程的 IME 生命周期事件不得影响快捷设置栏**,三条路径统一口径:
   - `FocusIn(profile)` ⇒ 不请求显示、不改在场状态;
   - `FocusOut` ⇒ 不请求收起、不改在场状态;
   - `RemoveSession(active)` ⇒ 不当作「火流猩离场」(不改在场状态、不收起面板)。

   其余行为(候选窗刷新、活动会话赋值、窗口隐藏)一律保持原样,只抑制
   快捷面板相关副作用。
3. **判定逻辑放共享纯函数** `include/WeaselIPC.h` 的 `namespace fluxing`
   (`ClassifyIpcClient` / `IsOwnUiClient` / `ShouldShowQuickPanelOnFocusInForClient` /
   `ShouldHideQuickPanelOnFocusOutForClient` / `ShouldTreatSessionEndAsFluxingAbsent`),
   生产代码与 `test/TestWeaselIPC` 的 `/verify-own-ui-events` 同源(宪法 R7)。
   既有的 `ShouldShowQuickPanelOnFocusIn` / `ShouldHideQuickPanelOnFocusOut`
   **原样保留**,不做破坏性签名变更。
4. **身份必须活过 `RemoveSession`**。TSF 的 `Deactivate` 路径是「先 `EndSession`
   (→ `RemoveSession`)再发 `FocusOut(PROFILE_DEACTIVATED)`」,FocusOut 到达时
   会话已经销毁。因此身份表**不随会话结束删除**;`ipc_id` 会被回收复用
   (`_GenerateNewWeaselSessionId`),所以 `AddSession` 必须先清掉同 id 的旧身份,
   否则复用后的用户应用会被误判成自家 UI。身份表另设 256 条上限防无界增长。
5. **exe 名走共享常量** `fluxing::OwnUiClientExeName()`,消除第二份字面量
   (`WeaselServer/PhrasesDialogIPC.cpp` 原来硬编码 `FluxingPhrasesDialog.exe`)。
6. **IPC 线格式零变化** ⇒ `WeaselIPC` 客户端、TSF 前端、deployer 端**零改动**;
   判定全部落在服务端。老客户端行为完全不变。

被否决的替代方案:

- **焦点顺序判据**(原修复建议):两条链路事件序列完全同形,顺序规则必然同时
  作用于两者 ⇒ 修 bug 就会破坏 spec 091/094 的反向口径。
- **管道 PID 方案**(`GetNamedPipeClientProcessId`):技术上可行(`PipeServer::Listen`
  对每个连接起一条线程,句柄按连接独占),但要改 `RequestHandler` 虚函数签名 +
  新增 `weasel::Server` 公开 API + 跨模块接线;且 `PipeChannel` 的句柄是
  `static thread_local`,跨连接还要额外映射。收益仅是「不受客户端自报名影响」,
  而该面非安全关键,成本远大于收益。
- **在短语窗口进程里禁用 tip**:CTF 会为文本控件加载当前 tip,禁不掉 ⇒
  短语编辑框将无法输入中文。
- **TSF 端 aux UI 模式**(环境变量 / 命令行标记):需要前端额外识别自家进程并
  上报新命令,跨模块改动更大;而客户端自报身份已足够。

## Consequences

- ✅ 「按 Esc 退出用户短语窗口」不再连带收起快捷设置栏(瞬时态与常驻态都成立)。
- ✅ spec 091/094 的反向口径原样保留:用户真的切到别的输入法,快捷设置栏仍然
  收起 —— `/verify-own-ui-events` 里有独立断言守住这一半。
- ✅ IPC 线格式零变化 ⇒ 客户端 / TSF / deployer 三端零改动,老客户端安全降级。
- ✅ 自家 UI 的 exe 名只有一个字面量(单一真相源,宪法 R7)。
- ⚠️ 身份靠客户端自报的 exe 名 ⇒ 同名 exe 可以伪装。非安全关键面(只影响
  快捷设置栏的显隐),但记在这里以免日后被误当成安全边界。
- ⚠️ 身份表不随会话结束删除 ⇒ 必须有容量上限(256);超限删最老的一条。
  最坏情况是某条身份丢失,退回到「按普通用户应用处理」—— 那是安全方向。
- ⚠️ `ipc_id` 会回收复用 ⇒ `AddSession` 必须先清同 id 的旧身份,否则用户应用
  会被误判成自家 UI(会悄悄破坏反向口径)。
- ⚠️ 本次只抑制快捷面板相关副作用。自家 UI 仍然会抢 `m_active_session`
  (`AddSession` 无条件赋值),这是**另一个**已知遗留问题,本次不修。
- ↩️ 可逆:撤掉会话身份表与三处分支即回到改动前行为;协议侧无遗留。
