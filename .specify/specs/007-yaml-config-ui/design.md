# 007 · 火流猩输入法 v2 · yaml 可视化设置 UI

> 范围：把 4 类 yaml 配置（快捷键 / 常用短语 / 用户词典 / 方案）暴露为 mac 风可视化编辑器；不再要求用户手编 yaml。
> 复用 spec 006 的 FluxingPanelHost 与 FluxingComponents；不复用 spec 009 的 PhrasesStore（不重复实现）。

## 0. 上下文

- 当前 `WeaselDeployer` 有样式设置对话框 (`UIStyleSettingsDialog`)、方案切换对话框 (`SwitcherSettingsDialog`)、词典管理对话框 (`DictManagementDialog`) — 全部旧式控件。
- 4 类 yaml 配置文件：
  - `default.yaml` — 全局 key_binder / ascii_composer / switcher / navigator
  - `weasel.yaml` — 样式
  - `<schema>.schema.yaml` — 方案
  - `user.db`（RIME 引擎自有，文本格式 `userdb.txt` 导出）
- 用户的核心痛点：4 个文件分散，编辑时不知道字段含义。

## 1. 产品视角

### 1.1 UI 结构

```
设置 UI（独立窗口；spec 006 的 FluxingPanelHost 承载）
├─ 侧边导航
│  ├─ 通用
│  │  ├─ 快捷键          ← spec 005 改的键位
│  │  ├─ 界面外观         ← weasel.yaml
│  │  └─ 高级
│  ├─ 输入方案
│  │  └─ 方案管理         ← <schema>.schema.yaml
│  ├─ 词典与短语
│  │  ├─ 用户词典         ← user.db / userdb.txt
│  │  └─ 常用短语         ← 跳到 spec 009 窗口
│  └─ 同步（占位 P2）     ← spec 010
└─ 主区域
   └─ 当前选中页的表单
```

### 1.2 各页表单

#### 1.2.1 快捷键页

- 列表显示 `key_binder.bindings` 所有条目（按 `when` 分组：paging / has_menu / always）。
- 列表项右侧显示"按键录制器"（`KeyCap` 控件）：点击 → 弹小窗"按下任意键..." → 按键 → 自动解析为 `mods+key` 字符串。
- 冲突检测：与系统 / IDE 已知快捷键数据库比对，标黄警告（如 `Ctrl+Shift+0` 在 Chrome 是 reset zoom）。
- 操作：编辑、删除、新增；保存 → 写回 `default.yaml` 的对应路径（保留其他字段、注释、key 顺序）。

#### 1.2.2 界面外观页

- 配色：候选字背景 / 文字 / 高亮 / 阴影 — 调色板（基于 weasel.yaml `style.color_scheme`）
- 透明度滑块（0-100%）
- 字体：中文 / 英文 / emoji — 下拉选择（系统已装字体列表）
- 字体大小滑块（10-24 px）
- 水平/竖直布局切换
- 横竖排候选切换
- 暗色 / 亮色 / 跟随系统 单选

#### 1.2.3 方案管理页

- 列表显示 `schema_list` 中的所有方案
- 操作：启用 / 停用 / 调整顺序（拖拽）/ 配置 `<schema>.schema.yaml` 的字段（拼写风格、候选项数、清空、模糊音等）

#### 1.2.4 用户词典页

- 列表显示 `user.db` 所有词条（`text 权重 码表` 格式）
- 搜索框（按 text 模糊匹配）
- 操作：编辑、删除、新增、批量导入（拖 .txt 到窗口）

### 1.3 用户故事

- **US3-A** [P1]：用户进入设置 UI → 快捷键页 → 列表显示当前 default.yaml 全部 key_binder 条目 → 点"翻页（下一页）"项的按键录制器 → 按 `/` → 自动解析为 `slash` → 保存 → 立即生效。
- **US3-B** [P1]：界面外观页拖透明度滑块 50% → 候选窗实时半透明。
- **US3-C** [P1]：方案管理页拖"朙月拼音"到顶 → 重启输入法后默认方案改变。
- **US3-D** [P1]：用户词典页搜索"测试" → 显示 3 条 → 删除其中 1 条 → 关闭设置 → 重新打字"ceshi" → 该词条不再出现。

### 1.4 验收

- Given 全新安装 Fluxing v2.0.0
- When 用户打开设置 UI（托盘面板的"偏好设置"或 `Alt+,` → ⚙ 更多 → 偏好）
- Then 弹出 mac 风独立窗口，左侧导航 + 右侧表单
- And 快捷键页加载并显示 default.yaml 当前所有 key_binder 条目（数 = X）
- And 修改任一条 → 保存 → 立即生效（无需重启）

## 2. 技术视角

### 2.1 新增模块

| 模块 | 路径 | 角色 |
|---|---|---|
| `FluxingConfigEditor/App` | `ConfigEditorApp.{h,cpp}` | 设置 UI 主窗口（独立进程？或 FluxingPanelHost 内的二级窗口） |
| `FluxingConfigEditor/HotkeyPage` | `HotkeyPage.{h,cpp}` | 快捷键页（UI + 按键录制器） |
| `FluxingConfigEditor/StylePage` | `StylePage.{h,cpp}` | 界面外观页（基于 weasel.yaml） |
| `FluxingConfigEditor/SchemaPage` | `SchemaPage.{h,cpp}` | 方案管理页 |
| `FluxingConfigEditor/UserDictPage` | `UserDictPage.{h,cpp}` | 用户词典页（基于 user.db） |
| `FluxingConfigEditor/YamlRoundTrip` | `YamlRoundTrip.{h,cpp}` | 保留注释 + key 顺序的 yaml 读写（基于 yaml-cpp + 自写 visitor） |
| `FluxingConfigEditor/ConflictChecker` | `ConflictChecker.{h,cpp}` | 与系统 / IDE 已知快捷键冲突检测（静态数据库） |
| `FluxingConfigEditor/ShortcutRecorder` | `ShortcutRecorder.{h,cpp}` | 按键录制（接受 WM_KEYDOWN，转 rime modifier+key 字符串） |

