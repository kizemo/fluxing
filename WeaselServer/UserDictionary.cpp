//
// UserDictionary.cpp — v0.19.0.60 (Phase L 调整 1) STUB
//
// 历史:spec 044 UserDictionary 管理 Modal 实现 (1723 行)。
// v0.19.0.60:用户决定下线 UserDictionary 模块 — 浮动设置栏按钮 / 设置栏 builtin /
// Ctrl+Shift+U 热键 / 用户词典代码全部移除。
// 此 .cpp 保留是为了让引用 UserDictionary::Show 等 API 的二进制(test 程序)仍能链接,
// 所有方法实现为 no-op,运行期按钮按下/快捷键触发统一沉默失败。
//
// 不再 ship 这部分 UI,部署等价于"无"。任何 test 引用 UserDictionary 应在源头用
// `#if 0` 包裹(见 test/Verifier2_G2_G12/v0_19_0_32_G2_G12.cpp G6-G12 段、
// test/v0_19_0_32_e2e/* UserDictionary section、test/TestUserDictionary/* 等)。
//
#include "stdafx.h"
#include "UserDictionary.h"

// ===== Static state 定义 (默认值, no-op module 不引用) =====
HWND UserDictionary::s_hwnd         = nullptr;
HWND UserDictionary::s_hList        = nullptr;
HWND UserDictionary::s_hSearch      = nullptr;
HWND UserDictionary::s_hStatus      = nullptr;
HWND UserDictionary::s_hToast       = nullptr;
HWND UserDictionary::s_hBtnAdd      = nullptr;
HWND UserDictionary::s_hBtnDel      = nullptr;
HWND UserDictionary::s_hBtnImport   = nullptr;
HWND UserDictionary::s_hBtnExport   = nullptr;
HWND UserDictionary::s_hBtnCancel   = nullptr;
HWND UserDictionary::s_hBtnDeploy   = nullptr;
HWND UserDictionary::s_hEditDlg     = nullptr;
HWND UserDictionary::s_hEditText    = nullptr;
HWND UserDictionary::s_hEditCode    = nullptr;
HWND UserDictionary::s_hSliderWeight = nullptr;
HWND UserDictionary::s_hComboSchema = nullptr;
HWND UserDictionary::s_hBtnEditOk   = nullptr;
HWND UserDictionary::s_hBtnEditCancel = nullptr;

std::wstring UserDictionary::s_yamlPath;
UserDictDeployFn  UserDictionary::s_deployFn  = nullptr;
UserDictYamlIoFn  UserDictionary::s_yamlIoFn  = nullptr;
UserDictBackupFn  UserDictionary::s_backupFn  = nullptr;

std::vector<UserDictEntry> UserDictionary::m_entries;
int UserDictionary::m_selectedIndex = -1;
std::set<int> UserDictionary::m_multiSelected;

std::wstring UserDictionary::s_searchQuery;
bool   UserDictionary::m_dirty = false;
DWORD  UserDictionary::s_lastSaveFailTick = 0;
DWORD  UserDictionary::s_showTime = 0;

UserDictionary::State UserDictionary::s_state = UserDictionary::State_Hidden;
int UserDictionary::m_editingIndex = -1;

std::thread UserDictionary::s_deployThread;
DWORD UserDictionary::s_deployStartTick = 0;
DWORD UserDictionary::s_deployDoneTick = 0;
int   UserDictionary::s_deployResult = 0;
std::wstring UserDictionary::s_deployErrMsg;

HFONT UserDictionary::s_hFontUi = nullptr;
int   UserDictionary::kListH_phys = 0;
int   UserDictionary::kBtnY_phys  = 0;

// ===== Stub API 实现 — v0.19.0.60 所有方法 no-op =====
void UserDictionary::Show()  {}
void UserDictionary::Hide()  {}
bool UserDictionary::IsVisible() { return false; }

void UserDictionary::SetDeployFn(UserDictDeployFn) {}
void UserDictionary::SetYamlIoFn(UserDictYamlIoFn) {}
void UserDictionary::SetBackupFn(UserDictBackupFn) {}
void UserDictionary::SetYamlPath(const std::wstring&) {}

