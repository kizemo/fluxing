---
name: rime-skill
version: 1.0.0
description: >-
  Use when customizing a Rime 输入法 (librime / 小狼毫 / 鼠鬚管) — 写补靪 patch、
  加快捷键、调标点映射、造输入方案、排查部署失败。给出可直接抄的 YAML 与判定依据。
triggers:
  - "rime 补靪 patch 怎么写"
  - "自定义 rime 快捷键"
  - "小狼毫 每页候选数 怎么调"
  - "rime 部署失败 / 配置不生效"
  - "写一个 rime 输入方案"
  - "rime 标点不对"
  - "rime 模糊音 双拼 反查"
mutating: true
brain_first: exempt
when_to_use: >-
  Use when the user asks how to customize a Rime input method — 补靪/patch、
  快捷键绑定、标点映射、方案与词典、部署与排错。
tools:
  - bash
  - read
  - write
  - edit
  - grep
  - glob
---

# Rime 定制与理解

> **铁律：改 Rime 只改 `<名字>.custom.yaml` 的 `patch:`，改完必须重新部署——直接改发行版 `.yaml` 会被升级覆盖，改完不部署则运行时读的还是旧编译结果。**

## 何时用 / 何时不用（When to use / When NOT to use）

**何时用**：用户要改 Rime 行为（补靪、快捷键、标点、候选排版、方案菜单、繁简、模糊音、反查、用户词典、部署故障），或要理解 Rime 的配置机制与输入方案体系。

**何时不用**：写输入法引擎 C++ 代码（用 `codebase-graph` / `graphify`）；改前端 UI 布局（TSF/UI 属 `WeaselUI`）；与 Rime 无关的通用 YAML/配置问题。

## 核心流程（Core Process）

| # | 动作 | 发生什么 |
|---|---|---|
| 1 | **diagnose** 定位 | 先分清是「配置层」「方案层」「词典层」「前端层」，查 `references/troubleshooting.md` 对症状 |
| 2 | **scope** 判定改动面 | 全局 `default.custom.yaml` / 单方案 `<schema>.custom.yaml` / 前端 `weasel.custom.yaml`、`squirrel.custom.yaml` / 词典 `*.dict.yaml` |
| 3 | **patch** 写补靪 | 按 `references/cookbook.md` 取片段，用 `references/settings-reference.md` 查配置项；写完跑 `scripts/validate_custom_yaml.py` |
| 4 | **deploy** 重新部署 | 运行时读编译结果，不部署等于没改（操作见 ch06） |
| 5 | **verify** 验证 | 看 `build/<name>.schema.yaml` 合并结果 + 日志 `E`/`WARNING` 行 + 现场复现 |
| 6 | **record** 落档 | 改了发行版默认行为就写进项目文档/ADR，别只留在聊天里 |

## Rime 六条铁律（Iron Rules）

1. **只改 `.custom.yaml`**。发行版 `.yaml` 升级时被覆盖，且会丢掉本次升级新增的修复。
2. **改完必部署**。运行时读的是编译结果（`build/`），不是源文件。
3. **一个文件只能有一个 `patch:` 键**。重复写会静默覆盖前一份。
4. **路径是 DSL 不是字符串**：`/` 分层、`@0/@last/@before N/@after N/@next` 选列表项、`/+` 合并、`/=` 替换；key 本身不能含 `/`；含特殊字符的路径用单引号。
5. **patch 前先确认源节点是不是 `__include` 引入的**。librime 的 kPatch(2) > kInclude(1)，patch 先于 include 执行；此时节点还是 `__include` 字符串，dict-merge **静默失败 → patch 完全无效**。解法：在 patch 里重述 `__include: default:/<节点>` 再写覆盖项。
6. **状态开关默认跨方案继承**。切换方案时未显式 `reset` 的开关保留上一方案状态；想改变输出字形要另起独立开关名并配 `simplifier/option_name`。

## 常见借口（Common Rationalizations）

