# Rime 自定义机制 —— 我的消化理解

> **目标读者**: 已经装好 Rime 引擎,但面对 `*.schema.yaml` 一头雾水的人
> **消化来源**:
> - [Rime_description.md](Rime_description.md) — Schema.yaml + Dict.yaml 完整 reference (雪齋)
> - [Rime方案製作詳解.html](Rime方案製作詳解.html) — 同一内容 HTML 版
> - [鼠鬚管介面配置指南.md](鼠鬚管介面配置指南.md) — Squirrel UI 配置
> - [squirrel.custom.yaml](squirrel.custom.yaml) — 真实 Squirrel 用户配色 + patch 范例
> - [date_time.lua](date_time.lua) — 真实 lua_translator 范例 (中国农历 / 24 节气 / 八字 / 月相)
> - [Old_cantonese.markdown](Old_cantonese.markdown) — 真实方案文档范例
>
> **本文是消化,不是翻译**。我会用自己的话重新组织,加进自己画的架构图,以及我与 Fluxing 项目相关的反思。

---

## 0. Rime 是什么 — 三句话本质

1. **引擎/方案分离**: `librime` 是 C++ 引擎,只认"按键"和"输出字符",不认"拼音"、"五笔"、"仓頡"。
2. **方案即数据**: 任何一种输入法,包括它的候选规则、拼写规则、标点映射、皮肤,**全都是 YAML/文本文件**,不是代码。
3. **Lua 是边界逃生口**: 当 YAML 表达力不够时,可以通过 Lua 函数插入"动态逻辑"。

**这一句话理解 Rime 的全部设计哲学**: *打字方案应该像词库一样可以被人编辑、分享、合并,而不是被焊死在二进制里。*

---

## 1. 核心架构:三层

```
┌────────────────────────────────────────────┐
│  前端 (Frontend)                            │
│  • macOS: 鼠鬚管 (Squirrel)                │
│  • Windows: 小狼毫 (Weasel) / Fluxing     │
│  • Linux: ibus-rime / fcitx-rime          │
│  职责: 系统集成 + UI 渲染 + 把按键给引擎  │
└────────────────────────────────────────────┘
                    ↓ keystrokes / commits
┌────────────────────────────────────────────┐
│  引擎 (librime)                            │
│  职责: 装配方案,执行四组组件流水线         │
│  关键文件: librime-lua (Lua 扩展插件)    │
└────────────────────────────────────────────┘
                    ↓
┌────────────────────────────────────────────┐
│  数据 (用户目录 $RIME_USER_DIR)            │
│  • *.schema.yaml — 方案描述               │
│  • *.dict.yaml   — 码表                    │
│  • *.userdb      — 用户词频/用户词         │
│  • *.lua         — 动态逻辑                │
│  • default.yaml  — 全局默认 (菜单/按键)   │
│  • <frontend>.yaml — 前端 UI 默认         │
│  • <frontend>.custom.yaml — 用户 UI 覆盖 │
└────────────────────────────────────────────┘
```

**关键设计选择**: 数据是"用户级",而非"系统级"。Rime 装在 `/usr` 或 `C:\Program Files\fluxing`,但所有可改的东西都在 `~/.config/ibus/rime/` (Linux) / `~/Library/Rime/` (macOS) / 用户目录 (Windows)。**重装前端 = 不丢配置**。

---

## 2. 三大基石文件

| 文件 | 角色 | 比喻 |
|---|---|---|
| `*.schema.yaml` | 方案定义 (行为+装配) | **菜谱** — 决定"按这个键"时做什么 |
| `*.dict.yaml`   | 码表 (字符↔编码表) | **食材表** — 决定哪些字/词可用 |
| `rime.lua` + `*.lua` | 动态逻辑 | **现场厨师** — YAML 表达不了时由 Lua 接手 |

