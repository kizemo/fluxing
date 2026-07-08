// spec 050 T002 - KeyRecorder.cpp
//
// Implementation of the key capture dialog. See KeyRecorder.h for
// the contract. Notes:
//   - Resource IDs (IDD_KEY_RECORDER, IDC_KEY_DISPLAY) come from
//     WeaselDeployer/resource.h (the only place where dialog IDs
//     are defined; FluxingConfigEditor is a library consumed by
//     both the deployer and future exe targets).
//   - MapKeyCodeToLibrimeName is in an anonymous namespace and is
//     also referenced by OnKeyDown (member function); for the
//     function-local use, the namespace makes it internal-linkage
//     but accessible from the same translation unit.

#include "KeyRecorder.h"
#include "../WeaselDeployer/resource.h"  // for IDD_KEY_RECORDER + IDC_KEY_DISPLAY

#include <windowsx.h>  // GET_X_LPARAM, GET_Y_LPARAM for WM_LBUTTONDOWN

namespace {

std::wstring MapKeyCodeToLibrimeName(UINT vk) {
  switch (vk) {
    case VK_BACK:    return L"BackSpace";
    case VK_TAB:     return L"Tab";
    case VK_RETURN:  return L"Return";
    case VK_ESCAPE:  return L"Escape";
    case VK_PRIOR:   return L"Page_Up";
    case VK_NEXT:    return L"Page_Down";
    case VK_END:     return L"End";
    case VK_HOME:    return L"Home";
    case VK_LEFT:    return L"Left";
    case VK_UP:      return L"Up";
    case VK_RIGHT:   return L"Right";
    case VK_DOWN:    return L"Down";
    case VK_INSERT:  return L"Insert";
    case VK_DELETE:  return L"Delete";
    case VK_SPACE:   return L"space";
    case VK_OEM_COMMA:   return L"comma";
    case VK_OEM_PERIOD:  return L"period";
    case VK_OEM_MINUS:   return L"minus";
    case VK_OEM_PLUS:    return L"plus";
    case VK_OEM_2:       return L"slash";
    case VK_OEM_3:       return L"grave";
    case VK_OEM_4:       return L"bracketleft";
    case VK_OEM_5:       return L"backslash";
    case VK_OEM_6:       return L"bracketright";
    case VK_OEM_7:       return L"apostrophe";
    case VK_OEM_1:       return L"semicolon";
    case VK_OEM_102:     return L"backslash";
    case VK_NUMPAD0: case VK_NUMPAD1: case VK_NUMPAD2:
    case VK_NUMPAD3: case VK_NUMPAD4: case VK_NUMPAD5:
    case VK_NUMPAD6: case VK_NUMPAD7: case VK_NUMPAD8:
    case VK_NUMPAD9: {
      wchar_t buf[8];
      swprintf_s(buf, L"KP_%d", vk - VK_NUMPAD0);
      return buf;
    }
    case VK_MULTIPLY:    return L"KP_Multiply";
    case VK_ADD:         return L"KP_Add";
    case VK_SUBTRACT:    return L"KP_Subtract";
    case VK_DECIMAL:     return L"KP_Decimal";
    case VK_DIVIDE:      return L"KP_Divide";
    default: break;
  }
  if (vk >= 'A' && vk <= 'Z') {
    wchar_t buf[2] = {(wchar_t)(vk - 'A' + 'a'), 0};
    return buf;
  }
  if (vk >= '0' && vk <= '9') {
    wchar_t buf[2] = {(wchar_t)vk, 0};
    return buf;
  }
  if (vk >= VK_F1 && vk <= VK_F24) {
    wchar_t buf[8];
    swprintf_s(buf, L"F%d", vk - VK_F1 + 1);
    return buf;
  }
  UINT sc = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
  if (sc == 0) return L"";
  wchar_t name[64] = {0};
  int len = GetKeyNameTextW((LONG)((sc << 16) | 0x1), name, 64);
  if (len <= 0) return L"";
  for (int i = 0; i < len; ++i) {
    if (name[i] == L' ') name[i] = L'_';
  }
  return std::wstring(name);
}

}  // namespace

LRESULT KeyRecorderDialog::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
  CenterWindow(GetParent());
  display_label_ = GetDlgItem(IDC_KEY_DISPLAY);
  display_label_.SetWindowTextW(L"\xe7\xa2\x9c\xe6\x8c\x91\xe7\xbb\x84\xe5\x90\x88\xe9\x94\xae...");  // "请按组合键..."
  accept_.clear();
  captured_ = false;
  // Start auto-cancel timer (using CWindow::SetTimer which takes id + ms).
  timer_id_ = SetTimer(1, (UINT)timeout_ms_, nullptr);
  return TRUE;
}

LRESULT KeyRecorderDialog::OnKeyDown(UINT, WPARAM wParam, LPARAM, BOOL&) {
  if (wParam == VK_SHIFT || wParam == VK_CONTROL || wParam == VK_MENU ||
      wParam == VK_LWIN || wParam == VK_RWIN || wParam == VK_CAPITAL) {
    ComposeAndDisplay();
    return 0;
  }
  accept_.clear();
  bool ctrl  = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
  bool shift = (GetAsyncKeyState(VK_SHIFT)   & 0x8000) != 0;
  bool alt   = (GetAsyncKeyState(VK_MENU)     & 0x8000) != 0;
  bool win   = (GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 ||
               (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0;
  if (ctrl)  accept_ += L"Control+";
  if (shift) accept_ += L"Shift+";
  if (alt)   accept_ += L"Alt+";
  if (win)   accept_ += L"Control+";
  std::wstring key_name = MapKeyCodeToLibrimeName((UINT)wParam);
  if (key_name.empty()) {
    display_label_.SetWindowTextW(L"\xe6\x9c\xaa\xe7\x9f\xa5\xe9\x94\xae\xef\xbc\x8c\xe8\xaf\xb7\xe9\x87\x8d\xe8\xaf\x95");
    captured_ = false;
    return 0;
  }
  accept_ += key_name;
  display_ = accept_;
  display_label_.SetWindowTextW(display_.c_str());
  captured_ = true;
  return 0;
}

LRESULT KeyRecorderDialog::OnSysKeyDown(UINT, WPARAM wParam, LPARAM lParam, BOOL&) {
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
  if (!captured_) return 0;
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
  bool ctrl  = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
  bool shift = (GetAsyncKeyState(VK_SHIFT)   & 0x8000) != 0;
  bool alt   = (GetAsyncKeyState(VK_MENU)     & 0x8000) != 0;
  bool win   = (GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 ||
               (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0;
  std::wstring s;
  if (ctrl)  s += L"Control+";
  if (shift) s += L"Shift+";
  if (alt)   s += L"Alt+";
  if (win)   s += L"Control+";
  s += L"_";
  display_label_.SetWindowTextW(s.c_str());
}

void KeyRecorderDialog::Reset() {
  accept_.clear();
  display_.clear();
  captured_ = false;
  display_label_.SetWindowTextW(L"\xe7\xa2\x9c\xe6\x8c\x91\xe7\xbb\x84\xe5\x90\x88\xe9\x94\xae...");
}