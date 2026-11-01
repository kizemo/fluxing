# 094 · Spec · 快捷设置栏切走不消失 + 候选词无法从词典删除（装机实测缺陷修复）

- **状态**：实施中（2026-11-01）
- **来源**：用户装机实测（1.1.6，spec 093 交付后）
- **关联**：ADR 0024、ADR 0023、ADR 0025、ADR 0012、lessons L161

## 1. 问题陈述

用户在 1.1.6 装机实测中发现两个独立缺陷：

1. **快捷设置栏（QuickPanel）在切到其它输入法后不消失**。面板留在屏幕上，
   覆盖输入区域。
2. **有候选窗时无法把候选词从词典里去掉**。右键候选词、或 `Ctrl+Shift+数字`
   都不生效。

两个缺陷的共同表象都是「用户操作了，界面没反应」。

## 2. 根因（已坐实）

### 2.1 缺陷 A：切走后快捷设置栏不消失

三处叠加，任何一处单独修都不够：

| # | 位置 | 问题 |
|---|---|---|
| A1 | `WeaselTSF::OnActivated` | 首行 `if (!IsEqualCLSID(clsid, c_clsidTextService)) return S_OK;` 只认自身 CLSID。而 TSF 投递 `OnActivated` 的语义是「**新的上位者是谁**」：用户切到别的输入法时，本进程收到的是 `(别的输入法 clsid, isActivated=TRUE)`，于是「切走」信号被整条丢弃。 |
| A2 | `WeaselTSF::Deactivate` | 第一件事就是 `m_client.EndSession()`（session_id → 0），随后任何带会话号的 IPC 都发不出去。 |
| A3 | `WeaselTSF::_ProfileNotifyLoop` | Deactivated 分支带 `if (m_client.HasSession())` 门禁，而 A2 已经把 session 清零 ⇒ 门禁恒假 ⇒ 通知必丢。 |

服务端链路本身是好的：`ClientImpl::FocusOut` 没有 `_Active()` 门禁，服务端
`OnFocusOut` 也不校验 session_id，直接透传 `wParam`。

> **已排除的假设：装机载荷陈旧。** `output/weasel.dll` 与
> `D:\Program Files\fluxing\weasel\weasel.dll` 的 SHA256 完全一致
> （`D9328D3A…F9FD0A13`），产物 mtime 晚于源码修改 ⇒ 是代码缺陷，不是打包缺陷。

### 2.2 缺陷 B：候选词删不掉

| # | 位置 | 问题 |
|---|---|---|
| B1 | `WeaselUI/WeaselPanel.h` | `m_deleteCallback` 声明为**值** `std::function<void(size_t)>`，构造初始化列表只写了 `_UICallback(...)`，值成员默认构造为空 ⇒ `OnRButtonDown` 里的 `if (m_deleteCallback)` 永假 ⇒ **右键删词整条链路从未生效**。同文件的 `_UICallback` 是**引用**且工作正常。 |
| B2 | `WeaselTSF/KeyEventSink.cpp` | 没有「Ctrl+Shift+数字」的拦截。`ConvertKeyEvent` 产出的是「产生的字符」（Ctrl+Shift+1 → `!`），无法反推数字键 ⇒ 必须用 `wParam` 的 VK 码 + `GetKeyboardState` 的 Ctrl/Shift 位。 |
| B3 | `WeaselTSF/CandidateList.cpp` | `_DeleteCandidateOnCurrentPage` 发完命令什么都不做。服务端 DELETE 不回 body ⇒ 客户端读到的是上一次的管道缓冲 ⇒ 候选窗要等下一次按键才刷新。 |

## 3. 功能需求

- **FR-101**：「切走到别的输入法」必须被识别，并收起快捷设置栏（**即便勾了「常驻」**）。
- **FR-102**：`Ctrl+Shift+<1..9>` 在**候选窗可见**时删除当前可见页的第 N 个候选；
  候选窗不可见、或序号超出可见页范围时，**不得**拦截（让位给 Rime 默认的
  `Ctrl+Shift+3` / `Ctrl+Shift+4` 绑定）。