**核心要点**:
- schema 决定"流程" (用哪个 engine component,拼写规则是什么,翻到什么候选)
- dict 决定"原料" (有什么字/词可用,频次多少)
- Lua 决定"即兴发挥" (按当前时间生成日期,按光标位置过滤候选)

---

## 3. 引擎的"四组流水线"模型

schema 里的 `engine:` 段,按四组顺序声明组件。**每组的职责严格分离,这是 Rime 最精彩的设计**:

```
用户按键序列
   ↓
[1] processors  ——  按键层处理
   • ascii_composer    切中西文
   • recognizer        模式识别 (网址?反查?拼音反查?)
   • key_binder        快捷键重映射
   • speller           拼写规整 (algebra)
   • punctuator        标点直接映射
   • selector/navigator 选词/光标
   • express_editor    回车上屏/退格
   • lua_processor     Lua 钩子 (少用)
   ↓
[2] segmentors   ——  把输入串切成段,给每段打 tag
   • ascii_segmentor   西文段 → ascii
   • matcher           配合 recognizer 给特定段打 reverse_lookup 等 tag
   • affix_segmentor@x 自定义段 (prefix/suffix 包裹)
   • abc_segmentor     默认中文段 → abc
   • punct_segmentor   标点段 → punct
   • fallback_segmentor 其他兜底
   ↓
[3] translators  ——  翻译每段为候选列表
   • table_translator   码表翻译 (仓頡/五笔)
   • script_translator  脚本翻译 (拼音/粵拼,基于音节)
   • lua_translator@x   动态生成候选 (日期/时间/八字)
   • echo_translator    兜底回显
   • punct_translator   标点→符号
   ↓
[4] filters     ——  过滤/排序候选
   • simplifier@x       繁简转换
   • uniquifier         去重
   • cjk_minifier       字符集裁剪
   • single_char_filter 单字优先
   • reverse_lookup_filter 反查
   • lua_filter@x       自定义过滤
   ↓
候选列表 → 上屏
```

**一个例子走通**:
- 用户在朙月拼音方案输入 `ni3hao3`:
  1. **processors** — `speller` 的 algebra 把 `ni3hao3` 拆解/规整 (这里的 `3` 是声调)
  2. **segmentors** — `abc_segmentor` 给这段打 `abc` tag
  3. **translators** — `script_translator` 读 `luna_pinyin.dict.yaml` 查 `ni3` 候选,把"你/泥/尼..."喂入;再查 `hao3` 候选
  4. **translators** (续) — 接着调用 `enable_sentence` 自动组句,产出"你好"
  5. **filters** — `simplifier@zh_simp` 把"妳好"转"你好";`uniquifier` 去重

---

## 4. schema.yaml 全景

### 4.1 元数据段

```yaml
schema:
  name: "朙月拼音"           # 显示名 (菜单中给用户看)
  schema_id: luna_pinyin      # 内部名 (代码/配置引用)
  author: ["佛振 / 雪齋"]    # 多个贡献者列表
  description: |              # 多行描述
    朙月拼音方案,基于《通用规范汉字表》
  dependencies:               # 依赖其它方案 (反查/混用)
    - cangjie6
  version: "0.19"             # 重要 — 改了必须升版
```

**`schema_id` 不可变**,是 git/ref 的标识。`name` 改了无影响,只是显示。

### 4.2 switches 段 (开关)

```yaml
switches:
  - name: ascii_mode           # 开关内部名
    reset: 0                   # 0=默认关闭
    states: ["中文", "西文"]   # 给 UI 显示用
  - name: full_shape
    states: ["半角", "全角"]
  - name: simplification        # 关联 simplifier 的 option_name
    states: ["漢字", "汉字"]
  - name: ascii_punct
    states: ["句讀", "符號"]
  - options: [zh_tw, zh_cn, zh_mars]   # 多选开关 (互斥)
    states: ["字型 → 漢字", ...]
    reset: 0
```

