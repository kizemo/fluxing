#pragma once
//
// UserDictionary v0.19.0.28 — 用户词典管理 Modal 对话框
// spec: .specify/specs/044-user-dict/design.md
//
// 设计 (复用 spec 043 PhrasesDialog v0.19.0.27 模式 + spec 044 mockup v3):
// - Modal WS_POPUP + WS_EX_LAYERED (per-pixel alpha,跟 QuickPanel / PhrasesDialog 一致)
// - Title bar 38px 自绘 (mockup v3 TITLE_H=38, L96-AP-G 不再用 WS_CAPTION)
// - Toolbar: 搜索框 (h=38) + 已部署 pill + schema dropdown
// - Table: SysListView32 LVS_REPORT 4 列 (text/code/weight/schema) — 跟 QuickPanel / PhrasesDialog 同源
// - Weight signature: 数字 + 短轨道 slider (token `color.weight.slider.thumb`)
// - Add/Edit 子 modal: ES_AUTOHSCROLL text + trackbar weight slider + schema dropdown
// - YAML 文件 <user_data>/user_dict.yaml,自写 minimal parser,容错 fallback 空 list
// - RIME deploy worker thread: spec §3.3 10 步,StartMaintenance/EndMaintenance 包裹
//   (CLAUDE.md §2 强约束,leveldb LOCK 失败兜底)
// - Debounce save 500ms (复用 PhrasesDialog IDT_SAVE pattern),原子写 .tmp + MoveFileEx
// - LRU backup 保留 5 个 .bak.<timestamp> (spec §6.4)
// - Keyboard: ↑↓/Enter/Esc/Ctrl+N/E/F/S/A/Delete/F5 (spec §4)
//
// 历史雷区 (不重复犯错,见 lessons-learned L94/L95/L96/L97):
// 绝对不在 PhrasesDialog 同款踩坑点(UserDictionary 同样要走):
//   不在 WM_KEYDOWN / WndProc 回调中阻塞 I/O (constitution P2) → debounce 解决
//   不在子 dialog 用 EnableWindow(parent, FALSE) (L96-AP-A)
//   不在 OnPaint GradientFill 整个 client (L96-AP-B) — 只涂 [0, kTitleH)
//   不在 Hide() 时忘记 FlushSave (spec §10 risk)
//   不在 deploy 时省略 StartMaintenance (CLAUDE.md §2 / spec §11 risk)
//   必须在所有 paint path 调 RepaintLayered (L97 fix pattern)
//   deploy 失败 toast 跟 diagnostic print 必须拆开 (AP-L66-B)
//
#include <set>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>                          // v0.19.0.28-fix: SHGetFolderPathW (跟 PhrasesDialog 一致, 不用 deprecated RimeGetUserDataDir)
#include <RimeWithWeasel.h>                  // v0.19.0.28-fix: RimeWithWeaselHandler::StartMaintenance/EndMaintenance

// 测试友好的注入函数指针(默认 = mock RIME deploy,test 可换 mock)
class UserDictionary;
using UserDictDeployFn = bool (*)(const std::vector<class UserDictEntry>& entries,
                                  const std::wstring& dictName,
                                  std::wstring& errMsg);
using UserDictYamlIoFn = bool (*)(const std::wstring& path,
                                  std::vector<class UserDictEntry>& out,
                                  bool load);
using UserDictBackupFn = bool (*)(const std::wstring& path);

// 数据条目 (spec §5.1)
struct UserDictEntry {
  std::wstring text;
  std::wstring code;
  int          weight = 50;     // 0..100 (0 = auto;RIME 实际写 1)
  std::wstring schema;          // schema id,空 = 当前 active
};

class UserDictionary {
 public:
  // ===== 单例 API (复用 PhrasesDialog 模式) =====
  // Show() 加载 YAML + 创窗口 + PopulateList + ShowWindow
  // Hide() 先 FlushSave + KillTimer + JoinDeployThread + DestroyWindow
  static void Show();
  static void Hide();
  static bool IsVisible();

