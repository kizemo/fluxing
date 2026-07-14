# 常用短语 Modal v2 — design spec

> **Status**: design ready for review (代码侧未动)
> **日期**: 2026-07-13
> **关联**: spec 042 v0.19.0.25 / v0.19.0.26 已 ship + user 反馈 L95 (外观简陋 / Add-Edit 关闭)
> **设计目标**: 在保留 v0.19.0.26 核心功能(YAML / SendInput / 树形 / Alt+. / 短语按钮)的前提下,
> 统一视觉语言(Liquid Glass chrome),并修复子 dialog 关闭 + 视觉消失两个 user-facing 问题。
> **本 spec 范围**: 设计稿 + 决策 + token 增量 + 测试矩阵。**不写代码,不改 cpp/h**。

---

## 1. Goals / Non-Goals

### 1.1 Goals

1. **视觉统一**: PhrasesDialog 跟 QuickPanel 共用 Liquid Glass chrome(WS_EX_LAYERED + per-pixel alpha + SetWindowRgn radius.lg=14)。第一眼看上去是"Fluxing 风格",而不是 Windows 原生 dialog。
2. **修复 v0.19.0.26 user-facing bug**:
   - **外观简陋**(crude look)→ 全自定义 title bar(去除 WS_CAPTION)、custom hairline 边框、Segoe UI Variable 14px title、`color.bg.panel.top → bot` 浅冷蓝渐变(继承 QuickPanel L87-fix)。
   - **Add/Edit 子 dialog 关闭 + parent 视觉消失** → 改用 **inline edit**(选定 phrase → 文本字段就地编辑),无需禁用 parent。备选:独立 top-level 子窗口(不调 `EnableWindow(parent, FALSE)`)。
3. **完整 keyboard-only UX**: 树形 ←/→ 展开折叠、↑↓ 移动、Enter 注入、Esc 关闭、F2 编辑、Delete 删除、Ctrl+N 新建、Ctrl+E 编辑、Ctrl+F 搜索、Ctrl+Shift+N 新分类。
4. **搜索 / 过滤**: 顶部 search box,实时过滤 tree,不匹配的分类 / phrase 灰显(而非隐藏),保留 hierarchy。
5. **分类管理**: 右键分类行 → context menu(Rename / Delete / New sub-category)。
6. **持久化实时**: 任何 add/edit/delete 500ms debounce 后写盘(YAML);失败保留 in-memory + toast 警告。
7. **错误处理**: YAML 解析失败 → 空 list + 顶部 status bar warning;磁盘满 / 权限失败 → toast(不丢 in-memory)。

### 1.2 Non-Goals

1. 不重做 QuickPanel / 不改 QuickPanel 视觉 token(只消费 `panel_glass_top/bot`)。
2. 不引入 GDI+ / Direct2D 到 PhrasesDialog(沿用 GDI + SetLayeredWindowAttributes)。
3. 不做 multi-select / drag-drop reorder / per-phrase icon(YAGNI)。
4. 不做 iCloud / sync / 多用户 YAML。
5. 不做 tree 控件替代品(继续用 SysTreeView32,节省 ~400 行)。
6. 不引入 yaml-cpp 依赖(沿用 v0.19.0.25 极简 parser)。

---

## 2. UI 草图

### 2.1 整体外观(mac Liquid Glass 风格)

```
┌─ 常用短语 ──────────────────────── ·  ·  ·  ┐ ← 30px title bar (渐变 + 自绘 close ×)
│                                                    │
│  ┌─ 🔍 [ 搜索短语...            ] ──┐    ← 32px search box (radius.md=10)
│  └────────────────────────────────────┘
│                                                    │
│  ▼ 📁 工作                              (3)   ← 分类行:▼/▶ + 图标 + 名 + count
│      你好                                   │
│      谢谢                                   │
│      期待合作                          ★ ← 选中行 (kSelBg 浅橙底)
│  ▶ 📁 日常                              (1)   ← 折叠分类:▶ + 名 + count
│  ▼ 📁 (未分类)                          (2)   │
│      Hello                                  │
│      测试短语                               │
│                                                    │
│  ─────────────────────────────────────   ← hairline 分隔 (1px, kBorderColor)
│  [+ 添加]  [✎ 编辑]  [− 删除]  [取消] [搜索/...]    ← 32px button row
└────────────────────────────────────────────────────┘
       360 × 460 px (size.modal.dialog.w/h 调整: 420→460, 增 40px 给 search box)
       圆角 radius.lg = 14 (kDlgRadius = 14, 2r=28 GDI ellipse)
       外描 hairline 1px @ color.modal.border (kBorderColor)
```

### 2.2 Search-active 状态

```
┌─ 常用短语 ──────────────────────── ·  ·  ·  ┐
│                                                    │
│  ┌─ 🔍 [ 工作                       ] ──┐
│  └────────────────────────────────────┘
│                                                    │
│  ▼ 📁 工作                              (3)   ← 匹配,正常显示
│      你好                                   │   ← 匹配,正常
│      谢谢                                   │   ← 匹配,正常
│      期待合作                              │   ← 不匹配 → dim 50% (TVIS_CUT)
│  ▶ 📁 日常                              (1)   ← 不匹配分类 → dim 50%,折叠保留
│  ▼ 📁 (未分类)                          (0)   ← 空分类 (不匹配的 phrases 灰显)
│      (空)                                     │
│                                                    │
│  ─────────────────────────────────────
│  [+ 添加]  [✎ 编辑]  [− 删除]  [取消]
└────────────────────────────────────────────────────┘
```

