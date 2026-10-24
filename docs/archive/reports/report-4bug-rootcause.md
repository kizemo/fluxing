# 4 User-Reported Bug Root-Cause Report (read-only)

**Scope:** v0.19.0.34 (`release/fluxing-0.19.0.34-installer.exe`, md5 `43dca014e04e6da3c15072842dce5ba1`)
**Target source tree:** `WeaselServer/{UserDictionary.cpp, PhrasesDialog.cpp, ModalChrome.cpp}` at HEAD `ccd2d954`
**Author:** systematic-debugging + frontend-ui-engineering subagent (read-only)

---

## Axis 1 — UserDict 文本与按键被 dialog 边缘遮挡

### 现象 (user-reported)
- 6 底部按钮 (添加 / 删除 / 导入 / 导出 / 取消 / 部署) 在 dialog 底部边缘被切
- ListView 列 header "text / code / weight / schema" 被切
- title bar 下方的 "已部署" pill 与搜索框视觉重叠

### 根因 (file:line 精确定位)

**主因 — `WeaselServer/UserDictionary.cpp:82-83` 几何常量比 v0.19.0.28 (c6fec000) 缩了 120 px:**
```
constexpr int kDialogW      = 760;
constexpr int kDialogH      = 480;   // ← v0.19.0.28 是 600 (spec 044 §7),现缩水 120px
```

**次因 — `UserDictionary.cpp:1109-1115` OnCreate 的物理几何反推公式:**
```cpp
kListH_phys = kDialogH - kTitleH - kSearchH - kStatusBarH - kBtnH - 4 * kGap;
//         = 480 - 38 - 28 - 24 - 32 - 32
//         = 326px (clip, since 326 ≥ 80)
kBtnY_phys  = kTitleH + kSearchH + kListH_phys + 2 * kGap;
//          = 38 + 28 + 326 + 16 = 408
if (kBtnY_phys + kBtnH > kDialogH)              // 408 + 32 = 440 ≤ 480 → **不会 clamp**
    kBtnY_phys = kDialogH - kBtnH - 2;           // 但 status bar 已经在 326+388=386~410 段
```
→ 实际 list 顶 = `kTitleH + kSearchH + kGap = 38+28+8 = 74`,list 底 = `listY + kListH_phys = 74+326 = 400`。
→ 但 `kStatusY = listY + kListH_phys = 400`(UserDictionary.cpp:1183),status bar 占用 `y=400..424`,**跟底部按钮 y=408-440 完全重叠**(UserDictionary.cpp:1213)。
→ ListView 列 header (`LVCF_TEXT`) 默认在 list 顶部,跟 "已部署" pill (`{16, kTitleH+6, 96, kTitleH+26}` = y=44..64) 也挤在 title bar 下沿。

**ModalChrome 圆角 — `ModalChrome.cpp:32-42` PaintBorder + `UserDictionary.cpp:241-245` SetWindowRgn:**
```cpp
HRGN rgn = CreateRoundRectRgn(0, 0, kDialogW, kDialogH, kDlgRadius * 2, kDlgRadius * 2);
```
→ `kDlgRadius=14`(UserDictionary.cpp:94),圆角 28px,**listview/buttons 矩形 region 跟圆角 region 像素重叠时 (radius=14, button y=408-440)** ,bottom-right 圆角会"咬掉"按钮右半部约 8px。
→ `SetWindowRgn` 的 `TRUE` (redraw) 参数会让 chrome 边框命中边界, 圆角区段=1 像素白边盖到子控件。

### 真因一句话总结
`kDialogH=480` 太小, **status bar (kStatusBarH=24) 被公式漏算** — list 公式只减 5 项,但底部实际占 6 项(title+search+list+status+btn+gaps),所以 status bar 跟 buttons 在 y=408..440 像素重叠,被 SetWindowRgn 圆角咬掉。

### Fix 方向 (排序见 §修复方案排序)
1. **P0:** 把 `kDialogH` 从 480 改回 v0.19.0.28 的 600 (UserDictionary.cpp:83)。
2. **P1:** 公式重写为 `kDialogH = kTitleH + kSearchH + kListH_phys + kStatusBarH + kBtnH + 5*kGap`,并验证 `kBtnY_phys + kBtnH ≤ kDialogH - kDlgRadius`。
3. **P1:** 删 SetWindowRgn (`UserDictionary.cpp:241-245`) 改用 ModalChrome 自绘圆角 + child region 钳到 (`kDlgRadius, kDlgRadius, kDialogW-kDlgRadius, kDialogH-kDlgRadius`)。

