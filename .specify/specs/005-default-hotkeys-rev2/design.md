# 005 · 火流猩输入法 v2 · 默认快捷键 rev2

> 范围：把 `output/data/default.yaml` 的 4 类快捷键（翻页、切换键、中英文切换、候选字上屏）替换为用户指定的新值。
> 不动 RIME 引擎子模块；不改 key_binder 之外的键位（如 `speller/algebra`）。

## 0. 上下文

- 当前 `output/data/default.yaml` 是 RIME 社区默认 + 本仓库的派生配置。
- spec 002 之后，用户数据目录已迁至 `%LocalAppData%\Fluxing`，但 `output/data/default.yaml`（位于 `%ProgramFiles%\Fluxing\weasel\data`）作为"出厂默认"保留。
- 用户首次安装使用此默认；后续可通过 spec 007 UI 或手动编辑覆盖。
- 本 spec 范围 = 改 `output/data/default.yaml` 的 4 类键位，**不动 `weasel.yaml`、`*.schema.yaml`、RIME 引擎子模块**。

## 1. 产品视角（PRD 段）

### 1.1 目标

让 Fluxing v2.0.0 开箱即用以下快捷键：

| 功能 | 旧默认 | 新默认 | 备注 |
|---|---|---|---|
| 翻页（下一页 / Page_Down） | `=` | **`.`** (period) | 主键盘区；`.`=前进 / `,`=回看（英文阅读习惯） |
| 翻页（上一页 / Page_Up） | `-` | **`,`** (comma) | 同上 |
| 切换中英标点 | `Control+Shift+3` | **`Control+Shift+9`** | 与简繁切换相邻 |
| 切换简繁 | `Control+Shift+4` | **`Control+Shift+0`** | 与中英标点相邻 |
| **中英文切换** | `Shift_L`（commit_code） / `Shift_R`（noop） | **`Shift+space`** | L18 修复：单键 Shift 切中英会与 `shift+=` / `shift+Enter` 等 release 事件冲突 |
| 选择第 2 候选 | `Control+Shift+1` 等组合（无） | **`Shift_L`**（单键，has_menu 时） | 搜狗拼音习惯 |
| 选择第 3 候选 | （无） | **`Shift_R`**（单键，has_menu 时） | 搜狗拼音习惯 |

**键位设计决策记录**：

- **`.` = 下页 / `,` = 上页**：用户 2026-07-01 拍板。中文"分别使用 period. 和 comma ,"的语义。
  这与 vim 风格（h/j/k/l 或 n/N）不同，但与英文阅读习惯（逗号停顿=回看，句号=前进）一致；
  旧 `=` / `-` 是数字键区，在 mac 风格笔记本键盘上没有，弃用。
- **`Shift+space` 切中英**（L18 修复）：原始设计是单键 `Shift_L` / `Shift_R` 切中英（commit `d30f69c`），
  但用户 2026-07-01 实测发现 `shift+=` / `shift+Enter` / `shift+<letter>` 等组合键的 release 事件
  会被 key_binder 误匹配切中英或选候选，原因是 TSF 注入的 release 事件 `keycode=Shift_L` 与
  单键 Shift binding 的 `accept: Shift+Shift_L` 冲突。改为 `Shift+space` 后键码独立（`space` ≠ `Shift_L`），
  release 事件不再误匹配。详见 L18。
- **has_menu 时 `Shift+Shift_L` 选第 2 候选** 仍保留（用户认可搜狗拼音习惯，has_menu 时 `Shift_L` 不会被其他键的
  release 事件干扰，因为候选窗打开时 `shift+<other>` 走的是候选窗的菜单键处理，与 key_binder 解耦）。
- **`ascii_composer.switch_key.Shift_L/R: noop`**：让 ascii_composer 不抢 Shift 键事件，
  全权交给 key_binder 决定如何响应（切中英 / 选候选）。`load_bindings:37-38` 跳过 noop，
  `ToggleAsciiModeWithKey` 返回 false（L18 验证）。

### 1.2 用户故事

- **US1-A** [P1]：用户全新安装 Fluxing v2.0.0，在任意输入框打字出现候选后按 `,` 翻到下一页；按 `.` 翻回上一页。
- **US1-B** [P1]：在中文输入态按 `Shift+space` 切到英文（status 指示器变 `A`）；再按 `Shift+space` 切回中文（L18）。在有 2 个以上候选时按 `Shift_L` 上屏第 2 候选；按 `Shift_R` 上屏第 3 候选（候选消失，输入栏插入该候选文字）。
- **US1-C** [P1]：按 `Ctrl+Shift+9` 切换中英标点（中文 → 英文标点或反之）；按 `Ctrl+Shift+0` 切换简繁。
- **US1-D** [P1]：以上快捷键在 spec 007 UI 中可被查看和编辑。