- **FR-103**：右键候选词能删除该候选词（可见位置 → 引擎位置的换算沿用 ADR 0023 D5）。
- **FR-104**：删除成功后，候选窗**立即**刷新（不等下一次按键）。
- **FR-105**：`Ctrl+Shift+数字` 里的 Shift 不得原样转发给服务端（否则会被当成
  「选词 Shift」，在菜单打开时把候选上屏）。

## 4. 验收标准

1. 勾选「常驻」后切到别的输入法 ⇒ 快捷设置栏消失。
2. 候选窗可见时 `Ctrl+Shift+1` ⇒ 第 1 个候选消失且候选窗立即刷新。
3. 候选窗可见时右键候选词 ⇒ 该候选消失。
4. 候选窗不可见时 `Ctrl+Shift+3` ⇒ 仍然触发 Rime 的 `ascii_punct`（不被抢）。
5. 回归套件 `scripts/test-infra/run-test-suite.bat` 全绿。

## 5. 非目标

- 不改「常驻」开关的产品形态（收进统一设置面板是 spec 093 交接卡 §6 的下一阶段）。
- 不改 `FocusIn/FocusOut` 的对称位标志协议表（同样属交接卡 §6）。
- 不动 `librime/` 子模块。

## 6. 影响面

| 文件 | 改动 |
|---|---|
| `include/WeaselIPC.h` | 新增 `ProfileNotifyDecision` + `DecideProfileNotify`；`RequestHandler::DeleteCandidateOnCurrentPage` 增加 `EatLine eat` 形参 |
| `include/RimeWithWeasel.h` | override 签名同步 |
| `include/FluxingCandidateDeleteShortcut.h` | **新建**：删词快捷键判定纯函数（header-only） |
| `WeaselIPCServer/WeaselServerImpl.cpp` | DELETE 传 `eat` |
| `RimeWithWeasel/RimeWithWeasel.cpp` | DELETE 回 body + 标记 `interveningKey` |
| `WeaselTSF/WeaselTSF.{h,cpp}` | `Deactivate` 提前投递 + 去 `HasSession` 门禁 + `OnActivated` 判定表 + `_shiftSuppressed` |
| `WeaselTSF/KeyEventSink.cpp` | 键盘状态判定 + Shift 抑制 + 数字键拦截 |
| `WeaselTSF/CandidateList.{h,cpp}` | `GetVisibleCandidateCount` + 删词后立即刷新 + 缩进整理 |
| `WeaselUI/WeaselPanel.{h,cpp}` | `m_deleteCallback` 改引用绑定 + 访问器 |
| `test/TestCandidateRButtonDown/*` | T7/T8/T9 判别性断言 |
| `test/TestWeaselIPC/*` | `DecideProfileNotify` 判定表 + `/verify-delete-body` 模式 |
| `scripts/test-infra/run-test-suite.bat` | 接线 `/verify-delete-body` |

## 7. 风险与取舍

- **Ctrl+Shift+3/4 与 Rime 默认绑定冲突**：只在候选窗可见且序号在范围内时拦截，
  属已知取舍，记在 ADR 0025。
- **`EatLine` 形参变更**：`RequestHandler` 虚函数签名变化 ⇒ 按 CLAUDE.md §2
  的 IPC 铁律走 ADR + server/TSF/deployer 三端 + 测试一次性提交。
  **deployer 端不受影响**：`WeaselDeployer` 没有 `RequestHandler` 实现
  （`grep "RequestHandler|DeleteCandidate" WeaselDeployer/` 无匹配），
  且 IPC **线格式（命令号 / wParam / lParam）零变化**。
- **TSF 层真机行为缺自动化证据**：TSF 投递 `(别的 clsid, TRUE)` 还是
  `(自己 clsid, FALSE)` 取决于系统与前台进程。修法采用「两种都覆盖」，
  判定表本身由共享纯函数逐条断言；真机行为列人工验证步骤。