---

## Axis 2 — UserDict 6 按钮 click 触发 dialog Hide / 功能未实现

### 现象 (user-reported)
- 点任何底部按钮 → dialog 消失
- Add 按钮点了没添加 entry
- Import/Export 点了没反应

### 根因 (file:line 精确定位)

**主因 — `UserDictionary.cpp:1515-1549` OnCommand handler 是 "占位" 实现:**
```cpp
case ID_BTN_IMPORT:
    // 占位: 真实实现需要打开 file dialog + parse custom_phrase.txt
    ShowToast(L"\u5bfc\u5165\u529f\u80fd\u8bf7\u7528\u53f3\u952e\u83dc\u5355");  // ← 用 toast 假装
    return 0;
case ID_BTN_EXPORT:
    ShowToast(L"\u5bfc\u51fa\u529f\u80fd\u8bf7\u7528\u53f3\u952e\u83dc\u5355");  // ← 同上
    return 0;
```
→ Import/Export 完全没实现 (commit msg 也确认了 v0.19.0.28 follow-up)。

**主因 — `UserDictionary.cpp:1073-1080` WM_KILLFOCUS 强制 Hide:**
```cpp
case WM_KILLFOCUS: {
    if (s_state == State_Editing) return 0;
    DWORD nowTick = GetTickCount();
    if ((nowTick - s_showTime) >= kShowGraceMs) {
      Hide();    // ← 点 button = focus 离开 listview → WM_KILLFOCUS → Hide()
    }
    return 0;
}
```
→ 6 个按钮是子控件,点击时鼠标点到 button → 原 ListView 失焦 → `WM_KILLFOCUS` → `Hide()` 立即关掉 dialog。**这是 "click 触发 dialog Hide" 的直接路径**。
→ 该 handler 没有 white-list 子控件 — 跟 `PhrasesDialog.cpp:401-416` 同样的"忽略焦点到子控件" 模式不同,UserDict **漏写 child-focus 豁免**。

**次因 — `UserDictionary.cpp:1517-1528` Add/Del 实现不写盘:**
```cpp
case ID_BTN_ADD:
    EnterEditingState(-1);   // OK,弹出子 dialog (走 EnterEditingState)
    return 0;
case ID_BTN_DEL:
    if (m_selectedIndex >= 0 && ...) {
        m_entries.erase(m_entries.begin() + m_selectedIndex);
        PopulateListImpl(s_hList);
        ScheduleSave();      // ← 走 500ms debounce,延迟写盘;但 dialog 已 Hide()
        m_selectedIndex = -1;
        if (s_hBtnDel) EnableWindow(s_hBtnDel, FALSE);
    }
    return 0;
```
→ Add 走 EnterEditingState OK,但用户关 dialog 后子 dialog 走 `Hide()` (UserDictionary.cpp:852-897 ExitEditingState) 没 FlushSave — **save timer 未触发就被 KillTimer** (UserDictionary.cpp:259 `KillTimer(s_hwnd, IDT_SAVE)`)。
→ Del 调 `ScheduleSave()` 但 dialog 在 2000ms 内 Hide,`KillTimer` 把 IDT_SAVE 杀掉 → 修改丢失。

**主因 — `UserDictionary.cpp:1537-1539` Cancel 直接 Hide(设计本意),但被 KILLFOCUS 抢先:**
```cpp
case ID_BTN_CANCEL:
    Hide();
    return 0;
```
→ 用户点 Cancel 之前 ListView 失焦就触发 KILLFOCUS → Hide,所以 Cancel 看起来"点了就消失",**用户感不到 Cancel 实际有效果**。

