---
paths:
  - "WeaselUI/**/*.cpp"
  - "WeaselUI/**/*.h"
  - "WeaselServer/QuickPanel*"
  - "WeaselDeployer/**/*.cpp"
  - "WeaselDeployer/**/*.h"
  - "WeaselSetup/**/*.cpp"
  - "WeaselSetup/**/*.h"
  - "docs/design/**/*.md"
---

# UI Design Tokens (spec 040)

你正在改 UI 代码。在写任何**颜色 / 字号 / 圆角 / 间距 / 阴影**之前，**必须**先读
[FLUENT-UI-TOKENS.md](../../docs/design/FLUENT-UI-TOKENS.md)。

## 4 条铁律

1. **新视觉常量 → 先入 token 表，再在代码里用**。表里没就新建一行（带"来源"列标注）。
2. **禁止在 `.cpp` / `.h` 里写死颜色 / 几何 / 字号**（仅当 token 表显式标 `WE-PICK` 且暂未抽到 token 时例外）。
3. **改上游遗留值前**（如 rime/weasel navy hilited），先确认 token 文档不视为"保留"。
4. **视觉现代化改动**（用户可见，如本次 hilited_text 改 Apple systemBlue）必须 CHANGELOG 头条 + first-run 提示。

## 来源等级（FLUENT-UI-TOKENS.md 第 3 列）

| 标记 | 含义 | 可破？ |
|---|---|---|
| `HIG` | Apple Human Interface Guidelines | 否——一致性原则 |
| `FLUENT` | Microsoft Fluent UI 2 官方 | 否——一致性原则 |
| `D2D-WIN` | Win32 / Direct2D / DWM 平台约束 | 否——平台约束 |
| `WEASEL` | 上游 rime/weasel 0.17.4 | 需 spec + ADR |
| `WE-PICK` | 我们拍脑袋 | 有 ADR 即可破 |

## 适用路径

- `WeaselUI/**/*.cpp/.h` — 候选框 (`WeaselPanel.*`)、新组件 (`FluxingComponents/*`)、布局 (`*Layout.*`)
- `WeaselServer/QuickPanel*` — 浮动工具栏（L82-L88 系列 fix 源头）
- `WeaselDeployer/**/*` — 设置 GUI（v0.19.0.x 暂未 ship，未来扩展）
- `WeaselSetup/**/*` — installer 首屏（仅在改 installer 视觉时）

## 为什么这条规则存在

L67 → L88 八条 lessons-learned 全在 QuickPanel 视觉上烧——per-pixel alpha / GDI brush
/ WS_EX_LAYERED 都是**症状**，根本原因是**视觉真值没有 single source of truth**。
FLUENT-UI-TOKENS.md 是这个根因的修复。

任何"快速视觉调整"如果不进 token 文档，**下个版本就会变成下个 L## 的素材**。

## 决策记录（ADR）

若必须偏离 token 表（HIG / FLUENT 来源不可破，WEASEL / WE-PICK 可破）：
- 在 `docs/adr/NNNN-<name>.md` 写 ADR（参考 `docs/adr/README.md`）
- 必须含：原 token 值、提议新值、理由、影响范围、是否可回滚
- ADR 落定后**同步更新** FLUENT-UI-TOKENS.md 对应行

## 相关

- [FLUENT-UI-TOKENS.md](../../docs/design/FLUENT-UI-TOKENS.md) — 主真值表（5 张表：Color / Typography / Spacing / Radius / Elevation）
- [spec 040](../../.specify/specs/040-fluent-ui-tokens/spec.md) — 当前活跃 spec
- [CLAUDE.md](../../CLAUDE.md) §1 进站阅读 / §2 必分流 / §3 自我约束
- [constitution.md](../../.specify/memory/constitution.md) — R1-R9 / P1-P8 法则层