### 2.3 Inline edit 状态(取代 v0.19.0.25 子 dialog)

```
┌─ 常用短语 ──────────────────────── ·  ·  ·  ┐
│                                                    │
│  ┌─ 🔍 [ 搜索短语...            ] ──┐
│  └────────────────────────────────────┘
│                                                    │
│  ▼ 📁 工作                              (3)   │
│      你好                                   │
│      ┌──────────────────────────────────┐   │
│      │ 谢谢_                             │   ← F2 / 双击: 文本就地编辑
│      └──────────────────────────────────┘      │   ES_AUTOHSCROLL + 焦点 + 全选
│      期待合作                              │   │
│  ▶ 📁 日常                              (1)   │   Enter 保存
│  ▼ 📁 (未分类)                          (2)   │   Esc 取消
│      Hello                                  │
│      测试短语                               │
│                                                    │
│  ─────────────────────────────────────
│  [保存]  [取消]                                  ← 按钮在 edit 期间变成 Save/Cancel
└────────────────────────────────────────────────────┘
```

> **决策**: inline edit 优先。如果 user 测试反馈 inline 不直观(光标移动时被 tree 抢占焦点),备选 §6 决策表 D-Edit-2 (独立 top-level dialog 不禁 parent)。

### 2.4 Add new phrase(选分类 + inline)

```
┌─ 常用短语 ──────────────────────── ·  ·  ·  ┐
│                                                    │
│  Ctrl+N 后:                                        │
│                                                    │
│  ▼ 📁 工作                              (3)   │
│      你好                                   │
│      ┌──────────────────────────────────┐   │
│      │ (新短语)|                         │   ← placeholder "(新短语)"
│      └──────────────────────────────────┘      │   按 ↓ 立刻可改 category
│      期待合作                              │   │
│      [category: 工作 ▾]                       │   ← 分类下拉(可输入新名,autocomplete)
│  ▶ 📁 日常                              (1)   │
│                                                    │
│  ─────────────────────────────────────
│  [保存]  [取消]
└────────────────────────────────────────────────────┘
```

### 2.5 Context menu(右键分类行)

```
┌────────────────────────┐
│ 重命名              F2 │   ← 触发 inline edit category 名
│ ───────────────────────│
│ 新建短语         Ctrl+N│
│ 新建子分类  Ctrl+Shift+N│
│ ───────────────────────│
│ 删除分类         Del   │   ← 二次确认: "分类 '工作' 内有 3 条短语,确定删除?(短语将变为未分类)"
└────────────────────────┘
```

### 2.6 Status bar(顶部 warning)

```
┌─ ⚠ phrases.yaml 解析失败,已加载空列表 ─── ✕ ┐   ← kStatusBarH = 24, error 时显示
│                                                       黄色底 (kWarnBg = RGB(255, 244, 220))
│  (正常时不渲染这一行)
```

---

## 3. State machine

```
                ┌──────────────────┐
                │   Hidden         │ ← 初始 / Esc / Cancel / Enter 注入后
                │   (s_hwnd=null)  │
                └─────┬────────────┘
                      │ Show() / Alt+. / Phrase button click
                      ▼
                ┌──────────────────┐
                │   Loading        │ ← LoadPhrases() 同步读 YAML (≤ 50ms)
                └─────┬────────────┘
                      │ OK
                      ▼
                ┌──────────────────┐
        ┌──────▶│   Browsing       │ ← 默认状态: tree focus,↑↓/←→/Enter/Esc
        │       │   m_search==""   │
        │       └─┬───────┬────────┘
        │         │       │
        │ Ctrl+F  │       │ Ctrl+N / F2 / 双击 / 点 Edit
        │         ▼       ▼
        │       ┌─────┐ ┌──────────────┐
        │       │Search│ │   Editing    │ ← inline edit current phrase
        │       │active│ │  m_search!="" │
        │       └──┬──┘ └──────┬───────┘
        │  Esc     │           │ Enter → Save
        │          │           │ Esc  → Cancel
        │          ▼           ▼
        │       ┌──────────────────┐
        └───────│   Browsing       │   (回到 Browsing,SavePhrases debounced 500ms)
                └──────────────────┘
```

| State | entry trigger | exit trigger | 备注 |
|---|---|---|---|
| `Hidden` | Hide() / Enter inject | Show() | s_hwnd=null |
| `Loading` | Show() | Load 结束 | ≤50ms,UI 暂不显示;失败 → Browsing + warn bar |
| `Browsing` | Loading OK / Esc / Edit cancel | Ctrl+F / Ctrl+N / F2 / 双击 / 点按钮 | 默认状态 |
| `Search` | Ctrl+F | Esc / 点 × | 实时过滤 tree,dim 不匹配项 |
| `Editing` | Ctrl+N / F2 / 双击 / Edit 按钮 | Enter / Esc / 点按钮 | inline edit,Save 后立即 PopulateTree + debounced save |

