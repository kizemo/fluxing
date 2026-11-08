#pragma once
//
// ShortcutSettings v0.19.0.28 — 快捷键设置 Modal 对话框 (Track 3 of 3 UI)
// spec: .specify/specs/045-shortcut-settings/design.md
// chrome: 复用 PhrasesDialog v0.19.0.27 modal pattern (spec 043)
//
// 设计 (spec 045):
// - 800×680 modal WS_POPUP + WS_EX_LAYERED + SetWindowRgn(radius.lg=14) +
// hairline
// - 标题栏: 38px 自绘, ⌨ icon + "快捷键设置" 21px bold + subtitle 13px gray +
// 圆形 ✕
// - 工具栏: filter chips (全部/编辑类/切换类/部署类) + search box + ⌘F hint +
// luna_pinyin dropdown
// - 状态行: "● 未保存 (N 修改)" + "⚠ N 个冲突" red badge
// - 表格 3 列 (ListView LVS_REPORT): Action / Current / New
// - 键捕获 popover (signature element): 浮动 card + 实时 key combo 显示 + 闪烁
// caret
//   + 实时 conflict warning + Enter 确认 / Esc 还原 / Backspace 清空
// - 底栏: + 自定义 / ⟳ 恢复默认 (danger) / 导入 / 导出 / 取消 / 保存 (primary
// orange)
// - WH_KEYBOARD_LL hook: 本进程线程范围, 关闭即注销
// - 冲突检测: librime 内置 ~30 条 + 同表 cross-row; L18/L19 防御 (Shift_L/R
// 单键)
// - 落盘: <user_data_dir>/default.custom.yaml (L40 教训); 触发 WeaselDeployer
// /deploy
//
// 历史雷区 (不重复犯错,见 lessons-learned L09 / L18 / L19 / L40 / L66 / L94 /
// L97): ❌ 绝对不用 GDI+ (QuickPanel L67-L69 教训) ❌ 绝对不在 ShortcutSettings
// 内写硬编码颜色 (走 FLUENT-UI-TOKENS §3.6 / §3.6.3) ❌ 绝对不在 WH_KEYBOARD_LL
// hook 内 SendInput / IPC / 同步 I/O (constitution P2) ❌ 绝对不绑定 Shift_L/R
// 单键 ascii_mode (L18/L19 防御) ❌ 绝对不写 weasel.yaml (L40 教训; 写
// default.custom.yaml) ❌ 绝对不在 WndProc 忘记 RepaintLayered (L97 fix) ✅
// YamlRoundTrip 保留 key order (spec 024 已 ship) ✅ WH_KEYBOARD_LL
// 限定本进程线程, 关闭即 UnhookWindowsHookEx ✅ Grace guard
// (kShowGraceMs=2000): WM_ACTIVATEAPP/WM_KILLFOCUS 不立即 Hide
//
#include <string>
#include <vector>
#include <windows.h>
#include <commctrl.h>

// spec 095: 「快捷面板常驻」的运行期状态 + 落盘(与 QuickPanelDialog 共用
// 单一事实源,constitution R7)。
#include <FluxingAlwaysShow.h>

class ShortcutSettings {
 public:
  // ===== 单例 API =====
  // Show() 加载 default.custom.yaml + 创窗口 + PopulateTable + ShowWindow
  // Hide() 先 KillTimer + UnhookWindowsHookEx + DestroyWindow
  static void Show();
  static void Hide();
  static void Toggle();

  // spec 095: 「界面 > 快捷面板常驻」勾选框的生产 API。
  // AlwaysShowChecked() 读运行期状态;SetAlwaysShowChecked() 改状态 +
  // 落盘 + 通知观察者(alpha 复位 / 重绘)。两者都不需要建窗口 ⇒ 可被
  // test/TestShortcutSettings 直接断言(生产代码与测试共用同一实现)。
  static bool AlwaysShowChecked();
  static void SetAlwaysShowChecked(bool checked);

  // 设置 YAML 路径(测试用,默认 = <user_data>/default.custom.yaml via
  // RimeGetUserDataDir)
  static void SetYamlPath(const std::wstring& path);

  // ===== 公开访问 (供 Track 3 / 测试) =====