| 借口 | 现实 |
|---|---|
| "改一下 `.yaml` 更快" | 升级即被覆盖，还丢掉新版本修复；`.custom.yaml` 只需多写一个 `patch:` |
| "改完没反应，那就是没生效" | 九成是没部署；先部署，再看 `build/` 里的合并结果 |
| "patch 写了但没效果" | 优先查铁律 5（`__include` 冲突）与铁律 3（重复 `patch:`） |
| "直接改 `default.yaml` 就能全方案生效" | 改 `default.yaml` 影响所有方案；只想改一个方案就写 `<schema>.custom.yaml` |
| "我改了 `key_binder` 但按键没反应" | `when` 条件写错（`paging` 要先翻过页）；`accept` 修饰符要严格匹配 |
| "这不就是改个 yaml 吗" | Rime 的配置是**带编译指令的 DSL**，`patch` 路径语法有 6 类操作符，不是字符串替换 |

## 危险信号（Red Flags）

- patch 挂在 `punctuator/half_shape` 上却完全无效 → 多半撞上 `__include` 优先级（铁律 5）
- 部署日志出现 `invalid key binding #N, with invalid accept pattern` → 键名大小写/拼写错，或把 `bindings` 写成了 list 而不是 list of map
- 换方案后繁简/中英状态莫名变了 → 开关缺 `reset`，或该用独立开关名（铁律 6）
- 用户词典被锁、`rime_dict_manager` 报错 → 输入法还在运行，独占打开了词典文件
- 改 `output/data/**/*.yaml`（发行版文件）而不是 `user-custom/*.custom.yaml` → 下次升级丢失
- 候选菜单里出现「☯ 阴阳鱼」标记 → 混元编码器猜的词，码表里本来没有
- 日志里 `WARNING` 刷屏但功能正常 → 多半是 `accept` 模式永不命中

## 验证清单（Verification）

- [ ] 改动落在 `.custom.yaml` 的 `patch:` 下，且全文件只有一个 `patch:`
- [ ] `scripts/validate_custom_yaml.py <file>` 通过（0 error）
- [ ] 已重新部署，部署日志无 `E` 开头的行
- [ ] `build/<name>.schema.yaml` 里能看到预期的合并结果
- [ ] 现场复现：实际敲一遍触发路径（不是"看着对"）
- [ ] 改动面确认无误（没误伤其他方案：切到别的方案试一次）
- [ ] 改了发行版默认行为的，已落文档

## 反模式（Anti-Patterns）

- **直接改发行版 `.yaml`** —— 升级即失。
- **一个 `.custom.yaml` 写两个 `patch:`** —— 后者覆盖前者，且不报错。
- **不部署就下结论"配置无效"** —— 编译结果没更新。
- **用 `__patch` 写用户层补靪** —— 用户层用 `patch:`；`__patch:` 是源文件里的编译器指令。
- **把 `bindings` 写成 list 而不是 list of map** —— 整段静默失效。
- **用 `/` 写含斜杠的键名** —— 路径按 `/` 分层，键名含 `/` 无法表达。
- **在 `punctuator` 里指望 Shift 生效** —— Punctuator 用 unshifted ASCII 作映射键，对 Shift 不感知；Shift+标点必须 `key_binder` 拦截。
- **把 `simplifier` 单独装上** —— 必须同时有 `uniquifier`，否则「鐘→钟」「鍾→钟」这类同字候选重复。
- **给每个方案重复写同一份补靪** —— 放 `default.custom.yaml` 全局生效。

## 输出格式（Output Format）

回答 Rime 定制问题时，按这个顺序给：

1. **改哪个文件**（完整路径 + 为什么是它）
2. **补靪片段**（可直接抄的 YAML，标明 `patch:` 层级）
3. **重新部署命令**（按发行版）
4. **怎么验证**（看 build/ 哪一项、敲什么键、期望什么现象）
5. **风险与副作用**（影响哪些方案、是否跨方案继承）

不确定用户要哪种发行版时，先看 `output/data/` 或用户自述；两说并存就两个都给。

## 失败时怎么办（When it fails）

