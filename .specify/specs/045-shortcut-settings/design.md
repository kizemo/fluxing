# 045 — 快捷键设置 UI (ShortcutSettings) — design

> **Status**: Draft v0 (UI design only; 尚未进入 implementation)
> **日期**: 2026-07-13
> **关联**: spec 050 `yaml-hotkey-editor-mvp` (MVP 数据/落盘逻辑已规划, 此 spec 是 visual + UX overlay)
> **接续**: spec 040 Fluent UI Tokens · spec 037 FluxingComponents · spec 042 PhrasesDialog (chrome 模板)
> **目标**: 让 Fluxing 用户**看到全部快捷键**与**重新绑定** key combination, **不需要手写 yaml** 也不需要重启 WeaselServer

---

## 1. Goals / Non-Goals

### 1.1 Goals

1. **单一真相 UI**: 用一张表显示当前 `key_binder/bindings` 全部条目, **Action** 列用人话 ("切换中英"), **Key Combo** 列显示 `Shift+Space` 而非 `space+Shift_L`
2. **键捕获 (capture) UX**: 点 Edit → 行变 active (蓝条 + "请按快捷键..." 提示) → 捕获下一个 keydown → 实时显示 `Ctrl+Shift+D?` → 确认/取消
3. **冲突检测 (lite)**: 实时检查与 librime 内置 binding 冲突 (e.g. `Control+1` 默认选候选 1), 警告**但允许 override**; 不查 OS / 其他 app 冲突 (那是 OS hotkey daemon 的事, P3 scope)
4. **持久化**: 写入 `<user_data_dir>/default.custom.yaml` 而非 `weasel.yaml` (L40 教训 + spec 050 §1.4), 触发 `WeaselDeployer.exe /deploy` 后立即生效
5. **零重启**: 保存后 WeaselServer 通过 librime re-deploy 接管新 binding, **不需要** taskkill / 退出 / 重启
6. **统一 chrome**: 跟 QuickPanel / PhrasesDialog 共享同一套 Liquid Glass token, 走 spec 040 §3.6 既有 modal 体系

### 1.2 Non-Goals

- ❌ 不做 OS-level hotkey 冲突检测 (e.g. VS Code / Vim 占的键) — 超出 P3 scope, 推荐用 user 手动 verify
- ❌ 不做 schema-aware 智能提示 (e.g. "你想绑的 toggle 在 librime 里有别名") — spec 050 §1.4 推迟
- ❌ 不做 undo/redo 多步栈 — 单次 "保存即生效", 想回退用 "重置全部"
- ❌ 不做 per-binding 热键预览 (在候选框模拟触发) — UX 复杂, v2 议题
- ❌ 不做 import/export 配置包 — spec 055 范围
- ❌ 不改 weasel.yaml 本身 (RIME schema) — 改的是 librime 用户配置 `default.custom.yaml`

---

## 2. UI 草图 (ASCII art)

### 2.1 主对话框 — Browse 状态 (520 × 560, 144 DPI 物理像素)

```
┌─ 快捷键设置 ───────────────────────────────────────[X]┐
│                                                       │  ← kTitleH=30
│  搜索: [____________________________]  [清空]         │  ← kSearchH=36
├───────────────────────────────────────────────────────┤
│  ┌─ Action ──────────┬─ Key Combo ─┬─  ─┐            │  ← kListH=380
│  │ 切换中英           │ Shift+Space │ ✎ ⟲│            │
│  │ 全/半角切换        │ Shift+Space │ ✎ ⟲│            │  ← kRowH=32
│  │ ASCII 模式         │ Ctrl+Shift  │ ✎ ⟲│            │
│  │ 翻页 - 上页        │ comma        │ ✎ ⟲│            │
│  │ 翻页 - 下页        │ period       │ ✎ ⟲│            │
│  │ 选第 2 候选        │ Control+1    │ ✎ ⟲│            │
│  │ 选第 3 候选        │ Control+2    │ ✎ ⟲│            │
│  │ 中英标点切换       │ Ctrl+Shift+9 │ ✎ ⟲│            │
│  │ 简繁切换           │ Ctrl+Shift+0 │ ✎ ⟲│            │
│  │ 用户词典           │ Ctrl+Shift+D │ ✎ ⟲│            │
│  │ 常用短语           │ Alt+.        │ ✎ ⟲│            │
│  │ 重新部署           │ Ctrl+F5      │ ✎ ⟲│            │
│  │  ... (滚动) ...                                     │
│  └────────────────────────────────────────────────────┘│
├───────────────────────────────────────────────────────┤
│  [+ 自定义快捷键]   [⟳ 重置全部]      [取消]  [保存]  │  ← kBtnBarH=48
└───────────────────────────────────────────────────────┘
   ↑ ↑                ↑ ↑              ↑ ↑    ↑ ↑
   kBtnW=88           kBtnW=88         kBtnW  primary
```

### 2.2 行内 Capturing 状态 (编辑某行, 行变 active 高亮)

```
│  │ 切换中英   ►│ Shift+Space │ ✎ ⟲│    ← 蓝色 active 行 (kAccentPrimary 14% alpha 底)
│  │            │请按快捷键...│      │    ← 占位文字, 灰 italic, kTextSecondary
│  │            │ Esc 取消     │      │    ← 副标题, kTextTertiary 11px
│  │            │ Enter 确认   │      │
```