**开关与快捷键的关系**: `key_binder.bindings` 里写 `{accept: "Control+1", toggle: ascii_mode, when: always}`,这就是 UI 上"Ctrl+1 切中英文"的实现。

### 4.3 engine 段 (装配四组组件)

**关键规则**:
- **列表顺序** = 处理顺序。speller 必须在 translator 前面,因为 translator 要消费 speller 规整过的输入。
- **`@name`** = 给同类组件加多个实例。例如 `script_translator@pinyin` 和 `script_translator@jyutping` 是两个不同的翻译器,各用各的 dict。

```yaml
engine:
  processors:
    - ascii_composer
    - recognizer
    - key_binder
    - speller
    - punctuator
    - selector
    - navigator
    - express_editor
  segmentors:
    - ascii_segmentor
    - matcher
    - affix_segmentor@pinyin        # 自定义段,加 prefix/suffix
    - abc_segmentor
    - punct_segmentor
    - fallback_segmentor
  translators:
    - table_translator              # 主翻译器 (无 @ 后缀)
    - script_translator@pinyin      # 副翻译器
    - lua_translator@get_date
  filters:
    - simplifier@zh_simp
    - uniquifier
    - cjk_minifier
    - lua_filter@single_char_first
```

### 4.4 speller 段 (拼写规整)

```yaml
speller:
  alphabet: zyxwvutsrqponmlkjihgfedcba    # 全部合法输入键
  delimiter: " '"                          # 上屏时音节之间的分隔符
  algebra:
    - erase/^xx$/                          # 删除正则匹配的输入
    - abbrev/^([a-z]).+$/$1/               # 简拼(只取首字母)
    - abbrev/^([zcs]h).+$/$1/              # zh/ch/sh 简拼
    - derive/^([nl])ve$/$1ue/              # 衍生(nve → nue,即 nü)
    - derive/^([jqxy])u/$1v/               # 衍生(ju → jv,即 ju)
    - xlit|abc|xyz|                        # 字符替换(适合大量一对一)
```

**algebra 的 6 个动作**(我会这样记):
| 动作 | 作用 | 是否保留原形 |
|---|---|---|
| `xform` | 改写 | ❌ 不保留 |
| `derive` | 衍生 | ✅ 保留原形 + 衍生形 |
| `abbrev` | 简拼 | (独立优先级,最低) |
| `fuzz` | 畧拼 | 只组词,不出单字 |
| `xlit` | 字符变换 | 1-to-1 替换 |
| `erase` | 删除 | 删掉不合法输入 |

### 4.5 translator 段 (翻译器)

**主翻译器**用 `translator:` 命名,副翻译器用 `translator_name:` 命名。

**核心可配项**:
```yaml
translator:
  dictionary: cangjie6           # 主码表
  prism: cangjie6                # 拼写棱镜(由 speller/algebra 生成)
  user_dict: cangjie6            # 用户词典
  db_class: tabledb              # 文本 vs userdb 二进制

  enable_charset_filter: true    # 字符集过滤
  enable_encoder: true           # 自动造词
  enable_completion: true        # 提前显示未打完的候选
  enable_sentence: true          # 智能组句
  enable_user_dict: true         # 用用户词典
  encode_commit_history: true    # 上屏过的字自动成词
  max_phrase_length: 5           # 自动造词最大词长

  preedit_format:                # 输入码显示格式
    - xform/^([a-z ]+)$/$1｜\U$1\E/  # 加 ｜ 分隔 + 大写化
  comment_format:                # 候选词附带的"注疏"
    - xlit|abcdef...|日月金木...|    # 把五笔字母转五笔字根
  initial_quality: 0.75          # 这个翻译器出字优先级 (0~1)

  spelling_hints: 9              # 多少字以内的候选标注完整带调拼音
  always_show_comments: false
```

### 4.6 simplifier 段 (繁简转换)

