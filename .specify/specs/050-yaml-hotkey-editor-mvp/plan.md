# spec 050 计划 - yaml 快捷键可视化编辑器（v0.18.30 F3 MVP）

## 1. 技术上下文

- **现有资源**：
  - `FluxingConfigEditor/YamlRoundTrip.{h,cpp}` (spec 024) — yaml 读写，保留 key order
  - `FluxingConfigEditor/DeployerUiHelper.h` (cursor) — Dpi/字体/缩放 utility
  - WeaselDeployer.exe /deploy IPC — 触发 librime 重新加载
  - spec 037 FluxingComponents 控件 — 可选（按钮 / 列表）

- **修改文件**：
  - `FluxingConfigEditor/HotkeyBinding.h` - 新建数据结构
  - `FluxingConfigEditor/HotkeyEditorDialog.{h,cpp}` - 新建主对话框
  - `FluxingConfigEditor/KeyRecorder.{h,cpp}` - 新建按键录制器
  - `WeaselDeployer/WeaselDeployer.rc` - 加 menu item
  - `WeaselDeployer/Configurator.cpp` - 加启动入口
  - `weasel.sln` + `xmake.lua` + `scripts/test-infra/run-test-suite.bat` - 注册
  - `test/TestHotkeyEditor/` - 新建测试项目

- **依赖**：
  - WTL / ATL 框架（已 vendored via wtl include）
  - yaml-cpp（已 vendored via FluxingConfigEditor 依赖）
  - Windows SDK GetAsyncKeyState / MapVirtualKey（按键录制）

## 2. 技术方案

### 2.1 数据结构 (HotkeyBinding.h)

```cpp
#pragma once
#include <string>
#include <vector>

namespace fluxing {

enum class HotkeyWhen { Always, Composing, HasMenu, Paging, Empty };
enum class HotkeyAction { Send, Toggle, Select };

struct HotkeyBinding {
  HotkeyWhen when = HotkeyWhen::Empty;
  std::wstring when_str;  // raw "composing" / "has_menu" / "always"
  std::wstring accept;     // "Control+Shift+F1"
  HotkeyAction action = HotkeyAction::Send;
  std::string action_value;  // "2" (send) / "ascii_mode" (toggle) / ".next" (select)

  // Parse librime accept key notation
  // "Control+Shift+F1" → modifier=Control+Shift, key=F1
  // "Shift+Shift_L" → modifier=Shift, key=Shift_L (mod-bit separate)
  // "comma" → no modifier, key=comma
  static std::wstring NormalizeAccept(const std::wstring& accept);
};

}  // namespace fluxing
```

### 2.2 主对话框 (HotkeyEditorDialog.h)

```cpp
#pragma once
#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlcrack.h>
#include <atlctrls.h>
#include <vector>
#include "HotkeyBinding.h"

class HotkeyEditorDialog : public ATL::CDialogImpl<HotkeyEditorDialog> {
 public:
  enum { IDD = IDD_HOTKEY_EDITOR };

  BEGIN_MSG_MAP(HotkeyEditorDialog)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    COMMAND_ID_HANDLER(IDOK, OnSave)
    COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
    COMMAND_ID_HANDLER(IDC_ADD, OnAdd)
    COMMAND_ID_HANDLER(IDC_EDIT, OnEdit)
    COMMAND_ID_HANDLER(IDC_DELETE, OnDelete)
    COMMAND_ID_HANDLER(IDC_RESET, OnReset)
    NOTIFY_HANDLER(IDC_LIST, LVN_ITEMCHANGED, OnListItemChanged)
  END_MSG_MAP()

 private:
  LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnSave(WORD, WORD, HWND, BOOL&);
  LRESULT OnCancel(WORD, WORD, HWND, BOOL&);
  LRESULT OnAdd(WORD, WORD, HWND, BOOL&);
  LRESULT OnEdit(WORD, WORD, HWND, BOOL&);
  LRESULT OnDelete(WORD, WORD, HWND, BOOL&);
  LRESULT OnReset(WORD, WORD, HWND, BOOL&);
  LRESULT OnListItemChanged(int, LPNMHDR, BOOL&);

  void LoadBindings();
  void SaveBindings();
  void RefreshList();
  void TriggerDeploy();
  static int CALLBACK ListCompare(LPARAM, LPARAM, LPARAM);

  std::vector<fluxing::HotkeyBinding> bindings_;
  std::wstring yaml_path_;  // <user_data_dir>/default.custom.yaml
  CListViewCtrl list_;
};
```

