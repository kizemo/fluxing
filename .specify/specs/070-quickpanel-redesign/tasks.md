# 070 - Tasks: QuickPanelDialog v3-rev3 实现

> 严格 R3:每任务有 P1/P2/P3 + [USxxx] 标签 + 单文件 1-3 文件修改 + ≤4h。
> R5 拆分为最小 ship 单元。L67/L68/L69 历史雷区用 fr-write 模式(完全重写 QuickPanelDialog.cpp,不改其它文件)规避。
> FR / SC traceability:每个 P1 task 头显式 `(covers FR-xxx)` 注释;每个 V-### 显式 `(verifies SC-xxx)` 引用。

## P1 (must) - v0.19.0 ship 切片

- [ ] T001 [P1, [US070-A], [FR-001 FR-007], 2h, WeaselServer/QuickPanelDialog.h + .cpp]
      重写 QuickPanelDialog.h 字段声明:
      - 加 `ID2D1Factory* m_pD2DFactory`(共享,WeaselServerApp 创建并传入)
      - 加 `ID2D1HwndRenderTarget* m_pRT`
      - 加 `ID2D1Bitmap* m_pLogo`(fluxing-logo.png 700×700 解码结果)
      - 加 `ID2D1SolidColorBrush* m_pBrushDim`(灰图标 hover 前)
      - 加 `ID2D1SolidColorBrush* m_pBrushAccent`(品牌橙,hover 后)
      - 加 `ID2D1SolidColorBrush* m_pBrushPressed`(active 态白)
      - 加 `ID2D1LinearGradientBrush* m_pBrushActive`(active 态橙→紫渐变背景)
      - 加 `ID2D1PathGeometry* m_pIconSchema/Phrase/Symbols/Settings/Account`(5 矢量图标)
      - 删 `static std::unique_ptr<Image> s_logo` 和 `static IStream* s_logo_stream`(L67/L68 雷区)

- [ ] T002 [P1, [US070-A], [FR-001 FR-007], 3h, WeaselServer/QuickPanelDialog.cpp]
      重写 QuickPanelDialog.cpp::LoadLogo → LoadLogoWIC:
      ```cpp
      void QuickPanelDialog::LoadLogoWIC() {
        if (m_pLogo) return;
        Microsoft::WRL::ComPtr<IWICImagingFactory> wic;
        CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_ALL,
                         IID_PPV_ARGS(&wic));
        ComPtr<IWICBitmapDecoder> dec;
        wic->CreateDecoderFromFilename(
            L"fluxing-logo.png", nullptr, GENERIC_READ,
            WICDecodeMetadataCacheOnLoad, &dec);
        ComPtr<IWICBitmapFrameDecode> frame;
        dec->GetFrame(0, &frame);
        m_pRT->CreateBitmapFromWicBitmap(frame.Get(), nullptr, &m_pLogo);
      }
      ```
      关键:不创建 IStream,不做 stream->Release()。D2D bitmap 由 WIC frame 直接创建。

- [ ] T003 [P1, [US070-A], [FR-002 FR-003], 4h, WeaselServer/QuickPanelDialog.cpp]
      实现 5 个 ID2D1PathGeometry 图标(方案 ↔️ / 短语 💬 / 符号 ⌨️ / 设置 ⚙️ / 账号 👤):
      每个图标 ~15 行 GeometrySink 代码,总 ~80 行。
      复用 v3-rev3 设计稿 SVG 路径(viewBox 24×24,缩放到 button 内 38×38 渲染)。
      LoadIconPaths() 静态方法在 OnCreate 末尾调一次。

- [ ] T004 [P1, [US070-A], [FR-002 FR-003 FR-004], 4h, WeaselServer/QuickPanelDialog.cpp]
      重写 OnPaint:
      - 调 renderTarget->BeginDraw() / EndDraw()
      - Clear 到透明
      - FillRoundedRect 画 panel 背景(Liquid Glass 双层渐变,用 LinearGradientBrush,创建 1 次复用)
      - 顶部 1px 高光线(clip 到圆角内)
      - DrawBitmap 画 fluxing-logo PNG(56×56 brand 块)
      - DrawGeometry 画 5 个图标(stroke 用对应 brush)
      - DrawGeometry 时 hover 的按钮 stroke 用 m_pBrushAccent(橙),其它用 m_pBrushDim(灰)

- [ ] T005 [P1, [US070-B US070-C], [FR-003 FR-004], 2h, WeaselServer/QuickPanelDialog.cpp]
      实现 hover/active 状态机:
      - WM_MOUSEMOVE → TrackMouseEvent(TME_LEAVE)
      - WM_MOUSELEAVE → s_hoveredIdx = -1;InvalidateRect
      - WM_LBUTTONUP → s_activeIdx = hit_test;InvalidateRect;FireButton(s_activeIdx)
      - 重画时根据 s_hoveredIdx / s_activeIdx 选 brush
      - **不**用 timer 做过渡(D2D 关闭 BeginDraw/EndDraw 太重),用属性动画 timer(50ms 一次 200ms 完成)

