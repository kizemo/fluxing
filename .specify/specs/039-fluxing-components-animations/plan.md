# 039 · Plan · FluxingComponents 动画扩展 + 4 新控件

> YAGNI v1 切片。spec 037 (基础 4 控件) + spec 038 (QuickPanelDialog 重构) 之后的第三阶段 ship 切片。本 spec ship 5 个 P1 控件动画状态 + 4 个新控件 + 2 个新测试项目。

## 1. 技术上下文

- **D2D / DirectWrite** (与 spec 037 相同栈): ID2D1Factory / ID2D1HwndRenderTarget / IDWriteTextFormat / D2D1_COLOR_F interpolation。
- **修改文件**: WeaselUI/FluxingComponents/Button.{h,cpp}, Toggle.{h,cpp}, Panel.{h,cpp}, Label.{h,cpp} (加 hover/press 状态机), WeaselUI/WeaselUI.vcxproj (加 4 新 .cpp + stdafx.cpp for Slider/Dropdown/Checkbox/Radio), WeaselUI/xmake.lua (同步), weasel.sln (加 2 新测试项目)。
- **新建文件** (4 新控件 + 2 新测试项目):
  - WeaselUI/FluxingComponents/Slider.h + .cpp (200x24, 圆角 12px, 圆头 20px, 拖动 SetValue 0-100)
  - WeaselUI/FluxingComponents/Dropdown.h + .cpp (200x30, 子菜单 HWND popup)
  - WeaselUI/FluxingComponents/Checkbox.h + .cpp (200x20, 方框 + 打勾)
  - WeaselUI/FluxingComponents/Radio.h + .cpp (200x20, 圆圈 + 实心点)
  - test/TestFluxingHover/ (新项目, 8+ assertions: 4 控件 × 2 状态)
  - test/TestFluxingComponentsV2/ (新项目, 16+ assertions: 4 新控件 × 4 assertions)
- **Palette 扩展**: FluxingDarkModeBridge 加 hilited_back / pressed_back / hover_track / pressed_track / hilited_border / hilited_text 颜色常量 (spec 033 + 039 联合)。
- 不引入新依赖 (d2d1 / dwrite / windowscodecs 已在 WeaselUI link 配置)。
- 不修改 install.nsi。

## 2. 架构

### 2.1 命名空间与目录

```
WeaselUI/
  FluxingComponents/
    Slider.h + .cpp    [新]
    Dropdown.h + .cpp  [新]
    Checkbox.h + .cpp  [新]
    Radio.h + .cpp     [新]
    Button.{h,cpp}     [改: 加 hover/press]
    Toggle.{h,cpp}     [改: 加 hover/press]
    Panel.{h,cpp}      [改: 加 hover]
    Label.{h,cpp}      [改: 加 hover (Large)]
```

namespace `fluxing::ui`, class 前缀 Fluxing。

### 2.2 hover/press 状态机

每个控件维护 enum State { Normal, Hover, Pressed, Transitioning } + 一个 200ms timer + progress 0.0-1.0。颜色通过 FluxingTheme::Instance().CurrentPalette().InterpolateColor(from, to, progress) 实时计算。spec 039 v1 使用固定 200ms 时长, spec 040+ 可配置。

### 2.3 Slider 拖动

FluxingSlider 处理 WM_LBUTTONDOWN (开始拖动, SetCapture) + WM_MOUSEMOVE (节流到 16ms = 60 FPS, 调 OnChanged) + WM_LBUTTONUP (结束拖动, ReleaseCapture)。value 0-100 映射到 0..width-padding*2 像素。

### 2.4 Dropdown 子菜单

FluxingDropdown 创建时接收 `std::vector<std::wstring> items` + OnSelected callback。点击时 `CreateWindowExW(0, L"FLUXING_DROPDOWN_POPUP", ..., WS_POPUP)` 在控件下方弹出。popup HWND 处理 WM_LBUTTONUP + WM_KEYDOWN + WM_KILLFOCUS (Esc/失焦关闭)。

### 2.5 Checkbox/Radio

FluxingCheckbox 处理 WM_LBUTTONUP -> toggle checked + Repaint + OnChanged。FluxingRadio 同, 但多一个 RadioGroup helper (TestFluxingComponentsV2 提供) 用于一组 Radio 的互斥管理。

## 3. 验收 (L46 recipe)

- Path 1 xbuild.bat weasel installer -> exit 0, installer ~43 MB (4 新 .cpp 加 5 改 .cpp 加 ~150KB)
- Path 2 msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32 /m -> 0 errors
- Path 3 scripts\test-infra\run-test-suite.bat -> 17/17 PASS (was 15/15 + TestFluxingHover + TestFluxingComponentsV2), 200+ assertions / 0 FAIL
- L42 byte-verify: 0x1E1E1E 仍在 weasel.dll + 新 0x2D2D30 / 0x1F1F22 / 0x353539 颜色字节存在
- L14 arch-verify: 6 binary 全部 arch 一致 (x86=0x14C, x64=0x8664, ARM64=0xAA64)
- L47 byte-verify: 全部 4 新 .h+.cpp + 2 新 vcxproj byte-healthy
- L48 防御性测试退出模式: TestFluxingHover + TestFluxingComponentsV2 用 ExitProcess(rc) 跳过 atexit static destructors

## 4. 风险缓解

- R039-A (D2D render target 掉帧): 200ms timer + 60 FPS 节流 + 复用 FluxingD2DRenderer HWND cache
- R039-B (Slider 回调过频): WM_MOUSEMOVE 节流到 16ms, OnChanged 只在 value 变化时触发
- R039-C (Dropdown 多 DPI 错位): v1 仅 96 DPI baseline, spec 041 处理 DPI
- R039-D (Radio 互斥): RadioGroup helper 在测试侧, 生产侧 QuickPanelDialog 用 lambda 自行管理

## 5. 推迟原因

- Grid 布局: FluxingPanelHost 重构, YAGNI v1
- 多 DPI: DPI 100/150/200 验证, v1 仅 96 DPI
- 拖动: DWM 拖动 API + z-order, v2+
- 子菜单: popup stacking 复杂, YAGNI