### 1.3 验收（Given/When/Then）

- Given 全新安装 Fluxing v2.0.0
- When 用户在中文输入态连续按 `.` 5 次（`.` = Page_Down，下页）
- Then 候选窗每按一次翻到下一页（不超界）

- When 用户连续按 `,` 5 次（`,` = Page_Up，上页）
- Then 候选窗每按一次翻到上一页（不超界）

- Given 输入法处于中文模式且候选窗有 3 个候选
- When 用户按 `Shift_L`
- Then 第 2 候选上屏，候选窗关闭

- Given 输入法处于中文模式但无候选（光标前未输入字符）
- When 用户按 `Shift+space`（L18）
- Then 输入法切到英文模式（status 指示器变 `A`）

- **L18 回归**：当用户按 `shift+=`、`shift+Enter`、`shift+<letter>` 等组合键时
  - Then ascii_mode **不**被误切换，`+`/`<Enter>`/`<letter>` 正常输入到 focus app

- Given 任意输入态
- When 用户按 `Ctrl+Shift+0`
- Then 输入法在"原字 / 简体 / 繁体"三态间循环（当前为 cycle 行为）

## 2. 技术视角（TDD 段）

### 2.1 改动文件

| 文件 | 改动 |
|---|---|
| `output/data/default.yaml` | 改 `key_binder.bindings` / `ascii_composer/switch_key` / `switcher.hotkeys` / `key_binder.select_candidate`（如存在） |

### 2.2 default.yaml 当前形态（与 0.18.5+ shipped 版本一致）

> **重要**：以下样例与 `output/data/default.yaml` 的 **0.18.5+ shipped 版本** 1:1 对齐。
> 任何修改必须同时更新 `output/data/default.yaml` + `test/TestDefaultHotkeys.cpp` + 本节。
> 早期版本（0.18.0-0.18.4）曾使用 `set_option:ascii_punct` / `set_option:simplification` / `Page_Page_Down` / `shift+l` (小写) / `commit_code` 等错误形式，已在 0.18.4 → 0.18.5 间逐次修复（L16 + L18）。

```yaml
# output/data/default.yaml 中与 spec 005 相关的 key_binder 段（精简版，去掉无关行）

key_binder:
  bindings:
    # 翻页 vim 风格（用户拍板：, = 上页 Page_Up, . = 下页 Page_Down）
    - { when: has_menu, accept: comma, send: Page_Up }
    - { when: has_menu, accept: period, send: Page_Down }

    # Shift_L/R 单键：has_menu 时选候选 2/3（搜狗拼音习惯）
    - { when: has_menu, accept: Shift+Shift_L, send: 2 }
    - { when: has_menu, accept: Shift+Shift_R, send: 3 }

    # Shift+space 切中英（L18 修复：单键 Shift 切中英会与 shift+= / shift+Enter 的 release 事件冲突）
    - { when: always, toggle: ascii_mode, accept: Shift+space }

    # 中英标点 / 简繁（toggle: 标准 RIME 语法）
    - { when: always, toggle: ascii_punct, accept: Control+Shift+9 }
    - { when: always, toggle: traditionalization, accept: Control+Shift+0 }

ascii_composer:
  good_old_caps_lock: true
  switch_key:
    Caps_Lock: clear
    Shift_L: noop         # 让 key_binder 接管单键 Shift 事件
    Shift_R: noop
    Control_L: noop
    Control_R: noop
```

> **优先级说明**：`key_binder/bindings` 按列表顺序匹配，第一个匹配命中即停止。
> - `has_menu: Shift+Shift_L, send: 2` 必须在 `always: Shift+space, toggle ascii_mode` 之前 — 这样候选窗打开时单键 Shift 选候选 2，候选窗关闭时 Shift+space 切中英。
> - `toggle: ascii_mode` 是 RIME 1.13+ 的 key_binder 4 种 action type 之一（见 L04）。`set_option:<name>` 不是合法 action；正确方式是 `toggle: <option_name>`。

### 2.3 验证步骤

