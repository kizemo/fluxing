# spec 052 计划 - QuickPanel 长显模式

## 1. 技术上下文

- v0.18.29.0 (spec 045) 已 ship QuickPanel 8 入口 mac 风格
- 当前 Show() / Hide() / OnKillFocus / OnTimer 行为见 QuickPanelDialog.cpp
- 现有 Alt+, 路由：OnHotkey → PostMessage WM_COMMAND → WeaselServerApp menu handler → QuickPanelDialog::Show

## 2. 技术方案

### 2.1 QuickPanelDialog.h 改动

```cpp
class QuickPanelDialog {
 public:
  enum class Mode { kHidden, kAlwaysShow, kFocused };

  // 已有：Show(...)
  // 已有：Hide()

  // 新增：
  static void SetMode(Mode m);          // 内部状态机切换
  static void ToggleMode();              // 公开给 Alt+ handler
  static void EnableAlwaysShowMode();    // 启动时调用：创建 + 20% 长显
  static Mode CurrentMode();             // 查询（用于测试/调试）

 private:
  static void SetOpacity(int alpha);     // 0-255
  static void OnPointerEnter();           // 内部 hover 处理
  static void OnPointerLeave();           // 0.5s 后回 20%
  static Mode s_mode;
  static int s_alpha;
  static bool s_mouse_tracked;
};
```

### 2.2 QuickPanelDialog.cpp 改动

#### 2.2.1 窗口创建（CreateWindowEx）

```cpp
HWND hwnd = CreateWindowExW(
  WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,  // 新加 LAYERED
  kClassName, ...,
  WS_POPUP | WS_BORDER, ...);
SetLayeredWindowAttributes(hwnd, 0, 51, LWA_ALPHA);  // 立即 20%
```

#### 2.2.2 WndProc 加 handler

```cpp
BEGIN_MSG_MAP(QuickPanelDialog)
  MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)        // 新加
  MESSAGE_HANDLER(WM_MOUSELEAVE, OnMouseLeave)     // 新加
  MESSAGE_HANDLER(WM_NCMOUSEMOVE, OnNcMouseMove)  // 新加（caption 区域）
  ...
END_MSG_MAP()
```

#### 2.2.3 鼠标 hover 处理

```cpp
LRESULT OnMouseMove(UINT, WPARAM, LPARAM, BOOL&) {
  if (s_mode == Mode::kAlwaysShow) {
    SetOpacity(255);  // 100%
    if (!s_mouse_tracked) {
      TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, s_hwnd, 0};
      TrackMouseEvent(&tme);
      s_mouse_tracked = true;
    }
  }
  return 0;
}

LRESULT OnMouseLeave(UINT, WPARAM, LPARAM, BOOL&) {
  s_mouse_tracked = false;
  if (s_mode == Mode::kAlwaysShow) {
    SetOpacity(51);  // 回 20%
  }
  return 0;
}
```

#### 2.2.4 ToggleMode

```cpp
void ToggleMode() {
  if (s_mode == Mode::kHidden) {
    EnableAlwaysShowMode();  // 创建 + 20% 长显
  } else {
    SetMode(Mode::kHidden);
    ShowWindow(s_hwnd, SW_HIDE);
  }
}

void EnableAlwaysShowMode() {
  s_mode = Mode::kAlwaysShow;
  if (!s_hwnd) {
    Show(...);  // 复用现有 Show() 创建窗口
  }
  ShowWindow(s_hwnd, SW_SHOWNOACTIVATE);
  SetOpacity(51);  // 20%
}
```

### 2.3 WeaselServerApp.cpp 改动

```cpp
// 1) 新加 menu handler（启动时调用）
m_server.AddMenuHandler(ID_QUICKPANEL_ALWAYS_SHOW, [this] {
  QuickPanelDialog::EnableAlwaysShowMode();
});

// 2) 修改 Alt+, handler：Show → ToggleMode
m_server.AddMenuHandler(ID_WEASELTRAY_QUICK_PANEL, [this] {
  // 已有：拉取状态 + 调 Show()
  // 改为：调 ToggleMode()
  QuickPanelDialog::ToggleMode();
});
```

### 2.4 WeaselServerImpl.cpp 改动

Alt+ handler 不变（仍 PostMessage WM_COMMAND ID_WEASELTRAY_QUICK_PANEL 0），但下游 handler 改成 ToggleMode。

### 2.5 resource.h 改动

```cpp
#define ID_QUICKPANEL_ALWAYS_SHOW   40019  // 新加
```

### 2.6 启动时触发

`WeaselServerApp::Run()` 在 WeaselServer 启动完成后调：
```cpp
// 切到火流猩输入法时（service 启动或激活）→ 触发长显
QuickPanelDialog::EnableAlwaysShowMode();
```

## 3. 任务粒度（R5: < 4h, 1-3 files）

- T001: QuickPanelDialog.h 加 Mode 枚举 + 新方法声明（30 min）
- T002: QuickPanelDialog.cpp 加 SetMode / SetOpacity / EnableAlwaysShowMode / ToggleMode（1.5 h）
- T003: QuickPanelDialog.cpp 加 WM_MOUSEMOVE / WM_MOUSELEAVE / WM_NCMOUSEMOVE handlers（1 h）
- T004: resource.h 加 ID_QUICKPANEL_ALWAYS_SHOW（5 min）
- T005: WeaselServerApp.cpp 修改 Alt+ handler + 加启动时触发（30 min）
- T006: xmake build 0 errors（10 min）
- T007: 跑测试套件（5 min）
- T008: 重打 installer v0.18.31.0（5 min）
- T009: commit + CHANGELOG（10 min）
- **总计**: 4.25 h

## 4. 依赖

- v0.18.29.0 spec 045（QuickPanel 8 入口已 ship）
- v0.18.30.0 spec 050（installer pipeline 已就位）
- WTL / ATL（已 vendored）

## 5. 验收回执

- [ ] T-A1: xmake -y 0 errors 0 warnings
- [ ] T-A2: TestQuickPanelDialog 10/10 PASS（不回归）
- [ ] T-A3: installer 包含新行为（字符串搜索 ToggleMode / EnableAlwaysShowMode 命中）
- [ ] T-A4: 重启输入法服务即可生效（无需重启系统）
- [ ] T-A5: 切到火流猩 → 面板自动显示 20%
- [ ] T-A6: 鼠标悬停 → 100%
- [ ] T-A7: Alt+, → 隐藏/显示切换

## 6. 风险

- WM_MOUSELEAVE 在 TSF 内部 hover 不触发（已知 Windows 行为，需用 TrackMouseEvent 补）
- 切走火流猩输入法时面板不会自动隐藏（v0.19 再做）
- 测试 QuickPanelDialog 是 10/10 老的，行为改了不会回归（创建窗口路径不变）