### Fix 方向
1. **P0:** `WM_KILLFOCUS` handler 加 child-focus 白名单 (`UserDictionary.cpp:1073-1080`),white-list s_hBtnAdd/Del/Import/Export/Cancel/Deploy,跟 `PhrasesDialog.cpp:402-410` 对齐。
2. **P0:** ID_BTN_IMPORT: 打开 file picker (`IFileOpenDialog`) + parse `custom_phrase.txt` → `m_entries.insert(...)` → `PopulateListImpl` → `FlushSave`。
3. **P0:** ID_BTN_EXPORT: 打开 save picker + `EntriesToTxt()` + 写 UTF-8 BOM。
4. **P0:** ID_BTN_ADD: 子 dialog close 后立刻 `FlushSave()` (不依赖 timer)。
5. **P1:** 删 IDT_SAVE debounce,Add/Edit/Del 改用直接 `FlushSave()` (跟 PhrasesDialog 一致)。

---

## Axis 3 — PhrasesDialog 输入框只能英文 (ESL_INPUT LANGUAGE 错)

### 现象 (user-reported)
- 顶部 input 只能输入英文字母,无法调出中文 IME

### 根因 (file:line 精确定位)

**主因 — `PhrasesDialog.cpp:439-447` OnCreate 创 Edit 时无 IME 关联:**
```cpp
s_hInput = CreateWindowExW(
    WS_EX_CLIENTEDGE, L"EDIT", L"",
    WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS |
        ES_AUTOHSCROLL,                 // ← **没 ES_MULTILINE / WS_CLIPCHILDREN / IME 关联**
    inputX, inputY, kInputW, kInputH, hwnd,
    reinterpret_cast<HMENU>(ID_INPUT),
    GetModuleHandle(nullptr), nullptr);
```
→ ESL_INPUT LANGUAGE 是 modal dialog 顶层 hwnd 关联的 default input language (`LANG_ENGLISH_US` = 0x0409),**没 `ImmAssociateContextEx(s_hInput, NULL, IACE_DEFAULT)` 或 override**。
→ Windows 默认把 IME 上下文从 caller thread 继承 — 但 `WeaselServer` 是 TSF server,自身 IME context 已被劫持 → Edit 创出来是 "english-only mode"。

**主因 — `PhrasesDialog.cpp:166-171` 圆角 region 把 IME composition window 切掉:**
```cpp
HRGN rgn = CreateRoundRectRgn(0, 0, kDialogW, kDialogH,
                              kDlgRadius * 2, kDlgRadius * 2);
if (rgn) {
    SetWindowRgn(s_hwnd, rgn, TRUE);   // ← **SetWindowRgn 限制 child window region**
}
```
→ Windows IME composition window / candidate window 弹在父窗口外部时被 region clip → 即使 IME 上下文 enable 了,composition UI 显示不全 → 用户看不到 "正在打字" 的 candidate → 误以为 "不能输入中文"。
→ 这跟 UserDictionary 同样问题 (`UserDictionary.cpp:241-245`),**ModalChrome 没考虑 IME child window region**。

**对比 UserDictionary 注释 — `UserDictionary.cpp:1117-1120`:**
```cpp
s_hFontUi = CreateFontW(kUiFontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                        FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                        VARIABLE_PITCH | FF_SWISS, L"Segoe UI Variable");
```
→ 没设 `lfFaceName="Segoe UI Variable"` 中文 fallback font (Microsoft YaHei UI / SimSun),导致部分 IME candidate UI 字形 fallback 失败。

**历史线索 — v0.19.0.32 commit msg:**
> 修法: 重写 UX: 顶部 1 个 input (s_hInput) + 1 个 Add 按钮 (s_hBtnAddTop) 同行
> **中间 ListView (s_hList) 单列 "短语" (替代 TreeView, 简化)**

→ v0.19.0.32 是把 v0.19.0.31 inline-edit (s_hEditText / s_hEditCat) 完全删了,**重写时没保留 v0.19.0.31 inline-edit 路径下的 IME 关联 (如果有)** — 老版本没修这个 bug,新版本直接丢功能。

### 真因一句话总结
**Edit 控件没设 IME 关联 + 圆角 region clip IME composition window** → 中文 IME 弹不出 composition UI,用户看似"只能输入英文"。