捕获到 keydown 后:

```
│  │ 切换中英   ►│ Control+Shift+D │ ✎ ⟲│  ← 实时显示当前按下的组合
│  │            │ ⚠ 与「选第 N 候│      │  ← 冲突警告 (若 librime 内置冲突)
│  │            │   选」冲突       │      │     kDestructive 11px + ⓘ 图标
```

### 2.3 自定义快捷键 (新增) 弹窗 (320 × 220, 子 modal)

```
┌─ 添加自定义快捷键 ─────────────────────┐
│                                          │
│  动作类型:  [▾ send______________]      │  ← ComboBox: send / toggle / select
│  动作值:   [_______________________]    │  ← TextBox (e.g. "Page_Down" / "ascii_mode")
│  触发条件:  [▾ composing_________]      │  ← ComboBox: always / composing / has_menu
│                                          │
│  按键:    [Capture______________]  📋   │  ← KeyCapture 子控件, 嵌入式
│                                          │
│              [取消]         [确定]       │
└──────────────────────────────────────────┘
```

---

## 3. State Machine

```
                  ┌──────────────┐
                  │  Init        │  (Load default.custom.yaml → parse bindings)
                  └──────┬───────┘
                         │
                         ▼
       ┌─────────────────────────────────┐
       │  Browse                         │  (显示表格, 可滚动 / 搜索)
       │   ├─ onClickEdit(row)           │
       │   ├─ onClickReset(row)          │
       │   ├─ onClickAddCustom()         │
       │   ├─ onClickResetAll()          │
       │   ├─ onClickSave()              │
       │   ├─ onClickCancel()            │
       │   └─ onSearchChange(q)          │
       └────┬─────────┬─────────┬─────────┘
            │Edit     │+Custom  │Save
            ▼         ▼         ▼
   ┌────────────┐ ┌─────────┐ ┌──────────────┐
   │ Capturing  │ │ AddNew  │ │ Saving       │  (写 yaml + 触发 /deploy)
   │ (row)      │ │ (modal) │ └──────┬───────┘
   └────┬───────┘ └────┬────┘        │
        │Esc/Cancel   │OK             ▼
        │Enter        │Cancel    ┌─────────────┐
        │Backspace    │          │ Browse     │  ← 回到 Browse, status bar 显示
        │ValidKeydown ▼          └─────────────┘     "已保存, deploy 成功"
        │
        ▼
   ┌──────────────────────┐
   │ Confirmed (本地态)   │  → 写 in-memory m_bindings, 回到 Browse
   └──────────────────────┘
```

### 3.1 状态转移表

| From | Event | To | Side effect |
|---|---|---|---|
| Init | 加载失败 | Browse (empty) | status bar warn: "无法读取 default.custom.yaml, 已 fallback 到 default.yaml" |
| Browse | onClickEdit(r) | Capturing(r) | row r 变 active; 启动 keydown hook; 显示占位 "请按快捷键..." |
| Capturing | WM_KEYDOWN valid | Capturing(r) | 实时更新 Combo 文本 + conflict check |
| Capturing | WM_KEYDOWN Backspace | Capturing(r) | 清空当前 Combo, 显示空状态 |
| Capturing | Enter | Browse (edit applied) | 写入 m_bindings[r].combo; status bar "已修改" |
| Capturing | Esc | Browse (no change) | 退出捕获; 还原原 Combo 显示 |
| Browse | onClickReset(r) | Browse | m_bindings[r].combo = m_default[r].combo; row flash 200ms |
| Browse | onClickAddCustom | AddNew modal | 弹出子 modal |
| AddNew | OK + valid | Browse (新增一行) | 追加 m_bindings.back(); sort by when |
| AddNew | Cancel | Browse (no change) | 关闭 modal |
| Browse | onClickResetAll | Browse | m_bindings = m_default; 所有 row flash |
| Browse | onClickSave | Saving | 序列化 + 写 yaml + 触发 /deploy; Save 按钮 disable |
| Browse | onClickCancel | (关闭对话框) | 询问 "未保存的修改将丢失, 确定关闭?" |
| Saving | yaml write 成功 | Browse | status bar "已保存, deploy 已触发"; Save 按钮 enable |
| Saving | yaml write 失败 | Browse | status bar 红色 "保存失败: {reason}, 配置未变更"; 弹出 modal "是否重试?" |

---

## 4. 键盘捕获 UX 详细

### 4.1 进入 Capturing 状态

1. 用户点击某行 **Edit 按钮** (✎)
2. 该行立即获得 active 高亮 (`kAccentPrimary` 14% alpha 背景 + 1px `kAccentPrimary` 左边条)
3. KeyCombo 列原本显示 `Shift+Space`, **替换为** 占位文字 "请按快捷键..." (`kTextSecondary`, italic, 12px)
4. 副标题 (kRowH 的下 16px) 显示提示:
   - "Esc 取消"
   - "Enter 确认"
   - "Backspace 删除"
5. 启动 low-level keyboard hook (`SetWindowsHookEx(WH_KEYBOARD_LL, ...)`), **只捕获本进程的 keydown**, 不抢全局焦点

### 4.2 修饰键处理

