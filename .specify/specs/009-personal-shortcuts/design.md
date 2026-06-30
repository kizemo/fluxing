# 009 · 火流猩输入法 v2 · 常用短语（独立于用户词典）

> 范围：把"常用短语"从用户词典中拆出来，做成独立存储 + 独立 UI（`Alt+K` 弹列表）+ 独立编辑/添加流程；与 RIME 引擎主流程解耦。

## 0. 上下文

- 当前 Weasel 没有"常用短语"概念；用户用 `user_phrase` 方案或自建 `custom_phrase.txt` 方案。
- 现有 `RimeWithWeaselHandler::GetCustomResource` 支持加载任意文件 → 资源。
- v2 引入独立存储 `%LocalAppData%\Fluxing\phrases.json`，与 `user.db` 物理隔离；UI 复用 spec 006 的 FluxingPanelHost 组件库。

## 1. 产品视角

### 1.1 数据模型

```json
// %LocalAppData%\Fluxing\phrases.json
{
  "version": 1,
  "phrases": [
    { "id": "uuid-1", "text": "邮箱：duanyi@aiec.fun", "tags": ["contact"], "pinyin_hint": "youxiang", "created_at": "2026-06-29T10:00:00Z", "use_count": 12 },
    { "id": "uuid-2", "text": "好的，收到！", "tags": ["ack"], "pinyin_hint": "haode", "created_at": "2026-06-29T10:05:00Z", "use_count": 3 }
  ]
}
```

字段：
- `id`：UUID v4（CRUD 唯一标识）
- `text`：短语文本（≤ 200 字节）
- `tags`：标签数组（用于分类过滤）
- `pinyin_hint`：拼音首字母（用于列表内过滤加速）
- `created_at`：创建时间
- `use_count`：上屏次数（用于按热度排序）

### 1.2 UI 行为

- **触发**：`Alt+K` 全局热键（与 spec 006 的 `Alt+,` 同机制）。
- **窗口**：mac 风列表窗口，width = 480px，height = 480px，position = 屏幕右下角（与快速设置面板同位）。
- **交互**：
  - **上下箭头**：选中上/下一项
  - **PgUp/PgDn**：跳页（每页 50 条）
  - **`/` 或 `Ctrl+F` + 输入拼音首字母**：实时过滤（如 `yx` 匹配 `youxiang`）
  - **`Enter`**：高亮项上屏（不关闭输入）
  - **`Esc`** 或窗口失焦 1s：关闭窗口
  - **`+` 新增**：弹编辑窗（text + tags）
  - **`✎` 编辑**：弹编辑窗（text + tags + use_count 重置）
  - **`Delete`**：删除高亮项（无确认）
- **排序**：默认按 `use_count desc`（热度最高在前），可切"按时间"。

### 1.3 用户故事

- **US5-A** [P1]：用户在 notepad 按 `Alt+K` → 列表窗弹出，显示所有常用短语（默认 50 条/页），按热度排序。
- **US5-B** [P1]：用户在列表窗按 `Enter` 选中第 1 项 → 该短语上屏到 notepad，列表窗**不关闭**。
- **US5-C** [P1]：用户在列表窗按 `/yx` → 列表过滤为"邮箱：duanyi@aiec.fun"（`pinyin_hint=youxiang` 匹配）。
- **US5-D** [P1]：用户按 `+` → 弹编辑窗，填写 text="新的短语"，tags=["work"]，保存 → 立即出现在列表。

### 1.4 验收

- Given 常用短语库有 5 条
- When 用户按 `Alt+K`
- Then 列表窗弹出，按 use_count desc 排序，第 1 项为最高 use_count 的短语
- And 按 `Enter` → 该短语上屏到光标位置
- And 列表窗不关闭，可继续选择
- And 按 `Esc` 或失焦 1s → 关闭

## 2. 技术视角

### 2.1 新增模块

