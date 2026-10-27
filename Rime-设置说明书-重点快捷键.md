# Rime 输入法设置说明书(重点:快捷键)

> **版本基线**:研究于 2026-08-12,基于
> - GitHub 官方 wiki [rime/home.wiki](https://github.com/rime/home/wiki) master 分支
> - librime 源码 `src/rime/gear/key_binder.cc`、`src/rime/gear/editor.cc` master 分支
> - 本机 `output/data/rime_ice.schema.yaml` + `output/data/user-custom/rime_ice.custom.yaml` v0.21.0.1 现状
>
> **目标读者**:rime_claude / Fluxing 项目成员,需要给 Rime 输入法做用户级配置(尤其是快捷键)。

---

## 0. 阅读地图

| 章节 | 主题 | 适合谁 |
|---|---|---|
| §1 | 体系总览 | 第一次接触 Rime 配置的人 |
| §2 | 文件位置与组织 | 想知道配置存在哪 |
| §3 | YAML + 编译指令语法 | 要写 `<方案>.custom.yaml` 的人 |
| §4 | 部署(Deploy) | 改完配置不生效的人 |
| **§5** | **设置项速查(非快捷键)** | 想改标点、繁简、模糊音的人 |
| **§6** | **快捷键绑定 ⭐(主菜)** | **几乎所有人都要看** |
| **§7** | **editor 绑定 ⭐** | **要改空格/回车/退格行为的人** |
| §8 | 默认值参考 | 想知道什么键在干什么 |
| §9 | 本机现状对照 | 在改 rime_ice 的人 |
| §10 | 排错 | 出问题找不到原因的人 |
| §11 | 同步 | 多端保持一致 |
| 附录 A | 完整按键名 | 写 binding 不知道键名怎么拼的人 |
| 附录 B | 10 个最常见定制场景 | 抄例子最快 |

---

## 1. 体系总览:从配置到运行时

Rime 的设置是**两层架构**:

```
┌─────────────────────────────────────────────────────────────┐
│                       共享文件夹 (只读)                       │
│  (小狼毫:安装目录\data   鼠鬚管:/Library/Input Methods/...   │
│   ibus-rime:/usr/share/rime-data)                          │
│  → default.yaml / luna_pinyin.schema.yaml / 预置方案源      │
└─────────────────────────────────────────────────────────────┘
                            │
                            │  部署 (deploy) 时合并
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                       用户文件夹 (可写)                       │
│  (小狼毫:%APPDATA%\Rime   鼠鬚管:~/Library/Rime             │
│   ibus-rime:~/.config/ibus/rime   fcitx5:~/.local/share/...)│
│                                                             │
│  → <方案>.custom.yaml  ← 用户的「补靪」,不覆盖源文件         │
│  → installation.yaml  ← 部署时间戳                            │
│  → user.yaml          ← 上次退出时的方案/开关状态             │
│  → build/*.schema.yaml / *.prism.bin / *.table.bin ← 编译产物│
└─────────────────────────────────────────────────────────────┘
                            │
                            │  运行时
                            ▼
┌─────────────────────────────────────────────────────────────┐
│              引擎 (key_event → processor → segmentor →        │
│              translator → filter → commit/sink)              │
└─────────────────────────────────────────────────────────────┘
```

**核心约定**:
- **不要直接改共享文件夹** — Rime 升级会覆盖你的修改。
- **用户改配置 = 在用户文件夹写 `<name>.custom.yaml`** — 不会被升级覆盖。
- **修改后必须重新部署**(`小狼毫` = 开始菜单 → 重新部署;`鼠鬚管` = 系统输入法菜单 → 重新部署;`ibus-rime` = 状态栏 ⟲ 按钮)。

---

## 2. 文件位置与组织

### 2.1 用户文件夹的位置

| 发行版 | 路径 | 打开方式 |
|---|---|---|
| 小狼毫 (Windows) | `%APPDATA%\Rime` | 开始菜单 → 小狼毫输入法 → 用户文件夹 |
| 鼠鬚管 (macOS) | `~/Library/Rime` | 系统输入法菜单 → 鼠鬚管 → 用户设定… |
| ibus-rime (Linux) | `~/.config/ibus/rime` | (≤ 0.9.1: `~/.ibus/rime`) |
| fcitx-rime | `~/.config/fcitx/rime` | — |
| fcitx5-rime | `~/.local/share/fcitx5/rime/` | — |

> ⚠️ **librime 允许指定用户文件夹的绝对路径**,**请勿使用相对路径**(参见 [weasel#563](https://github.com/rime/weasel/issues/563))。

### 2.2 用户文件夹的产物

| 文件/目录 | 性质 | 作用 |
|---|---|---|
| `<方案>.schema.yaml` | 用户装 | 输入方案定义 |
| `<词库>.dict.yaml` | 用户装 | 词库源 |
| `<词库>.txt` | 用户装 | 文本词表(如 custom_phrase.txt) |
| `<name>.custom.yaml` | 用户写 | **补靪,所有用户自定义走这里** |
| `default.custom.yaml` | 用户写 | 全局补靪 |
| `weasel.custom.yaml` | 用户写 | 小狼毫前端补靪(外观、配色) |
| `installation.yaml` | 输入法写 | 安装信息 |
| `user.yaml` | 输入法写 | 上次的方案选择/开关状态 |
| `<方案>.userdb/` | 输入法写 | 用户词库(leveldb 格式) |
| `build/*` | 部署生成 | 编译后的机读配置 |
| `trash/*` | 升级生成 | 旧版本失效文件,可手动删 |

### 2.3 共享文件夹 (只读)

| 发行版 | 路径 |
|---|---|
| 小狼毫 | `<安装目录>\data` |
| 鼠鬚管 | `/Library/Input Methods/Squirrel.app/Contents/SharedSupport` |
| ibus/fcitx-rime | `/usr/share/rime-data` |

> 找资源时,**优先用户文件夹**,没有再找共享文件夹。

---

## 3. 配置语法基石:YAML + 编译指令

Rime 的 `.yaml` 是 **YAML 1.1 + 4 个 Rime 专用编译指令**的子集。

### 3.1 `__include` — 包含

把另一个 YAML 节点**整块插入**当前位置。

```yaml
# 包含本文件其他节点
local:
  node: contents
include_example:
  __include: local/node   # 引用 local/node 的内容

# 包含另一个文件
include_example_2:
  __include: config:/external/node    # 引用 config.yaml 的 /external/node
```

> 路径 `<filename>:/` 的 `:/` 不可省略。扩展名 `.yaml` 可省略。

**包含 + 合并**:`__include` 所在 map 除指令外的 key-value 与被包含的 map 合并:

```yaml
include_example_5:
  __include: some_map
  occupation: journalist   # 新增
  simplicity: very         # 覆盖被包含 map 的 simplicity: somewhat
```

**包含 list**:所在 map 整个**替换**为引用的 list(不能用 `__include` 后追加,要用 `__append` 或 `__patch`)。

### 3.2 `patch` 与 `__patch` — 补靪(最重要)

**补靪** = 在不重写源文件的前提下,**用路径指向**精确修改某个节点。

#### 3.2.1 用户级补靪 (`<name>.custom.yaml`)

最常用的形式,放在用户文件夹:

```yaml
# default.custom.yaml  —— 全局
patch:
  "menu/page_size": 9
  "switches/@0/reset": 1
  "punctuator/half_shape/,": "，"
```

> 配置源文件**会自动应用** `<name>.custom:/patch?`(由 `configuration.md` 的 "配置编译器插件" 定义),所以用户的 `patch:` 会被合并进 `<name>.yaml` 或 `<name>.schema.yaml`。

#### 3.2.2 内嵌补靪 (`__patch` 指令)

在源文件内部用的指令,功能相同但用 `/` 分隔路径:

```yaml
patch_example_1:
  sibling: old value
  replace_list:
    - item 1
  __patch:
    sibling: new value
    replace_list/=:
      - only item
```

#### 3.2.3 路径分隔符与特殊字符

- 节点路径用 `/` 分隔
- 如果 key 本身含 `/`,**不能**做路径的一部分 — 改 key 名或换文件结构
- 字符串值**优先用单引号** `''`,避免双引号的转义

#### 3.2.4 操作符 `/+` 与 `/=`

```yaml
__patch:
  # /+ : 与 list/map 合并 (append for list, merge for map)
  append_to_list/+:
    - appended item
  merge_with_map/+:
    key: new value

  # /= : 整体替换 (不写 /= 默认就是替换,但显式写更清晰)
  replace_list/=:
    - only item
  replace_map/=:
    only_key: value
```

| 操作符 | 行为 | list | map |
|---|---|---|---|
| 不写 | **整体替换** | 替换整个 list | 替换整个 map |
| `/+` | 追加 / 合并 | list 追加元素 | map 合并键值对 |
| `/=` | 整体替换(显式) | 同上 | 同上 |

### 3.3 列表的特殊语法

```yaml
# 下标从 0 开始
"some_list/@0": new value          # 修改第 1 个元素
"some_list/@last": new value       # 修改最后 1 个

# 插入
"some_list/@before 0": new value   # 插到第 1 个之前
"some_list/@after last": new value  # 插到最后 1 个之后
"some_list/@next": new value        # = @after last 简写
```

> ⚠️ `@before / @after` 在补靪中**不推荐用**,因为它依赖原 list 的具体下标,升级后可能错位。

### 3.4 `__append` 与 `__merge`(可选)

只用在 `__include` 所在节点及其子节点:

```yaml
append_merge_example:
  __include: starcraft
  __merge:
    made_by: blizzard entertainment
  races:
    __append:                # 追加 list 项
      - protoss
      - zerg
starcraft:
  first_release: 1998
  races:
    - terrans
```

### 3.5 可选的 `?` 后缀

当包含/补靪目标**可能不存在**时,加 `?` 不报错:

```yaml
__patch: default.custom:/patch?       # default.custom.yaml 不存在也不报错
nice_to_have:
  __include: optional_config?         # 同上
```

### 3.6 编译指令执行顺序

同一节点下:
```
__include → 合并 key-value → __patch
```

map 的 key 是无序的,但**单次执行过程中是确定的**。不要靠书写顺序影响行为。

---

## 4. 部署 (Deploy) — 一切的开关

**改完配置不部署 = 没改**。

| 发行版 | 触发方式 |
|---|---|
| 小狼毫 | 开始菜单 → 重新部署;或托盘图标右键 → 重新部署 |
| 鼠鬚管 | 系统输入法菜单 → 重新部署 |
| ibus-rime | 状态栏 ⟲ (Deploy) 按钮;或 `touch ~/.config/ibus/rime/; ibus restart` |
| 老 ibus-rime (≤ 0.9.1) | 删 `default.yaml` + `ibus-daemon -drx` |

部署做什么:
1. 把 `<name>.custom.yaml` 的 `patch:` 合并进 `<name>.yaml` / `<name>.schema.yaml`
2. 编译词库 → `<name>.prism.bin` / `<name>.table.bin` / `<name>.reverse.bin`
3. 输出到 `build/`

---

## 5. 设置项速查(非快捷键)

### 5.1 `switches` — 状态开关(影响方案选单与快捷键)

```yaml
switches:
  - name: ascii_mode              # 状态名,key_binder/toggle 引用
    reset: 0                      # 切到本方案时是否重置 (0/1 或省略)
    states: [ 中, Ａ ]            # 两个状态,方案选单显示为"中 → Ａ"
    abbrev: A                     # 可选,单字符缩写,选单里短一点
  - name: ascii_punct
    states: [ ¥, $ ]
  - name: traditionalization
    states: [ 简, 繁 ]
  - name: full_shape
    states: [ 半角, 全角 ]
```

| 字段 | 必选 | 含义 |
|---|---|---|
| `name` | ✅ | 状态名,内部用,key_binder 的 `toggle:` / `set_option:` 引用它 |
| `states` | ✅ | 2 个字符串,`[state0, state1]` |
| `reset` | ❌ | 0 或 1,切到本方案时是否重置;不写 = 保留上一个方案的状态 |
| `abbrev` | ❌ | 选单里显示的短字符(默认 = states[0] 的首字) |

### 5.2 `menu` — 候选项排版

```yaml
menu:
  page_size: 9                    # 每页 1~9 (部分发行版支持 10)
  alternative_select_keys: "asdf"  # 备选选字键(数字键被占用时)
```

### 5.3 `recognizer/patterns` — 识别规则

正则匹配输入码,命中后**整段**打标签,由对应的 `segmentor/translator` 处理。

```yaml
recognizer:
  import_preset: default          # 继承 default.yaml 的通用规则
  patterns:
    email: "[a-z]+@[a-z]+"        # 自定义规则,标签名=pattern key
    reverse_lookup: "`[a-z]+$"   # `开头识别为反查
```

预置标签:
- `email` — 邮箱
- `url` — 网址
- `number` — 数字
- `punct` — 标点

### 5.4 `punctuator` — 标点映射

```yaml
punctuator:
  __include: default:/punctuator          # 继承 default
  full_shape:                              # 全角 (按 Shift)
    "/": [ ／, "/", ÷ ]                   # 一次按键循环出 3 个候选
  half_shape:                              # 半角 (默认)
    "/": [ "/", ／, ÷ ]
  symbols:                                 # `/` 键前导的特殊符号集
    "/divide": [ ÷, "÷" ]
```

| 字段 | 含义 |
|---|---|
| `full_shape` | 全角 (按 Shift) 时的映射 |
| `half_shape` | 半角 (默认) 时的映射 |
| `symbols` | 输 `/{key}` 触发的符号表 (Rime 默认 = `/` 前缀) |
| 映射值 | 单字符 = 直接上屏;list = 循环出候选 |

**改 `/` 键为「、」**:
```yaml
patch:
  "punctuator/full_shape/": "、"
  "punctuator/half_shape/": "、"
```

### 5.5 `ascii_composer` — 中西文切换 ⭐

详见 §6.7 (与快捷键强相关)。

### 5.6 `speller/algebra` — 拼写运算

四类指令,作用于 speller 收到的拼写串:

| 指令 | 作用 | 例 |
|---|---|---|
| `xform/<regex>/<repl>/` | sed 式替换 | `xform/^([jqxy])u/$1v/` (ju→jv) |
| `xlit/<src>/<dst>/` | 字符级映射 | `xlit/QWERTY/qwerty/` |
| `derive/<regex>/<repl>/` | 派生新拼写(原拼写保留) | `derive/^([zcs])h/$1/` (zh→z) |
| `abbrev/<regex>/<repl>/` | 简拼(原拼写也保留) | `abbrev/^([a-z]).+$/$1/` |
| `erase/<regex>/` | 删除匹配的拼写 | `erase/^hm$/` |

### 5.7 `translator` — 主翻译器参数

```yaml
translator:
  dictionary: rime_ice           # 词库名
  enable_completion: true        # 补全
  enable_sentence: true          # 智能组句
  enable_user_dict: true         # 写入用户词库
  initial_quality: 1.0           # 初始权重
  preedit_format:                # 输入框显示前的变换
    - xform/([jqxy])v/$1u/      # ju→显示为 ju(实际拼音)
  comment_format:                # 候选右侧的注释
    - xform/^/［/
  spelling_hints: 5              # 错音纠错提示的候选数
  always_show_comments: true     # preedit==comment 时也显示
  max_sentences: 1               # 造句数量 (≥ librime 1.17.0)
```

### 5.8 `app_options` — 按应用定制 (小狼毫/鼠鬚管)

```yaml
# weasel.custom.yaml 或 squirrel.custom.yaml
patch:
  app_options/gvim.exe:          # 小狼毫用程序小写名
    ascii_mode: true             # 在 gVim 里默认英文
  app_options/com.apple.Terminal:
    ascii_mode: true
  app_options/com.alfredapp.Alfred: {}  # 不改,跟随默认
```

> 小狼毫 ≥ 0.9.16 / 鼠鬚管 ≥ 0.9.9 支持。

### 5.9 `switcher/hotkeys` — 方案选单的快捷键

```yaml
patch:
  "switcher/hotkeys":
    - "Control+grave"   # Ctrl+`  (注意:不是 Ctrl+`, 而是 IBus 规范)
    - "Control+s"
    - F4
```

> 写法见 §6.2 修饰键。

---

## 6. 快捷键绑定 ⭐(主菜)

### 6.1 整体规则

**`key_binder`** 是 Rime 的"按键重定向"组件,在 processor 链中**早于** speller/punctuator 执行。
它可以:
- 拦截某按键,**改派为另一个按键**
- 拦截某按键,**触发一个动作**(切换选项、调用方案…)

**位置**:通常挂在 `engine/processors` 链头部:

```yaml
engine:
  processors:
    - ascii_composer
    - recognizer
    - key_binder      # ← 这里
    - speller
    - punctuator
    - selector
    - navigator
    - express_editor
```

**配置结构**:

```yaml
key_binder:
  import_preset: default              # 继承 default.yaml 的 binding (可选)
  search: "`"                         # 候选项过滤的前导键 (极少用)
  bindings:                           # 自定义 binding 列表
    - {when: <条件>, accept: <键串>, <动作>: <值>}
    - ...
```

**列表语义**:`bindings` 是 list,patch 时:
- **不写 `bindings/+`** → 整段替换
- **写 `bindings/+`** → 追加 (合并到 default 已有 binding 后)

### 6.2 修饰键 + 按键名

**`accept:` 和 `send:` 都是「IBus 规范键串」**,格式:

```
[修饰符+]+[按键名]
```

**修饰符** (Rime 通用):

| 修饰符 | 含义 |
|---|---|
| `Release` | 弹起时触发 (而非按下) |
| `Shift` | Shift |
| `Control` | Ctrl |
| `Alt` | Alt (macOS 叫 Option,Windows 上 Alt+字母会被系统优先识别为菜单快捷键) |
| `Super` / `Meta` | Linux 专用,跨平台一致性差,慎用 |
| `Caps_Lock` | Caps Lock (一般用在 `ascii_composer/switch_key`,不在 key_binder) |

**多个修饰符**:用 `+` 连接,**顺序无关**。
- `Control+Shift+numbersign` ✅
- `Shift+Control+numbersign` ✅ (但建议固定顺序)

**按键名**:来自 X11 keysym,**去掉前缀 `XK_`**(完整列表见 `librime/include/X11/keysymdef.h`)。

常用按键名速查:

| 类别 | 按键名 |
|---|---|
| 字母 | `a` ~ `z` (大写 `A` ~ `Z` 也行,但**一般不区分大小写**) |
| 数字 | `0` ~ `9` |
| 功能键 | `F1` ~ `F12` |
| 方向键 | `Left` `Right` `Up` `Down` `Home` `End` `Prior` `Page_Up` `Next` `Page_Down` `Begin` |
| 编辑键 | `BackSpace` `Return` `Escape` `Tab` `Delete` `Insert` `space` |
| 修饰键 | `Shift_L` `Shift_R` `Control_L` `Control_R` `Alt_L` `Alt_R` `Meta_L` `Meta_R` `Super_L` `Super_R` `Hyper_L` `Hyper_R` `Caps_Lock` |
| 标点 | `comma` `period` `semicolon` `apostrophe` `grave` `bracketleft` `bracketright` `backslash` `slash` `minus` `equal` |
| 小键盘 | `KP_0` ~ `KP_9` `KP_Decimal` `KP_Enter` `KP_Add` `KP_Subtract` `KP_Multiply` `KP_Divide` `KP_Space` `KP_Tab` |
| 其它 | `numbersign` (#) `dollar` ($) `underscore` (_) `exclam` (!) `question` (?) `bar` (\|) `braceleft` ({) `braceright` (}) `colon` (:) `quotedbl` (") `at` (@) |

> ⚠️ **特殊键名不是按字面字符**,而是用名字:`comma` 才是 `,`,`period` 才是 `.`,`semicolon` 才是 `;`。

完整列表见 [附录 A](#附录-a-完整按键名对照表)。

### 6.3 6 种 `when` 条件(条件 / 上下文)

来源:`librime/src/rime/gear/key_binder.cc:21-28` 枚举 `KeyBindingCondition`:

| 条件 | 含义 | 触发场景 |
|---|---|---|
| `always` | 无条件 | 任何时候都生效 |
| `composing` | 正在输入 | 输入码非空 (有 preedit) |
| `has_menu` | 候选菜单可见 | 至少有 1 个候选 |
| `paging` | 已向后翻页 | 用户已按过 `Page_Down` 之类的翻页键 |
| `predicting` | 预测模式 | librime 预测候选 (Rime 1.x 较少见) |
| (其他) | → 不匹配 | 拼错或者写错就直接忽略,日志会有 WARNING |

**优先级**:Rime 内部把 binding 按条件排序,匹配时**只触发第一个命中**的;高优先级条件(具体场景)排在前面,`always` 排在最后。

### 6.4 5 种 action(动作)

来源:`librime/src/rime/gear/key_binder.cc:161-226` `LoadBindings` 解析:

| action | 字段 | 行为 |
|---|---|---|
| **重派单个按键** | `send: <键串>` | 把当前按键**改派**成另一个按键,后续组件按新按键处理 |
| **重派按键序列** | `send_sequence: <键串1> <键串2>...` | 依次发送一串按键(空格分隔) |
| **切换选项** | `toggle: <选项名>` | 切换 `switches` 中的某个选项 (radio 组则循环) |
| **设置选项** | `set_option: <选项名>` | 强制设为 1 / radio 组中选中 |
| **取消选项** | `unset_option: <选项名>` | 强制设为 0 / radio 组中恢复默认 |
| **切换方案** | `select: <方案ID 或 ".next">` | 切到指定方案;`.next` = 下一个 |

**重要约束**(`key_binder.cc:218-223`):
- 5 种 action **互斥** — 同一 binding 只能有 1 种
- `when` 和 `accept` **必填**,否则忽略
- `send` / `send_sequence` / `toggle` / `set_option` / `unset_option` / `select` 5 选 1

> 注:用 `select: ".next"` 配合 `toggle` 或 hotkey,可以做出「下一方案」快捷键。

### 6.5 `accept` 接收规则

- `accept` 是**要拦截的按键**(用户实际按的)
- `send` / `send_sequence` 是**要改派成什么**
- 修饰符**严格匹配** — `accept: Control+9` 不会匹配用户只按 `9`

### 6.6 `send` vs `send_sequence`

```yaml
# send: 单按键改派
- {accept: Tab, send: Shift+Right, when: composing}
# 用户按 Tab → 实际向 speller 发 Shift+Right(光标右移 1 段)

# send_sequence: 按键序列
- {accept: "Control+p", send_sequence: "Up Up Return", when: has_menu}
# 用户按 Ctrl+p → 依次发 Up(选下个候选) Up(再下个) Return(确认)
```

> `send_sequence` 中的键串用**空格分隔**,可以发任意多个键。

### 6.7 完整例子:翻页/选字/切换

```yaml
key_binder:
  bindings:
    # === 翻页 (有候选时) ===
    - {when: paging,    accept: bracketleft,  send: Page_Up}      # [ = 向前翻
    - {when: has_menu,  accept: bracketright, send: Page_Down}    # ] = 向后翻

    # === 光标移动 (正在输入时) ===
    - {when: composing, accept: Tab,          send: Shift+Right}  # Tab = 右移 1 段
    - {when: composing, accept: "Shift+Tab",  send: Shift+Left}   # Shift+Tab = 左移
    - {when: composing, accept: "Alt+Left",   send: Shift+Left}
    - {when: composing, accept: "Alt+Right",  send: Shift+Right}

    # === 切换中英标点 (始终生效) ===
    - {when: always, accept: "Control+Shift+numbersign", toggle: ascii_punct}
    - {when: always, accept: "Control+Shift+dollar", toggle: traditionalization}

    # === 切换 ASCII 模式 (始终生效) ===
    - {when: always, accept: "Control+grave", toggle: ascii_mode}
    # 注意:Control+` 在 IBus 规范里是 Control+grave,不是字面字符

    # === 用回车清除输入 ===
    - {when: composing, accept: Return, send: Escape}

    # === 自定义符号键 ===
    - {when: always, accept: "Shift+comma",     send: "《"}
    - {when: always, accept: "Shift+period",    send: "》"}
    - {when: always, accept: "Shift+semicolon", send: "："}

    # === 切方案 ===
    - {when: always, accept: "Control+Shift+F1", select: luna_pinyin}
    - {when: always, accept: "Control+Shift+F2", select: rime_ice}
```

### 6.8 `ascii_composer` 的中西文切换键(另一个层面)

**`ascii_composer`** **不是** `key_binder`,是另一个组件,专门管中西文切换。
详见 librime `src/rime/gear/ascii_composer.cc` 与 `default.yaml` 的 `ascii_composer:` 段。

```yaml
ascii_composer:
  good_old_caps_lock: true              # Caps Lock 切到 ASCII 时默认大写
  switch_key:                            # 切换键 + 切换策略
    Caps_Lock: commit_code               # 按下时把已输入编码当英文上屏
    Shift_L:   commit_text               # 按下时把已上屏文字当英文上屏
    Shift_R:   commit_text
    Control_L: noop                      # 屏蔽
    Control_R: noop
```

| 切换策略 | 含义 |
|---|---|
| `inline_ascii` | 临时英文模式:输入字母数字后回车/空格**自动**回中文 |
| `commit_text` | 临时英文模式:输入字母数字上屏(不回中文) |
| `commit_code` | 把当前**输入码**当英文上屏,然后切到英文 |
| `noop` | 屏蔽该键 |

**关键点**:
- 与 `key_binder` 不同,`ascii_composer` 的 `switch_key` 是**按**事件触发,不是按**键串**触发
- 上游 rime 的默认配置是这样;**但本 fork 不是** —— 见下方警告与 §9.3

> ⚠️ **本 fork 的现状(2026-10-27 起变了)**：小狼毫(WeaselTSF)的 TSF 前端把 `Shift_L`/`Shift_R` **整段吃掉**
> (只走 `WEASEL_IPC_SHIFT_DOWN/UP` 的 IPC),librime **根本看不到** Shift ⇒ `ascii_composer/switch_key`
> 与 `key_binder` 里的 Shift 条目 **永远不可能触发**（ADR 0009 / ADR 0010）。
> **裸按 Shift 切中英现在由服务端状态机接管**（ADR 0010，0.21.0.9 起）：
> 候选窗**没开** ⇒ 切中英;候选窗**开着** ⇒ 仍选第 2/3 候选。详见 §9.3。

### 6.9 与前端的关系(小狼毫/鼠鬚管)

`key_binder` 是 librime 内部的事件,**与前端 (Weasel/Squirrel) 的系统级快捷键**是两回事:
- librime `key_binder` 只能在 **IME 已激活**(英文/中文状态)、**输入码区** 内拦截
- 「`Ctrl+Shift` 切中英」是 **Windows 系统级**,由 WeaselTSF 处理,**不**走 librime

所以**改不了系统级快捷键** (Ctrl+Shift 切中英、Ctrl+Space 开关输入法) — 那些是 Windows 注册表 / 系统输入法管理的事。

---

## 7. `editor` 绑定 ⭐(空格/回车/退格的底层)

### 7.1 12 种 action

来源:`librime/src/rime/gear/editor.cc:19-32` `editor_action_definitions[]`:

| action 名称 | 行为 | 默认按键 (`express_editor`) |
|---|---|---|
| `confirm` | 确认当前候选(若有)或 commit 全部 | `space` |
| `toggle_selection` | 在「打开上一段」和「确认当前」之间切换(用于以词定字) | — |
| `commit_comment` | 把当前候选的 comment (拼音提示) 上屏 | `Control+Shift+Return` |
| `commit_raw_input` | 把原始输入码上屏(不经过 preedit_format) | `Return` |
| `commit_script_text` | 把脚本化后的输入码(经过 preedit_format)上屏 | `Control+Return` |
| `commit_composition` | 确认当前段(若有候选)再 commit 全部 | `Return` (FluidEditor) |
| `revert` | 撤销上一步:回退上屏/删除最后输入字符 | `BackSpace` |
| `back` | 回到上一段输入(回退) | — |
| `back_syllable` | 删 1 个音节(组词) | `Control+BackSpace` |
| `delete_candidate` | 删除或降权当前候选 | `Control+Delete` |
| `delete` | 向后删除 1 个字符 | `Delete` |
| `cancel` | 取消整个输入 | `Escape` |

### 7.2 默认绑定(全方案继承)

`express_editor` 默认:
```yaml
editor:
  bindings:
    space: confirm
    Return: commit_raw_input
    Control+Return: commit_script_text
    Control+Shift+Return: commit_comment
    BackSpace: revert
    Delete: delete
    Control+BackSpace: back_syllable
    Control+Delete: delete_candidate
    Escape: cancel
```

`fluid_editor` (注音、語句流用) 类似,但 `Return = commit_composition`。

### 7.3 改写例子

```yaml
editor:
  bindings:
    space: confirm                    # 空格:上屏候选
    Return: commit_raw_input          # 回车:上屏原始输入
    "Control+Return": commit_script_text
    "Control+Shift+Return": commit_comment
    BackSpace: revert                 # 退格:撤销
    Delete: delete
    "Control+BackSpace": back_syllable
    "Control+Delete": delete_candidate
    Escape: cancel                    # Esc:取消输入
    # --- 自定义 ---
    "semicolon": confirm              # ; 也上屏
    "apostrophe": confirm             # ' 也上屏(适合形码快手)
    # 按 ; = 上屏候选(同 space)
```

> `editor/bindings` 是 map,patch 时是覆盖单键,不必整段替换。

### 7.4 `char_handler`(非 binding 但相邻)

`editor/char_handler` 控制「未匹配 binding 的可打印字符」如何处理:

| 值 | 行为 |
|---|---|
| `direct_commit` | 直接上屏(默认,**`express_editor`**) |
| `add_to_input` | 加入输入码(默认,`fluid_editor` 用于注音) |
| `noop` | 不处理 |

> 这就是 `express_editor` 与 `fluid_editor` 的核心区别。

---

## 8. 默认值参考:常见键位清单

Rime `default.yaml` 中**默认的 key_binder/bindings**(小狼毫/鼠鬚管通用):

| 类别 | accept | send | when | 作用 |
|---|---|---|---|---|
| 光标 | `Tab` | `Shift+Right` | composing | 右移 1 段 |
| 光标 | `Shift+Tab` | `Shift+Left` | composing | 左移 1 段 |
| 光标 | `Alt+Left` | `Shift+Left` | composing | 左移 1 段 |
| 光标 | `Alt+Right` | `Shift+Right` | composing | 右移 1 段 |
| 翻页 | `minus` | `Page_Up` | paging | `-` 向前翻 |
| 翻页 | `equal` | `Page_Down` | paging | `=` 向后翻 |
| 翻页 | `comma` | `Page_Up` | paging | `,` 向前翻 |
| 翻页 | `period` | `Page_Down` | has_menu | `.` 向后翻 |
| 开关 | `Control+Shift+numbersign` | toggle ascii_punct | always | 切中英标点 |
| 开关 | `Control+Shift+dollar` | toggle traditionalization | always | 切繁简 |
| 开关 | `Shift+space` | toggle ascii_mode | always | 切中英 |
| 选字 | (由 selector 默认) | `1`~`9` 选候选 | has_menu | 数字键选字 |
| 选字 | `space` | confirm | has_menu | 空格确认 |
| 小键盘 | `KP_0`~`KP_9` | `0`~`9` | composing | 小键盘数字直接上屏 |
| 小键盘 | `KP_Enter` | `Return` | composing | 小键盘回车 |
| 小键盘 | `KP_Add` | `plus` | composing | 小键盘 + |
| 小键盘 | `KP_Subtract` | `minus` | composing | 小键盘 - |
| 小键盘 | `KP_Multiply` | `asterisk` | composing | 小键盘 * |
| 小键盘 | `KP_Divide` | `slash` | composing | 小键盘 / |
| 小键盘 | `KP_Decimal` | `period` | composing | 小键盘 . |

> 来源:`rime/librime` 源码 + `default.yaml`;具体到本机 rime_ice 还有 `Shift+标点 → 符号` 4 条,见 §9。
>
> ⚠️ **ADR 0009(2026-10-25)**:本 fork 的 TSF 前端发的是「按键产生的字符」,
> 所以 `Control+Shift+9` / `Control+Shift+0` 这类**写数字键名**的组合**永远不触发**
> ⇒ 已从出货 `default.yaml` 删除。切中英标点用 `Control+Shift+numbersign`,
> 切简繁用 `Control+Shift+dollar`。
>
> ⚠️ **ADR 0009(2026-10-26)**:同一条规则也适用于 `Shift+标点` —— `accept` 必须写
> **这个组合键产生的字符**的键名（`Shift+quotedbl` / `Shift+bar` /
> `Shift+braceleft` / `Shift+braceright`），写物理键名永远不触发。
>
> ℹ️ `full_shape` 与 `emoji` 两个开关**目前没有任何绑定**
> （出货 `default.yaml` 里两条都是注释掉的），所以「快捷键设置」界面列出的
> `Control+Shift+F` / `Control+Shift+e` 按下去没有反应 —— 这是界面在骗人，
> 记在 ADR 0009「两个后续遗留」第 2 条。

---

## 9. 本机现状对照(`output/data/`)

### 9.1 当前 `key_binder/bindings` (`rime_ice.schema.yaml:295-339`)

共 39 条 = 出货 `default.yaml` 27 条 + 3 份 `user-custom/*.custom.yaml` 各 4 条
(门禁实测:`TestHotkeyBindings` → `bindings=39|reachable=39|dead=0|known_dead=0`;
known-dead 基线已于 2026-10-26 清零 ⇒ 任何一条死绑定直接让构建变红)。
下面只摘录与本机 rime_ice 相关的部分。

```yaml
key_binder:
  search: "`"
  bindings:
    # --- 光标移动 (保留 default) ---
    - {accept: "Shift+Tab", send: "Shift+Left",  when: composing}
    - {accept: Tab,         send: "Shift+Right", when: composing}
    - {accept: "Alt+Left",  send: "Shift+Left",  when: composing}
    - {accept: "Alt+Right", send: "Shift+Right", when: composing}

    # --- 翻页 (保留 default) ---
    - {accept: comma,  send: Page_Up,   when: has_menu}  # , = 上一页
    - {accept: period, send: Page_Down, when: has_menu}  # . = 下一页

    # --- 开关 (保留 default) ---
    # ADR 0009:带修饰键的 accept 必须写「该组合产生的字符」的键名,
    # 所以切中英标点是 Control+Shift+numbersign(#),切简繁是 Control+Shift+dollar($)。
    - {accept: "Control+Shift+numbersign", toggle: ascii_punct, when: always}
    - {accept: "Control+Shift+dollar",     toggle: traditionalization, when: always}

    # --- 小键盘 (保留 default, 16 条) ---
    - {accept: KP_0,         send: 0,        when: composing}
    - {accept: KP_1,         send: 1,        when: composing}
    - {accept: KP_2,         send: 2,        when: composing}
    # ... KP_3 ~ KP_9
    - {accept: KP_Decimal,   send: period,   when: composing}
    - {accept: KP_Multiply,  send: asterisk, when: composing}
    - {accept: KP_Add,       send: plus,     when: composing}
    - {accept: KP_Subtract,  send: minus,    when: composing}
    - {accept: KP_Divide,    send: slash,    when: composing}
    - {accept: KP_Enter,     send: Return,   when: composing}

    # --- Shift+标点 → 符号 (用户自定义 4 条) ---
    # ✅ ADR 0009:这 4 条现在**真的能触发** —— accept 必须写「组合键产生的字符」
    # 的键名(前端发 0x22 / 0x7C / 0x7B / 0x7D);写物理键名(0x27 / 0x5C / 0x5B / 0x5D)
    # 永远不匹配。见 spec 081 T1.1-T1.4。
    - {when: always, accept: "Shift+quotedbl",  send: '"'}
    - {when: always, accept: "Shift+bar",       send: "|"}
    - {when: always, accept: "Shift+braceleft",  send: "{"}
    - {when: always, accept: "Shift+braceright", send: "}"}
```

### 9.2 当前 `editor/bindings` (`rime_ice.schema.yaml:341-352`)

保持 librime `express_editor` 默认,**未做改写**:

```yaml
editor:
  bindings:
    space: confirm
    Return: commit_raw_input
    Control+Return: commit_script_text
    Control+Shift+Return: commit_comment
    BackSpace: revert
    Delete: delete
    Control+BackSpace: back_syllable
    Control+Delete: delete_candidate
    Escape: cancel
```

### 9.3 当前 `ascii_composer/switch_key` (`output/data/default.yaml`)

**这段在 `default.yaml` 里,不在 `user-custom/*.custom.yaml` 里** —— 覆盖文件没有
patch `switch_key`。实际内容:

```yaml
ascii_composer:
  switch_key:
    Caps_Lock: clear      # commit_code | commit_text | clear
    Shift_L: noop         # 火流猩 v2: noop 让 ascii_composer 不接管 Shift_L(避免与 key_binder 冲突)
    Shift_R: noop         # commit_code | commit_text | inline_ascii | clear | noop | set_ascii_mode | unset_ascii_mode
    Control_L: noop       # commit_code | commit_text | inline_ascii | clear | noop | set_ascii_mode | unset_ascii_mode
    Control_R: noop       # commit_code | commit_text | inline_ascii | clear | noop | set_ascii_mode | unset_ascii_mode
```

**坑**:
- librime `key_event.h:43` 的 `operator==` 比较 modifier 含 `RELEASE_MASK`
- WeaselTSF `KeyEvent.cpp:27-28` 在 release 事件**强制**加 `RELEASE_MASK`
- `key_binder` 的 `accept: Shift_L` (modifier=0) **永远不匹配** release (modifier=RELEASE_MASK)
  —— 这条对**本 fork 已无意义**：前端从 spec 077 起**不再把 Shift 转给 librime**,
  「release 误匹配」这一类 bug 不可能再发生（L18 的根因已消失）。
- ⚠️ **本 fork 把 `Shift_L` / `Shift_R` 设成 `noop`(不是 `commit_text`)** ⇒
  `ascii_composer` **不再接管** Shift,所以「单独 Shift 切中英」在 yaml 层走不通。
  spec 077 起 Shift 改由**前端 IPC** 接管(`WEASEL_IPC_SHIFT_DOWN/UP`) —— 完整分析见 **ADR 0010**。
- ✅ **0.21.0.9 起（ADR 0010 已实施）**：裸按 Shift 由**服务端状态机**做上下文消歧 ——
  候选窗**没开** ⇒ 切中英（只切当前会话，与 `Shift+space` 同作用域）;
  候选窗**开着** ⇒ 仍选第 2/3 候选（spec 077 行为不变）。
  开关在 `weasel.yaml` 的 `[global settings]`：`bare_shift_toggles_ascii`（默认 `true`）。
- rime_ice default 的 8 条 `Shift_L/R`/`Shift+l/r` binding 整段删了 — 因为它们 `accept` 是 Shift 单独 / Shift+l,都受这个 bug 影响

### 9.4 本机 `user-custom/rime_ice.custom.yaml` 的 patch 结构

不是「覆盖」,而是 `patch:` 补靪:

```yaml
patch:
  punctuator/half_shape:              # 覆盖 + 继承(必须重述 __include,见文件头注释)
    __include: default:/punctuator/half_shape
    ',': '，'                         # 覆盖 default 的映射
    ...
  key_binder/bindings/+:               # 追加(用 /+,不替换)
    - {when: always, accept: "Shift+quotedbl", send: '"'}
    - {when: always, accept: "Shift+bar", send: "|"}
    - {when: always, accept: "Shift+braceleft", send: "{"}
    - {when: always, accept: "Shift+braceright", send: "}"}
```

> ⚠️ `rime_ice.custom.yaml` **没有** patch `ascii_composer/switch_key`
> —— 那一段在 `output/data/default.yaml` 里,且 `Shift_L` / `Shift_R` 是 `noop`(见 §9.3)。
>
> ⚠️ `punctuator/half_shape` 必须**重述** `__include: default:/punctuator/half_shape`,
> 否则 patch 会因为「half_shape 还是 __include 字符串」而静默失效(见文件头注释)。

---

## 10. 排错:日志与常见坑

### 10.1 日志位置

| 发行版 | 路径 |
|---|---|
| 小狼毫 | `%TEMP%\rime.weasel.*` |
| 鼠鬚管 | `$TMPDIR/rime.squirrel.*` |
| ibus-rime | `/tmp/rime.ibus.*` |
| 老版本 | `<用户文件夹>/rime.log` |

级别:INFO / WARNING / ERROR。**WARNING 与 ERROR 重点关注**。

### 10.2 常见坑

| 症状 | 原因 | 解决 |
|---|---|---|
| 改完配置不生效 | 没重新部署 | 开始菜单 → 重新部署 |
| 部署报错 `invalid key binding #N, with invalid accept pattern: ...` | 键名拼错(如 `comma` 写成 `Comma`) | 检查大小写、查 X11 keysym |
| `when` 写了 `paging` 但没生效 | `paging` 条件要先有 `Page_Down` 触发过 | 改 `has_menu` |
| 中文标点怎么输都不对 | `recognizer/patterns/punct` 覆盖了 `punctuator` | 把 `key_binder` 放 `engine/processors` 头部 |
| `Shift+Tab` 没用 | modifier 顺序错(应该是 `Shift+Tab` 不是 `Tab+Shift`) | 固定修饰符顺序:Shift → Control → Alt |
| 单独 Shift 切不了中英 | 前端把 Shift 整段吃掉,librime 根本看不到它;本 fork 的 `ascii_composer/switch_key` 又是 `noop` | **0.21.0.9 起已修好**（ADR 0010）：候选窗没开时裸 Shift 切中英;开着时仍选第 2/3 候选。改 `weasel.yaml` 的 `bare_shift_toggles_ascii: false` 可关闭 |
| `Ctrl+Shift+9` 切不动标点 | 前端发的是 `(` 不是 `9`,这个组合**永远不触发** | 用 `Control+Shift+numbersign`(ADR 0009) |
| `Control+grave` 没用 | 大小写或修饰符拼错 | 用字面字符时**加引号** |
| `bindings: [a, b, c]` 整段失效 | 写成 list,不是 list of map | 写成 `- {when:..., accept:...}` |

### 10.3 调试技巧

1. **删了重建**:把用户文件夹的 `<name>.custom.yaml` 改名或移走,部署一次看 baseline 行为
2. **二分 patch**:patch 太大就拆成两半,看哪一半出锅
3. **看 build/**:部署后看 `<用户文件夹>/build/<name>.schema.yaml`,这是合并后的最终配置,直接看 binding 是否如预期
4. **INFO 日志有 `invalid key binding`**:直接定位到 `key_binder/bindings` 的第 N 条

---

## 11. 同步与多端一致性

**【同步】指用户数据同步**,**不**指配置同步(配置要在每端分别维护,但 git 管理)。

同步文件夹路径:在 `installation.yaml` 中:

```yaml
sync_dir: /path/to/your/sync/folder
```

被同步的目录:
- `<方案>.userdb/` (用户词库)
- `<方案>.userdb.txt` (用户词库快照,文本格式)
- `user.yaml` (开关/方案记忆)

**多端保持配置一致**:用 git 仓库管理 `*.custom.yaml` / `*.schema.yaml`,`/D=<path>` 显式指定用户文件夹重装。

---

## 附录 A. 完整按键名对照表

来源:librime `include/X11/keysymdef.h`,去掉 `XK_` 前缀。

### 字母 / 数字

`a` `b` `c` `d` `e` `f` `g` `h` `i` `j` `k` `l` `m` `n` `o` `p` `q` `r` `s` `t` `u` `v` `w` `x` `y` `z`
`A` `B` ... `Z`(一般不区分大小写)
`0` `1` `2` `3` `4` `5` `6` `7` `8` `9`

### 功能键

`F1` `F2` `F3` `F4` `F5` `F6` `F7` `F8` `F9` `F10` `F11` `F12`

### 方向 / 翻页

`Left` `Right` `Up` `Down`
`Home` `End` `Prior` `Page_Up` `Next` `Page_Down` `Begin`

### 编辑键

`BackSpace` `Return` `Escape` `Tab` `Delete` `Insert` `space` `Linefeed` `Clear`

### 修饰键

`Shift_L` `Shift_R` `Control_L` `Control_R` `Alt_L` `Alt_R` `Meta_L` `Meta_R` `Super_L` `Super_R` `Hyper_L` `Hyper_R` `Caps_Lock` `Shift_Lock` `Num_Lock` `Scroll_Lock`

### 标点 / 特殊字符(键名 → 字面)

| 字面 | 键名 |
|---|---|
| `,` | `comma` |
| `.` | `period` |
| `;` | `semicolon` |
| `:` | `colon` |
| `'` | `apostrophe` |
| `"` | `quotedbl` |
| `` ` `` | `grave` |
| `~` | `asciitilde` |
| `!` | `exclam` |
| `@` | `at` |
| `#` | `numbersign` |
| `$` | `dollar` |
| `%` | `percent` |
| `^` | `caret` / `asciicircum` |
| `&` | `ampersand` |
| `*` | `asterisk` |
| `(` | `parenleft` |
| `)` | `parenright` |
| `-` | `minus` |
| `_` | `underscore` |
| `=` | `equal` |
| `+` | `plus` |
| `[` | `bracketleft` |
| `]` | `bracketright` |
| `{` | `braceleft` |
| `}` | `braceright` |
| `\` | `backslash` |
| `\|` | `bar` |
| `/` | `slash` |
| `?` | `question` |
| `<` | `less` |
| `>` | `greater` |

### 小键盘

`KP_0` `KP_1` ... `KP_9`
`KP_Decimal` `KP_Enter` `KP_Space` `KP_Tab`
`KP_Add` `KP_Subtract` `KP_Multiply` `KP_Divide`
`KP_Separator` `KP_Equal` `KP_Home` `KP_End` `KP_Page_Up` `KP_Page_Down` `KP_Left` `KP_Right` `KP_Up` `KP_Down` `KP_Insert` `KP_Delete` `KP_Begin`

> 完整列表(几百个)见 `librime/include/X11/keysymdef.h`。

---

## 附录 B. 10 个最常见定制场景(抄例子)

### B.1 每页 9 个候选

```yaml
# default.custom.yaml
patch:
  "menu/page_size": 9
```

### B.2 `/` 键直接出「、」

```yaml
# <方案>.custom.yaml
patch:
  "punctuator/full_shape/": "、"
  "punctuator/half_shape/": "、"
```

### B.3 `,` `.` 翻页(Mac 风格)

```yaml
patch:
  "key_binder/bindings":
    - {when: paging,   accept: comma,  send: Page_Up}
    - {when: has_menu, accept: period, send: Page_Down}
```

### B.4 `[` `]` 翻页(已在 §6.7)

### B.5 用 `;` `'` 选第 2/3 候选(形码快手)

```yaml
patch:
  "key_binder/bindings":
    - {when: has_menu, accept: semicolon,   send: 2}
    - {when: has_menu, accept: apostrophe,  send: 3}
```

### B.6 `Control+` 唤出方案选单(替代 `Ctrl+\``)

```yaml
patch:
  "switcher/hotkeys":
    - "Control+s"
    - F4
```

### B.7 Caps Lock 切到 ASCII + 默认大写

```yaml
patch:
  "ascii_composer/good_old_caps_lock": true
  "ascii_composer/switch_key":
    Caps_Lock: commit_code
    Shift_L: noop
    Shift_R: noop
    Control_L: noop
    Control_R: noop
```

### B.8 在 gVim / 终端里默认英文

```yaml
# weasel.custom.yaml
patch:
  "app_options/gvim.exe":        {ascii_mode: true}
  "app_options/windowsterminal.exe": {ascii_mode: true}
  "app_options/alacritty.exe":   {ascii_mode: true}
```

### B.9 关闭逐键提示(表形输入)

```yaml
# <码表方案>.custom.yaml
patch:
  "translator/enable_completion": false
```

### B.10 关闭用户词库与字频(隐私 / 重置)

```yaml
patch:
  "translator/enable_user_dict": false
```

---

## 附录 C. 来源与参考

| 引用 | URL |
|---|---|
| Rime 官方 wiki 首页 | https://github.com/rime/home/wiki |
| 定製指南(最权威) | https://github.com/rime/home/wiki/CustomizationGuide |
| 配置文件语法 | https://github.com/rime/home/wiki/Configuration |
| 用户文件夹 | https://github.com/rime/home/wiki/UserData |
| 共享文件夹 | https://github.com/rime/home/wiki/SharedData |
| 方案概念(八股文 / 拼寫運算 / 引擎組件) | https://github.com/rime/home/wiki/RimeWithSchemata |
| 拼写运算 | https://github.com/rime/home/wiki/SpellingAlgebra |
| 配方(進階自訂) | https://github.com/rime/home/wiki/Recipes |
| 小狼毫发布手册 | https://github.com/rime/home/wiki/WeaselReleaseManual |
| 雪斋的设定项速查 | https://github.com/LEOYoon-Tsaw/Rime_collections/blob/master/Rime_description.md |
| librime `key_binder` 源码 | https://github.com/rime/librime/blob/master/src/rime/gear/key_binder.cc |
| librime `editor` 源码 | https://github.com/rime/librime/blob/master/src/rime/gear/editor.cc |
| X11 keysym 完整列表 | https://github.com/rime/librime/blob/master/include/X11/keysymdef.h |
| 東風破 (Rime 配置管理) | https://github.com/rime/plum |
| 雾凇拼音 (本机用方案) | https://github.com/iDvel/rime-ice |

---

## 附录 D. 本说明书使用建议

1. **第一次看**:从 §1 读到 §5,理解体系
2. **改快捷键**:直接跳 §6,找到要改的动作 + 例子
3. **写 patch**:参考 §3 的补靪语法 + §9 的本机现状
4. **出问题了**:看 §10 的常见坑
5. **不知道按键怎么拼**:查附录 A

如果要把 Rime 装到新机器上,**只拷这 3 个文件**就够了:
- `<方案>.schema.yaml`(本机是 rime_ice)
- `<方案>.custom.yaml`(你的 patch)
- `default.custom.yaml`(你的全局 patch)

> 其余的(default.yaml、词库、安装文件)都走 NSIS 装机,自动给。