---

## 4. 键盘绑定(全量)

| 按键 | 状态 | 行为 |
|---|---|---|
| `↑` | Browsing | 移到上一个**可见**节点(category + visible phrase) |
| `↓` | Browsing | 移到下一个**可见**节点 |
| `←` | 选中 category, 展开 | 折叠 |
| `←` | 选中 category, 折叠 | no-op |
| `←` | 选中 phrase | 跳到所属 parent category |
| `→` | 选中 category, 折叠 | 展开 |
| `→` | 选中 category, 已展开 | 跳到第一个 child phrase |
| `→` | 选中 phrase | no-op |
| `Enter` | Browsing + 选中 phrase | InjectText + Hide() |
| `Enter` | Browsing + 选中 category | toggle 折叠 |
| `Enter` | Editing | Save + 退出 Editing |
| `Esc` | Browsing | Hide() |
| `Esc` | Search | 清空 search → Browsing |
| `Esc` | Editing | Cancel + 退出 Editing |
| `Tab` | Browsing | 在 button 行 (Add/Edit/Del/Cancel) 间循环 |
| `Tab` | Editing | text → category → Save → Cancel |
| `Shift+Tab` | 同 Tab 反向 |
| `F2` | Browsing + 选中 phrase | 进 Editing(全选 text) |
| `F2` | Browsing + 选中 category | Rename category(inline) |
| `Delete` | Browsing + 选中 phrase | 删除当前 phrase(无确认,保留 undo:50 步? → 后续 spec) |
| `Delete` | Browsing + 选中 category | 删除分类(二次确认,短语降级到未分类) |
| `Ctrl+N` | Browsing | 在当前选中分类下新增 phrase → Editing 状态 |
| `Ctrl+E` | Browsing + 选中 phrase | 进 Editing |
| `Ctrl+F` | Browsing | 焦点到 search box,进 Search 状态 |
| `Ctrl+Shift+N` | Browsing + 选中 category | 新建子分类 → inline edit 名 |
| `Ctrl+S` | Browsing / Editing | 立即 Save(跳过 debounce) |
| `Ctrl+Z` | Browsing | undo 上次 add/edit/delete(50 步 ring buffer,YAGNI v1,占位) |

---

## 5. 数据流(Load YAML → Tree → Edit → Save YAML)

```
[App 启动 / 用户点 Phrase 按钮 / Alt+.]
       │
       ▼
PhrasesDialog::Show()
       │
       ├─ 1. LoadPhrases(yaml_path, m_phrases)
       │     ├─ 读 UTF-8+BOM → wstring
       │     ├─ 极简 parser(- text / category:)
       │     ├─ 失败 → m_phrases.clear() + status bar "⚠ 解析失败"
       │     └─ OK → m_phrases 填充
       │
       ├─ 2. CreateWindowExW(WS_EX_LAYERED, WS_POPUP, no caption)
       │     ├─ SetLayeredWindowAttributes(LWA_ALPHA=255) — uniform
       │     ├─ SetWindowRgn(round-rect radius.lg=14)
       │     └─ SetWindowPos(TOPMOST + center)
       │
       ├─ 3. OnCreate:
       │     ├─ CreateFontW(Segoe UI Variable, 14px) → s_hFontUi
       │     ├─ CreateWindowEx search box (EDIT, 32px h)
       │     ├─ CreateWindowEx SysTreeView32 (TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS)
       │     ├─ CreateWindowEx 4 buttons (Add/Edit/Del/Cancel, 32px h, radius.xs=6)
       │     └─ PopulateTree() → 默认选中第一条 phrase
       │
       ▼
[Browsing 状态]
       │
       │ Ctrl+N / F2 / 双击 phrase
       ▼
[Editing 状态]
       │
       ├─ SetFocus(hTextEdit)
       ├─ SetWindowText 预填 (F2:全选;Ctrl+N:空+placeholder)
       ├─ category 控件 (ComboBox,ES_AUTOHSCROLL + autocomplete from m_phrases cats)
       ├─ 用户编辑 → Enter → 保存:
       │     ├─ m_phrases.push_back(Phrase{text,cat}) 或 update idx
       │     ├─ PopulateTree() (rebuild)
       │     ├─ 选中刚编辑的 phrase (re-find by text+cat)
       │     └─ ScheduleSave() — SetTimer(IDT_SAVE, 500ms, one-shot)
       └─ 用户编辑 → Esc → 取消:
             └─ PopulateTree() 恢复原状
       │
       │ 500ms timer 触发 SavePhrases()
       ▼
SavePhrases(yaml_path, m_phrases)
       ├─ UTF-8 + BOM 写盘
       ├─ 失败 → toast(右下角 3s)+ 保留 in-memory + 重试 1 次(2s 后)
       └─ 成功 → 静默(不打扰用户)

       │
       │ Enter inject / Esc / Cancel / 点击外(grace 2s)
       ▼
Hide() → DestroyWindow(s_hwnd) → DeleteObject(s_hFontUi)
       │
       ▼
s_hwnd=null → 下次 Show() 走 Loading 路径
```

