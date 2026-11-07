# 0040. 快捷设置栏的全/半角状态源统一为 `ascii_punct`，并由中英切换联动标点宽度

- **Status**: Accepted
- **Date**: 2026-11-07
- **Deciders**: Fluxing maintainers
- **Related**: spec 099（批次 H）、ADR 0010、ADR 0012、ADR 0036、lessons L197

## Context

用户对快捷设置栏提出三条相互纠缠的要求：

1. 「全/半角」图标糊成一块、不可辨；
2. 面板切到半角后，输入的**仍然是中文标点**（用户要的「半角」= 英文标点）；
3. 用 Shift 切中英文时，标点不随动、面板的全/半角图标与「中/英」标志也不随动。

追根因发现三件事：

1. **面板的状态源与用户心里的「全/半角」不是同一个量**。Rime 里 `full_shape`、`ascii_mode`、
   `ascii_punct` 是三把独立开关。`full_shape` 只决定标点转换的目标宽度；真正让 punctuator
   整个 `kNoop`（标点原样直出 ASCII）的是 `ascii_punct`
   （`librime/src/rime/gear/punctuator.cc:102-104`）。用户说的「半角 = 英文标点」对应的是
   `ascii_punct`，不是 `full_shape`。
2. **标点宽度与中英状态正交**：切 `ascii_mode` 不会改标点，所以需求③必须显式联动。
3. **「英文态打出 ASCII 标点」是天然现象**（`ascii_composer` 在 `ascii_mode` 下 `kRejected` 直出），
   所以验收必须用**中文态的 `Ctrl+.` 隔离实验**；否则「切到英文后打出 `,`」不能作为
   `ascii_punct` 生效的证据（lessons L197）。

另有两条硬约束：显示路径（100 ms 轮询的 state provider）**禁止 I/O 与副作用**
（宪法 P2 / spec 099）；裸 Shift 的作用域限当前会话（ADR 0010 / 0012）。

## Decision

1. **统一模型**：`ascii_punct = true` ⇔「半角 / 英文标点」；`full_shape = !ascii_punct`；
   面板显示口径 `fullwidth = !ascii_mode && !ascii_punct`，下沉为纯函数
   `fluxing::panel_status::ShapeIsFullwidth(ascii_mode, ascii_punct)`
   （`include/FluxingPanelStatus.h`），生产代码与单测同源。
2. **写入侧**：服务端新增 `RimeWithWeaselHandler::SetPunctuationWidth(bool halfwidth, WeaselSessionId)`，
   由设置栏「全/半角」回调调用（不再直接 `SetOption("full_shape", …)`）。内部
   `_ResolveTargetSession` 决定作用会话（当前会话优先，回退「最近焦点会话」），
   `_ApplyPunctWidth` 同向写 `ascii_punct` 与 `full_shape`。
3. **联动侧**：裸 Shift 的 `kToggleAsciiMode` 分支在切换 `ascii_mode` 的同时调用
   `_ApplyPunctWidth(session, !ascii_before)` ⇒ 中文→英文 得半角（英文标点），
   英文→中文 得全角（中文标点）。
4. **显示侧**：`GetPanelInputState` 改为 live 读 `get_status` 的
   `is_ascii_mode / is_full_shape / is_ascii_punct`，**不调 `set_option`、不读文件**；
   `m_active_session` 被 `FocusOut` 清零的问题用「最近焦点会话」`m_last_focus_session` 回退
   （`FocusOut` 不清它）。
5. **不广播**：裸 Shift 的标点联动只作用于当前会话，不写全局镜像、不跨会话广播。

## Consequences

- ✅ 「半角」在语义上就是英文标点，用户的第 ② 条需求从「加开关」变成「改状态源」。
- ✅ 面板图标、「中/英」标志与实际输入三者同源，不会再互相矛盾。
- ⚠️ Shift 切中英文会覆盖用户先前手动设置的标点宽度 —— 这是需求③的字面要求；
  设置栏的「中/英」按钮**不**强制改 `ascii_punct`，只有裸 Shift 联动。
- ⚠️ 目标方案必须声明 `ascii_punct`，否则 `set_option` 会静默失败。本轮给 `melt_eng` /
  `radical_pinyin` 补上声明且**不写 `reset`** ⇒ 默认 index 0（¥ = 中文标点），与
  `rime_ice.schema.yaml` 同口径。
- ⚠️ 面板反映的是**当前会话**的标点宽度，无法显示其它窗口里手动设置的宽度 ——
  这是 Rime 会话语义的固有限制，不是本决策引入的。
- ↩️ 可逆：把 `ShapeIsFullwidth` 与 `_ApplyPunctWidth` 的映射改回 `full_shape` 即可，成本低。
