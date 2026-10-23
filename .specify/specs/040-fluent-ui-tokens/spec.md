# 040 - Fluent UI Token Convergence

> **Status**: Proposed (Draft v1, 2026-07-12)
> **Origin**: User authorization on 2026-07-12 to commit three visual direction decisions; see `docs/design/FLUENT-UI-TOKENS.md` §5 Decision Log.
> **Replaces**: scattered visual constants across 5+ source files with a single source of truth + a unified theme layer.

## 0. Background

Fluxing 当前的视觉常量散落在 5+ 个源文件里：候选框走 4 字段颜色表（spec 033 抽取），QuickPanel 用 5 个野生 RGB 常量 + 11 个几何量，新组件（按钮 / 标签 / 开关）各持各的 token，没有 single source of truth。

过去 8 个版本（v0.19.0.10 → v0.19.0.17）都在 QuickPanel 视觉上烧——L67 / L74 / L77 / L81 / L83 / L85 / L86 / L87 八条 lessons-learned，主线是把"模拟玻璃"路径调稳。但**视觉真值表**这件事一直没有立。

2026-07-12 用户授权三个决议（详见 Decision Log §5）：
- **决议 1**：双 accent——保留火流猩品牌橙 + 引入 Apple systemBlue 作为候选框高亮
- **决议 2**：Win11 build ≥ 22621 走系统材质；旧版 fallback 到 L87 per-pixel alpha
- **决议 3**：v1 不做运行时主题文件覆盖

## 1. Goals

1. **真值表**：所有视觉常量（color / typography / spacing / radius / elevation）集中到 `docs/design/FLUENT-UI-TOKENS.md` 一份文档，每行带来源标注
2. **候选框现代化**：候选框高亮文字从 rime/weasel 上游遗留的 navy 改为 Apple systemBlue（light）+ 白色（dark）
3. **系统材质主路径**：Win11 22H2+ 用户 QuickPanel 走系统材质路径（不再 per-pixel alpha 模拟玻璃）
4. **QuickPanel 接主题**：5 个野生 RGB 常量从源码里移除，改为从主题层读取
5. **向后兼容**：现有 4 字段 Palette 消费者（WeaselPanel / StandardLayout）继续工作，不破坏 spec 033 / 037 测试

## 2. User Stories

| ID | Priority | Story |
|---|---|---|
| US001-A | P1 | 作为最终用户，我在候选框 / QuickPanel / 部署器看到的视觉风格是一致的（同一份色板 / 同一份间距 / 同一份字体节奏） |
| US001-B | P1 | 作为 Win11 22H2+ 用户，我的 QuickPanel 使用系统材质（不是 per-pixel alpha 模拟），性能更好、随系统主题实时切换 |
| US001-C | P1 | 作为 Fluxing 贡献者，我想查"按钮圆角多大"时，打开一份文档就能找到，不需要 grep 5 个 cpp 文件 |
| US002-A | P2 | 作为最终用户，我的候选框高亮色是现代蓝色（Apple systemBlue），不是 90 年代的 navy |
| US003-A | P3 | 作为设计师，我可以参考 `FLUENT-UI-TOKENS.md` 给 QuickPanel v8 / v9 提案新视觉，无需读源码 |

## 3. Functional Requirements

| ID | Requirement |
|---|---|
| FR-001 | `docs/design/FLUENT-UI-TOKENS.md` 必须包含 5 张表（Color / Typography / Spacing / Radius / Elevation），每行包含"名称 / 值 / 当前位置 / 目标位置 / 来源"五列 |
| FR-002 | Palette 数据结构必须扩展 10 个新字段（8 DWORDs + 2 BYTEs）：`panel_glass_top/bot`、`icon_dim`、`accent_primary/secondary`、`hairline`、`highlight_top`、`destructive`、`alpha_panel_top/bot` |
| FR-003 | Palette `hilited_text` 在 light 主题下必须是 Apple systemBlue `#0A84FF`（替换上游 navy `#000080`），dark 主题下保持 `#FFFFFF` |
| FR-004 | QuickPanel 创建时必须根据 OS build 号选择材质后端：build ≥ 22621 走系统材质；build < 22621 / Win10 走 L87 per-pixel alpha |
| FR-005 | QuickPanel 必须移除 5 个野生 `static constexpr COLORREF` 常量（`kBgTop`、`kBgBot`、`kIconDim`、`kAccent`、`kAccent2`），改为从主题层读取 |
| FR-006 | 11 个 QuickPanel 几何常量（`kPanelW` / `kPanelH` / `kBtnSize` / `kBtnGap` / `kIcoSize` / `kBtnRadius` / `kBrandSize` / `kPanelRadius` / `kPanelPadding` + 相关 `_phys`）必须抽到统一 token 头文件，供 QuickPanel + WeaselPanel / StandardLayout 共用 |
| FR-007 | 现有 4 字段 Palette 消费者（WeaselPanel `_RefreshStylePalette`）必须 byte-equal 兼容，不修改其调用方 |
| FR-008 | material backend 必须在 QuickPanel 创建时一次性选定，**运行中不切换**——避免用户升级 OS 不重启进程时面板闪烁 |

## 4. Non-Goals