- 修饰键 (Ctrl/Shift/Alt/Win) 单独按下 → **忽略**, 不算 valid combo (避免 "用户只按了 Ctrl 就确认")
- 至少 **1 个非修饰键** 才算 valid combo
- 修饰键 + 字符键 (e.g. Ctrl+D) → valid, 显示 `Control+D`
- 修饰键 + 功能键 (e.g. Ctrl+F1) → valid, 显示 `Control+F1`
- 修饰键 + 修饰键 (e.g. Ctrl+Shift) → **忽略**, 等用户继续按
- Esc → 取消捕获, 退出 Capturing, Combo 还原原值
- Enter → 确认当前显示的 Combo (即便 Combo 为空也算 "删除该 binding"), 退出 Capturing
- Backspace → 清空当前 Combo 显示, 状态保持 Capturing, 等用户继续输入

### 4.3 修饰键名映射 (librime YAML schema 格式)

| 按键 | YAML 字符串 |
|---|---|
| `VK_CONTROL` | `Control` (librime 也接受 `Ctrl`, 但 spec 050 用 `Control`) |
| `VK_SHIFT` | `Shift` |
| `VK_MENU` (Alt) | `Alt` |
| `VK_LWIN` / `VK_RWIN` | `Super` (librime 0.17+ 支持, 上游 weasel 默认不用但 yaml 可识别) |
| `VK_ESCAPE` | `Escape` (作为非修饰键的特殊 case, 显示在 combo 中) |

### 4.4 显示格式 (Key Combo 文本)

- 顺序: **modifier 字典序** (`Alt` < `Control` < `Shift` < `Super`), 后跟非修饰键
- 大小写: 字母键大写 (`Shift+a` → `Shift+A`), 数字键保持数字
- 分隔符: `+`, 无空格 (`Control+Shift+D`, 不是 `Control + Shift + D`)
- 例:
  - `Shift+Space`
  - `Control+Shift+D`
  - `Control+1`
  - `Alt+period`
  - `Control+F5`

### 4.5 实时冲突检测 (在 Capturing 状态内)

每次 keydown 后:

```
function CheckConflict(combo, when) -> ConflictReport:
    report = { conflicts: [], warnings: [] }

    # 1. 与本表其他行冲突 (跨 row 重复 combo + when)
    for binding in m_bindings:
        if binding.row != self.row and binding.combo == combo and binding.when == when:
            report.conflicts.push({
                type: 'duplicate',
                action: binding.action_desc,
                when: binding.when,
            })

    # 2. 与 librime 内置 binding 冲突 (静态 db, ~30 条)
    for builtin in BUILTIN_BINDINGS[when]:
        if builtin.combo == combo:
            report.warnings.push({
                type: 'builtin',
                action: builtin.action_desc,
                severity: 'warn',  # 允许 override
            })

    # 3. ⚠️ Shift_L / Shift_R 单键 binding (L19 防御)
    if combo in ('Shift+Shift_L', 'Shift+Shift_R', 'Shift_L', 'Shift_R') and when != 'has_menu':
        report.warnings.push({
            type: 'l19_defense',
            severity: 'error',  # 阻止保存
        })

    # 4. ⚠️ ascii_composer 单键 binding (L18 防御)
    if combo matches /^Shift_L$/ or /^Shift_R$/ and action == 'toggle: ascii_mode':
        report.warnings.push({
            type: 'l18_defense',
            severity: 'error',
        })

    return report
```

UI 反馈:

- **0 conflict, 0 warning**: 副标题显示 "✓ 无冲突", `kSuccess` 绿色 11px
- **1+ conflict (duplicate)**: 副标题显示 "⚠ 与「{action}」冲突, 仍可保存", `kDestructive` 红 + ⓘ 图标
- **1+ warning (builtin)**: 副标题显示 "⚠ 覆盖默认行为「{action}」", `kWarning` 橙
- **1+ error (L18/L19)**: 副标题显示 "✗ 此组合不被允许 (Shift_L/R 单键)", `kDestructive` 红 + 阻止保存

### 4.6 退出 Capturing

- 用户按 Enter → `m_bindings[self.row].combo = current_combo`, 退出, status bar 闪 "已修改 (未保存)"
- 用户按 Esc → 丢弃本次修改, 退出, row 恢复
- 用户关闭对话框 → 同 Esc 处理

---

## 5. YAML Schema

### 5.1 实际写入路径

**关键决策** (L40 教训 + spec 050 §1.4): 编辑器**不**改 `output/data/weasel.yaml`, 改的是 `<user_data_dir>/default.custom.yaml`. 这样:

1. 升级时用户的 custom 不会被默认配置覆盖
2. 与 librime 用户配置层 (custom > default) 优先级机制一致
3. 不污染上游 weasel schema

`<user_data_dir>` = `RimeGetUserDataDir()` 在 Windows 上通常是 `%APPDATA%\Rime` (spec 050 §0)

### 5.2 当前 default.yaml 格式 (librime 标准)

```yaml
# default.yaml (上游 librime, 只读参考)
patch:
  key_binder/bindings:
    - { when: composing, accept: "Shift+Tab", send: Shift+Left }
    - { when: composing, accept: Tab, send: Shift+Right }
    - { when: has_menu, accept: comma, send: Page_Up }
    - { when: has_menu, accept: period, send: Page_Down }
    - { when: has_menu, accept: "Control+1", send: 2 }
    - { when: has_menu, accept: "Control+2", send: 3 }
    - { when: always, accept: "Shift+space", toggle: ascii_mode }
    - { when: always, accept: "Control+Shift+9", toggle: ascii_punct }
    - { when: always, accept: "Control+Shift+0", toggle: traditionalization }
```