  // 注入 mock(测试用)
  static void SetDeployFn(UserDictDeployFn fn);
  static void SetYamlIoFn(UserDictYamlIoFn fn);
  static void SetBackupFn(UserDictBackupFn fn);

  // 设置 YAML 路径(测试用,默认 = <user_data>/user_dict.yaml via RimeGetUserDataDir)
  static void SetYamlPath(const std::wstring& path);

  // ===== 公开访问(供 Track 3 / 测试) =====
  static std::vector<UserDictEntry>& MutableEntries();
  static const std::vector<UserDictEntry>& Entries();

  // YAML I/O 公开(测试用)
  // load=true: Load;load=false: Save
  static bool LoadYaml(const std::wstring& path, std::vector<UserDictEntry>& out);
  static bool SaveYaml(const std::wstring& path,
                       const std::vector<UserDictEntry>& data);

  // TXT 格式转换(测试可验证 deploy worker 路径)
  // custom_phrase 格式:text\tcode\tweight\n
  static std::string EntriesToTxt(const std::vector<UserDictEntry>& entries);

  // 默认 mock deploy(测试环境无 librime,用 inline mock)
  static bool MockDeploy(const std::vector<UserDictEntry>& entries,
                         const std::wstring& dictName,
                         std::wstring& errMsg);

  // v0.19.0.28-fix: 真实 prod deploy 路径 — Code Review blocker 修复
  // 写 TXT + 备份 + Start/EndMaintenance 包裹 (CLAUDE.md §2 强约束)。
  // 默认 s_deployFn 指向此 (非 Mock);测试可通过 SetDeployFn(&MockDeploy) 替换。
  static bool ProductionDeploy(const std::vector<UserDictEntry>& entries,
                               const std::wstring& dictName,
                               std::wstring& errMsg);

  // LRU backup(测试可独立验证)
  static bool MakeBackup(const std::wstring& path);

  // 内部:listview populate(测试可用 mock HDC 验证 item 数量)
  static int PopulateListCount(HWND hList);

  // v0.19.0.28 测试/调试: 应用搜索过滤(对 listview 应用 LVIS_CUT dim)
  static int ApplySearchFilter(const std::wstring& searchQuery);

  // v0.19.0.28 测试/调试: 调度 debounce 写盘
  static void ScheduleSave();
  static void FlushSave();

  // v0.19.0.28 测试/调试: 显示 toast (右下角,4s 自动消失)+ HideToast。
  // kind: 0=info(灰) / 1=success(绿) / 2=error(红)
  static void ShowToast(const std::wstring& text, int kind = 0);
  static void HideToast();
  static bool IsToastVisible();

  // v0.19.0.28 测试/调试: 触发 Add/Edit 对话框。-1 = add,>=0 = edit index。
  static bool OpenEntryDialog(int entryIndex);
  static void CloseEntryDialog(bool save);

  // v0.19.0.28 测试/调试: 触发 deploy(异步 worker)。会 PostMessage WM_USER_DEPLOY_DONE。
  static void DeployAsync();
  // join deploy worker(测试可显式等线程结束)
  static void JoinDeployThread();
  static bool IsDeployInProgress();

  // State enum (spec §3)
  enum State {
    State_Hidden,
    State_Loading,
    State_Browsing,
    State_Search,
    State_Editing,        // Add/Edit modal open
    State_Deploying       // deploy worker 跑中
  };
  static State GetState() { return s_state; }

  // WndProc
  static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
  // Add/Edit modal WndProc (独立 class)
  static LRESULT CALLBACK EditDlgProc(HWND, UINT, WPARAM, LPARAM);

  // ===== Static state =====
  static HWND   s_hwnd;
  static HWND   s_hList;         // SysListView32
  static HWND   s_hSearch;       // 搜索框 EDIT
  static HWND   s_hStatus;       // 状态栏 STATIC (warn / info)
  static HWND   s_hToast;        // toast STATIC
  static HWND   s_hBtnAdd;       // + 添加
  static HWND   s_hBtnDel;       // − 删除
  static HWND   s_hBtnImport;    // 导入
  static HWND   s_hBtnExport;    // 导出
  static HWND   s_hBtnCancel;    // 取消
  static HWND   s_hBtnDeploy;    // ⟳ 部署
  static HWND   s_hEditDlg;      // Add/Edit 子 modal