---

## 6. 关键决策表(with rationale)

| ID | 决策点 | 选择 | 备选 | 理由 |
|---|---|---|---|---|
| **D-Chrome-1** | 整体视觉语言 | mac Liquid Glass (WS_EX_LAYERED + per-pixel alpha + SetWindowRgn radius.lg=14) | 维持 v0.19.0.25 原生 dialog | user "外观简陋"反馈;跟 QuickPanel 一套 |
| **D-Chrome-2** | 圆角半径 | `radius.lg = 14` (kDlgRadius=14, 2r=28 GDI ellipse) | 8 / 10 / 20 | 跟 FLUENT-UI-TOKENS §3.4 `radius.lg=14` (panel 标准) |
| **D-Chrome-3** | 边框 | hairline 1px @ `color.modal.border = RGB(217,217,217)` (8% 黑) | 无 / 2px | 跟 v3-macos `box-shadow 0 0 0 0.5px` 等价;mac HIG 推荐 hairline |
| **D-Chrome-4** | Title bar 高度 | 30px (kTitleH,继承 v0.19.0.25) | 36 / 24 | mac 标题栏节奏,够放 14px 文字 + 上下 8px padding |
| **D-Chrome-5** | Title bar 文字 | 自绘 "常用短语" + `color.modal.text = RGB(30,30,40)` + DT_CENTER\|DT_VCENTER | 系统 title | user 反馈"简陋"主因之一,自绘 title = 现代感 |
| **D-Chrome-6** | Title bar 关闭按钮 | 自绘 `×` (右上角, 16px) | 系统 menu | 配 WS_CAPTION 移除 + 自绘,需要配套 |
| **D-Search-1** | 搜索框位置 | title bar 下方、tree 上方,32px 高 | 嵌入 title bar 内 / 右下角浮窗 | HIG search field 标准位置,不影响 tree |
| **D-Search-2** | 搜索过滤算法 | 实时过滤, 不匹配项 dim 50% (TVIS_CUT), 保留 hierarchy | 完全隐藏不匹配 | dim 比 hide 友好(用户保持空间感,知道哪些分类存在) |
| **D-Search-3** | 搜索匹配规则 | 大小写不敏感, 子串匹配 text + category 名 | 正则 / 拼音 / fuzzy | YAGNI v1;子串覆盖 95% 场景 |
| **D-Edit-1** | Add/Edit 子 UI | **inline edit**:选定 phrase 行 → 该行变成 EDIT 控件(ES_AUTOHSCROLL + WS_BORDER) | 独立子 dialog (v0.19.0.25) | 修 v0.19.0.26 user bug: parent EnableWindow(FALSE) 让 PhrasesDialog 视觉"消失"。inline 不需要 dialog |
| **D-Edit-2** | Add/Edit 备选方案 | 独立 top-level dialog,**不调** `EnableWindow(parent, FALSE)` | inline edit (D-Edit-1) | D-Edit-1 失败时的 fallback,spec §10 risk 行 |
| **D-Edit-3** | category 控件 | ComboBox (CBS_DROPDOWN + autocomplete) | 纯 EDIT | 让用户从已有分类选,避免拼写错;同时可输入新分类 |
| **D-Edit-4** | 编辑焦点切换 | Tab: text → category → Save → Cancel | 鼠标点 | 键盘可达 |
| **D-Save-1** | 写盘时机 | 500ms debounce (`SetTimer` one-shot,IDT_SAVE) | 立即写 / 失焦写 | 防连续 edit 触发多次 I/O;debounce 不影响体验 |
| **D-Save-2** | 写盘失败处理 | toast 右下角 3s + 保留 in-memory + 2s 后重试 1 次 | 立即报错 / 弹 MessageBox | MessageBox 模态打断 UX,toast 更柔和;in-memory 保留避免用户丢数据 |
| **D-Save-3** | YAML 解析失败 | status bar 顶部 24px 高警告条,kWarnBg 浅黄底 | 弹 MessageBox / 静默 | 跟 QuickPanel L87-fix 模式一致,non-blocking |
| **D-Hotkey-1** | 全局 hotkey Alt+. | 沿用 v0.19.0.25 RegisterHotKey(MOD_ALT, VK_OEM_PERIOD) | 改成 Ctrl+. | user v0.19.0.25 spec 钦定,不破 |
| **D-Hotkey-2** | 内部快捷键 | Ctrl+N / Ctrl+E / Ctrl+F / F2 / Delete / Esc / Enter (跟 macOS 标准一致) | 自定义组合 | 符合 macOS HIG keyboard shortcut 习惯 |
| **D-CtxMenu-1** | 右键菜单 | WM_CONTEXTMENU + TrackPopupMenu, 自绘 MenuItem | 系统 menu | 自绘 = 视觉一致;但 v1 可先用系统 menu 减代码,YAGNI |
| **D-Resize-1** | 窗口尺寸 | 固定 360 × 460 (kDialogW=360, kDialogH=460) | 可拖拽改变大小 | YAGNI,modal 不需要 resize |
| **D-DPI-1** | DPI 适配 | 物理像素 = 逻辑像素 × `s_dpr` (OnCreate 算, OnPaint 用) | 完全不缩放 | 跟 QuickPanel L83-fix pattern 一致 |

