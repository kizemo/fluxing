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
