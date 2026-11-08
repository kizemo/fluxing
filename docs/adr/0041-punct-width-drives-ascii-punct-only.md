# 0041. 标点宽度只驱动 `ascii_punct`，`full_shape` 永久关闭（全角 = 中文标点）

- **Status**: Accepted
- **Date**: 2026-11-08
- **Deciders**: Fluxing maintainers
- **Related**: spec 099（批次 H / 批次 H2）、**ADR 0040（本文作废其 Decision 第 1、2、3 条中关于 `full_shape` 的部分）**、
  ADR 0010、ADR 0012、ADR 0036、lessons L197
- **Supersedes（部分）**: [ADR 0040](0040-panel-fullwidth-state-source-and-shift-punct-linkage.md)

## Context

ADR 0040 把面板的「全/半角」形式化为 `full_shape = !ascii_punct`，并让 `_ApplyPunctWidth`
**同向写两把开关**（`ascii_punct` 与 `full_shape`）。交付后用户给出语义纠正：

> 可能我对全半角的理解有误。我希望的全角，只是中文标点符号。但数字、其他符号等，
> 还应该和英文状态一致，应该还是半角的。参照搜狗输入法等输入法的设置。

这条纠正与 librime 的实际语义**完全对得上**，说明 ADR 0040 的映射方向选错了量：

1. `ascii_punct` 才是「标点是否走中文映射」。为真时 punctuator 整个 `kNoop`
   （`librime/src/rime/gear/punctuator.cc:102-104`），标点原样直出 ASCII ⇒ 这正是用户要的「英文标点」。
2. `full_shape` **不是**「标点语言」开关，它是 punctuator 的**表选择器**：置 1 时选
   `punctuator/full_shape/` 那张表，于是**数字、空格、引号、斜杠等一并被换成全角字符**
   （`default.yaml:127-` 的 `full_shape` 段：`' '` → `'　'` 等）。这与「数字和其他符号还应是半角」直接冲突。
3. 因此 ADR 0040 的 `full_shape = !ascii_punct` 会在「全角（中文标点）」态**顺手把数字也变全角**，
   这是本轮用户报告的实际症状来源。

另外两条现场事实（本轮实测，用于界定影响面）：

- `default.yaml:276-277, 303` 里 `toggle: full_shape` 的绑定**全部是注释**，且 `299-300` 明写
  「Control+comma (full_shape) is still deliberately unbound -- 全半角 currently has no hotkey.」
  ⇒ **除面板按钮外没有任何热键入口**能打开 `full_shape`。
- `default.yaml` 的 `switcher/save_options` 原本含 `full_shape` ⇒ 一旦被置 1 就会被写进
  `user.yaml` **持久化**，重启后仍是全角。用户机器上确实存在这种残留（`full_shape=true`
  导致「英文标点」态也打出全角空格）。

## Decision

1. **唯一写入量是 `ascii_punct`**。`RimeWithWeaselHandler::_ApplyPunctWidth` 只写 `ascii_punct`，
   且**两个分支都无条件** `set_option("full_shape", 0)` —— 不是只在 `halfwidth==true` 时清，
   因为历史 `user.yaml` 残留需要被主动纠正，与本次切到哪个方向无关。
2. **`full_shape` 在本项目永久关闭**。本项目不再支持「字符宽度」这个输入维度；
   Rime 侧 schema 里 `- name: full_shape` 的 switch **声明保留**（9 个 schema 不动），
   理由：声明本身不打开它（默认 index 0 = 半角），且 Rime 自身的状态展示仍引用它；
   删声明会把 diff 摊到 9 个文件却没有任何行为收益。
3. **写入侧语义改名（用户可见）**：面板槽 5 从「全/半角切换」改为「**中英文标点切换**」，
   状态文案从「全/半」改为「**中文标点 / 英文标点**」。纯函数
   `panel_status::ShapeModeText(bool)` 改名 **`panel_status::PunctModeText(bool chinesePunct)`**。
4. **内部标识符保留旧名**：`ShapeIsFullwidth(ascii_mode, ascii_punct)`、
   `PanelState::fullwidth`、调用方 `onFullwidth` **不重命名**，只补注释声明
   「沿用旧名，语义 = 中文标点模式，**不是** librime 的 `full_shape`」。
   理由：重命名会波及 `RimeWithWeasel.cpp` / `WeaselServerApp.cpp` / 三个测试工程，
   波及面远大于收益，而单一真相源已由 `ShapeIsFullwidth` 的注释与 ADR 承担。
5. **配置侧阻断持久化**：`default.yaml` 的 `switcher/save_options` **移除 `full_shape`**，
   阻止未来再被写进 `user.yaml`。（注意：移除 `save_options` 项**清不掉历史残留**，
   历史残留由第 1 条的「两分支都清 0」兜底。）

## Consequences

- ✅ 数字、空格、其他符号**始终半角**，只有标点随中英切换 —— 与搜狗等主流输入法一致，
  也就是用户真正想要的模型。
- ✅ 面板文案与行为字面一致：看到「中文标点」就打出 `，`，看到「英文标点」就打出 `,`。
- ✅ 与 ADR 0040 的其余结论**不冲突**：状态源仍是 live `get_status`
  （`GetPanelInputState` 零 I/O、不调 `set_option`）、裸 Shift 仍联动、仍不跨会话广播。
  本文只作废「`full_shape` 参与映射」这一处。
- ⚠️ **名称与语义不再字面一致**：`ShapeIsFullwidth` / `fullwidth` 名字里的「fullwidth」
  指的是「中文标点」，不是字符宽度。已用注释 + 本 ADR 标注；下游若按字面理解会误判。
- ⚠️ 用户若手工用 Rime 的命令行 / 第三方工具把 `full_shape` 置 1，本项目的下一次
  `_ApplyPunctWidth` 会把它清掉。这是**刻意行为**，不是 bug。
- ⚠️ 面板仍无法显示「某个窗口里被第三方工具改过的字符宽度」—— 因为该维度已被本项目废弃。
- ↩️ 可逆：把 `_ApplyPunctWidth` 的 `full_shape` 写入恢复成 `!halfwidth`、
  并把 `full_shape` 加回 `save_options` 即可回到 ADR 0040 口径，成本低。