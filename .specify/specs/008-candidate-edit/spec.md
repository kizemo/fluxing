# 008 · 火流猩输入法 v2 · 候选字编辑（删除 + 屏蔽）

> 元 spec 004 拆分。在候选字窗的候选条目上接收右键，按规则触发"删除"或"屏蔽"动作；不弹菜单、不弹确认、不弹 toast。

## 0. 上下文

- 当前 `WeaselUI/WeaselPanel` 处理 `WM_LBUTTONDOWN`（上屏）/ `WM_MOUSEMOVE`（hover）/ `WM_MOUSEWHEEL`（翻页）。
- `WM_RBUTTONDOWN` 当前未处理。v2 改造为：右键候选 → 删除或屏蔽。

## 1. 产品视角

### 1.1 触发规则

- 候选属性 `text.length() >= 2` AND `is_user_dict == true` → **删除**（从 user.db 移除）
- 其他（单字 / 共享词典命中）→ **屏蔽**（加入 `<schema_id>.user_ignore.txt`）
- 不弹任何菜单、确认、toast。
- 不撤销（用户已确认需求："无 5s toast"）。
- 数据可恢复：托盘面板 US2 加"恢复"按钮（spec 006）。

### 1.2 用户故事

- US4-A [P1]：候选窗有 5 候选，第 3 候选"测试短语"（用户词典命中，2 字节以上），右键它立即从候选窗消失。
- US4-B [P1]：候选窗有 5 候选，第 1 候选"中"（单字或共享词典），右键它立即从候选窗消失；之后任何输入"zhong"该候选不再出现。
- US4-C [P1]：右键动作不打断当前输入流程——删除后候选窗不关闭，用户可继续选其他候选。

### 1.3 验收

- Given 输入法处于中文模式，输入"ceshi shurut" → 候选窗出现"测试输入"作为第 2 候选。
- When 用户右键点击"测试输入"。
- Then 该候选立即从候选窗消失；重新输入"ceshi shurut"，"测试输入"不再出现。

## 2. Out of scope

- 不做"加入常用短语"（spec 009 单独做）。
- 不做"撤销 / 历史记录"（用户已确认不要 5s toast）。
- 不做"批量删除"（右键是单条目原子操作）。
- 不做"右键中键 / Shift+右键"扩展。
- 暗色主题：详细规范见 spec 004 §9；本 spec 实施时引用之。

## 3. 依赖

- librime 1.13 `RimeUserDict::user_dict_update(dict, key, value, -1)` API（删除词条）。
- librime 1.13 `rime_candidate_t::is_user_dict` 字段（task 阶段验证是否存在；不存在则用启发式：候选 text 与 user.db 内容匹配 → 视为用户词典）。
- RIME 引擎 1.13 `user_ignore` 钩子：spec 004 §8 风险 R4 记录"钩子不存在时走 schema patch fallback"。
- spec 004 §9 F11 暗色主题横切规范。