| ID | Out of scope |
|---|---|
| NG-001 | 运行时 theme.json override（推迟到 spec 041+） |
| NG-002 | Acrylic 材质（v1 不启用，仅在文档中预留 token） |
| NG-003 | Button.h spec 037 R3 中"主按钮用 hilited_back 灰 还是 accent 蓝"——**已由 T000a 决议保留 hilited_back 灰** |
| NG-004 | CJK 字号策略（YaHei non-Variable Font 字距问题，独立 spec） |
| NG-005 | QuickPanel logo 方块渐变——**已由 T000b 决议采用品牌橙→紫**（FLUENT-UI-TOKENS.md §3.1 新增 `color.brand.gradient` token；实现推迟到 v0.19.0.19+） |
| NG-006 | 用户可见字符串汉化（已由 P5 / spec 001 覆盖） |
| NG-007 | 品牌重塑（决议 1 已锁定保留火流猩橙，不再讨论） |

## 5. Acceptance Criteria

| ID | Criterion | Verification |
|---|---|---|
| SC-001-1 | FLUENT-UI-TOKENS.md 包含 5 张表，每行有来源列 | 文件存在 + grep "来源" 计数 ≥ 60 |
| SC-001-2 | Palette 扩展后，spec 033 / 037 测试套件全绿 | 主题桥测试套件 + 新组件测试套件 全 pass |
| SC-002-1 | 候选框高亮在 light 主题下视觉为 `#0A84FF` | 截图比对（spec 022 测试夹具复用） |
| SC-002-2 | 候选框高亮在 dark 主题下视觉为 `#FFFFFF` | 截图比对 |
| SC-003-1 | Win11 build ≥ 22621 上 QuickPanel 显示为系统材质（不是 per-pixel alpha 渐变） | 抓帧验证 + Process Hacker 检查无 per-pixel alpha scan 调用 |
| SC-003-2 | Win10 / Win11 < 22621 上 QuickPanel 显示为 per-pixel alpha 渐变（与 L87 byte-equal） | 抓帧对比 spec 070 L87 fix 截图 |
| SC-004-1 | QuickPanel 源码不再包含 `kBgTop` / `kBgBot` / `kIconDim` / `kAccent` / `kAccent2` 5 个野生常量 | grep 验证 |
| SC-005-1 | QuickPanelDialog 改造前后，相同输入（DPI / 主题 / hover 索引）下像素差 < 0.1% | TestQuickPanelDialog 像素 diff |
| SC-006-1 | 11 个几何常量迁出 QuickPanelDialog.h，迁入统一 token 头文件 | grep + 文件路径断言 |
| SC-007-1 | OS build 检测函数覆盖 22621 边界（≥ / <） | 单测：mock build 22620 / 22621 / 22622 / 19045 三个值，断言后端选择 |

## 6. Risks

| ID | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| R-001 | 系统材质属性在多显示器 + 特定显卡驱动下 z-order 异常 | M | H | Phase 2 验证矩阵：多屏切换 / DPI 切换 / Win10↔11 切换；不通过则默认 fallback 到 L87 |
| R-002 | 系统材质属性在 build < 22621 silently no-op（误判已启用） | M | H | 必须显式调用 OS build 检测 + 单测覆盖 22621 边界 |
| R-003 | QuickPanel 拆双 backend（系统材质 vs per-pixel alpha）估时翻倍 | H | M | Phase 2 第一刀只合 feature flag，灰度后再默认 |
| R-004 | 候选框 hilited 颜色变化是面向所有用户的可见改动 | H | M | CHANGELOG 头条 + first-run wizard 提示 "现代化候选框高亮色为 Apple systemBlue" |
| R-005 | 替换现有渲染路径引入回归——历史上 8 条 lessons-learned 全在这条路径 | M | H | 保留 fallback 路径；像素 diff 测试强制通过；visual regression 截图比对 spec 070 L87 baseline |
| R-006 | 决议 1（Apple systemBlue 替代 navy）破坏某些老用户的视觉习惯 | L | L | 仅影响 light 主题高亮文字色；dark 主题不变 |
| R-007 | Palette 字段扩展后字段顺序变化可能让 spec 033 老二进制布局不匹配 | L | H | 用 `static_assert(sizeof(Palette) == ...)` 在测试夹具里钉死布局 |
| R-008 | QuickPanelDialog 创建时检测 OS build 增加启动延迟 | L | L | `RtlGetVersion` 是 native API，< 1ms；无可观察影响 |

## 7. Dependencies

| ID | Dependency | Status |
|---|---|---|
| DEP-001 | spec 033 (system theme bridge) | ✅ 已 ship（2026-07-04） |
| DEP-002 | spec 037 (new UI components v0) | ✅ 已 ship（2026-07-05） |
| DEP-003 | spec 070 (QuickPanel redesign) | ✅ 已 ship（v0.19.0.10 → v0.19.0.17） |
| DEP-004 | spec 039 (new UI components animations) | ⏳ Pending（仅 Phase 5 依赖，可推迟） |
| DEP-005 | spec 041 (DPI validation) | ⏳ Pending（Phase 3 几何 token 可能需协调 DPI 倍率） |
| DEP-006 | `docs/design/FLUENT-UI-TOKENS.md` | ✅ 已起草（2026-07-12 Draft v0） |
| DEP-007 | Decision Log (FLUENT-UI-TOKENS.md §5) | ✅ 用户授权 2026-07-12 |

## 8. Out of Scope (重复 §4 强调)

见 §4 Non-Goals。其中 **NG-001**（运行时 theme.json）需在 spec 041+ 单独议，但**不影响本 spec 的 token 表架构**——token 命名已经按"可序列化"原则设计（color 用 hex 字符串、不嵌渲染类型），未来加 JSON 解析层不需重命名。

---

*Last updated: 2026-07-12 · Status: Proposed, awaiting plan.md + tasks.md review*