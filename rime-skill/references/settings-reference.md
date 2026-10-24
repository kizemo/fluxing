# Settings Reference：配置项全表

> 按「段 → 项」组织。写补靪时用**左列的完整路径**。
> 权威原文：上游 wiki `Configuration` / `RimeWithSchemata`「設定項速查手冊」；本项目还有一份
> `.research/rime-docs/Rime_description.md`（HTML 转存的完整版）。

## schema（描述档）

| 项 | 含义 |
|---|---|
| `schema/name` | 方案显示名（出现在方案选单里） |
| `schema/schema_id` | 方案内部名，代码引用用（英文/数字/下划线） |
| `schema/author` | 作者。改过请把新作者追加在后面 |
| `schema/description` | 方案历史、码表来源、规则说明 |
| `schema/dependencies` | 依赖的其他方案（通常是反查目标） |
| `schema/version` | 版本号。**加引号**（`0.10` 会被当成数值 `0.1`） |

## switches（开关）

| 项 | 含义 |
|---|---|
| `name` | 开关名。`simplifier` 通过 `option_name` 指向它 |
| `states` | 两个状态名，方案选单里显示「漢字→汉字」这种 |
| `reset` | 初始态。`0` = 第一个，`1` = 第二个。**切换方案时不 reset 会继承上一方案状态** |

## engine（引擎装配）

| 段 | 作用 | 常见项 |
|---|---|---|
| `processors` | 按键 → 编码 | `ascii_composer` `recognizer` `key_binder` `speller` `punctuator` `selector` `navigator` `express_editor` `chord_composer` |
| `segmentors` | 切分编码 | `abc_segmentor` |
| `translators` | 编码 → 候选 | `table_translator` `script_translator` `punct_translator` `reverse_lookup_translator` |
| `filters` | 候选后处理 | `simplifier` `uniquifier` `emoji` `simplify` `grease_pinyin` |

> 装配顺序 = 处理顺序。`key_binder` 要早于 `punctuator`，否则自定义按键被吃掉。

## speller（拼写规整）

| 项 | 含义 |
|---|---|
| `speller/alphabet` | 字母表（一般 `zyxwvutsrqponmlkjihgfedcba`） |
| `speller/delimiter` | 音节分隔符（一般是 `space` 和 `'`） |
| `speller/algebra` | 拼写运算算式列表。**模糊音/简拼/双拼全在这里** |
| `speller/import_preset` | 继承一套预设拼写配置 |

## translator（翻译器）

| 项 | 含义 |
|---|---|
| `translator/dictionary` | 用哪个词典 |
| `translator/import_preset` | 继承一套预设翻译器配置 |
| `translator/enable_completion` | 逐键提示。`false` = 只出精确匹配 |
| `translator/enable_user_dict` | 用户词典与字频调整 |
| `translator/enable_sentence` | 码表连打造句（**仅 `table_translator`**） |
| `translator/packs` | 词典扩展包列表 |
| `translator/preedit_format` | 编码区显示格式（可用拼写运算改写） |
| `translator/comment_format` | 候选后缀注释格式（简拼/全拼提示等） |
| `translator/initial_quality` | 候选初始权重（调词频） |
| `translator/strict_spelling` | 限定「模糊」拼写不用于单字 |
| `translator/encode_commit_history` | 是否把上屏历史编入编码 |

## recognizer（模式识别）

| 项 | 含义 |
|---|---|
| `recognizer/patterns/<名>` | 正则 → 识别为西文/反查等特殊输入 |
| `recognizer/import_preset` | 继承预设识别规则 |

内置 `punct` 规则会**覆盖 punctuator**——中文标点不对时先查这一条。

## key_binder（快捷键）

| 项 | 含义 |
|---|---|
| `key_binder/bindings` | 绑定列表。每条 `{when, accept, send/…}` |
| `key_binder/import_preset` | 继承预设绑定 |

- `when`：`always` `composing` `has_menu` `paging` `predicting`
- action（互斥）：`send` `send_sequence` `toggle` `set_option` `unset_option` `select`

## editor（编码区底层）