  static std::wstring s_yamlPath;
  static UserDictDeployFn s_deployFn;
  static UserDictYamlIoFn s_yamlIoFn;
  static UserDictBackupFn s_backupFn;

  static std::wstring s_searchQuery;
  static bool   m_dirty;             // YAML 修改过但未 flush
  static DWORD  s_lastSaveFailTick;  // 上次写盘失败时间(0 = 无失败)
  static DWORD  s_showTime;          // Show() 时刻(GetTickCount)

  static State  s_state;

  // v0.19.0.28: Add/Edit modal 子控件
  static HWND   s_hEditText;        // text EDIT
  static HWND   s_hEditCode;        // code EDIT
  static HWND   s_hSliderWeight;    // weight trackbar
  static HWND   s_hComboSchema;     // schema dropdown
  static HWND   s_hBtnEditOk;       // 确定
  static HWND   s_hBtnEditCancel;   // 取消
  static int    m_editingIndex;     // -1 = add,>=0 = edit

  // v0.19.0.28: deploy worker thread
  static std::thread s_deployThread;
  static DWORD       s_deployStartTick;
  static DWORD       s_deployDoneTick;   // 0 = 未完成;非0 = GetTickCount()
  static int         s_deployResult;     // 0 = ok / pending,1 = success,2 = fail
  static std::wstring s_deployErrMsg;

  // Time constants (mirrors PhrasesDialog)
  static constexpr DWORD kSaveDebounceMs  = 500;   // IDT_SAVE
  static constexpr DWORD kDeployToastMs   = 4000;  // IDT_TOAST (deploy)
  static constexpr DWORD kShowGraceMs     = 2000;  // auto-hide grace
  static constexpr DWORD kDeployTimeoutMs = 30000; // deploy 线程 panic 兜底

  static constexpr UINT_PTR IDT_SAVE  = 9101;
  static constexpr UINT_PTR IDT_TOAST = 9102;
  static constexpr UINT_PTR IDT_DEPLOY_POLL = 9103;  // 进度条 poll

  // Custom window messages(主线程 ↔ deploy worker)
  static constexpr UINT WM_USER_DEPLOY_DONE = WM_USER + 0x200;

  static constexpr int kBackupKeepCount = 5;

  // v0.19.0.28: HFONT 静态持有,OnDestroy 释放。
  static HFONT s_hFontUi;

  // 物理几何(运行时算)
  static int kListH_phys;     // listview 高度
  static int kBtnY_phys;      // 按钮行起点 y

 private:
  // 内部 helpers(测试可达)
  static std::wstring Trim(const std::wstring& s);
  static std::wstring Unquote(const std::wstring& s);
  static COLORREF WeightColor(int weight);

  // WndProc 分发
  static LRESULT OnCreate(HWND);
  static LRESULT OnDestroy(HWND);
  static LRESULT OnPaint(HWND);
  static LRESULT OnKeyDown(HWND, WPARAM);
  static LRESULT OnNotify(HWND, LPARAM);
  static LRESULT OnCommand(HWND, WPARAM);
  static LRESULT OnTimer(HWND, WPARAM);
  static LRESULT OnCtlColor(HWND, WPARAM, LPARAM);
  static LRESULT OnDrawItem(HWND, LPARAM);

  // List populate / rebuild
  static int PopulateListImpl(HWND hList);

  // 工具
  static void CenterOnPrimaryMonitor(HWND hwnd, int w, int h);
  static void RepaintLayered(HWND hwnd);

  // 子 modal helpers
  static void EnterEditingState(int entryIndex);
  static void ExitEditingState(bool save);

  // 应用数据
  static std::vector<UserDictEntry> m_entries;
  static int m_selectedIndex;  // 当前单选;-1 = 无
  static std::set<int> m_multiSelected;  // 多选 index 集合
};