1. **TDD（test）**：`test/TestDefaultHotkeys.cpp`（已存在，25/25 PASS）：
   - 翻页方向：`accept: comma, send: Page_Up` 与 `accept: period, send: Page_Down`。
   - 标点/简繁：`toggle: ascii_punct, accept: Control+Shift+9` 与 `toggle: traditionalization, accept: Control+Shift+0`。
   - Shift_L/R 选候选：`has_menu: accept: Shift+Shift_L, send: 2` 与 `Shift+Shift_R, send: 3`。
   - **Shift+space 切中英**：`always: toggle: ascii_mode, accept: Shift+space`（L18 修复）。
   - 顺序检查：has_menu 的 Shift+Shift_L 必须在 Shift+space 切中英之前。
   - 负断言：`Shift+Shift_L/R` ascii_mode 已移除（L18）；`Shift+l/r` 组合键 ascii_mode 已移除（L16）。
2. **构建**：`xbuild.bat weasel` → 0 错误（已验证 2026-07-01）。
3. **deploy**：`rime_deployer.exe /deploy`（确保 RIME 引擎接受新 default.yaml）。
4. **手动验证**（Win10 22H2 / Win11 23H2 / DPI 100/150/200 各一遍）：
   - 在 notepad 打字"nihao"，候选"你好/ni hao/泥好/拟好/..."，按 `,` 翻页（Page_Up），按 `.` 翻页（Page_Down）。
   - 按 `Shift_L` 选第 2 候选；按 `Shift_R` 选第 3 候选（候选窗打开时）。
   - 按 `Shift+space` 切到英文；再按 `Shift+space` 切回中文。
   - **回归 L18**：按 `shift+=`、`shift+Enter` 后 ascii_mode **不**被误切换。
   - 按 `Ctrl+Shift+9` 切中英标点；按 `Ctrl+Shift+0` 切简繁。
5. **回归**：
   - 旧的 `-` / `=` 翻页**不再生效**（spec 005 干净切换，不是叠加）。
   - 旧的 `Control+Shift+3/4` 标点/简繁切换**不再生效**。
   - 旧的 `set_option:ascii_punct` / `send: commit_code` 语法**不再存在**（这些不是合法 key_binder action）。

### 2.4 风险

- **R1**：RIME 引擎 1.13.1 在 `has_menu` 时 `shift+l` 的"先上屏第 2 候选"语义——若 RIME 引擎不支持 `send: <number>` 直接索引候选，需改用 `select_candidate: 2`。
- **R2**：`Switch_L / Switch_R`（macOS Caps Lock 替代）在 Windows 上不存在，本 spec 不涉及。
- **R3**：与系统 / IDE 快捷键冲突（如 `Ctrl+Shift+0` 在 Chrome 是 reset zoom）—— spec 007 UI 提示"潜在冲突"，但不强制阻止。

## 3. Out of scope

- 不改 `weasel.yaml` 样式。
- 不改 `rime_ice.schema.yaml` 内的 speller/algebra。
- 不改 RIME 引擎子模块。
- 不为不同方案（luna-pinyin / rime_ice）做单独的快捷键 patch——所有方案继承 default.yaml。

## 4. 子 spec 自身完成定义

> **本 spec 的实际状态**：0.18.0 → 0.18.5 已 ship 4 个 commit（`d30f69c` + `6277b59` + `828ae07` + L18 修复待 commit）。
> `output/data/default.yaml` 4 类键位已改；`TestDefaultHotkeys.cpp` 25/25 PASS；`xbuild.bat weasel` 0 errors。
> 当前 §4 列表供"未来回归时"参考，标记已完成的项用 `[x]`。

- [x] T001 改 `output/data/default.yaml` 4 类键位（4 commits across 0.18.0-0.18.5）
- [x] T002 `test/TestDefaultHotkeys.cpp` 单测通过（25/25 PASS）
- [x] T003 `xbuild.bat weasel` 0 errors（2026-07-01 验证）
- [ ] T004 手动验证清单（2.3 第 4 步）3 平台 3 DPI 全过 — **待用户 0.18.5 之后实测确认**，重点是 L18 回归（`shift+=` 等组合键不切中英）
- [x] T005 旧键位回归清单（2.3 第 5 步）通过 `TestDefaultHotkeys.cpp` 的负断言覆盖
- [ ] T006 L18 修复 commit：`fix(fluxing): spec 005 - Shift+space for ascii_mode toggle (L18)`（含 `default.yaml` + `TestDefaultHotkeys.cpp` + `lessons-learned.md`）
- [x] T007 release `fluxing-0.18.5.0-installer.exe`（已 ship，含 L13/L14/L17；L18 待 0.18.6 release）