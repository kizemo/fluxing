---
name: Bug A 追查 — kTitleH 30→48 视觉未生效
description: v0.19.0.49 装机端蓝色标题条视觉上没变高，源码分析 + 真因定位
type: project
---

# Bug A 追查报告 — 2026-07-22

> v0.19.0.49 kTitleH 30→48 源码分析，回答"为什么装机端蓝色标题条没变高"

## 1. 代码分析结论

### 1.1 源码确认：kTitleH=48 已正确落地

`WeaselServer/PhrasesDialog.cpp:75`: `constexpr int kTitleH = 48;` ✓

**所有使用 kTitleH 的位置全部保持一致**：

| 位置 | 行号 | 用法 | 正确? |
|---|---|---|---|
| OnCreate 几何计算 | 636 | `titleH = (int)(kTitleH * dpiScale)` | ✓ |
| OnCreate inputY | 669 | `inputY = titleH + gap` = 48+8=56 | ✓ |
| OnPaint titleRc | 868 | `RECT titleRc = {0, 0, paintW, (int)(kTitleH * s_dpiScale)}` | ✓ |
| LayoutDialog titleH | 1379 | `titleH = (int)(kTitleH * s_dpiScale)` | ✓ |
| LayoutDialog inputY | 1397 | `inputY = titleH + gap` | ✓ |
| OnLButtonDown drag hit | 1243 | `titleH_phys = (int)(kTitleH * s_dpiScale)` | ✓ |

**结论：源码层面无 bug。**

### 1.2 ModalChrome::PaintBackgroundAndBorder 没有 hardcoded 高度

`WeaselServer/ModalChrome.cpp:44-48`:
```cpp
void PaintBackgroundAndBorder(HDC hdc, int w, int h, ...) {
  PaintBackground(hdc, w, h, bgTop, bgBot);  // 全 client area
  PaintBorder(hdc, w, h, radius, borderColor);
}
```

- `PaintBackground` 接受 `(w, h)` 参数，画 **整个 client rect**（0,0 → w,h）的渐变
- 不包含任何 hardcoded 30px 或跟 kTitleH 相关的逻辑
- 这是正确的——ModalChrome 是通用 helper，不应耦合某 dialog 的 layout 常量

handoff 中的假说 "ModalChrome::PaintBackgroundAndBorder 用 hardcoded 30 像素" **不成立。**

### 1.3 真因：渐变覆盖全 client，"蓝色标题条"无独立视觉边界

这是 Bug A 的核心发现：

**渐变画全 client area**：
```
kBgTop = RGB(245, 245, 248)  ← 顶部颜色（近白微蓝）
kBgBot = RGB(220, 222, 230)  ← 底部颜色（浅蓝灰）
GradientFill(hdc, v, 2, &g, 1, GRADIENT_FILL_RECT_V)
  // v[0] at (0,0), v[1] at (0, clientH) — 全高度线性过渡
```

**蓝色通道在 460px 高度的分布**：
```
y=0:   蓝 = 248  (近白)
y=30:  蓝 ≈ 246.8
y=48:  蓝 ≈ 246.1
y=460: 蓝 = 230  (浅蓝灰)
```

蓝通道衰减 = 18 单位 / 460px ≈ 0.039/px。在 y=30 和 y=48 之间，蓝通道差 ≈ 0.7 单位——人眼无法分辨。

**视觉上不存在独立的"蓝色标题条"区域**。整个对话框的背景是连续渐变，"蓝色"是用户对顶部区域的主观描述。kTitleH 改变不影响渐变颜色分布。

### 1.4 kTitleH 30→48 真正改变的是什么

| 项目 | 旧值 (kTitleH=30) | 新值 (kTitleH=48) | 用户可感知? |
|---|---|---|---|
| 标题文字 "常用短语" 中心 Y | y=15 | y=24 | 极微弱 |
| 输入框起始 Y | y=38 | y=56 | **应该明显**（下移 18px） |
| ListView 高度 | 342px | 324px | 微弱 |
| Drag 命中区 | 30px | 48px | 功能级别 |
| 渐变颜色（视觉"蓝色条"） | 不变 | 不变 | **无变化** ← 这是用户报告的症结 |

### 1.5 所以用户为什么说"没变高"?