std::vector<UserDictEntry>& UserDictionary::MutableEntries() {
  static std::vector<UserDictEntry> s_v;
  return s_v;
}
const std::vector<UserDictEntry>& UserDictionary::Entries() {
  static const std::vector<UserDictEntry> s_v;
  return s_v;
}

bool UserDictionary::LoadYaml(const std::wstring&, std::vector<UserDictEntry>&) { return false; }
bool UserDictionary::SaveYaml(const std::wstring&, const std::vector<UserDictEntry>&) { return false; }

std::string UserDictionary::EntriesToTxt(const std::vector<UserDictEntry>&) { return std::string(); }

bool UserDictionary::MockDeploy(const std::vector<UserDictEntry>&,
                                const std::wstring&,
                                std::wstring& errMsg) {
  errMsg = L"UserDictionary disabled (v0.19.0.60)";
  return false;
}
bool UserDictionary::ProductionDeploy(const std::vector<UserDictEntry>&,
                                     const std::wstring&,
                                     std::wstring& errMsg) {
  errMsg = L"UserDictionary disabled (v0.19.0.60)";
  return false;
}

bool UserDictionary::MakeBackup(const std::wstring&) { return false; }

int UserDictionary::PopulateListCount(HWND) { return 0; }
int UserDictionary::ApplySearchFilter(const std::wstring&) { return 0; }

void UserDictionary::ScheduleSave() {}
void UserDictionary::FlushSave() {}

void UserDictionary::ShowToast(const std::wstring&, int) {}
void UserDictionary::HideToast() {}
bool UserDictionary::IsToastVisible() { return false; }

bool UserDictionary::OpenEntryDialog(int) { return false; }
void UserDictionary::CloseEntryDialog(bool) {}

void UserDictionary::DeployAsync() {}
void UserDictionary::JoinDeployThread() {}
bool UserDictionary::IsDeployInProgress() { return false; }

LRESULT CALLBACK UserDictionary::WndProc(HWND, UINT, WPARAM, LPARAM) { return 0; }
LRESULT CALLBACK UserDictionary::EditDlgProc(HWND, UINT, WPARAM, LPARAM) { return 0; }

// ===== Private helpers =====
std::wstring UserDictionary::Trim(const std::wstring& s) { return s; }
std::wstring UserDictionary::Unquote(const std::wstring& s) { return s; }
COLORREF UserDictionary::WeightColor(int) { return 0; }

LRESULT UserDictionary::OnCreate(HWND) { return 0; }
LRESULT UserDictionary::OnDestroy(HWND) { return 0; }
LRESULT UserDictionary::OnPaint(HWND) { return 0; }
LRESULT UserDictionary::OnKeyDown(HWND, WPARAM) { return 0; }
LRESULT UserDictionary::OnNotify(HWND, LPARAM) { return 0; }
LRESULT UserDictionary::OnCommand(HWND, WPARAM) { return 0; }
LRESULT UserDictionary::OnTimer(HWND, WPARAM) { return 0; }
LRESULT UserDictionary::OnCtlColor(HWND, WPARAM, LPARAM) { return 0; }
LRESULT UserDictionary::OnDrawItem(HWND, LPARAM) { return 0; }

int UserDictionary::PopulateListImpl(HWND) { return 0; }

void UserDictionary::CenterOnPrimaryMonitor(HWND, int, int) {}
void UserDictionary::RepaintLayered(HWND) {}

void UserDictionary::EnterEditingState(int) {}
void UserDictionary::ExitEditingState(bool) {}

// ===== 模块下线提示 (dev-side) =====
#ifdef _DEBUG
#include <cstdio>
namespace {
// 主动提示:开发期若 UserDictionary 任何 entry 被实际调用,console 提醒。
struct UserDictDisabledNotice {
  UserDictDisabledNotice() {
    std::fprintf(stderr,
                 "[UserDictionary v0.19.0.60] module disabled — Phase L 调整 1\n");
  }
};
static UserDictDisabledNotice g_notice;
}  // namespace
#endif  // _DEBUG
