# FLUENT-UI-TOKENS.md — Fluxing 视觉真值表

> **Status**: Draft v0（仅骨架 + 来源标注 + 落地路径；具体数值仍待 spec 040 决议）
> **Date**: 2026-07-12
> **Spec**: 接续 spec 033（bridge）/ spec 037（theme adapter）/ spec 039（动画） → 提议 spec 040「Fluent UI Token Convergence」
> **Audience**: `WeaselUI/`、`WeaselServer/QuickPanelDialog.cpp`、`WeaselDeployer/`、installer first-run wizard
> **起源**：[getdesign.md/apple](https://getdesign.md/apple/design-md) 的 token 表格化结构 + Win11 Fluent UI 2 / Mica / Segoe UI Variable
>
> 本文件**只承载**"每个 token 是什么、现在在哪、要去哪、为什么是这个值"。
> 怎么落地（file move / refactor）见 §4；动画 / 渐变 / 阴影细节见 spec 039 / 各 lessons-learned。

---

## 1. Goals / Non-goals

**Goals**
1. 把现存散落在 5+ 个 cpp 里的视觉常量（色值、几何、字体）合并成 5 张表，每个 token 一行
2. 每个 token 必须标注**来源**（HIG 出处 / Fluent UI 官方 / 上游 Weasel / 我们拍脑袋）
3. 标识**缺口**：哪些 token 当前没有，但应该统一收口
4. 给出**落地路径**：current location → target location + spec 编号

**Non-goals**
1. 不是 CSS 框架 / 不是运行时 theme.json 配置（v1 暂缓，runtime override 是后续 spec）
2. 不重新发明 Fluent UI 数值（accent `#0078D4`、Mica `rgba(243,243,243,0.85)` 等直接用 Microsoft 官方）
3. 不覆盖动画曲线 / transition tokens（spec 039 已有 scope）
4. 不引入 SF Pro / Segoe UI Variable 之外的字体（避免授权风险）

---

## 2. 现状快照（截至 v0.19.0.17）

| 层级 | 现状 | 文件 |
|---|---|---|
| **Theme infra** | ✅ `FluxingDarkModeBridge`（读 HKCU `AppsUseLightTheme`）+ ✅ `FluxingTheme` adapter（spec 037） | `RimeWithWeasel/FluxingDarkModeBridge.{h,cpp}`、`WeaselUI/FluxingComponents/FluxingTheme.{h,cpp}` |
| **Color tokens (候选框)** | ✅ 4 色 Palette：dark `{0x1E1E1E,0xE0E0E0,0x2D2D30,0xFFFFFF}` / light `{0xF0F0F0,0x000000,0xD0D0D0,0x000080}`（byte-equal to upstream pre-033） | `FluxingDarkModeBridge.cpp:31-32` |
| **Color tokens (QuickPanel)** | ❌ 野生 5 个 `static constexpr COLORREF`：`kBgTop=RGB(200,225,250)`、`kBgBot=RGB(155,195,240)`、`kIconDim=RGB(60,60,67)`、`kAccent=RGB(255,95,49)`、`kAccent2=RGB(155,81,224)` | `WeaselServer/QuickPanelDialog.h:153-158` |
| **Color tokens (FluxingComponents)** | ✅ Button.h 已订阅 `FluxingTheme`，引用 `palette.hilited_back` | `WeaselUI/FluxingComponents/Button.h` |
| **Geometry tokens** | ❌ QuickPanel 有 11 个 `static constexpr int`（`kPanelW=252, kBtnSize=39, kBtnGap=1, kBtnRadius=10, kPanelRadius=20` 等），WeaselPanel / Layout 未抽出 | `WeaselServer/QuickPanelDialog.h:135-143` |
| **Typography tokens** | ❌ 无抽出；走 `weasel.yaml` 的 `style.font_face` / `style.font_point` | CONTEXT.md §3.2 → `output/data/weasel.yaml` |
| **Spacing scale** | ❌ 无；QuickPanel `kPanelPadding=5`、`kBtnGap=1` 是 ad-hoc | `WeaselServer/QuickPanelDialog.h:135-138` |
| **Radius scale** | ❌ 无；QuickPanel `kBtnRadius=10`、`kPanelRadius=20` 是 ad-hoc | `WeaselServer/QuickPanelDialog.h:139-141` |
| **Elevation tokens** | ❌ QuickPanel 4 层 shadow 叠加（`box-shadow: 0 0 0 0.5px, 0 8px 24px, 0 1px 2px`）写死在 v3-macos 设计稿；D2D / GDI 实现里分散 | `docs/design/04-quick-settings-v3-macos.html:38-44` + 多个 cpp |
| **Mica / Acrylic** | ❌ 完全未启用；QuickPanel GDI 路径走 WS_EX_LAYERED + per-pixel alpha 模拟（L77/L81/L83 链） | `WeaselServer/QuickPanelDialog.cpp` |
| **System accent 跟随** | ❌ 未读 `HKCU\Software\Microsoft\Windows\DWM\AccentColor` | 暂无 |
| **DPI scaling** | ✅ QuickPanel 用 `_phys` 后缀常量 + `s_dpr_x/y` 缩放（L83 fix） | `WeaselServer/QuickPanelDialog.h:101-118` |

**关键缺口**：
- (A) QuickPanel 与 FluxingTheme **完全脱节** —— 5 个 RGB 常量不走 bridge，dark/light 切换时 panel 不变色
- (B) Geometry / spacing / radius / elevation 全部散落在 cpp，没有 single source of truth
- (C) Typography 走 yaml 而不走代码；改字体要重 deploy，不能热切换

---

## 3. Token 表

> **来源标记约定**：`HIG` = Apple Human Interface Guidelines · `FLUENT` = Microsoft Fluent UI 2 官方 · `WEASEL` = 上游 rime/weasel 0.17.4 · `WE-PICK` = 我们拍脑袋 · `D2D-WIN` = Win32 / Direct2D / DWM 平台约束

### 3.1 Color

| Token | Value (hex) | 当前位置 | 目标位置 | 来源 |
|---|---|---|---|---|
| `color.bg.canvas` | `#FFFFFF` (light) / `#1E1E1E` (dark) | `FluxingDarkModeBridge.cpp:31` Palette.back | 同（不动） | FLUENT Canvas |
| `color.text.primary` | `#000000` / `#E0E0E0` | `FluxingDarkModeBridge.cpp:31-32` Palette.text | 同 | FLUENT TextFillColorPrimary |
| `color.text.accent` | `#0A84FF` / `#FFFFFF` | `FluxingDarkModeBridge.cpp:32` Palette.hilited_text（byte-equal 改） | 同 | **决策1**：Apple systemBlue（design-md / Apple HIG），替代 rime/weasel 0.17.4 遗留的 navy `#000080` |
| `color.bg.hilited` | `#D0D0D0` / `#2D2D30` | `FluxingDarkModeBridge.cpp:31-32` Palette.hilited_back | 同 | WEASEL pre-033 |
| `color.bg.panel.top` | `RGB(200,225,250)` (QuickPanel L87) | `QuickPanelDialog.h:153` `kBgTop` | `FluxingTheme` 新增 `panel_glass_top` field | WE-PICK（L87-fix 浅冷蓝） |
| `color.bg.panel.bot` | `RGB(155,195,240)` | `QuickPanelDialog.h:154` `kBgBot` | 同上 `panel_glass_bot` | WE-PICK |
| `color.icon.dim` | `RGB(60,60,67)` | `QuickPanelDialog.h:155` `kIconDim` | FluxingTheme `icon_dim` | HIG ink-muted-80 等价 |
| `color.accent.primary` | `RGB(255,95,49)` 品牌橙 | `QuickPanelDialog.h:156` `kAccent` | FluxingTheme `accent_primary` | WE-PICK（火流猩 brand） |
| `color.accent.secondary` | `RGB(155,81,224)` 品牌紫 | `QuickPanelDialog.h:157` `kAccent2` | FluxingTheme `accent_secondary` | WE-PICK |
| `color.accent.system` | `#0078D4` (Win11 AccentColor) | 暂无 | 新增 `accent_system`（运行时读 HKCU） | FLUENT SystemAccentColor |
| `color.hairline` | `rgba(0,0,0,0.08)` (light) / `rgba(255,255,255,0.10)` (dark) | FluxingComponents D2DRenderer | FluxingTheme `hairline` | FLUENT ControlStroke |
| `color.highlight.top` | `RGB(255,255,255)` | `QuickPanelDialog.h:158` `kHighlight` | FluxingTheme `highlight_top` | WE-PICK（macOS 玻璃高光） |
| `color.brand.gradient` | `linear-gradient(135deg, RGB(255,95,49), RGB(155,81,224))` | 暂无（v3-macos.html mockup 用 Apple blue） | FluxingTheme `brand_gradient`（实现推迟 v0.19.0.19+） | **决策 4（T000b）**：品牌橙→紫渐变；火流猩火系品牌意象，"chrome 暖 / 功能冷"对比 |
| `color.destructive` | `#D04545` | Button.h Destructive style | FluxingTheme `destructive` | WE-PICK |
| `alpha.panel.top` | `220 (0xDC)` | `QuickPanelDialog.h:164` `kAlphaPanelTop` | FluxingTheme `alpha_panel_top` | L87-fix（L86 → L87 验证后） |
| `alpha.panel.bot` | `80 (0x50)` | `QuickPanelDialog.h:165` `kAlphaPanelBot` | FluxingTheme `alpha_panel_bot` | L87-fix |

### 3.2 Typography

| Token | Value | 当前位置 | 目标位置 | 来源 |
|---|---|---|---|---|
| `font.ui.face` | `Segoe UI Variable` (主) / `Microsoft YaHei UI` (CJK fallback) | `weasel.yaml` `style.font_face` | yaml 不动；FluxingTheme 新增 `font_ui_face`（与 yaml 双向校验） | FLUENT Segoe UI Variable |
| `font.candidate.face` | `Segoe UI Variable` | `weasel.yaml` | yaml 不动 | FLUENT |
| `font.candidate.size` | `16` pt（默认） | `weasel.yaml` `style.font_point` | yaml 不动；候选框 D2D 直读 | WEASEL pre-033 |
| `font.ui.size.tiny` | `11 px` | 暂无 | 新增 | HIG caption1 |
| `font.ui.size.small` | `12 px` | QuickPanel 隐式 | 新增 | HIG caption2 / FLUENT Caption |
| `font.ui.size.body` | `13–14 px` | QuickPanel 隐式 | 新增 | HIG body / FLUENT Body |
| `font.ui.size.button` | `14 px` | QuickPanel 隐式 | 新增 | HIG callout |
| `font.ui.size.label` | `17 px` | QuickPanel 隐式 | 新增 | HIG body（mac 17px 节奏） |
| `font.ui.weight.regular` | `DWRITE_FONT_WEIGHT_REGULAR (400)` | FluxingComponents Button.h | FluxingTheme 常量 | HIG |
| `font.ui.weight.semibold` | `DWRITE_FONT_WEIGHT_SEMIBOLD (600)` | FluxingComponents Button.h | FluxingTheme 常量 | HIG |
| `letter-spacing.tight` | `-0.224 px @ 14px` | 暂无 | 新增 | design-md apple pattern |
| `letter-spacing.display` | `-0.28 px @ 56px` | 暂无（仅 hero，不适用） | N/A | design-md |

> 关键约束：QuickPanel 等 tray UI 是 pixel-perfect surface，**不**走 viewport responsive；字号是固定值，不参与设计稿 responsive breakpoints 概念（参 `docs/design/quickpanel-compare.html` 对比说明）

### 3.3 Spacing

| Token | Value (px) | 当前位置 | 目标位置 | 来源 |
|---|---|---|---|---|
| `space.xxs` | `2` | QuickPanel `kBrandGap` (品牌→按钮) | FluxingTheme 常量 | design-md |
| `space.xs` | `4` | 隐式 | FluxingTheme 常量 | design-md |
| `space.sm` | `6` | QuickPanel `kPanelPadding` left/right padding | FluxingTheme | WE-PICK（小于 design-md 8） |
| `space.md` | `10` | QuickPanel | FluxingTheme | design-md |
| `space.lg` | `14` ★ (v0.19.0.24-fix, L93=11 → 24=14,user +3 px 累计) | QuickPanel `kBtnGap=14` (按钮间距,user v0.19.0.24 再 +3 px) | FluxingTheme | WE-PICK（user 反馈后采用,11→14 累计 +8 px 自 L92=6） |
| `space.lg.history` | `6 (v0.19.0.22)` / `11 (v0.19.0.23)` / `14 (v0.19.0.24)` | 撤销但记入 history 字段 | FluxingTheme history | WE-PICK 历史 |
| `space.xl` | `17` ★ | 暂无（design-md 魔数） | FluxingTheme | design-md magic |
| `space.xxl` | `21` ★ | 暂无（design-md 魔数） | FluxingTheme | design-md magic |
| `space.section` | `32` | 暂无 | FluxingTheme | design-md |
| `space.magic.17` | `17` (QuickPanel kPanelPadding 隐式) | 显式化 | FluxingTheme | design-md |
| `space.magic.21` | `21` (QuickPanel 隐式) | 显式化 | FluxingTheme | design-md |
| `size.panel.desktop.w` | `289` (v0.19.0.24-fix, L93=277 → 24=289,+12 = 4×3 btn gap 累加保守路线) | `QuickPanelDialog.h` `kPanelW=289` (user "右间距保留 16 px" → 277+12=289) | FluxingTheme | WE-PICK（保留 rightPad=16 保守路线;激进备选 kPanelW=277,rightPad=4,见 §3.2.3） |
| `size.panel.desktop.h` | `48` | `QuickPanelDialog.h` `kPanelH=48` (v0.19.0.16 起 70% 缩放后稳定值) | FluxingTheme | design-md |
| `size.btn.desktop` | `35` | `QuickPanelDialog.h` `kBtnSize=kBrandSize=35` (v0.19.0.24 不缩按钮) | FluxingTheme | WE-PICK（与 panel 高度 48 比例 73%,与 70% design 一致） |
| `icon.bbox.cx_off` | `15` (viewBox 局部坐标) | L93-fix 新增 — DrawIcon* 描线 viewBox X bbox center | FluxingTheme `icon_bbox_center_x` | WE-PICK（viewBox 24×24 内统一,Schema/Phrase/Symbols/Settings/Account 全部 =15） |
| `icon.bbox.cy_off` | `15` | L93-fix 新增 — DrawIcon* 描线 viewBox Y bbox center (Account 略偏 16,统一 15) | FluxingTheme `icon_bbox_center_y` | WE-PICK |
| `time.grace.show_ms` | `2000` (v0.19.0.24 新增) | QuickPanel `kShowGraceMs` Show() 后 auto-hide grace period | FluxingTheme | WE-PICK（spec 040 时间 token, 跟 design-md 节奏一致;user 期望"show 出来能看一会儿"） |

> 8px 基线 + 17/21 流体节奏（design-md 哲学），不从 0 起步所以最稀有的极小间距（1/2/3px）单独列出避免被 8 倍数稀释

### 3.4 Radius

| Token | Value (px) | 当前位置 | 目标位置 | 来源 |
|---|---|---|---|---|
| `radius.none` | `0` | tile / 无圆角场景 | FluxingTheme 常量 | design-md |
| `radius.xs` | `5` | QuickPanel `kPanelPadding` 隐式 | FluxingTheme | design-md |
| `radius.sm` | `7` | v3-macos `--btn-radius:7px` | FluxingTheme | HIG (mac button) |
| `radius.md` | `10` | QuickPanel `kBtnRadius=10` | FluxingTheme | design-md |
| `radius.lg` | `14` | v3-macos `--radius:14px` (panel) | FluxingTheme | HIG (mac panel) |
| `radius.xl` | `20` | QuickPanel `kPanelRadius=20` | FluxingTheme | WE-PICK（Win11 Fluent 12-16，QuickPanel 偏大） |
| `radius.pill` | `9999` (height/2) | v3-macos pill button | FluxingTheme | design-md |
| `radius.full` | `50%` | circular icons | FluxingTheme | design-md |

### 3.5 Elevation

| Token | Composition | 当前位置 | 目标位置 | 来源 |
|---|---|---|---|---|
| `elevation.flat` | 无 shadow，仅 hairline | 大多数面板 | FluxingTheme / StyleSheet | design-md "Flat" |
| `elevation.hairline` | `1px rgba(0,0,0,0.08)` 边框 | FluxingComponents Button/Input | FluxingTheme | design-md "Soft hairline" |
| `elevation.glass.panel` | `per-pixel alpha 220→80 gradient + 20px 圆角`（QuickPanel L87） | `QuickPanelDialog.h:164-165` | FluxingTheme `glass_alpha_top/bot` | L87-fix（自创，模拟 macOS 玻璃） |
| `elevation.shadow.toolbar` | `0 0 0 0.5px rgba(0,0,0,0.04)` | v3-macos CSS box-shadow | FluxingTheme | design-md "Product shadow"（唯一允许 drop-shadow 的场景） |
| `elevation.shadow.popover` | `0 8px 24px rgba(0,0,0,0.10)` | v3-macos CSS | FluxingTheme | HIG popover |
| `elevation.shadow.modal` | `0 1px 2px rgba(0,0,0,0.04)` | v3-macos CSS | FluxingTheme | HIG modal |
| `elevation.mica` | `DWM_SYSTEMBACKDROP_MAIN` + `rgba(243,243,243,0.85)` | 暂无 | 新增（**决策2**：Win11 build ≥ 22621 走 Mica，< 22621 fallback 到 per-pixel alpha L87） | FLUENT Mica |
| `elevation.acrylic` | `DWM_SYSTEMBACKDROP_TRANSIENT` + tint color | 暂无 | 新增（备选，v1 不启用） | FLUENT Acrylic |

> QuickPanel 当前 4 层 shadow 叠加（`v3-macos.html:38-44`）实际**只用了 1 层视觉**：per-pixel alpha gradient 已吃掉背景深度，多余 shadow 是从 macOS CSS 照搬的 dead weight → spec 040 收敛到 `elevation.flat` + `glass.panel`

---

## 4. 落地路径（Migration Plan）

### Phase 0 — 本文件定稿（spec 040 spec.md）

- [ ] spec 040 创建：`specs/040-fluent-ui-tokens/spec.md` / `plan.md` / `tasks.md`
- [ ] 召集 review：token 值采纳 vs 拍脑袋，特别是 `accent.primary` 品牌橙 vs 系统蓝的优先级
- [ ] **决策点**：运行时 theme.json override（v1 还是 v2）

### Phase 1 — Palette 扩字段（spec 040 T001-T003，**不动 UI**）

1. **`RimeWithWeasel/FluxingDarkModeBridge.h`** `struct Palette` 扩字段：
   - 增加 `panel_glass_top/bot`、`icon_dim`、`accent_primary/secondary/system`、`hairline`、`highlight_top`、`destructive`、`alpha_panel_top/bot`
   - 保持原有 4 字段不变（向后兼容，spec 033 测试套件不能炸）
   - 新增 `kPaletteDark/light` 在 `FluxingDarkModeBridge.cpp` 增补

2. **测试**：`TestDarkModeBridge` / `TestFluxingComponents` 加字段验证

### Phase 2 — FluxingTheme 转发（spec 040 T004，**接 QuickPanel**）

1. `WeaselUI/FluxingComponents/FluxingTheme.h` 新增 typed accessors：
   - `ColorF Accent() const` / `ColorF PanelGlassTop() const` / `BYTE AlphaPanelTop() const` 等
   - 返回 `D2D1::ColorF` 让 FluxingComponents 直接消费
2. `QuickPanelDialog` 不直接 include FluxingTheme（避免拉入 D2D）—— 走 IPC 拉一份（spec 040 需设计 wire format）

> **⚠️ 风险**：`QuickPanelDialog` 是 GDI 路径，不依赖 D2D；FluxingTheme 当前依赖 `D2DRenderer.h`。需要**拆 layer**：Theme 数据层（D2D-free） + Theme 渲染层（D2D）
> 这是 spec 040 必须解决的第一个真问题

### Phase 3 — Geometry / Spacing / Radius 抽头（spec 040 T005-T008）

1. 新建 `WeaselUI/include/GeometryTokens.h`，命名空间 `fluxing::tokens`
2. 把 `QuickPanelDialog.h:135-143` 的 11 个 `static constexpr int` 全部迁入
3. WeaselPanel / StandardLayout / Button / Toggle / Label 同步迁入
4. 验证 `TestQuickPanelDialog` / `TestQuickPanelRefactor` 像素不变

### Phase 4 — Typography 收口（spec 040 T009，可选）

1. `weasel.yaml` 加 `style.font_ui_face`、`style.font_ui_size_tiny/small/body/button/label`
2. DirectWrite 解析时与 FluxingTheme 双向校验
3. **风险**：weasel.yaml 是 weasel.yaml 是 RIME YAML schema，扩展可能影响上游 deployer

### Phase 5 — 动画接入（依赖 spec 039）

1. spec 039 完成后，token 表新增 `transition.duration.short/normal/long`、`easing.standard/decelerate/accelerate`
2. QuickPanel 灰显 / dark-mode 切换应用 200ms gradient 过渡

---

## 5. 已决议（Decision Log）

> 三项 v1 关键决策（2026-07-12 用户授权 Claude 决策）

### 决策 1 — Accent 双轨：保留品牌橙 + 引入 Apple systemBlue

**结论**
- `accent.primary` 保留 `RGB(255,95,49)` 品牌橙（QuickPanel hover / active / brand chrome）
- `accent.secondary` 保留 `RGB(155,81,224)` 品牌紫
- **`color.text.accent`（候选框 hilited text）从 navy `#000080` 改为 Apple systemBlue `#0A84FF`（light）/ `#FFFFFF`（dark）**——byte-equal 改 `FluxingDarkModeBridge.cpp:32` `kPaletteLight`
- 引入 Apple systemBlue → systemIndigo 渐变 `#0a84ff → #5e5ce6` 用于 logo 方块 / link 风格 icon 背景

**Trade-off**
- v0.18.x 升级用户看到候选框颜色变化——CHANGELOG 标 "现代化候选框高亮色为 Apple systemBlue"
- Fluxing 仍是 "暖橙品牌 + 现代蓝 accent" 双色系，不强求纯 Apple 单蓝——保留 fork 差异化

**残余风险**
- 部分用户反馈 "蓝色像 QQ"——CHANGELOG 注明是设计意图

### 决策 2 — Win11 Mica，Win10 / 旧 build fallback 到 per-pixel alpha

**结论**
- 启动时 `RtlGetVersion` 检测 build number
- build ≥ 22621：QuickPanel 用 `DWMWA_SYSTEMBACKDROP = DWMSYSBT_MAIN` + tint `rgba(243,243,243,0.85)`——系统 Mica
- build < 22621 / Win10：保留 L87 per-pixel alpha gradient（220→80 已 ship 验证）
- material backend 存 `FluxingTheme::material_backend_` 字段，**运行中不切换**（避免升级系统不重启进程时闪烁）
- 备选 `elevation.acrylic`（`DWMSYSBT_TRANSIENT`）v1 不启用

**Trade-off**
- Win11 ≥ 22621（>95%）vs 旧版（<5%）视觉不完全一致——first-run 提示 "建议升级到 Win11 22H2+ 获得最佳视觉效果"
- Mica 是 uniform tint，无 220→80 渐变——要渐变只能 fallback L87 path
- QuickPanelDialog.cpp 要拆 `mica_backend` / `per_pixel_backend` 两个绘制路径，Phase 2 估时翻倍

**残余风险**
- `DWMWA_SYSTEMBACKDROP` 在某些显卡驱动 + 多屏配置有 z-order 历史 issue——spec 040 Phase 2 必跑验证矩阵：多屏切换 / DPI 切换 / Win10 ↔ Win11 切换
- build < 22621 的 `DWMWA_SYSTEMBACKDROP` 是 silently no-op——必须显式检测 build，避免误判

### 决策 3 — v1 不做 runtime theme.json override

**结论**
- v1（spec 040）：token 是**只读 C++ 常量**——编译期定，运行时不可改
- v2（spec 041+ 单独议题）：可选用 `%AppData%\Fluxing\theme.json` 覆盖，**只读 + 重启生效**
- 永远不做：热重载 / user.css-like override / 第三方插件

**Trade-off**
- 想做 "暗色粉" / "圣诞红" 个性化主题的用户**做不到**——明确告知不支持，是 scope 纪律
- spec 040 估时减少 ~30%（无 JSON 解析、无 hot-reload watcher、无 spec 041 placeholder）
- `weasel.yaml` 已有 `style.font_face` / `style.font_point`——typography 已经半 runtime，**够了**，color 不加第二层

**残余风险**
- 若用户爆发 theme.json 需求——spec 041 加，**不影响** spec 040（真值表与 runtime override 正交）

### 决策 4 — Button 主色保留 `hilited_back` 灰底（spec 037 R3 不动）

**结论**
- Button.h spec 037 R3 维持现状：Primary 按钮 fill = `palette.hilited_back`（light `#D0D0D0` / dark `#2D2D30`）
- **不**改 Apple systemBlue，也不改品牌橙

**理由**
- 品牌橙预算留给 QuickPanel hover chrome（不与 action 抢色）
- Apple systemBlue 预算留给候选框 hilited（不与 button primary 撞色）
- macOS toolbar 风格就是 subdued gray button + 彩色 data highlight，对比清晰
- spec 037 已 ship，用户反馈中性，切换需要灰度

**残余风险**
- 部分用户希望 button primary 更醒目——CHANGELOG 不提此决策（无视觉变化）

### 决策 5 — Logo 方块渐变采用品牌橙→紫

**结论**
- `color.brand.gradient = linear-gradient(135deg, RGB(255,95,49), RGB(155,81,224))`
- v0.19.0.19+ 才在 QuickPanel 实际应用，本 spec 仅在 FLUENT-UI-TOKENS.md 立 token

**理由**
- 火流猩品牌 = 火系（"火"字 + 红色猩猩剪影），暖色渐变匹配 fire/sunset 意象
- "chrome 暖 / 功能冷"对比清晰——QuickPanel logo 块是 chrome（暖），候选框 hilited 是功能（冷）
- 与上游 Apple-clone IME 拉开视觉差

**残余风险**
- v3-macos.html mockup 与最终实现不一致——需更新 mockup 为新渐变（v0.19.0.19+ 单独 task）

---

## 6. 仍待 spec 040 决议（Open Questions for spec 040 §X.Y）

1. **CJK 字号**：QuickPanel 当前混用 SF Pro 思维（17px 节奏）+ 中文（实际渲染是 Microsoft YaHei UI），YaHei 没有 Variable Font，14px → 21px 字间距差异可能视觉不连续。spec 040 typography 章节需补 DirectWrite tracking 策略

---

## 7. 引用

- 设计哲学参考：[getdesign.md/apple](https://getdesign.md/apple/design-md)（结构借鉴，非数值借鉴）
- Microsoft Fluent UI 2：[Fluent 2 Design](https://fluent2.microsoft.design/) · [Mica / Acrylic](https://learn.microsoft.com/en-us/windows/apps/design/style/acrylic)
- Apple HIG：[Apple Human Interface Guidelines](https://developer.apple.com/design/human-interface-guidelines/)（玻璃材质 + SF Pro 节奏）
- 现有 bridge 实现：`RimeWithWeasel/FluxingDarkModeBridge.h:41-46` Palette struct
- 现有 theme adapter：`WeaselUI/FluxingComponents/FluxingTheme.h:32-75`
- QuickPanel 现状：`WeaselServer/QuickPanelDialog.h:135-165`
- D2D 渲染坑位：`.specify/memory/lessons-learned.md` L70 / L70-suppl-v1 / L70-suppl-v2
- 跨工程约束：CLAUDE.md §2「必查图谱 / 必中文 / 必 IPC 四边同步 / 必不开 TSF 阻塞 I/O」

---

*Last updated: 2026-07-12 (Draft v0) · Owner: 待 spec 040 指派*