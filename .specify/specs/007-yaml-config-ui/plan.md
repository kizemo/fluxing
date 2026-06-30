# 007 · Plan · yaml 可视化设置 UI

## 1. 技术上下文

- **C++ 17 / WTL / ATL / D2D / DirectWrite**（与 006 同栈）。
- **修改文件**：新增 `FluxingConfigEditor/{ConfigEditorApp,HotkeyPage,StylePage,SchemaPage,UserDictPage,YamlRoundTrip,ConflictChecker}.{h,cpp}`。
- 不引入新依赖；`weasel.sln` + `xmake.lua` 加新 project。
- **YamlRoundTrip**：保留 yaml 注释 + key 顺序；基于 yaml-cpp + 自写 visitor。

## 2. Architecture

- **设置 UI 进程**：`FluxingPanelHost` 内的二级窗口（与 006 共享进程），由 `Alt+,` → ⚙ 更多 → 偏好 触发。
- **YamlRoundTrip**：yaml-cpp 解析后保留注释（用 `mark` 节点指针）+ key 顺序（用 vector 而非 map），保存时按原顺序写回。
- **ConflictChecker**：与系统 / IDE 已知快捷键数据库比对（静态 db，复用 spec 005 + 006 的检测逻辑）。
- **暗色主题**：订阅 `FluxingDarkModeBridge`（006 引入）；界面外观页加"暗色 / 亮色 / 跟随系统"单选。

## 3. Constitution Check

| Rule | Status | Notes |
|---|---|---|
| I. Intent | OK | spec.md §1.1 明确"不读 yaml 也能改" |
| II. Test | OK | TestYamlRoundTrip + TestConflictChecker 引入 |
| III. Spec-Artifact | OK | 3 件套齐全 |
| IV. Clarification | OK | 无 [NEEDS CLARIFICATION] |
| V. Incremental | OK | 007 在 006 + 009 之后 ship |
| R1-R9 | OK | 引用 L04 + spec 004 §9 |
| P1-P8 | OK | P8 brand-fork scope |

## 4. 风险

- **R1**：YamlRoundTrip 保留注释难度高；librime yaml-cpp 注释节点 API 不稳定，task 阶段验证。
- **R2**：ConflictChecker 静态数据库覆盖不全（Chrome / VSCode / IDEA / …），task 阶段建立 JSON db。
- **R3**：用户词典 user.db 是 sqlite（librime 1.13+）；需要单独 SQLite 依赖，task 评估是否使用 vendored 库。