### 2.3 按键录制器 (KeyRecorder.h)

```cpp
// 无窗口，用模态对话框实现
class KeyRecorderDialog : public ATL::CDialogImpl<KeyRecorderDialog> {
 public:
  enum { IDD = IDD_KEY_RECORDER };

  BEGIN_MSG_MAP(KeyRecorderDialog)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    MESSAGE_HANDLER(WM_KEYDOWN, OnKeyDown)
    MESSAGE_HANDLER(WM_LBUTTONDOWN, OnCancelClick)
    COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
  END_MSG_MAP()

  std::wstring accept_str;  // result, set by OK button

 private:
  LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnKeyDown(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnCancelClick(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnCancel(WORD, WORD, HWND, BOOL&);
};
```

实现逻辑：捕获 WM_KEYDOWN 事件，组合 GetAsyncKeyState(VK_CONTROL) / VK_SHIFT / VK_MENU / VK_LWIN，按下时把它们加到修饰键；非修饰键加上按键名（用 MapVirtualKeyW + GetKeyNameTextW）。

### 2.4 入口（WeaselDeployer）

```cpp
// Configurator.cpp 新增
int Configurator::OpenHotkeyEditor() {
  // No mutex needed (read-only, no maintenance)
  STARTUPINFOW si = {0};
  PROCESS_INFORMATION pi = {0};
  std::wstring cmd = L"\"" + (install_dir() / L"HotkeyEditor.exe").wstring() + L"\"";
  CreateProcessW(NULL, &cmd[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  return 0;
}
```

加 WeaselDeployer.rc menu item: "快捷键编辑器 (&H)\tCtrl+Shift+E", ID_HOTKEY_EDITOR

## 3. 任务粒度（满足 R5: < 4h, 1-3 files）

- T001 [P1] HotkeyBinding.h 数据结构 (1h)
- T002 [P1] KeyRecorder.{h,cpp} 按键录制器 (2h)
- T003 [P1] HotkeyEditorDialog.{h,cpp} 主对话框 + ListView + buttons (4h) — 可拆
  - T003a: OnInitDialog + LoadBindings + RefreshList (2h)
  - T003b: Add/Edit/Delete handlers (1h)
  - T003c: Save + deploy (1h)
- T004 [P1] WeaselDeployer 入口 + menu item + OpenHotkeyEditor (1h)
- T005 [P1] weasel.sln + xmake.lua + run-test-suite.bat 注册 (30min)
- T006 [P1] TestHotkeyEditor 行为级测试 (2h)
  - 加载 default.yaml 的 10+ bindings
  - 添加 → 解析 "Control+Shift+F1" → 出现在列表
  - 删除 → 列表少一项
  - 保存 → 写回 default.custom.yaml → 触发 deploy
- T007 [P1] xbuild.bat weasel → 0 errors + 跑 TestHotkeyEditor (30min)
- T008 [P1] commit (15min)
- T009 [P1] CHANGELOG + 不发 installer (10min)
- **总计**: ~12 小时（满足 R5 4h × 3 task = 12h）

## 4. 依赖

- spec 024 YamlRoundTrip（已 ship）
- spec 037 FluxingComponents（已 ship，可选用于按钮 / 列表 — MVP 用原生 ListView）
- spec 041 DPI 修复（已 ship，144 DPI 下视觉正确）
- WTL / ATL（vendored）
- WeaselDeployer.exe /deploy（已有）
- L17 / L18 / L19 / L21（Shift key binding 风险 awareness）

## 5. 验收回执

- [ ] T-A1: xbuild.bat weasel → 0 errors
- [ ] T-A2: TestHotkeyEditor 4/4 PASS
- [ ] T-A3: 手测：打开编辑器 → 添加 binding → 保存 → deploy → 新 binding 生效
- [ ] T-A4: 升级时 default.custom.yaml 不被覆盖（部署目录 vs 用户数据目录隔离）
- [ ] T-A5: L17/L18/L19 防御：保存前 linter 警告 keycode=Shift_L 单键 binding