# Glossary

> 按字母/拼音序排列。括号内是本技能内的章节。

**abc_segmentor** — 把编码串切分成音节的组件（ch04）。

**algebra（拼寫運算）** — 按规则改写编码的机制，模糊音/简拼/双拼的底层（ch05）。

**app_options** — 前端配置段，按应用定制（如在某些程序里默认关中文）（ch08、ch03）。

**ascii_composer** — 负责中西文切换与全半角的组件（ch03、ch04）。

**build/** — 部署产物目录，存放编译结果与快照。**验证改动的唯一可信证据**（ch06、ch07）。

**chord_composer** — 双键和弦输入的组件（ch04）。

**commit_code** — ascii_composer 切换策略：已输入的**编码**上屏并转西文（ch03）。

**commit_text** — ascii_composer 切换策略：已输入的**候选**上屏并转西文（ch03）。

**default.custom.yaml** — 全局补靪文件，作用于 `default.yaml`（ch03、ch08）。

**default.yaml** — 发行版默认配置文件，是所有方案设置的公共底座（ch01）。

**deploy（部署）** — 把源文件编译成运行时读的配置。**不部署 = 没改**（ch01、ch06）。

**dictionary / 韵书** — 编码与候选文字的映射表（ch04）。

**distributor / 词典扩展包** — librime 1.6+ 的扩展词典机制，需与主词典音节表一致（ch04）。

**editor** — 处理编码区底层交互的组件（空格/回车/退格/光标）（ch04）。

**filters** — 引擎四组组件之一，对候选做后处理（ch04）。

**glyph / 码表** — 静态的编码-文字对照表，编译成 `*.table.bin`（ch04）。

**good_old_caps_lock** — `ascii_composer` 项：`false` = Caps Lock 切西文时输出小写（ch03）。

**grammar** — 语法（script_translator 用的语法描述）（ch04）。

**inline_ascii** — ascii_composer 切换策略：临时西文编辑区，回车上屏后自动复位（ch03）。

**inline_preedit** — 前端项：内嵌编码行（仅 TSF）（ch08）。

**key_binder** — 组件，把按键绑定到动作（send/toggle/select…）（ch03、ch08）。

**menu** — 候选栏配置段（`page_size` 等）（ch03）。

**mix / 混元编码器** — 码表未命中时推测候选的算法，产物带 ☯ 标记（ch04、ch07）。

**noop** — ascii_composer 切换策略：屏蔽该切换键（ch03）。

**opencc** — 繁简/地区用字转换引擎与配置（ch04、ch08）。

**plum / 東風破** — Rime 的配置管理器与输入方案仓库，也是配方分发工具（ch08）。

**preedit_format** — 编码区显示格式，可用拼写运算改写（ch04、ch05）。

**processor / segmentor / translator / filter** — 引擎的四组组件（ch04）。

**punctuator** — 标点映射组件。**用 unshifted ASCII 作键，对 Shift 不感知**（ch03、ch04）。

**recipe / 配方** — plum 支持的 Rime 数据分发形式（℞）（ch08）。

**recognizer** — 模式识别组件（如把 `rime123` 识别为西文）（ch03、ch04）。

**reverse_lookup** — 反查配置段：`dictionary` / `prism` / `tips` / `preedit_format`（ch03、ch04）。

**schema** — 输入方案。= 配置信息 + 词典（ch01、ch04）。

**speller** — 拼写规整组件：`alphabet` / `delimiter` / `algebra`（ch04、ch05）。

**simplifier** — 繁简转换过滤器。**必须配 uniquifier**（ch03、ch04）。

**switches** — 状态开关段。**不显式 reset 就跨方案继承**（ch03、ch04）。

**switcher** — 方案选单配置段（`hotkeys` 等）（ch03、ch08）。

**translator** — 翻译器配置段（`dictionary` / `enable_*` / `packs` …）（ch04）。

**uniquifier** — 同字候选去重过滤器，与 `simplifier` 配对使用（ch03、ch04）。

**userdb / 用户词典** — 记录输入习惯的运行时数据，**被输入法独占**（ch06、ch07）。

**user.yaml** — 用户状态文件：当前方案与开关状态（ch01、ch06）。

**X11 keysym** — 键名表（`comma` `period` `grave`…），去 `XK_` 前缀（ch03、ch07）。

**八股文 / essay** — Rime 的预设词典与语言模型（ch04）。

**补靪 / patch** — 以源文件为基线改写配置项的机制。用户层写 `patch:`，编译器层写 `__patch:`（ch02）。

**共享文件夹** — 发行版自带的方案/词典/默认配置与预编译缓存，只读（ch01）。

**用户文件夹** — 用户可写目录：自定义文件 + 运行时数据 + 部署产物（ch01、ch06）。

**部署产物 / 编译结果** — 源文件合并后的最终配置，运行时真正读的东西（ch01、ch06）。

**方案选单 / switcher** — `Ctrl+\`` 或 F4 唤出的方案切换界面（ch03）。

**拼写法 / 正字法** — 有效拼写集合到编码集合的映射（ch05）。

**投影** — 连续执行一组拼写运算，得到新的拼写↔编码映射（ch05）。

**音节表** — 方案中所有编码的集合（ch05）。
