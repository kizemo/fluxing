# QuickPanel 二次问题调研 (issue 1+2+3+4) — L94 debate

> **状态**: 进行中 — 3 位团队成员分头调研中
> **日期**: 2026-07-13
> **Spec 关联**: 接续 spec 070 v0.19.0.23 (L93),user 反馈 v0.19.0.23 ship 后还有 4 个问题
> **方法**: 3 个独立 hypothesis,Round 1 各自写出 + Round 2 读他人证伪 + Round 3 共识

## 0. 问题陈述 (user 反馈,post v0.19.0.23 ship)

| # | 问题 | user 截图证据 |
|---|---|---|
| 1 | 按钮距离设置栏顶部和底部边框的距离不一致,导致按钮和按钮内的图标又偏上了 | 截图: 按钮竖直位置偏 panel 上半 |
| 2 | 按钮间的距离请再增加 3 px | user 文字 |
| 3 | 上一轮反馈的,点击按钮后,按钮背景色变成橙色,但鼠标松开点击后,背景色不恢复的 bug 仍然存在 | user 文字 (post v0.19.0.23 仍出现) |
| 4 | 上一轮反馈的,第一次点击快捷键调出设置栏,但设置栏很快消失的 bug 没有修复 | user 文字 (post v0.19.0.23 仍出现) |

## 1. Round 1 — 3 个独立 hypothesis

### 1.A Hypothesis: 几何/布局 (Agent A 负责 issue 1+2)

#### 1.A.1 结论与 primary hypothesis

**Primary hypothesis（主推方案）**：issue 1 的直接几何根因是按钮仍以
`y0 = pad = 5` 放置，但 `panelH = 48`、`btnSize = 35`，于是按钮占用半开区间
`[5, 40)`（像素行 5..39），上方留白 5 px、下方留白 `48 - (5 + 35) = 8 px`；
按钮几何中心为 `22.5`，panel 中心为 `24`，按钮整体偏上 1.5 px。主推将垂直布局
独立于水平 `pad`，按 `(panelH - btnSize) / 2` 居中，即整数 `y0 = 6`，形成上 6 px、
下 7 px（离散像素下最接近对称），并让 icon 随按钮整体下移 1 px；不要把 top highlight
或 bottom shadow 当作 layout inset。issue 2 主推保持 `panelW = 277` 与按钮 35 px 不缩，
将 `kBtnGap` 改为 14，此时 rightPad 为 4 px。若设计要求延续当前 16 px 右边距，则应把
`panelW` 扩为 289 px。

**Alternative hypothesis（备选）**：用户判断“偏上”主要基于 panel 的可见内容区，而不是
48 px 外框。若扣除顶部 1 px highlight 与底部预留 4 px shadow，可见内容区近似
`y=[1,44)`，中心为 `22.5`，恰与当前按钮中心 `22.5` 一致；这会证伪“按钮数学中心错误”
作为唯一根因。备选方案是保留 `y0=5`，重新定义/移动 highlight 与 shadow，使上下视觉重量
对称，或以视觉实验确定 optical offset，而不是先改 panel/按钮尺寸。

引用：常量位于 `WeaselServer/QuickPanelDialog.h:126-161`；按钮 `y0=pad` 位于
`WeaselServer/QuickPanelDialog.cpp:716-727`；highlight 与 shadow 位于
`WeaselServer/QuickPanelDialog.cpp:668-680`；icon 定位位于
`WeaselServer/QuickPanelDialog.cpp:774-806`。

#### 1.A.2 当前精确几何（1× DPI / logical px）

输入：`panelW=277`、`panelH=48`、`pad=5`、`btnSize=35`、`btnGap=11`、
`brandSize=35`、`brandGap=2`、`icoSize=19`（`WeaselServer/QuickPanelDialog.h:132-144`）。
运行时会将常量缩放成 `_phys`（`WeaselServer/QuickPanelDialog.cpp:574-596`）；以下先按
用户给定的 1× 几何计算。

1. **按钮 Y**：`y0 = pad = 5`（`WeaselServer/QuickPanelDialog.cpp:724-727`）。按钮 region
   为 `y=[5,40)`；从尺寸公式看，上距 5 px、下距 8 px。按钮中心
   `5 + 35/2 = 22.5`，panel 中心 `48/2 = 24`，故按钮几何中心偏上 1.5 px。
   注意 GDI `CreateRoundRectRgn` 的右/下边界语义不应改变 layout 尺寸公式；若截图按实际
   着色像素逐行量测，应同时记录 region rasterization 的端点，避免把 API 边界语义误算成
   多 1 px。
2. **icon anchor Y**：代码执行 integer division：
   `iconY = y0 + s_btnSize_phys / 2 - kIconBboxCyOff`
   `= 5 + 35/2 - 15 = 5 + 17 - 15 = 7`，不是 7.5
   （`WeaselServer/QuickPanelDialog.cpp:789-800`）。`kIcoSize=19` 不参与当前 icon positioning；
   它只被缩放存入 `s_icoSize_phys`（`WeaselServer/QuickPanelDialog.cpp:590-596`）。
3. **icon 描线 bbox 中心**：前四个 icon 的局部 Y bbox 中心是 15，所以屏幕中心为
   `iconY + 15 = 22`；Account 的局部中心约 16.5，所以是 23.5。对应：
   - integer button pixel center：`y0 + btnSize/2(integer) = 22`，前四个严格对齐；
   - continuous button center：22.5，前四个偏上 0.5 px，Account 偏下 1 px；
   - panel continuous center：24，前四个偏上 2 px，Account 偏上 0.5 px。
   各 icon 原始 bbox 见 `WeaselServer/QuickPanelDialog.h:146-161`，实际 drawing 坐标见
   `WeaselServer/QuickPanelDialog.cpp:70-157`。
4. **扣除 highlight + shadow 的可见内容中心**：highlight 从 `y=1` 起，高 1 px；shadow
   终点是 `H-4=44`，高度 1 px，即实际线在 `[43,44)`，但“扣 4 px shadow”按任务口径
   是排除底部 `[44,48)`。因此内容区取 `[1,44)`，高度 43、中心 22.5。
   - button continuous center 22.5：与内容中心一致；
   - 前四个 icon bbox center 22：偏上 0.5 px；
   - Account center 23.5：偏下 1 px。
   这说明用户看到的“按钮偏上”与选择哪套视觉边界高度相关：相对完整 48 px panel 确有
   1.5 px 偏上，相对 `[1,44)` 可见内容区则按钮已居中。并且代码实际只画 1 px shadow，
   `H-4` 更像 4 px 底部预留/offset，不是“4 px 高 shadow”
   （`WeaselServer/QuickPanelDialog.cpp:668-680`）。

#### 1.A.3 issue 1 候选方案与 trade-off

| 方案 | 精确结果（1×） | 优点 | Trade-off / 风险 |
|---|---|---|---|
| A. 独立垂直居中（primary） | `y0=(48-35)/2=6`（integer），上 6、下 7；iconY=8；前四 icon center=23 | 最小视觉位移；不影响水平布局；不改 panel 外形和按钮大小 | 完整 panel 中仍有 1 px 离散不对称；相对 `[1,44)` 内容中心会变成按钮偏下 1 px；需同步 HitTest 的 Y 范围，避免画面/点击错位（当前 HitTest 用 `padding`，`WeaselServer/QuickPanelDialog.cpp:387-392`） |
| B. 缩短 panelH | `panelH=45` 且 y0=5 → 上/下均 5，中心 22.5 | 数学完全对称；保留 pad、btnSize | panel 更矮 3 px，圆角、窗口定位、DPI surface 与 brand 外框都变化；底部 shadow 的 `H-4` 位置上移，可能更拥挤；与现有 48 px 视觉 token/设计稿偏离 |
| C. 增大按钮 | `btnSize=38`、panelH=48、y0=5 → 上/下均 5 | 完全填平多出的 3 px；点击目标更大 | 水平总宽增加 `5*3=15` px，必须扩 panelW 或压缩 gap/rightPad；logo 35 与按钮 38 尺寸失配；icon 在更大按钮内显得更小 |
| D. 改统一 pad | `pad=6` → 上 6、下 7；等价于 A 的 Y，但同时 buttonStartX/logo X/Y 都变 | 单常量修改看似简单 | `kPanelPadding` 同时控制 X/Y；会让总水平公式增加 1 px，右边距减少，并让 logo 从 (5,5) 移到 (6,6)，无法只修按钮；不推荐把水平与垂直约束继续耦合 |
| E. 调 highlight/shadow（alternative） | 保持 y0=5，移动/对称化两条视觉线，或明确内容区为 `[1,44)` | 若截图中的偏移来自视觉重量，可保留已正确的内容区中心；不破坏 hit-test/layout | 不能改变按钮相对完整 panel 的 1.5 px 数学偏移；需截图/A-B 实验证据，否则只是 optical compensation；shadow 与 alpha/rasterization 可能导致不同壁纸下观感变化 |

补充：只改 `iconY` 不是完整修复，因为 issue 1 同时指出“按钮”本体偏上。若按钮下移，当前
icon 公式会随 `y0` 同步下移；单独加 icon offset 会重新引入按钮中心与 bbox 中心不一致。

#### 1.A.4 issue 2：`kBtnGap=14` 后的新水平布局

当前起点是 `buttonStartX = pad + brandSize + brandGap = 5+35+2 = 42`
（`WeaselServer/QuickPanelDialog.cpp:716-725`；1× 时 `max(2,2*dpr)=2`）。新 gap 下：

`5 + 35 + 2 + 5*35 + 4*14 + rightPad = 273 + rightPad`。

- **方案 2A（primary，固定 panelW）**：保持 `panelW=277`、`btnSize=35`，则
  `rightPad = 277 - 273 = 4 px`。按钮 x 区间为 `[42,77)`、`[91,126)`、
  `[140,175)`、`[189,224)`、`[238,273)`，末端到 panel 右边 4 px。
  - 优点：窗口宽度不变，满足“不缩按钮”和 gap +3；改动范围最小。
  - 缺点：当前推导中的 rightPad=16（`WeaselServer/QuickPanelDialog.h:131-135`）骤降到
    4，右侧视觉呼吸感与左 pad=5 接近但不再保留原 16 px；圆角半径 20 下末按钮靠近右圆角，
    必须截图验证是否显挤。
- **方案 2B（保留当前 rightPad=16）**：扩 `panelW = 273 + 16 = 289 px`。
  - 优点：仅把 4 个 gap 各增加 3 px，总宽正好 +12；保留现有右侧余量和按钮大小。
  - 缺点：panel 更宽、屏幕右下定位左移 12 px；若用户意图只增加按钮间距而不增加 panel
    footprint，会显得整体过宽。
- **方案 2C（对称 5 px 外边距）**：扩 `panelW = 278 px`，得到 rightPad=5。
  - 优点：左/右外边距数值对称，panel 只加宽 1 px。
  - 缺点：brand 左侧 5 px 与按钮右侧 5 px 的内容类型不同，数值对称不必然 optical 对称；
    仍放弃现有 16 px 右余量。

不建议缩按钮：若固定 `panelW=277` 且强保留 rightPad=16，五个按钮总宽只剩
`277-5-35-2-56-16=163`，平均 32.6 px，必须混用 32/33 px，既违反“保持 btn 不缩”
也增加 rounding 与 icon/点击区不一致风险。

#### 1.A.5 brand area 与 DPI 的视觉影响

- Logo 的 destination 是 `pad, pad, s_brandSize_phys, s_brandSize_phys`
  （`WeaselServer/QuickPanelDialog.cpp:700-710`），与按钮同为 35 logical px；其 WIC scaler
  直接按 `s_brandSize_phys` 生成正方形 bitmap（`WeaselServer/QuickPanelDialog.cpp:254-273`）。
  因而 **35×35 PNG 画布/目标框不会因 DPI 单独比 35×35 按钮小**：brand 与 button 都用
  `scale_x`（`WeaselServer/QuickPanelDialog.cpp:590-596`）。
- 但“看起来小”仍可能发生：PNG 画布若含透明内边距，真正红色 logo 的 alpha bbox 小于
  35×35，而按钮的可感知边界/hover target 是完整 35×35。WIC 会缩放整张画布，不会裁掉
  transparent padding；应量测 PNG alpha bbox，而非只看文件尺寸。
- DPI 下两者尺寸缩放一致，但 icon drawing 是 raw pixel offsets、没有随 dpr 缩放
  （`WeaselServer/QuickPanelDialog.cpp:789-798`）。高 DPI 时按钮和 logo 会变大，矢量 icon
  描线 bbox 仍约 21..25 raw physical px；这会让 **icon 相对按钮/Logo 越来越小**，可能被误报为
  “logo/按钮大小不一致”。此外 X/Y 分别基于 `dpr_x/dpr_y`，button/brand size 用 x scale，
  icon size 用 y scale（`WeaselServer/QuickPanelDialog.cpp:579-596`）；非等比窗口缩放时应额外检查。
- 1× 当前 brand 为 `[5,40)×[5,40)`，与按钮同 Y；它相对 48 px panel 也同样上 5、下 8。
  若只把 buttons 的 y0 改为 6，logo 会比按钮高 1 px。Primary 的视觉验证必须比较两种选择：
  logo 与按钮保持同 y0（一起下移）还是保留 logo；不能只截取按钮区域判断。

#### 1.A.6 Falsification checks

1. **整 panel 像素量测（最可能证伪 primary）**：在 100% DPI 的 raw `qp-dump.bmp` 或无缩放
   截图中标出 panel 实际 alpha/边框上下边界、按钮 active region 上下边界。若量得按钮相对
   完整 panel 已是上下等距，或实际 client height 不是 48，则“`y0=5` 导致 5/8”被证伪；
   应转查 layered-window/DPI rasterization。
2. **内容区对照实验**：保持 `y0=5`，分别隐藏 highlight、隐藏 shadow、两者都隐藏后截图盲评。
   若“不等距/偏上”随 highlight/shadow 消失，而按钮 bbox 坐标不变，则 Alternative 获支持，
   Primary 不是唯一根因。
3. **单变量 y0 A/B**：仅在实验 build 将垂直起点从 5 改 6（水平 pad 不变），保持 panelH、
   btnSize、highlight、shadow 全不变。若用户仍认为按钮和 icon 偏上，或认为明显偏下，则
   1 px 下移不足/方向错误，Primary 被证伪。正式实现还需同步 HitTest，实验应同时用视觉 dump
   与点击测试区分 paint 和 interaction。