### 5.3 编辑器落盘的 default.custom.yaml (同一格式, 增量 patch)

```yaml
# default.custom.yaml (用户层, 编辑器唯一写入目标)
patch:
  key_binder/bindings:
    # 用户覆盖 default.yaml 的「切中英」从 Shift+space 改为 Control+Shift+space
    - { when: always, accept: "Control+Shift+space", toggle: ascii_mode }
    # 用户新增: 常用短语
    - { when: always, accept: "Alt+period", send: "X11::Phrases/X11" }
    # 用户删除 default 里的「Control+1」(让出给别的 app)
    # 用 placeholder "" 实现, 或保留并 toggle 一条 "noop"
```

### 5.4 Action 描述映射 (UI 显示 ↔ yaml 实际值)

UI 显示人话, 内部存 yaml 格式. UI 维护一张静态 `ActionDesc` 表:

| UI Action | YAML 字段 |
|---|---|
| "切换中英" | `toggle: ascii_mode` |
| "全/半角切换" | `toggle: full_shape` (待定, 与 ascii_mode 关系见 spec 042 / L18) |
| "ASCII 模式" | `toggle: ascii_mode` (与「切换中英」等价, 不同 when) |
| "翻页 - 上页" | `accept: comma, send: Page_Up, when: has_menu` |
| "翻页 - 下页" | `accept: period, send: Page_Down, when: has_menu` |
| "选第 2 候选" | `accept: Control+1, send: 2, when: has_menu` |
| "选第 3 候选" | `accept: Control+2, send: 3, when: has_menu` |
| "中英标点切换" | `toggle: ascii_punct, accept: Control+Shift+9, when: always` |
| "简繁切换" | `toggle: traditionalization, accept: Control+Shift+0, when: always` |
| "用户词典" | (自定义 action, UI 加 hint) |
| "常用短语" | (自定义 action, Alt+.) |
| "重新部署" | (自定义 action, Ctrl+F5) |

> 用户自定义行: action desc 字段为空, 但 yaml 完整保留 raw accept/send/toggle, UI 显示原始 yaml 字符串作为 desc fallback.

### 5.5 YamlRoundTrip 集成 (spec 024)

```
editor.Save():
  1. m_yaml = YamlRoundTrip.Load("<user_data_dir>/default.custom.yaml")
     - 若不存在 → 创建空 {"patch": {"key_binder/bindings": []}}
  2. m_yaml["patch"]["key_binder/bindings"] = m_bindings (List<HotkeyBinding>)
  3. YamlRoundTrip.WriteString(m_yaml)
  4. 写回 default.custom.yaml (UTF-8 + BOM + CRLF, L09 教训)
  5. SpawnProcess("WeaselDeployer.exe /deploy")
  6. status bar: "已保存, deploy 已触发, 新快捷键立即生效"
```

---

## 6. 冲突检测算法

### 6.1 静态内置 binding db (librime 0.17+ 默认 ~30 条)

```cpp
struct BuiltinBinding {
    std::string combo;
    std::string when;       // always / composing / has_menu / paging
    std::string action;     // "select 1" / "commit_comment" / etc.
    std::string desc_zh;    // "选第 1 候选" / "提交注释"
};

// 静态 db 来源: librime source `src/key_table.cc` + weasel patch
// 注: 完整列表由 spec 050 T002 维护, 这里只列高频冲突
const BuiltinBinding kBuiltinBindings[] = {
    // always
    { "Control+space",   "always",    "commit_code",   "上屏编码" },
    { "Escape",          "always",    "clear",          "清空候选" },
    { "Return",          "always",    "commit",         "回车上屏" },
    { "Control+Return",  "always",    "commit",         "Ctrl+Enter 上屏" },
    { "BackSpace",       "always",    "backspace",      "退格" },
    { "Delete",          "always",    "delete",         "删除" },
    // composing
    { "Shift+Left",      "composing", "cursor_left",    "光标左移" },
    { "Shift+Right",     "composing", "cursor_right",   "光标右移" },
    { "Home",            "composing", "cursor_home",    "光标到首" },
    { "End",             "composing", "cursor_end",     "光标到尾" },
    // has_menu
    { "1",               "has_menu",  "select 1",       "选第 1 候选" },
    { "2",               "has_menu",  "select 2",       "选第 2 候选" },
    { "3",               "has_menu",  "select 3",       "选第 3 候选" },
    { "Tab",             "has_menu",  "select_next",    "下个候选" },
    // paging (extracted from has_menu 模式)
    { "comma",           "paging",    "Page_Up",        "翻页上" },
    { "period",          "paging",    "Page_Down",      "翻页下" },
    // ... 完整列表见 spec 050 T002
};
```

### 6.2 检测算法 (linear scan, db 规模 < 50)

```
conflict_score(combo, when, action) -> { score, report[] }:
    # score: 0 = no conflict, 1 = warn (builtin), 2 = error (L18/L19 / hard conflict)
    if ComboMatchL18L19Defense(combo, action):
        return 2, [{ type: 'l18_l19', severity: 'error' }]

    if DuplicateInTable(combo, when, exclude_self=true):
        return 2, [{ type: 'duplicate', severity: 'error' }]

    builtin = FindBuiltinBinding(combo, when)
    if builtin:
        return 1, [{ type: 'builtin', severity: 'warn', action: builtin.desc_zh }]

    return 0, []
```