有 3 种可能：

**可能 1（最可能）**: 渐变无边界，视觉上无法区分"标题条"和"主体"。用户期待的"蓝色条变高"是指看到一条更高、更蓝的条状区域——但这需要**单独画一个 title bar 色块**（如 SolidFill 或第二个渐变覆盖 titleRc），当前代码没有这样做。

**可能 2**: 安装路径二进制 stale。尽管 md5 验证通过，但实际运行的 exe 可能来自不同路径（D 盘 vs C 盘 vs release/ 目录）。

**可能 3**: 输入框下移 18px 确实发生了，但用户没注意到（因为整体 UI 比例没变——对话框总高不变，只是 ListView 矮了 18px）。

## 2. 建议的修复方案

### 方案 A（简单，推荐）：OnPaint 中单独画 title bar 色块

在 `PaintBackgroundAndBorder` 之后、`DrawTextW` 之前，增加一个 title bar 色块填充：

```cpp
// v0.19.0.50: 单独涂 title bar 色块（让"蓝色标题条"视觉可见）
const int titleH = (int)(kTitleH * s_dpiScale);
RECT titleRc = {0, 0, paintW, titleH};
HBRUSH hTitle = CreateSolidBrush(kTitleBarBg);  // 新增常量 e.g. RGB(230,240,255)
FillRect(hdc, &titleRc, hTitle);
DeleteObject(hTitle);
// 然后画 border（视觉上分割 title 和 body）
// 然后 DrawTextW 标题文字...
```

**效果**: 用户能看到一条明确的蓝色标题条，高度严格 = kTitleH × dpiScale。kTitleH 改 30→48 后立刻可见变高。

**代价**: 新增 1 个颜色常量 kTitleBarBg，约 5 行代码。

### 方案 B（更完整）：Title bar + body 双段渐变

保留全 client 渐变作为 body 背景，用 2 段式：
1. Title bar: SolidFill kTitleBarBg [0, titleH)
2. Body: GradientFill kTitleBarBg → kBgBot [titleH, clientH)

**效果**: 跟 macOS / WinUI 3 title bar 风格一致。

**代价**: 多一些 paint 指令，需验证 `GRADIENT_FILL_RECT_V` 支持自定义起点。

## 3. 下一步建议

- **如果用户主要关注 drag 命中区扩大（functional bug）**: 当前代码已修复（OnLButtonDown 用 titleH_phys=48），无需额外改动
- **如果用户主要关注视觉"蓝色条变高"**: 建议实施方案 A（OnPaint 单独涂 title bar 色块），在 Phase K 中作为 Bug A 修复一起 ship
- **验证建议**: 新会话装机后先确认输入框是否下移了 18px（量屏幕像素），如果确实下移了，说明 kTitleH=48 已生效，只是视觉"蓝色条"不够明显

## 4. 关键代码路径索引

| 文件 | 行号 | 功能 |
|---|---|---|
| `WeaselServer/PhrasesDialog.cpp` | 75 | `constexpr int kTitleH = 48` |
| `WeaselServer/PhrasesDialog.cpp` | 88-89 | `kBgTop` / `kBgBot` 颜色定义 |
| `WeaselServer/PhrasesDialog.cpp` | 636 | `titleH` DPI 缩放 |
| `WeaselServer/PhrasesDialog.cpp` | 649 | `kListH_phys` 公式（依赖 titleH） |
| `WeaselServer/PhrasesDialog.cpp` | 669 | `inputY = titleH + gap` |
| `WeaselServer/PhrasesDialog.cpp` | 860-861 | `PaintBackgroundAndBorder` 全 client 渐变 |
| `WeaselServer/PhrasesDialog.cpp` | 868 | `titleRc` 定义（用 kTitleH） |
| `WeaselServer/PhrasesDialog.cpp` | 1379 | `LayoutDialog::titleH` 重算 |
| `WeaselServer/PhrasesDialog.cpp` | 1243 | `OnLButtonDown::titleH_phys` drag 检测 |
| `WeaselServer/ModalChrome.cpp` | 9-30 | `PaintBackground` 全 client 渐变实现 |
| `WeaselServer/ModalChrome.cpp` | 44-48 | `PaintBackgroundAndBorder` 复合 helper |
