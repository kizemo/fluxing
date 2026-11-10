# ADR 0052 — 快捷设置栏「方向 A」分层半透明主题（GDI 自绘 chrome）

- 状态: 已采纳 (2026-11-10)
- 影响范围: `include/FluxingPanelTheme.h`（新建）、`WeaselServer/QuickPanelDialog.{h,cpp}`、`test/TestQuickPanelDialog`
- 决策类型: 架构决策（渲染模型 + 主题真相源 + alpha 合成口径）
- 关联: 交接 `docs/archive/handoffs/handoff-2026-11-10-batch-v-panel-anchor-and-theme.md`；设计稿 `docs/design/quickpanel-v6/`（`design-spec.md` / `direction-approved.md` / `design-demos/a-benchmark.html`）
- 前序: ADR 0013（面板透明度）、ADR 0035（位置持久化）、ADR 0050（4 槽 + 符号同比）

## Context

### 用户诉求

用户反馈「设置栏缺乏美感」⇒ 本轮走 huashu-design：出三方向真实初稿（HTML + 截图），
用户**选定方向 A —— 微软拼音 IME 工具栏 / Fluent Design 2 标杆迁移**
（批准记录 `docs/design/quickpanel-v6/direction-approved.md`）。

### 旧渲染模型表达不了方向 A

旧实现（v0.19.0.10 起）：

1. `CreateRoundRectRgn` + 白底 + 1px 边框 + 顶部高光（**全部实色**）；
2. 画完再用 `ApplyAlphaGradient` **按颜色阈值猜 alpha**（「这个像素接近底色 ⇒ 给它
   alpha=梯度值」）。

⇒ 只能表达「一块实色 + 一个透明度」，**表达不了分层半透明**：
面板体 .72 / 外描边 .28 / 顶部高光 .55 / 品牌分隔 .18 / 轨道底 .07 / 轨道描边 .22 /
三层投影 .05+.04+.03 —— 每一层的 alpha 都不一样，且要**叠**在一起。

## Decision

1. **只借结构语法，不借配色**：借「半透明层级 / 1px 发丝描边 / 分层软投影 /
   分组轨道 / 光学均衡」；**不借** Fluent 蓝、Mica、Acrylic 玻璃。
   品牌橙（#FF5F31）只用于中文侧状态与 hover 轮廓。
2. **交付形态仍是 Win32 GDI 自绘**：32-bit BGRA DIB + `UpdateLayeredWindow`。
   **无 CSS、无 D2D、无 GDI+、无 blur / backdrop-filter**（这是本仓库的交付约束）。
3. **alpha 模型换血**：自己按 **src-over 预乘**逐层合成，把结果写进 DIB 的 alpha 字节，
   由 `UpdateLayeredWindow` 按 per-pixel alpha 上屏。
   ★ **绝不使用 GDI `AlphaBlend`** —— 品牌 logo 黑框的前科：那条路径把半透明像素当反向权重。
4. **单一真相源 = `include/FluxingPanelTheme.h`**：调色板 / alpha / 几何 /
   `ChromeSpec` 构造（`BuildChromeSpec`）/ `RasterizeChrome` 全部在此。
   生产代码与单测读**同一份**（constitution R7）；测试**不复制**任何一条公式。
5. **圆角语义改用 SVG `rx`**：角圆心 = `(l+r, t+r)`，半径 `r`。
   ⚠ GDI 的 `CreateRoundRectRgn(l,t,w,h,ellipseW,ellipseH)` 的角半径是
   `ellipseW/2` ⇒ 传 14 只有 **7** 的圆角（旧面板比设计稿「方」）。
6. **投影必须裁剪到面板体轮廓之外**（★ 本轮实测踩到并修掉的坑）：
   投影形状 = **下移后的面板体**，与面板体本身重叠；面板体半透明（α .72）⇒
   不裁剪时影子会**透过面板体**把整块面板压暗（面板体 alpha 从 184 被抬到 ~200+，整体发灰）。
   ⇒ 投影只保留**面板体之外**的部分（底部那圈软影）。
7. **窗口加高 `kPanelShadowH = 6` logical px**（只多一条透明阴影带）。
   **面板体语义仍是 155×32** ⇒ 槽位几何 / 命中区 / 既有单测断言全部不变。
   投影带高度按 `dpr_x` 缩放（`dpr_x` 只依赖宽度 ⇒ 无循环依赖），
   三层投影偏移与带宽同源 ⇒ 偏移永远 ≤ 带宽。
8. **内容不透明判定**（`FinalizeThemeAlpha`）：GDI 画的 logo / 图标 / 文字必须不透明。
   判定 = **该像素 RGB 与 chrome 快照不同 ⇒ 被 GDI 改过 ⇒ alpha = 255**，
   容差 **2/通道**。理由：预乘↔直通往返有 ±1 误差（面板体 #FAF7F2 往返成
   (249,247,243)），**不留容差**会把 logo 的预合成底色误判成内容 ⇒
   logo 周围出现一圈不透明方块。
9. **hover 只做 1px 橙描边轮廓浮出（α .50），不铺橙底** —— 守 L86 用户指令
   「hover 时底色不变，只 icon 线条变橙」。
10. **状态取色**：视觉位 1「中/英」中文态 #FF5F31 / 英文态 #241F1A；
    视觉位 2 全角环 #FF5F31 / 半角实心点 #241F1A；active 态统一白。
11. **不动的部分**：4 键帽结构与 `kVisualToLogical = {0,4,5,1}` 不变；
    托盘右键「快捷键设置」入口保留（只摘面板键帽）；
    显示路径**零 I/O**（constitution P2）。

## Consequences

- 面板从「实色白块」变成**分层半透明**：面板体透出桌面、描边与高光是发丝、
  底部有软投影、状态区有分组轨道。
- 渲染模型与主题常量各自只有一处真相源（`FluxingPanelTheme.h`），
  改配色/改几何只动这一个文件；单测 T41 组 30 条断言直接断言这些常量与合成结果。
- 旧的「按颜色阈值猜 alpha」路径整体下线（`ApplyAlphaGradient` 改为调用
  `FinalizeThemeAlpha`），那条路径表达不了分层 alpha，且猜阈值天然脆。
- **视觉仍需用户在真机确认**：自动化能给出像素级证据（`FLUXING_QP_DIAG_DUMP=1`
  导出 `qp-dump.bmp`），但「好不好看」是人的判断。
- 圆角与阴影是**真几何**（SVG rx 语义 + 独立投影带），
  改动会影响面板占位与命中区之外的观感，需与 ADR 0050 的槽位几何一起看。
