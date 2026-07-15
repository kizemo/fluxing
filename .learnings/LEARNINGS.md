# Learnings

Corrections, insights, and knowledge gaps captured during development.

**Categories**: correction | insight | knowledge_gap | best_practice

---

## [LRN-20260715-001] insight

**Logged**: 2026-07-15T11:00:00Z
**Priority**: critical
**Status**: pending
**Area**: tests

### Summary
v0.19.0.30 ship 时只跑了 unit test + e2e binary sandbox (mechanism verify), 没验 **end-to-end UI 渲染** (tree/listview 真的显示内容)。User 上传截图 UserDict + PhrasesDialog 都 **空 body** (没有 listview / tree items), 但 174 unit + 38 e2e binary assertions 全 PASS — **测试盲区 + 假装 PASS**。

### Details
- 2026-07-14 ship v0.19.0.30 (commit aa3f9b94) 3 bug 修复 (grace + SetFocus + child-focus exception)
- 验证: 5 unit test exe (12+1+75+26+22=136 PASS) + e2e binary (38/38 PASS)
- e2e 验证**机制** (callback invoke 1x, hit==2/3 routing live, grace guard 拦 Hide): bug 都修
- 但 e2e **没验**:
  1. 真实 dialog **Show** 后 Paint cycle (PaintOpaqueContent 真的画 ListView 4 列 / Tree items 吗?)
  2. **OnCreate** 真的创子控件吗? (s_hTree / s_hListView 在 WndProc 处理 WM_CREATE 时分配吗?)
  3. **PopulateTree / PopulateList** 真的被调吗? (Show() 调还是不调?)
- 2026-07-15 user 报: UserDict + PhrasesDialog "无法调出" / "界面不同" / "无法使用" (screenshots 证实 — 空 body, no listview / no tree)

**3 个 verdict 维度 FAIL**:
1. **Code Review** PASS (静态 8 项)
2. **Unit test** PASS (5 套)
3. **E2E binary mechanism** PASS (38 assertions, callback 真的 invoke 1x)
但 **真实 end-to-end UI 渲染 FAIL** (4 个 verifier 都看不到空 body)

### Suggested Action
- L98 fix: 加 **真 GUI 渲染测试** (模拟 user 启动 WeaselServer, CreateWindow 触发 OnCreate, 验证 s_hTree / s_hListView 创建 + PopulateTree 调 + TreeView_InsertItem 真的 add 数据)
- L98 e2e 加 T_Bug: 实际跑 Show() 后查 tree item 数 (TreeView_GetCount) > 0
- L98 加 T_Populate: Show() 后 ListView_GetItemCount > 0 (UserDict / PhrasesDialog v2)
- v0.19.0.31+ 流程: **任何 modal Show 路径必须验证 paint 真的输出 item, 不只验证机制 invoke**

### Metadata
- Source: user_feedback
- Related Files: WeaselServer/UserDictionary.cpp, WeaselServer/PhrasesDialog.cpp, test/v0_19_0_30_e2e/
- Tags: tests, e2e, gui-rendering, mechanism-vs-end-result
- See Also: LRN-20260715-097 (L97 第一次 e2e 缺 end-to-end 验证), AP-L97-A
- Pattern-Key: tests.gui-rendering-blind-spot
- Recurrence-Count: 1
- First-Seen: 2026-07-15
- Last-Seen: 2026-07-15

---

## [LRN-20260715-002] insight

**Logged**: 2026-07-15T11:05:00Z
**Priority**: high
**Status**: pending
**Area**: tests

### Summary
链接生产代码 (link-probe) 不等于真 GUI 渲染。e2e 链接 `PhrasesDialog.cpp + QuickPanelDialog.cpp` 但没真 CreateWindow 触发完整 WndProc 周期, 只测 static 字段访问 / SendMessageW 走特定分支, 漏了 OnCreate / OnPaint / PopulateTree 路径。

### Details
- v0_19_0_30_e2e.cpp 405 行, link-probe 模式
- 走真实 OS dispatch (`SendMessageW`) 测:
  - `s_showTime` 设值 + grace 期间 WM_ACTIVATEAPP 拦 Hide
  - `SetOnUserDict + WM_LBUTTONDOWN/UP` 测 callback invoke
  - `EnterEditingState + SetFocus` 测 state transition
  - `WM_KILLFOCUS(s_hBtnAdd)` 测 isChild 例外