### Fix 方向
1. **P0:** `OnCreate` 创 `s_hInput` 后调 `ImmAssociateContextEx(s_hInput, NULL, IACE_DEFAULT)`,并 `ImmReleaseContext` 触发 user32 重新探测 default IME。
2. **P0:** 删 `SetWindowRgn` (`PhrasesDialog.cpp:166-171`),改 ModalChrome 自绘圆角 + `WS_CLIPCHILDREN` 让 IME child window 不被切。
3. **P1:** font fallback 加 `lfFaceName="Microsoft YaHei UI"` 优先,失败 fallback "Segoe UI Variable"。

---

## Axis 4 — PhrasesDialog 缺少短语分类 / 默认选 / 键盘导航

### 现象 (user-reported, 5 sub-claims)
1. 没分类 (TreeView)
2. 默认不选第一条
3. 回车没上屏
4. 上下箭头没切换选中
5. 右箭头没展开分类
6. 分类默认展开关闭

### 根因 — v0.19.0.32 (cd6f61a9) 删 5 大功能

**commit `cd6f61a9` (v0.19.0.32) PhrasesDialog.h diff 全清单 (来自 `git show cd6f61a9 -- WeaselServer/PhrasesDialog.h`):**
```
-  static HWND   s_hTree;                                    // ← 删 TreeView
-  static HWND   s_hSearch;                                  // ← 删 search box
-  static HWND   s_hStatus;                                  // ← 删 status bar
-  static HWND   s_hToast;                                   // ← 删 toast
-  static HWND   s_hBtnSave;                                 // ← 删 Save 按钮
-  static std::unordered_map<std::wstring, bool> m_expanded; // ← 删折叠状态
-  // 折叠状态:key = category(L"" = 未分类)
-  struct Phrase { std::wstring text; std::wstring category; }; // ← 删 category 字段
-  static int m_editingIndex;                                // ← 删 inline edit index
-  static HWND s_hEditText;                                  // ← 删 inline edit text
-  static HWND s_hEditCat;                                   // ← 删 inline edit category
-  static std::wstring s_searchQuery;                         // ← 删 search query
-  static bool m_dirty;                                       // ← 删 dirty flag
-  static DWORD s_lastSaveFailTick;                          // ← 删 save fail tick
-  enum State { State_Hidden, State_Loading, State_Browsing, State_Search, State_Editing };  // ← 4 态 → 2 态
-  static void PopulateTree(HWND hTree);                     // ← 删 tree populate
-  static std::vector<HTREEITEM> CollectVisibleItems(HWND);   // ← 删导航辅助
-  static HTREEITEM MoveSelection(HWND, int dir);             // ← 删键盘导航
-  static void HandleLeftRight(HWND, bool right);             // ← **删 ←/→ 展开/折叠**
-  static void HandleEnter(HWND);                            // ← **删回车上屏逻辑**
-  static void EnterEditingState(int, bool);                 // ← 删 inline edit
-  static void ExitEditingState(bool);                       // ← 删
-  static void ShowStatusWarning(...);                       // ← 删
-  static void SetButtonsForBrowsing();                      // ← 删
-  static void SetButtonsForEditing();                       // ← 删
```

### 6 sub-claim 逐条 file:line 证据

**(a) 没分类:**
- `PhrasesDialog.h:34-36` (v0.19.0.34 head):
  ```cpp
  struct Phrase {
    std::wstring text;
    // ← category 字段没了 (cd6f61a9 删)
  };
  ```
- `PhrasesDialog.cpp:744-756` PopulateListImpl 用 ListView 单列,没 TreeView:
  ```cpp
  LVITEMW item = {};
  item.mask = LVIF_TEXT | LVIF_PARAM;
  item.iItem = static_cast<int>(i);
  item.iSubItem = 0;
  item.pszText = const_cast<wchar_t*>(m_phrases[i].text.c_str());
  // ← 单列 "短语",无 category subitem
  ```

**(b) 默认不选第一条:**
- `PhrasesDialog.cpp:515-520` OnCreate 末尾:
  ```cpp
  PopulateList(s_hList);
  // v0.19.0.32: 默认焦点在顶部 input
  if (s_hInput) {
    SetFocus(s_hInput);   // ← 焦点在 input,不是 list 第一条
  }
  ```