```yaml
zh_simp:
  option_name: simplification    # 关联到 switches 中的同名开关
  opencc_config: t2s.json        # OpenCC 配置: 繁→简
  tags: [abc]                    # 作用范围:仅常规中文段
  tips: char                     # 显示原字注释: none/char/all
  show_in_comment: false
  excluded_types: [reverse_lookup]  # 反查的候选不做繁简转换
```

**OpenCC 内置 9 个配置文件**:
- `t2s.json` 繁→简 (默认)
- `s2t.json` 简→繁
- `t2tw.json` 繁→台灣正體
- `t2hk.json` 繁→香港
- `t2jp.json` 繁→日文 (旧字体)
- `s2tw.json` 简→台灣
- `s2hk.json` 简→香港
- `s2twp.json` 简→台灣 (含词组)
- `tw2sp.json` 台灣→简 (含词组)

### 4.7 recognizer 段 (模式识别 + 反查触发)

```yaml
recognizer:
  import_preset: default         # 导入默认规则
  patterns:
    email: "^[a-z][-_.0-9a-z]*@.*$"   # 邮箱直接输出
    url: "^(www[.]|https?:|ftp:|mailto:).*$"
    reverse_lookup: "`[a-z]*;?$"       # 反查触发: `xxxx;
    pinyin_lookup: "`P[a-z]*;?$"        # 拼音反查
    jyutping_lookup: "`J[a-z]*;?$"      # 粵拼反查
    pinyin: "(?<!`)P[a-z']*;?$"          # 拼音前缀 P
    jyutping: "(?<!`)J[a-z']*;?$"
    punct: "/[a-z]*$"                    # 特殊符号前缀 /
```

**反查的核心技巧**:
- 用户在仓頡方案里输入 ``` `nihao; ``` → 触发 `reverse_lookup` segmentor → 后面跟的 `pinyin` 翻译器查 `luna_pinyin.dict.yaml` → 返回仓頡码
- 这就是"仓頡方案中能用拼音查字"的实现

### 4.8 key_binder 段 (快捷键)

```yaml
key_binder:
  import_preset: default
  bindings:
    - {accept: semicolon, send: 2, when: has_menu}   # 分号选第二候选
    - {accept: "Control+1", select: .next, when: always}
    - {accept: "Control+2", toggle: full_shape, when: always}
    - {accept: "Control+3", toggle: simplification, when: always}
    - {accept: "Control+4", toggle: extended_charset, when: always}
```

**`when` 上下文**(极其重要):
- `paging` —— 翻页时
- `has_menu` —— 候选框出现时
- `composing` —— 在编辑输入码时
- `always` —— 全局

**`accept` 可用键名**: 26 字母 + 0-9 + 完整键盘符号集(X11 风格的 keysym,文档列了 ~80 个)

### 4.9 punctuator 段 (标点)

```yaml
punctuator:
  import_preset: symbols
  half_shape:
    "'": {pair: ["「", "」"]}    # 第一次按出「,第二次按出」
    "(": ["〔", "［"]           # 弹菜单选
    .: {commit: "。"}            # 直接上屏,无菜单,优先级最高
  full_shape:
    ",": {pair: ["，", "、"]}
```

---

## 5. dict.yaml 全景

### 5.1 元数据

```yaml
name: cangjie6.extended
version: "0.1"
```

**关键**: `name` 必须与文件名一致,且必须被某个 schema 引用。

### 5.2 配置