- **漏测**:
  - OnCreate 实际创建 s_hTree / s_hListView / s_hBtnAdd 等子控件
  - OnPaint 实际画 tree items / listview items
  - PopulateTree 真的填充 item (vs s_hTree 创了但 PopulateTree 失败)
- e2e 38/38 assertions **全 PASS**, 但 user 跑真 binary 看到空 body

### Suggested Action
- L98 e2e 加:
  - T_Populate_PhrasesDialog: Show() 后 `TreeView_GetCount(s_hTree) > 0`
  - T_Populate_UserDict: Show() 后 `ListView_GetItemCount(s_hList) > 0`
  - T_Paint_Bitmap: Show() 后 GetDC + BitBlt 到内存 bitmap + 验证 bitmap 不是空白
  - T_WM_PAINT_Cycle: 手动 dispatch WM_PAINT + verify RepaintLayered 调用
- 任何新 modal Show 路径必须 verify item 数 > 0
- e2e test 不能只测机制, 必须测**真输出**

### Metadata
- Source: user_feedback
- Related Files: test/v0_19_0_30_e2e/v0_19_0_30_e2e.cpp
- Tags: tests, link-probe, gui, paint, item-populate
- See Also: LRN-20260715-001
- Pattern-Key: tests.mechanism-vs-result-gap
- Recurrence-Count: 1
- First-Seen: 2026-07-15
- Last-Seen: 2026-07-15

---

## [LRN-20260715-003] insight

**Logged**: 2026-07-15T11:10:00Z
**Priority**: critical
**Status**: pending
**Area**: frontend

### Summary
3 个 modal (QuickPanel / PhrasesDialog / UserDictionary) ShipHistory 含 5+ 个版本 (v0.19.0.24-30), 每次 ship 都说 "visual/功能都好了" 但 user 实际跑都看到空 / 简陋 UI, 我方 ship 跟 user 跑 之间脱节, 连续 4 轮 ship 没真正解决 user 问题。

### Details
- v0.19.0.24 (L94): QuickPanel grace fix, user 报 "短暂消失", 仍存在 → v0.19.0.25 又 ship 一遍
- v0.19.0.25 (L95): PhrasesDialog v1, user 报 "简陋", 仍简陋 → v0.19.0.26 又 ship 一遍
- v0.19.0.26 (L96): PhrasesDialog bug fix, user 报 "简陋 / Add Edit 关闭", 仍存在 → v0.19.0.28 ship Phrases v2
- v0.19.0.28 (L95): Phrases v2 / UserDict / Shortcut 一次 ship, user 报 "外观和功能都还没有实现" → v0.19.0.29 接 QuickPanel 按钮
- v0.19.0.29 (L96): QuickPanel 5 按钮, user 报 "无法调出" → v0.19.0.30 fix 3 bug
- v0.19.0.30 (L97): 3 bug 修复, 我说 "174 assertions + 38 e2e 全 PASS, 38/38 e2e", user 上传 2 截图: **空 body, 完全不可用**

5+ 轮迭代, 每次都 "ship 成功", user 每次都说 "bug 还在", **ship-loop 失败模式**:
- 我方 ship = 174 assertions 全 PASS
- User 实际 = 截图空 body

### Suggested Action
- **L98 流程: 任何 ship 必须做真 binary 端到端 GUI 渲染验证** (不是 link-probe, 是真 Show 出来 GetDC + 抓 bitmap 验证非空)
- v0.19.0.31 流程: ship 前必须有 `T_Render_Bitmap` test, 验证 modal 真 paint 出 ListView/Tree 内容
- 重新审视: 之前 5 轮 ship 报告的 "tests 全 PASS" 都只是 mechanism test, **不是 end-to-end test**
- user 报 bug 优先级 > 我方静态分析 (user 看真屏幕, 我看代码)

### Metadata
- Source: user_feedback
- Related Files: WeaselServer/{PhrasesDialog,UserDictionary,ShortcutSettings}.{h,cpp}
- Tags: ship-loop, user-priority, gui-rendering, e2e-blind-spot
- See Also: LRN-20260715-001, LRN-20260715-002
- Pattern-Key: ship.gui-rendering-blind-spot
- Recurrence-Count: 5
- First-Seen: 2026-07-13
- Last-Seen: 2026-07-15

---
