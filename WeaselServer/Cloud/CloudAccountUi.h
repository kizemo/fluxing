#pragma once
//
// 批次 AT-1 · 账号页 UI 重构（登录态区分 + 独立子窗口）
// 批次 BB · 账号页 DPI 感知布局（修缺陷 C：>150% 缩放下「换钥提示」被裁）
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
// /utf-8），不会按系统代码页解释。真正的硬约束是上面那条「不包含
// <windows.h>」。
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
// DPI 缩放（布局字面量的基准）
// ---------------------------------------------------------------------------
//
// 布局表里的字面量一律按 **96 dpi（= 100% 缩放）** 书写。UI 字体（系统
// message 字体，DEFAULT_GUI_FONT，9pt）会随 DPI 放大，而盒子若不跟着放大，
// 文字就会被裁：
//
//   · 批次 BA 实测（缺陷 C）：144 dpi 下「换钥提示」的 37 个全角字折成两行，
//     两行正好 36 px，而控件高度字面量也是 36 ⇒ **余量 0 px**；
//     >150% 缩放的机器必然裁掉末行（175% 需 42 px、200% 需 48 px）。
//
// 因此本文件的铁律：**所有几何 × dpi/96**。于是「文字 : 盒子」的比例与
// DPI 无关 ⇒ 任何缩放下都不会裁切；96 dpi 下逐像素等于旧布局（零行为变化）。
//
// 缩放语义 = MulDiv(v, dpi, 96)（四舍五入），与 WeaselUI 的 Scale()
// 惯用法一致。
inline constexpr int kAccountBaseDpi = 96;

// 96 dpi 的布局字面量 → 目标 DPI（dpi ≤ 0 视作 96）。
inline int AccountScale(int value, int dpi) {
  if (dpi <= 0)
    dpi = kAccountBaseDpi;
  return (value * dpi + kAccountBaseDpi / 2) / kAccountBaseDpi;
}

// 系统 DPI 归一化（**纯函数、无状态**）：探测值不合理 ⇒ 回退 96。
//   合法区间 [48, 960]（50% ~ 1000%）：下界沿用旧实现的两处 `>= 48`，
//   上界只防「探测值荒诞」（Windows 上限 = 500%）。
// ⚠️ 与批次 BB 缺陷 D 配套：`AccountUiDpi()` 只允许缓存**函数指针**，
//    绝不允许缓存本函数的返回值 —— 用户改缩放后同一进程内必须立刻拿到新值
//    （官方文档 GetDpiForSystem：You should not cache the system DPI）。
inline int AccountNormalizeSystemDpi(long raw) {
  if (raw < 48 || raw > 960)
    return kAccountBaseDpi;
  return static_cast<int>(raw);
}

// 整表等比缩放（**唯一的缩放入口** ⇒ 结构上保证没有哪个控件漏缩）。
inline std::vector<AccountLayoutEntry> AccountScaleLayout(
    const std::vector<AccountLayoutEntry>& base,
    int dpi) {
  std::vector<AccountLayoutEntry> scaled;
  scaled.reserve(base.size());
  for (size_t i = 0; i < base.size(); ++i) {
    AccountLayoutEntry entry;
    entry.id = base[i].id;
    entry.x = AccountScale(base[i].x, dpi);
    entry.y = AccountScale(base[i].y, dpi);
    entry.w = AccountScale(base[i].w, dpi);
    entry.h = AccountScale(base[i].h, dpi);
    scaled.push_back(entry);
  }
  return scaled;
}

// ---------------------------------------------------------------------------
// 字体度量模型（判定「控件装不装得下这行字」用）
// ---------------------------------------------------------------------------
//
// UI 字体 = 系统 message 字体（DEFAULT_GUI_FONT，9pt）。**全角字步进宽**
// 在 96 dpi 下 = 12 px（实测：144 dpi 下 37 个全角字整行 666 px ⇒ 18 px/字
// ⇒ 96 dpi 12 px/字）；行高同值 ⇒ 文字宽度 = 全角字数 × 字高。
inline constexpr int kAccountUiEmPx96 = 12;

// 96 dpi 下 DEFAULT_GUI_FONT 的**行高**（px）。生产侧用
// GetTextMetrics 量真实字体（tmHeight + tmExternalLeading）再按本常数
// 反推有效 DPI ⇒ 布局永远跟着**实际渲染出来的字体**走，而不是猜一个 DPI。
inline constexpr int kAccountUiFontPx96 = 12;