- 没有 `ListView_SetItemState(s_hList, 0, LVIS_SELECTED | LVIS_FOCUSED, ...)` 调用。
- 对比 UserDictionary `UserDictionary.cpp:1230-1234` 有正确 default-select 实现:
  ```cpp
  if (ListView_GetItemCount(s_hList) > 0) {
    ListView_SetItemState(s_hList, 0, LVIS_SELECTED | LVIS_FOCUSED,
                          LVIS_SELECTED | LVIS_FOCUSED);
    SetFocus(s_hList);
  }
  ```
- **UserDict 有,PhrasesDialog 缺 — 真因。**

**(c) 回车没上屏:**
- `PhrasesDialog.cpp:579-593` OnKeyDown:
  ```cpp
  case VK_RETURN: {
    if (s_hInput && GetFocus() == s_hInput) {        // ← 焦点在 input → 调 Add
      SendMessageW(hwnd, WM_COMMAND, MAKEWPARAM(ID_BTN_ADD_TOP, BN_CLICKED), 0);
      return 0;
    }
    if (s_hList && GetFocus() == s_hList && m_selectedIndex >= 0 && ...) {
      InjectText(m_phrases[m_selectedIndex].text);
      Hide();
    }
    return 0;
  }
  ```
- 逻辑看似有,但 `m_selectedIndex=-1` 初始值,且没 default-select,所以**焦点在 list 时回车不触发任何东西**。

**(d) 上下箭头没切换:**
- `PhrasesDialog.cpp:575-622` OnKeyDown:
  ```cpp
  switch (wp) {
    case VK_RETURN: { ... }
    case VK_ESCAPE: Hide(); return 0;
    case VK_DELETE: { ... }
    case VK_TAB: { ... }
  }
  return DefWindowProcW(hwnd, WM_KEYDOWN, wp, 0);  // ← VK_UP / VK_DOWN fall through
  ```
- `DefWindowProcW` 让 Windows 处理 → 因为 ListView focus 状态下 Windows 默认 ↑↓ 切换 ListView item,**这部分应 work**。
- 但实测 user report "上下箭头没切换" — 推测是 `WM_KILLFOCUS` (PhrasesDialog.cpp:400-416) 在 ListView 不在 focus 时立刻 Hide,所以用户"按上下箭头" = ListView 失焦 = Hide → 看不到切换效果。

**(e) 右箭头没展开分类:**
- `PhrasesDialog.cpp:575-622` OnKeyDown 没 `VK_RIGHT / VK_LEFT` case,完全没 `HandleLeftRight` 函数 — **cd6f61a9 删了**。
- v0.19.0.25 老代码 (commit 3f2c1694) 写过 TreeView 的 ←/→ 展开/折叠,被 cd6f61a9 删掉。

**(f) 分类默认关闭 (无法验证):**
- cd6f61a9 删了 `m_expanded` 字段,TreeView 没了,这个 claim 不适用 (已经无分类 → 无展开/折叠语义)。

### Fix 方向
1. **P0:** `Phrase` struct 加回 `std::wstring category` 字段 (`PhrasesDialog.h:34-36`)。
2. **P0:** OnCreate (`PhrasesDialog.cpp:515-520`) 加 `ListView_SetItemState(s_hList, 0, LVIS_SELECTED|LVIS_FOCUSED, ...)` (跟 UserDictionary 对齐)。
3. **P0:** 引入 TreeView (s_hTree) 替代 ListView (s_hList),或者保留 ListView 但加 category 列 + 上下箭头跳分类。
4. **P0:** OnKeyDown 加 `VK_UP/VK_DOWN/VK_LEFT/VK_RIGHT` 处理 (return to v0.19.0.25 的 `HandleLeftRight` 实现)。
5. **P1:** YAML load/save (`PhrasesDialog.cpp:226-290 / 292-334`) 加回 category 字段读写 (v0.19.0.32 注释 "向后兼容忽略 category" 应改成 "向前兼容保留 category")。

---

## Axis 5 — git history 对比 (老 UI → v0.19.0.32 删什么 → v0.19.0.34 还缺什么)

### 关键 commit 时间线