### 6.3 冲突处理 UX

| Score | UI | Save 行为 |
|---|---|---|
| 0 (无冲突) | 副标题 "✓ 无冲突" `kSuccess` | 允许 save |
| 1 (builtin warn) | 副标题 "⚠ 覆盖默认行为「{desc}」" `kWarning` | 允许 save (override) |
| 2 (hard error) | 副标题 "✗ {reason}" `kDestructive` | **disable Save 按钮** + 红色 ⓘ |

---

## 7. weasel.yaml reload 流程

### 7.1 保存后的部署链

```
[Save clicked]
  ↓
[YAML write] default.custom.yaml
  ↓
[SpawnProcess] WeaselDeployer.exe /deploy
  ↓
[librime RimeApi::deploy]  ← 实际接管: WeaselServer 检测 custom.yaml 变化
  ↓ (可选) WeaselServer::Reinit 或直接 RimeApi::process_key
  ↓
[新 binding 生效]          ← 候选框里直接 Ctrl+1 验证
```

### 7.2 失败处理

| 步骤 | 失败信号 | UI 反应 |
|---|---|---|
| YAML write | `WriteFile` 返回 ERROR_ACCESS_DENIED | status bar 红: "无法写入 default.custom.yaml (权限不足), 检查 %APPDATA%\Rime 目录权限"; 不触发 deploy |
| YAML write | YAML 解析错误 (e.g. 字段缺失) | status bar 红: "生成的 yaml 无效: {reason}, 已回滚到内存"; 不触发 deploy |
| SpawnProcess | WeaselDeployer.exe 路径不存在 | status bar 红: "WeaselDeployer.exe 未找到, 请重新安装 Fluxing"; 弹 modal 提示 |
| deploy | WeaselServer 返回失败 | status bar 红: "deploy 失败: {reason}, 重启 WeaselServer 生效"; 不回滚 yaml (用户可手动验证) |

### 7.3 验证回环

保存成功后, UI 提供 "立即验证" 按钮 (status bar 旁边, 仅在 deploy 成功时显示), 用户点击 → 模拟一次按键 (SendInput) 触发新绑定, 若 librime 响应正确则 status bar "✓ 验证通过", 否则 "✗ 验证失败, 请检查 librime 日志".

---

## 8. 关键决策表

| 决策点 | 选择 | 理由 | 替代方案 |
|---|---|---|---|
| 编辑器入口 | QuickPanel 设置按钮 (spec 040 L94 已 ship) | 与 PhrasesDialog / UserDictionary 入口统一, 视觉一致 | Alt+, 全局热键 (冲突多, 不推荐) |
| 编辑器进程 | 独立 exe (`FluxingConfigEditor.exe`) | spec 050 §2.1 决策, 与 deployer 解耦; PPL 风险规避 | WeaselServer 内嵌 modal (改动大, 风险高) |
| 落盘路径 | `<user_data_dir>/default.custom.yaml` | L40 教训 (升级不被覆盖) + spec 050 §1.4 | weasel.yaml (污染 RIME schema, 升级丢失) |
| 触发 reload | `WeaselDeployer.exe /deploy` | spec 050 §3 数据流, 现有 IPC | 重启 WeaselServer (PPL 不可靠, L17/L18) |
| chrome 复用 | 复用 spec 040 §3.6 PhrasesDialog modal 体系 | 视觉一致 + token 不重定义 | 自创 chrome (增加 token, 违反 FLUENT-UI-TOKENS.md 铁律 1) |
| 冲突检测 scope | librime 内置 + 同表 cross-row | spec 050 §1.4 P2 scope, 实现可行 | OS-level (P3, 引入 OS hook 风险) |
| KeyCapture 实现 | `WH_KEYBOARD_LL` low-level hook + `MSGFilter` only-in-process | 只听本进程, 不抢全局焦点 | RegisterHotKey 全局 (冲突 OS, 难调试) |
| 冲突 UX | 警告 + 允许 override | 用户最终决定权, librime 默认行为可改 | 硬阻止 (限制用户) |
| 搜索算法 | substring match (case-insensitive) on action desc + combo text | 简单可预测, < 100 行 | fuzzy match (YAGNI) |
| Reset 全部 | m_bindings = m_default (硬重置, 弹 modal "确定丢弃所有自定义?") | 用户预期清晰 | soft 重置 + 单步 undo (复杂) |
| 状态持久化 | 不持久化 Capturing 状态 (关闭对话框即丢失) | 简化, 用户可重做 | 持久化 + 启动恢复 (过度设计) |

---

## 9. Token 增量

### 9.1 新增 token (录入 FLUENT-UI-TOKENS.md §3.1 Color)

| Token | Value (light) | Value (dark) | 当前位置 | 来源 |
|---|---|---|---|---|
| `color.scheme.hotkey.conflict_bg` | `rgba(208, 69, 69, 0.14)` (浅红底) | `rgba(208, 69, 69, 0.18)` (深红底, 微强) | 暂无 | WE-PICK (Mac HIG destructive-with-alpha 等价) |
| `color.scheme.hotkey.warning_text` | `RGB(196, 110, 28)` | `RGB(255, 176, 96)` | 暂无 | WE-PICK (FLUENT Warning 颜色近似) |
| `color.scheme.hotkey.success_text` | `RGB(36, 138, 61)` | `RGB(86, 201, 113)` | 暂无 | WE-PICK (FLUENT Success 颜色近似) |