- [ ] T006 [P1, [US070-A US070-D US070-E], [FR-002 FR-006], 3h, WeaselServer/QuickPanelDialog.cpp + WeaselServer/WeaselServerApp.h]
      集成到 WeaselServerApp:
      - WeaselServerApp::Run 末尾调 EnableQuickPanelTrigger()(L69 解禁)
      - ShowWindow 用 WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_LAYERED
      - CreateWindowExW 调 SetLayeredWindowAttributes(colorkey, alpha=240, LWA_ALPHA)
      - 1 秒失焦自动关闭(WM_KILLFOCUS 启动 SetTimer,WM_TIMER 关闭)
      - ESC 关闭(WM_KEYDOWN VK_ESCAPE → Hide)

- [ ] T007 [P1, [US070-A US070-F], [FR-002 FR-006], 2h, WeaselServer/WeaselServer.cpp + WeaselIPCServer/WeaselServerImpl.cpp]
      重新启用 Alt+, 热键(L69 已注释):
      - WeaselServerImpl::OnCreate 取消注释 ::RegisterHotKey(...Alt+, VK_OEM_COMMA)
      - 加 WeaselServerApp::OnAltPlusComma() handler: 显示 QuickPanel
      - 托盘左键单击:调 QuickPanelDialog::Show()(WeaselServerApp.cpp::SetupMenuHandlers 里改)

- [ ] T008 [P1, [US070-A], [FR-002], 1h, installer/install.nsi]
      NSIS 把 `docs/design/fluxing-logo.png` 打包进 `weasel\fluxing-logo.png`:
      ```nsis
      File "fluxing-logo.png"   ; 必须从 output\fluxing-logo.png
      ```
      加 copy 命令从 docs/design/fluxing-logo.png 到 output/fluxing-logo.png 在 build 前。

- [ ] T009 [P1, [US070-A US070-C US070-D], [FR-001..FR-007], 3h, 集成测试]
      在 dev 机器测试:
      - 装 v0.19.0
      - 登录 Fluxing 默认 IME
      - Alt+, → panel 出现(v3-rev3 视觉)
      - hover 每个按钮,图标染橙,scale 缩放
      - 5 个按钮都不崩
      - 切 en-US → 切回 Fluxing → 仍能输入中文
      - WeaselServer.exe 一直在 tasklist
      - 反复 Alt+, 100 次,无内存泄漏(RSS 增量 ≤ 5MB)
      跑 SC-001-1 到 SC-007-1 所有验收。

- [ ] T010 [P1, [US070-A], 2h, 文档化 + ship]
      - 写 L70 lessons 到 `.specify/memory/lessons-learned.md`
      - commit + tag v0.19.0 (git tag v0.19.0 5eacef7a — 等 T009 通过后)
      - 推 kizemo/Fluxing

## P2 (should) - v0.19.1 增强

- [ ] T101 [P2, [US070-F], 2h, WeaselServer/QuickPanelDialog.cpp]
      Dark mode 自动跟随:监听 WM_SETTINGCHANGE / lstrlenW(lpszSectionName)=="ImmersiveColorSet",重画。

- [ ] T102 [P2, [US070-E], 2h, WeaselServer/QuickPanelDialog.cpp]
      透明背景的 pixel-level alpha(Margins API + DwmExtendFrameIntoClientArea),让 panel 真正融入桌面,不是单色块。

- [ ] T103 [P2, [US070-B], 1h, WeaselServer/QuickPanelDialog.cpp]
      5 个按钮的 hover tooltip(WM_MOUSEMOVE + TrackMouseEvent + 600ms delay 后 CreateWindowExW(WS_EX_TOOLWINDOW) tooltip)。

## P3 (could) - v0.20+ 后续

- [ ] T201 [P3, 后续 spec, [US070-A]] Alt+, + 托盘 + FocusIn auto-show(spec 052 重新启用) + 自定义触发(将来)
- [ ] T202 [P3, 后续 spec, [US070-F]] 用户偏好的透明度/位置/自动关闭延迟 持久化到 HKCU\Software\Fluxing\QuickPanel
- [ ] T203 [P3, 后续 spec, [US070-A]] 实际功能:方案切换 / 短语加载 / 符号面板 / 设置面板 / 账号登录
- [ ] T204 [P3, 后续 spec] 主题切换 + 皮肤支持
- [ ] T205 [P3, 后续 spec] 多显示器 / 高 DPI 精细适配(PerMonitorV2 awareness)

## Verification

- [ ] V001 (verifies SC-007-1) [T009 后]: `xmake build WeaselServer` 退出码 0,无新警告
      `make install` 退出码 0,fluxing-logo.png 出现在 `D:\Program Files\fluxing\weasel\fluxing-logo.png`
- [ ] V002 (verifies SC-001-1..SC-007-1) [T009 后]: 装上 dev 机器,跑 SC-001-1 到 SC-007-1,全部 ✓
- [ ] V003 (verifies SC-007-1) [T009 后]: Windows Event Viewer 24h 内无 WeaselServer.exe Application Error 事件
- [ ] V004 (verifies SC-007-1) [T009 后]: `~/AppData/Local/CrashDumps/` 24h 内无新 WeaselServer.exe.*.dmp
- [ ] V005 [T010 后]: lessons-learned.md 含 L70 完整 post-mortem + 雷区提醒
- [ ] V006 [T010 后]: git log 显示 v0.19.0 commit hash 与 installer SHA256 一致
- [ ] V007 (verifies SC-007-1 反向:L67/L68/L69 历史问题) [T009 后]: 反复触发 Alt+, 500 次,无 STATUS_HEAP_CORRUPTION,无 RtlDeleteTimer 异常