```yaml
sort: by_weight                   # 初始排序:original 按字典序,by_weight 按频次
use_preset_vocabulary: true       # 是否引入八股文 (内置词库)
vocabulary: []                    # 手动引入其它 dict
max_phrase_length: 5              # 八股文最大词长
min_phrase_weight: 1000           # 八股文最低频次

columns:                          # 码表各列定义
  - text                          # 字/词
  - code                          # 编码
  - weight                        # 频次
  - stem                          # 造词码(形码用)

import_tables:                    # 合并其它码表
  - cangjie6                      # 引用 cangjie6.dict.yaml

encoder:                          # 形码自动造词规则
  exclude_patterns:
    - '^z.*$'                     # 排除某些编码
  rules:
    - length_equal: 2             # 二字词规则
      formula: "AaAzBaBbBz"       # 大写=字序,小写=该字中的位置
    - length_equal: 3
      formula: "AaAzBaYzZz"
    - length_in_range: [4, 5]
      formula: "AaBzCaYzZz"
  tail_anchor: "'"
```

### 5.3 码表本体

```yaml
個	owjr	246268	ow'jr
看	hqbu	245668
中	l	243881
用	bq	168934	b'q
```

- Tab 分隔,顺序对应 `columns`
- 数字是 weight (字频)
- 第四列 stem 是造词码,缺省时自动从前几列的 code 衍生

**`encoder/rules` 的公式语法**(我自己的理解):
- `A B C D` = 4 个汉字
- `AaAz` = 第一字的首码 + 末码
- `BaYz` = 第二字的首码 + 倒数第二码
- 公式自动从输入词中提取对应的形码位

---

## 6. 输入码变候选词:完整生命周期(我画的时序图)

```
用户键入按键
    │
    ▼
[preprocessor] ←── 键盘布局转换 (Dvorak? Colemak?)
    │
    ▼
[processor: ascii_composer]   中西文状态判断
    │
    ├── 西文模式 → 直接上屏
    │
    └── 中文模式 ↓
        │
        ▼
    [processor: speller]        algebra 规整 (简拼/衍生/删除)
        │
        ▼
    [processor: key_binder]    快捷键重映射 (Ctrl+1 → .next)
        │
        ▼
    [processor: punctuator]    标点键 → 标点字符
        │
        ▼
    [segmentor: matcher/recognizer]  模式匹配 (网址/反查?)
        │
        ├── 网址 → [ascii_segmentor] tag=ascii → 直接上屏
        │
        └── 反查 → [affix_segmentor@reverse_lookup] tag=reverse_lookup
            │
            ▼
        [segmentor: abc_segmentor]    兜底 tag=abc
            │
            ▼
        [translator: script_translator@pinyin]  查 luna_pinyin.dict
            │                                      │
            │                                      ├─→ 候选 1
            │                                      ├─→ 候选 2
            │                                      └─→ ...
            │
        [translator: lua_translator@get_date]   动态生成日期候选
            │
            ▼
        [filter: simplifier@zh_simp]  繁→简
            │
            ▼
        [filter: uniquifier]          去重
            │
            ▼
        [filter: lua_filter@single_char_first] 单字优先
            │
            ▼
        候选列表 → 显示
            │
        用户按数字键
            │
            ▼
        [processor: selector]        选词 → 上屏
            │
        [editor: encode_commit_history] 把上屏字加入用户词库
```

---

## 7. 进阶:多方案互查与反查

### 7.1 反查的真实流程

cangjie6 方案里**既能打仓頡码,也能打拼音反查仓頡码**,靠的是:

1. **recognizer** 匹配 `` `nihao; `` → 命中 `reverse_lookup` pattern
2. **affix_segmentor@reverse_lookup** 把这段打上 `reverse_lookup` tag,prefix=`` ` ``,suffix=`;`
3. **reverse_lookup_filter@pinyin_reverse_lookup** 挂载在 `script_translator@pinyin` 上:
   ```yaml
   pinyin_reverse_lookup:
     tags: [pinyin_lookup]        # 挂到拼音翻译器上
     overwrite_comment: true
     dictionary: cangjie6          # 反查用仓頡码表
     comment_format:               # 注释显示为"〔仓頡码〕"
       - "xform/$/〕/"
       - "xform/^/〔/"
   ```

### 7.2 方案依赖

