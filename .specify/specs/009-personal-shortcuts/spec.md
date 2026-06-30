# 009 · 火流猩输入法 v2 · 个人快捷短语

> 元 spec 004 拆分。`Alt+K` 弹出 mac 风列表 UI，按 use_count desc 排序，支持上下箭头翻页 / 输入拼音首字母过滤 / Enter 上屏 / Esc 关闭。

## 0. 上下文

- 常用短语与动态用户词典（user.db）是两类不同心智模型。
- 数据保存至 `%LocalAppData%\Fluxing\phrases.json`（与用户词典 `user.db` 物理隔离）。
- 复用 spec 006 的 FluxingComponents 组件库（List / Button）。

## 1. 产品视角

### 1.1 目标

让用户在不进设置 UI 的情况下，1 次快捷键呼出常用短语库，搜得到、上屏快。

### 1.2 用户故事

- US5-A [P1]：常用短语库有 5 条 → 按 `Alt+K` 弹出列表 → 列表窗按 use_count desc 排序 → 第 1 项为最高 use_count。
- US5-B [P1]：列表窗不关闭，可继续选择；按 Enter → 该短语上屏到光标位置。
- US5-C [P1]：按 Esc 或失焦 1s → 关闭。
- US5-D [P1]：按 `+` → 弹编辑窗，填写 text="新的短语"，tags=["work"]，保存 → 立即出现在列表。

### 1.3 验收

- Given 常用短语库有 5 条，
- When 用户按 `Alt+K`，
- Then 列表窗弹出，按 use_count desc 排序，第 1 项为最高 use_count 的短语。
- And 按 Enter → 该短语上屏到光标位置。
- And 列表窗不关闭，可继续选择。

## 2. Out of scope

- 不做"短语云同步"（spec 010）。
- 不做"短语置顶 / 分组"（仅按 use_count 排序）。
- 不做"短语在候选窗显示"（仅通过 `Alt+K` 列表访问）。
- 不做"短语导入/导出"（spec 010 加）。
- 不做"短语图标 / 颜色"。
- 暗色主题：详细规范见 spec 004 §9；本 spec 实施时引用之。

## 3. 依赖

- spec 006 FluxingPanelHost + FluxingComponents 组件库（List 复用）。
- RIME 引擎 1.13 `RimeTraits::get_translator` + `RimeGetPhoneticCode`（拼音首字母提取，spec 009 §2.4）。
- 现有 `WeaselServer/WeaselServerApp.cpp` `RegisterHotKey` 全局热键机制。
- `WeaselIPC` 客户端（spec 006 引入 `FluxingIPCClient`）。