### 2.2 进程模型

- 复用 spec 006 的 `FluxingPanelHost.exe`，但通过命令行参数 `--window=settings` 启动新窗口实例。
- 设置 UI 启动后从 WeaselServer 拉当前 status、weasel.yaml、default.yaml、user.db 快照；编辑时本地缓存；保存时 IPC 给 WeaselServer → WeaselServer 调 RIME 引擎重新加载 + 写文件。

### 2.3 yaml round-trip

- **挑战**：yaml-cpp 读写会丢注释和 key 顺序；用户希望看到原 yaml 的注释。
- **方案 A（首选）**：基于 yaml-cpp AST，遍历 node tree 时用 mark 保留注释。
- **方案 B（fallback）**：用正则 + 行号定位修改（脆弱但能用）。
- **方案 C（最简）**：放弃注释保留，每次写回重排（用户体验差但最稳）。
- **本 spec 默认 A**，若 A 实施成本 > 1 周，回退 C。

### 2.4 按键录制器

- 接受 WM_KEYDOWN + WM_SYSKEYDOWN → 解析为 `(modifier_mask, vkey)`。
- RIME 引擎 modifier 字符串格式：`Control+Shift+9`、`alt+Release` 等。
- 输出直接对应 `key_binder.bindings` 的 `accept` 字段。
- 边界：组合键最多 4 modifier；按 `Esc` 取消录制。

### 2.5 验证步骤

1. **TDD**：
   - `TestYamlRoundTrip.cpp` — 读 + 写 + 重新读，断言原字符串 diff 为 0（保留注释 + 顺序）。
   - `TestShortcutRecorder.cpp` — mock WM_KEYDOWN，断言转字符串正确。
   - `TestConflictChecker.cpp` — 给定 `Ctrl+Shift+0` → 返回 `{app: Chrome, action: reset_zoom}`。
2. **手动验证**：
   - 装 Fluxing v2.0.0，打开设置 UI，4 页全测。
   - DPI 100/150/200 各验证一次。
3. **回归**：
   - spec 005 的新快捷键在 UI 中可显示、可改、可保存回 yaml。
   - 关闭 UI 后 IME 仍正常。
   - yaml 写回后 spec 008 的右键屏蔽功能仍正常。

## X. 暗色主题集成（F11 横切）

本 spec 涉及的所有 mac 风窗口都必须在 v2.0.0 整合时支持暗色主题。具体集成点：

- **主题源**：`%LocalAppData%\Fluxing\weasel.yaml` 的 `style.color_scheme`；新增 `theme.light` / `theme.dark` 双套色板。
- **触发**：监听 Windows `WM_SETTINGCHANGE` (lParam = `SPI_SETDESKWALLPAPER` 等) 主题变更 → 走 `FluxingDarkModeBridge` 广播给所有 mac 风窗口。
- **过渡**：色板切换 200ms 渐变（使用 `ID2D1SolidColorBrush` 的 `ColorF` 插值）。
- **存储**：颜色缓存按主题名索引（`LightColors` / `DarkColors`），切换时换指针；不重新分配资源。
- **DPI**：暗色切换不触发 `dpiScaleLayout` 重新计算。
- **候选面板**：与本 spec 的 mac 风窗口同步（共享色板缓存）。
- **测试**：`TestDarkModeBridge.cpp` mock `WM_SETTINGCHANGE`，断言所有订阅窗口收到回调 + 色板指针更新。

涉及文件：
- `FluxingComponents/Theme.{h,cpp}`（spec 006 引入，**所有 spec 共用**）
- `FluxingPanelHost/DarkModeBridge.{h,cpp}`（spec 006 引入）
- `RimeWithWeasel/WeaselUtility.{h,cpp}` 加主题切换广播
- `WeaselUI/WeaselPanel.cpp` 接收主题切换

依赖：spec 006（Theme/DarkModeBridge 必先 ship），spec 008/009/007 在 006 之后 ship。

## 3. Out of scope

- 不做"方案市场 / 一键安装方案"（v3+）。
- 不做"主题市场"（v3+）。
- 不做"云同步设置"（spec 010）。
- 不做"快捷键录制器"的多键组合（如自定义 chord）。

## 4. 完成定义

- [ ] T001 `FluxingConfigEditor/ConfigEditorApp.{h,cpp}` 骨架
- [ ] T002 `FluxingConfigEditor/HotkeyPage.{h,cpp}` + `ShortcutRecorder.{h,cpp}`
- [ ] T003 `FluxingConfigEditor/StylePage.{h,cpp}`
- [ ] T004 `FluxingConfigEditor/SchemaPage.{h,cpp}`
- [ ] T005 `FluxingConfigEditor/UserDictPage.{h,cpp}`
- [ ] T006 `FluxingConfigEditor/YamlRoundTrip.{h,cpp}`（方案 A 优先，fallback C）
- [ ] T007 `FluxingConfigEditor/ConflictChecker.{h,cpp}` + 静态数据库
- [ ] T008 `weasel.sln` + `xmake.lua` 加新模块
- [ ] T009 `test/TestYamlRoundTrip.cpp` + `TestShortcutRecorder.cpp` + `TestConflictChecker.cpp` 单测通过
- [ ] T010 手动验证 3 平台 3 DPI
- [ ] T011 commit：`feat(fluxing): spec 007 yaml config editor UI`
- [ ] T012 release `fluxing-0.22.0.0-installer.exe`