#pragma once
#include <functional>
#include <string>
#include <vector>
#include <windows.h>

class QuickPanelDialog {
 public:
  using OnClick = std::function<void()>;
  using OnToggle = std::function<void(bool)>;

  enum class Mode { kHidden, kAlwaysShow };

  static void Show(bool currentFullwidth,
                   OnClick onSchema,
                   OnClick onUserFolder,
                   OnClick onPhrases,
                   OnToggle onFullwidth,
                   OnClick onSymbols,
                   OnClick onLogin);
  static void Hide();
  static void ToggleMode();

  // spec 061: EnableAlwaysShowMode now takes the same 6 callbacks
  // as Show(), stored as static members. This ensures that when
  // ToggleMode() triggers EnableAlwaysShowMode (e.g. from right-click
  // menu), the buttons have real callbacks - not null lambdas.
  // The previous spec 060 fix called EnableAlwaysShowMode() without
  // callbacks, so button clicks were no-ops (FireButton checked
  // if (s_onSchema) and skipped when null).
  static void EnableAlwaysShowMode(
      OnClick onSchema,
      OnClick onUserFolder,
      OnClick onPhrases,
      OnToggle onFullwidth,
      OnClick onSymbols,
      OnClick onLogin);

  static Mode CurrentMode() { return s_mode; }
  static HWND ActiveHwnd() { return s_hwnd; }
  static int  CurrentAlpha() { return s_alpha; }
  static bool IsFullwidth() { return s_fullwidth; }

  static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

  // All static data members are public so anonymous namespace callbacks
  // (FadeTimerProc, FireButton) can access them directly.
  static HWND     s_hwnd;
  static Mode     s_mode;
  static bool     s_fullwidth;
  static bool     s_mouseTracked;
  static int      s_alpha;
  static int      s_targetAlpha;
  static OnClick  s_onSchema;
  static OnClick  s_onUserFolder;
  static OnClick  s_onPhrases;
  static OnToggle s_onFullwidth;
  static OnClick  s_onSymbols;
  static OnClick  s_onLogin;

  static LRESULT OnCreate(HWND);
  static LRESULT OnPaint(HWND);
  static LRESULT OnLButtonUp(HWND, int, int);
  static void    OnMouseMove(HWND);
  static void    OnMouseLeave(HWND);
  static LRESULT OnTimer(HWND, WPARAM);
  static LRESULT OnDestroy(HWND);
};