---

## 7. Token 增量(必须先入 token 表再写代码)

> 路径: `docs/design/FLUENT-UI-TOKENS.md` §3.6 PhrasesDialog 块追加,§3.1/§3.3/§3.4/§3.5 视情况加。

### 7.1 新增颜色 token

| Token | Value | 来源 | 备注 |
|---|---|---|---|
| `color.modal.bg.top` | `RGB(245, 245, 248)` (kBgTop,沿用 v0.19.0.25) | WE-PICK | §3.6 已存在 |
| `color.modal.bg.bot` | `RGB(220, 222, 230)` | WE-PICK | §3.6 已存在 |
| `color.modal.text` | `RGB(30, 30, 40)` | WE-PICK | §3.6 已存在 |
| `color.modal.sel_bg` | `RGB(255, 235, 220)` (浅橙底) | WE-PICK | §3.6 已存在 |
| `color.modal.border` | `RGB(217, 217, 217)` (8% 黑 hairline) | WE-PICK | **新增** |
| `color.modal.warn_bg` | `RGB(255, 244, 220)` (status bar 浅黄) | WE-PICK | **新增** |
| `color.modal.warn_text` | `RGB(120, 80, 30)` (status bar 棕字) | WE-PICK | **新增** |
| `color.modal.search_bg` | `RGB(255, 255, 255)` (search box 白底) | WE-PICK | **新增** |
| `color.modal.search_border` | `RGB(220, 220, 225)` | WE-PICK | **新增** |
| `color.modal.edit_bg` | `RGB(255, 252, 240)` (inline edit 高亮) | WE-PICK | **新增** |
| `color.modal.button_bg` | `RGB(245, 245, 248)` | WE-PICK | **新增**,跟 title bar 同色 |
| `color.modal.button_bg.hover` | `RGB(230, 232, 240)` | WE-PICK | **新增** |
| `color.modal.button_bg.pressed` | `RGB(255, 235, 220)` (浅橙底) | WE-PICK | **新增**,跟 sel_bg 同色 |

### 7.2 新增尺寸 / 间距 token

| Token | Value (px) | 来源 | 备注 |
|---|---|---|---|
| `size.modal.dialog.w` | `360` (沿用) | WE-PICK | §3.6 已存在 |
| `size.modal.dialog.h` | `460` (420→460, +40 给 search) | WE-PICK | **修改** |
| `size.modal.search.h` | `32` | WE-PICK | **新增** |
| `size.modal.status.h` | `24` (warn bar) | WE-PICK | **新增** |
| `size.modal.title.h` | `30` (沿用) | WE-PICK | §3.6 已存在 |
| `size.modal.btn.h` | `32` (沿用) | WE-PICK | §3.6 已存在 |
| `size.modal.btn.w` | `76` (沿用) | WE-PICK | §3.6 已存在 |
| `size.modal.btn.radius` | `6` (radius.xs 等价, button 圆角) | WE-PICK | **新增** |
| `space.modal.gap` | `8` (沿用) | WE-PICK | §3.6 已存在 |
| `space.modal.btn.gap` | `8` (沿用) | WE-PICK | §3.6 已存在 |
| `space.modal.search.margin_x` | `12` | WE-PICK | **新增** |

### 7.3 字体 token(沿用 §3.2)

| Token | Value | 备注 |
|---|---|---|
| `font.ui.size.body` | `14 px` | tree / button / search box / category combo |
| `font.ui.size.label` | `14 px` (跟 body 同,因 modal 节奏小) | title "常用短语" |
| `font.ui.face` | `Segoe UI Variable` / fallback `Microsoft YaHei UI` | CJK fallback |

### 7.4 Radius token(沿用 §3.4)

| Token | Value | 备注 |
|---|---|---|
| `radius.lg` | `14` | panel / search box / dialog |
| `radius.xs` | `6` (新增到 §3.4) | button |

### 7.5 时间 token

| Token | Value (ms) | 备注 |
|---|---|---|
| `time.save.debounce_ms` | `500` | **新增**,debounce save 写盘 |
| `time.save.retry_ms` | `2000` | **新增**,失败重试 1 次延迟 |
| `time.toast.show_ms` | `3000` | **新增**,toast 显示时长 |
| `time.grace.show_ms` | `2000` (沿用 v0.19.0.26) | 失焦 grace |

---

## 8. 文件改动清单(file:line 估算)

> **本 spec 不实现,只估算**,留待 implementation spec (v0.19.0.27 或新 spec 044)。

