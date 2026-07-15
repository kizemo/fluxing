# Feature Requests

Capabilities requested by the user.

---

## [FEAT-20260715-001] 真 binary GUI 渲染验证 流程

**Logged**: 2026-07-15T11:00:00Z
**Priority**: critical
**Status**: pending
**Area**: tests

### Requested Capability
- ship 前必须做真 binary 端到端 GUI 渲染验证 (不是 link-probe, 是真 Show 出来 GetDC + 抓 bitmap 验证非空)
- e2e test 必须 verify item 数 > 0 (TreeView_GetCount / ListView_GetItemCount) + 真 paint 验证 (GetDC + BitBlt bitmap 验证非空)

### User Context
User 5+ 轮 ship 反馈 "bug 还在", 实际跑 WeaselServer 看截图 "空 body, 完全不可用", 我方 ship 报告 174 unit + 38 e2e binary 全 PASS。Ship-loop 失败模式: 我方 ship ≠ user 实际体验。User 报 bug 优先级 > 我方静态分析。

### Complexity Estimate
medium

### Suggested Implementation
- L98 e2e 加 T_Render_Bitmap: Show() 后 GetDC + BitBlt 到内存 bitmap + 验证 bitmap 颜色变化
- L98 e2e 加 T_Populate_*: Show() 后 TreeView_GetCount / ListView_GetItemCount > 0
- v0.19.0.31 流程: ship 前必须有真 binary GUI 渲染验证

### Metadata
- Frequency: recurring (5+ ship 轮都缺)
- Related Features: v0.19.0.30 e2e link-probe
- See Also: LRN-20260715-001, LRN-20260715-002, LRN-20260715-003
- Pattern-Key: tests.gui-rendering-blind-spot

---

## [FEAT-20260715-002] 完整 user flow e2e 验证 (不只 mechanism/pixel)

**Logged**: 2026-07-15T12:30:00Z
**Priority**: critical
**Status**: pending
**Area**: tests

### Requested Capability
- e2e 必须 verify **完整 user flow**, 不只 mechanism (callback invoke) 或 pixel (RGB 非 0)
- 必加:
  - T_Add_Phrase: type text in search/input → click Add → 验证 list 增 1 item + item 文本 = typed text
  - T_Select_Edit: click list item → 验证顶部 input 显示 item.text
  - T_Save_Phrase: 修改 → Save → 验证 YAML 写盘 + 内存 m_phrases 更新
  - T_Phrases_Hotkey: Alt+. → 弹 modal
  - T_Phrases_QuickPanel: hit==1 button → 弹 modal (跟 hotkey 同 instance, 同 state)
  - T_UserDict_Hotkey: Alt+/ → 弹 modal (新 hotkey)
  - T_UserDict_QuickPanel: hit==2 button → 弹 modal
  - T_UserDict_State: modal 跟 PhrasesDialog 共享 s_state enum / 独立 / 一致性
- T_ShortcutSettings_Hotkey: Ctrl+Shift+K → 弹 modal
- T_ShortcutSettings_QuickPanel: hit==3 button → 弹 modal

### User Context
v0.19.0.31 ship 报告 "189 + 53 e2e binary (含 pixel-level) PASS" — 但 user 跑真 binary 看到:
- PhrasesDialog inline edit 是坏的 (2 个 unclickable input, 不是 user 期望 "顶部 input + add 按钮 + list" UX)
- QuickPanel Phrase 按钮路径 ≠ hotkey 路径
- UserDict 按钮 仍无反应

我方 e2e 只验 RGB 值非 0 (chrome paint 出非空像素), **没验** 完整 user flow。

### Complexity Estimate
medium

### Suggested Implementation
- L99 流程: e2e 必须 verify 完整 user flow, 不只 mechanism + pixel-level
- v0.19.0.32 修: 重做 PhrasesDialog UX (顶部 input + Add 按钮 + list, 替代 v0.19.0.30 的 inline edit overlay)
- v0.19.0.32 加: Alt+/ hotkey for UserDict
- v0.19.0.32 修: QuickPanel 按钮 path 跟 hotkey path 完全一致 (用同一 Show(), 不分支)

### Metadata
- Frequency: recurring (v0.19.0.30 + v0.19.0.31 都没真验)
- Related Features: v0.19.0.31 e2e
- See Also: LRN-20260715-004, ERR-20260715-002
- Pattern-Key: tests.mechanism-not-user-flow

---

## [FEAT-20260715-003] 短语 UI UX 重做 (顶部 input + Add 按钮 + list)

**Logged**: 2026-07-15T12:30:00Z
**Priority**: high
**Status**: pending
**Area**: frontend

### Requested Capability
短语 UI UX 重做:
- 顶部一个 input box (输入 phrase 文本)
- 右侧 Add 按钮
- 中间 list 显示已加 phrase
- 点击 list 中 phrase → input box 显示该 phrase 内容 (可编辑)
- 底部: 编辑/删除当前选中的 phrase, 取消

