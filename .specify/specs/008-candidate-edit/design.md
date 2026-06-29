# 008 · 火流猩输入法 v2 · 候选字编辑（删除 + 屏蔽）

> 范围：在候选字窗的候选条目上接收**右键**，按规则触发"删除"或"屏蔽"动作；不弹菜单、不弹确认、不弹 toast。

## 0. 上下文

- 当前 `WeaselUI/WeaselPanel` 处理候选字窗的鼠标消息：`WM_LBUTTONDOWN`（上屏）、`WM_MOUSEMOVE`（hover）、`WM_MOUSEWHEEL`（翻页）。
- `WM_RBUTTONDOWN` 当前未处理。
- `RimeWithWeaselHandler` 已通过 `SelectCandidateOnCurrentPage` 上屏候选；删除用户词典词条需要 RIME 引擎的 `get_user_dict` + 改写 + 重新 `commit_recent`，或通过 RIME 引擎的 `customization` 钩子。
- 候选属性 (`is_user_dict`, `type`, `text`) 在 RIME 引擎 1.13 通过 `RimeCandidate` 字段暴露。

## 1. 产品视角

### 1.1 触发规则

| 候选属性 | 右键行为 |
|---|---|
| `text.length() >= 2` AND `is_user_dict == true` | **删除**：从用户词典 `user.db` 中移除该词条 |
| 其他（单字 / 共享词典命中 / 长度 < 2 字节） | **屏蔽**：加入 `<schema_id>.user_ignore.txt`，下次加载时从候选中过滤 |

- **不弹任何菜单、确认、toast**。
- **不撤销**（用户已确认需求："无 5s toast"）。
- 数据可恢复：托盘面板 US2 加"恢复"按钮（spec 006 任务），从最近一次自动备份还原 `user.db` 和 `phrases.json`。

### 1.2 用户故事

- **US4-A** [P1]：候选窗有 5 候选，第 3 候选"测试短语"（用户词典命中，2 字节以上），右键它，该候选立即从候选窗消失；切换到下一输入仍不出现在候选。
- **US4-B** [P1]：候选窗有 5 候选，第 1 候选"中"（单字或共享词典），右键它，该候选立即从候选窗消失；**之后任何输入**"zhong"该候选不再出现。
- **US4-C** [P1]：右键动作**不打断当前输入流程**——删除后候选窗不关闭，用户可继续选其他候选。

### 1.3 验收

- Given 输入法处于中文模式，输入"ceshi shurut"
- When 候选窗出现"测试输入"（用户词典命中）作为第 2 候选
- Then 用户右键点击"测试输入"→ 该候选立即从候选窗消失→ 关闭候选窗后重新输入"ceshi shurut"，"测试输入"不再出现在候选窗

- Given 输入"ceshi shurut"但"测试输入"是共享词典（luna-pinyin）命中
- When 用户右键点击"测试输入"
- Then 该候选立即从候选窗消失；下次重新输入"ceshi shurut"，"测试输入"仍不出现（已被屏蔽）
- And `%LocalAppData%\Fluxing\luna_pinyin.user_ignore.txt` 文件新增一行 `测试输入`

## 2. 技术视角

### 2.1 改动文件

| 文件 | 改动 |
|---|---|
| `WeaselUI/WeaselPanel.cpp` | 加 `OnRButtonDown(UINT, CPoint)` 消息处理；判断候选命中 → 调 `RimeWithWeaselHandler::RequestDeleteCandidate(id)` |
| `WeaselUI/WeaselPanel.h` | 加 `OnRButtonDown` 声明 |
| `RimeWithWeasel/RequestHandler.h` | 加新方法 `RequestDeleteCandidate(size_t index, const std::string& text, bool is_user_dict)` |
| `RimeWithWeasel/RimeWithWeaselHandler.cpp` | 实现删除 / 屏蔽；写 `user.db`（用 rime_api）或 `<schema>.user_ignore.txt` |
| `RimeWithWeasel/CandidateEdit.{h,cpp}`（新） | 封装"用户词典删除 / 屏蔽"两套逻辑 |
| `RIME 引擎/src/rime/.../customization.cc`（可选） | 若 RIME 引擎不读 `user_ignore.txt`，**不动 RIME 引擎子模块**——改为方案 patch（见 2.3） |
| `output/data/rime_ice.schema.yaml`（或当前默认方案的 yaml） | 加 `customization` 钩子（如 RIME 引擎 1.13 支持）或加 `__patch:/recognizer_pattern/speller/algebra` 过滤（fallback） |

### 2.2 RIME 引擎 1.13 已知接口

- `RIME_API struct rime_api_t { ... RimeUserDict* (*get_user_dict)(RimeContext* ctx, const char* name); ... }` — 拿用户词典句柄。
- `RimeUserDictIterator* (*user_dict_lookup)(RimeUserDict* dict, const char* key, bool predictive, bool unique);` — 查词条。
- `bool (*user_dict_update)(RimeUserDict* dict, const char* key, const char* value, int commit_count);` — 改写（**用 -1 commit_count 删除**）。
- `bool (*commit_recent)(RimeContext* ctx, const char* commit_text);` — 提交最近候选（用于"加入"而非"删除"，本 spec 不用）。
- `RIME_API struct rime_candidate_t` — 字段：`text`, `comment`, `preedit`, `is_user_dict` (1.13+ 添加？需验证)。