4. **DPI 矩阵**：在 100%、125%、150%、以及项目曾遇到的 sub-100% 环境分别量测
   `s_panelH_phys`、`s_btnSize_phys`、`s_panelPadding_phys` 和 icon bbox。若问题仅出现在某一 DPI，
   根因更可能是独立 rounding/raw-pixel icon，而不是 1× 常量本身；相关缩放代码见
   `WeaselServer/QuickPanelDialog.cpp:574-596`。
5. **PNG alpha bbox 检查**：读取缩放前后的 logo alpha bbox。若非透明内容已接近完整 35×35，
   “透明内边距让 logo 看起来小”被证伪；若内容 bbox 明显小，则不能用 brandSize 与 btnSize
   相等推导 optical size 相等。
6. **gap/rightPad 截图验证**：把 gap 设为 14 后分别渲染 277 px（rightPad=4）与 289 px
   （rightPad=16）两版。若 277 px 版末按钮与右圆角发生视觉/alpha 裁切或明显拥挤，则否决
   issue 2 的 fixed-width primary，选择扩 panelW。

#### 1.A.7 推荐顺序

1. 先用 raw dump 验证完整 panel 边界与 `[1,44)` 内容边界，避免把 4 px shadow offset 当成
   4 px shadow thickness。
2. issue 1 优先验证独立 `buttonY=6`；若用户以内容区为基准，则改测视觉线权重方案。
3. issue 2 先展示 `277/rightPad=4` 与 `289/rightPad=16` 两版；需求只说 gap +3，数学上两者
   都满足，最终选择取决于是否要保留现有 16 px rightPad。
4. 所有正式变更必须保持 paint 与 HitTest 同源；本段仅调研，不修改任何源码。


### 1.B Hypothesis: 状态机 (Agent B 负责 issue 3)

**调研范围**: 仅 issue 3 — "点击按钮后背景色变橙,松开后不恢复"。**未读任何代码
外实现,只读源码 + 注释 + lessons-learned 引用**。

#### 1.B.1 相关字段 (QuickPanelDialog.h)

- `static int s_hoveredIdx` (QuickPanelDialog.h:85) — 当前 hover button idx, -1 = none
- `static int s_activeIdx` (QuickPanelDialog.h:86) — 当前 **LButtonDown 时锁定的
  button idx** (active bg 状态机 source of truth)
- `static bool s_dragging` (QuickPanelDialog.h:122) — L87-fix 手动 drag 状态机,
  L88-fix 改为任何位置 LButtonDown 都进入
- `static int s_outsideMs` (QuickPanelDialog.h:125) — L89-fix auto-hide 累加器

#### 1.B.2 WndProc 关键分支 (QuickPanelDialog.cpp)

**WM_LBUTTONDOWN (cpp:441-459)** — 无条件启动 drag + (可选) 锁定 active:

```cpp
case WM_LBUTTONDOWN: {                                    // cpp:441
  POINT p = {LOWORD(l), HIWORD(l)};
  int hit = HitTest(p.x, p.y);
  // L88-fix: drag 任何位置都启动
  SetCapture(hwnd);
  s_dragging = TRUE;                                      // cpp:449
  GetCursorPos(&s_dragStartCursor);
  GetWindowRect(hwnd, &rc);
  s_dragStartWindow = rc;
  if (hit >= 0) {
    s_activeIdx = hit;                                    // cpp:455 锁定 click target
    InvalidateRect(hwnd, NULL, FALSE);
  }
  return 0;
}
```

> 注: `s_dragging = TRUE` **无条件** (cpp:449),不依赖 `hit`。这是 L88-fix 的
> 设计意图 — "drag **任何位置**都启动"(cpp:444-447 注释)。

**WM_LBUTTONUP (cpp:488-505)** — 两个分支,**s_activeIdx reset 不对称**:

```cpp
case WM_LBUTTONUP: {                                      // cpp:488
  if (s_dragging) {
    s_dragging = FALSE;                                   // cpp:490
    ReleaseCapture();
    // drag 结束后清 hover
    s_hoveredIdx = -1;                                    // cpp:493 ✓
    InvalidateRect(hwnd, NULL, FALSE);
    // ❌ 没有 reset s_activeIdx                          // cpp:494 缺失行
  } else {
    POINT p = {LOWORD(l), HIWORD(l)};
    int hit = HitTest(p.x, p.y);
    if (hit >= 0 && hit == s_activeIdx) {
      // 5 按钮 no-op (spec 070 T007)
    }
    s_activeIdx = -1;                                     // cpp:501 ✓ (only else branch)
    InvalidateRect(hwnd, NULL, FALSE);
  }
  return 0;
}
```

**WM_TIMER id=2 (cpp:522-548)** — polling hover + auto-hide:

```cpp
case WM_TIMER: {
  if (w == 2) {
    POINT p;
    if (GetCursorPos(&p) && ScreenToClient(hwnd, &p)) {
      int hit = HitTest(p.x, p.y);
      if (hit != s_hoveredIdx) {                          // cpp:527
        s_hoveredIdx = hit;
        InvalidateRect(hwnd, NULL, FALSE);
      }
      // auto-hide: hit==-1 && !s_dragging 时 s_outsideMs += 100
      if (hit == -1 && !s_dragging) {                     // cpp:535
        s_outsideMs += 100;
        if (s_outsideMs >= 1500) { s_outsideMs = 0; Hide(); }
      } else {
        s_outsideMs = 0;
      }
    }
    return 0;
  }
  return OnTimer(hwnd, w);
}
```

#### 1.B.3 PaintOpaqueContent 中的 active bg 判定 (cpp:725-746)

```cpp
for (int i = 0; i < 5; i++) {
  bool isActive = (i == s_activeIdx);                     // cpp:729
  bool isHover  = (i == s_hoveredIdx);
  HBRUSH bgBrush = NULL;
  if (isActive) {
    bgBrush = s_hBrushActive;                             // cpp:737 = 橙色 brush
  }
  if (bgBrush) {
    FillRgn(hdc, rgn, bgBrush);                           // cpp:744 画橙色 bg
  }
  ...
}
```

> **关键**: 橙色 bg 是否画 = `s_activeIdx != -1` 在 paint 时刻。`memset 0` 在
> RepaintLayered:892 已经做了,所以 bg 残留**只能来自状态机没 reset**,不是
> DIB pixel 残留(L81-fix 已防住后者,见 lessons-learned L81)。

#### 1.B.4 Cause chain (≥3 种让 active bg 残留的触发链)

##### Cause 1 (PRIMARY): drag 分支不 reset s_activeIdx

**触发场景**: 用户点 button0 (button 内),拖动 (>系统 drag threshold 或仅仅几 px),
松开在 button0 **内或外**。

**步骤**:
1. `WM_LBUTTONDOWN` (cpp:441) → `s_dragging = TRUE` (cpp:449) + `s_activeIdx = 0`
   (cpp:455,hit==0)
2. `WM_MOUSEMOVE` (cpp:460) → `s_dragging==TRUE` 分支移动 window,**不更新
   `s_hoveredIdx`**,**不更新 `s_activeIdx`**
3. `WM_LBUTTONUP` (cpp:488) → `s_dragging==TRUE` 进入 if-d 分支 →
   `s_dragging = FALSE` + `s_hoveredIdx = -1`,**`s_activeIdx` 保持 0**
4. `RepaintLayered` (cpp:874) → `PaintOpaqueContent` 循环看到 `s_activeIdx==0`
   → `isActive=true` → `FillRgn` 画橙 bg (cpp:744)
5. 后续 timer / mousemove 只更新 `s_hoveredIdx`,不会清 `s_activeIdx`

**结果**: button0 永久橙色直到下一次 `WM_LBUTTONDOWN` 覆盖或 `Hide()`(cpp:1106)。

> **Strength**: 这是 user 反馈 "post v0.19.0.23 仍然存在" 的最直接路径 —
> L88-fix (v0.19.0.22 之前) 把 `s_dragging=TRUE` 改为无条件后,LButtonUp 的
> if-d 分支没同步补 reset。

##### Cause 2 (ALTERNATIVE): LButtonUp 时 cursor 在 panel 外 hit==-1

**触发场景**: 用户点 button0 后,**不松开**直接拖到 panel 外松开。

**步骤**:
1. `WM_LBUTTONDOWN` → `s_dragging=TRUE`, `s_activeIdx=0`, SetCapture
2. 拖动 → SetWindowPos 移动 panel,也可能 cursor 出 panel 边界
3. `WM_LBUTTONUP` 在 panel 外触发 → SetCapture 已经拦截,WM_LBUTTONUP 仍能收到
4. 进入 if-d 分支 (cpp:489) → `s_hoveredIdx=-1`,**`s_activeIdx` 保持 0**
5. 后续没有 LButtonDown (因为 user 已经松开) → `s_activeIdx` 永驻

**关键点**: 即使 drag 没真正移动 window (delta=0),只要 `s_dragging==TRUE`
走 LButtonUp,**`s_activeIdx` 永远不 reset**。这是 cause 1 的子集,但用户感知
不同: "我明明是 click 啊,没拖动啊"。

##### Cause 3 (ALTERNATIVE): re-enter panel 时的状态污染

**触发场景**: user 在 button0 上点击,**拖出** panel → 松开 → **重新 hover**
button1 → 看到 button0 残留橙色 + button1 hover 线条变橙。

**步骤**:
1-4. 同 cause 1,残留 `s_activeIdx=0`
5. `WM_TIMER id=2` polling (cpp:522) 看到 cursor 进入 button1 → `s_hoveredIdx=1`
6. `RepaintLayered` → button0 `isActive=true` 画橙 bg,button1 `isHover=true`
   画橙线条
7. 视觉上: button0 是"死橙" (active 残留) + button1 是"活橙" (hover 线条)
   → user 看到 "button0 永远卡住"

##### Cause 4 (LATENT): WM_NCHITTEST HTCAPTION 与 SetCapture 的潜在冲突

**观察**: `WM_NCHITTEST` (cpp:412-418) 在 hit==-1 时返回 HTCAPTION,这时
DefWindowProc 会处理 system drag / 非 client mouse move。我们的 `SetCapture`
(cpp:448) 抢走了 capture,**但 HTCAPTION 路径下** `WM_NCLBUTTONDOWN` 可能
先生效。如果系统先发 NCapture 再发 client LButtonDown,顺序会乱。**目前没有
直接证据**,列为 latent risk。

#### 1.B.5 修法选项 (≥3 种)

##### Fix A (MINIMAL): 在 LButtonUp if-d 分支 reset s_activeIdx

```cpp
case WM_LBUTTONUP: {
  if (s_dragging) {
    s_dragging = FALSE;
    ReleaseCapture();
    s_hoveredIdx = -1;
    s_activeIdx = -1;        // ← 加这一行
    InvalidateRect(hwnd, NULL, FALSE);
  } else { ... }
}
```

**优点**: 1 行 fix,直接对症 cause 1/2/3。
**缺点**: "drag 后松手" 的语义仍混在一起 — user drag 后松手,bg 闪一下消失
(被 active→无的 transition)。视觉上可能不够优雅,但**功能正确**。

##### Fix B (SEMANTIC): 区分 drag 启动条件 — 只在 hit==-1 时启动 drag

```cpp
case WM_LBUTTONDOWN: {
  int hit = HitTest(p.x, p.y);
  if (hit == -1) {
    SetCapture(hwnd);
    s_dragging = TRUE;
    // record start ...
  }
  if (hit >= 0) {
    s_activeIdx = hit;
    InvalidateRect(hwnd, NULL, FALSE);
  }
}
```

**优点**: 恢复 L86 之前的语义 — button click **不** 触发 drag,只有空白区拖动。
**缺点**: user 反馈 "L87 button 内长按无法拖动" 是这个语义下的痛点。Fix B
会回退到那个痛点。**L88-fix 的"任何位置都能 drag"是有意为之**,不能简单回退。

##### Fix C (CLEAN): 拆分 click vs drag 状态机 — 用 drag-threshold 判定

```cpp
case WM_LBUTTONDOWN: {
  int hit = HitTest(p.x, p.y);
  s_dragging = TRUE;            // 总是进入 pending-drag 状态
  s_activeIdx = (hit >= 0) ? hit : -1;  // 但只锁定 button
  s_dragStartCursor = ...;
  SetCapture(hwnd);
  InvalidateRect(hwnd, NULL, FALSE);
}

case WM_MOUSEMOVE: {
  if (s_dragging) {
    int dx = cur.x - s_dragStartCursor.x;
    int dy = cur.y - s_dragStartCursor.y;
    if (abs(dx) + abs(dy) > GetSystemMetrics(SM_CXDRAG)) {
      // 超过系统 drag threshold,真正 drag,清 active
      s_activeIdx = -1;
      SetWindowPos(...);
    }
  } else { ... }
}

case WM_LBUTTONUP: {
  if (s_dragging) {
    s_dragging = FALSE;
    ReleaseCapture();
    POINT p = {LOWORD(l), HIWORD(l)};
    int hit = HitTest(p.x, p.y);
    if (hit >= 0 && hit == s_activeIdx) {
      // click 触发 (s_activeIdx 保持 click 之前的值)
    } else {
      s_activeIdx = -1;        // drag 后松手,清 active
    }
    s_hoveredIdx = -1;
    InvalidateRect(hwnd, NULL, FALSE);
  }
}
```

**优点**: 语义清晰 — click / drag 分开判定,符合 Windows 标准 (SM_CXDRAG)。
**缺点**: 状态机重写,涉及 WM_MOUSEMOVE 路径,可能引入新的 edge case
(如: click 0 → drag 出 threshold → 回原位松手 → 是否触发 click?)。

##### Fix D (PARANOID): SetCapture 失败 fallback

如果 `SetCapture` (cpp:448) 因窗口 z-order / WS_EX_LAYERED 失败,后续
`WM_LBUTTONUP` 可能收不到,`s_dragging=TRUE` 永久残留。**实际中这种概率极低**
(panel 一直前台),但属于 latent risk。可加 try-catch / GetCapture 检查。

#### 1.B.6 user 截图 + "点 button0 然后拖到 button1 再松开" 推演

**场景**: user 在 button0 上 LButtonDown → 拖动 cursor 到 button1 上 →
LButtonUp 在 button1。

**推演**:
1. `LButtonDown` (cpp:441): `hit=0` → `s_dragging=TRUE`, `s_activeIdx=0`
   → Repaint: button0 橙 bg
2. `MouseMove` × N (cpp:460): `s_dragging==TRUE` → SetWindowPos 移动 window
   (或不动,如果 cursor 在 panel 内),`s_hoveredIdx` **不更新** (被 if-else 跳过,
   cpp:471)。button0 仍橙
3. `LButtonUp` 在 button1 上 (cpp:488): `s_dragging==TRUE` → if-d 分支 →
   `s_dragging=FALSE`, `s_hoveredIdx=-1`, **`s_activeIdx` 保持 0**
   → Repaint: button0 **仍橙** (因为 `s_activeIdx==0` isActive=true),button1
   线条**不橙** (因为 `s_hoveredIdx==-1` isHover=false)