> `color.destructive` 已在 FLUENT-UI-TOKENS §3.1 收录 (`#D04545`), 不重复; `conflict_bg` 是 destructive 的 14% alpha 底, 区分纯红字 vs 红底两种语义.

### 9.2 复用既有 token (引用即可)

| Token | 复用位置 | spec 040 § |
|---|---|---|
| `size.modal.dialog.w` / `h` | ShortcutSettings 主对话框 520×560 | §3.6 (新值 520 / 560, 录入) |
| `size.modal.btn.w` / `h` | 4 个底部按钮 88×32 | §3.6 |
| `space.modal.btn.gap` / `margin_x` | 按钮间距 8 / 12 | §3.6 |
| `color.modal.bg.top` / `bot` | 主对话框玻璃底 | §3.6 |
| `color.modal.text` | 主文本 | §3.6 |
| `color.modal.sel_bg` | active row (kAccentPrimary 14% alpha 替代? 或复用? 待 spec 040 §3.1 二次确认) | §3.6 |
| `radius.md` / `lg` | 输入框 / 按钮 / 主对话框 | §3.4 |
| `space.md` / `lg` | 行内 padding / 按钮间距 | §3.3 |
| `font.ui.size.body` / `small` / `label` | 主文本 / 副标题 / 表头 | §3.2 |
| `elevation.glass.panel` | 主对话框玻璃背景 | §3.5 |
| `elevation.shadow.modal` | 主对话框阴影 | §3.5 |
| `elevation.flat` | row / 按钮 flat 状态 | §3.5 |

### 9.3 size.modal.dialog 新增 (520×560 vs 现有 360×420)

- 当前 `size.modal.dialog.w` = 360, `h` = 420 (PhrasesDialog)
- ShortcutSettings 需求: 12+ 行 × 32px 行高 + 标题 + 搜索 + 按钮栏 ≈ 560, 宽度 520 (含 4 列 + padding)
- 决策: **新增** `size.modal.shortcut.w` = 520, `size.modal.shortcut.h` = 560, 不覆盖 PhrasesDialog 的 360×420 (chrome 一致但尺寸可异)
- 录入 FLUENT-UI-TOKENS.md §3.6

---

## 10. 文件改动清单

> 仅 design 阶段预演, implementation 阶段由 spec 050 + 本 spec 合并的 implementation spec 实际执行

### 10.1 新建 (UI 设计层, 不在 design 范围)

- `FluxingConfigEditor/ShortcutSettingsDialog.{h,cpp}` - 主对话框 (spec 045 设计输入)
- `FluxingConfigEditor/KeyCapture.{h,cpp}` - 键捕获子控件
- `FluxingConfigEditor/HotkeyTableView.{h,cpp}` - 表格视图 (CListViewCtrl 子类化, 12 行)
- `FluxingConfigEditor/HotkeyBinding.h` - 数据结构 + Action 描述表
- `FluxingConfigEditor/ConflictChecker.{h,cpp}` - §6 静态 db + 算法
- `FluxingConfigEditor/BuiltinBindings.h` - 静态 ~30 条内置 binding
- `FluxingConfigEditor/YamlIO.{h,cpp}` - 包装 YamlRoundTrip (spec 024)
- `test/TestShortcutSettings/` - 行为级测试 (10+ test case)

### 10.2 修改

- `docs/design/FLUENT-UI-TOKENS.md` - 录入 §9.1 / §9.3 新增 token
- `.specify/specs/050-yaml-hotkey-editor-mvp/spec.md` - 引用本 spec §3 / §4 / §5 (visual / UX overlay)
- `CHANGELOG.md` - 新增「快捷键设置 UI」v0.19.0.27 entry (P5)
- `output/install.nsi` - 注册 `FluxingConfigEditor.exe` 到 deploy (deployer 派生子进程)

### 10.3 不改

- `include/WeaselIPCData.h` - 编辑器走独立 exe, **不**走 IPC (与 spec 050 §2.2 一致)
- `output/data/weasel.yaml` - 编辑器不直接改 (L40 教训)
- `librime/` - 不动 librime 内部

---

## 11. 测试矩阵