`schema/dependencies: [luna_pinyin, jyutping]` —— 这表示本方案可使用 luna_pinyin / jyutping 的翻译器作为反查源。

---

## 8. Lua 扩展的边界与威力

**何时用 Lua**: 当 YAML 无法表达"动态"或"复杂逻辑"时。
- 日期时间(随当前时间变化)
- 用户词典操作
- 复杂正则过滤
- 上下文相关候选(如根据已上屏内容调整候选)

**4 个 Lua hook 点**:

| Hook | 时机 | 用途 |
|---|---|---|
| `lua_processor` | 按键层 | 改写按键行为 |
| `lua_segmentor` | 切段层 | 自定义分段规则 |
| `lua_translator` | 翻译层 | 动态生成候选 (date_time.lua 典型) |
| `lua_filter` | 过滤层 | 自定义过滤/排序 |

**`date_time.lua` 真实范例**(我读完后提炼的关键点):

```lua
local function date_translator(input, seg, env)
  local on = env.engine.context:get_option("show_date")
  -- 必须显式检查开关,否则即使没开也触发
  if (on and input == "ona") then        -- "ona" 是仓頡的"今日"码
    local preedit = "人弓 日"            -- 给输入码加视觉提示
    local date = string.gsub(os.date("%Y年%m月%d日"), "([^%d])0+", "%1")
    local weekday = chinese_weekday(os.date("%w"))
    local candidate = Candidate("date", seg.start, seg._end, date, weekday)
    candidate.preedit = preedit
    yield(candidate)                      -- 一次 yield 一个候选
    -- ... 可以 yield 多个不同格式的候选
  elseif (on and input == "tmtjmhda") then  -- 華曆
    -- ... 农历计算
  end
  -- input 是拼写后字符串
  -- seg.start / seg._end 标记此段在原输入中的位置
  -- env.engine.context:get_option(name) 读开关
end
```

**学习要点**:
1. **input 来自拼写层**: 不是按键,也不是"ni"的字符,是 speller 处理后的串
2. **yield 而非 return**: 可以产生多个候选
3. **`Candidate(type, start, end, text, comment)`**: 第二三个参数是段位置,影响替换范围
4. **必须检查开关**: lua_translator 不会被自动按开关过滤,这是手动责任
5. **可用 Cangjie 码做触发**: `ona` = 今日,`tmtjmhda` = 華曆。说明这个 lua 脚本被挂在了仓頡方案上

**Lua 局限**:
- 不能改写 speller 的拼写(那是 algebra 的事)
- 不能创建新 tag(那是 segmentor 的事)
- 不能直接做繁简转换(那是 simplifier 的事)
- 性能受 librime-lua 插件版本影响(早期版慢)

---

## 9. OpenCC 与繁简转换

**OpenCC = Open Chinese Convert**,独立 C++ 库,Rime 集成它做繁简/异体字/地区差异字转换。

**关键 tips 配置**(我自己的理解):
- `tips: none` —— 完全不显示原字
- `tips: char` —— 只在单字候选后显示原字
- `tips: all` —— 所有候选后都显示
- `show_in_comment: true` —— 把转换结果放在"注"里(灰色小字),主候选保持原字

**`excluded_types` 的妙用**: 反查结果通常是"码+原字"对,如果也做繁简转换就会乱。一般设 `excluded_types: [reverse_lookup]` 跳过。

---

## 10. UI 定制(以鼠鬚管/小狼毫为例)

**两个层次的定制**:

1. **全局**:`squirrel.yaml` / `weasel.yaml` (写在前端包里,通常不直接改)
2. **用户级**:`squirrel.custom.yaml` / `weasel.custom.yaml` (用户目录,用 `patch:` 语法覆盖)

**`squirrel.custom.yaml` 真实范例核心结构**:

