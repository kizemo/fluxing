# 070 - QuickPanelDialog v3-rev3 重新启用

> **元 spec**(WHAT 层,无技术栈词)。
> L69 临时关闭了 QuickPanel,本 spec 在"绕开历史雷区"前提下,把 v3-rev3 设计稿落成可 ship 的产品。
> 设计稿:`docs/design/quickpanel-v3/index.html` (v3-rev3,真火流猩 PNG + 38px 右侧图标 + hover 染品牌橙 + Liquid Glass 背景)

## 0. 背景

L69 之前(v0.18.29.0 – v0.18.41.3)共 4 个版本都在用 GDI+ `Bitmap(IStream*)` 渲染 logo。GDI+ 内部为 lazy pixel-decode 缓存 IStream 指针;`stream->Release()` 后首次 paint 触发 vtable UAF → heap corruption → 任意后续堆操作检测损坏 → STATUS_HEAP_CORRUPTION → WeaselServer.exe 退出 → TSF pipe 断开 → 用户无法输入中文。L67/L68/L69 修了 3 轮未收敛,L69 决定**临时关闭 QuickPanel 止血**(v0.18.41.4 已 ship)。

本 spec 不修复历史 GDI+ bug,而是**用不会触发那个 bug 的技术栈重写 QuickPanel**,把 v3-rev3 设计稿落成 v0.19.0 ship 切片。

## 1. 目标

让 Fluxing 用户重新能用 QuickPanel — Alt+, 或左键托盘图标打开后,看到 Fluxing 品牌 logo + 5 个功能入口。视觉规格与 v3-rev3 设计稿完全一致。

## 2. 用户故事

- US070-A [P1] 用户在任意 app 焦点下按 Alt+, → 屏幕右下角出现 QuickPanel,6 个元素横排:Fluxing 品牌 logo + 方案/短语/符号/设置/账号 5 个图标按钮
- US070-B [P1] 鼠标悬停某个图标按钮 → 该图标 stroke 颜色变为品牌橙(#FF5F31),背景轻微玻璃提亮,scale 1.08(150ms 过渡)
- US070-C [P1] 点击某个图标按钮 → 触发对应回调(占位 no-op);active 态显示橙→紫渐变背景
- US070-D [P2] 点击 panel 外部或按 ESC → panel 隐藏
- US070-E [P2] 1 秒失焦后 panel 自动关闭
- US070-F [P3] dark mode 时 panel 自动跟随系统颜色切换(Light: 白底黑字;Dark: 深灰底白字)

## 3. 功能需求

- FR-001: 渲染管线必须使用 [避开历史 bug 的图像加载方式],见 spec/plan 技术选型
- FR-002: 6 元素横排布局,Fluxing 品牌 logo 在最左,占位功能按钮在右
- FR-003: hover 反馈必须包含 (a) 图标颜色变化、(b) 背景提亮、(c) scale 缩放
- FR-004: active 状态显示品牌橙→紫渐变背景,白色图标
- FR-005: 5 个占位按钮点击当前触发 no-op(v0.19.0 ship 切片);占位文本/图标已就位
- FR-006: window class 注册为不可见、不抢焦(window style 必须含 WS_EX_NOACTIVATE + WS_EX_TOOLWINDOW)
- FR-007: 进程退出时 panel 资源必须被正确释放,无任何 [绕过历史 bug 的图像加载方式] 路径

## 4. 非目标

- 5 个按钮的真实功能(方案切换 / 短语加载 / 符号面板 / 设置 / 账号) — 由后续 spec 实现
- FocusIn 自动长显(对应 spec 052) — 需另开 spec
- 用户偏好的透明度/位置/自动关闭延迟 持久化 — 需另开 spec
- dark mode 自动切换逻辑 — 需另开 spec

## 5. 验收标准

- SC-001-1: 按 Alt+, 后,屏幕右下角 392×68 px 区域出现 v3-rev3 设计稿形态的 panel
- SC-001-2: Fluxing 品牌 logo 像素清晰(700×700 PNG source,缩放至 56×56 不糊)
- SC-001-3: 5 个图标按钮(方案 ↔️ / 短语 💬 / 符号 ⌨️ / 设置 ⚙️ / 账号 👤)形态各异,无视觉混淆
- SC-002-1: 鼠标 hover 任意按钮,图标 stroke 变橙(#FF5F31),过渡时间 ≤ 200ms
- SC-002-2: hover 时按钮 scale 1.08,松开后回 1.0,无卡顿
- SC-003-1: 点击 5 个按钮,均不触发 WeaselServer.exe crash;进程在 tasklist 中保持运行
- SC-004-1: 任意 active 按钮(aria-pressed=true)显示品牌橙→紫渐变背景
- SC-005-1: 5 个按钮的点击回调均为 no-op(v0.19.0 ship 切片)
- SC-006-1: panel 不会抢其它窗口焦点(WS_EX_NOACTIVATE)
- SC-007-1: 卸载/重装 100 次后无内存泄漏(每个版本 ≤ 5MB RSS delta)

## 6. 风险

- R-001: 如果实现选错技术栈(用了 GDI+ Bitmap(IStream*)),会复现 L67-L69 的崩溃链
- R-002: 如果 PNG 解码用了 GDI+ 类 API,即使包成 D2D bitmap,也可能引入 GDI+ 内部状态(见 L67)
- R-003: Active 状态渐变背景用 LinearGradientBrush 时,如果每个按钮创建独立 brush,内存会泄漏
- R-004: hover 反馈 150ms 过渡如果用 timer 而不是属性 transition,会增加消息循环复杂度

## 7. 依赖

- D-001: `fluxing-logo.png` 700×700 PNG 文件已存在于 `docs/design/fluxing-logo.png`
- D-002: 资源打包脚本(`installer.nsi` 或 `fluxing-logo.bmp.res`)需要把 PNG 嵌入 exe
- D-003: IDR_FLUXING_LOGO 资源 ID 已存在于 `WeaselServer/resource.h`

## 8. v1 排除

- 实际功能(方案/短语/符号/设置/账号)
- FocusIn 自动长显(对应 spec 052)
- 用户偏好的持久化
- dark mode 自动切换
- Alt+, 之外的其它触发方式(暂时仅 Alt+, + 左键托盘图标)
- 透明背景的真实 Windows Alpha API(目前用 panel-level alpha,不是 pixel-level alpha)
- 主题切换 / 皮肤
- 多显示器 / 高 DPI 精细适配(只做 PerMonitorDPI aware)