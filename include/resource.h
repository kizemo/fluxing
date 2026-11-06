//{{NO_DEPENDENCIES}}
// Microsoft Visual C++ generated include file.
// Used by WeaselTSF.rc
//
#define IDI_WEASEL                      100
#define IDI_EN                          101
#define IDI_ZH                          102
#define IDI_RELOAD                      103
// spec 100 / ADR 0038: 菜单唯一化到 WeaselServer 的 IDR_MENU_POPUP。
// WeaselTSF.rc 的三套 MENU 资源(英/繁/简)已删除 —— 但这个 id **不能删**:
// WeaselServer 的 WeaselTrayIcon.cpp 用 `#include <resource.h>`(尖括号 ⇒ 命中
// 本文件,不是 WeaselServer/resource.h)引用它。同号定义也存在于
// WeaselServer/resource.h,两边取值必须一致(都是 105)。
#define IDR_MENU_POPUP                  105
#define IDI_FULL_SHAPE                  106
#define IDI_HALF_SHAPE                  107
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

// spec 036 + L51: tray quick panel v0
#define ID_HOTKEY_QUICK_PANEL          9001
#define ID_WEASELTRAY_QUICK_PANEL      40018

// spec 084 / ADR 0013 D4 + spec 086: quick panel always-show switch.
// The same id also lives in WeaselServer/resource.h - both values
// must stay equal, because WeaselServer.rc carries its own popup menu
// and still needs this entry.
//
// spec 091 / ADR 0024 + spec 100 / ADR 0038: the always-show switch lives
// **only** in the WeaselServer popup menu (IDR_MENU_POPUP in WeaselServer.rc).
// The language bar popup menu was removed entirely in spec 100 (ADR 0038),
// so this entry now has exactly one owner. The id stays reserved here so
// both headers stay in sync.
#define ID_QUICKPANEL_ALWAYS_SHOW      40019

// spec 037 / ADR 0037 + spec 100 / ADR 0038: import Sogou user dictionary.
// The entry now lives **only** in the WeaselServer popup menu; WeaselServer
// registers AddMenuHandler(ID_WEASELTRAY_SOGOU_IMPORT) and launches
// "WeaselDeployer.exe /sogou". The WeaselTSF language bar no longer owns a
// menu (its three MENU resources were deleted), so nothing forwards this id
// from the TSF side any more.
// The same id also lives in WeaselServer/resource.h - both values must
// stay equal.
#define ID_WEASELTRAY_SOGOU_IMPORT      40022

// Next default values for new objects
//
#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NEXT_RESOURCE_VALUE        104
#define _APS_NEXT_COMMAND_VALUE         40001
#define _APS_NEXT_CONTROL_VALUE         1001
#define _APS_NEXT_SYMED_VALUE           101
#endif
#endif
