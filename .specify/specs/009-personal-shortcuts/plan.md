# 009 · Plan · 个人快捷短语

## 1. 技术上下文

- **C++ 17 / WTL / ATL**（与 006 同栈）。
- **修改文件**：
  - 新建 `FluxingPersonalShortcuts/{PhrasesStore,PhrasesWindow,PhraseEditWindow,PinyinHint}.{h,cpp}`。
  - `WeaselServer/WeaselServerApp.cpp` 注册 `Alt+K` 全局热键。
- 不引入新依赖；`weasel.sln` + `xmake.lua` 加新 project。
- **数据格式**：`phrases.json`（jsoncpp 或自写；推荐 jsoncpp 与 librime 现有依赖对齐）。

## 2. Architecture

- **进程模型**：`FluxingPanelHost` 内的二级窗口（与 006 共享进程）；由 `Alt+K` 触发。
- **数据流**：用户按 `Alt+K` → WeaselServer 收 ID_HOTKEY_PHRASES → IPC `show Phrases` 给 FluxingPanelHost → PhrasesWindow 显示（读 PhrasesStore） → 用户按 Enter → 调 `weasel::Client::CommitText` 复用 WeaselIPC 客户端 → WeaselServer 调 `rime_api->commit` → use_count++ 持久化。
- **拼音首字母提取**：加载时一次性把每条 text 转拼音首字母缓存到内存（O(n) 一次性，O(1) 查询）。
- **暗色主题**：PhrasesWindow 订阅 `FluxingDarkModeBridge`（spec 004 §9.4 集成点）。

## 3. Constitution Check

| Rule | Status | Notes |
|---|---|---|
| I. Intent | OK | spec.md §1.1 明确"1 键呼出 + 搜得到 + 上屏快" |
| II. Test | OK | TestPhrasesStore + TestPinyinHint 引入 |
| III. Spec-Artifact | OK | 3 件套齐全 |
| IV. Clarification | OK | 无 [NEEDS CLARIFICATION] |
| V. Incremental | OK | 009 在 006 之后 ship |
| R1-R9 | OK | 引用 spec 004 §9 |
| P1-P8 | OK | P8 brand-fork scope |

## 4. 风险

- **R1**：`Alt+K` 与系统 / IDE 已知快捷键冲突（K 键在 Chrome 是搜索栏聚焦）；spec 007 冲突检测承担。
- **R2**：拼音首字母提取对多音字不准确（任务阶段评估是否用 RIME 引擎 API 还是简单字符映射）。
- **R3**：use_count 持久化并发读写需加锁（std::shared_mutex + 写时拷贝）。