  // Hotkey 行 (UI 内状态; 修改只改 in-memory, Save() 才落盘)
  enum HotkeyAction {
    Action_None = 0,
    Action_ToggleChinese,     // toggle: ascii_mode
    Action_ToggleFullHalf,    // toggle: full_shape
    Action_ToggleAsciiPunct,  // toggle: ascii_punct
    Action_ToggleTrad,        // toggle: traditionalization
    Action_PageUp,            // send: Page_Up
    Action_PageDown,          // send: Page_Down
    Action_SelectN,           // send: N (select candidate N)
    Action_OpenQuickPanel,    // custom: show QuickPanel
    Action_OpenPhrases,       // custom: show PhrasesDialog
    // v0.19.0.60 (Phase L 调整 1): Action_OpenUserDict 删除 — UserDictionary
    // 模块下线。
    Action_ReselectCandidate,  // custom: reselect
    Action_SecondCandidate,    // custom: second candidate
    Action_TriggerDeploy,      // custom: trigger /deploy
    Action_VerifyHotkey,       // custom: verify hotkey after save
    Action_CustomUser,         // user-added custom binding
  };

  struct Hotkey {
    int action;                // HotkeyAction enum
    std::wstring action_desc;  // UI 显示 ("切换中英文")
    std::wstring
        action_yaml;      // YAML 字段 ("toggle: ascii_mode" / "send: Page_Up")
    std::wstring when;    // "always" / "composing" / "has_menu" / "paging"
    std::wstring accept;  // 当前键 ("Shift+Space")
    std::wstring accept_new;  // 用户编辑的新键 (空 = 未改)
    std::wstring yaml_raw;    // 自定义行的 raw yaml fallback
    bool is_custom;           // 用户自定义行
    bool
        is_default;  // 来自 default.yaml 内置 (vs default.custom.yaml override)
  };

  // YAML 落盘 (T106 托管块)。public 是为了让 test/TestShortcutSettings 能直接
  // 盯落盘逻辑;界面"保存"与"导出"共用同一实现,overwrite_file=true 表示整份
  // 覆写(导出/全新文件),false 表示只替换文件里的托管块、保留块外内容。
  static bool SaveHotkeys(const std::wstring& path,
                          const std::vector<Hotkey>& data,
                          bool overwrite_file = false);

  static std::vector<Hotkey>& MutableHotkeys();
  static const std::vector<Hotkey>& Hotkeys();

  // 当前编辑行 (Capturing 状态); -1 = no capture
  static int s_capturingRow;
  // 当前 pending combo (Capturing 中累积)
  static std::wstring s_pendingChord;

  // ===== WndProc =====
  static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

  // ===== Static state =====
  static HWND s_hwnd;
  static HWND s_hTable;    // ListView (LVS_REPORT, 3 列)
  static HWND s_hStatus;   // 状态行 STATIC
  static HWND s_hToolbar;  // 工具栏容器
  static HWND s_hSearch;   // 搜索框 EDIT
  static HWND s_hPopover;  // 键捕获 popover (子窗口)
  static HWND s_hBtnAdd;
  static HWND s_hBtnReset;
  static HWND s_hBtnImport;
  static HWND s_hBtnExport;
  static HWND s_hBtnCancel;
  static HWND s_hBtnSave;
  static HWND s_hChkAlwaysShow;  // spec 095: 「快捷面板常驻」勾选框
  static std::wstring s_yamlPath;
  static DWORD s_showTime;  // Show() 时刻(GetTickCount), auto-hide grace

  // WH_KEYBOARD_LL hook handle (Capturing 时挂上)
  static HHOOK s_hKeyboardHook;

  // 字号 / 几何 (运行时计算, dpi-aware)
  static int kTableH_phys;
  static int kBtnY_phys;

  // 时间常量 (FLUENT-UI-TOKENS §3.6.3)
  static constexpr DWORD kShowGraceMs = 2000;    // grace auto-hide
  static constexpr DWORD kCaptureBlinkMs = 500;  // caret blink
  static constexpr DWORD kPopoverShowMs = 150;   // fade-in

  // Timer IDs (与 PhrasesDialog 错开; 8xxx 段)
  static constexpr UINT_PTR IDT_BLINK = 8501;
  static constexpr UINT_PTR IDT_POPOVER = 8502;

  // 子控件 ID (2100-2199 段; PhrasesDialog 用 1010-1205)
  static constexpr UINT ID_SEARCH = 2100;
  static constexpr UINT ID_TABLE = 2101;
  static constexpr UINT ID_BTN_ADD = 2110;
  static constexpr UINT ID_BTN_RESET = 2111;
  static constexpr UINT ID_BTN_IMPORT = 2112;
  static constexpr UINT ID_BTN_EXPORT = 2113;
  static constexpr UINT ID_BTN_CANCEL = 2114;
  static constexpr UINT ID_BTN_SAVE = 2115;
  static constexpr UINT ID_POPOVER = 2120;
  // spec 095: 「界面 > 快捷面板常驻」勾选框 ID(2130 段空闲)。
  static constexpr UINT ID_CHK_ALWAYS_SHOW = 2130;

  // 静态 hook proc (WH_KEYBOARD_LL)
  static LRESULT CALLBACK LowLevelKeyboardProc(int, WPARAM, LPARAM);

