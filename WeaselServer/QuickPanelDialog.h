// spec 036 + spec 038 + spec 045 v0.18.29.0: QuickPanelDialog -
// mac-style tray QuickSettings pop-up. Triggered by Alt+, global
// hotkey or left-click tray icon.
//
// spec 036 v0 scope: ASCII toggle button + Deploy button + close X.
// spec 038 refactor: replaced the three native Win32 BUTTON controls
// with spec 037 FluxingComponents (Button/Toggle/Label/Panel).
// spec 045 v0.18.29.0 ship: extend to 8-12 entry layout per spec 006
// design.md section 1.2:
//   Row 1: title label "Fluxing" + close X (native IDCANCEL)
//   Row 2: 3 toggles - 中/英 (ascii) / 简/繁 (simp) / 全/半角 (full)
//   Row 3: 当前方案 label + 切换 button (cascades to legacy right-click
//          tray menu where the user can pick a scheme; F3 spec 046
//          will replace with a real popup list).
//   Row 4: 用户文件夹 / 程序文件夹
//   Row 5: 部署 / 退出
// Native close button (IDCANCEL) is preserved so existing spec 036
// TestQuickPanelDialog WM_COMMAND tests still pass.
//
// Threading: created on the WeaselServer main UI thread. All callbacks
// fire on the same thread; the request handler is responsible for
// any thread-safety of the underlying state.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <windows.h>

namespace fluxing {
namespace ui {
class FluxingButton;
class FluxingToggle;
class FluxingLabel;
class FluxingPanel;
}  // namespace ui
}  // namespace fluxing

class QuickPanelDialog {
 public:
  // Show the QuickPanel centered above the system tray. Hides any
  // previous instance (only one panel at a time). The dialog auto-closes
  // on ESC, on click outside after 1s debounce, or on button click.
  //
  // Initial state args (read-only, drives initial toggle/label state):
  //   currentAscii, currentSimp, currentFullwidth: option bools.
  //   currentSchema: current schema id (e.g. "luna_pinyin").
  //   availableSchemas: all loaded schema ids, for 切换 label tooltip.
  //
  // Callbacks fire on the dialog UI thread; caller must marshal to
  // whatever thread owns the underlying state.
  //   onAsciiToggle / onSimpToggle / onFullwidthToggle: forwarded to
  //     RimeWithWeaselHandler::SetOption.
  //   onSelectSchema: forwarded to handler SelectSchema().
  //   onOpenUserFolder / onOpenProgramFolder: explore(dir).
  //   onDeploy: spawn WeaselDeployer.exe /deploy.
  //   onQuit: WeaselServer.exe /q.
  static void Show(bool currentAscii,
                   bool currentSimp,
                   bool currentFullwidth,
                   const std::wstring& currentSchema,
                   const std::vector<std::wstring>& availableSchemas,
                   std::function<void(bool)> onAsciiToggle,
                   std::function<void(bool)> onSimpToggle,
                   std::function<void(bool)> onFullwidthToggle,
                   std::function<void(const std::wstring&)> onSelectSchema,
                   std::function<void()> onOpenUserFolder,
                   std::function<void()> onOpenProgramFolder,
                   std::function<void()> onDeploy,
                   std::function<void()> onQuit);

  // Close the active QuickPanel (if any). Safe to call when no panel is
  // visible. Used during WeaselServer shutdown to avoid orphaned windows.
  static void Hide();

  // WndProc and the message handlers are public so the anonymous-
  // namespace RegisterClassOnce helper in QuickPanelDialog.cpp can
  // set `lpfnWndProc = QuickPanelDialog::WndProc`. The class is fully
  // static; visibility is not load-bearing for correctness.
  static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
  static LRESULT OnCreate(HWND, WPARAM, LPARAM);
  static LRESULT OnDestroy(HWND);
  static LRESULT OnCommand(HWND, WPARAM, LPARAM);
  static LRESULT OnKillFocus(HWND, WPARAM, LPARAM);
  static LRESULT OnTimer(HWND, WPARAM, LPARAM);