| # | 场景 | 输入 | 期望输出 | 验证手段 |
|---|---|---|---|---|
| T01 | 加载 default.yaml 全部 binding | 全新安装, default.yaml 12 条 | UI 显示 12 行, Action / Key Combo 正确 | TestShortcutSettings::TestLoadDefaults |
| T02 | 加载 custom.yaml override | default.yaml + custom.yaml 3 条 | UI 显示 15 行, custom 标记 "✎ 已自定义" 角标 | TestShortcutSettings::TestLoadCustomOverride |
| T03 | YAML 解析失败 fallback | default.custom.yaml 损坏 | UI 显示 0 行 + status bar warn, 不 crash | TestShortcutSettings::TestParseFailureFallback |
| T04 | 搜索过滤 | "翻页" | 只显示 "翻页 - 上页" / "翻页 - 下页" 两行 | TestShortcutSettings::TestSearchFilter |
| T05 | 搜索大小写 | "SHIFT" | 显示所有 shift 开头的 combo | TestShortcutSettings::TestSearchCaseInsensitive |
| T06 | Edit 行 → 捕获 Ctrl+Shift+D | 按下 Ctrl+Shift+D | 显示 "Control+Shift+D", 无冲突, Enter 确认 | TestShortcutSettings::TestCaptureValidCombo |
| T07 | Edit 行 → Esc 取消 | 按 Esc | Combo 还原原值, 不写 m_bindings | TestShortcutSettings::TestCaptureEscapeCancel |
| T08 | Edit 行 → Backspace 清空 | 按 Backspace | Combo 显示空, Capturing 状态保持 | TestShortcutSettings::TestCaptureBackspace |
| T09 | 修饰键单独按下 | 只按 Ctrl | 不算 valid, 状态保持 Capturing, 等下一键 | TestShortcutSettings::TestCaptureModifierOnly |
| T10 | 冲突检测: 重复 combo | 试图把 "切中英" 绑到 Ctrl+1 | status "✗ 与「选第 2 候选」冲突", Save disable | TestShortcutSettings::TestConflictDuplicate |
| T11 | 冲突检测: 覆盖默认 | 把 "Control+space" 改成 "commit_code" | status "⚠ 覆盖默认行为「上屏编码」", Save enable | TestShortcutSettings::TestConflictBuiltinOverride |
| T12 | L18/L19 防御 | 试图绑 "Shift_L" 单键 ascii_mode toggle | status "✗ 此组合不被允许", Save disable | TestShortcutSettings::TestL18L19Defense |
| T13 | Reset 单行 | 点某行 ⟳ | Combo 还原 default.yaml 原值 | TestShortcutSettings::TestResetRow |
| T14 | Reset 全部 | 点底部 "⟳ 重置全部" | 弹 modal 确认 → m_bindings = m_default | TestShortcutSettings::TestResetAll |
| T15 | 添加自定义 | 弹 modal + 填 "Send Page_Down" + 按 Alt+Right | UI 末尾新增一行 | TestShortcutSettings::TestAddCustom |
| T16 | 保存成功 | 点保存 | 写 default.custom.yaml + spawn /deploy, status "已保存" | TestShortcutSettings::TestSaveSuccess |
| T17 | 保存失败: 权限拒绝 | default.custom.yaml read-only | status 红 "权限不足", 不触发 deploy, m_bindings 不变 | TestShortcutSettings::TestSavePermissionDenied |
| T18 | 保存后立即验证 | "立即验证" 按钮 | SendInput 触发新 combo, 验证 librime 响应 | TestShortcutSettings::TestVerifyAfterSave |
| T19 | Cancel 不写盘 | 点取消 | 弹 modal "未保存的修改将丢失", 确认后关闭 | TestShortcutSettings::TestCancelNoWrite |
| T20 | dark mode 切换 | 系统 dark mode toggle | ShortcutSettings 立即重新渲染 dark token | TestShortcutSettings::TestDarkModeSwitch (集成 spec 033 bridge) |
| T21 | DPI 144% | 144% DPI 屏幕 | 表格 / 按钮 / 文本不模糊, 物理像素对齐 (spec 041 L41) | TestShortcutSettings::TestDpi144 |
| T22 | 关闭对话框不退出 WeaselServer | 关闭 ShortcutSettings | WeaselServer.exe 仍在 tray, 候选框正常 | TestShortcutSettings::TestNoImpactOnWeaselServer |

### 11.1 L-Defense 矩阵 (沿用 spec 050 §5 教训)

| L## | 教训 | 编辑器如何防御 |
|---|---|---|
| L09 | install.nsi UTF-8 + BOM + CRLF | 编辑器写 yaml 用相同规范 (YAML 解析测试覆盖) |
| L17 / L18 | WeaselServer PPL 杀不掉 | 编辑器独立 exe, 不影响 PPL |
| L19 | Shift_L / Shift_R 单键 release event 误匹配 | ConflictChecker §6.1 静态 db 阻止此类 binding |
| L40 | default.yaml 升级覆盖 | 编辑器写 default.custom.yaml, 不污染 default |
| L66 | NSIS reg write 嵌套 If 失败 | (不适用, 编辑器是 exe, 不写 NSIS) |

---

## 12. Risk / Trade-off

| Risk | Severity | Mitigation |
|---|---|---|
| librime 内置 binding db 不完整, 用户覆盖后才发现冲突 | M | db 维护由 spec 050 T002 持续更新; UI 提示 "此 db 可能不完整, 验证请在候选框实测" |
| WH_KEYBOARD_LL 在某些 app (e.g. 全屏游戏) 失效 | L | UI 提示 "请在非全屏应用下使用键捕获"; 备选手动输入 combo (textbox) |
| WeaselDeployer.exe /deploy 耗时 > 5s | L | status bar 显示进度 spinner, Save 按钮 disable 期间不允许多次点击 |
| 用户开了多显示器, 弹窗位置错位 | M | 居中到主显示器 (CW_USEDEFAULT + CenterWindow) |
| 144 DPI + per-pixel alpha 玻璃背景渲染慢 | L | spec 041 已 ship L41, 走既有路径 |
| 用户 hotkey 撞 OS 已有 binding (e.g. Win+L 锁屏) | M | UI 显式警告 "⚠ 此组合可能被操作系统或其他 app 占用"; 但不阻止 save (override 用户决定) |
| KeyCapture 抢了用户原本想输入的键 (e.g. 正在聊天时按了 Ctrl+Shift+D) | L | hook 只在 ShortcutSettings 窗口 active 时注册, 关闭即注销 |
| YAML key 顺序写乱 | L | 走 YamlRoundTrip (spec 024 已 ship 保留 key order) |
| 表格性能: 100+ 自定义 binding | L | 暂不优化; v2 加 virtual list; spec 042 经验 < 50 行很常见 |