| 项 | 含义 |
|---|---|
| `editor/bindings` | 空格/回车/退格/方向键等在编码区的行为 |
| `editor/include` | 引入其他 editor 配置 |
| `editor/char_handler` | 字符处理（相邻但独立的段） |

## punctuator（标点）

| 项 | 含义 |
|---|---|
| `punctuator/import_preset` | 继承默认符号表（`default`） |
| `punctuator/half_shape` | 半角符号表。**用 unshifted ASCII 作键，对 Shift 不感知** |
| `punctuator/full_shape` | 全角符号表 |
| `punctuator/digit_separators` | 数字后智能转换的符号（默认 `.` `:`） |
| `punctuator/digit_separator_action` | `commit` = 直接上屏西文；`no_confirm` = 需确认 |

## ascii_composer（中西文切换）

| 项 | 含义 |
|---|---|
| `ascii_composer/switch_key` | 切换键映射：`Caps_Lock` `Shift_L` `Shift_R` `Control_L` `Control_R` → `commit_code` `commit_text` `inline_ascii` `noop` |
| `ascii_composer/good_old_caps_lock` | `false` = Caps Lock 切西文时输出小写；`true` = 大写 |
| `ascii_composer/switch_key` 之外的 `import_preset` | 继承预设 |

## simplifier（繁简转换）

| 项 | 含义 |
|---|---|
| `simplifier/option_name` | 关注哪个开关。**必须与 `switches` 里的名字一致** |
| `simplifier/opencc_config` | OpenCC 转换配置（如 `t2tw.json`）。不给则用默认繁→简 |
| `simplifier/comment_format` | 转换后的注释格式 |

> `simplifier` 必须配 `uniquifier`（在 `engine/filters` 里），否则同字候选重复。

## menu / switcher（候选与选单）

| 项 | 含义 |
|---|---|
| `menu/page_size` | 每页候选数，1~9（个别发行版 10），默认 5 |
| `switcher/hotkeys` | 唤出方案选单的快捷键列表 |
| `switcher/options` | 选单里可切换的选项 |

## reverse_lookup（反查）

| 项 | 含义 |
|---|---|
| `reverse_lookup/dictionary` | 反查用哪个词典 |
| `reverse_lookup/prism` | 反查用哪套拼写方案（双拼反查改这个） |
| `reverse_lookup/tips` | 反查提示样式（如 `〔粵拼〕`） |
| `reverse_lookup/preedit_format` | 反查时的编码区显示（`[]` = 清空） |

## 前端（weasel.yaml / squirrel.yaml）

| 项 | 含义 |
|---|---|
| `style/font_face` | 候选字体名 |
| `style/font_point` | 字号（只认数字） |
| `style/horizontal` | 候选横排 |
| `style/inline_preedit` | 内嵌编码（仅 TSF） |
| `style/display_tray_icon` | 显示托盘图标 |
| `style/color_scheme` | 当前配色名 |
| `preset_color_schemes/<名>/*` | 配色定义：`name` `author` `text_color` `candidate_text_color` `back_color` `border_color` `hilited_text_color` `hilited_back_color` `hilited_candidate_text_color` `hilited_candidate_back_color` |
| `app_options/<程序标识>` | 按应用定制（如 `ascii_mode: true`） |

色值是 24 位 **`0xBBGGRR`**（蓝绿红），不是 RGB。

## 词典（*.dict.yaml）

| 项 | 含义 |
|---|---|
| `name` | 词典名 |
| `version` | 版本号（加引号） |
| `sort` | 排序方式（`original` / `by_weight`） |
| `use_preset_vocabulary` | 是否用预设词库（【八股文】） |
| `import_tables` | 从别的词典源文件导入码表 |

## 用户文件夹（运行时文件）

| 文件 | 含义 |
|---|---|
| `installation.yaml` | 安装信息 + `sync_dir`（同步目录） |
| `user.yaml` | 当前方案与开关状态 |
| `<语言代号>.userdb/` | 用户词典（**独占资源**） |
| `build/` | 部署产物（编译结果 + 快照）——**验证的唯一可信证据** |
