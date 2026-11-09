# ADR 0049 — 快捷面板 DPI 适配:单一缩放系数 + logo 位图随尺寸重建

- 状态: 已采纳 (2026-11-09)
- 影响范围: `WeaselServer/QuickPanelDialog.{h,cpp}`、`include/FluxingPanelDpi.h`、`include/FluxingPanelFwIcon.h`
- 决策类型: 架构决策(UI 缩放口径)+ 缺陷根因结论
- 关联: 用户报告「远程更换显示分辨率后,快捷设置栏品牌图标只显示一部分、按钮符号偏大,需要适应不同分辨率」

## Context

用户真机反馈(远程桌面换分辨率后): 快捷设置栏
1. **品牌图标只显示一部分**(P0),
2. **按钮符号偏大、比例失衡**(P0),
3. 需要**适应不同分辨率**。

静态诊断报告: `tmp/dpi-diagnosis.md`。三条根因:

1. **品牌只显示一部分**: `s_hBmpLogo` 只在 `OnCreate` 里按当时的 brand 尺寸用 WIC 预缩放解码**一次**;
   `ApplyPhysicalLayout` / `WM_DPICHANGED` 只更新 `s_brandSize_phys`,**从不重建位图**;
   而绘制用 `BitBlt(SRCCOPY)` 1:1 贴图(GDI 的 BitBlt 不缩放)⇒
   DPI 变化后目标矩形与旧位图尺寸脱钩,只剩旧图一角。
2. **符号偏大**: 键帽 `btn_size` 按 dpr 缩放(`ScaleRound(28→30, dpr_x)`),
   但 5 个 `DrawIcon*` 与 fwicon 图元按 **raw 像素**绘制(原点常量 15 不乘 dpr),
   dpr 下降时最宽 25 raw px 的图元溢出缩小后的键帽
   (`QuickPanelDialog.h` 的 `kBtnSize >= 28` 不变量被破坏)。
3. **复用路径不重查 DPI**(P1): `QueryPanelDpi` 用 `GetDpiForMonitor(MDT_EFFECTIVE_DPI)`
   且只在**建窗路径**调用;`Show()` 的窗口复用路径不重查 DPI,
   `RepairOffscreenPanel` 会用新 DPI 配旧尺寸 ⇒ 面板状态自相矛盾。

## Decision

1. **单一缩放系数源**。面板只有一个缩放系数来源: `dpi / 96`,且 **钳到 [96, 384]**,
   即系数**下限 1.0**。由此**窗口物理尺寸恒 ≥ 逻辑尺寸**(下限与尺寸下限同时成立)。
   钳制与系数计算落在 `include/FluxingPanelDpi.h`:
   `ClampPanelDpi(int)` / `PanelScaleFactor(int)` / `ScaleByFactor(int, double)`。
   ★ `ScaleLogicalByDpi` 的既有语义**不改**(单测 T25 锁死其逐位口径),只新增上述三族函数。
2. **键帽 / 槽位 / 窗口尺寸 / 图标几何 / fwicon 几何用同一个系数**。
   `DrawIcon*` / `DrawIconFullwidth` 全部新增 `scale` 参数,图元坐标与笔宽乘该系数;
   raw 坐标保留为**设计稿基准形状**,不做手工微调。
   走的是「几何乘 dpr」而不是「dpr 下限 1.0 但图标仍 raw」——
   后者在 dpr > 1 时图标偏小,不是真适配。
3. **logo 位图随 brand 物理尺寸重建**。`ApplyPhysicalLayout` 里比较
   `s_logoSize_phys != s_brandSize_phys` ⇒ 释放旧位图(`DeleteObject`)并用同一条
   WIC 预缩放路径重建 ⇒ 仍是 1:1 不透明贴图,不变合成路径(保住需求⑦ 的
   「预合成到面板底色、alpha 恒 255、无近黑像素」结论)。
   `OnDestroy` 同步复位 `s_logoSize_phys`。
4. **DPI 一律按窗口当前值取,且必须运行时解析**。
   新增 `GetDpiForWindowDynamic()`(运行时 `GetProcAddress`,**不静态导入**
   `api-ms-win-shcore-scaling-l1-1-1.dll`,避免旧系统装载失败),
   取不到时退显示器有效 DPI。
5. **窗口复用路径必须重查 DPI**。新增
   `QuickPanelDialog::SyncPanelSizeToCurrentDpi(HWND)`:读当前 DPI ⇒
   算出应有物理尺寸 ⇒ 与当前尺寸不一致就 `SetWindowPos` 改尺寸 + `ApplyPhysicalLayout`
   (键帽 / 图标系数 / logo 位图 / off-screen DIB 全部跟随),返回 true 表示需要重画。
   `Show()` 的建窗路径与复用路径都调用它。
6. `WM_DPICHANGED` 里系统建议矩形只用于**位置**(`lParam` 的 RECT),
   尺寸一律按新 DPI 自己算;DPI 先钳到 [96,384] 再参与计算。

## Consequences

- 换分辨率 / 改显示缩放 / 跨屏移动后,面板的键帽、图标、品牌 logo、off-screen 表面
  **尺寸一致**,不再出现「logo 只显示一部分」或「符号溢出键帽」。
- 缩放系数下限 1.0 意味着 **100% 以下不缩小**面板(亚像素几何会导致命中区不可靠、
  图标比例反常);代价是低分辨率下面板占屏更大,这是刻意的取舍。
- 图元几何改为按系数换算 ⇒ 任何新增图元都必须显式传 `scale`,
  漏传会退化成 raw 像素(与修复前的病灶同类);编译期无法完全拦住,靠 review 与
  `TestQuickPanelDialog` 的墨迹盒断言兜底。
- `GetDpiForWindow` 走动态解析 ⇒ 最低支持的 Windows 版本不受影响。
