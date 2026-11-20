#pragma once
//
// 批次 AT-1 · 账号页 UI 重构（登录态区分 + 独立子窗口）
//
// 本头是「账号页控件可见性 + 布局」的**唯一真相源**：生产实现
// （WeaselServer/Cloud/CloudAccountDialog.cpp）与单测
// （test/TestFluxingCloud）共用同一份规则 —— constitution R7
// 「生产与单测共用一套实现」。账号页的 UI 本体无法自动化，
// 但「哪个窗口有哪几个控件、哪个登录态下哪些控件可见、控件摆在哪」
// 这三条规则可以（且必须）被断言。
//
// PCH 注意：本头**故意**不包含 <windows.h> ⇒ 任何工程（含单测工程）
// 都能直接包含它，不会与 windows.h 的 min/max 宏或 <algorithm> 打架。
//
// 编码注意：本头允许中文注释 —— 生产工程 WeaselServer.vcxproj 与单测工程
// TestFluxingCloud.vcxproj 的**全部**配置都开了 /utf-8（MSVC AdditionalOptions
// /utf-8），不会按系统代码页解释。真正的硬约束是上面那条「不包含 <windows.h>」。
//

#include <algorithm>
#include <string>
#include <vector>

namespace fluxing {
namespace cloud {

// ---------------------------------------------------------------------------
// 控件 ID
// ---------------------------------------------------------------------------
//
// 1001–1017 是 T305 / T505 时代定下的（历史上被外部脚本与探针引用过），
// **不得改动已有值**；1018 起是批次 AT-1 新增的。
enum AccountControlId : int {
  kIdEmail = 1001,
  kIdPassword = 1002,
  kIdLogin = 1003,
  kIdLogout = 1004,
  kIdRefresh = 1005,
  kIdStatus = 1006,
  kIdDevices = 1007,
  kIdRevoke = 1008,
  kIdDeleteData = 1009,
  kIdClose = 1010,
  kIdRefreshDevices = 1011,
  kIdVerifyCode = 1012,
  kIdRegister = 1013,
  kIdVerify = 1014,
  // spec 100 阶段 F · T505 / 阶段 G：追加在末尾，**不要**改动已有值。
  kIdResend = 1015,
  kIdSyncNow = 1016,
  kIdSyncStatus = 1017,
  // ---- 批次 AT-1 新增（从 1018 起追加）----
  kIdHistoryCombo = 1018,     // 历史邮箱下拉（未登录态）
  kIdSaveAccount = 1019,      // 「保存账号」复选框（未登录态）
  kIdAccountLabel = 1020,     // 「当前登录账号」的邮箱文本（已登录态）
  kIdForgotPassword = 1021,   // 「忘记密码」按钮（未登录态）
  kIdOpportunityNote = 1022,  // 「机会性同步」说明行（已登录态，ADR 0062 决策 5）
  kIdNewPassword = 1023,      // 找回口令界面的「新密码」输入框
  kIdResetPassword = 1024,    // 找回口令界面的「重置口令」按钮
  kIdEmailLabel = 1025,       // 「邮箱地址」静态标签（未登录态）
  kIdPasswordLabel = 1026,    // 「密码」静态标签（未登录态）
  kIdAccountCaption = 1027,   // 「当前登录账号」静态标签（已登录态）
  kIdVerifyCaption = 1028,    // 验证码窗口的说明标签
  kIdRegisterCaption = 1029,  // 注册窗口的说明标签
  kIdForgotCaption = 1030,    // 找回口令窗口的说明标签
  kIdDeviceCaption = 1031,    // 设备与云数据窗口的说明标签
  kIdVerifyCodeLabel = 1032,  // 「验证码」静态标签
  kIdNewPasswordLabel = 1033, // 「新密码」静态标签
  kIdHistoryLabel = 1034,     // 「历史」静态标签（历史邮箱下拉左侧）
  // 换钥提示（ADR 0063 补充）：已登录态常驻说明「重置口令后云端旧条目
  // 再也解不开」+ 补救指引。只在已登录态可见 ⇒ 不进两态交集。
  kIdKeyRotationNote = 1035,
};

// ---------------------------------------------------------------------------
// 窗口角色
// ---------------------------------------------------------------------------
//
// 账号页在本批次拆成 5 个窗口。角色与服务端无关，纯粹是「哪一组控件」。
// 角色值与 CloudStatusScope 一一对应（见 CloudAccountDialog.h）。
enum class AccountWindowRole {
  kAccount = 0,  // 账号页主窗口（登录态区分）
  kVerify = 1,   // 验证码窗口
  kRegister = 2, // 注册窗口
  kForgot = 3,   // 找回口令窗口
  kDevice = 4,   // 设备与云数据窗口
  kCount = 5,
};

// ---------------------------------------------------------------------------
// 控件类别
// ---------------------------------------------------------------------------
enum class AccountWidgetClass {
  kStatic = 0,
  kEdit,
  kPasswordEdit,
  kNumberEdit,
  kButton,
  kCheckbox,
  kCombo,
  kListBox,
};

// 控件规格：id + 类别 + 几何（相对客户区左上角，单位像素）。
struct AccountWidgetSpec {
  int id;
  AccountWidgetClass cls;
  int x;
  int y;
  int w;
  int h;
};

// 布局条目（可见控件的几何）。
struct AccountLayoutEntry {
  int id;
  int x;
  int y;
  int w;
  int h;
};

// ---------------------------------------------------------------------------
// 几何表（唯一真相源）
// ---------------------------------------------------------------------------

// 账号页主窗口 · 未登录态。
inline std::vector<AccountLayoutEntry> AccountLoginLayout() {
  std::vector<AccountLayoutEntry> v;
  v.push_back({kIdEmailLabel, 12, 14, 70, 20});
  v.push_back({kIdEmail, 88, 10, 186, 26});
  v.push_back({kIdHistoryLabel, 278, 14, 48, 20});
  v.push_back({kIdHistoryCombo, 330, 10, 138, 26});
  v.push_back({kIdPasswordLabel, 12, 48, 70, 20});
  v.push_back({kIdPassword, 88, 44, 244, 26});
  v.push_back({kIdSaveAccount, 88, 78, 244, 20});
  v.push_back({kIdLogin, 12, 108, 140, 28});
  v.push_back({kIdRegister, 160, 108, 140, 28});
  v.push_back({kIdForgotPassword, 308, 108, 160, 28});
  v.push_back({kIdStatus, 12, 148, 456, 40});
  // 「关闭」两态同一底锚 y=250（客户区抬高到 290 是为了容纳已登录态的
  // 换钥提示；若只挪已登录态，未登录态会在底部多出 40 px 留白）。
  v.push_back({kIdClose, 368, 250, 100, 28});
  return v;
}

// 账号页主窗口 · 已登录态。
inline std::vector<AccountLayoutEntry> AccountLoggedInLayout() {
  std::vector<AccountLayoutEntry> v;
  v.push_back({kIdAccountCaption, 12, 14, 456, 20});
  v.push_back({kIdAccountLabel, 12, 38, 456, 22});
  v.push_back({kIdLogout, 12, 70, 140, 28});
  v.push_back({kIdSyncNow, 160, 70, 140, 28});
  v.push_back({kIdSyncStatus, 12, 108, 456, 20});
  v.push_back({kIdOpportunityNote, 12, 132, 456, 18});
  // 换钥提示（ADR 0063 补充）：h=36 按「8pt DEFAULT_GUI_FONT 折两行」留足
  // 余量（单行实测参照 = 上一行 23 个中文字在 456 px 内不折行）；宁可留白也
  // 不能裁掉安全文案。
  v.push_back({kIdKeyRotationNote, 12, 156, 456, 36});
  v.push_back({kIdStatus, 12, 198, 456, 40});
  v.push_back({kIdClose, 368, 250, 100, 28});
  return v;
}

// 验证码窗口。
inline std::vector<AccountLayoutEntry> VerifyLayout() {
  std::vector<AccountLayoutEntry> v;
  v.push_back({kIdVerifyCaption, 12, 14, 376, 32});
  v.push_back({kIdVerifyCodeLabel, 12, 58, 70, 18});
  v.push_back({kIdVerifyCode, 88, 54, 140, 26});
  v.push_back({kIdVerify, 236, 52, 100, 28});
  v.push_back({kIdResend, 88, 88, 140, 28});
  v.push_back({kIdStatus, 12, 126, 376, 40});
  v.push_back({kIdClose, 288, 174, 100, 28});
  return v;
}

// 注册窗口。
inline std::vector<AccountLayoutEntry> RegisterLayout() {
  std::vector<AccountLayoutEntry> v;
  v.push_back({kIdRegisterCaption, 12, 14, 376, 20});
  v.push_back({kIdEmailLabel, 12, 48, 70, 18});
  v.push_back({kIdEmail, 88, 44, 240, 26});
  v.push_back({kIdPasswordLabel, 12, 82, 70, 18});
  v.push_back({kIdPassword, 88, 78, 240, 26});
  v.push_back({kIdRegister, 88, 112, 160, 28});
  v.push_back({kIdStatus, 12, 148, 376, 40});
  v.push_back({kIdClose, 288, 196, 100, 28});
  return v;
}

// 找回口令窗口。
inline std::vector<AccountLayoutEntry> ForgotLayout() {
  std::vector<AccountLayoutEntry> v;
  v.push_back({kIdForgotCaption, 12, 14, 376, 20});
  v.push_back({kIdEmailLabel, 12, 48, 70, 18});
  v.push_back({kIdEmail, 88, 44, 240, 26});
  v.push_back({kIdVerifyCodeLabel, 12, 82, 70, 18});
  v.push_back({kIdVerifyCode, 88, 78, 140, 26});
  v.push_back({kIdResend, 236, 76, 152, 28});
  v.push_back({kIdNewPasswordLabel, 12, 116, 70, 18});
  v.push_back({kIdNewPassword, 88, 112, 240, 26});
  v.push_back({kIdResetPassword, 88, 146, 140, 28});
  v.push_back({kIdStatus, 12, 182, 376, 40});
  v.push_back({kIdClose, 288, 230, 100, 28});
  return v;
}

// 设备与云数据窗口。
inline std::vector<AccountLayoutEntry> DeviceLayout() {
  std::vector<AccountLayoutEntry> v;
  v.push_back({kIdDeviceCaption, 12, 12, 456, 20});
  v.push_back({kIdDevices, 12, 36, 456, 140});
  v.push_back({kIdRevoke, 12, 184, 140, 28});
  v.push_back({kIdRefreshDevices, 160, 184, 140, 28});
  v.push_back({kIdRefresh, 308, 184, 160, 28});
  v.push_back({kIdDeleteData, 12, 220, 160, 28});
  v.push_back({kIdStatus, 12, 258, 456, 40});
  v.push_back({kIdClose, 368, 306, 100, 28});
  return v;
}

// 某角色的**全部**布局条目（= 该窗口要创建的控件集合）。
inline std::vector<AccountLayoutEntry> AccountRoleLayout(
    AccountWindowRole role) {
  switch (role) {
    case AccountWindowRole::kAccount: {
      std::vector<AccountLayoutEntry> v = AccountLoginLayout();
      const std::vector<AccountLayoutEntry> logged = AccountLoggedInLayout();
      for (size_t i = 0; i < logged.size(); ++i) {
        bool exists = false;
        for (size_t j = 0; j < v.size(); ++j) {
          if (v[j].id == logged[i].id) {
            exists = true;
            break;
          }
        }
        if (!exists)
          v.push_back(logged[i]);
      }
      return v;
    }
    case AccountWindowRole::kVerify:
      return VerifyLayout();
    case AccountWindowRole::kRegister:
      return RegisterLayout();
    case AccountWindowRole::kForgot:
      return ForgotLayout();
    case AccountWindowRole::kDevice:
      return DeviceLayout();
    default:
      break;
  }
  return std::vector<AccountLayoutEntry>();
}

// 某角色窗口在给定登录态下**可见**的布局条目。
//   · 主窗口随登录态切换（两套布局互斥）；
//   · 其余窗口与登录态无关（全部可见）。
inline std::vector<AccountLayoutEntry> AccountVisibleLayout(
    AccountWindowRole role,
    bool logged_in) {
  if (role == AccountWindowRole::kAccount)
    return logged_in ? AccountLoggedInLayout() : AccountLoginLayout();
  return AccountRoleLayout(role);
}

// 某角色窗口在给定登录态下**可见**的控件 ID（升序、去重）。
inline std::vector<int> AccountVisibleIds(AccountWindowRole role,
                                          bool logged_in) {
  const std::vector<AccountLayoutEntry> layout =
      AccountVisibleLayout(role, logged_in);
  std::vector<int> ids;
  ids.reserve(layout.size());
  for (size_t i = 0; i < layout.size(); ++i)
    ids.push_back(layout[i].id);
  std::sort(ids.begin(), ids.end());
  ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
  return ids;
}

// 账号页（主窗口）在指定登录态下**可见**的控件 ID（升序、去重）。
//
// 两态的控件集合**互斥**（除了共用的状态行与「关闭」）—— 用户的要求是
// 「按登录态区分」，不是「全部堆在一屏再灰掉一部分」。
//
// ⚠️ 与 AccountLoginLayout / AccountLoggedInLayout 同源（不是另抄一份），
//    这样「可见性」与「几何」永远不会漂移（单测会断言两者一致）。
inline std::vector<int> AccountVisibleControlIds(bool logged_in) {
  return AccountVisibleIds(AccountWindowRole::kAccount, logged_in);
}

// 控件 ID → 控件类别。
inline AccountWidgetClass AccountWidgetClassOf(int id) {
  switch (id) {
    case kIdEmail:
      return AccountWidgetClass::kEdit;
    case kIdPassword:
    case kIdNewPassword:
      return AccountWidgetClass::kPasswordEdit;
    case kIdVerifyCode:
      return AccountWidgetClass::kNumberEdit;
    case kIdLogin:
    case kIdLogout:
    case kIdRegister:
    case kIdForgotPassword:
    case kIdSyncNow:
    case kIdClose:
    case kIdVerify:
    case kIdResend:
    case kIdResetPassword:
    case kIdRevoke:
    case kIdRefreshDevices:
    case kIdRefresh:
    case kIdDeleteData:
      return AccountWidgetClass::kButton;
    case kIdSaveAccount:
      return AccountWidgetClass::kCheckbox;
    case kIdHistoryCombo:
      return AccountWidgetClass::kCombo;
    case kIdDevices:
      return AccountWidgetClass::kListBox;
    default:
      return AccountWidgetClass::kStatic;
  }
}

// 某角色窗口要创建的**全部**控件（id 唯一；几何取首次出现处）。
inline std::vector<AccountWidgetSpec> AccountWidgets(AccountWindowRole role) {
  const std::vector<AccountLayoutEntry> layout = AccountRoleLayout(role);
  std::vector<AccountWidgetSpec> specs;
  specs.reserve(layout.size());
  for (size_t i = 0; i < layout.size(); ++i) {
    AccountWidgetSpec spec;
    spec.id = layout[i].id;
    spec.cls = AccountWidgetClassOf(layout[i].id);
    spec.x = layout[i].x;
    spec.y = layout[i].y;
    spec.w = layout[i].w;
    spec.h = layout[i].h;
    specs.push_back(spec);
  }
  return specs;
}

// 窗口客户区尺寸。
inline void AccountClientSize(AccountWindowRole role,
                               int* width,
                               int* height) {
  switch (role) {
    case AccountWindowRole::kAccount:
      // 250 → 290：已登录态新增「换钥提示」多占一行（ADR 0063 补充），
      // 关闭按钮两态统一下移到 y=250；高度 = 最大底边 278 + 底部余量。
      *width = 480;
      *height = 290;
      return;
    case AccountWindowRole::kVerify:
      *width = 400;
      *height = 214;
      return;
    case AccountWindowRole::kRegister:
      *width = 400;
      *height = 236;
      return;
    case AccountWindowRole::kForgot:
      *width = 400;
      *height = 270;
      return;
    case AccountWindowRole::kDevice:
      *width = 480;
      *height = 346;
      return;
    default:
      break;
  }
  *width = 480;
  *height = 250;
}

// 「设备与云数据」窗口在指定登录态下**可交互**（EnableWindow）的控件 ID
// （升序、去重）。未登录时全部置灰：窗口照样能打开（菜单项常在），
// 但用户点不动任何会改云端数据的按钮。
inline std::vector<int> DeviceDialogEnabledControlIds(bool logged_in) {
  std::vector<int> ids;
  if (!logged_in)
    return ids;
  ids.push_back(kIdDeleteData);
  ids.push_back(kIdRefresh);
  ids.push_back(kIdRefreshDevices);
  ids.push_back(kIdRevoke);
  std::sort(ids.begin(), ids.end());
  return ids;
}

}  // namespace cloud
}  // namespace fluxing