| 模块 | 路径 | 角色 |
|---|---|---|
| `FluxingPersonalShortcuts/PhrasesStore` | `PhrasesStore.{h,cpp}` | 读写 `phrases.json`；线程安全（读写锁） |
| `FluxingPersonalShortcuts/PhrasesWindow` | `PhrasesWindow.{h,cpp}` | 列表 UI（复用 spec 006 的 `FluxingComponents::List`） |
| `FluxingPersonalShortcuts/PhraseEditWindow` | `PhraseEditWindow.{h,cpp}` | 编辑/新增弹窗 |
| `FluxingPersonalShortcuts/PinyinHint` | `PinyinHint.{h,cpp}` | text → pinyin 首字母（如 `邮箱` → `yx`） |

### 2.2 全局热键

- `RegisterHotKey(HWND_BROADCAST, ID_HOTKEY_PHRASES, MOD_ALT, 'K')`。
- 与 `Alt+,` 同样由 WeaselServer 注册 → IPC 转发给 FluxingPanelHost。

### 2.3 数据流（上屏）

```
用户按 Alt+K
  → WeaselServer 收 ID_HOTKEY_PHRASES
  → IPC: "show Phrases" 给 FluxingPanelHost
  → PhrasesWindow 显示（读 PhrasesStore）
  → 用户按 Enter
  → PhrasesWindow::OnEnter
  → 调 weasel::Client::CommitText(text)  ← 复用 WeaselIPC 客户端
  → WeaselServer 调 rime_api->commit
  → use_count++ 持久化
```

### 2.4 拼音首字母提取

- **依赖**：RIME 引擎 1.13 已含 `RimeTraits::get_translator` + `RimeGetPhoneticCode`。
- **实现**：PhrasesStore 加载时一次性把每条 text 转拼音首字母缓存到内存（O(n) 一次性，O(1) 查询）。

### 2.5 验证步骤

1. **TDD**：
   - `TestPhrasesStore.cpp` — 断言：
     - 读 `phrases.json` 正确解析
     - 写回 round-trip 一致
     - 并发读写不破坏数据（10 线程随机读写）
   - `TestPinyinHint.cpp` — 给定 text="邮箱" → "yx"
2. **手动验证**：
   - 装 Fluxing v2.0.0，启动 FluxingPanelHost。
   - 创建 5 条短语（通过 spec 007 UI 或直接编辑 json）。
   - 按 `Alt+K` → 列表弹出。
   - 上下箭头 / Enter / Esc / /yx 过滤 / + 新增 / ✎ 编辑 / Delete 全测。
3. **回归**：
   - RIME 引擎主输入流程不受影响（短语库独立于候选）。

## X. 暗色主题集成（F11 横切）

> 详细规范见 [spec 004 §9 F11 暗色主题横切规范](../004-fluxing-v2-roadmap/spec.md#9-f11-暗色主题横切规范spec-004-统一收口)。
> 本 spec 实施时引用之，不重复定义。

涉及本 spec 的窗口：
- 常用短语列表窗（PhrasesWindow.cpp）
- 短语编辑弹窗（PhraseEditWindow.cpp）

## 3. Out of scope

- 不做"短语云同步"（spec 010）。
- 不做"短语置顶 / 分组"（仅按 use_count 排序）。
- 不做"短语在候选窗显示"（仅通过 `Alt+K` 列表访问）。
- 不做"短语导入/导出"（spec 010 加）。
- 不做"短语图标 / 颜色"。

## 4. 完成定义

- [ ] T001 `FluxingPersonalShortcuts/PhrasesStore.{h,cpp}`（含读写锁 + use_count 持久化）
- [ ] T002 `FluxingPersonalShortcuts/PinyinHint.{h,cpp}`（基于 RIME 引擎 API）
- [ ] T003 `FluxingPersonalShortcuts/PhrasesWindow.{h,cpp}`（列表 UI）
- [ ] T004 `FluxingPersonalShortcuts/PhraseEditWindow.{h,cpp}`（编辑/新增）
- [ ] T005 `weasel.sln` + `xmake.lua` 加新模块
- [ ] T006 `WeaselServer/WeaselServerApp.cpp` 加 `Alt+K` 热键
- [ ] T007 `test/TestPhrasesStore.cpp` + `TestPinyinHint.cpp` 单测通过
- [ ] T008 手动验证 3 平台 3 DPI
- [ ] T009 commit：`feat(fluxing): spec 009 personal shortcuts (phrases)`
- [ ] T010 release `fluxing-0.21.0.0-installer.exe`