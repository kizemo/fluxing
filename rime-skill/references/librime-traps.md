# librime 行为级陷阱

> 这些**不是** wiki 里的内容，是 librime 的实际行为/实现细节，也是最容易把人卡住的地方。
> 每一条都写清「现象 → 根因 → 正确做法」。

## T1 · patch 优先于 include，被 patch 的节点还是 `__include` 字符串时**静默失效**

**现象**：补靪语法完全正确，部署无任何报错，行为一点没变。

**根因**：librime 的 `DependencyPriority` 是 `kPatch=2 > kInclude=1`——**patch 先于 include 执行**。此时目标节点的值还是 `__include: default:/xxx` 这个字符串，不是 map，dict-merge 失败被吞掉。

**正确做法**：在 patch 里重述 include，覆盖项写在它下面：

```yaml
patch:
  punctuator/half_shape:
    __include: default:/punctuator/half_shape
    '/': '、'
```

**验证**：`build/<name>.schema.yaml` 里能看到覆盖项。

> 本项目实例：`output/data/user-custom/rime_ice.custom.yaml` 与 `double_pinyin_sogou.custom.yaml` 的注释段记录了完整溯源（v0.19.0.59 / commit 1c0d711 首次迁移后按 `/` 仍输出 `/`）。

**反向陷阱**：如果目标节点**本来就不是** include 引入的（直接是 map），写上 `__include:` 反而会把内容清空。**先看源 schema 再决定写不写。**

## T2 · Punctuator 用 unshifted ASCII 作映射键，对 Shift 完全不感知

**现象**：`punctuator/half_shape` 里写了 `'Shift+comma': '《'` 之类**毫无效果**；或者 `Shift+,` 仍然输出 `,`。

**根因**：`librime/src/rime/gear/punctuator.cc` 的 mapping key 是**未加 Shift 的 ASCII 字符**，它拿不到 Shift 状态。

**正确做法**：两层分工——

- 直接按的标点 → `punctuator/half_shape`（覆写 T1 的 include 陷阱）；
- `Shift+标点` → `key_binder` 拦截后 `send` 目标字符。

```yaml
patch:
  key_binder/bindings/+:
    - { when: always, accept: "Shift+comma", send: "《" }
```

## T3 · 开关跨方案继承，除非显式 `reset`

**现象**：从【朙月拼音】切到别的方案，繁简/中英状态莫名变了；或反向——某方案的"独立简体开关"影响了别的方案。

**根因**：切方案时，**没有显式 `reset` 的开关保留上一方案的状态**。

**正确做法**：

- 想固定初始态 → 显式 `reset: 0/1`；
- 想彻底隔离 → 用**独立开关名**，并让 `simplifier/option_name` 指向它（内置【简化字】方案就是这么做的：`zh_simp`）。

## T4 · 列表类配置无法增删单项，只能整表替换

**现象**：`patch: { schema_list: [ ... ] }` 加一个方案后，原来的方案全没了。

**根因**：YAML 列表没有「按项合并」的语义，补靪对列表的默认操作是**替换**。

**正确做法**：

- 用 `/+` **追加**（`key_binder/bindings/+`、`schema_list/+`）；
- 或整表重写（记得把所有项都列全）。

**注意**：`@before` / `@after` / `@next` 官方明确**不建议**在补靪中使用。

## T5 · `bindings` 写成 list 而不是 list of map → 整段静默失效

**现象**：`key_binder/bindings` 整段没反应，日志干净。

**根因**：格式错了。正确形态是 `- {when:…, accept:…}` 的**列表 of map**。

**正确做法**：

```yaml
key_binder/bindings:
  - { when: composing, accept: Return, send: Escape }
```

## T6 · `accept` 的修饰符**严格匹配**

**现象**：`accept: Control+9` 绑定了，但用户只按 `9` 时也触发了（或反之，按 `Control+9` 不触发）。

**根因**：修饰符必须完全一致才算命中。

**正确做法**：想同时覆盖裸键与带修饰键，**写两条**；`always` 的绑定排在具体条件之后（只触发第一个命中的）。

## T7 · `when: paging` 需要先翻过页

**现象**：`when: paging` 的翻页键没生效。

**根因**：`paging` 表示「用户已按过 `Page_Down` 之类的翻页键」，**先有翻页才有 paging 状态**。

**正确做法**：翻页类绑定用 `has_menu`；`paging` 用于「翻页后才有意义」的场景。

## T8 · `recognizer/patterns/punct` 会盖掉 punctuator

**现象**：中文标点怎么设都不对。

**根因**：内置的 `punct` 识别规则把标点输入接管了，优先级高于 `punctuator`。

**正确做法**：把 `key_binder` 放到 `engine/processors` 头部，抢在识别之前拦截。

## T9 · 用户词典被输入法独占

**现象**：`rime_dict_manager` 报错、或用户词典改不动。

**根因**：输入法运行时以**独占方式**打开词典文件。

**正确做法**：先退出输入法再操作词典工具。

## T10 · 版本号不加引号会被解析成数值

**现象**：`version: 0.10` 变成 `0.1`。

**正确做法**：`version: '0.10'`（一律加引号）。

## T11 · 编码回显格式里的 `xlit` 用 UTF-32 而非 UTF-8

**现象**：`xlit` 替换非 ASCII 字符时长度对不上。

**根因**：只有 `xlit` 一个算子把运算元与参数按 UTF-32 处理（按 Unicode 字符数计长度）；其余算子按 UTF-8。

**正确做法**：`xlit` 的左右字母表**必须等长**（按 Unicode 字符数），否则整体错位。

## T12 · 部署日志里的 `invalid key binding #N`

**现象**：部署报 `invalid key binding #N, with invalid accept pattern: ...`。

**根因**：第 N 条绑定的 `accept` 键名拼错或大小写错。

**正确做法**：按日志里的 #N 直接定位到 `key_binder/bindings` 的第 N 条；键名查 X11 keysym（去 `XK_` 前缀）：`comma` `period` `slash` `semicolon` `apostrophe` `backslash` `bracketleft` `bracketright` `grave` `numbersign`。**字面字符加引号**。

## T13 · 同一文件两个 `patch:` 键

**现象**：前面的补靪"莫名其妙消失"。

**根因**：YAML 同名键后者覆盖前者，**不报错**。

**正确做法**：一个 `.custom.yaml` 只有一个 `patch:` 键；后续内容同缩进追加。

## T14 · 部署产物与源文件不是一一对应

**现象**：在源文件里找不到自己写的某段配置。

**根因**：编译结果被**重组**为 `default` + 各方案配置。

**正确做法**：验收看 `build/`，不要在源文件里找。