| commit | date | 含义 | PhrasesDialog 影响 |
|---|---|---|---|
| `3f2c1694` | 2026-07-13 | v0.19.0.25 引入 TreeView + 分类 + Alt+. | **新增**:s_hTree / s_hEditText / s_hEditCat / m_editingIndex / State_Editing / State_Search / HandleLeftRight / HandleEnter / MoveSelection / CollectVisibleItems / EnterEditingState / ExitEditingState / SetButtonsForBrowsing / SetButtonsForEditing / ShowStatusWarning / ApplySearchFilter / ScheduleSave / PopulateTreeCount / BeginInlineEdit |
| `2c0b2a16` | 2026-07-14 | v0.19.0.26 bug fix (grace + mac style + cleanup) | 微调 |
| `c6fec000` | 2026-07-14 | v0.19.0.28 重写 v2 (search box + status bar + inline edit) | 引入 s_hSearch / s_hStatus / s_hToast / s_hBtnSave / kSaveDebounceMs / IDT_SAVE / IDT_TOAST |
| `aa3f9b94` | 2026-07-15 | v0.19.0.30 grace + SetFocus + child-focus | 修 KILLFOCUS 加子控件白名单 |
| `cef9c67a` | 2026-07-15 | v0.19.0.31 modal chrome paint 修复 | 微调 |
| **`cd6f61a9`** | **2026-07-15** | **v0.19.0.32 UX redo** | **删 13 个功能**(见 Axis 4 清单) — 单 ListView 替代 TreeView, 删 keyboard nav, 删 category 字段, 删 inline-edit |
| `deebaf74` | 2026-07-17 | v0.19.0.33 Phase A Phrase module 修复 | 修 alt+/ 路由 + body paint |
| `8fe29885` | 2026-07-18 | v0.19.0.33 Phase B Bug 2b | body paint BeginPaint 路径 |
| `463ee158` | 2026-07-18 | clean-code review | wchar_t + 4 分支 refactor + 删 RepaintLayered |
| `ccd2d954` | 2026-07-18 | v0.19.0.34 installer ship | — |

### 老 Phrase UI 实现精确引用 (供恢复用)

**1. Phrase struct + category 字段 — 老 v0.19.0.27 (c6fec000) 状态:**
- `git show c6fec000:WeaselServer/PhrasesDialog.h` (line 50-53):
  ```cpp
  struct Phrase {
    std::wstring text;
    std::wstring category;  // "" = uncategorized
  };
  ```
- `git show c6fec000:WeaselServer/PhrasesDialog.cpp` line 280-305 `LoadPhrases` 完整 category 读取逻辑。
- `git show c6fec000:WeaselServer/PhrasesDialog.cpp` line 380-415 `SavePhrases` 完整 category 写入逻辑。

**2. TreeView 老实现 (v0.19.0.25 引入,v0.19.0.27 spec 043 强化) — `3f2c1694` head + `c6fec000`:**
- `git show c6fec000:WeaselServer/PhrasesDialog.cpp` line 660-750 `PopulateTreeImpl`:
  ```cpp
  // 按 category 分组,parent 是 category item (e.g. "问候语"),child 是 phrase item
  TVINSERTSTRUCTW tvis = {};
  tvis.hParent = hCatItem;
  tvis.item.pszText = (LPWSTR)p.text.c_str();
  tvis.item.lParam = (LPARAM)i;
  TreeView_InsertItem(hTree, &tvis);
  ```

**3. 键盘导航老实现 — `git show c6fec000:WeaselServer/PhrasesDialog.cpp` line 850-920:**
- `CollectVisibleItems(HWND)` 返回 (category parent + visible children) 顺序
- `MoveSelection(HWND, int dir)` 移动 LVIS_SELECTED
- `HandleLeftRight(HWND, bool right)` → right: 折叠 cat 时展开,展开 cat 时跳入第一个 child;left: 反之
- `HandleEnter(HWND)` → 选中 cat 展开/折叠,选中 phrase → InjectText + Hide

**4. 默认选中第一条 — 老 `git show c6fec000:WeaselServer/PhrasesDialog.cpp` line 1210-1218:**
  ```cpp
  // 找到第一个非 cat item,默认选中
  HTREEITEM first = TreeView_GetChild(hTree, TreeView_GetRoot(hTree));
  // 跳到下一个 visible item 直到是 phrase (不是 cat)
  ```

