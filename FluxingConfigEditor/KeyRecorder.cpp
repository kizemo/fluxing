#include "stdafx.h"
// spec 050 T002 - KeyRecorder.cpp
//
// Implementation of the key capture dialog. See KeyRecorder.h for
// the contract.
//
// The chord -> accept string decision does not live in this file: it
// lives in the shared unit include/KeyNameMapper.h, which the settings
// dialog, the shortcut panel and the acceptance gate all read
// (ADR 0011 D1). This file only takes the keyboard snapshot and shows
// the result, so the dialog can never mint a binding that the front end
// would not send.
//
// The character and the modifier state come from one and the same
// GetKeyboardState() snapshot: reading them one after the other could
// produce a pattern whose character and modifier do not belong together,
// which is exactly how a dead binding is born.
//
// Resource IDs (IDD_KEY_RECORDER, IDC_KEY_DISPLAY) come from
// WeaselDeployer/resource.h (the only place where dialog IDs are
// defined; FluxingConfigEditor is a library consumed by both the
// deployer and future exe targets).

#include "KeyRecorder.h"
#include "../WeaselDeployer/resource.h"  // for IDD_KEY_RECORDER + IDC_KEY_DISPLAY

#include <windowsx.h>  // GET_X_LPARAM, GET_Y_LPARAM for WM_LBUTTONDOWN

#include <KeyNameMapper.h>

namespace {

// The Simplified Chinese message for every refusal. The shared unit keeps
// its error codes in ASCII on purpose (constitution P2); the wording is the
// dialog's own.
const wchar_t* RefusalText(fluxing::AcceptError error) {
  switch (error) {
    case fluxing::kAcceptNoName:
      return L"\x8be5\x7ec4\x5408\x952e\x65e0\x6cd5\x8868\x8fbe\xff0c\x8bf7"
             L"\x6309\x4f4f\x0020\x0053\x0068\x0069\x0066\x0074\x0020\x518d"
             L"\x8bd5";
    case fluxing::kAcceptModifierKey:
      return L"\x4fee\x9970\x952e\x4e0d\x80fd\x5355\x72ec\x4f5c\x4e3a\x5feb"
             L"\x6377\x952e\xff0c\x8bf7\x518d\x6309\x4e00\x4e2a\x666e\x901a"
             L"\x952e";
    case fluxing::kAcceptUnprintable:
      return L"\x8be5\x6309\x952e\x65e0\x6cd5\x4f5c\x4e3a\x5feb\x6377\x952e";
    case fluxing::kAcceptWinKey:
      return L"\x4e0d\x652f\x6301\x0020\x0057\x0069\x006e\x0020\x952e\xff0c"
             L"\x8bf7\x6539\x7528\x0020\x0043\x0074\x0072\x006c\x0020\x002f"
             L"\x0020\x0053\x0068\x0069\x0066\x0074\x0020\x002f\x0020\x0041"
             L"\x006c\x0074\x0020\x7ec4\x5408";
    case fluxing::kAcceptNoCharacter:
    default:
      return L"\x672a\x77e5\x6309\x952e\xff0c\x8bf7\x91cd\x8bd5";
  }
}

// The modifier part of the live display, e.g. "Control+Shift+".
std::wstring ModifierPrefix(const BYTE* state) {
  std::wstring prefix;
  if ((state[VK_CONTROL] & 0x80) != 0)
    prefix += L"Control+";
  if ((state[VK_SHIFT] & 0x80) != 0)
    prefix += L"Shift+";
  if ((state[VK_MENU] & 0x80) != 0)
    prefix += L"Alt+";
  if ((state[VK_LWIN] & 0x80) != 0 || (state[VK_RWIN] & 0x80) != 0)
    prefix += L"Win+";
  return prefix;
}

}  // namespace

LRESULT KeyRecorderDialog::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
  CenterWindow(GetParent());
  display_label_ = GetDlgItem(IDC_KEY_DISPLAY);
  display_label_.SetWindowTextW(
      L"\x8bf7\x6309\x7ec4\x5408\x952e\x002e\x002e\x002e");
  accept_.clear();
  captured_ = false;
  // Start auto-cancel timer (using CWindow::SetTimer which takes id + ms).
  timer_id_ = SetTimer(1, (UINT)timeout_ms_, nullptr);
  return TRUE;
}

LRESULT KeyRecorderDialog::OnKeyDown(UINT,
                                     WPARAM wParam,
                                     LPARAM lParam,
                                     BOOL&) {
  const UINT vkey = static_cast<UINT>(wParam);
  if (vkey == VK_SHIFT || vkey == VK_CONTROL || vkey == VK_MENU ||
      vkey == VK_LWIN || vkey == VK_RWIN || vkey == VK_CAPITAL) {
    ComposeAndDisplay();
    return 0;
  }
  accept_.clear();
  captured_ = false;
  BYTE state[256];
  if (!GetKeyboardState(state)) {
    display_label_.SetWindowTextW(RefusalText(fluxing::kAcceptNoCharacter));
    return 0;
  }
  const fluxing::CapturedAccept captured =
      fluxing::CaptureAccept(vkey, KeyInfo(lParam), state, NULL);
  if (!captured.ok) {
    // The chord cannot be written down as a binding: say why, in Chinese.
    display_label_.SetWindowTextW(RefusalText(captured.error));
    return 0;
  }
  accept_ = captured.pattern;
  display_ = accept_;
  display_label_.SetWindowTextW(display_.c_str());
  captured_ = true;
  return 0;
}

LRESULT KeyRecorderDialog::OnSysKeyDown(UINT,
                                        WPARAM wParam,
                                        LPARAM lParam,
                                        BOOL&) {
  // Alt+<key> comes through as WM_SYSKEYDOWN. Route to the same
  // capture logic. We invoke OnKeyDown with the WM_KEYDOWN
  // signature; the lParam is forwarded.
  BOOL b = FALSE;
  return OnKeyDown(WM_KEYDOWN, wParam, lParam, b);
}

LRESULT KeyRecorderDialog::OnLButtonDown(UINT, WPARAM, LPARAM lParam, BOOL&) {
  RECT rc;
  GetClientRect(&rc);
  POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
  if (!PtInRect(&rc, pt)) {
    EndDialog(IDCANCEL);
  }
  return 0;
}

LRESULT KeyRecorderDialog::OnOK(WORD, WORD, HWND, BOOL&) {
  if (!captured_)
    return 0;
  if (timer_id_) {
    KillTimer(timer_id_);
    timer_id_ = 0;
  }
  EndDialog(IDOK);
  return 0;
}

LRESULT KeyRecorderDialog::OnCancel(WORD, WORD, HWND, BOOL&) {
  if (timer_id_) {
    KillTimer(timer_id_);
    timer_id_ = 0;
  }
  EndDialog(IDCANCEL);
  return 0;
}

void KeyRecorderDialog::ComposeAndDisplay() {
  BYTE state[256];
  std::wstring prefix;
  if (GetKeyboardState(state))
    prefix = ModifierPrefix(state);
  const std::wstring text = prefix + L"_";
  display_label_.SetWindowTextW(text.c_str());
}

void KeyRecorderDialog::Reset() {
  accept_.clear();
  display_.clear();
  captured_ = false;
  display_label_.SetWindowTextW(
      L"\x8bf7\x6309\x7ec4\x5408\x952e\x002e\x002e\x002e");
}
