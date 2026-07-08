// spec 050 T002 - KeyRecorder.h
//
// Minimal modal dialog that captures a key combination from the
// user and returns it as a librime-style "accept" string
// (e.g. "Control+Shift+F1"). Pure Win32 + WTL; no spec 037
// FluxingComponents dependency (the editor uses native Win32
// controls throughout MVP).
//
// Usage (from HotkeyEditorDialog::OnAdd / OnEdit):
//   KeyRecorderDialog dlg;
//   if (dlg.DoModal() == IDOK) {
//     std::wstring accept = dlg.accept();
//     // ... insert into bindings list ...
//   }
//
// Implementation detail: WM_KEYDOWN is captured and translated
// via GetAsyncKeyState (modifiers) + MapVirtualKeyW +
// GetKeyNameTextW (key name). Output matches librime's accept
// string format (e.g. "Control+Shift+Tab" not "Ctrl+Shift+Tab").
// Per L21, "Shift+Shift_L" style is preferred over bare "Shift_L"
// to avoid TSF release-event collisions; this dialog always
// includes any active modifier in the output, so users naturally
// produce the safe form.

#pragma onc

#include "../WeaselDeployer/resource.h"  // for IDD_KEY_RECORDER, IDC_KEY_DISPLAY


#include <string>
#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlcrack.h>
#include <atlctrls.h>

class KeyRecorderDialog : public ATL::CDialogImpl<KeyRecorderDialog> {
 public:
  enum { IDD = IDD_KEY_RECORDER };

  BEGIN_MSG_MAP(KeyRecorderDialog)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    MESSAGE_HANDLER(WM_KEYDOWN, OnKeyDown)
    MESSAGE_HANDLER(WM_SYSKEYDOWN, OnSysKeyDown)
    MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
    COMMAND_ID_HANDLER(IDOK, OnOK)
    COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
  END_MSG_MAP()

  // The captured accept string, valid only after DoModal returns IDOK.
  const std::wstring& accept() const { return accept_; }

  // Pre-populate the captured accept (for Edit flow). When set, the
  // dialog shows the initial value as a starting point and accepts
  // the next keypress as the new value (the user can press OK without
  // pressing any new key to keep the original).
  void set_initial_accept(const std::wstring& s) { accept_ = s; display_ = s; captured_ = true; }

  // Time (ms) before auto-cancel if no key is pressed. Default 10s.
  void set_timeout_ms(int ms) { timeout_ms_ = ms; }

 private:
  LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnKeyDown(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnSysKeyDown(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnLButtonDown(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnOK(WORD, WORD, HWND, BOOL&);
  LRESULT OnCancel(WORD, WORD, HWND, BOOL&);

  // Compose the current accept string from current modifier state +
  // last pressed non-modifier key, and update the display.
  void ComposeAndDisplay();
  // Reset the recorded state (e.g. on ESC within the dialog).
  void Reset();

  std::wstring accept_;
  std::wstring display_;  // the rendered "Control+Shift+F1" string
  bool captured_ = false;  // true after first non-modifier key pressed
  UINT_PTR timer_id_ = 0;
  int timeout_ms_ = 10000;

  CStatic display_label_;
};