4. user 期望: button1 应该 hover 变橙线条 + button0 恢复默认
   实际: button0 永久橙 + button1 灰色

**根因**: `WM_MOUSEMOVE` 在 `s_dragging==TRUE` 分支**完全跳过 hover 更新**
(cpp:471 注释明示),所以 LButtonUp 后 cursor 在 button1,**第一次** timer polling
(id=2, 100ms 后) 才更新 `s_hoveredIdx=1`。但 `s_activeIdx` 永驻 0。

#### 1.B.7 Primary hypothesis & falsification checks

**Primary**: LButtonUp if-d 分支 (cpp:489-494) 没 reset `s_activeIdx`,且
WM_MOUSEMOVE 在 dragging 期间**跳过 hover 更新** (cpp:471),导致 drag 结束后
button 残留 active bg 直到下一次 LButtonDown 覆盖。

**Alternative 1**: Cause 2 (LButtonUp 出 panel) 是 sub-class,实际机制同 primary。
**Alternative 2**: WM_NCHITTEST HTCAPTION 路径 + SetCapture 抢 capture 顺序
冲突 — 需要更多证据 (latent,未证实)。

**Falsification checks**:

1. **Check F1 (BEHAVIORAL)**: 如果在 `WM_LBUTTONUP if-d 分支` 加
   `s_activeIdx = -1;`,user 复现 "点击 button 0 → 拖到 button 1 → 松开"
   路径,bug 是否消失?
   - **PASS** → primary 证实。
   - **FAIL** → 转移到 Alternative 2 调查 WM_NCHITTEST / SetCapture 顺序。
   - **验证方法**: 用 `FLUXING_QP_DIAG_DUMP=1` env var (cpp:902) 看 `qp-state.txt`
     中 `active=` 在 LButtonUp 后的值。

2. **Check F2 (BINARY)**: 如果暂时回退 L88-fix (在 hit==-1 时才 `s_dragging=TRUE`),
   保留 LButtonUp if-d 分支不 reset `s_activeIdx`:
   - **PASS** (bug 消失) → 是 L88-fix 的 drag-any-area 引入的,锁定 primary。
   - **FAIL** (bug 仍在) → 可能是 SetCapture 顺序问题或 DIB 残留
     (但 L81 memset 已防住后者)。

3. **Check F3 (STATE INSPECTION)**: 在 `PaintOpaqueContent` 入口
   (cpp:638) 加 `OutputDebugStringW` 打印 `s_activeIdx`,user 复现 bug 时看
   每次 Repaint 的 active 值是否残留:
   - 如果 Repaint 之间 active 值**不变**(始终是 button idx) → state 残留,证实
     primary。
   - 如果 Repaint 之间 active 跳到 -1 又跳回 → 不是 state 残留,可能是另外的
     渲染问题 (但 memset 已防住,概率低)。

4. **Check F4 (CONTROL)**: 测试纯 click 不动 cursor 的场景:user 点 button0,
   **不拖动**, 松开在 button0 内。期望: button0 闪一下橙 → 立刻恢复。
   - 实际代码路径: LButtonDown → s_activeIdx=0, LButtonUp → s_dragging==TRUE
     (因为 L88-fix 无条件启动) → **进入 if-d 分支** → s_activeIdx 永驻
     → button0 **永久橙**。
   - **这意味着**: 即使 user 完全不拖动,只要他点 button,**必中 bug**。
   - 这是 L88-fix 设计本身的副作用,**最确凿的 primary 证据**。

#### 1.B.8 推荐修法

**推荐 Fix A** (最小修改) 作为 hotfix ship,因为:
- 1 行 fix,改动面最小,risk 最低
- 直接对症 cause 1/2/3
- 不改变 L88-fix 的 drag-any-area 设计

**后续可考虑 Fix C** 作为 refactor,把 click / drag 拆开,但需要单独 spec
和测试覆盖 (涉及 WM_MOUSEMOVE 路径修改 + SM_CXDRAG 阈值)。

#### 1.B.9 引用 (file:line)

- `WeaselServer/QuickPanelDialog.cpp:441-459` — WM_LBUTTONDOWN
- `WeaselServer/QuickPanelDialog.cpp:449` — `s_dragging = TRUE` 无条件 (L88-fix)
- `WeaselServer/QuickPanelDialog.cpp:455` — `s_activeIdx = hit` 锁定 click target
- `WeaselServer/QuickPanelDialog.cpp:460-487` — WM_MOUSEMOVE
- `WeaselServer/QuickPanelDialog.cpp:471` — drag 期间跳过 hover 更新
- `WeaselServer/QuickPanelDialog.cpp:488-505` — WM_LBUTTONUP (关键分支)
- `WeaselServer/QuickPanelDialog.cpp:489-494` — if-d 分支不 reset s_activeIdx
- `WeaselServer/QuickPanelDialog.cpp:496-503` — else 分支 reset s_activeIdx
- `WeaselServer/QuickPanelDialog.cpp:522-548` — WM_TIMER id=2 polling hover
- `WeaselServer/QuickPanelDialog.cpp:638-746` — PaintOpaqueContent active bg 判定
- `WeaselServer/QuickPanelDialog.cpp:729` — `bool isActive = (i == s_activeIdx)`
- `WeaselServer/QuickPanelDialog.cpp:737` — `bgBrush = s_hBrushActive` (橙色)
- `WeaselServer/QuickPanelDialog.cpp:744` — `FillRgn(hdc, rgn, bgBrush)` 画橙 bg
- `WeaselServer/QuickPanelDialog.cpp:874-1002` — RepaintLayered
- `WeaselServer/QuickPanelDialog.cpp:892` — L81 memset 0 (防 DIB 残留,但**不防 state 残留**)
- `WeaselServer/QuickPanelDialog.h:85-86` — s_hoveredIdx / s_activeIdx 字段
- `WeaselServer/QuickPanelDialog.h:122` — s_dragging 字段

#### 1.B.7 Round 2 — Falsifications & Defense

> **本节为 Agent B (1.B) 对 Agent A (几何) 与 Agent C (生命周期) 的科学辩论**。
> 不修改任何源码, 仅做 falsification + defense。
>
> 立场速览:
> - **接受** 1.A primary (`y0=6` 修 issue 1) — 见 1.B.7.1
> - **有条件接受** 1.C primary (Show grace period 修 issue 4) — 见 1.B.7.2
> - **坚持** 自己 primary (LButtonUp if-d 分支 reset `s_activeIdx` 修 issue 3) — 见 1.B.7.3
> - **ship 顺序**: 1.A 独立 ship, 1.B + 1.C 必须同步 ship 同一 hotfix — 见 1.B.7.4

##### 1.B.7.1 对 1.A (Agent A 几何) 的 falsification

**Falsification #1 — orthogonal 不构成证伪, 但暴露 1.A 的边界**

1.A primary 假设根因是 `y0=5` 让按钮几何中心 `22.5` 相对 panel 中心 `24` 偏上 `1.5 px`,
修法 `y0=6`(QuickPanelDialog.h:132 + cpp:716-727)。

这条 primary 与 1.B primary (`s_activeIdx` 残留) **完全正交**:
- 1.A 改 paint 时按钮 y 坐标 → user 看到的"按钮位置"
- 1.B 改 state machine → user 看到的"按钮颜色"
- 两个 user 报告分属不同感知通道 (布局 vs 颜色), 不可能互证伪

**所以**: 即使 1.A primary 完全对 (`y0=6` 让按钮 100% 视觉居中), **issue 3 仍残留** — 因为 `s_activeIdx` 残留与几何无关。
反之, 即使 1.B primary 完全对 (drag 后 `s_activeIdx` 必清), **issue 1 仍残留** — 因为几何仍 `5/8` 不对称。

**这意味着**: 1.A 跟 issue 3 **没有 falsification 关系**, 但 1.A **不能跨进 1.B 的领域**。若 Agent A 在 hotfix 描述里写 "this fixes the visual offset AND the orange button", 那就是越界, 必须 challenge。

**对 1.A 的真正压力点** (不是 falsification, 是邻接区域 concern):

- **替代方案 E 的风险** (1.A.3): 1.A 提了 Alternative E "调 highlight/shadow 让 visible content 中心对称"。**若 Agent A 走 Alternative E (保留 `y0=5` + 改视觉线)**, 我预测:
  - issue 1 视觉上确实可能改善 (用户感知"按钮居中")
  - 但 `s_activeIdx` 残留仍存在, issue 3 仍报 → user 会二次反馈 → user 体验 "fix 没修好"
  - 因此 **Alternative E 把 issue 1 修复期延后到 issue 3 hotfix 完成之后** 才有效, 因为必须先让用户不再因 issue 3 重新关注 panel
  - 1.A 应改 ship 注释: "Alternative E ship 后必须 issue 3 also fixed, 否则 user 报告会混淆"

- **1.A primary 的 opt-out 风险** (1.A.3 table): 1.A 提到"icon 公式随 `y0` 同步下移, 单独加 icon offset 会重新引入按钮中心与 bbox 中心不一致"。
  - 1.A 在 hotfix 时**只**改 `y0=6` 不够: 还必须验证 `iconY` formula (cpp:789-800) 是否仍产出"按钮内 icon 居中", 不能出现 `y0=6` 让按钮居中但 icon 反而偏下
  - 这不是 falsification, 是 ship 时的 validation hook, 1.A 应配套加 TestQuickPanel 一个像素 dump 快照 (HIT icon center == button center)

**结论**: 1.A primary 与 1.B primary 互相独立, **都需要 ship** 才能修完 issue 1+2+3。

##### 1.B.7.2 对 1.C (Agent C 生命周期) 的 falsification

**Falsification #2 — 1.C 忽略 L88-fix 的副作用会放大 issue 3 的复现**

1.C primary (1.C.8) 假设: L89 auto-hide 在 hotkey 调出场景被立即触发, 因为 cursor 停在文本输入框不在 panel 内, 1500 ms 后 `Hide()`。
修法 #1: Show() 后 2 秒 grace period, 期间不累加 `s_outsideMs` (1.C.5 #1, cpp:535-543 改)。

我**有条件接受**这条修 issue 4, 但 1.C 的 grace 修法**没有看到 L88-fix 的副作用**,会**间接放大** issue 3 的复现路径。

**具体推理**:
1. L88-fix (v0.19.0.22): `s_dragging = TRUE` 从 "只在 hit==-1" 改成"无条件"
   (QuickPanelDialog.cpp:441-459)
2. 1.C grace period 期间: cursor 在 panel 外 (典型 hotkey 场景, panel 右下角, cursor 文本输入框), `s_outsideMs` 不累加, **不** Hide
3. **但 grace 期间 user 移 cursor 进 panel** → user 想"点 button0" → **LButtonDown 触发**
   - `s_dragging = TRUE` (cpp:449)
   - `s_activeIdx = 0` (cpp:455)
4. **LButtonUp 时** (`s_dragging==TRUE` 走 if-d 分支 cpp:489-494):
   - 当前代码: `s_dragging=FALSE`, `s_hoveredIdx=-1`, **不** reset `s_activeIdx`
   - 视觉: button0 残留橙 bg
5. **bug 出现**: user 在 grace period 内必须点 panel 才会跟 panel 交互;一旦点了 → 必中 issue 3

**这意味着 1.C grace period 让 user 有更多机会跟 panel 交互** → user 接触 panel 频率 ↑ → issue 3 复现率 ↑ 至少 2x (从 hotkey-only 到 grace-period 范围)。

**若 1.C ship 不带 1.B ship**, 会发生:
- user v0.19.0.24 (假设只 ship 1.C) 反馈 "issue 4 好了, 但 issue 3 仍存在, 而且**比以前更明显** 因为我终于能多按几下 hotkey 了"
- 这是 1.B primary ("LButtonUp if-d reset") 的真正强 falsification risk, 因为它会让 user 关注到 issue 3
- 因此 **1.C 拒绝单独 ship**

**Falsification #3 — 1.C cause 3 (复用路径 timer 未重启) 其实是 1.B 的 latch**

1.C.4 cause 3 提到: 复用路径 Show() (cpp:1048-1055) 不重启 timer id=2 — 1.C 标为 "latent, 独立于 issue 4"。

我**反对**这标 "独立":
- 复用路径不重启 timer → polling 死了 → `s_hoveredIdx` 永远停在 Hide 前的值 → issue 3 (active bg 残留) 在第二次 Show 后**视觉叠加** hover 残留
- 实际: 假设 user 在 button2 上 click (active=2), panel Hide, 复用路径 Show, 但 timer 已 KillTimer (cpp:1102) 没重启 → `s_hoveredIdx` 永驻 2 → 第二次 Show 时 button2 **同时** active 橙 bg + hover 橙线条 (双层叠加)
- 这不是 1.B primary (state 残留), 是 timer 不工作 → state 不更新 → 视觉看像残留

