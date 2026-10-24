# Cheatsheet：一页决策表

> 改配置时贴着手边看。决策规则优先，片段去 [cookbook.md](cookbook.md)。

## 一、我该改哪个文件？

| 我想改… | 文件 | 依据 |
|---|---|---|
| 所有方案都受影响（候选数、方案菜单、全局快捷键、默认配置） | `default.custom.yaml` | 它是 `default.yaml` 的补靪 |
| 只影响一个输入方案（该方案的标点、词典、逐键提示） | `<方案代号>.custom.yaml` | 它是 `<方案代号>.schema.yaml` 的补靪 |
| 界面外观（字体、配色、横排、托盘） | `weasel.custom.yaml` / `squirrel.custom.yaml` | 前端配置 |
| 某程序内关闭中文 | `weasel.custom.yaml`（`app_options/<小写程序名>`）/ `squirrel.custom.yaml`（`app_options/<BundleID>`） | 前端配置 |
| 词库内容 | `*.dict.yaml`（新建，别改发行版自带的） | 词典源文件 |
| 自定义短语（不想建词典） | `custom_phrase.txt` | 文本词典 |
| 用户词典里的词频与习惯 | 用 `rime_dict_manager` / 小狼毫「用户词典管理」 | 运行时数据 |

**判断法则**：这个设置属于「引擎行为」「方案行为」还是「前端显示」？行为 → 方案层；显示 → 前端层。

## 二、补靪速查

| 操作 | 写法 |
|---|---|
| 改一个标量 | `patch:` → `"一级/二级": 新值` |
| 覆盖一个 map 节点 | `patch:` → `一级/二级:` + 若干键值（**若该节点是 `__include` 引入的，必须先重述 `__include:`**） |
| 追加到列表 | `patch:` → `一级/列表/+:` + 列表项 |
| 替换整个列表 | `patch:` → `一级/列表:` + 完整列表 |
| 改列表第 n 项 | `patch:` → `"一级/列表/@0/键": 值` |
| 改列表末项 | `patch:` → `"一级/列表/@last/键": 值` |
| 插入到某项前 | `patch:` → `'一级/列表/@before 0/键': 值`（**不建议**） |
| 追加到末尾 | `patch:` → `一级/列表/@next:` 或 `/+`（**用 `/+` 更稳**） |
| 引用另一文件节点 | `patch:` → `default.custom:/patch?`（`?` = 目标不存在不报错） |

## 三、开关（switches）决策

| 我想… | 写法 |
|---|---|
| 某方案默认简体 | 该方案 `switches` 里给 `simplification` 加 `reset: 1` |
| 某方案默认英文 | `patch:` → `"switches/@0/reset": 1`（`@0` = 第一个开关 ascii_mode） |
| 某方案**独立**的繁简开关 | 新起开关名（如 `zh_simp`）+ `simplifier/option_name: zh_simp` |
| 切方案后状态被继承 | 显式 `reset: 0/1`；或用独立开关名隔离 |
| 切换中西文 | `ascii_composer/switch_key`（不是 `key_binder`） |

**阈值与默认值**：`menu/page_size` 范围 1~9（个别发行版支持 10），默认 5。

## 四、快捷键决策

| 我想… | 写法 |
|---|---|
| 改键 | `key_binder/bindings` 加一条 `{when:…, accept:…, send:…}` |
| 追加而不覆盖已有绑定 | `key_binder/bindings/+` |
| 翻页 | `send: Page_Up` / `Page_Down` |
| 选第 n 个候选 | `send: n`（数字） |
| 上屏当前编码 | `{when: composing, accept: Return, send: Escape}` |
| 切下一个方案 | `select: ".next"` |
| 切换某个开关 | `toggle: <开关名>` |
| 改编码区显示 | `editor/bindings` 或 `translator/preedit_format`（可用拼写运算改写） |

**`when` 条件（5 种）**：`always`（无条件，排最后）、`composing`（输入码非空）、`has_menu`（有候选）、`paging`（**翻过页后**才成立）、`predicting`（少用）。**只触发第一个命中的绑定**；具体场景排前面，`always` 排最后。

**`action`（互斥，5 选 1）**：`send` / `send_sequence` / `toggle` / `set_option` / `unset_option` / `select`。
`when` 与 `accept` 必填；修饰符**严格匹配**（`accept: Control+9` 不匹配裸按 9）。

**修饰符顺序固定**：`Shift → Control → Alt`。键名大小写敏感（`comma` ✅ / `Comma` ❌）。

## 五、标点决策

| 场景 | 落点 |
|---|---|
| 直接按的标点（`, . / ;`） | `punctuator/half_shape`（**若源 schema 用 `__include` 引入，先重述 include**） |
| 全角标点 | `punctuator/full_shape` |
| **Shift+标点** | `key_binder` 拦截（Punctuator 用 unshifted ASCII 作键，对 Shift 不感知） |
| 数字后的 `.` `:` | `punctuator/digit_separators`（默认 `.` `:`）、`punctuator/digit_separator_action`（`commit` = 直接上屏西文，代价是失去双击上中文句号） |
| 识别特殊串为西文 | `recognizer/patterns/<名>: "<正则>"` |

## 六、阈值与默认值速查

| 项 | 值 |
|---|---|
| 每页候选数 | 1~9（默认 5） |
| 繁简转换开关名（多数方案） | `simplification`；内置【简化字】用 `zh_simp` |
| 繁简转换必需的 filters | `simplifier` + `uniquifier`（缺一不可） |
| 方案选单快捷键默认 | `Ctrl+\`` 或 `F4` |
| 候选数键位 | 数字键（注音用 `Shift+数字`） |
| 词典正文分隔 | Tab，三列：文字、编码、频次 |
| 词典编码 | UTF-8 **无 BOM** |
| 列表算子 | `@0` 起，`@last`、`@before N`、`@after N`、`@next` |
| 合并/替换算子 | `/+` 合并、`/=` 替换 |
| 路径可选标记 | 末尾 `?`（目标缺失不报错） |

## 七、"闻到"就该警觉（tells & smells）

| 现象 | 真相 |
|---|---|
| 部署日志干净但功能没变 | dict-merge 静默失败 → 查 `__include` 冲突 |
| 改了一个方案，另一个方案也变了 | 开关跨方案继承，或改动落在了 `default.custom.yaml` |
| `build/` 里有你的值但行为不对 | 语义问题：`when` 条件、开关继承、Punctuator 不感知 Shift |
| 候选里两个一样的字 | `simplifier` 没配 `uniquifier` |
| 候选带 ☯ | 混元编码器猜的词 |
| 部署报 `invalid key binding #N` | 第 N 条绑定的 `accept` 拼错 |
| 用户词典改不动 | 输入法在运行，独占着词典 |
