# 0027. FocusIn / FocusOut 的位标志收敛成一张对称协议表

- **状态**: Accepted
- **日期**: 2026-11-01
- **决策者**: Fluxing maintainers
- **相关**: spec 096、ADR 0008、ADR 0024、ADR 0025

## 背景

交接卡 §6.3 的开发要求 ② 是：**补 FocusIn/FocusOut 对称位标志协议表**。

接手时两侧的位标志是**分别长出来**的：

- `WEASEL_IPC_FOCUS_IN_FLAG_PROFILE_ACTIVATED = 1u << 0` —— ADR 0008（spec 080）加的，
  语义是「用户刚把活动输入配置切到火流猩」；
- `WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED = 1u << 0` —— ADR 0024（spec 091）加的，
  语义是「用户刚把活动输入配置切离火流猩」。

两个常量**值相同、语义对称、但写在两个独立的匿名 enum 里**，彼此之间没有任何
机器可验证的约束。加上 spec 094（ADR 0025）引入的 `DecideProfileNotify`
共享纯函数之后，`include/WeaselIPC.h` 顶部已经有三段「各说各话」的判定逻辑：

```
ShouldHideQuickPanelOnFocusOut(param, always_show)   // FocusOut 侧
DecideProfileNotify(is_self_clsid, is_activated)      // TSF 侧
（FocusIn 侧散在 RimeWithWeaselHandler::FocusIn 里）  // 没有共享口径
```

三条后果：

1. **FocusIn 侧没有共享判定函数**。`RimeWithWeaselHandler::FocusIn` 里直接写
   `if (client_caps & WEASEL_IPC_FOCUS_IN_FLAG_PROFILE_ACTIVATED)`，
   而 FocusOut 侧有 `ShouldHideQuickPanelOnFocusOut`。两侧不对称 ⇒
   未来加语义时极容易只改一侧。
2. **「窗口获得焦点」在 FocusIn 侧不可表达**。普通窗口焦点事件发的是
   `wParam = 0`（`WeaselTSF::OnSetFocus`），而 FocusOut 侧从 spec 091 起
   就有 `PROFILE_DEACTIVATED` 位用来区分「切走输入法」与「应用失焦」。
   FocusIn 侧想区分「切到火流猩」与「窗口获得焦点」，**只能靠「有没有 bit0」
   反推** —— 没有正向的位标志。
3. **对称性只能靠人眼保证**。没有任何测试断言「同一 bit 编号在两侧代表同一语义维度」。
   有人给 FocusOut 加了 `1u << 2` 而 FocusIn 没加，或者两侧 `1u << 1`
   含义不同，测试全绿、真机行为漂移。

## 决策

**我们把两侧的位标志收敛成一张共享协议表，放在 `include/WeaselIPC.h`
（全局作用域，与 `ShouldHideQuickPanelOnFocusOut` / `DecideProfileNotify` 并列）。**

```cpp
struct FocusFlagSlot {
  DWORD in_bit;          // FocusIn 侧位掩码
  DWORD out_bit;         // FocusOut 侧位掩码(与 in_bit 同 bit 编号)
  const char* dimension; // 语义维度名(两侧共用)
};
inline const FocusFlagSlot* FocusFlagProtocolTable(size_t& count);
```

表当前两行：

| bit | 维度 | FocusIn | FocusOut |
| --- | --- | --- | --- |
| 0 | `ime_profile_activation` | `WEASEL_IPC_FOCUS_IN_FLAG_PROFILE_ACTIVATED` | `WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED` |
| 1 | `app_focus` | `WEASEL_IPC_FOCUS_IN_FLAG_APP_FOCUS_GAINED` | `WEASEL_IPC_FOCUS_OUT_FLAG_APP_FOCUS_LOST` |

**我们规定：同一 bit 编号在两侧代表同一语义维度。** 新增维度只能**加行**，
不允许某一侧单独加位；表的 `in_bit` 与 `out_bit` 必须相等，对称性由
`TestWeaselIPC` 的 `/verify-focusin-flags` 逐行断言。

**我们补齐 FocusIn 侧的正向位标志。** `WEASEL_IPC_FOCUS_IN_FLAG_APP_FOCUS_GAINED = 1u << 1`
与 `WEASEL_IPC_FOCUS_OUT_FLAG_APP_FOCUS_LOST = 1u << 1` 成对加入。
普通窗口焦点事件（`WeaselTSF::OnSetFocus`）现在带
`APP_FOCUS_GAINED` / `APP_FOCUS_LOST`，不再发 `wParam = 0`。

