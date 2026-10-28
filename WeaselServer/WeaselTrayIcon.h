#pragma once
#include <WeaselUI.h>
#include <WeaselIPC.h>
#include "SystemTraySDK.h"
#include <functional>

#define WM_WEASEL_TRAY_NOTIFY (WEASEL_IPC_LAST_COMMAND + 100)

class WeaselTrayIcon : public CSystemTray {
 public:
  enum WeaselTrayMode {
    INITIAL,
    ZHUNG,
    ASCII,
    DISABLED,
  };

  WeaselTrayIcon(weasel::UI& ui);

  BOOL Create(HWND hTargetWnd);
  void Refresh();

  // spec 084 / ADR 0013 D10: 可选钩子 — 菜单弹出前逐项询问是否打勾。
  // 返回 true 的项会被 CheckMenuItem 打上勾;默认空 => 菜单不打勾。
  void SetMenuItemCheckedCallback(std::function<bool(UINT)> fn);

 protected:
  virtual void CustomizeMenu(HMENU hMenu);

  weasel::UIStyle& m_style;
  weasel::Status& m_status;
  WeaselTrayMode m_mode;
  std::wstring m_schema_zhung_icon;
  std::wstring m_schema_ascii_icon;
  bool m_disabled;
  std::function<bool(UINT)> m_menuItemChecked;
};