> 若 `is_user_dict` 字段在 1.13 不存在，agent 必须先读 RIME 引擎头文件确认；不存在则**用启发式**：候选 text 与 `user.db` 内容匹配 → 视为用户词典。

### 2.3 屏蔽（user_ignore）实施双方案

#### 方案 A：RIME 引擎 1.13 customization 钩子（首选）

若 RIME 引擎 1.13 暴露 `RIME_API struct rime_customization_t` 或 `RimeSchema::customization_file`，则：

```yaml
# rime_ice.schema.yaml
customization:
  - user_ignore_file: "%LocalAppData%\\Fluxing\\rime_ice.user_ignore.txt"
```

候选过滤在 RIME 引擎层完成；应用层只写文件。

#### 方案 B：schema patch + speller/algebra（fallback）

若 RIME 引擎 1.13 不支持 customization 钩子：

```yaml
# rime_ice.schema.yaml patch
patch:
  "speller/algebra/@exclude_pattern":
    # 动态生成：读 user_ignore.txt 转 speller/exclude_pattern
    # 但 yaml 静态配置不支持动态 → 改为 preprocessor 钩子
```

**真正可行** = 在 `RimeWithWeaselHandler::UpdateUI` 后做客户端过滤：
- RIME 引擎给候选 → 我们读 `user_ignore.txt` → 移除命中行 → 显示。
- 简单可靠，0 RIME 引擎依赖。
- 性能：每次候选更新读一次文件 + O(n) 过滤；n = 屏蔽条数（一般 < 200）；可接受。

**本 spec 默认采用方案 B**（客户端过滤），因为它 0 RIME 引擎依赖、风险最低。

### 2.4 数据流

```
用户右键候选 (x,y)
  → WeaselPanel 命中候选 index
  → WeaselPanel::OnRButtonDown
  → RequestHandler::RequestDeleteCandidate(index, text, is_user_dict)
  → RimeWithWeaselHandler::RequestDeleteCandidate
       if is_user_dict && text.length >= 2:
           user_dict_update(dict, key, value, -1)  # RIME 引擎 API
           sync_user_dict()  # RIME 引擎 API
       else:
           append text + "\n" to <schema>.user_ignore.txt
  → RimeWithWeaselHandler::RefreshMenu
  → UI 重新显示候选（不命中行 / 不命中屏蔽行）
```

### 2.5 验证步骤

1. **TDD（test）**：在 `test/` 加：
   - `TestCandidateEdit.cpp` — mock 候选列表 + mock RIME 引擎 user_dict_update，断言：
     - 长度 ≥ 2 字节 + `is_user_dict=true` 触发 user_dict_update(-1)。
     - 其他情况追加到 `<schema>.user_ignore.txt`。
     - 同一候选短时间内多次右键不重复写文件（防抖 100ms）。
2. **手动验证**：
   - 装 Fluxing v2.0.0，输入"ceshi shurut"，候选出现"测试输入"（用户词典）。
   - 右键"测试输入" → 立即消失。
   - 切到另一个 app，重新输入"ceshi shurut" → "测试输入"不再出现。
   - 检查 `%LocalAppData%\Fluxing\user.db` (snap) 中"测试输入"词条被删除。
3. **回归**：
   - 旧行为：左键上屏候选 → 仍然正常。
   - 旧行为：右键唤出系统菜单 → 不再唤出（被 RButtonDown 拦截）。

## X. 暗色主题集成（F11 横切）

本 spec 涉及的所有 mac 风窗口（候选字窗的右键后续反馈、屏蔽规则编辑浮窗）都必须在 v2.0.0 整合时支持暗色主题。具体集成点：

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

- 不做"加入常用短语"（spec 009 单独做）。
- 不做"撤销 / 历史记录"（用户已确认不要 5s toast）。
- 不做"批量删除"（右键是单条目原子操作）。
- 不做"右键中键 / Shift+右键"扩展。

## 4. 完成定义

- [ ] T001 `RimeWithWeasel/CandidateEdit.{h,cpp}` 实现（含防抖）
- [ ] T002 `WeaselUI/WeaselPanel.cpp` 加 OnRButtonDown
- [ ] T003 `RimeWithWeasel/RimeWithWeaselHandler.cpp` 加 RequestDeleteCandidate
- [ ] T004 `test/TestCandidateEdit.cpp` 单测通过
- [ ] T005 手动验证清单（2.5 第 2 步）3 平台 3 DPI 全过
- [ ] T006 回归清单（2.5 第 3 步）全过
- [ ] T007 commit：`feat(fluxing): spec 008 candidate right-click delete/ignore`
- [ ] T008 release `fluxing-0.19.0.0-installer.exe`