 private:
  // WndProc 分发
  static LRESULT OnCreate(HWND);
  static LRESULT OnDestroy(HWND);
  static LRESULT OnPaint(HWND);
  static LRESULT OnNotify(HWND, LPARAM);
  static LRESULT OnCommand(HWND, WPARAM);
  static LRESULT OnTimer(HWND, WPARAM);
  static LRESULT OnKeyDown(HWND, WPARAM);
  static LRESULT OnCtlColor(HWND, WPARAM, LPARAM);

  // Table populate / rebuild
  static void PopulateTable();
  static void UpdateRow(int row);  // 局部更新一行 (Capturing 状态变化)

  // 搜索过滤 (substring + case-insensitive)
  static void ApplySearchFilter(const std::wstring& q);

  // Capturing state machine
  static void EnterCapturing(int row);
  static void ExitCapturing(bool save);
  static void ResetCapturingRow();  // Esc 还原

  // 捕获提示: popover + 状态行 (抓到的组合 / 拒绝原因)
  static void SetCaptureHint(const wchar_t* text);

  // T101: popover 单独更新 —— 状态行不再被捕获提示覆盖,改由 RefreshStatus()
  // 统一持有 ("● 未保存 (N 修改)" + 冲突计数)。
  static void SetPopoverHint(const std::wstring& text);

  // Conflict detection (spec §4.5)
  struct ConflictReport {
    int score =
        0;  // 0 = none, 1 = warn (builtin), 2 = error (L18/L19/duplicate)
    std::wstring conflict_action;  // 冲突的 action desc (UI 显示)
    std::wstring warning_action;   // builtin override 警告 (UI 显示)
    bool l18_l19_error = false;
    bool duplicate_error = false;
  };
  static ConflictReport CheckConflict(const std::wstring& combo,
                                      const std::wstring& when,
                                      int exclude_row);

  // T101: 把 ConflictReport 翻成 design §4.5 的四档中文文案
  //   ✓ 无冲突
  //   ⚠ 与「{action}」冲突，仍可保存
  //   ⚠ 覆盖默认行为「{action}」
  //   ✗ 此组合不被允许 (Shift_L/R 单键)
  static std::wstring ConflictHintText(const ConflictReport& r);

  // T101: 捕获过程中的实时冲突检测 —— 读 s_pendingChord + s_capturingRow,
  // 结果写 popover 与状态行。只做内存比较,不做 I/O (design AP-045-B)。
  static void UpdateCaptureConflict();

  // T102: 状态行统一刷新。所有会改动 in-memory 状态的路径最后都落到
  // PopulateTable(),由它收口调用这里 ⇒ 编辑后状态行不会停在旧值。
  static void RefreshStatus();

  // 静态内置 binding db (spec §6.1 ~30 条)
  struct BuiltinBinding {
    std::wstring combo;  // normalized
    std::wstring when;
    std::wstring action;
    std::wstring desc_zh;
  };
  static const std::vector<BuiltinBinding>& GetBuiltinBindings();

  // YAML 读入 (SaveHotkeys 见 public 区: 单测要直接调)
  static bool LoadHotkeys(const std::wstring& path, std::vector<Hotkey>& out);
  // 加载 default.yaml 内置 (作为 fallback)
  static bool LoadDefaults(std::vector<Hotkey>& out);

  // Save() 触发 deploy 链 (spec §7.1)
  static bool SaveAndDeploy();
  // spec 045 P0: 原来 SpawnDeploy() 是空占位(状态栏却写"deploy 已触发")。
  // 现在真的去拉 WeaselDeployer.exe /deploy,并把成功与否返回给调用方。
  static bool SpawnDeploy();
  // spec 045 P0: 导入/导出用的往返文件(与线上 default.custom.yaml 分开,
  // 导出不再覆写线上配置)。
  static std::wstring ExportYamlPath();

  // 工具
  static void CenterOnPrimaryMonitor(HWND, int, int);
  static std::wstring ToLower(const std::wstring& s);
  static bool ContainsCI(const std::wstring& h, const std::wstring& n);
  static void RepaintLayered(HWND hwnd);  // L97 fix

  // 数据 (in-memory)
  static std::vector<Hotkey> m_hotkeys;
  static std::vector<Hotkey> m_defaults;  // reset 时还原

  // T103: 搜索框过滤词 (空 = 显示全部)。PopulateTable 按它重建列表 ⇒
  // 过滤是"真的不显示",而不是把匹配结果算完就丢掉。
  static std::wstring s_filterQuery;

  // T102: 当前状态行是否存在冲突 (供 OnCtlColor 决定文字颜色)
  static bool s_statusConflict;
};