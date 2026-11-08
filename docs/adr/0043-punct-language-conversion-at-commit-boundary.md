# ADR 0043 · 半角标点(英文标点)由 fork 在提交边界做标点语言转换

- 状态:Accepted
- 日期:2026-11-08
- 关联:ADR 0040(部分作废)、ADR 0041(补充)、ADR 0042(同批次 · 会话门禁)
- 影响模块:`RimeWithWeasel`(提交边界)、`include/FluxingPunctText.h`(新增)
- 不影响:`librime/`(P3)、`output/data/**/*.yaml`(本决策不改表)、IPC 协议

## 背景

用户 2026-11-08 验收项②:「手点面板『中英文标点』切到半角 → 中文态打 `,`,
确认变成半角逗号;再点回,确认变 `,`」。

真机证据(`tmp/batch-i3.log`,探针 v4):**三态**(state0 / state1 / state2)打出的逗号
全部是 U+FF0C(中文逗号);而槽 2 图标像素**确实翻转**(state0 ink=47 bbox 18×24 →
state1 ink=66 bbox 23×26)⇒ 面板状态变了、引擎输出没跟上 ⇒ 这是真实缺陷,不是探针假象。

## 根因(源码级)

`librime/src/rime/gear/punctuator.cc`:

- `:23-30` `PunctConfig::LoadConfig` 选表**只看** `full_shape` option:
  `shape(full_shape ? "full_shape" : "half_shape")`;`:26-28` 按 shape 缓存,形状变了才重载。
- `:102-104` `if (ctx->get_option("ascii_punct")) return kNoop;` ⇒ `ascii_punct`
  只让**处理器**短路,不换表。
- `:116`(处理器)/ `:249`(分段器)/ `:329`(翻译器)三条路径每次都 `LoadConfig`
  ⇒ 表始终跟随 `full_shape`。
- 翻译路径(`:316-351`)完全不读 `ascii_punct`。

本 fork 的表(`output/data/default.yaml`):`punctuator/full_shape`(`:127-160`)与
`punctuator/half_shape`(`:161-193`)**都**映射中文标点 ⇒ 无论 `full_shape` 取什么值,
引擎吐出来的都是中文标点。

⇒ 结论:「半角标点」在引擎层**没有可用开关** —— `ascii_punct` 改不了表,
`full_shape` 能改表但会连带全角化。

## 为什么不用 `full_shape` 选表

`librime/src/rime/gear/shape.cc`:

- `:16-36` `ShapeFormatter::Format`:`full_shape` 为真时把 ASCII 全转全角;
- `:38-55` `ShapeProcessor::ProcessKeyEvent`:`full_shape` 为真时把 ASCII 键直接转全角上屏。

本 fork 的 schema 未把 `shape` 组件挂进 engine 链(`output/data/rime_ice.schema.yaml:43-52`
只有 `ascii_composer` / `recognizer` / `key_binder` / `speller` / `punctuator` /
`selector` / `navigator` / `express_editor`)⇒ `ShapeProcessor` 是哑的;但
`PunctTranslator` 自带 `ShapeFormatter formatter_`(`punctuator.h:88`),在
`punct_number` 分支(`:318-323`)对数字分隔符生效 ⇒ 若用 `full_shape` 选表,
**半角标点态下「3.14」会变成「3．14」**(全角点)。

更根本的:`full_shape` 一开,数字与 `@#%` 会被全角化,直接违反用户硬要求
「中文标点态下数字与 @#% 恒半角」(ADR 0041 已记录 `full_shape` 与「标点语言」无关)。

## 决策

1. **不改 `librime/`**(P3)、**不碰 `full_shape`**。
2. 由 fork 在**提交边界**做标点语言转换:半角标点状态(引擎 `ascii_punct` 为真)下,
   把上屏文本里「纯标点」提交中的中文标点换成对应 ASCII 标点。
3. 钩子位置:`RimeWithWeasel.cpp` `_Respond()` —— 全仓**唯一**一处 `get_commit`
   (grep 确认),所有上屏文本都从这里出去,无遗漏路径。
4. 状态事实源:live `RimeStatus.is_ascii_punct`(与面板图标同一个事实源);
   读不到才回落服务端镜像 `m_ascii_punct`。
5. 转换范围:只作用于「整串都是标点」的提交(`IsPunctuationOnly`);词库短语、
   数字、字母一律原样 ⇒ 不会把词条里的「，」改掉。
6. 纯函数放在 `include/FluxingPunctText.h`,生产代码与 `test/TestWeaselIPC`
   单测共用同一实现(constitution R7)。

## 后果

- ✅ 「半角标点 ⇒ 英文标点」在不改引擎的前提下成立;面板图标与实际输出同源同状态。
- ✅ 数字与 `@#%` 恒半角(本决策完全不碰 `full_shape`)。
- ✅ 中文标点态零改动(引擎照旧吐中文标点,转换只在半角态生效)。
- ⚠ 词库短语里的中文标点在半角标点态下**不**转换(有意为之:半角标点只影响
  「标点键」,与搜狗/微软的「半角标点」一致)。
- ⚠ 反向映射有歧义的少数字符按本 fork 自己的 `half_shape` 表反推
  (`` ` ``→`·` ⇒ `·`→`` ` ``,`$`→`¥` ⇒ `¥`→`$`)。
- ⚠ `output/data/t9.schema.yaml` 没有 `switches:` 段 ⇒ `ascii_punct` 未声明 ⇒
  `set_option("ascii_punct")` 静默失败 ⇒ t9 方案下本机制不生效
  (既有缺口,不在本批范围;批次 H 已在 `radical_pinyin.schema.yaml` 记过同类坑)。

## 备选方案与否决理由

| 方案 | 否决理由 |
|---|---|
| 用 `full_shape` 选表 | 半角态下「3.14」→「3．14」;且 `full_shape` 语义被污染,违反 ADR 0041 |
| 改 `librime/punctuator.cc` 让翻译路径读 `ascii_punct` | 违反 P3(不改 librime) |
| 面板按钮直接切 `ascii_mode` | 语义不同:用户要「中文态 + 英文标点」 |
| 在 `output/data` 把两张表拆成中/英两套 | 表的选择**只**由 `full_shape` 决定,不引入 `full_shape` 就无法切换 ⇒ 等价于上一个否决项 |