**我们把两侧的判定收敛成对称的分类器。**

```cpp
enum class FocusInKind  { kProfileActivated, kAppFocusGained,  kUnknown };
enum class FocusOutKind { kProfileDeactivated, kAppFocusLost, kUnknown };
inline FocusInKind  ClassifyFocusIn(DWORD param);
inline FocusOutKind ClassifyFocusOut(DWORD param);
inline bool ShouldShowQuickPanelOnFocusIn(DWORD param);  // 与 ShouldHideQuickPanelOnFocusOut 对称
inline bool HasFocusInFlag(DWORD param, DWORD flag);
inline bool HasFocusOutFlag(DWORD param, DWORD flag);
```

`RimeWithWeaselHandler::FocusIn` 改用 `ShouldShowQuickPanelOnFocusIn(client_caps)`
（与 FocusOut 侧 `ShouldHideQuickPanelOnFocusOut(param, ...)` 同构），
不再自己写位与运算。**分类器对「同时带 profile 位与 app-focus 位」取 profile 优先**，
因为「用户切到火流猩」是更具体、更强的信号。

**我们不动 bit0 的取值，线格式零变化。** 命令号、`wParam` / `lParam` 字段布局、
pipe 消息结构全部不变。旧客户端发 0 → `ClassifyFocusIn` 返回 `kUnknown` →
走与今天完全相同的普通分支；旧服务端只认 bit0，收到新客户端发的 bit1 仍是旧语义
⇒ **双向安全降级**。

**deployer 端豁免。** `WeaselDeployer` 没有 `RequestHandler` 实现
（`grep "RequestHandler" WeaselDeployer/` 无匹配），它不消费 FocusIn/FocusOut，
故本次无需同步 deployer 改动（与 ADR 0025 D 段的处理一致）。

### 被否决的替代方案

- **把 FocusIn/FocusOut 合并成一条消息**：线格式变化，破坏与老服务端互通的性质。
- **给 FocusIn 单独加一张表**：两张表仍然可能漂移；本决策的要点就是**一张**表。
- **用 `std::map<DWORD, const char*>` 而不是静态数组**：引入动态分配与
  头文件里的全局构造顺序问题，收益为零。
- **让普通窗口焦点事件继续发 0**：FocusIn 侧就没有正向位标志，
  「窗口获得焦点」只能靠「有没有 bit0」反推 —— 正是本决策要消除的耦合。

## 后果

- ✅ 两侧的位标志**只有一处真相**（`FocusFlagProtocolTable`），
  新增语义维度必须加行 ⇒ 结构性防止「只改一侧」。
- ✅ FocusIn / FocusOut 有了**对称的判定口径**
  （`ShouldShowQuickPanelOnFocusIn` ↔ `ShouldHideQuickPanelOnFocusOut`），
  未来加语义时照抄对面即可。
- ✅ 「窗口获得焦点 / 应用失焦」有了**正向位标志**，
  不再靠「有没有 bit0」反推。
- ✅ IPC **线格式零变化** ⇒ 老客户端 / 老服务端仍可互通（双向安全降级）。
- ✅ 对称性、取值、分类器优先级全部由 `TestWeaselIPC` 的
  `/verify-focusin-flags` **逐条断言**，且与生产代码同源（宪法 R7）。
- ⚠️ 普通窗口焦点事件的 `wParam` 从 0 变成 2。**这是取值变化，不是线格式变化**；
  旧服务端忽略未知位 ⇒ 行为不变。若将来服务端要求「普通焦点事件必须带 bit1」，
  旧客户端发 0 会被判为 `kUnknown`，仍走普通分支 ⇒ 不会退化。
- ⚠️ `WeaselTSF::_Reconnect()` 里的 `m_client.FocusIn()` / `FocusOut()`
  **刻意保持 0**：那是「客户端刚连上 / 没拿到响应」的连接事件，
  不是焦点事件，不属于协议表里的任何维度。
- ↩️ 本决策**可逆**：协议表是纯头文件常量 + 纯函数，改回 `wParam = 0`
  与恢复 `client_caps &` 判定即可，线格式不受影响。