```yaml
patch:
  # 1. app_options: 针对特定 app 强制某开关
  app_options/+:
    com.wolfram.Mathematica: { ascii_mode: true }
    com.tencent.qq: { no_inline: true }
  
  # 2. 键盘布局
  keyboard_layout: USExtended
  
  # 3. style: 整体外观
  style:
    candidate_format: "[label] [candidate]  [comment]"  # 候选显示格式
    font_face: "SourceSans3VF-Roman, SourceHanSans..."   # 多字体 fallback
    font_point: 15
    inline_preedit: true
    inline_candidate: true
    candidate_list_layout: stacked
    text_orientation: vertical
    corner_radius: 16
    translucency: true
    color_scheme: solarized-light
    color_scheme_dark: solarized-dark
  
  # 4. preset_color_schemes: 配色方案
  preset_color_schemes/+:
    solarized-dark:
      name: "曬經・月"
      back_color: 0x66352A0A     # AABBGGRR 格式
      # ...
```

**色值格式** `0xAABBGGRR` —— 不是 `0xAARRGGBB`!注意是 `蓝绿红` 不是 `红绿蓝`,且 `A` 在最前。**坑过很多新人**。

**`menu:` 段**(在 default.yaml 里,不在 squirrel.custom.yaml):
```yaml
menu:
  alternative_select_labels: [①, ②, ③, ④, ⑤]   # 改成汉字数字
  alternative_select_keys: ASDFGHJKL              # 改用字母选字(数字键被占用时)
  page_size: 5
```

**`patch/+:` 语法**: 表示"合并到现有的列表/字典",而不是替换整个。这是 Rime 覆盖机制的精髓。

---

## 11. 实战路线:从 0 到自定义方案

**推荐路径**(我整理的,不是文档写的):

### Level 1: 不写任何新方案
- 复制 `luna_pinyin.schema.yaml` → `luna_pinyin.custom.yaml`
- 在里面加 `patch:` 改 `switches/+` / `key_binder/bindings/+`
- 这就是日常 90% 的定制

### Level 2: 复用官方 dict + 加自定义 phrase
- 在用户目录加 `custom_phrase.txt`:
  ```
  fluxing	vflx	1
  雾月	wue	1
  ```
- `luna_pinyin.custom.yaml` 里加:
  ```yaml
  patch:
    translator/+:
      dictionary: ""             # 强制空,主表变成自定义
      user_dict: custom_phrase
      db_class: tabledb
      enable_sentence: false
      enable_completion: false
  ```

### Level 3: 写自己的 dict
- `myext.dict.yaml` 放个人词库(诗词/专有名词)
- 主表加 `import_tables: [myext]`

### Level 4: 写自己的 schema
- 复制 `cangjie6.schema.yaml` 改名 → 改 `schema_id`
- 改 `switches` / `key_binder` / `recognizer/patterns`
- 加 `lua_translator@my_func`

### Level 5: 写 lua 扩展
- 实现 `lua_translator(input, seg, env)` 函数
- 在 schema 引用 `lua_translator@my_func`

### Level 6: 修改前端
- 需要 C++ 经验
- 改 librime / WeaselServer / Squirrel 源码

---

## 12. 关键 takeaways(我的总结)

### 12.1 Rime 的设计哲学
- **数据优先于代码**: 90% 定制靠改 YAML,不写代码
- **流程可装配**: 4 组 engine 组件可增删,顺序重要
- **扩展有边界**: Lua 是边界逃生口,不是替代 YAML
- **可逆可共享**: 所有定制都是文件,git 管理,revert 容易

### 12.2 新人常见误区(我自己整理的)
1. **忘了升 version** —— 改了 schema 不升版,可能不生效
2. **alphabet 漏配** —— 用了小写字母但 `alphabet` 没列,候选不出现
3. **recognizer 正则错** —— 反查不触发,90% 是正则没匹配
4. **OpenCC 色值写反** —— 写成 RRGGBB 而不是 BBGGRR
5. **affix_segmentor 和 translator 名字对不上** —— 段打了 tag 但没翻译器认领
6. **speller 顺序错** —— speller 没在 translator 前声明,algebra 不生效
7. **lua_translator 没检查开关** —— 即使开关关闭也触发,候选乱入