替代 v0.19.0.30 的 inline edit overlay (BeginInlineEdit 创 2 个 unclickable EDIT 控件, 不是 user 期望 UX)

### User Context
"应当简化逻辑, 在顶部输入框录入后, 下面可以直接点击添加就进入列表。选中列表中的短语, 短语出现再顶部, 可以直接进行编辑保存。"

### Complexity Estimate
medium

### Suggested Implementation
- 改 PhrasesDialog.cpp OnCreate:
  - 删 BeginInlineEdit overlay (s_hEditText, s_hEditCat 句柄)
  - 加 1 个顶部 input (单 EDIT, s_hInput)
  - 加 1 个 Add 按钮
  - 改 ListView 取代 TreeView (3 列: phrase text / category / shortcut) — 简化分类
- 改 EnterEditingState: 改为 SelectInList(phraseIndex) — 高亮 list item + fill input box with phrase.text
- 改 OnCommand ID_BTN_ADD: 直接调 m_phrases.push_back(input_text) + PopulateList + SavePhrases
- 改 OnCommand ID_BTN_EDIT: 调 m_phrases[selected] = current input → SavePhrases
- 改 OnCommand ID_BTN_DEL: erase selected + PopulateList

### Metadata
- Frequency: first_time
- Related Features: v0.19.0.30 BeginInlineEdit
- See Also: LRN-20260715-004
- Pattern-Key: ux.phrases-redesign

---

## [FEAT-20260715-004] UserDict Alt+/ hotkey

**Logged**: 2026-07-15T12:30:00Z
**Priority**: high
**Status**: pending
**Area**: frontend

### Requested Capability
UserDict 加 Alt+/ 全局热键 (同 Ctrl+Shift+U 入口)
- WeaselServerApp.cpp RegisterHotKey(MOD_ALT, VK_OEM_2 /* / */, ID_HOTKEY_USER_DICT_ALT_SLASH)
- 子类化 IPC server WM_HOTKEY 拦截 → UserDictionary::Show()
- resource.h 加 ID_HOTKEY_USER_DICT_ALT_SLASH
- QuickPanel hit==2 按钮修 wiring (确保 真调 s_onUserDict → UserDictionary::Show())

### User Context
"请为这个UI也设置一个快捷键 alt+/"
"设置栏的用户词典按钮, 没有反应, 至今还是无法调出这个UI"

### Complexity Estimate
simple

### Suggested Implementation
- WeaselServerApp.cpp: 加 RegisterHotKey(MOD_ALT, VK_OEM_2) → 调 UserDictionary::Show()
- resource.h: 加 #define ID_HOTKEY_USER_DICT_ALT_SLASH 9004
- WeaselServerApp.cpp: 加分支 `if (wParam == ID_HOTKEY_USER_DICT_ALT_SLASH) UserDictionary::Show();`
- 修 QuickPanelDialog.cpp hit==2 button wiring (确保 callback 真 invoke)
- v0.19.0.32 修

### Metadata
- Frequency: first_time
- Related Features: v0.19.0.29 QuickPanel button wiring
- See Also: LRN-20260715-004, ERR-20260715-002
- Pattern-Key: ux.userdict-hotkey

---

## [FEAT-20260715-005] QuickPanel 按钮路径 ≠ hotkey 路径 修复

**Logged**: 2026-07-15T12:30:00Z
**Priority**: high
**Status**: pending
**Area**: frontend

### Requested Capability
QuickPanel 按钮 (hit==1/2/3) 调出的 UI 跟 hotkey 调出的 UI **完全一致** (同一 Show(), 同一 state, 同一 dialog 状态)
- 排查: v0.19.0.29 接的 SetOnUserDict/SetOnShortcut setter 真 invoke 吗? wiring 链 OK 吗?
- 修: 让按钮路径跟 hotkey 走同一 Show() (UserDictionary::Show() 同一调用, 不分支)
- e2e 加 T_Phrases_QuickPanel_Equals_Hotkey: 2 路径调 Show() 后 s_state 跟 s_hwnd 一样

### User Context
"点击设置栏中的常用短语按钮, 出现的UI与使用快捷键调出的常用短语UI不同, 显示不正常"

### Complexity Estimate
medium

### Suggested Implementation
- 排查 QuickPanelDialog.cpp hit==1 button invoke 路径
- 排查 WeaselServerApp.cpp Run() 里 SetOnPhrase callback 是否跟 Alt+. hotkey 路径走同一 PhrasesDialog::Show()
- 如果不同, 修统一
- v0.19.0.32 fix

### Metadata
- Frequency: first_time
- Related Features: v0.19.0.29 QuickPanel button wiring
- See Also: LRN-20260715-004, ERR-20260715-002
- Pattern-Key: ux.button-path-consistency

---
