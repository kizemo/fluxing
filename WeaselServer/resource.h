//{{NO_DEPENDENCIES}}
// Microsoft Visual C++ generated include file.
// Used by WeaselServer.rc
//
#define IDI_WEASEL                      100
#define IDI_EN                          101
#define IDI_ZH                          102
#define IDI_RELOAD                      103
#define IDR_MENU_POPUP                  105
#define IDI_FULL_SHAPE                  106
#define IDI_HALF_SHAPE                  107
#define IDS_STR_SYSTEM_VERSION_WARNING_CAPTION 300
#define IDS_STR_SYSTEM_VERSION_WARNING  301
#define IDS_STR_UNDER_MAINTENANCE       302
#define ID_WEASELTRAY_QUIT              40001
#define ID_WEASELTRAY_DEPLOY            40002
#define ID_WEASELTRAY_CHECKUPDATE       40003
#define ID_WEASELTRAY_FORUM             40004
#define ID_WEASELTRAY_HOMEPAGE          40005
#define ID_WEASELTRAY_INSTALLDIR        40006
#define ID_WEASELTRAY_USERCONFIG        40007
#define ID_WEASELTRAY_SETTINGS          40008
#define ID_WEASELTRAY_WIKI              40009
#define ID_WEASELTRAY_DICT_MANAGEMENT   40010
#define ID_WEASELTRAY_SYNC              40012
#define ID_WEASELTRAY_ENABLE_ASCII      40013
#define ID_WEASELTRAY_DISABLE_ASCII     40014
#define ID_WEASELTRAY_RERUN_SERVICE     40015
#define ID_WEASELTRAY_LOGDIR            40016
#define ID_WEASELTRAY_RESTORE_IGNORED   40017

// spec 036: tray quick panel v0
#define ID_HOTKEY_QUICK_PANEL          9001
#define ID_WEASELTRAY_QUICK_PANEL      40018

// spec 042: Alt+. global hotkey → PhrasesDialog::Show
#define ID_HOTKEY_PHRASES_DOT          9002

// spec 045 v0.19.0.28: Ctrl+Shift+K global hotkey → ShortcutSettings::Show()
#define ID_HOTKEY_SHORTCUT             9004

// v0.19.0.32-fix (Bug 3b): Alt+/ 原计划 → UserDictionary::Show() 但 v0.19.0.33 已 reroute 到 PhrasesDialog
// (跟 QuickPanel Button 共享入口)
//   / 键的虚拟键码是 VK_OEM_2 (= 0xBF)
// v0.19.0.60 (Phase L 调整 1): ID 保留(行为已 reroute 到 PhrasesDialog),但宏名保留作为历史 audit
#define ID_HOTKEY_USER_DICT_ALT_SLASH  9005

// v0.19.0.60 (Phase L 调整 1): ID_HOTKEY_USER_DICT (9003) 删除 — Ctrl+Shift+U 已停用
// 历史: spec 044 原本绑 UserDictionary::Show;现 UserDictionary 模块整体下线

// spec 052 v0.18.31.0: enable QuickPanel always-show mode (20% alpha)
#define ID_QUICKPANEL_ALWAYS_SHOW      40019
#define IDR_FLUXING_LOGO                108
#define ID_QUICKPANEL_BTN_ASCII        41001
#define ID_QUICKPANEL_BTN_DEPLOY       41002


// Next default values for new objects
// 
#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NEXT_RESOURCE_VALUE        104
#define _APS_NEXT_COMMAND_VALUE         40003
#define _APS_NEXT_CONTROL_VALUE         1001
#define _APS_NEXT_SYMED_VALUE           101
#endif
#endif