// 真实字体行高（px）→ 有效 DPI。
//   12 px → 96、15 px → 120、18 px → 144、21 px → 168、24 px → 192、
//   30 px → 240、36 px → 288。
inline int AccountEffectiveDpiFromFontPx(int font_px) {
  if (font_px <= 0)
    return kAccountBaseDpi;
  return (font_px * kAccountBaseDpi + kAccountUiFontPx96 / 2) /
         kAccountUiFontPx96;
}

// 换钥提示文案（ADR 0063 补充）。放在这里而不是 .cpp：单测要拿它判定
// 「控件装不装得下这行字」（R7：生产与单测共用同一份实现）。
// ⚠️ 逐字不得改动 —— 账号页文案与重启清单 A-1 的期望值逐字比对。
inline const wchar_t* AccountKeyRotationNoteText() {
  return L"重置登录口令后，云端旧条目将无法再解开，可在「设备与云数据」清空云端"
         L"数据。";
}

// 字符串长度（wchar_t 个数；不含结尾 '\0'）。
inline int AccountWideCharCount(const wchar_t* text) {
  int count = 0;
  if (text == nullptr)
    return count;
  for (const wchar_t* p = text; *p != L'\0'; ++p)
    ++count;
  return count;
}

// 一行文字在给定 DPI 下的宽度（全角字模型）。
inline int AccountTextWidthEm(int char_count, int dpi) {
  return char_count * AccountScale(kAccountUiEmPx96, dpi);
}

// 文字在给定宽度里折成几行（向上取整；至少 1 行）。
inline int AccountTextLineCount(int text_width, int box_width) {
  if (box_width <= 0 || text_width <= 0)
    return 1;
  return (text_width + box_width - 1) / box_width;
}

// 一行的高度（px）。
inline int AccountLineHeightPx(int dpi) {
  return AccountScale(kAccountUiEmPx96, dpi);
}

// 文字在给定盒子宽度里**实际需要的**高度（px）。
inline int AccountRequiredHeightForText(int char_count,
                                        int box_width,
                                        int dpi) {
  const int width = AccountTextWidthEm(char_count, dpi);
  const int lines = AccountTextLineCount(width, box_width);
  return lines * AccountLineHeightPx(dpi);
}

// 控件的**最小安全高度**：文字行数 + 1 行余量。
// ⚠️ 「余量 ≥ 1 行」正是缺陷 C 的判据（150% 现场余量 0 ⇒ 判红）。
inline int AccountMinHeightForText(int lines, int dpi) {
  return (lines + 1) * AccountLineHeightPx(dpi);
}

// ---------------------------------------------------------------------------
// 几何表（唯一真相源）
// ---------------------------------------------------------------------------
//
// ⚠️ 以下五张表是 **96 dpi 基准**（不做任何缩放）；对外的取用函数一律
//    显式带 dpi 参数并统一走 AccountScaleLayout —— **不保留「无 dpi」的
//    兼容重载**：无 dpi 版正是本缺陷（缺陷 C）的形态，留着就是复发入口。

// 账号页主窗口 · 未登录态（96 dpi 基准）。
// 「单行 EDIT 的文字在客户区贴顶」这一 Windows 绘制行为的量化常量：墨迹中心
// 距框顶恒为 7 base px（175% 真机实测 12 物理 px ÷ 1.75）。填写框与标题
// 垂直对齐的正确几何是
//   edit.y + AccountEditInkCenterOffsetBase() == label.y + label.h / 2
// —— 把 EDIT 高度收缩到字体行高（h=15 base）即可让框内文字自然居中。
// 生产侧与单测共用本函数，不复制魔数（R7）。
inline int AccountEditInkCenterOffsetBase() { return 7; }

