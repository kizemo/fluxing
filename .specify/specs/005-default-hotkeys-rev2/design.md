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
| 翻页（下一页） | `=` | **`,`** | 主键盘区，vim 风格 |
| 翻页（上一页） | `-` | **`.`** | 主键盘区，vim 风格 |
| 切换中英标点 | `Control+Shift+3` | **`Control+Shift+9`** | 与简繁切换相邻 |
| 切换简繁 | `Control+Shift+4` | **`Control+Shift+0`** | 与中英标点相邻 |
| 中英文切换 | `Shift_L`（commit_code） / `Shift_R`（noop） | **`Shift_L` 或 `Shift_R` 任一** → 切中英 | 双 shift 同义 |
| 选择第 2 候选 | `Control+Shift+1` 等组合（无） | **`Shift_L`** | 单键 |
| 选择第 3 候选 | （无） | **`Shift_R`** | 单键 |

**与 `Shift_L / Shift_R` 中英切换的冲突**：
- 当前 `default.yaml` `ascii_composer/switch_key` 默认为 `Shift_L` = `commit_code`。
- v2 把 `Shift_L` 重新定义为"中英切换" + "选第 2 候选"；`Shift_R` 重新定义为"中英切换" + "选第 3 候选"。
- "选第 X 候选"的优先级**高于**"中英切换"——即按 Shift_L 时若有候选则上屏第 2 候选，无候选时切中英。

### 1.2 用户故事

- **US1-A** [P1]：用户全新安装 Fluxing v2.0.0，在任意输入框打字出现候选后按 `,` 翻到下一页；按 `.` 翻回上一页。
- **US1-B** [P1]：在中文输入态按 `Shift_L` 切到英文；按 `Shift_R` 同样切；按 `Shift_L` 且**有 2 个以上候选**时上屏第 2 候选（候选消失，输入栏插入该候选文字）。
- **US1-C** [P1]：按 `Ctrl+Shift+9` 切换中英标点（中文 → 英文标点或反之）；按 `Ctrl+Shift+0` 切换简繁。
- **US1-D** [P1]：以上快捷键在 spec 007 UI 中可被查看和编辑。

### 1.3 验收（Given/When/Then）

- Given 全新安装 Fluxing v2.0.0
- When 用户在中文输入态连续按 `,` 5 次
- Then 候选窗每按一次翻到下一页（不超界）

- Given 输入法处于中文模式且候选窗有 3 个候选
- When 用户按 `Shift_L`
- Then 第 2 候选上屏，候选窗关闭

- Given 输入法处于中文模式但无候选（光标前未输入字符）
- When 用户按 `Shift_L`
- Then 输入法切到英文模式（status 指示器变 `A`）

- Given 任意输入态
- When 用户按 `Ctrl+Shift+0`
- Then 输入法在"原字 / 简体 / 繁体"三态间循环（当前为 cycle 行为）

## 2. 技术视角（TDD 段）

### 2.1 改动文件

| 文件 | 改动 |
|---|---|
| `output/data/default.yaml` | 改 `key_binder.bindings` / `ascii_composer/switch_key` / `switcher.hotkeys` / `key_binder.select_candidate`（如存在） |

### 2.2 default.yaml 改后样例

```yaml
# === 火流猩输入法 v2 默认快捷键（spec 005）===

patch:
  key_binder/bindings:
    # 翻页（vim 风格，, = 下一页，. = 上一页）
    - { when: paging, accept: "comma", send: Page_Page_Down }
    - { when: paging, accept: "period", send: Page_Page_Up }
    - { when: has_menu, accept: "comma", send: Page_Page_Down }
    - { when: has_menu, accept: "period", send: Page_Page_Up }

    # 候选字上屏：Shift_L/R 单键（优先级高于中英切换）
    - { when: has_menu, accept: "shift+l", send: 2 }   # 上屏第 2 候选
    - { when: has_menu, accept: "shift+r", send: 3 }   # 上屏第 3 候选

    # 中英文切换：Shift_L 或 Shift_R（无候选时）
    - { when: always, accept: "shift+l", send: "commit_code" }
    - { when: always, accept: "shift+r", send: "commit_code" }

    # 切换中英标点 / 简繁
    - { when: always, accept: "Control+Shift+9", send: "set_option:ascii_punct" }
    - { when: always, accept: "Control+Shift+0", send: "set_option:simplification" }

  # ascii_composer/switch_key 不再使用（已被上面的 key_binder 替代）
  ascii_composer:
    switch_key:
      Shift_L: commit_code
      Shift_R: commit_code

  # 保留 rime 社区默认的：
  # - Control+Shift+grave (`: 反查) / Control+Shift+slash (/ : 第二方案)
  # - F4 / F5 / F6 / F7 / F8 / F9 / F10: 切换方案
  # 详见 RIME 默认。
```

> **优先级说明**：`key_binder/bindings` 的 `when: has_menu` 规则**优先于** `when: always`；RIME 引擎按列表顺序匹配，第一个匹配命中即停止。所以上面"has_menu"行必须排在"always"行之前。

### 2.3 验证步骤

1. **TDD（test）**：在 `test/` 加单测 `TestDefaultHotkeys.cpp`，mock `RimeTraits` 加载新 default.yaml，断言：
   - `parse_key("comma") == XK_comma`，且能映射到 `Page_Page_Down`。
   - `parse_key("shift+l")` 在 has_menu 上下文匹配"选第 2 候选"。
   - `parse_key("shift+l")` 在空上下文匹配 `commit_code`。
2. **构建**：msbuild Release|x64 + Win32，跑完整 weasel.sln。
3. **deploy**：`rime_deployer.exe /deploy`（确保 RIME 引擎接受新 default.yaml）。
4. **手动验证**（Win10 22H2 / Win11 23H2 / DPI 100/150/200 各一遍）：
   - 装 Fluxing v2.0.0；安装目录 `%ProgramFiles%\Fluxing\weasel\data\default.yaml` 行数与上面样例一致。
   - 在 notepad 打字"nihao"，候选"你好/ni hao/泥好/拟好/..."，按 `,` 翻页，按 `.` 回。
   - 按 `Shift_L` 切中英；再按 `Shift_L` 切回中文。
   - 在有 3 候选时按 `Shift_L`，上屏"ni hao"。
   - 按 `Ctrl+Shift+9` 切标点；按 `Ctrl+Shift+0` 切简繁。
5. **回归**：
   - 旧的 `-` / `=` 翻页**不再生效**（确保 spec 005 干净切换，不是叠加）。
   - 旧的 `Control+Shift+3/4` 标点/简繁切换**不再生效**。
   - 旧的 `Control+Shift+1..9` 选候选（如有）**不再生效**（v2 改用 Shift_L/R）。

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

- [ ] T001 改 `output/data/default.yaml` 4 类键位
- [ ] T002 `test/TestDefaultHotkeys.cpp` 单测通过
- [ ] T003 msbuild Release|x64 + Win32 编译通过
- [ ] T004 手动验证清单（2.3 第 4 步）3 平台 3 DPI 全过
- [ ] T005 旧键位回归清单（2.3 第 5 步）全过
- [ ] T006 commit：`feat(fluxing): spec 005 default hotkeys rev2`
- [ ] T007 release `fluxing-0.18.0.0-installer.exe` 并推到 origin