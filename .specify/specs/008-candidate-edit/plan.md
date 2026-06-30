# 008 · Plan · 候选字编辑

## 1. 技术上下文

- **C++ 17 / WTL / ATL**（与 WeaselUI 同栈）。
- **修改文件**：
  - `WeaselUI/WeaselPanel.{h,cpp}` — 加 `OnRButtonDown(UINT, CPoint)` 消息处理。
  - `RimeWithWeasel/{RequestHandler.h,RimeWithWeaselHandler.cpp}` — 加 `RequestDeleteCandidate(size_t index, const std::string& text, bool is_user_dict)`。
  - 新建 `RimeWithWeasel/CandidateEdit.{h,cpp}` — 封装"用户词典删除 / 屏蔽"两套逻辑 + 防抖 100ms。
  - `output/data/rime_ice.schema.yaml`（fallback） — 加 schema patch 走 `speller/algebra/@exclude_pattern`（若 RIME 引擎 1.13 不支持 customization 钩子）。
- 不引入新依赖；`weasel.sln` 加 `CandidateEdit` 单元。

## 2. Architecture

- **右键事件**：`WeaselPanel::OnRButtonDown` 命中候选 → 调 `RequestHandler::RequestDeleteCandidate`。
- **删除路径**：`is_user_dict && text.length >= 2` → `user_dict_update(dict, key, value, -1)` + `sync_user_dict()` → 刷新候选。
- **屏蔽路径**：append `text + "\n"` 到 `<schema>.user_ignore.txt` → 客户端过滤（spec 004 §8 风险 R4 fallback 方案 B）。
- **防抖**：同一候选 100ms 内多次右键不重复写文件。
- **暗色主题**：WeaselPanel 接收主题切换（spec 004 §9.4 集成点）；屏蔽规则浮窗也订阅。

## 3. Constitution Check

| Rule | Status | Notes |
|---|---|---|
| I. Intent | OK | spec.md §1.1 明确"无确认无 toast" |
| II. Test | OK | TestCandidateEdit 引入（mock 候选 + mock user_dict_update） |
| III. Spec-Artifact | OK | 3 件套齐全 |
| IV. Clarification | OK | librime `is_user_dict` 字段是 task 阶段验证项 |
| V. Incremental | OK | 008 独立可 ship |
| R1-R9 | OK | 引用 L16 + L18 + spec 004 §8 风险 R4 |
| P1-P8 | OK | P8 brand-fork scope |

## 4. 风险

- **R1**：librime 1.13 `is_user_dict` 字段可能不存在（spec 008 §2.2 标注"需验证"）；fallback 用启发式匹配 user.db。
- **R2**：user_ignore 钩子不存在时走 schema patch fallback（spec 004 §8 R4），task 阶段验证 RIME 引擎 1.13 行为。
- **R3**：删除用户词典词条不可逆（无撤销），用户已确认；保留 user.db 备份由 spec 006 托盘面板"恢复"按钮承担。