  // spec 038: public accessors so the anonymous-namespace lambdas in
  // CreateFluxingControls can interact with the static state without
  // violating C++ private-member access rules. These are read-only for
  // ActiveHwnd / OnAsciiToggle / OnDeploy; the control accessors return
  // mutable references because the dialog must be able to reset() them
  // on Hide() and assign fresh instances on Show().
  static HWND ActiveHwnd() { return s_hwnd; }
  static std::unique_ptr<fluxing::ui::FluxingButton>& DeployButton() {
    return s_deploy_button_;
  }
  static std::unique_ptr<fluxing::ui::FluxingToggle>& AsciiToggle() {
    return s_ascii_toggle_;
  }
  static std::unique_ptr<fluxing::ui::FluxingLabel>& TitleLabel() {
    return s_title_label_;
  }
  static std::unique_ptr<fluxing::ui::FluxingPanel>& CardPanel() {
    return s_card_panel_;
  }
  // spec 045 v0.18.29.0: additional row 2-5 controls.
  static std::unique_ptr<fluxing::ui::FluxingToggle>& SimpToggle() {
    return s_simp_toggle_;
  }
  static std::unique_ptr<fluxing::ui::FluxingToggle>& FullwidthToggle() {
    return s_fullwidth_toggle_;
  }
  static std::unique_ptr<fluxing::ui::FluxingLabel>& SchemaLabel() {
    return s_schema_label_;
  }
  static std::unique_ptr<fluxing::ui::FluxingButton>& SchemaButton() {
    return s_schema_button_;
  }
  static std::unique_ptr<fluxing::ui::FluxingButton>& UserFolderButton() {
    return s_user_folder_button_;
  }
  static std::unique_ptr<fluxing::ui::FluxingButton>& ProgramFolderButton() {
    return s_program_folder_button_;
  }
  static std::unique_ptr<fluxing::ui::FluxingButton>& QuitButton() {
    return s_quit_button_;
  }
  static std::function<void(bool)>& OnAsciiToggle() { return s_onAsciiToggle; }
  static std::function<void(bool)>& OnSimpToggle() { return s_onSimpToggle; }
  static std::function<void(bool)>& OnFullwidthToggle() {
    return s_onFullwidthToggle;
  }
  static std::function<void(const std::wstring&)>& OnSelectSchema() {
    return s_onSelectSchema;
  }
  static std::function<void()>& OnOpenUserFolder() {
    return s_onOpenUserFolder;
  }
  static std::function<void()>& OnOpenProgramFolder() {
    return s_onOpenProgramFolder;
  }
  static std::function<void()>& OnDeploy() { return s_onDeploy; }
  static std::function<void()>& OnQuit() { return s_onQuit; }

 private:
  // The active HWND, or NULL when no panel is shown. Used to enforce
  // the "only one panel at a time" invariant.
  static HWND s_hwnd;

  // Static callbacks captured at Show() time. The dialog owns the
  // lifetime; callbacks are valid only while the dialog is shown.
  static std::function<void(bool)> s_onAsciiToggle;
  static std::function<void(bool)> s_onSimpToggle;
  static std::function<void(bool)> s_onFullwidthToggle;
  static std::function<void(const std::wstring&)> s_onSelectSchema;
  static std::function<void()> s_onOpenUserFolder;
  static std::function<void()> s_onOpenProgramFolder;
  static std::function<void()> s_onDeploy;
  static std::function<void()> s_onQuit;

  // spec 038 + spec 045: spec 037 FluxingComponents wrappers. Stored as
  // unique_ptr so the destructor (which calls Destroy + KillTimer +
  // Unsubscribe) runs deterministically when Hide() or WM_DESTROY
  // resets them. AP-038-F: do NOT let the FluxingToggle 200ms slide
  // animation timer outlive the dialog.
  static std::unique_ptr<fluxing::ui::FluxingButton> s_deploy_button_;
  static std::unique_ptr<fluxing::ui::FluxingButton> s_quit_button_;
  static std::unique_ptr<fluxing::ui::FluxingButton> s_schema_button_;
  static std::unique_ptr<fluxing::ui::FluxingButton> s_user_folder_button_;
  static std::unique_ptr<fluxing::ui::FluxingButton> s_program_folder_button_;
  static std::unique_ptr<fluxing::ui::FluxingToggle> s_ascii_toggle_;
  static std::unique_ptr<fluxing::ui::FluxingToggle> s_simp_toggle_;
  static std::unique_ptr<fluxing::ui::FluxingToggle> s_fullwidth_toggle_;
  static std::unique_ptr<fluxing::ui::FluxingLabel> s_title_label_;
  static std::unique_ptr<fluxing::ui::FluxingLabel> s_schema_label_;
  static std::unique_ptr<fluxing::ui::FluxingPanel> s_card_panel_;
};