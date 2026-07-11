# 070 - Plan: QuickPanelDialog v3-rev3 实现

## Constitution Check

| 原则 | 状态 | 说明 |
|---|---|---|
| I - R1 Rime 集成 | ✓ | QuickPanel 由 RIME 通过 WeaselIPC 触发,Al+，/ FocusIn 模式由调用方决定(本 spec 不引入 FocusIn,只用 Alt+, 和托盘) |
| II - R2 WeaselIPC + WeaselTSF | ✓ | QuickPanel 仍走 WeaselServer 单进程内部,不增加 IPC |
| III - R3 task-level 标签 | ✓ | tasks.md 每任务标 P1/P2/P3 + [USxxx] |
| IV - R4 安全 | ✓ | 输入参数仅 WS 热键和托盘消息,无文件路径用户输入 |
| V - R5 ≤4 小时任务 | ✓ | tasks.md 已拆分为 ≤4h 单元 |
| VI - R6 不盲改 | ✓ | 本 spec 不改 L67/L68 历史代码,只新增 QuickPanelDialog 重写 |
| VII - R7 verify-before-complete | ✓ | SC-001-1 到 SC-007-1 都给了可执行验收 |
| VIII - R8 用户可见 | ✓ | SC-001-1 是用户视角验收 |
| IX - R9 单测 | (N/A) | GUI 单测在 R9 中暂缓(spec 070 不引入新单测基础设施) |
| X - P1-P8 产品约束 | ✓ | UI 与文档全是简体中文;不破坏现有 IPC 协议 |

## 技术选型(避雷)

**绝对禁止**:
- ❌ GDI+ `Bitmap(IStream*)` (L67 雷区)
- ❌ `stream->Release()` 早于 `Bitmap` 销毁 (L68 雷区)
- ❌ 用 `CreateStreamOnHGlobal` 后立即 `Release()`(无论 fDeleteOnRelease 设什么)(L67 根因)

**必须使用**:
- ✅ `ID2D1HwndRenderTarget` (主渲染)
- ✅ `ID2D1BitmapRenderTarget` 或 `ID2D1DeviceContext::DrawBitmap` (画 PNG)
- ✅ `ID2D1PathGeometry` / `ID2D1GeometrySink` (画 SVG 路径,即 5 个图标)
- ✅ `ID2D1SolidColorBrush` + `ID2D1LinearGradientBrush` (背景 + hover)
- ✅ `IDWriteTextFormat` (将来加标签用)
- ✅ **WIC** (Windows Imaging Component) 解码 PNG: `IWICImagingFactory::CreateDecoderFromFilename` → `IWICBitmapDecoder::GetFrame` → `ID2D1Bitmap::CreateBitmapFromWicBitmap`
- ✅ **不缓存 IStream**,WIC 直接给 D2D ID2D1Bitmap,生命周期 = renderTarget 生命周期

**关键 invariant**:
- 任何"图像"对象都是 `ID2D1Bitmap`(不是 `Gdiplus::Bitmap`)
- 任何"图标"对象都是 `ID2D1PathGeometry`(矢量,可任意缩放)
- 任何"颜色笔刷"都是 `ID2D1SolidColorBrush` 或 `ID2D1LinearGradientBrush`,创建一次复用
- 任何"WIC 接口"在 OnCreate 时创建一次,OnDestroy 时统一 Release

## 文件级改动

| 文件 | 操作 | 说明 |
|---|---|---|
| `WeaselServer/QuickPanelDialog.cpp` | **重写** | 完全替换现有 GDI+ Bitmap 实现,改为 ID2D1PathGeometry + ID2D1Bitmap |
| `WeaselServer/QuickPanelDialog.h` | **小幅修改** | 加 D2D 字段 (`ID2D1HwndRenderTarget*`, `ID2D1Bitmap*` brand, `ID2D1SolidColorBrush*` × N, `ID2D1PathGeometry*` × 5) |
| `WeaselServer/WeaselServer.cpp` | **小幅修改** | `_tWinMain` 启动 D2D factory;在 `WeaselServerApp` 销毁时统一 Release |
| `WeaselServer/WeaselServerApp.cpp` | **小幅修改** | 在 `Run()` 末尾 `EnableQuickPanelTrigger()`(原 L69 已 disable 的 4 个路径,本 spec 解禁) |
| `WeaselServer/WeaselServerApp.h` | **小幅修改** | 加 `ID2D1Factory*` 字段 |
| `WeaselIPCServer/WeaselServerImpl.cpp` | **小幅修改** | OnCreate 重新注册 Alt+, 热键(L69 已注释,本 spec 解禁) |
| `WeaselServer/xmake.lua` | **不变** | 已包含 D2D 依赖 (d2d1, dwrite, windowscodecs) |
| `WeaselServer/resource.h` | **不变** | IDR_FLUXING_LOGO 已在 |
| `installer/install.nsi` | **修改** | 把 `docs/design/fluxing-logo.png` 打包进 `weasel/fluxing-logo.png` |
| `output/.specify/specs/070-quickpanel-redesign/` | **新建** | 本 spec/plan/tasks 三个文件 |

## 构建/测试/发布 影响

- 构建:`xmake build WeaselServer` + `makensis install.nsi`;D2D lib 已链(原有)
- 测试:本 spec 不引入新单测;验收走 v3-rev3 设计稿(`docs/design/quickpanel-v3/index.html`)与 L69 修复路径双重确认(SC-003-1 是关键)
- 发布:v0.19.0.0 ship 切片,**不要**自动开启 FocusIn auto-show(由后续 spec 决定);Alt+, + 左键托盘 默认开启

## 风险缓解

| 风险 | 缓解 |
|---|---|
| R-001 用 GDI+ Bitmap | plan 已强制 ID2D1Bitmap;code-review 必查 |
| R-002 PNG 解码误用 GDI+ | plan 强制 WIC;code-review 查 `CreateDecoderFromFilename` |
| R-003 brush 泄漏 | plan 强制 "创建一次,复用"模式;tasks 加 D2D resource RAII |
| R-004 hover 用 timer | plan 强制属性 transition(D2D `SetValue(TRANSITIONEND)`) |

## 任务依赖图

```
[T01 WIC + D2D init]  →  [T02 资源加载 (PNG)]
       ↓
[T03 PathGeometry 5 图标]  →  [T04 渲染管线]
                              ↓
[T05 hover/active 状态机]   [T06 window 集成]
                              ↓
[T07 触发器解禁(Alt+,+托盘)]  →  [T08 集成测试 + ship]
                              ↓
[T09 lessons-learned L70 + design v3-rev3 commit]
```

## 验证

每个 task 完成后:
- `xmake build WeaselServer` 编译通过
- `make install` 走 NSIS 一遍
- 在 dev 机器上:
  - 装 v0.19.0
  - 登录,默认 IME 是 Fluxing
  - 按 Alt+, → panel 出现,v3-rev3 视觉一致
  - hover 每个按钮,图标染橙,scale 缩放
  - 点 5 个按钮都不崩
  - 切到 en-US → 切回来 → 仍能用
  - WeaselServer.exe 一直在 tasklist

完成后即可 ship。