inline std::vector<AccountLayoutEntry> AccountLoginLayoutBase() {
  std::vector<AccountLayoutEntry> v;
  // 邮箱/密码两行的**标签与输入框按中心线对齐**：label 保持 h=26（原基线），
  // edit 收窄到 h=15 —— 单行 EDIT 的文字在客户区贴顶，墨迹中心距框顶恒为 7
  // base（见 AccountEditInkCenterOffsetBase），故 edit.y + 7 == label 中心线。
  // 标签再用 StyleFor(kStatic) 的 SS_CENTERIMAGE 在框内垂直居中，于是
  // 「输入框里的文字」与「填写框标题」垂直对齐（用户 2026-11-21 要求）。
  v.push_back({kIdEmailLabel, 12, 10, 70, 26});
  // 邮箱框独占整行（右沿 468 与状态行/关闭钮对齐）：历史邮箱不再占用一条
  // 独立的「历史」标签 + 下拉框（用户明确要求「不要另外增加一个下拉框
  // 模块」），改由邮箱框自身在获焦/输入时弹出建议列表（见
  // CloudAccountDialog.cpp 的 ShowSuggestIfAny）。⇒ kIdHistoryLabel(1034)
  // 与 kIdHistoryCombo(1018) 保留编号与控件类别，但**不再进本布局表**。
  v.push_back({kIdEmail, 88, 16, 380, 15});
  v.push_back({kIdPasswordLabel, 12, 44, 70, 26});
  v.push_back({kIdPassword, 88, 50, 244, 15});
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
inline std::vector<AccountLayoutEntry> AccountLoggedInLayoutBase() {
  std::vector<AccountLayoutEntry> v;
  v.push_back({kIdAccountCaption, 12, 14, 456, 20});
  v.push_back({kIdAccountLabel, 12, 38, 456, 22});
  v.push_back({kIdLogout, 12, 70, 140, 28});
  v.push_back({kIdSyncNow, 160, 70, 140, 28});
  v.push_back({kIdSyncStatus, 12, 108, 456, 20});
  v.push_back({kIdOpportunityNote, 12, 132, 456, 18});
  // 换钥提示（ADR 0063 补充）：h=36 是**按 96 dpi 基准**写的（= 3 行行高）。
  // 缺陷 C 的教训：盒子若不随 DPI 一起放大，37 个全角字在 144 dpi 下折成
  // 两行正好 36 px ⇒ 余量 0 ⇒ >150% 必裁。修法见本文件「DPI 缩放」一节：
  // 整表等比放大 ⇒ 文字:盒子比值恒定，永远裁不到。
  v.push_back({kIdKeyRotationNote, 12, 156, 456, 36});
  v.push_back({kIdStatus, 12, 198, 456, 40});
  v.push_back({kIdClose, 368, 250, 100, 28});
  return v;
}

// 验证码窗口。
inline std::vector<AccountLayoutEntry> VerifyLayoutBase() {
  std::vector<AccountLayoutEntry> v;
  v.push_back({kIdVerifyCaption, 12, 14, 376, 32});
  v.push_back({kIdVerifyCodeLabel, 12, 58, 70, 18});
  v.push_back({kIdVerifyCode, 88, 60, 140, 15});
  v.push_back({kIdVerify, 236, 52, 100, 28});
  v.push_back({kIdResend, 88, 88, 140, 28});
  v.push_back({kIdStatus, 12, 126, 376, 40});
  v.push_back({kIdClose, 288, 174, 100, 28});
  return v;
}

// 注册窗口。
inline std::vector<AccountLayoutEntry> RegisterLayoutBase() {
  std::vector<AccountLayoutEntry> v;
  v.push_back({kIdRegisterCaption, 12, 14, 376, 20});
  v.push_back({kIdEmailLabel, 12, 48, 70, 18});
  v.push_back({kIdEmail, 88, 50, 240, 15});
  v.push_back({kIdPasswordLabel, 12, 82, 70, 18});
  v.push_back({kIdPassword, 88, 84, 240, 15});
  v.push_back({kIdRegister, 88, 112, 160, 28});
  v.push_back({kIdStatus, 12, 148, 376, 40});
  v.push_back({kIdClose, 288, 196, 100, 28});
  return v;
}

// 找回口令窗口。
inline std::vector<AccountLayoutEntry> ForgotLayoutBase() {
  std::vector<AccountLayoutEntry> v;
  v.push_back({kIdForgotCaption, 12, 14, 376, 20});
  v.push_back({kIdEmailLabel, 12, 48, 70, 18});
  v.push_back({kIdEmail, 88, 50, 240, 15});
  v.push_back({kIdVerifyCodeLabel, 12, 82, 70, 18});
  v.push_back({kIdVerifyCode, 88, 84, 140, 15});
  v.push_back({kIdResend, 236, 76, 152, 28});
  v.push_back({kIdNewPasswordLabel, 12, 116, 70, 18});
  v.push_back({kIdNewPassword, 88, 118, 240, 15});
  v.push_back({kIdResetPassword, 88, 146, 140, 28});
  v.push_back({kIdStatus, 12, 182, 376, 40});
  v.push_back({kIdClose, 288, 230, 100, 28});
  return v;
}

// 设备与云数据窗口。
inline std::vector<AccountLayoutEntry> DeviceLayoutBase() {
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

// 某角色的**全部**布局条目（= 该窗口要创建的控件集合），已按 dpi 缩放。
inline std::vector<AccountLayoutEntry> AccountRoleLayout(AccountWindowRole role,
                                                         int dpi) {
  switch (role) {
    case AccountWindowRole::kAccount: {
      std::vector<AccountLayoutEntry> v = AccountLoginLayoutBase();
      const std::vector<AccountLayoutEntry> logged =
          AccountLoggedInLayoutBase();
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
      return AccountScaleLayout(v, dpi);
    }
    case AccountWindowRole::kVerify:
      return AccountScaleLayout(VerifyLayoutBase(), dpi);
    case AccountWindowRole::kRegister:
      return AccountScaleLayout(RegisterLayoutBase(), dpi);
    case AccountWindowRole::kForgot:
      return AccountScaleLayout(ForgotLayoutBase(), dpi);
    case AccountWindowRole::kDevice:
      return AccountScaleLayout(DeviceLayoutBase(), dpi);
    default:
      break;
  }
  return std::vector<AccountLayoutEntry>();
}

// 某角色窗口在给定登录态下**可见**的布局条目（已按 dpi 缩放）。
//   · 主窗口随登录态切换（两套布局互斥）；
//   · 其余窗口与登录态无关（全部可见）。
inline std::vector<AccountLayoutEntry>
AccountVisibleLayout(AccountWindowRole role, bool logged_in, int dpi) {
  if (role == AccountWindowRole::kAccount) {
    return logged_in ? AccountScaleLayout(AccountLoggedInLayoutBase(), dpi)
                     : AccountScaleLayout(AccountLoginLayoutBase(), dpi);
  }
  return AccountRoleLayout(role, dpi);
}

// 某角色窗口在给定登录态下**可见**的控件 ID（升序、去重）。
inline std::vector<int> AccountVisibleIds(AccountWindowRole role,
                                          bool logged_in,
                                          int dpi) {
  const std::vector<AccountLayoutEntry> layout =
      AccountVisibleLayout(role, logged_in, dpi);
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
// ⚠️ 与 AccountLoginLayoutBase / AccountLoggedInLayoutBase 同源（不是另抄
//    一份），这样「可见性」与「几何」永远不会漂移（单测会断言两者一致）。
inline std::vector<int> AccountVisibleControlIds(bool logged_in, int dpi) {
  return AccountVisibleIds(AccountWindowRole::kAccount, logged_in, dpi);
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

// 某角色窗口要创建的**全部**控件（id 唯一；几何取首次出现处；已缩放）。
inline std::vector<AccountWidgetSpec> AccountWidgets(AccountWindowRole role,
                                                     int dpi) {
  const std::vector<AccountLayoutEntry> layout = AccountRoleLayout(role, dpi);
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

// 窗口客户区尺寸（已按 dpi 缩放）。
inline void AccountClientSize(AccountWindowRole role,
                              int dpi,
                              int* width,
                              int* height) {
  int base_width = 480;
  int base_height = 250;
  switch (role) {
    case AccountWindowRole::kAccount:
      // 250 → 290：已登录态新增「换钥提示」多占一行（ADR 0063 补充），
      // 关闭按钮两态统一下移到 y=250；高度 = 最大底边 278 + 底部余量。
      base_width = 480;
      base_height = 290;
      break;
    case AccountWindowRole::kVerify:
      base_width = 400;
      base_height = 214;
      break;
    case AccountWindowRole::kRegister:
      base_width = 400;
      base_height = 236;
      break;
    case AccountWindowRole::kForgot:
      base_width = 400;
      base_height = 270;
      break;
    case AccountWindowRole::kDevice:
      base_width = 480;
      base_height = 346;
      break;
    default:
      break;
  }
  if (width != nullptr)
    *width = AccountScale(base_width, dpi);
  if (height != nullptr)
    *height = AccountScale(base_height, dpi);
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