---

## 13. Anti-patterns

### AP-045-A: 编辑器改 weasel.yaml 而非 default.custom.yaml

❌ 错误: `YamlRoundTrip.WriteString("weasel.yaml", m_bindings)`
✅ 正确: `YamlRoundTrip.WriteString("default.custom.yaml", m_bindings)` — 升级不被覆盖 (L40)

### AP-045-B: 在 keydown hook 里调 SendInput 或触发 IPC

❌ 错误: 捕获到 Ctrl+Shift+D → `WeaselIPC::Client::Hotkey(...)` 模拟触发
✅ 正确: 捕获仅用于显示 Combo 文本, **不**实际触发; 用户自己到候选框验证

### AP-045-C: 把 librime 全部 binding 列表硬编码 200+ 行

❌ 错误: 在 ConflictChecker.h 里写 200 行 `if (combo == "...")`
✅ 正确: 静态 `kBuiltinBindings[]` 表 + lookup; 缺失的 binding 让用户实测, db 持续更新 (spec 050 T002)

### AP-045-D: 键捕获时不区分修饰键 / 非修饰键

❌ 错误: 用户只按了 Ctrl → 立刻确认 "Control" 作为 combo
✅ 正确: 至少 1 个非修饰键才算 valid (§4.2)

### AP-045-E: Save 失败后回滚用户编辑

❌ 错误: yaml write 失败 → 清空 m_bindings 让用户重新输入
✅ 正确: m_bindings 是 UI 内状态, 与磁盘无关; 写失败只提示用户, 不破坏 UI 内状态

### AP-045-F: 把 chrome 颜色 / 几何写死在 .cpp

❌ 错误: `static constexpr COLORREF kRowActiveBg = RGB(...)`
✅ 正确: 引用 FLUENT-UI-TOKENS.md §3.1 / §3.3 token, 不写硬编码 (token 铁律 2)

### AP-045-G: 引入 yaml-cpp 依赖解析 default.custom.yaml

❌ 错误: link yaml-cpp + 解析
✅ 正确: 走既有 YamlRoundTrip (spec 024) 或 spec 042 手写 minimal parser

### AP-045-H: 在 dark mode bridge 未就绪时启动编辑器

❌ 错误: 直接 hardcode 颜色, dark mode 下亮瞎眼
✅ 正确: 通过 FluxingTheme (spec 037) 读取 palette, 跟随系统 dark mode (spec 033 bridge)

### AP-045-I: 重置全部时不弹确认

❌ 错误: 直接 m_bindings = m_default, 一键抹掉用户辛苦的设置
✅ 正确: 弹 modal "将丢弃所有自定义快捷键, 确定?" + 显示影响行数

### AP-045-J: 键捕获时 hook 全局

❌ 错误: `SetWindowsHookEx(WH_KEYBOARD_LL, ...)` 不区分线程
✅ 正确: hook 限定 ShortcutSettings 窗口所在线程, 关闭即注销 (P2 + 抢焦点风险)

---

## 14. 关联

- spec 007 `yaml-config-ui` (完整 4 页蓝图, 本 spec 是第 1 页子集)
- spec 024 `yaml-round-trip-key-order-preserve` (YamlRoundTrip)
- spec 033 `fluxing-dark-mode-bridge` (dark mode 联动)
- spec 037 `fluxing-components-v0` (UI 控件可复用)
- spec 040 `fluent-ui-tokens` (本 spec 的视觉 token 来源)
- spec 042 `phrases-ui` (chrome / YAML / 树形 UX 模板)
- spec 050 `yaml-hotkey-editor-mvp` (数据 / 落盘 / IPC 逻辑, 本 spec 是 visual + UX overlay)
- `.specify/memory/constitution.md` P1-P8 (尤其 P2 「不开 TSF 阻塞 I/O」+ P5 「CHANGELOG」)
- `.specify/memory/lessons-learned.md` L09 / L17 / L18 / L19 / L21 / L40 / L66

---

## 15. Open Questions (待 user / spec 040 / spec 050 确认)

1. **搜索框是否支持 fuzzy match?** v1 用 substring, v2 加 fuzzy?
2. **自定义 binding 是否需要 "立即验证" 按钮?** 还是只依赖 status bar "已保存" 提示?
3. **dark mode 切换时, ShortcutSettings 是否要淡入淡出?** spec 039 动画未 ship, 暂用瞬切?
4. **CSV / TSV 导出?** 用户能否导出当前 binding 列表? spec 055 推迟?
5. **QuickPanel 入口位置**: 「设置」按钮 (⚙) 当前 spec 040 L94 ship 在 panel 第 5 位 (末尾), 是否前移到更显眼位置?

---

*Last updated: 2026-07-13 (Draft v0) · Owner: UI Design Agent (ShortcutSettings)*