**对 1.C 修法 #4 的支持**:
- `Hide()` reset `s_outsideMs` + 复用路径 Show() restart timer (1.C.5 #4) → 同时修 1.C cause 2 + 1.C cause 3 (timer latch)
- **强烈建议 1.C #1 + #4 同时 ship**, 这点 1.C 自己 §1.C.8 也提到, 我赞同

但我**额外压力测试** 1.C 关于 timer restart 的假设:
- 修法 #4 在 Show() 复用路径加 `KillTimer + SetTimer` → 每次 Show 都 reset → 但 `s_hoveredIdx` Hide 时已 reset (cpp:1105), 因此第二次 Show 第一 tick `GetCursorPos` 立即取新 cursor → 自然更新 → **不会** 因为 timer restart 引入 hover 闪烁 (poll interval 100ms 内已是稳定状态)
- 因此 #4 是 safe 的, ship 风险低

**Falsification #4 — 1.C 推荐 ship 的"F-1 用 grace=5s" 测试不足以验证 primary**

1.C.7 F-1: 把 `kShowGraceMs = 5000` 验证 panel hold >5 秒 → 证 cause 1。

我**反对**这 F-1 单独使用:
- 若 grace=5s panel hold 5 秒以上 → **只能**证明 grace 抑制了 L89 auto-hide, 不能区分 "cause 1 (cursor 在外)" 还是 "cause 2 (s_outsideMs 残留)"
- 真正的 falsification **必须**包含 F-2 (cold start first Show) — 1.C 自己提了 (1.C.7 F-2), 但 1.C.8 推荐 ship 时**没**强制 F-2 必须先跑
- 我要求 1.C 在 ship 前**至少**同时跑 F-1 + F-2 + F-3 三个 checks, 排除 cause 1 / 2 / 3 三个独立假设后再 ship

##### 1.B.7.3 对自己 (1.B primary) 的 defense

**F-5 (1.C.7 反向) 的延伸: "若修了 issue 3, 是否还需要担心 WM_TIMER 累加 outsideMs 的副作用?"**

1.C 可能会问: 1.B 修 issue 3 后, issue 4 (WM_TIMER 累加 outsideMs) 是否仍存在? — 答:**独立存在**, 1.B 不涉及任何 timer 逻辑, 1.B ship 完全不影响 L89 auto-hide 行为。

但反过来 — **1.C ship 后是否会让 issue 3 复现率↑**? 见 1.B.7.2 Falsification #2 论证, **会**。因此 1.B 必须 ship。

**Defense #1 — L88-fix 不可回退**

1.A/1.C 可能会推 fix B (1.B.5 Fix B): "只在 hit==-1 时启动 drag"。

我**反对** Fix B, 理由:
- L88-fix 设计目标 (git log: v0.19.0.20 commit cf3dfe4e "drag any area + btn gap 加大"): user 在 button 内也能拖 panel
- Fix B 直接回退这条设计, user 报告 "L87 button 内无法拖动" 的痛点必然复现
- L88-fix 的 user 期望是 "**任意位置都能 drag panel**", 这是 v0.19.0.20 ship 时 user 认可的
- 回退 = 牺牲已知好行为换 issue 3 fix, **trade-off 严重不对称**

**Defense #2 — Fix A 是真正 minimal, 不引入 state regression**

1.A 可能会问: "Fix A 是 'drag 后松手 active→无的 transition', 视觉上不够优雅, 是否应改 Fix C (click/drag 拆开用 SM_CXDRAG)?"

我**坚持** Fix A (1.B.5 Fix A, 1.B.8 推荐), 理由:
- Fix A 1 行 code, fix 1.B.4 cause 1/2/3 全部三个 chain (逻辑上 drag 结束 reset active idx = 不论 drag 是否真移动 window)
- Fix C 状态机重写, 涉及 WM_MOUSEMOVE 路径 (cpp:460-487), 引入新 edge case (drag 出 threshold → 回原位松手 → 是否 click?), 单独 spec + 测试覆盖
- Fix A 不改 L88-fix 设计意图, 保留 user 已认可行为
- "视觉闪一下消失" 是 acceptance issue, 通过 spec 041 加一个 TestQuickPanel grace-after-LButtonUp paint test 验证, ship 时已知

**Defense #3 — Fix A 对 L89 auto-hide 行为零影响**

补充防御点: 1.C 担心 1.B 修改影响 L89 timer。
- 1.B 的修改**只**在 cpp:493 (LButtonUp if-d 分支加一行 `s_activeIdx = -1`)
- cpp:493 与 cpp:522-548 (WM_TIMER) **不**共享变量, 不共享 paint path, 不共享 state 初始化
- 1.B 修改**完全不影响** `s_outsideMs` 累加逻辑、HitTest 行为、Hide()/Show() 调用
- 因此 1.B ship 不会破坏 L89 auto-hide, 也不会破坏 1.C 修法 #1/#4

**Defense #4 — 自证伪测试已写好**

为 defense, 我的 primary 自带 stronger falsification (1.B.7 F4): 即使 user 完全不拖动, 只 click button, **必中 bug** (因为 L88-fix 无条件 drag 启动, click 必走 if-d 分支)。
- 这等于在写代码前已经证伪了"LButtonUp 只在真 drag 时才进 if-d"的反假设
- ship 后 validation test: click button 不动 → 验证 active bg 在 LButtonUp 后消失, **1 test** 就能 lock issue 3 修复

##### 1.B.7.4 跨切面 insight — ship 顺序与组合

| 改动 | 涉及文件:行 | 行为影响 | ship 顺序 |
|---|---|---|---|
| 1.A primary (y0=5 → y0=6) | cpp:716-727 | 仅画按钮 y 偏移, 不动 state machine | **独立 ship** (`fluxing-0.19.0.24a`) |
| 1.A primary (kBtnGap 11 → 14, fixed panelW=277) | cpp:721-725 + h:135 | 仅水平 padding | **独立 ship** (同 0.24a) |
| 1.B Fix A (LButtonUp if-d reset `s_activeIdx`) | cpp:493 (加一行) | 仅清 active idx, 不影响 drag / hover / timer | **必须**与 1.C 同步 ship (`fluxing-0.19.0.24b`) |
| 1.C #1 (Show grace period 2 秒) | cpp:1095 + cpp:535-543 | 抑制 L89 auto-hide 触发, 不影响 drag / paint | **必须**与 1.B 同步 ship (同 0.24b) |
| 1.C #4 (Hide reset s_outsideMs + 复用路径 restart timer) | cpp:1099-1107 + cpp:1048-1055 | 清 outsideMs + 重启 polling | **强烈建议**与 1.B/1.C #1 同步 ship (同 0.24b) |

**冲突矩阵**:
- 1.A 修改 cpp:716-727 (几何), 1.B 修改 cpp:493 (state), 1.C 修改 cpp:535-543 / cpp:1095 / cpp:1099-1107 / cpp:1048-1055 (timer)
- **完全不重叠**, git diff 应干净, 不会 conflict
- 1.A 改动 `kPanelPadding` 风险 (1.A.3 方案 D): 若 1.A 走方案 D, cpp:716-725 + h:135 都动, **可能**与 1.B/1.C 的 timer 修改语义无关, 但需要 1.A 验证 `HitTest` (cpp:387-392) 仍正确 — 1.A §1.A.6 check #1 已涵盖

**ship 顺序推荐**:
1. **`fluxing-0.19.0.24a`**: 1.A primary (issue 1+2), 单独 PR, TestQuickPanel 像素 dump 验证
2. **`fluxing-0.19.0.24b` (hotfix)**: 1.B Fix A + 1.C #1 + 1.C #4 (issue 3+4), **必须 3 处同步改, 一个 commit**, TestQuickPanel 加:
   - click-button-no-drag: LButtonUp 后 active bg 消失 (1.B 验证)
   - grace-period-hold: Show 后 2.5 秒 panel 仍可见 (1.C #1 验证)
   - reuse-path-timer-restart: 第二次 Show hover 能更新 (1.C #4 验证)
3. **不 ship** 的 alternatives: 1.A 替代 E / 1.B Fix B / Fix C / 1.C #2 / #3 (保留 spec 041 future)

**理由**:
- 1.A 是视觉 polish, 不阻塞 user 关键路径 (用户能输入中文), 可以单独 ship
- 1.B + 1.C 是关键路径修复 (issue 3 影响视觉感知但功能可用, issue 4 让 panel 完全没法用 — 1.C.9 "功能性 0 容忍")
- 1.B 必须配 1.C, 1.C 也必须配 1.B (理由 1.B.7.2 Falsification #2 — grace 期间 user 跟 panel 交互频率↑, issue 3 暴露↑)
- 1.C #4 是顺手, 不 ship 也行, 但 ship 修复 timer latch 是低成本高收益

**最终立场**:
- **接受** 1.A primary (`y0=6` + `kBtnGap=14`) — 修 issue 1+2 独立 ship
- **接受** 1.C primary (Show grace period 2 秒) — 修 issue 4 必 ship, **必须**与 1.B 同时
- **坚持** 自己 primary (Fix A, LButtonUp if-d `s_activeIdx = -1`) — 修 issue 3 必 ship, **必须**与 1.C 同时
- **拒绝** Fix B (回退 L88-fix drag any area)
- **拒绝** Fix C (重写 WM_MOUSEMOVE, risk 高) — 留给后续 refactor spec

**关键 falsification check (我提供的)**:
**Check F4 (1.B.7) — "click button 不动 cursor, LButtonUp 后 button 永久橙"**:
- 这是 issue 3 的**最确凿**反向 falsification (drag 阈值无关论据)
- 验证方式: 在 `PaintOpaqueContent` 入口 (cpp:638) 加 `OutputDebugStringW` 打印 `s_activeIdx`, **测试纯 click 不动 cursor 路径** (cpp:441→cpp:488, 期间 cpp:460 WM_MOUSEMOVE 不触发或触发 1-2 次但 dx=0)
- 期望: `s_activeIdx` 在 LButtonDown → 0, 在 LButtonUp 后应 = -1 (Fix A ship 后)
- 实际 (Fix A 未 ship): `s_activeIdx` 在 LButtonUp 后 = 0 残留
- 这是 ship 前后**唯一需要**的 1-test regression, 3 行 code (1 行 fix + 2 行 test) 即可 lock issue 3 修复

---

### 1.C Hypothesis: 生命周期 (Agent C 负责 issue 4)

#### 1.C.1 问题陈述 (issue 4)

user v0.19.0.23 ship 后仍报: **第一次点击快捷键调出设置栏,但设置栏很快消失的 bug 没有修复**。
本段调研 QuickPanel 生命周期 (create → show → timer → hide → re-show),聚焦于
"为什么 panel 在 hotkey 调出后会 fast-hide"。

#### 1.C.2 L89-fix auto-hide 现状 (`WeaselServer/QuickPanelDialog.cpp:522-548`)

WM_TIMER w==2 分支 (id=2 hover polling, **同时** 兼 auto-hide):

```cpp
case WM_TIMER: {
  if (w == 2) {                                                   // cpp:523
    POINT p;
    if (GetCursorPos(&p) && ScreenToClient(hwnd, &p)) {           // cpp:525
      int hit = HitTest(p.x, p.y);
      ...
      if (hit == -1 && !s_dragging) {                              // cpp:535
        s_outsideMs += 100;                                        // cpp:536
        if (s_outsideMs >= 1500) {                                 // cpp:537
          s_outsideMs = 0;
          Hide();                                                  // cpp:539
        }
      } else {
        s_outsideMs = 0;                                           // cpp:542
      }
    }
    return 0;
  }
  return OnTimer(hwnd, w);
}
```

要点:
- 100 ms tick `SetTimer(s_hwnd, 2, 100, NULL)` 在 `Show()` (cpp:1095) 启动。
- 每 tick `GetCursorPos` → 物理 client 坐标 → `HitTest`。
- `hit == -1 && !s_dragging` → `s_outsideMs += 100`,1500 ms 阈值即 `Hide()`。
- 任何 `hit >= 0` 或 `s_dragging == true` 路径 → `s_outsideMs = 0`。
- `HitTest` (cpp:370-395) 出 panel 或在 panel 圆角外即返 `-1`。

#### 1.C.3 Show() 与 Hide() 启动/重置序列 (cpp:1042-1107)

`Show()` 两条路径(关键差异):
- **复用路径** (cpp:1048-1055): `if (s_hwnd) { ShowWindow(SW_SHOWNOACTIVATE);
  SetWindowPos(HWND_TOPMOST); RepaintLayered(); return; }` — **不重启 timer
  (id=2)**。
- **新建路径** (cpp:1077-1096): `CreateWindowExW` → `ShowWindow` →
  `SetWindowPos(HWND_TOPMOST)` → **`SetTimer(s_hwnd, 2, 100, NULL)`** (cpp:1095)
  → `RepaintLayered`。第一 tick 100 ms 内立即排队。

`Hide()` (cpp:1099-1107):
```cpp
void QuickPanelDialog::Hide() {
  if (s_hwnd && IsWindow(s_hwnd)) {
    KillTimer(s_hwnd, 1);                                          // cpp:1101
    KillTimer(s_hwnd, 2);                                          // cpp:1102 ← 停 auto-hide
    ShowWindow(s_hwnd, SW_HIDE);
  }
  s_hoveredIdx = -1;                                               // cpp:1105
  s_activeIdx  = -1;                                               // cpp:1106
  // 注: s_outsideMs **没有被重置**
}
```

#### 1.C.4 issue 4 cause chain(≥3 种)

##### Cause 1 — PRIMARY: Show() 后 cursor 立即在 panel 外 → 1500 ms 内 Hide

时间线(hotkey 调出时 user cursor 在文本输入框):
- t = 0:user 按 hotkey → WeaselServer 调 `Show()`,`SetTimer(2, 100)` 启动 polling。
- t ≈ 0:panel 在屏幕右下角创建(`workArea.bottom - kPanelH - 12`,
  即距离 taskbar 上沿约 60 px)。
- t = 100..1500 ms:每 100 ms tick,`GetCursorPos()` 取用户原 cursor 位置(在文本
  输入框中,距 panel 几百 px),`ScreenToClient` → `HitTest == -1`。
- t ≈ 1500 ms:`s_outsideMs == 1500` → `Hide()`。
- t = 1500..1700 ms:用户视觉上看到 panel 闪一下就消失。

**根因**:L89-fix 的设计假设是 "panel 显示后用户会立刻移 cursor 进 panel"。
但 hotkey 调出场景下,cursor 仍停留在用户上一次操作的文本输入框附近。panel 出现
位置 (屏幕右下) 与 cursor 当前位置 (文本框中) 距离可能 >500 px,`HitTest == -1`
持续 1500 ms,触发 auto-hide。这是 L89 "panel 卡输入区" fix 的副作用,L89
只解决了 "panel 阻挡" 但没考虑 "show 后 cursor 还未动"。

##### Cause 2 — ASSIST: s_outsideMs 残留

`Hide()` (cpp:1099-1107) 没有 reset `s_outsideMs`,第二次 `Show()` 后:
- WM_TIMER 重启,第一 tick (100 ms 内) `cursor` 仍在 panel 外 → `s_outsideMs += 100`。
- 残留值可能接近 1500 (例如用户停留 panel 较久,out 了 1400 ms 但 timer 还没
  凑够到 1500 就被手动 Hide),第二次 Show 又累加 100 → 立即 Hide。

单独这条 cause 不能解释"**第一次** Show 就出 bug",但协助 cause 1 在重复
调出场景下放大。`Hide()` 加 `s_outsideMs = 0;`(cpp:1100-1107 之间)是 simple fix。

##### Cause 3 — LATENT (但独立于 issue 4): 复用路径 timer 未重启

`Hide()` KillTimer(2) 停止 polling;复用路径 `Show()` (cpp:1048-1055) **不重启
timer**。意味着:
- 第二次 Show 时窗口可见,但 polling 已死。
- `s_hoveredIdx` 永远停在 Hide 前的值,hover 视觉不更新。
- auto-hide 也不工作 — 这**反向**让 issue 4 难调:panel 显示但**不** auto-hide,
  user 报告的"闪一下就消失"实际是第一次的 cause 1,复现路径不对。

修法:复用路径增加 `KillTimer(s_hwnd, 2); SetTimer(s_hwnd, 2, 100, NULL);`。

#### 1.C.5 ≥3 种修法候选

##### 修法 #1 (推荐,最小侵入): Show() 启动 grace period

```cpp
// 在 cpp:1095 之前/Show() 入口增加
static DWORD s_showTick = 0;
s_showTick = GetTickCount();

// WM_TIMER w==2 分支 (cpp:535 之前) 增加:
constexpr DWORD kShowGraceMs = 2000;
if (hit == -1 && !s_dragging) {
  if (GetTickCount() - s_showTick >= kShowGraceMs) {
    s_outsideMs += 100;
    if (s_outsideMs >= 1500) { s_outsideMs = 0; Hide(); }
  }
  // grace period 内:不累加 outsideMs,不 Hide
} else {
  s_outsideMs = 0;
}
```
- 优点:2 处修改,改动面最小,语义直接。
- 缺点:2 秒硬编码;若 user 2.5 秒才动 cursor 仍触发,需要平衡 grace vs 1500。

##### 修法 #2: 等 cursor 第一次进 panel 再开启 auto-hide

```cpp
static bool s_everInside = false;
// Show() 末尾: s_everInside = false;
// WM_TIMER w==2:
if (hit >= 0) { s_everInside = true; s_outsideMs = 0; }
else if (s_everInside && !s_dragging) {
  s_outsideMs += 100;
  if (s_outsideMs >= 1500) { s_outsideMs = 0; s_everInside = false; Hide(); }
}
```
- 优点:语义干净,"用户进 panel 才关心离开"。
- 缺点:user 从未 hover panel 又一直不动 cursor 也会一直 Show(可能反而好);但
  user 看到 panel 不主动 hover 也不 Hide,行为反直觉(违反 macOS)。

##### 修法 #3: in-panel-ms 阈值(等用户停留 300 ms 才激活)

```cpp
// WM_TIMER w==2 累计 in-panel 时间,达阈值后才允许 outsideMs 计时
static int s_inPanelMs = 0;
if (hit >= 0) {
  s_inPanelMs += 100;
  s_outsideMs = 0;
  if (s_inPanelMs >= 300) { /* 激活 outsideMs 计时 */ }
} else {
  s_inPanelMs = 0;
  if (/* 已激活 */) {
    s_outsideMs += 100;
    if (s_outsideMs >= 1500) { s_outsideMs = 0; Hide(); }
  }
}
```
- 优点:最贴近 macOS 习惯(进 panel 即开始计时)。
- 缺点:状态机分支多。

##### 修法 #4 (顺手): Hide() reset + 复用路径重启 timer

```cpp
// Hide() (cpp:1099) 末尾:
s_outsideMs = 0;
// Show() 复用路径 (cpp:1048-1055) 内:
KillTimer(s_hwnd, 2);
SetTimer(s_hwnd, 2, 100, NULL);
```
不直接解决 issue 4,但消除 cause 2 + cause 3 两个副作用。**强烈建议** 与
修法 #1 同步 ship。

#### 1.C.6 关于 "L93 中断尝试加了 `s_showTime` 字段但在 v0.19.0.23 我移除了它 — 为什么移除?"

**调查结论**:`s_showTime` 字段**从未进入 git history**(已用
`git log --all -p --pickaxe-regex -S"s_showTime"` 验证):
- 整个项目 (`*.cpp` / `*.h` / `*.md`) 仅 investigation.md §4 第 45 行提及
  (`移除 dead s_showTime 字段 (L93 中断尝试遗留,未使用)`)。
- v0.19.0.22 → v0.19.0.23 之间所有 commit (a8c4d801..48103ba5..17fa7845.. 等)
  没添加或删除 s_showTime,只是几何常量 (kPanelW/kBtnSize/kBtnGap 等) 与
  HitTest buttonStartX alignment。

因此"v0.19.0.23 我移除了 `s_showTime` 字段"是 **misremember**:
- "**移除**" 误解:没东西需要移除,字段从未存在。
- "**L93 中断尝试**" 误解:那次 plan / brainstorm 期间可能确实在 chat 里写过
  字段定义(用 `static DWORD s_showTime`、Show() 末尾 `s_showTime = GetTickCount()`、
  timer 比较 `GetTickCount() - s_showTime < 2000` 跳过累加 — 即修法 #1 的设计),
  但**没**真正落到 `.h`,**没** commit。
- v0.19.0.23 ship (a8c4d801) 仅动了几何常量 (kPanelW 360→277 等) 与
  WM_DPICHANGED handler,**未触碰** auto-hide 代码,也没添加 / 删除
  `s_showTime`。

**若 "L93 中断尝试" 真要 commit,可能的移除理由**:
1. dead code — 字段加了但 timer 里没人 `read` 它。
2. 编译错误 — 几乎不可能(GetTickCount() / DWORD 都是标准 Win32)。
3. 设计变化 — 改用别方案(例如 cursor-first-enter 修法 #2)。

**最可能**:那是一次 **mid-implementation draft**,从未 commit。v0.19.0.23
重写 L93 常量时整体重写相关代码块,draft 字段被"自然覆盖"(但从未被 commit)。
investigation.md §4 第 45 行用 "L93 中断尝试遗留" 措辞实际是 **decorative
retrospection** — 没证据。

#### 1.C.7 Falsification checks (≥3 个)

1. **F-1 (直接验证)**: 把 `kShowGraceMs = 5000`(够长),观察 panel 是否
   hold >5 秒。**Hold → cause 1 成立;Still Hide → 找别根因**。
2. **F-2 (排除 cause 2)**:重启 WeaselServer 后**第一次**按 hotkey(cold
   start,s_outsideMs 初始 0) 复现 issue 4。**Still reproduce → cause 2 不
   是主根因,cause 1 是**。
3. **F-3 (排除 cause 3)**:启动 panel,**不 Hide**,直接 ALT-TAB 切应用,
   触发 WM_ACTIVATEAPP (cpp:508) → Hide()。再切回应用按 hotkey Show。
   若复用路径下 panel 显示**不** auto-hide → cause 3 证实,顺手修 timer 重启。
4. **F-4 (排除 panel 出屏)**:ShowWindow 后立即 `GetWindowRect(s_hwnd, &rc)`;
   若 `rc` 完全在 `SystemParametersInfo(SPI_GETWORKAREA)` 外 → panel 在屏幕
   外→ user 看到"消失" 实际是从未显示过。这种情况 L92 已经 clamp
   (cpp:1072-1075),应不再出。
5. **F-5 (反向)**:若修法 #1 fix 后 5 秒 panel 仍 Hide,**grace 不够**
   或**根因不在 grace**;很可能 WM_DPICHANGED (cpp:423) 在 Show 后误触发
   进入什么分支?查 `SetWindowPos` 路径是否有意外 Hide call。

#### 1.C.8 Primary hypothesis & alternative

**Primary cause**:L89-fix (`QuickPanelDialog.cpp:531-540`) auto-hide 在 hotkey
调出场景下被立即触发,因为 cursor 在 hotkey 按下时停留在文本输入框(panel
右下角外),`GetCursorPos + HitTest == -1` 持续 1500 ms,`s_outsideMs` 阈值达成
自动 Hide。Show() 没留 grace period。

**Alternative hypotheses**:
- **Alt-1**:`s_outsideMs` 残留(Hide 没 reset)— assist cause,加重重复调出场景。
- **Alt-2**:复用路径 timer 没重启 — 独立 latent bug,与 issue 4 不同。
- **Alt-3**:WM_DPICHANGED handler 误触发导致意外 Hide。

**推荐 ship**:修法 #1 (grace period 2 秒) **+** 修法 #4 (Hide 重置 +
复用路径重启 timer),三处同步改,打 `fluxing-0.19.0.24`。Fix B 已经在 issue 3
那边 ship (Agent B 推的 LButtonUp reset)。

**Priority**:
1. Bug fix 主菜:修法 #1。
2. 顺手修:修法 #4 (低成本,修复两个 latent bug)。
3. 不动:修法 #2 / #3(行为反直觉,需要单独 spec + user validation)。

#### 1.C.9 不修的代价

issue 4 比 issue 3 更难掩盖:
- issue 3 (active bg 残留):用户在 panel 内仍能点击 + 视觉残留但不卡功能。
- issue 4 (fast-hide):用户**根本无法用** panel — 每次按 hotkey 都要立刻
  动 cursor 进 panel,否则 panel 闪退。功能性 0 容忍。

issue 4 必须进 hotfix,不能拖到下次 minor release。

#### 1.C.10 file:line 引用清单

| 引用 | file:line |
|---|---|
| WM_TIMER w==2 polling + s_outsideMs 累加 | `WeaselServer/QuickPanelDialog.cpp:522-548` (核心 cpp:535-543) |
| L89-fix 注释 (panel 卡输入区说明) | `WeaselServer/QuickPanelDialog.cpp:531-534` |
| HitTest 返回 -1 (cursor 出 panel) | `WeaselServer/QuickPanelDialog.cpp:372` |
| Show() 新建路径 `SetTimer(2, 100)` | `WeaselServer/QuickPanelDialog.cpp:1095` |
| Show() 复用路径**不重启** timer | `WeaselServer/QuickPanelDialog.cpp:1048-1055` |
| Hide() KillTimer(2) 但**没** reset s_outsideMs | `WeaselServer/QuickPanelDialog.cpp:1099-1107` |
| s_outsideMs 字段声明 | `WeaselServer/QuickPanelDialog.h:125` |
| s_outsideMs 默认 0 | `WeaselServer/QuickPanelDialog.cpp:170` |
| ShowWindow(SW_SHOWNOACTIVATE) 启动次序 | `WeaselServer/QuickPanelDialog.cpp:1087` |
| SetWindowPos(HWND_TOPMOST) explicit 置顶 | `WeaselServer/QuickPanelDialog.cpp:1091-1092` |
| WS_EX_NOACTIVATE / WS_EX_LAYERED 影响 input | `WeaselServer/QuickPanelDialog.cpp:1078` |
| WM_ACTIVATEAPP 触发 Hide() | `WeaselServer/QuickPanelDialog.cpp:508-513` |
| L92-fix position clamp (workArea 内) | `WeaselServer/QuickPanelDialog.cpp:1072-1075` |
| WM_DPICHANGED handler | `WeaselServer/QuickPanelDialog.cpp:423-440` |
| L89-fix design intent (lessons-learned) | L89 entry in `.specify/memory/lessons-learned.md` |

#### 1.C.11 总结

| 项 | 内容 |
|---|---|
| Primary cause | Show() 后 cursor 在 panel 外 → L89 auto-hide 立即触发 → panel flash-hide |
| Primary fix | Show() grace period (修法 #1,2 秒) + Hide reset + reuse-path timer restart (修法 #4) |
| Alternative | s_outsideMs 残留 / 复用路径 timer 死 / WM_DPICHANGED 误触 |
| 推荐 ship | `fluxing-0.19.0.24` hotfix,3 处修改 + TestQuickPanel 加一个 grace 测试 |
| Risk | grace vs 1500 阈值需平衡;2 秒太短 user 没动 cursor 仍 fast-hide |


#### 1.C.12 Round 2 — Falsifications & Defense (Agent C)

##### 1.C.12.1 对 1.A (几何) 的 falsification

**F-1A-A (关系正交性,强)**: 1.A 改 `y0` (垂直 layout) 与 1.C 的 `Show()` grace
period (生命周期) 在代码路径上完全无交集。1.A 触及 `PaintOpaqueContent` 内
的 button 几何 (`WeaselServer/QuickPanelDialog.cpp:716-727` +
`774-806`),1.C 触及 `WM_TIMER id=2` 的 `s_outsideMs` 累加
(`cpp:535-543`) + `Show()/Hide()` 边界 (`cpp:1048-1107`)。两者**不共享任何
变量或分支**。结论:1.A 的 ship **不会** regression 1.C 的修法 #1/#4;反之
亦然。两个 fix 可以并行入同一 hotfix 而无 conflict。1.A 的 primary
(改 `y0=6`) 与 1.C 的 primary (grace 2s) 是**正交修复**。

**F-1A-B (潜在 paint/hit 错位,弱)**: 1.A §1.A.3 primary 方案 A 自身
acknowledged "需同步 HitTest 的 Y 范围" (`QuickPanelDialog.cpp:387-392`)。
若 1.A 改 `y0=6` 但忘了同步 HitTest 的 Y 上限,会出现 paint region 与 hit
region 不一致 — 视觉上按钮在 `[6,41)`,但 click 命中条件仍是 `[5,40)`。
用户感知是 "按钮看起来在某个位置但点不到" / "点空白也能命中"。**但这跟
1.C 的 issue 4 无关**:issue 4 是 panel 整体 1500ms 内 Hide,与 click 命中区
错位正交。1.C **接受** 1.A 的 primary,但 1.A ship 时必须 paint + hit 同步
(否则 issue 1 修复但引入新 bug,被 user 报为"按钮点不到" — 不是 1.C 的责任,
但 user 会混为一谈)。

**F-1A-C (highlight/shadow 改动的可证伪性)**: 1.A §1.A.3 alternative E
(调 highlight/shadow 厚度) 的核心命题是 "不改 panel/按钮尺寸,通过移动
visual line 平衡上下视觉重量"。1.C 的 falsification:**不能证伪 1.A 的
alternative E 与 issue 4 无关**,因为 1.A 改 highlight 高度 0/1/2 px 都**不
改变 `panelH=48` 与 `HitTest` Y 范围**,所以**不**影响 paint/hit 一致性,
**不**影响 1.C 的任何代码路径。这是 weak ack — 1.A 怎么改都不会 regression
1.C。

**结论**: **我接受 1.A 的 primary** (改 `y0=6` 居中 + 同步 HitTest + gap=14),
与 1.C 正交可并行 ship。

---

##### 1.C.12.2 对 1.B (状态机) 的 falsification

**F-1B-A (click 是否 fire,核心疑问)**: 1.B primary 是 `WM_LBUTTONUP if-d 分支`
(`WeaselServer/QuickPanelDialog.cpp:488-494`) 缺 `s_activeIdx = -1`,导致
active bg 残留。1.B 推荐 Fix A (1 行 reset)。**1.C 的关键问题**:Fix A 修好后,
**click 事件真的 fire 了吗**?

回溯代码路径(`cpp:441-505`):
- LButtonDown (`cpp:441-459`) → `s_dragging = TRUE` (cpp:449, L88-fix 无条件)
  + `s_activeIdx = hit` (cpp:455)
- LButtonUp (`cpp:488-505`):
  - 进入 `if (s_dragging)` 分支 (cpp:489) — **永远进入**,因为 L88-fix 无条件
    设了 TRUE
  - `s_dragging = FALSE; ReleaseCapture(); s_hoveredIdx = -1;`
  - **不**调用任何 onClick / dispatch
- `else` 分支 (`cpp:496-503`) 才有 "5 按钮 no-op (spec 070 T007)" 注释 —
  这意味着即便走 else 分支,目前的代码也**不 fire** 任何 click 行为

**结论**:当前代码**完全没实现** button click action。issue 3 user 反馈
"按钮背景色变橙色,但鼠标松开点击后,背景色不恢复" 真实路径是:
LButtonDown 看到 `s_activeIdx` 变 → 画橙 bg → LButtonUp 走 if-d 分支 →
**click 没 fire** (本来就没实现) + active bg 残留。

1.B Fix A 修好后:click 仍然**不 fire** (因为根本没 click handler),但
bg 会正确恢复 — 视觉上"button 按下/松开"流程闭合,user 不会感知到 bug。

**1.C 反对 1.B 的"primary = if-d 分支不 reset s_activeIdx"** 不完整表述 —
应当说 "primary = (state 残留 active bg) AND (click handler 缺位)"。这是
双重根因,1.B 只修了一半。

**F-1B-B (LButtonUp 永远走 if-d 而非 else,语义错位)**: 1.B 描述 "LButtonDown
hit==0 → LButtonUp 走 if-d 分支 → `s_activeIdx` 永驻"。1.C 追问:**user
完全不动 cursor 的纯 click**,LButtonUp 也走 if-d 分支吗?

确认 (`cpp:488`): 进入 if-d 分支只看 `s_dragging`,**不**看是否真的 drag。
L88-fix 让 `s_dragging=TRUE` 无条件,所以**任何 LButtonUp** 都走 if-d 分支,
除非在 LButtonDown 与 LButtonUp 之间某处把 `s_dragging` 置 FALSE。

回查代码:`s_dragging = FALSE` 只在 `WM_LBUTTONUP if-d 分支` (cpp:490) 出现。
所以 LButtonDown → LButtonUp 之间 `s_dragging` 始终是 TRUE,**任何 click
都走 if-d 分支**,else 分支的 no-op 永远不被触发。

1.B §1.B.7 Check F4 (CONTROL) 也确认了这一点:"即使 user 完全不拖动,
只要他点 button,**必中 bug**"。但 1.B 没指出这是**click 整体缺位** —
Fix A 修 active bg 后,user 仍然感知"按了按钮没反应",只是不会有视觉残留。
issue 3 的"按下变橙"是视觉表征,user 真正想要的是 "按下按钮**做点啥**"。
若 button 当前是 no-op,Fix A 修了视觉但不修功能。

**F-1B-C (drag 启动覆盖 click 的吞没)**: 1.B Fix A 修好 `s_activeIdx` 后,
drag 启动仍然吞 click (因为 LButtonUp 永远走 if-d,不走 else 的 no-op)。
1.B 的 Fix C (用 `SM_CXDRAG` 阈值判定 click vs drag) 是真修复,但 1.B 自己也
acknowledged "需要单独 spec 和测试覆盖" — 不在当前 hotfix 范围。

**结论**:
- **我接受 1.B primary** 的 active bg 残留根因(状态机 bug),**接受** Fix A 作为 hotfix。
- **但 1.B 应当补充**:click handler (else 分支 no-op) 是 spec 070 已知限制
  (`QuickPanelDialog.cpp:496-503` 注释 "5 按钮 no-op"),不在 issue 3 修复
  范围。issue 3 user 文字 "点击按钮后背景色变成橙色" 的真实诉求 = "按下/松开
  视觉闭环",Fix A 满足此诉求。
- 1.B primary 不影响 1.C。1.B 改 `cpp:494`,1.C 改 `cpp:535-543` + `cpp:1048-1107`,
  无交集。

---

##### 1.C.12.3 对自己的 defense

**D-1C-A (反驳 "grace 与 L88 drag 冲突")**: 潜在反驳:"1.C 改 Show grace
会跟 1.B L88 drag-any-area 冲突,因为 drag 启动会让 grace 失效"。

反驳:1.C grace 修法 #1 (§1.C.5) 在 `WM_TIMER id=2` 分支 (cpp:535) **之前**
增加 `GetTickCount() - s_showTick >= 2000` 检查,**不**影响 drag 启动路径
(cpp:441-459)。drag 启动由 LButtonDown 触发,与 timer polling 是两条独立
事件链。即使 grace 期间有 LButtonDown:
- LButtonDown: `s_dragging = TRUE`, `s_activeIdx = hit`, `SetCapture`
- 后续 timer tick: `hit == -1 && !s_dragging` 判断 — `s_dragging==TRUE` 走
  else 分支 (cpp:541-543) → `s_outsideMs = 0`,**不**走 grace 检查路径
- 即 grace 检查只在 `!s_dragging && hit==-1` 才生效

drag 期间 grace 不生效是**正确**的(user 拖动 panel 时不需要 grace 保护)。
1.C grace 修法与 1.B L88 drag 是**互补正交**改动,无冲突。

**D-1C-B (反驳 "永不 auto-hide")**: 潜在反驳:"1.B 可能说 1.C 修法 #2 (首次
Show 不启动 auto-hide) 让 panel 永不自动 Hide,违反 L81 L89 设计意图"。

反驳:
1. 1.C primary 推荐的是**修法 #1 (grace 2s)** 而非修法 #2。修法 #1 保持
   L89 auto-hide 1500ms 阈值,只是延迟激活 — 2s grace 结束后,正常
   auto-hide 逻辑生效。这是**最小侵入**的 fix。
2. 1.C §1.C.5 明确把修法 #2/#3 列为"行为反直觉,需要单独 spec + user validation"
   — **不**推荐当前 hotfix ship。这与 1.B "user 期望永不 auto-hide" 假设无关。
3. 即使 1.B 想推"永不 auto-hide",这是 L89 的设计回退,**不是** 1.C 的
   falsification。1.B 想动这个,应当另起 spec (影响 macOS 习惯 vs 当前
   behavior),不是修改 1.C 的方案。
4. L89 design intent (lessons-learned L89) 是 "panel 卡输入区 → auto-hide",
   1.C grace 修法**保留**这个 intent,只是给了 2s 缓冲让 hotkey 调出场景
   不触发误判。

---

##### 1.C.12.4 跨切面 insight:三个 issue 是否独立,ship 顺序

**独立性判定**:

| Issue | 1.A (几何) | 1.B (状态机) | 1.C (生命周期) | 共享代码 |
|---|---|---|---|---|
| 1 (按钮偏上) | **A.1 primary** | 无关 | 无关 | PaintOpaqueContent (1.A) |
| 2 (gap+3px) | **A.1 primary** | 无关 | 无关 | PaintOpaqueContent (1.A) |
| 3 (active bg 残留) | 无关 | **B.1 primary** (Fix A) | 无关 (Hide 会 reset `s_activeIdx`,但 issue 3 发生在 Show 期间) | WM_LBUTTONUP (1.B) |
| 4 (fast-hide) | 无关 | 无关 | **C.1 primary** (Fix #1+#4) | WM_TIMER id=2 + Show/Hide (1.C) |

**结论**:**三个 issue 完全是独立 bug**:
- issue 1+2 = 几何 layout (paint-time)
- issue 3 = 状态机残留 (event-time)
- issue 4 = 生命周期时序 (event-time + boundary)

**是否有统一根因?** 1.C 自我审视:**没有**统一根因。
- issue 1+2 是 paint 时的几何常量错配。
- issue 3 是 event handler (`WM_LBUTTONUP`) 的状态机分支遗漏。
- issue 4 是 lifecycle 边界 (`Show()` → `WM_TIMER`) 的初始化不全。
三者触及不同函数、不同变量、不同调用栈。唯一的"共性"是**L93 落地时
multi-fix 一起 ship,各 fix 自测通过但 cross-fix 集成测试缺位**,user
报告的是累积 bug,而非耦合 bug。

**潜在 false unified root cause**:
- "Show() 之后所有 timer/state 没有完全 reset" — 这能 cover issue 4 的
  cause 2 (`s_outsideMs` 残留) + 部分 issue 3 (`s_activeIdx` 在 Show 后
  应该 reset,但 `Hide()` 已经 reset,所以 issue 3 的残留来自 LButtonUp
  路径而非 Show 路径)。但**不能** cover issue 1+2 的几何 layout。
- 即使将 issue 3 + 4 归为 "Show/Hide 边界 reset 不全",issue 1+2 仍是
  paint 几何问题,**无法**用统一根因描述。

**Ship 顺序建议**:
1. **fluxing-0.19.0.24 hotfix** (本次,必须): 三个 issue 一起 ship:
   - 1.A Fix: 改 `y0` + 同步 HitTest,加 button gap +3
   - 1.B Fix A: LButtonUp if-d 分支加 `s_activeIdx = -1`
   - 1.C Fix #1: Show() grace period 2s
   - 1.C Fix #4: Hide() reset `s_outsideMs` + 复用路径 restart timer
   - 一起 ship 因为:① 三个 fix 无冲突;② user 反馈四个 bug 一起报出,
     一个 hotfix 一起解决,避免分批 ship 让 user 反复验证;③ 三个 fix
     各自 < 5 行,合并 PR review 压力低
2. **后续 spec (fluxing-0.19.x.x)**: 1.B Fix C (drag-threshold 状态机拆分),
   这是 click handler 重构,需要单独 spec + 测试覆盖 (涉及 WM_MOUSEMOVE
   路径 + `SM_CXDRAG` 阈值 + 5 按钮 click dispatch 设计),**不**进 hotfix。
3. **不进 hotfix**: 1.C 修法 #2/#3 (行为反直觉,需要 user validation)。

**测试覆盖建议** (进 hotfix):
- TestQuickPanel 加 grace 测试: Show() → 不动 cursor 1500ms → 期望 panel **仍可见**
- TestQuickPanel 加 active reset 测试: LButtonDown button0 → LButtonUp → 期望
  `s_activeIdx == -1` 且 paint 中 button0 bg != active bg
- TestQuickPanel 加 gap 测试: 改 `kBtnGap=14` 后,button0/1/2/3/4 x 区间符合
  §1.A.4 公式

---

##### 1.C.12.5 立场总结

| 项 | 立场 |
|---|---|
| **接受 / 反对 1.A primary** | **接受** (改 `y0=6` + 同步 HitTest + gap=14) |
| **接受 / 反对 1.B primary** | **接受** (Fix A: LButtonUp if-d 分支 reset `s_activeIdx`);补充:click handler 缺位是 spec 070 已知限制,不在 issue 3 范围 |
| **自我 primary** | 修法 #1 (grace 2s) + 修法 #4 (Hide reset + 复用路径 restart timer) |
| **统一根因** | **不存在** — 三 issue 触及 paint / event / lifecycle 三个独立维度 |
| **Ship 顺序** | fluxing-0.19.0.24 hotfix = 1.A Fix + 1.B Fix A + 1.C Fix #1+#4 |
| **最关键 falsification check** | **F-1C-F1**: 把 `kShowGraceMs = 5000`,观察 panel 是否 hold >5s 不 Hide。这是**直接验证** cause 1 (Show 后 cursor 外 → 1500ms 内 Hide) 是否为 issue 4 主根因的最强证据。Hold → primary 证实;Still Hide → 找别根因 (可能 WM_DPICHANGED 误触,见 §1.C.7 F-5) |


## 2. Round 2 — 互相证伪

### 1.A.8 Round 2 — Falsifications & Defense (Agent A)

**前提回顾**: 1.A primary 推 `y0=6` (issue 1) + `kBtnGap=14`, `panelW=277` (issue 2)
(`QuickPanelDialog.h:126-161`, `QuickPanelDialog.cpp:716-727`, `QuickPanelDialog.cpp:789-800`)。
本节做三件事: (i) 挑战 1.B 的状态机主因; (ii) 挑战 1.C 的生命周期主因;
(iii) 防他人攻击 1.A。

---

#### 1.A.8.1 对 1.B 状态机主因的 falsification (≥2)

**F-B1 (Agent A 对 1.B 的 Check F4 的反向检验)**:
1.B §1.B.7 Check F4 (`QuickPanelDialog.cpp:488-505`) 自我宣称"完全静止不拖动场景
下复现 issue 3": user 点 button0 → 不动 cursor → 松手 → 走 LButtonUp if-d 分支 →
`s_activeIdx` 永驻 → button0 永久橙。1.B 把这条**当 primary 最确凿的证据**。

但 Agent A 注意到: 在 Check F4 推演的第 4 步
(`QuickPanelDialog.cpp:541-543` polling timer 进入 else 分支因为 `hit==0>=0`)
**会触发** `s_outsideMs = 0` + 100ms 后 `WM_TIMER id=2` polling 又读 cursor 进
button0 → `s_hoveredIdx=0`,所以"静止不拖动" 期间 polling timer **不会** idle
reset `s_activeIdx`,1.B 推演成立。✓ 这点 1.B 是对的。

**但 Agent A 的 falsification** 是: user 反馈说"**鼠标松开点击后,背景色不恢复**"
(`investigation.md:14` issue 3 截图证据)。中文里 "**点击**" 是非常明确的语义 —
"按下并立刻松开" 的动作,**不拖动**。1.B Check F4 的核心是这个"点 button 后
不动不拖动就立刻松手" 场景。

**Agent A 指出**: 这是 1.B 的最强论据 (Check F4),
**也是 1.B 最弱的论据**。理由:

- 如果 issue 3 真的纯由 L88-fix 副作用触发,那**所有点 button 的用户** 都会复现
  (因为 `s_dragging=TRUE` 无条件, `QuickPanelDialog.cpp:449`),100% 复现率。
- 但 user 是"**仍** 存在", 不是"**现在才有** 存在"。结合 timeline:
  L87 ("button 内长按无法拖动") 在 v0.19.0.18/19; L88 ("drag any area")
  在 v0.19.0.20 (`QuickPanelDialog.cpp:444-447` 注释);
  v0.19.0.20+.5 之前有一份 "fix": `s_dragging=TRUE` 之前,其实 LButtonUp 里
  的 activeIdx reset 是工作正常的 (走 else 分支, `QuickPanelDialog.cpp:501`)。
- **关键 1.B.6 节自己写**: "drag 期间跳过 hover 更新 (cpp:471)" — 这是
  **1.A 的领域**! L88-fix 的 side effect 不只是 LButtonUp 不 reset,还因为
  dragging 期间 hover 不更新 → 不能视觉提示 user "我已进入 drag 模式" →
  user 感知是"我点了 button", 实际是"我点了 button + 启动了 drag"。

→ **1.B 的 primary 是对 issue 3 行为表象的描述,但未排除
"issue 3 user 实际动作是拖动了几像素而非完全静止"**。User 截图说明无法
看出有无真实拖动。Check F4 在实验 build 内能精确复现 ≠ user 实际遭遇这条
路径。1.B 应当添加一条:**Check F4b**: 加 MoveAccumulator (`cpp:460-487` 的
`dx/dy` 累加, 默认 `SM_CXDRAG` 阈值) 输出 user 实际累计位移;若 user
实测 Δ > 阈值, 是 cause 1 不是 Check F4 那条"完全静止"路径。

**F-B2 (Agent A 对 1.B Cause 1 vs Cause 2 的边界)**: 1.B §1.B.4 列了
≥3 种 cause 链,其中 Cause 1 (drag 分支不 reset) 和 Cause 2 (LButtonUp 出
panel 是 cause 1 子集) 本质上是同一条路径的不同 user 叙事。Agent A 同意
这两条是同一根因,**但质疑 1.B 把 Cause 3 (re-enter) 也归到 primary**:

- Cause 3 是 Cause 1/2 的**后续效应**,不是独立 cause。
- 1.B §1.B.7 Primary hypothesis 包含 "**直到下一次 LButtonDown 覆盖**" —
  Agent A 指出: user 触发 issue 3 后**继续使用 panel** 时,下一次 click 任何
  button 都会让 `s_activeIdx=hit` (`cpp:455`) 覆盖前一个残留。所以
  "永久橙" 实际是"在第二次 click 前短暂橙"。Agent A 的 falsification:
  1.B 的"永久橙" 是过度归因; user 反馈的"不恢复" 大概率是**第二次 click
  在残留 button 上**, 而 `cpp:455` 在 `drag-any-area` 路径下总是覆盖
  → `s_activeIdx` 连续是同一个 idx,看上去就"橙一直有"。**真正的根因
  是 LButtonUp if-d 分支没 reset + 下一次 LButtonDown 时已经在 button
  内部 hit 同 idx**, 不是 s_activeIdx 永驻。

→ 1.B Fix A (在 if-d 分支加 `s_activeIdx = -1`) 是对的,
但 1.B 对症状的"用户感知模型" 应区分:
(i) 第一次 click → 静止松手 → 下次进入 panel → 看到橙 (覆盖路径)
(ii) 第一次 click → 静止松手 → 立刻 hover → 看到橙 (hover polling 不
touch active)

这两条 (i) vs (ii) 都是 drag-any-area 的副作用,但 (i) 是 user 实际
主要遭遇。

---

#### 1.A.8.2 对 1.C 生命周期主因的 falsification (≥2)

**F-C1 (1.C 忽略 L88-fix side-effect)**: 1.C §1.C.4 Cause 1 PRIMARY 假设
"Show() 后 cursor 立即在 panel 外 → 1500ms Hide"。Agent A 注意到
1.B §1.B.2 已经记录了 L88-fix (`QuickPanelDialog.cpp:449`) 让
`s_dragging=TRUE` **无条件**启动 — user 按 hotkey 调出 panel,
**第一次** 进 panel 时,如果 cursor 跨越 panel 边界进入,
WM_LBUTTONDOWN 会触发 (因为 panel `WS_EX_LAYERED` 但仍收 mouse)
→ `s_dragging=TRUE` + `SetCapture` (`QuickPanelDialog.cpp:448-449`)。

→ Agent A 指出:**1.C 完全忽略了 L88-fix 副作用**。user 第一次
"按 hotkey → panel 出现 → cursor 进 panel 边界 → 触发 LButtonDown",
现在 `s_dragging=TRUE`, `cpp:535` 的 polling timer 进入
"hit>=0 OR s_dragging" 的 else 分支 → `s_outsideMs = 0` (`cpp:542`)。
**auto-hide 永远不会累加!** 1.C grace period fix 是 **错的修法**, 因为
真根因是 L88-fix 让 user cursor 进 panel 时永远 `s_dragging=TRUE` → 屏蔽
auto-hide。如果 1.C 仅 ship grace period,2 秒 grace 结束后 user 仍
按住左键,cursor 仍 inside panel,`s_dragging=TRUE`,auto-hide 不进
accumulate。**user 看到的"闪退" 实际是因为 LButtonUp 后 `s_dragging=FALSE`
(`cpp:490`) → 立即 `hit==-1 && !s_dragging` (`cpp:535`) → `s_outsideMs`
重置到 0 但下次 tick 累加 100,1500ms 后 Hide**。

→ 1.C grace period 反而**会让 user 第一次 Show panel 永远不 hide**
(只要 LButtonDown 没松开; 但 user "show → 不点按钮 → 不动鼠标" 也是
个 corner case)。**1.C 应该添加 Check F-C-1A**: 临时修法
在 `QuickPanelDialog.cpp:535` 加 `&& !s_activeSpecified`
(`s_activeIdx != -1`); 如果 bug 消失,**L88-fix 是真根因**,1.C 的 grace
period 是次要。

**F-C2 (1.C Cause 3 "复用路径 timer 未重启" 不是 latent,是新 bug)**:
1.C §1.C.4 Cause 3 描述复用路径 `cpp:1048-1055` 不重启 `SetTimer(2,100)`,
认为这是**反向让 issue 4 难调**的独立 latent bug。

Agent A 反驳: user 反馈说 "**第一次**点击快捷键调出设置栏,很快消失" — user
实际遭遇的是"**首次** Show 后不久 Hide", 不是 "**第二次** Show 不再 auto-hide"。
1.C Cause 3 描述的复用路径 scenario 是 issue 4 的**反向**,不是 issue 4
本身。**如果 Cause 3 是真根因,user 应该说的是 "第二次按 hotkey 后 panel 不消失"**。

→ Agent A 指出 1.C 把不相关的 latent bug 拉进 primary, 分散了 fix 的清晰度。
1.C primary (Cause 1) 与 Cause 3 是**不同 issue**,不应一起 ship。Agent A
建议把 Cause 3 拆到 v0.19.0.25 修, v0.19.0.24 hotfix 只动 Cause 1+2。

---

#### 1.A.8.3 对 1.A primary 的 defense (≥2)

**D-A1 (1.B 可能的攻击: "你 1.A 改 y0 不解决 issue 3")**:
1.B 可能说: "1.A 把 y0 从 5 改 6,button region 整体下移 1 px,
但 user 截图里 button 已经'居中'了 — 你的 fix 只对 issue 1,不影响
issue 3 (active bg 残留)。"

**Agent A 的回应**: **我承认 — 1.A 的 primary 不解决 issue 3**。1.A section
1.A.1 自始就声明 "issue 1+2", 1.A 几何修法 与 issue 3 状态机问题**机制
独立**, 没有 conflict, 也没有 redundancy。两者应**并行 ship**:
- issue 1+2 fix → 改 `y0`/`kBtnGap` (`QuickPanelDialog.cpp:716-727`,
  `QuickPanelDialog.h:143`)
- issue 3 fix → 加 `s_activeIdx = -1` (`QuickPanelDialog.cpp:489-494` if-d 分支)

这正是 Round 3 应达到的共识 — multi-fix ship 互不干扰。1.B 的"1.A 应
cross-validate issue 3" 越界。

**D-A2 (1.C 可能的攻击: "y0=6 让 user 看着反而偏下")**:
1.C 可能说: "1.A primary y0=6 改了垂直 layout, 若 user 在 Show 之后立刻
看到按钮偏下 1 px (光感判断), 反而觉得更不对。"

**Agent A 的回应**:
1. 1.A §1.A.6 已经列出 **Falsification check #3**:
   "**单变量 y0 A/B**: 仅在实验 build 将垂直起点从 5 改 6, 保持 panelH,
   btnSize 全不变。若用户认为明显偏下, 1 px 下移方向错误, Primary 被证伪"。
   这就是 user-perception 验证, 1.A 主动 expose 给自己证伪。
2. 1.A §1.A.2 第 4 项已算: 相对 `[1,44)` 内容中心, button 改成 y0=6
   偏下 0.5 px (button center 23 vs 内容 center 22.5); 相对完整 48 panel,
   button 偏上 0.5 px (button center 23 vs panel center 24)。**所以
   y0=6 在**两个参考系**之间是**数学最优折中**, 不会"明显偏下"。
3. **若** user 反馈强烈反对 (极少见, 因为 1px 在 100% DPI 下肉眼难辨),
   1.A §1.A.3 Alternative E (改 highlight/shadow) 是 fallback。
4. 1.A §1.A.5 已记录 brand area: brand `35×35` 也走 `y0=pad=5`, 若只改
   button y0=6, **logo 与按钮**视觉错 1 px。Agent A 同步 ship fix 应当
   `buttonY = (panelH - btnSize) / 2` 公式同时应用到 logo destination。
   这是 1.A 自防御的 cross-check, 不依赖他人提醒。

---

#### 1.A.8.4 跨切面 insight: 4 个 issue 是否共享根因?

**3 个 issue 的 cause chain 共享"WS_EX_LAYERED + NOACTIVATE 系列**
**L82-L89 渐进 fix 残留"**:

| Issue | 直接表层根因 | L##-fix 残留 chain |
|---|---|---|
| issue 1 | button Y geometry 偏上 1.5 px | L82 (panel create) 没建立垂直 layout 公式, 一路 copy `y0=pad` |
| issue 2 | btnGap 不够 | L82 同上, 水平 layout 没 per-button padding 模型 |
| issue 3 | active bg 残留 | L88-fix (drag any area, `cpp:449`) + LButtonUp if-d 不 reset (`cpp:489-494`) |
| issue 4 | auto-hide 在 hotkey 后 1500ms 触发 | L89-fix (auto-hide) 不分 grace / steady-state |

**Agent A 主张**:

issue 1+2 **共享**: 几何 layout 没单独的 `buttonY` 公式, 与 `kPanelPadding`
耦合 (`QuickPanelDialog.cpp:716-727`)。一并 fix 合理。

issue 3 与 issue 4 **看似独立, 实际都源于 L88-fix 的 drag any area**:
- issue 3: drag 启动无条件 → LButtonUp if-d 多了一条路径 → reset 不对称
- issue 4: drag 中 `s_dragging=TRUE` (`cpp:449`) → auto-hide else 分支
  (`cpp:541`) 屏蔽 outsideMs 累加 → user 按 hotkey → cursor 进 panel
  触发 LButtonDown → 把 `s_dragging=TRUE` 撑住 → auto-hide 在 dragging
  期间永远不进 accumulate (但 LButtonUp 松手瞬间 `s_dragging=FALSE`
  (`cpp:490`)) → 接下来 1500ms 累加 → Hide

→ **issue 3 和 issue 4 的 "fix" 应配套 ship**:
- issue 3 修法 (1.B Fix A): if-d 加 `s_activeIdx = -1`
- issue 4 修法 (1.C 修法 #1 grace 2s) 反而**与 1.B 共因修复相反**, 1.C
  修法只挡 2 秒 grace, 不解决 L88-fix 的 s_dragging 副作用。

**Agent A 推荐**: issue 4 的真修法应该把 auto-hide 的 dragging 检查
**收紧**:
```cpp
// QuickPanelDialog.cpp:535 修改:
if (hit == -1 && !s_dragging) {
  // 额外: 检查 cursor 是否曾进过 panel (steady-state vs just-shown)
  if (s_everInsidePanel) {  // 新 static, Show() 时 false, hit>=0 首次触发后 true
    s_outsideMs += 100;
    if (s_outsideMs >= 1500) { s_outsideMs = 0; Hide(); }
  }
} else {
  s_outsideMs = 0;
  if (hit >= 0) s_everInsidePanel = true;
}
```

这才是 1.C "等 cursor 第一次进 panel 再开启 auto-hide" (1.C 修法 #2)
的真正 appropriate version。但 1.C 自己 §1.C.5 标记 #2 为 "**行为反直觉,
需要单独 spec + user validation**", 然后推荐 #1 (grace period)。
**Agent A 反驳 1.C 推荐**: grace period 是 hack, 没解决"用户从未 hover
panel 也永不 hide" 的 corner case。

**最终立场 (Agent A 立场声明)**:

- **接受 1.B 的 Primary** (LButtonUp if-d 不 reset s_activeIdx): **接受** —
  Agent A 的 F-B1 / F-B2 不否定 primary, 只是精确化 cause 与 user 感知
  之间的边界。1.B Fix A (1 行加 `s_activeIdx = -1`) 是 minimal correct fix。
- **接受 1.C 的 Primary** (Show() 缺 grace period): **反对作为 standalone** —
  Agent A 的 F-C1 表明 1.C 忽略了 L88-fix 副作用, grace period 修法掩盖
  了真因 (auto-hide dragging 屏蔽)。**反对 1.C 推荐 #1 作为 standalone
  ship, 接受 #1+#4 作为附带 fix, 但 primary 修法应该是收紧 s_dragging
  检查或加 `s_everInsidePanel` 门控**。1.C 的 §1.C.5 #2 (cursor 首次进 panel
  才开启 auto-hide) 才是 semantic correct, 1.C 自己怕 "行为反直觉" 而
  推 #1 是过度保守。
- **坚持 1.A 的 Primary**: 维持 §1.A.1 的 `y0=6` + `kBtnGap=14` + `panelW=277`
  不变, 上面 D-A1/D-A2 已 defense。**接受 user A/B 实验证伪** (§1.A.6 #3)。

**Agent A 提的关键 falsification check** (1 个):

> **Check FA-A (在真 ship build 内同步跑这条)**: 加 1 行临时日志到
> `QuickPanelDialog.cpp:535`:
> ```cpp
> OutputDebugStringW(L"QP: hit=%d drag=%d outsideMs=%d\n", hit, s_dragging, s_outsideMs);
> ```
> 在 v0.19.0.24 ship 前 **DebugView 监控 30 分钟**, 记录 issue 4 真实 user
> 遭遇时刻的: hit / s_dragging / s_outsideMs 三元组。**若 hit==-1 && s_dragging==FALSE
> && s_outsideMs 紧贴 1500 → 1.C Primary 证实 (cursor 在 panel 外,
> L88-fix 不参与, grace period 修法有效)**。**若 hit==-1 && s_dragging==TRUE
> && s_outsideMs==0 (auto-hide 被 dragging 屏蔽) → 1.C 错, 1.A 主张的
> L88-fix 共因对 issue 4 才是真因, 1.C grace period 不会根本性修复**。
> **期望结果**: DebugView 应反复出现 hit==-1 && s_dragging==FALSE &&
> outsideMs 累加,user 立即松手后 s_dragging=FALSE,**但 s_dragging
> 在 LButtonUp 瞬间由 TRUE→FALSE 的 100ms 窗口里 timer 已经 tick →
> 进入累加分支**; 这精确验证了 1.A 的 cross-cutting claim (issue 4
> 真因含 L88-fix 副作用)。

## 3. Round 3 — 最终共识

> **合成时间**: 2026-07-13
> **合成方**: 主 agent (读 §1.A + §1.B + §1.C 全部 Round 1+2 后)
> **3 位 agent 立场收敛**: 见 §3.1,分歧见 §3.2

### 3.1 三方一致点 (consensus)

| # | issue | 共识 (3 位 agent 全部接受) | 主要引用 |
|---|---|---|---|
| 1 | 按钮+图标偏上 | y0=pad=5 让 btn 几何中心 22.5 而 panel 中心 24,**偏上 1.5 px**。改 `y0=(panelH-btnSize)/2=6` 让 btn 中心 23.5 ≈ panel 中心 24,可见内容中心 22.5 (差 1 px 离散不可避免)。**必同步 HitTest Y 范围** | §1.A primary · §1.B 接受 · §1.C 接受 |
| 2 | 按钮间距 +3 | `kBtnGap 11→14` (user +3),**不缩 btn**。两种 layout 选项: ① `panelW=277, rightPad=4` (激进) ② `panelW=289, rightPad=16` (保守) | §1.A.4 · §1.B 接受 · §1.C 接受 |
| 3 | active bg 不恢复 | L88-fix (cpp:449) 让 LButtonDown 无条件 `s_dragging=TRUE`; LButtonUp if-d 分支 (cpp:489-494) **未 reset s_activeIdx**。**最小 fix**: drag 分支补 `s_activeIdx = -1` (1.B Fix A) | §1.B Cause 1 · §1.A 接受 · §1.C 接受 (但 1.C 补充: click handler 整体缺位是 spec 070 已知限制,不在 issue 3 修复范围) |

### 3.2 三方分歧与决议

#### 3.2.1 issue 4 (panel 快速消失) 根因: grace period 缺失 vs L88 共因

**Agent A 立场**: 1.C 完全忽略 L88-fix (`QuickPanelDialog.cpp:449`) 让 `s_dragging=TRUE` 屏蔽 auto-hide accumulate (`cpp:535-541`)。L88 不只是 issue 3 的共因,也是 issue 4 的共因。grace period 是 hack,真因是 **L88 链**。

**Agent B 立场**: 有条件接受 1.C primary,但**拒绝单独 ship**。issue 4 必须等 issue 3 (1.B Fix A) 修好后才好评估,因为 L88 drag 启动改变了 cursor 进 panel 时的行为。

**Agent C 立场**: 坚持 grace period 是 issue 4 的**直接**根因。L88 是 issue 3 的共因,但 L88 **不**直接触发 issue 4,因为 cursor 第一次进 panel 时 hit 应该 ≥0,s_outsideMs 不会累加;cursor 在 panel 外 + dragging 状态下,auto-hide 被 `!s_dragging` guard 屏蔽,**不累加 outsideMs**。

**主 agent 决议**:
- **issue 4 直接根因 = grace period 缺失** (Agent C 立场赢)。`!s_dragging` guard 让 L88 不直接让 outsideMs 累加 (1.C.12.2 §1036-1100 已论证)。
- **但 Agent A 的洞察仍然成立**: L88 链意味着 issue 3 修复后会暴露 issue 4 的其他触发面 (例如 cursor 在 panel 外但 user 期望面板留住)。所以 **issue 3 和 issue 4 必须一起 ship**,但 grace period 修法仍是 issue 4 的最小修法。
- grace 时长: Agent C 推 2 秒,Agent A 没具体提时长。**决议: 2 秒** (符合 user "show 出来后能看一会儿" 期望,不长到影响 panel 行为)。

#### 3.2.2 issue 3 修复范围: 1.B Fix A vs spec 070 click handler 缺位

**Agent B 立场**: 1.B Fix A 修"s_activeIdx 残留"这半根因,真正的 click 整体缺位 (LButtonUp else 分支 no-op) 是 spec 070 已知限制,不在 issue 3 修复范围。

**Agent C 立场**: 同 B,1.B Fix A 修半根因,click handler 缺位后续 spec 解决。

**主 agent 决议**:
- issue 3 修复 = **仅 1.B Fix A** (drag 分支补 `s_activeIdx = -1`)。
- click handler 缺位是单独 spec 范畴(技术债),**不在 v0.19.0.24 修复范围**。
- 写 lessons-learned.md 备注 click handler 缺位是已知限制,后续 spec 解决。

#### 3.2.3 issue 2 layout 选项: 激进 rightPad=4 vs 保守 rightPad=16

**Agent A 立场**: 两个选项都列出,没明确推哪个。

**Agent B 立场**: 接受 1.A 主张,ship `0.19.0.24a` 时**未指定**走哪条。

**Agent C 立场**: 接受 1.A,未指定。

**主 agent 决议**:
- user 之前从 9→11→16 (L87→L88→L92) 持续要"右间距增大"。**保守路线 (panelW=289, rightPad=16) 跟 user 期望一致**。
- 但 panelW 从 277→289 (+12 px) 在某些屏幕可能挤压 brand 区域。**决议: 保守路线 panelW=289**, 跟之前迭代历史一致。
- 备选 (panelW=277, rightPad=4) 作为 falsification 备选方案,如果 user 觉得 289 太宽可切回。

### 3.3 最终根因汇总

| issue | 直接根因 | 链式根因 | 共识修法 |
|---|---|---|---|
| **1** 按钮偏上 | `y0=pad=5`, 几何中心 22.5 vs panel center 24 | L93 撤销 L93 中断尝试 wrong 方案后没修垂直居中 (L92 之前所有 fix 都用 `pad` 作 Y 起点) | `y0 = (panelH - btnSize) / 2 = 6` (cpp:716-727) + 同步 HitTest Y 范围 (cpp:387-392) |
| **2** 按钮间距 | `kBtnGap=11` 仍小于 user 期望 | user 期望"按钮间宽松",L87→L88→L92 持续加 (`kBtnGap 3→4→6→11`) | `kBtnGap 11→14` + `kPanelW 277→289` (保守) 或 `kPanelW=277, rightPad=4` (激进) |
| **3** active bg 残留 | LButtonUp if-d 分支没 reset s_activeIdx | L88-fix drag any area 让 LButtonDown 无条件 `s_dragging=TRUE` → if-d 永走 → s_activeIdx 永驻 | 1.B Fix A: `cpp:493` 补 `s_activeIdx = -1;` (1 行) |
| **4** 首次 Show 快速消失 | Show() 后 1.5 s 内 cursor 还在 panel 外 → `s_outsideMs>=1500` → Hide() | L89-fix 1.5 s 阈值无 grace period; L88 副作用让 cursor 在 panel 外时 `s_dragging=TRUE` 屏蔽 auto-hide (Agent A 洞察) | 1.C 修法 #1: Show() 设 `s_showTime = GetTickCount()`; timer 检查 `elapsed < kShowGraceMs (2000)` 时不累加 outsideMs |

### 3.4 跨 issue 关联

- **issue 1 + 2 + 3 全部独立**,可并行 ship,但 1+2 是同一文件 (header 改常量) + 同一函数 (cpp:716-727),所以 ship 一起。
- **issue 3 + 4 共享 L88 链**: L88 让 dragging 屏蔽 auto-hide accumulate (cpp:535 guard),但 issue 4 仍可独立 ship (grace period 在 dragging 时也不累加,等于两层 guard)。
- **issue 3 + click handler 缺位** (spec 070 已知限制): 1.B Fix A 修 state 残留, 但 click event 没真的 fire 给上层 (no-op 在 cpp:496-503)。这是另一 spec 范畴。

### 3.5 建议 ship 顺序

**v0.19.0.24** (一次 ship 4 个 issue):
1. **§1.A 修法** (issue 1+2): header `kBtnGap 11→14`, `kPanelW 277→289`, `kPanelH` 不变; cpp:716-727 改 `y0 = (panelH - btnSize) / 2 = 6`; HitTest Y 范围同步
2. **§1.B Fix A** (issue 3): cpp:493 补 `s_activeIdx = -1;` (1 行)
3. **§1.C 修法 #1** (issue 4): header 加 `static DWORD s_showTime;` (跟 L93 中断尝试时一样的字段,这次真用); Show() 赋值 `s_showTime = GetTickCount();`; cpp:535 加 `if (now - s_showTime < kShowGraceMs) skip;` (guard)
4. **§1.C 修法 #4** (issue 4 顺手): Hide() reset `s_outsideMs = 0;` (现有已 reset,确认); 复用路径 (cpp:1031-1036) KillTimer 后 SetTimer
5. **CHANGELOG / FLUENT-UI-TOKENS / lessons-learned L94** 更新

### 3.6 Falsification experiment plan (实施前必跑)

按 3 位 agent 提的关键 checks 合并:

| Check | 实施 | 期望 | 若不符则 |
|---|---|---|---|
| **FA-A** (Agent A) | `cpp:535` 加 `OutputDebugStringW` 三元组 (hit / s_dragging / s_outsideMs), DebugView 监控 issue 4 复现 | hit==-1 && s_dragging==FALSE && s_outsideMs 累加到 1500 → 1.C grace 修法根本 | hit==-1 && s_dragging==TRUE → 1.C grace 仍是 hack,需先 ship 1.B Fix A 才能验证 issue 4 |
| **F4** (Agent B) | `PaintOpaqueContent` 入口加 `OutputDebugStringW` 打印 `s_activeIdx`, 测"点 button0 不动 cursor 松手" 路径 | ship 前: s_activeIdx=0 残留; ship 1.B Fix A 后: s_activeIdx=-1 | 残留 → Fix A 没生效,查 cpp:493 行是否真加了 |
| **F-1C-F1** (Agent C) | 把 `kShowGraceMs` 暂时调 5000, 看 issue 4 是否 hold >5s | hold >5s → 1.C primary 证实 (grace 真因) | still hide <5s → 转移查 WM_DPICHANGED (cpp:423-440) 误触 |
| **Agent A §1.A.6 #1** (几何) | 100% DPI raw `qp-dump.bmp` 量 panel 实际像素边界 + 按钮 region | y0=5 时按钮 region 真距顶 5 距底 8 | 量得等距 → 1.A primary 改 y0 治标不治本,根因在 highlight/shadow 视觉重量 |
| **Agent A §1.A.6 #2** (highlight/shadow 视觉) | 隐藏 highlight、隐藏 shadow、两者都隐藏后盲评 | "偏上" 随 highlight/shadow 消失 → Alternative E 获支持 | 仍偏上 → 1.A primary (改 y0) 是真因 |
| **Agent C §1.C.7 F1** (Show grace) | Show grace 2000 ms 实现后,实测 hotkey → panel 留住 ≥2 s 后才开始 auto-hide | ≥2 s 留住 → 1.C 修法 #1 成功 | <2 s 即 hide → 1.C 修法不根本,查 L88 链 |

### 3.7 风险与未解决项

1. **DPI 矩阵测试**: Agent A §1.A.5 指出 raw-pixel icon 不随 dpr 缩放, 高 DPI 时 icon 相对按钮/Logo 变小。issue 1+2 修复后,可能暴露新可见问题 (icon 在 150% DPI 看起来小)。**决议: ship v0.19.0.24 后单独 follow-up spec**。
2. **PNG logo 透明内边距**: Agent A §1.A.5 指出 WIC 缩放不裁透明 padding,可能让 logo 看起来比 button 小。**决议: 同上,follow-up spec**。
3. **click handler 整体缺位** (1.B §1.B.5 备注): 1.B Fix A 修 state 残留,但 LButtonUp else 分支 no-op 意味着 user 点 button 仍不 fire 上层。**决议: 不在 v0.19.0.24 修,记 lessons-learned L94, 后续 spec**。
4. **WM_DPICHANGED 在 Show 后的副作用**: Agent C 提到 F-1C-F1 fail 时需查 cpp:423-440。**决议: 实施时如出现,单独 root cause**。

### 3.8 ship 后验证 checklist

- [ ] xmake build WeaselServer 编译通过
- [ ] NSIS installer built
- [ ] TestDefaultHotkeys 35/35 PASS
- [ ] TestQuickPanelDialog 5/5 PASS
- [ ] TestQuickPanelRefactor 1/1 PASS
- [ ] 100% DPI 截图: button 上下等距, btn→btn gap 14 px, 右边距 16 px (或 4 px 激进版)
- [ ] 静态视觉: 点 button → 松手 → 橙 bg 立即恢复
- [ ] 静态视觉: hotkey Show → panel 留住 ≥2 s,鼠标离开 1.5 s 后才 hide
- [ ] 多 DPI (100/125/150%) 不回归


## 4. 引用与现状快照 (v0.19.0.23, L93 落地后)

- `WeaselServer/QuickPanelDialog.h`:
  - `kPanelW=277`, `kPanelH=48`, `kBtnSize=35`, `kBtnGap=11`, `kIcoSize=19`,
    `kBrandSize=35`, `kPanelRadius=20`, `kPanelPadding=5`
  - 新增 `kIconBboxCxOff=15`, `kIconBboxCyOff=15`
  - 注:`s_showTime` 字段在 git history 中**从未存在**,见 §1.C.6 — 此条 claim 是
    misremember,实际只是 brainstorming 期间的 draft
- `WeaselServer/QuickPanelDialog.cpp`:
  - `iconX = x0 + s_btnSize_phys/2 - kIconBboxCxOff` (raw pixel,NOT × dpr)
  - `iconY = y0 + s_btnSize_phys/2 - kIconBboxCyOff`
  - `HitTest` buttonStartX 用 `max(2, 2*dpr_x + 0.5f)` (跟 PaintOpaqueContent 对齐)
  - top highlight 1 px (L93 中断尝试的好改动,保留)
  - bottom shadow 4 px (y=H-4..H-1)
- L89-fix auto-hide: `s_outsideMs += 100` if `hit==-1 && !s_dragging`,>=1500 → Hide()
- L88-fix drag any area: LButtonDown 总是 `s_dragging = TRUE`,LButtonUp if-d 分支 reset 但 **不 reset s_activeIdx**
- L86 hover state: hover=橙线条,active=橙 bg + 白图标

### 已知 v0.19.0.22→23 内的可疑根因(预判, 待证伪/证实)

1. issue 1: `panelH=48` + `pad=5` + `btnSize=35` → btn at y0=5..40, 距顶 5 距底 8,**3 px 偏上**
2. issue 3: WM_LBUTTONUP 的 if-d 分支(s_dragging=TRUE 时)没 reset s_activeIdx → 橙色持续
3. issue 4: L89 auto-hide 无 Show() 后的 grace period → 第一次 Show 时 mouse 还没进 panel,1500ms 后 Hide

(以上为预判, agent 调研可能证伪)