| 文件 | 内容 | 行数估算 | 风险 |
|---|---|---|---|
| `WeaselServer/PhrasesDialog.h` | 新增 search box HWND / inline edit HWND / status bar HWND; 新增 state enum; 新增 SaveTimer 句柄 | +40 行 | 低(纯声明) |
| `WeaselServer/PhrasesDialog.cpp` | 1) 自绘 title bar (`OnPaint` 加 close ×); 2) Create search box; 3) Create status bar; 4) Inline edit Enter/Esc; 5) Search 过滤 (dim via `TVIS_CUT`); 6) Debounce save (`SetTimer`); 7) Toast 通知 (新 `ShowToast` helper); 8) 移除 v0.19.0.25 `ShowEditDialog` modal 子 dialog 调用; 9) 改用 inline 或独立非禁 parent dialog (备选) | ~+200 行 | 中(交互多) |
| `WeaselServer/PhrasesDialog.cpp` (移除) | 删除 `ShowEditDialog()` + `EditDlgProc()` (v0.19.0.25) | ~-130 行 | 低 |
| `WeaselServer/PhrasesDialog.cpp` (颜色常量) | 把 `kBgTop/kBgBot/kTextColor/kSelBg/kBorderColor` 改引用 `FluxingTheme` 或命名空间 `fluxing::tokens::modal::*` | 重构 20 行 | 中(需 IPC / 直接 include? 见 risk) |
| `WeaselServer/PhrasesDialog.cpp` (新增) | `ScheduleSave()` / `FlushSave()` / `ShowToast()` / `ApplySearchFilter()` | +80 行 | 低 |
| `docs/design/FLUENT-UI-TOKENS.md` | §3.6 追加新 token(`color.modal.border` 等 9 个颜色 + 6 个尺寸 + 1 个 radius + 3 个 time) | +60 行 | 低 |
| `test/TestPhrasesDialog/TestPhrasesDialog.cpp` | 加 search filter / inline edit / debounce save / toast / status bar 用例 | +150 行 | 低 |
| `CHANGELOG.md` | v0.19.0.27 条目:PhrasesDialog v2 (Liquid Glass + search + inline edit) | +30 行 | 低 |
| `.specify/memory/lessons-learned.md` | L96 条目 (本 spec 派生 issues) | +50 行 | 低 |
| `.specify/specs/043-phrases-ui-v2/` | `spec.md` / `tasks.md` / `plan.md` 派生 | +800 行 | 低 |

**总计**: 1 个 cpp 改 ~+200/-130 = 净 +70 行 cpp;test +150 行;token doc +60 行。

---

## 9. 测试矩阵

### 9.1 Unit 测试(`test/TestPhrasesDialog/`)