### 12.3 与 Fluxing 项目的关联(我自己的视角)
- **Fluxing = Weasel + WeaselServer(IPC 化)+ FluxingComponents(QuickPanel/PhrasesDialog)**
- **Weasel 是 Windows 前端**,只是 Rime 整套架构中的一环
- **Rime 的"前端可换"特性在这里被用上**: 小狼毫 → 鼠鬚管 → ibus-rime,数据全兼容
- **Fluxing 改的是 UI 行为(panel/QuickPanel),不动 librime 核心**
- **Spec 075 v0.74.3 (撤回 FocusIn Show)** 是 UI 层 bug,跟 Rime 自定义机制无关
- **如果未来要定制 Fluxing 方案**(比如加 project-specific shortcuts),仍然走"复制 schema → patch"这条标准路径

### 12.4 我读完后的"如果让我重做一遍"
1. **不要试图一次读懂整个 schema.yaml**。先复制一个官方方案,改一个小东西(比如 `key_binder`),重启验证。**实践优先**。
2. **algebra 是最有魔力的部分**。理解 derive / abbrev / xform 的区别后,你能定制任何"输入习惯"。
3. **recognizer + affix_segmentor + reverse_lookup_filter** 是 Rime 最强大的架构,真正学会"反查"才算入门。
4. **Lua 不是必学**。普通用户用不到,YAML 够了。Lua 适合"动态逻辑"(日期/计算)。

---

## 13. 参考资料

| 文档 | 角色 | 我用它学到了什么 |
|---|---|---|
| [Rime_description.md](Rime_description.md) | Schema/Dict 完整 reference | 4 组 engine + algebra 6 动作 + 28 个 translator 选项 |
| [Rime方案製作詳解.html](Rime方案製作詳解.html) | 同一内容 HTML 版 | CSS 排版,易读 |
| [鼠鬚管介面配置指南.md](鼠鬚管介面配置指南.md) | Squirrel UI 配置 | style 25 个选项 + preset_color_schemes 17 个颜色 |
| [squirrel.custom.yaml](squirrel.custom.yaml) | 真实用户配置范例 | patch/+: 语法、app_options、配色实现 |
| [date_time.lua](date_time.lua) | 真实 lua_translator 范例 | 农历/节气/八字/月相/时辰的工业级实现 |
| [Old_cantonese.markdown](Old_cantonese.markdown) | 真实方案文档 | 声母/韵母/声调演变的可视化方案(用 SVG) |
| [README.md](README.md) | 3 行简介 | "Deligate works for Rime IME" |

**官方文档**:
- librime 项目: https://github.com/rime/librime
- 雪齋 Rime 官网: https://rime.im/
- 官方 Wiki: https://github.com/rime/home/wiki

---

## 14. 后记:这份消化理解的局限性

**我没读透的**:
- **chord_composer 完整语法** (并击,只在宮保拼音用过一次,case 不够)
- **librime-lua 内部 API** (date_time.lua 只用到子集,没看完整 API 文档)
- **grammar / contextual_suggestions** (语言模型集成,需要外部 training corpus)
- **OpenCC 9 个 config 的细节差异** (t2tw vs t2twp 等)

**我没做但应该做的**:
- 没有真的改一份 schema 试过(只看文档不做项目=纸上谈兵)
- 没有对比 librime 1.x vs 2.x 的 API 差异
- 没有看 librime-lua 的 release notes

**如果这份文档有错,大概率错在**:
- algebra 例子(我据 description 抄的,没自己跑)
- recognizer 正则(那个 (?<!) 是 negative lookbehind,文档里写错位)
- 色值格式(AABBGGRR 我反复确认过,但仍可能是某些版本的反序)
