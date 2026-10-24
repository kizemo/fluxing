# Cookbook：照抄级配方

> 「我想 X」→ 直接抄。每条都标明**文件**与**验证方式**。
> 通用语法见 [cheatsheet.md](cheatsheet.md)；机制见 [ch02](../chapters/ch02-配置语法与补靪.md)。

## 全局设置

### 每页显示 9 个候选

```yaml
# default.custom.yaml
patch:
  "menu/page_size": 9
```

验证：`build/default.yaml` 的 `menu/page_size`；现场敲字看每页数量。

### 唤出方案选单的快捷键

```yaml
# default.custom.yaml
patch:
  "switcher/hotkeys":
    - "Control+s"
    - "Control+grave"
    - F4
```

注意：字面字符用引号；修饰符顺序 `Shift→Control→Alt`。

### 方案选单里加/减方案（整表替换）

```yaml
# default.custom.yaml
patch:
  schema_list:
    - schema: luna_pinyin
    - schema: luna_pinyin_simp
    - schema: cangjie5
    - schema: terra_pinyin
    - schema: bopomofo
    - schema: rime_ice
    # 非预装方案需先装配方包 https://rime.im/recipes/
    - schema: quick5
    - schema: jyutping
    - schema: wubi86
    - schema: double_pinyin
    - schema: combo_pinyin
```

## 单方案设置

### 简体输出（默认开繁→简）

```yaml
# luna_pinyin.custom.yaml
patch:
  switches:
    - name: ascii_mode
      reset: 0
      states: [ 中文, 西文 ]
    - name: full_shape
      states: [ 半角, 全角 ]
    - name: simplification
      reset: 1
      states: [ 漢字, 汉字 ]
```

配套：方案里 `engine/filters` 必须同时有 `simplifier` 与 `uniquifier`。

### 某方案独立一个繁简开关（不污染别的方案）

```yaml
# luna_pinyin.custom.yaml
patch:
  switches:
    - name: zh_simp
      reset: 1
      states: [ 漢字, 汉字 ]
  simplifier:
    option_name: zh_simp
```

### 默认英文输出

```yaml
patch:
  "switches/@0/reset": 1
```

### 关闭逐键提示

```yaml
patch:
  translator/enable_completion: false
```

### 关闭用户词典与字频调整

```yaml
patch:
  translator/enable_user_dict: false
```

### 关闭码表连打（☯ 混元词）

```yaml
patch:
  translator/enable_sentence: false
```

### 关闭仓颡与拼音混打

```yaml
patch:
  abc_segmentor/extra_tags: {}
```

## 标点

### 直接按的标点改中文（`/` → 「、」）

```yaml
# luna_pinyin.custom.yaml
patch:
  punctuator/half_shape:
    __include: default:/punctuator/half_shape   # ← 源 schema 用 include 引入时必须写这行
    ',': '，'
    '.': '。'
    '/': '、'
    ';': '；'
    "'": "'"
    "[": '【'
    "]": '】'
```

**如果源 schema 里 `half_shape` 不是 `__include` 引入的**（直接是 map），则**不要**写那行 `__include`，否则会覆盖成空。

### Shift+标点（`Shift+,` → 「《」）

```yaml
# weasel.custom.yaml / squirrel.custom.yaml / <schema>.custom.yaml 均可
patch:
  key_binder/bindings/+:
    - { when: always, accept: "Shift+comma", send: "《" }
    - { when: always, accept: "Shift+period", send: "》" }
    - { when: always, accept: "Shift+slash", send: "？" }
    - { when: always, accept: "Shift+semicolon", send: "：" }
    - { when: always, accept: "Shift+apostrophe", send: '"' }
    - { when: always, accept: "Shift+backslash", send: "|" }
    - { when: always, accept: "Shift+bracketleft", send: "{" }
    - { when: always, accept: "Shift+bracketright", send: "}" }
```

> 用 `/+` 追加，不覆盖已有 `key_binder` 绑定。
> 键名用 X11 名称（`comma` `period` `slash` `semicolon` `apostrophe` `backslash` `bracketleft` `bracketright`），**不要写成 `,` `.`**。

### 数字后的符号智能转换

```yaml
patch:
  punctuator/digit_separators: ',.:'      # 默认 '.':  增加千分位 ','
  punctuator/digit_separator_action: commit # 直接上屏西文符号（代价：失去双击上中文句号）
```

## 快捷键