**5. v0.19.0.30 引入的 KILLFOCUS 子控件白名单 — `git show aa3f9b94:WeaselServer/PhrasesDialog.cpp`:**
  ```cpp
  case WM_KILLFOCUS: {
    HWND newFocus = (HWND)wp;
    if (newFocus == s_hSearch || newFocus == s_hTree ||
        newFocus == s_hBtnAdd || ...) return 0;
    DWORD nowTick = GetTickCount();
    if ((nowTick - s_showTime) >= kShowGraceMs) Hide();
  }
  ```
- UserDict (UserDictionary.cpp:1073-1080) **没继承这个 fix** — Axis 2 真因。

### 老 git 引用速查表 (用于 Phase C 恢复)

| 需求 | commit | file | line |
|---|---|---|---|
| TreeView 替代 ListView (分类) | `c6fec000` | `WeaselServer/PhrasesDialog.cpp` | 660-750 (PopulateTreeImpl) |
| category 字段 (Phrase struct) | `c6fec000` | `WeaselServer/PhrasesDialog.h` | 50-53 |
| ←/→ 展开/折叠分类 | `c6fec000` | `WeaselServer/PhrasesDialog.cpp` | 850-920 (HandleLeftRight) |
| ↑↓ 跨 cat 导航 | `c6fec000` | `WeaselServer/PhrasesDialog.cpp` | 880-910 (CollectVisibleItems + MoveSelection) |
| 默认选第一条 | `c6fec000` | `WeaselServer/PhrasesDialog.cpp` | 1210-1218 |
| 回车上屏 | `c6fec000` | `WeaselServer/PhrasesDialog.cpp` | HandleEnter 函数 |
| KILLFOCUS 子控件白名单 | `aa3f9b94` | `WeaselServer/PhrasesDialog.cpp` | WM_KILLFOCUS case |
| UserDict KILLFOCUS 白名单 | — | `WeaselServer/UserDictionary.cpp` | 1073-1080 (需新建) |
| UserDict 6 按钮真实现 | `c6fec000` | `WeaselServer/UserDictionary.cpp` | 1382-1480 (看 v0.19.0.28 初版) |
| UserDict 几何恢复 600 高 | `c6fec000` | `WeaselServer/UserDictionary.cpp` | kDialogH=600 (commit diff) |
| IME 关联 + 圆角 region | — | `WeaselServer/PhrasesDialog.cpp` | 166-171 (删 SetWindowRgn) + 新增 ImmAssociateContextEx |

---

## 修复方案排序

### P0 — 必修 (破坏功能, 用户无法用)
| # | bug | 修法 | 文件:行 |
|---|---|---|---|
| P0-1 | UserDict 遮挡 | `kDialogH 480 → 600`, 重写几何公式 | `UserDictionary.cpp:83` |
| P0-2 | UserDict click 消失 | `WM_KILLFOCUS` 加子控件白名单 (跟 PhrasesDialog 对齐) | `UserDictionary.cpp:1073-1080` |
| P0-3 | UserDict Import/Export 占位 | 真实现 file picker + parse/write custom_phrase.txt | `UserDictionary.cpp:1530-1536` |
| P0-4 | UserDict Add/Del 不写盘 | `ScheduleSave` → `FlushSave` 立即写 | `UserDictionary.cpp:1525, 1518` |
| P0-5 | Phrase input 中文 IME | `ImmAssociateContextEx` + 删 SetWindowRgn | `PhrasesDialog.cpp:166-171, 439-447` |
| P0-6 | Phrase 分类缺失 | 加回 `Phrase.category` + TreeView (s_hTree) | `PhrasesDialog.h:34-36, .cpp:744-756` |
| P0-7 | Phrase 默认选第一条 | `ListView_SetItemState(... 0, LVIS_SELECTED|LVIS_FOCUSED)` | `PhrasesDialog.cpp:515-520` |
| P0-8 | Phrase ←/→/回车/↑↓ 键盘 | 加 `VK_UP/DOWN/LEFT/RIGHT/RETURN` 处理 | `PhrasesDialog.cpp:575-622` |

