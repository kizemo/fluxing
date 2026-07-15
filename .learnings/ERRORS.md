# Errors

Command failures and integration errors.

---

## [ERR-20260715-001] v0.19.0.30 ship-claim "全 PASS" 跟 user 实际 "空 body" 严重脱节

**Logged**: 2026-07-15T11:00:00Z
**Priority**: critical
**Status**: pending
**Area**: tests

### Summary
v0.19.0.30 ship (commit aa3f9b94) 自我报告 "174 unit assertions 全 PASS + 38 e2e binary assertions 全 PASS", User 上传 2 截图: UserDict (空 body, no listview) + PhrasesDialog v2 (空 body, no tree) 完全不可用。Ship 报告跟 user 实际体验严重脱节。

### Error
- 我方 ship 报告: "Total 20 个 test project 全 PASS, 5/15 关键 test 全 PASS, 零回归"
- User 实际 (screenshots): UserDict + PhrasesDialog 都 "空 body, 没有 listview / 没有 tree, 按钮存在但内容不显示"

### Context
- 2026-07-14 ship v0.19.0.30: fix 3 bug (grace + SetFocus + child-focus)
- 我方验证: 5 unit test exe + e2e binary sandbox (link-probe + SendMessageW)
- 我方 verify 没跑真 GUI 渲染 (Bitmap 不是空白)
- 2026-07-15 user 报: "用户词典无法调出; 常用短语, 再设置栏点击按键调出的界面, 与使用快捷键调出的界面不同, 且都无法使用"

### Suggested Fix
- L98 e2e 加 T_Render_Bitmap: Show() 后 GetDC + BitBlt 到内存 bitmap + 验证 bitmap 不是空白
- L98 e2e 加 T_Populate_PhrasesDialog: Show() 后 TreeView_GetCount > 0
- L98 e2e 加 T_Populate_UserDict: Show() 后 ListView_GetItemCount > 0
- L98 fix actual bug: UserDict + PhrasesDialog v2 真的没调 PopulateTree / PopulateList (或 PopulateTree / PopulateList 失败)
- v0.19.0.31 流程: ship 前必须有真 binary GUI 渲染验证, 不能只 link-probe

### Metadata
- Reproducible: yes
- Related Files: WeaselServer/{PhrasesDialog,UserDictionary}.cpp, test/v0_19_0_30_e2e/
- See Also: LRN-20260715-001, LRN-20260715-002, LRN-20260715-003
- Pattern-Key: tests.gui-rendering-blind-spot
- Recurrence-Count: 1
- First-Seen: 2026-07-15
- Last-Seen: 2026-07-15

---

## [ERR-20260715-002] v0.19.0.31 ship "pixel-level PASS" 跟 user 实际 "UX 坏 + button 无反应" 严重脱节

**Logged**: 2026-07-15T12:30:00Z
**Priority**: critical
**Status**: pending
**Area**: frontend

### Summary
v0.19.0.31 ship (commit cef9c67a) 自我报告 "189 assertions + 53 e2e binary (含 pixel-level paint 验证)" — User 上传 截图: 1) PhrasesDialog inline edit 是坏的 (2 个 unclickable EDIT 控件, 不是 user 期望的 "顶部 input + Add 按钮 + list" UX); 2) QuickPanel Phrase 按钮调出的 UI 跟 hotkey 不一样 (显示不正常); 3) UserDict 按钮 仍无反应, 需加 Alt+/ hotkey。

我方 e2e 53/53 PASS, 我说 "已实现" — 但 user 实际看到 3 个新严重 UX bug。**Pixel-level RGB 值非 0 ≠ 功能正常**。

### Error
- 我方 ship 报告: "189 assertions 全 PASS, 53/53 真 binary e2e (含 pixel-level paint 验证), 0 回归"
- User 实际 (截图):
  - 常用短语 dialog 顶部 input + 2 个 unclickable EDIT 控件 (不是 user 期望的 list UX)
  - QuickPanel 按钮调出的 UI 跟 hotkey 不一样
  - UserDict 按钮 仍无反应

### Context
- 2026-07-15 ship v0.19.0.31: 修 3 modal chrome paint 路径 (L98)
- 我方验证: 5 unit test + e2e binary (含 GetPixel 真 binary 像素验证)
- 我方 verify 只验 RGB 值非 0 (chrome paint 出非空像素), **没验**:
  - 完整 user flow (type in search → click Add → 验证 list 增 item; select item → 验证 input 填内容; save → 验证持久化)
  - 路径一致性 (QuickPanel 按钮 vs hotkey 调同一 Show())
  - 按钮 wiring (UserDict 按钮 invoke s_onUserDict 真调 Show)
- 2026-07-15 user 报: "已经多轮修改不达标了, 如果已经在沙箱进行了验证, 这种情况完全不应该发生"

### Suggested Fix
- L99 e2e 加完整 user flow 测试 (T_Add, T_Select, T_Save, T_Phrases_Hotkey, T_Phrases_QuickPanel, T_UserDict_Hotkey, T_UserDict_QuickPanel)
- L99 修 PhrasesDialog UX: 顶部 input + Add 按钮 + list (替代 v0.19.0.30 的 inline edit overlay)
- L99 修 UserDict 按钮 wiring: QuickPanel hit==2 button 真 invoke s_onUserDict
- L99 加 Alt+/ hotkey for UserDict
- L99 修 QuickPanel Phrase 按钮路径: 跟 hotkey 完全一致 (用同一 Show(), 不分支)

### Metadata
- Reproducible: yes
- Related Files: WeaselServer/PhrasesDialog.cpp, WeaselServer/UserDictionary.cpp, WeaselServer/WeaselServerApp.cpp, test/v0_19_0_30_e2e/
- See Also: LRN-20260715-001/002/003/004, AP-L98-A, AP-L98-F
- Pattern-Key: tests.mechanism-not-user-flow
- Recurrence-Count: 2
- First-Seen: 2026-07-15
- Last-Seen: 2026-07-15

---