- **改了没效果** → 依次查：部署了吗？→ `build/` 里有吗？→ 撞 `__include` 了吗？→ `patch:` 重复了吗？
- **部署报错** → 看部署日志里 `E` 开头的行，定位出错的源文件；键名错用 X11 keysym 表（去 `XK_` 前缀）。
- **候选/编码不对** → 回到 `speller/algebra` 与 `translator` 段；改编码映射优先用 `derive`/`abbrev` 而不是改词典。
- **看不懂上游某段配置** → 查 `E:\obsidian\rime\digest\` 的中文消化层（对应上游原文），或用 GBrain 检索（见 `docs/GBRAIN.md`）。
- **本项目专属细节**（当前 key_binder、当前 asciic_composer、TSF 的 RELEASE_MASK 坑） → 读仓库根的 `Rime-设置说明书-重点快捷键.md`，那是本项目的权威。

## 使用的工具（Tools Used）

| 工具 | 用途 |
|---|---|
| `read` | 读 `chapters/`、`references/` 与上游原文；**按需读，别一次全读** |
| `grep` | 在 `output/data/` 里找现成配置与绑定（先看有没有现成写法再动手） |
| `write` / `edit` | 写 `.custom.yaml`（只写 `patch:`，不碰发行版 `.yaml`） |
| `bash` | 跑 `scripts/validate_custom_yaml.py`；部署前后比对 `build/`；看日志 |
| `glob` | 列出 `output/data/**` 与用户文件夹里的 `.custom.yaml` |

## 章节索引（Chapter Index）

| # | 章节 | 解决什么 |
|---|---|---|
| [ch01](chapters/ch01-架构与数据分布.md) | 架构与数据分布 | 引擎/方案/词典/前端各是什么，文件在哪 |
| [ch02](chapters/ch02-配置语法与补靪.md) | 配置语法与补靪 | `__include`/`__patch`/`__append`/路径 DSL/加载顺序 |
| [ch03](chapters/ch03-定制处方集.md) | 定制处方集 | 9 个官方例子 + 19 条 DIY 处方（照抄级） |
| [ch04](chapters/ch04-方案与词典.md) | 方案与词典 | 引擎四组组件、词典格式、扩展包、各方案速查 |
| [ch05](chapters/ch05-拼写运算.md) | 拼写运算 | 6 个算子、投影、简拼/容错/模糊音写法 |
| [ch06](chapters/ch06-用户数据与部署.md) | 用户数据与部署 | 用户/共享文件夹、词典管理、部署、发布 |
| [ch07](chapters/ch07-排错手册.md) | 排错手册 | 症状→原因→处理，含本项目级陷阱 |
| [ch08](chapters/ch08-前端外观与发布.md) | 前端外观与发布 | 小狼毫/鼠鬚管外观、配色、发布流程 |

## 主题索引（Topic Index）

- **补靪 / patch** → ch02, ch03
- **`__include` 优先级陷阱** → ch02, ch07
- **快捷键 / key_binder** → ch03, ch08
- **标点 / punctuator** → ch03, ch08
- **候选排版 / menu** → ch03
- **繁简 / simplifier** → ch03, ch04
- **模糊音 / 双拼 / 反查** → ch04, ch05
- **词典 / 用户词典 / 扩展包** → ch04, ch06
- **部署 / 日志 / build** → ch06, ch07
- **配色 / 字体 / 外观** → ch08
- **开关继承 / reset** → ch03

## 支撑文件（Supporting Files）

- [references/cheatsheet.md](references/cheatsheet.md) — 决策表与阈值，一页速查
- [references/cookbook.md](references/cookbook.md) — 照抄级配方（需求 → YAML）
- [references/settings-reference.md](references/settings-reference.md) — 配置项全表
- [references/troubleshooting.md](references/troubleshooting.md) — 症状→原因→处理
- [references/librime-traps.md](references/librime-traps.md) — librime 行为级陷阱（wiki 里没有的）
- [references/hero-queries.md](references/hero-queries.md) — 触发评测集
- [glossary.md](glossary.md) — 术语表
- [patterns.md](patterns.md) — 定制模式库
- [docs/SOURCE.md](docs/SOURCE.md) — 语料来源与校验记录
- [docs/GBRAIN.md](docs/GBRAIN.md) — 用 GBrain 做深度检索
- `scripts/validate_custom_yaml.py` — 补靪静态校验器

## 范围与边界（Scope & Limits）

本技能覆盖 Rime 官方 wiki（35 篇）+ 中文消化层（9 篇）+ 本项目既有实践。
不覆盖引擎 C++ 实现、前端 TSF/UI 布局、安装器与签名流程——那些走本仓库的 `AGENTS.md` 与专项规则。