### P1 — 应该修 (UX polish)
| # | 修法 | 文件 |
|---|---|---|
| P1-1 | Phrase 折叠状态记忆 (`m_expanded` map) 恢复 | PhrasesDialog.h |
| P1-2 | UserDict status bar 单独占行 (不跟 buttons 重叠) | UserDictionary.cpp:1109-1115 |
| P1-3 | ModalChrome 抽公共 `SetRoundedChildRegion` helper (避免 SetWindowRgn 切 IME/candidate) | ModalChrome.cpp + WeaselServerApp.cpp |
| P1-4 | font fallback 加 Microsoft YaHei UI | UserDictionary.cpp:1117-1120 + PhrasesDialog.cpp:430-433 |

### P2 — Phase C 后续 polish
| # | 修法 |
|---|---|
| P2-1 | 抽 ModalChrome 共用 child-region manager (3 modal 都用) |
| P2-2 | Drag-any-area 优化 (避免 L94 dragThreshold 限制) |
| P2-3 | 真实 user-flow e2e 测试 (T_Add/T_Delete/T_AltSlash/T_QP_*) |

---

## 关键 file:line 索引 (供 Phase C 修复 agent 用)

**`F:\soft\00selfmade\rime_claude\WeaselServer\UserDictionary.cpp`:**
- 82-83: `kDialogW=760 / kDialogH=480` (Axis 1 fix)
- 94: `kDlgRadius=14` (Axis 1 polish)
- 196-202: `exStyle` 缺 `WS_EX_LAYERED` (Phase B 修过, 但 SetWindowRgn 还在)
- 241-245: `SetWindowRgn` (Axis 1 + Axis 3 polish — 切 child region)
- 1073-1080: `WM_KILLFOCUS` (Axis 2 必修 — 缺子控件白名单)
- 1101-1238: `OnCreate` 物理几何 (Axis 1 必修)
- 1117-1120: font creation (Axis 3 P1-4 fallback)
- 1382-1480: `OnCommand` 6 按钮 handler (Axis 2 必修 — Import/Export 占位)
- 1515-1551: 6 button case (Axis 2 真实现)

**`F:\soft\00selfmade\rime_claude\WeaselServer\PhrasesDialog.cpp`:**
- 34-36: `Phrase struct` 缺 `category` (Axis 4 必修)
- 50-51: `kDialogW=360 / kDialogH=460` (无 Axis 1 问题, 不需修)
- 119-126: `exStyle` (Axis 3 fix 候选 — 加 WS_EX_LAYERED 可恢复 IME composition)
- 166-171: `SetWindowRgn` (Axis 3 必修 — 切 IME composition window)
- 226-290: `LoadPhrases` (Axis 4 加回 category 读取)
- 292-334: `SavePhrases` (Axis 4 加回 category 写入)
- 422-523: `OnCreate` 缺默认选中 (Axis 4 P0-7)
- 439-447: `s_hInput` CreateWindowExW 缺 IME 关联 (Axis 3 P0-5)
- 575-622: `OnKeyDown` 缺 ←/→/↑↓ (Axis 4 P0-8)
- 744-756: `PopulateListImpl` 单列 ListView (Axis 4 P0-6 — 改 TreeView)

**`F:\soft\00selfmade\rime_claude\WeaselServer\ModalChrome.cpp`:**
- 32-42: `PaintBorder` 圆角自绘 (P2 抽公共 helper 候选)
- 全文 51 行 (很薄, 无 SetWindowRgn)

**`F:\soft\00selfmade\rime_claude\WeaselServer\PhrasesDialog.h`:**
- 34-36: Phrase struct (Axis 4 P0-6)

---

## 总诊断 (TL;DR)

v0.19.0.32 (cd6f61a9) **UX redo 是过度简化**,把 v0.19.0.25 (3f2c1694) 引入的 13 个核心功能 (TreeView / category / 键盘导航 / 默认选 / 折叠状态 / 折叠按钮) 一次性删掉,换来"无 2-EDIT 堆叠 bug"的简化 UI。但 user flow 退回到 v0.19.0.25 之前的 bare-bones 状态,5 个 sub-bug 都是这次 UX redo 的副作用。

UserDictionary 的 2 bug 来自 v0.19.0.28 创建时的几何公式漏算 status bar(28+38+326+24+32=448 > 480),加 v0.19.0.30 KILLFOCUS 修复**漏继承**给 UserDict。

Phase C 必修 8 项 (P0 列表) 总估时 ~600 LOC + 200 LOC test,**老 git commit (c6fec000) 是直接参考蓝本**。

— END OF REPORT —