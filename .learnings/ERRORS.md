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
