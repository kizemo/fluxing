# ADR 0050 — 快捷面板 4 槽化 + 键帽符号/笔宽严格同比缩放

- 状态: 已采纳 (2026-11-09)
- 影响范围: `include/FluxingPanelIconGeom.h`(新建)、`include/FluxingPanelFwIcon.h`、`WeaselServer/QuickPanelDialog.{h,cpp}`、`test/TestQuickPanelDialog`、`test/TestQuickPanelRefactor`
- 决策类型: 架构决策(面板槽位集合 + 缩放口径)+ 缺陷根因结论
- 关联: 交接 `docs/archive/handoffs/handoff-2026-11-09-batch-t-sogou-dpi.md` §8(P0 / P1);用户真机反馈「改分辨率后,logo 完整,但键帽符号还是没有同比缩放,线条也还是偏粗」

## Context

### P0:符号与笔宽「不同比」

ADR 0049 已把键帽 / 槽位 / 窗口 / 图标几何统一到同一个缩放系数(系数下限 1.0),
但用户真机换分辨率后仍看到两处现象:**符号没有同比缩放**、**线条偏粗**。

根因不是「改动没上机」:装机件 `D:\Program Files\fluxing\weasel\WeaselServer.exe`
SHA256 = `47B00A2B…6E59` = 构建产物 `output\Win32\WeaselServer.exe`,
且时间戳链 `QuickPanelDialog.cpp (21:47:07) < QuickPanelDialog.obj (22:03:23) < WeaselServer.exe (22:03:34)`
证明 83d1820 的图标缩放改动**确实在**装机二进制里。

真正的病灶是**两处取整**把「几何乘系数」破坏掉:

1. **笔宽就近舍入**。fwicon 空心环的 raw 笔宽是 **1px**(设计意图:细线比实心点轻)。
   在 150%(系数 1.5023)下 `round(1 × 1.5023) = 2px` ⇒ 笔宽 **2.0×** 而几何只有
   1.5× ⇒ 环相对粗 33% ⇒ 用户看到的「线条偏粗」。raw3 的加粗线同理会得 5px(理想 4.5)。
2. **逗号字号二次取整**。逗号是**真字形**,字号原先由
   `ScaleFontHeight(seed=48, 实测墨迹高, 目标墨迹高)` 反推,而目标墨迹高取自
   **已被取整**的缩放墨迹盒(13 → 19)⇒ 字号 = 48 × 19/13 = **70px**(理想 72px)
   ⇒ 符号只有 **1.46×** 而不是 1.5×,字形宽度同样不同比。

附带缺陷:tooltip 字体 `s_hFontTooltip` 只在**首次**显示 tooltip 时创建,
面板活过 DPI 变化后字号停在旧 DPI(同一类「没跟着缩放」)。

### P1:面板槽位集合

用户指定摘掉「快捷键设置」与「用户登录(关于·账号说明)」两个键帽 ⇒ 面板 6 槽 → 4 槽,
视觉顺序:切换输入法 / 中英文 / 全半角 / 用户短语。

## Decision

1. **新建共享口径头 `include/FluxingPanelIconGeom.h`**(header-only,
   namespace `fluxing::panel_icon_geom`),作为**符号几何与笔宽缩放的单一真相源**:
   `ClampFactor` / `ScaleCoord`(就近舍入,.5 向上)/ `ScaleStroke`(**向下取整**,下限 1)/
   5 个图标的 raw 墨迹盒 / `ScaledInkBox` / `ScaledInkSize`。
   ★ 生产代码与单测读**同一份**公式(constitution R7)——测试里抄一份缩放算式
   就是「假绿」的制造机(spec 092 记的死法)。
2. **笔宽口径 = 向下取整(floor),下限 1px**。理由:GDI 笔宽只能是整数,1.5px 不可表达;
   raw1 的线在 150% 下只能取 1px(相对细)或 2px(相对粗 33%)二选一,用户明确抱怨偏粗,
   故取**细**的一侧。raw ≥ 2 的线在 100/150/200% 下正好 2/3/4px(**严格同比**)。
   这条是**刻意的视觉取舍**,不是舍入 bug;改它必须同时改单测 `T31b`。
3. **坐标口径不动**。`ScaleCoord` 与 `include/FluxingPanelDpi.h` 的 `ScaleByFactor`
   逐位一致(就近舍入,.5 向上),既有布局口径与单测 T25 全部不变。
   `FluxingPanelFwIcon.h` 原先自己抄了一份取整公式 ⇒ 改为转发共享实现,消掉第二份真相源。
4. **逗号目标字号直接按系数给**。`Icon` 新增 `commaFontPx`:`Build()` 置为种子字号
   (`kCommaFontHeight = 48`),`ScaleIcon()` 按系数缩放(150% ⇒ 72,200% ⇒ 96)。
   `DrawInkPlacedChar` 新增 `fontPx` 形参;**字体自校正(按实测墨迹高反推字号)
   仅在 `fontPx <= 0` 时执行** —— 那是「目标机器没有 Microsoft YaHei」的兜底路径。
   自校正若在已给字号时仍执行,会把 72 拉回 70 ⇒ 退回 1.46×(旧病灶)。
5. **槽位集合 6 → 4**。`kButtonCount = 4`;`kVisualToLogical = {0, 4, 5, 1}`
   (视觉 0 切换输入法 / 1 中英文 / 2 全半角 / 3 用户短语);
   `kPanelW 215 → 155`(闭合式:4 + 26 + 3 + 4×28 + 3×2 + 4 = 155)。
   逻辑槽 2(快捷键设置)/ 3(关于·账号说明)退出映射表 ⇒ `VisualAtLogical(2/3) = -1`,
   面板上不再有这两个键帽。
6. **只摘面板键帽,不动功能入口**。托盘右键「快捷键设置」
   (`WeaselServer.rc` 的 `ID_WEASELTRAY_SHORTCUT_SETTINGS` → `ShortcutSettings::Show()`)
   与 Ctrl+Shift+K 全局热键**保留** —— 删入口属功能删减,超出本轮授权。
   「关于 / 账号说明」页(`AboutDialog`)模块保留,但面板上不再有入口。
7. **tooltip 字体随 DPI 重建**。`ApplyPhysicalLayout` 里删旧建新(按 `dpr_y`),
   避免旧字体泄漏(GDI 对象泄漏)。

## Consequences

- 符号尺寸与几何**严格同比**(150% ⇒ 逗号目标字号 72 而不是 70);
  笔宽不再「相对粗 33%」。口径全部落在共享头 + 单测 `T31`(三档 DPI 断言)。
- **P0 仍需用户人工改一次分辨率验收**:快捷面板是 IME 激活瞬间才存在的自绘顶层窗,
  跨进程自动化抓不到(见交接 §7.8),自动化只能提供单测级证据。
- 面板尺寸变为 100% 155×32 / 150% 233×48 / 200% 310×64(单测 T25 锁死)。
- 摘掉「关于」键帽 ⇒ `AboutDialog` 在面板侧不可达(模块未删,后续若要恢复入口
  只需把逻辑槽 3 放回映射表)。
- `ScaleStroke` 的「向下取整」是**取舍**:1px 线在 150% 下看起来比几何比例略细。
  若用户日后要求「宁可略粗不可略细」,改 `FluxingPanelIconGeom.h` 一处并同步 `T31b` 即可。