### 方括号翻页

```yaml
patch:
  "key_binder/bindings":
    - { when: paging, accept: bracketleft, send: Page_Up }
    - { when: has_menu, accept: bracketright, send: Page_Down }
```

### 回车清编码 + 分号/单引号选字

```yaml
patch:
  "key_binder/bindings":
    - { when: composing, accept: Return, send: Escape }
    - { when: has_menu, accept: semicolon, send: 2 }
    - { when: has_menu, accept: apostrophe, send: 3 }
```

### 空码时按空格清空

```yaml
patch:
  translator/enable_sentence: false
  key_binder/bindings:
    - { when: has_menu, accept: space, send: space }
    - { when: composing, accept: space, send: Escape }
```

### 用 Control 键切中西文

```yaml
patch:
  ascii_composer/good_old_caps_lock: true
  ascii_composer/switch_key:
    Caps_Lock: commit_code
    Shift_L: noop
    Shift_R: noop
    Control_L: commit_code
    Control_R: commit_code
```

`commit_code` = 已输入编码上屏并转西文；`commit_text` = 已输入候选上屏并转西文；`inline_ascii` = 临时西文区，回车后自动复位；`noop` = 屏蔽该键。

## 识别与反查

### 某些串强制识别为西文

```yaml
patch:
  recognizer/patterns/rime123: "^rime[0-9]+$"
  recognizer/patterns/rimeime: "^rimeime[-_.0-9]*$"
```

### 反查码换成别的方案

```yaml
# 速成的反查码改粤拼
patch:
  reverse_lookup/dictionary: jyutping
  reverse_lookup/tips: 〔粵拼〕
  reverse_lookup/preedit_format: []

# 仓颡的反查码改双拼（词典仍是 luna_pinyin，只换 prism）
patch:
  reverse_lookup/prism: double_pinyin
  reverse_lookup/tips: 〔雙拼〕
  reverse_lookup/preedit_format: []
```

## 模糊音与拼写运算

```yaml
patch:
  speller/algebra:
    __patch:
      - pinyin:/zh_z_bufen
      - pinyin:/n_l_bufen
      - pinyin:/hu_f_buhun
      - pinyin:/en_eng_bufen
      - pinyin:/abbreviation
      - pinyin:/spelling_correction
      - pinyin:/key_correction
```

老写法（直接插规则行）：

```yaml
patch:
  speller/algebra:
    - derive/^([zcs])h/$1/
    - derive/^n/l/
    - derive/^l/n/
    - abbrev/^([a-z]).+$/$1/
```

## 前端外观

```yaml
# weasel.custom.yaml
patch:
  "style/font_face": "明兰"
  "style/font_point": 14
  style/horizontal: true
  style/inline_preedit: true
  style/display_tray_icon: true
  "style/color_scheme": starcraft
  "preset_color_schemes/starcraft":
    name: 我的配色
    author: me
    text_color: 0xccaa88
    candidate_text_color: 0x30bb55
    back_color: 0x000000
    border_color: 0x1010a0
    hilited_text_color: 0xfecb96
    hilited_back_color: 0x000000
    hilited_candidate_text_color: 0x60ffa8
    hilited_candidate_back_color: 0x000000
```

## 按应用定制

```yaml
# weasel.custom.yaml（Windows，程序名全小写）
patch:
  app_options/gvim.exe:
    ascii_mode: true
```

```yaml
# squirrel.custom.yaml（macOS，用 Bundle Identifier）
patch:
  app_options/com.apple.Xcode:
    ascii_mode: true
  app_options/com.alfredapp.Alfred: {}
```

## 自定义词组（不建词典的偷懒法）

```yaml
patch:
  recognizer/patterns/reverse_lookup:        # 关掉 ` 键的反查
  'punctuator/half_shape/`':
    - '佛振 <chen.sst@gmail.com>'
    - 'http://rime.github.io'
```

含特殊字符的路径用**单引号**；重定义 `/` `+` `=` 时要直接覆盖 `half_shape` 节点。

## 每条配方的自检

1. 只有一个 `patch:` 键；
2. 改的是 `.custom.yaml` 不是发行版 `.yaml`；
3. 若改的节点是 `__include` 引入的，patch 里重述了 `__include:`；
4. 跑 `scripts/validate_custom_yaml.py <file>`；
5. 部署后看 `build/<name>.schema.yaml`；
6. 现场敲一遍。