| 用例 | 输入 | 期望 |
|---|---|---|
| `LoadPhrases_normal` | 7 phrases / 3 cats (working / daily / "") | m_phrases.size()==7; m_cats=={工作,日常,(uncat)} |
| `LoadPhrases_invalid` | 损坏的 YAML | 返回 false; m_phrases.clear() |
| `LoadPhrases_utf8_bom` | 带 BOM 的 YAML | 正确解析 |
| `LoadPhrases_chinese` | category="工作" text="谢谢" | 正确 wide-char 解析 |
| `SavePhrases_roundtrip` | 7 phrases → file → 重新 Load | size/text/category 全部一致 |
| `SavePhrases_special_chars` | text 含 `"` / `\` / `\n` | 正确转义 + Load 还原 |
| `PopulateTree_5p_2c` | 5 phrases / 2 cats | inserted == 7 (2 cat + 5 phrase) |
| `SearchFilter_dim_only` | search="你" → 2 match / 5 total | 匹配项正常;不匹配项 TVIS_CUT dim 50% |
| `SearchFilter_chinese` | search="工作" | 只 work 分类正常,其他 dim |
| `SearchFilter_clear` | search="" | 全部正常,无 dim |
| `InlineEdit_text` | F2 触发 → 改 text → Enter | m_phrases[idx].text 更新;SavePhrases 写盘 |
| `InlineEdit_category` | 改 category=新名 → Enter | m_phrases[idx].category 更新;PopulateTree 重建 |
| `InlineEdit_empty_text` | text="" → Enter | 不保存,弹出"text 不能为空"提示(或 inline 高亮红框) |
| `DebounceSave_500ms` | edit 后立即读 yaml | t<500ms 仍为旧值;t=600ms 后是新值 |
| `DebounceSave_coalesce` | 1s 内 5 次 edit | 只写盘 1 次(第 1 次后 timer reset) |
| `GraceGuard_2s` | Show() 后 1s 内失焦 | 不 Hide;t=2.5s 失焦 Hide (沿用 v0.19.0.26) |
| `GraceGuard_windowless` | Show() 后 0.5s 切到 en-US IME | 不 Hide |
| `InjectText_unicode` | text="你好" | SendInput mock 收到 2 unicode codepoints (U+4F60, U+597D) |
| `InjectText_empty` | text="" | 0 inputs,不调 SendInput |
| `Toast_disk_full_mock` | 模拟 write 失败 | toast 显示 + in-memory 保留 + 2s 后重试 1 次 |

### 9.2 Sandbox / Reality 测试

| 场景 | 步骤 | 期望 |
|---|---|---|
| **观感** | 启动 WeaselServer, 触发 PhrasesDialog (Alt+. / Phrase 按钮) | 圆角 14px, hairline 边框, 自绘 title, 浅冷蓝渐变, Segoe UI 14px 文字 |
| **树形 UX** | ↑↓ 切换 phrase, ← 折叠分类, → 展开分类, Enter 注入 | 全部 work,无 focus 丢失 |
| **搜索** | Ctrl+F → 输入 "你" | 树 dim 不匹配项,匹配项正常;Esc 清空 |
| **inline edit** | F2 → 改 "你好" → "您好" → Enter | m_phrases 更新; tree 显示新文本;500ms 后 yaml 落盘 |
| **新增** | Ctrl+N → 输入 text + category → Enter | m_phrases.push_back; PopulateTree;debounce save |
| **删除** | 选 phrase → Delete | m_phrases.erase; PopulateTree;debounce save |
| **失败** | chmod 0444 phrases.yaml → 编辑 | toast 显示 "保存失败,已重试";in-memory 仍新 |
| **跨 DPI** | 100% / 150% / 200% DPI 切换 | 视觉一致,文字不糊 |
| **dark mode** | 切 Win11 dark | dialog 不参与(panel 自绘 bg, 不依赖 system theme)— 决策: 维持 light chrome(沿用 v0.19.0.25)|
| **失焦关闭** | Show 后立刻 Alt+Tab | grace 2s 内不关;t=2.5s 时关 |
| **Alt+. 冲突** | 别的 app 占 Alt+. | RegisterHotKey 失败, log warning,不 crash |
| **持久化** | 编辑 → 关闭 WeaselServer → 重启 | m_phrases 从 yaml 恢复 |
| **YAML 损坏** | 手动写错 yaml → 重启 WeaselServer | 空 list + status bar "⚠ 解析失败" |

---

## 10. Risk / Trade-off

| Risk | 严重度 | 缓解 |
|---|---|---|
| **inline edit 跟 tree focus 抢占** | 中 | Enter/Esc 显式退出 edit;Esc 后 SetFocus(tree) |
| **search 过滤 dim 实现用 TVIS_CUT 是否 win11 兼容** | 低 | TVIS_CUT 自 Win2000 存在,无兼容问题 |
| **debounce save timer 在 Hide() 时未 flush** | 中 | Hide() 前调 `KillTimer + FlushSave()`,保证数据落盘 |
| **toast 实现复杂度 (需要单独 HWND, 计时器, fade)** | 中 | v1 用 static `HWND s_hToast` + SetTimer(IDT_TOAST, 3000, one-shot),简化;spec §11 anti-pattern AP-L96-A |
| **FluxingTheme 依赖 D2D, PhrasesDialog 不引 D2D** | 中 | 颜色常量走 namespace `fluxing::tokens::modal::*` (D2D-free), 不 include FluxingTheme.h;遵循 spec 040 §4 Phase 2 风险行 |
| **右键菜单自绘** | 低 | v1 用 `TrackPopupMenu` 系统菜单 + 自绘 owner-drawn item;YAGNI 自绘 chrome |
| **inline edit 视觉跟 tree 选中态区分** | 中 | edit 期间该行用 `color.modal.edit_bg` 浅黄底 + 1px solid border 区分 |
| **Ctrl+Z undo 50 步 ring buffer** | 中 | v1 **不实现** undo,占位;后续 spec |
| **m_phrases 在编辑期间被 Show() 重入** | 低 | Show() 走 reuse 路径(SetForegroundWindow),不重 Load;但 LoadPhrases 失败时 status bar 持久,需 reset |
| **macOS HIG "trailing close button ×" 在 modal 缺失** | 低 | 自绘 × 按钮在 title 右上,点击 = Hide();spec §2.1 已草图 |

---

## 11. Anti-patterns 新增(基于 L94 / L95 / L96 累积)

> 摘自 lessons-learned.md,本 spec 派生,新增 L96-Ap-*。

### L96-AP-A — **不要**用 `EnableWindow(parent, FALSE)` 弹子 dialog

v0.19.0.25 user 反馈:**Add/Edit 子 dialog 创建后,parent PhrasesDialog 视觉消失**。
根因:`WS_EX_DLGMODALFRAME` 子 dialog 的 modal loop 调 `EnableWindow(parent, FALSE)` → parent 不响应 WM_PAINT / WM_ACTIVATEAPP → 但 WS_EX_LAYERED 渲染依赖这些消息 → 视觉"消失"。
**修法**: 本 spec 改 inline edit (D-Edit-1), 备选独立 top-level dialog **不调** EnableWindow (D-Edit-2)。**禁止** future code 再走 v0.19.0.25 模式。

### L96-AP-B — title bar 渐变 gradient fill 涂没 button, 必须 clip

v0.19.0.26-fix(issue 2b) 教训:`OnPaint` 用 `GradientFill` 涂整个 client rect → button / tree 被涂没。
**修法**: `OnPaint` **只**涂 [0, kTitleH) 区域 (title bar); button / tree 区域用 `WS_CLIPCHILDREN` 让子控件自己画,Windows 自动 clip。沿用 v0.19.0.26 模式。

### L96-AP-C — 不要在子 dialog `while(GetMessageW)` loop 里阻塞 I/O

v0.19.0.25 `ShowEditDialog` 模态循环内 OK 按钮按下直接 `SavePhrases` (同步写盘)。在某些 anti-virus / OneDrive 同步路径下阻塞 200-500ms,UI 卡死。
**修法**: debounce 500ms (D-Save-1),OK 按钮只 push 到 in-memory + SetTimer,modal loop 立即返回。SavePhrases 在 timer 回调跑。

### L96-AP-D — 颜色常量不要硬编码, 走 token 表

v0.19.0.25 直接 `constexpr COLORREF kBgTop = RGB(245, 245, 248);` 在 cpp 头部。
**修法**: 本 spec §7 强制所有颜色先入 FLUENT-UI-TOKENS.md §3.6,代码引用 `fluxing::tokens::modal::bg_top` 命名空间常量(`WeaselUI/include/GeometryTokens.h`,spec 040 Phase 3 落地)。

### L96-AP-E — 高度 / Y 坐标必须自适应, 不要写死 magic number

v0.19.0.25 原 `kBtnY=378` 撞 tree 底 370(issue 2a)。
**修法**: 沿用 v0.19.0.26-fix 模式,所有 Y 坐标由公式 `kTitleH + kTreeH_phys + kGap` 等算出,kSearchH / kStatusBarH 改了 kDialogH 后其他自动跟着调。kSearchH 出现后,公式变 `kDialogH - kTitleH - kSearchH - kStatusBarH - kBtnH - 4*kGap`。

### L96-AP-F — 双验收 (Reality + Test) 仍强制

v0.19.0.25 缺 double-acceptance → user 反馈"功能未实现"。
**修法**: spec §9 测试矩阵 → Track 2 写完后,必须 Reality (visual) + Test (functional) **双 PASS** 才 ship。

### L96-AP-G — 视觉"简陋"是设计债, 不要用 WS_CAPTION 系统 title

v0.19.0.25 用 `WS_CAPTION | WS_SYSMENU` → 系统 title bar 跟 Liquid Glass chrome 视觉割裂。
**修法**: 本 spec D-Chrome-5/6,**不**用 WS_CAPTION,自绘 title + 自绘 close ×。沿用 v0.19.0.26-fix(issue 2c) 模式。

---

## 12. Open Questions(待 user 决策)

1. **inline edit vs 独立 top-level dialog**: D-Edit-1 vs D-Edit-2。user 倾向哪个? (default: inline edit,失败回退)
2. **dark mode 是否响应**: dialog 维持 light chrome 还是跟随 Win11 dark theme? (default: 维持 light,跟 QuickPanel 一致 — QuickPanel 也不跟随)
3. **toast 自绘**: v1 用 static HWND + GDI 还是引入新组件? (default: static HWND + GDI,YAGNI)
4. **right-click context menu**: 自绘还是系统 menu? (default: 系统 menu + owner-drawn item,YAGNI)
5. **Ctrl+Z undo 50 步**: v1 实现还是占位? (default: 占位,v2 spec)
6. **取消 button 还是 Esc 唯一**: 双路径都保留还是只保留 Esc? (default: 双路径都保留,跟 v0.19.0.25 一致)

---

## 13. 引用

- v0.19.0.25 spec: `.specify/specs/042-phrases-ui/spec.md` (348 行)
- v0.19.0.26 ship commit: `fix(WeaselServer): v0.19.0.26 - PhrasesDialog bug fix (grace + mac style + cleanup)`
- FLUENT-UI-TOKENS: `docs/design/FLUENT-UI-TOKENS.md` §3.6 (PhrasesDialog tokens)
- QuickPanel L87-fix pattern: per-pixel alpha gradient + radius.lg + hairline
- QuickPanel L94 grace guard: `kShowGraceMs=2000` + `s_showTime` GetTickCount 比对
- macOS HIG: 14px panel radius, hairline 1px @ 8% black, inline edit (Notes.app / Reminders.app)
- lessons-learned L94/L95/L96: anti-patterns 累积
- constitution P2: 禁止 TSF 阻塞 I/O → debounce save 触发
- constitution P5: 用户可见字符串中文
- constitution P4: commit 必须 `<type>(<scope>): <subject>`,scope=`WeaselServer` 或 `WeaselUI`

---

## 14. 设计稿收尾

| 维度 | 状态 |
|---|---|
| Goals / Non-Goals | ✅ |
| UI 草图 (ASCII 6 panels) | ✅ |
| State machine (5 states) | ✅ |
| 键盘绑定 (15+ combos) | ✅ |
| 数据流 (Load → Tree → Edit → Save) | ✅ |
| 关键决策 (20 条) | ✅ |
| Token 增量 (20+ 新 token) | ✅ |
| 文件改动清单 (10 行) | ✅ |
| 测试矩阵 (unit 18 + sandbox 12) | ✅ |
| Risk / Trade-off (10 行) | ✅ |
| Anti-patterns 新增 (L96-AP-A 到 G) | ✅ |
| Open Questions (6 待 user) | ✅ |

**状态**: design ready for review → 等 user 确认 §12 Open Questions 后,转 implementation spec (v0.19.0.27 或 spec 044)。

---

*Last updated: 2026-07-13 · Owner: UI Design Agent (PhrasesDialog v2) · Spec: 043-phrases-ui-v2/design.md*