# Troubleshooting：症状 → 探测 → 原因 → 处理

> 每条都给出**怎么探测**，因为「症状」在 Rime 里高度同形（都表现为"没反应"）。
> 通用排查顺序见 [ch07](../chapters/ch07-排错手册.md)；行为级根因见 [librime-traps.md](librime-traps.md)。

## 通用探测手段（先会这四招）

| 手段 | 怎么做 | 能回答什么 |
|---|---|---|
| **看 build/** | `<用户文件夹>/build/<name>.schema.yaml` | 我的值到底进没进编译结果 |
| **看日志** | 小狼毫 `%TEMP%\rime.weasel.*`；鼠鬚管 `$TMPDIR/rime.squirrel.*`；ibus `/tmp/rime.ibus.*` | 哪条绑定非法、哪个文件报错 |
| **删了重建** | 把 `<name>.custom.yaml` 移走 → 部署 → 观察 baseline | 问题是不是我的补靪引起的 |
| **二分 patch** | 补靪拆成两半，逐半加 | 哪一半出锅 |

> 日志级别 INFO / WARNING / **ERROR**。排查时重点看 WARNING 与 ERROR。
> 看到 `invalid key binding #N` 就能直接跳到 `key_binder/bindings` 第 N 条。

---

## A. 改了没反应

| 症状 | 探测 | 原因 | 处理 |
|---|---|---|---|
| 改了源文件，行为无变化 | 部署了吗 | 没部署 | 重新部署 |
| 部署了，build/ 里没有我的值 | 看 build/ | 补靪没进编译 | 查 T13（两个 `patch:`）/ T1（include 冲突）/ 路径语法 |
| build/ 里有值，行为仍不对 | 现场复现 | 语义问题 | 查 `when` 条件、开关继承、Punctuator 不感知 Shift |
| 只有某一个方案没生效 | 切到别的方案试 | 改动落在 `default.custom.yaml`（全局）或开关继承 | 确认改动面，见 cheatsheet 第一节 |
| 别的方案也变了 | 对比 build/ | 改的是全局文件，或开关跨方案继承 | 改用 `<方案>.custom.yaml` + 独立开关名 |

## B. 部署报错

| 症状 | 原因 | 处理 |
|---|---|---|
| `invalid key binding #N, with invalid accept pattern` | 第 N 条 `accept` 键名拼错/大小写错 | 查 X11 keysym；字面字符加引号 |
| 部署卡住/很慢 | 在编译大词典（essay.txt 6MB） | 正常，等待；或临时禁用该方案 |
| 部署后 `Ctrl+\`` 能开选单但方案不可用 | 方案没部署成功 | 看日志里 `E` 开头的行 |
| 编译结果里 `punctuator/half_shape` 是空的 | patch 里写了 `__include:` 但源节点本不是 include | 删掉那行 `__include:` |
| 部署后 `build/` 里某段消失 | 补靪整表替换了列表 | 用 `/+` 追加而不是替换 |

## C. 候选与上屏

| 症状 | 原因 | 处理 |
|---|---|---|
| 候选里两个一样的字（钟） | `simplifier` 没配 `uniquifier` | `engine/filters` 加 `uniquifier` |
| 候选带 ☯ 阴阳鱼 | 混元编码器猜的词 | 确认是否要；或关 `translator/enable_sentence` |
| 码表类方案出不了长句 | `enable_sentence` 被关 | 打开它（只对 `table_translator` 有效） |
| 逐键提示太多 | `enable_completion` 默认开 | `translator/enable_completion: false` |
| 用户词典不起作用 | 方案关了用户词典 | `translator/enable_user_dict: true` |
| 候选排序很怪 | `initial_quality` / 用户字频 | 调 `translator/initial_quality`；查用户词典 |
| 上屏的是编码而不是字 | 语句流方案用回车是整句上屏 | 先按左 Shift 切西文再回车 |
| 换方案后繁简变了 | 开关继承 | 显式 `reset` 或独立开关名 |
| 繁体用字不符合预期（為/為/著） | 用了 `t2s` 而不是 `t2tw` | `simplifier/opencc_config: t2tw.json` + `zh_tw` 开关 |

## D. 标点

| 症状 | 原因 | 处理 |
|---|---|---|
| 中文标点怎么设都不对 | `recognizer/patterns/punct` 接管了 | `key_binder` 放到 `engine/processors` 头部 |
| `Shift+,` 仍输出 `,` | Punctuator 不感知 Shift | 用 `key_binder` 拦截 `Shift+comma` |
| 直接按 `/` 改不动 | 撞 include 优先级 | 在 patch 里重述 `__include:`（T1） |
| 数字后 `.` 不变西文句点 | `digit_separator_action` 是 `no_confirm` | 设 `commit`（代价：失去双击上中文句号） |
| 双击 `.` 上不了中文句号 | 同上，`commit` 的代价 | 在 `digit_separators` 里去掉 `.` |
| 顿号变成了「、」但顿号键位不对 | 符号表是 unshifted ASCII 作键 | 确认你改的是 `half_shape` 还是 `full_shape` |

## E. 快捷键

| 症状 | 原因 | 处理 |
|---|---|---|
| 绑定整段失效 | 写成 list 而非 list of map | `- {when:…, accept:…}` |
| `when: paging` 不生效 | 需先翻过页 | 改 `has_menu` |
| `Shift+Tab` 无效 | 修饰符顺序 | `Shift+Tab`，不是 `Tab+Shift` |
| `Control+grave` 无效 | 大小写/引号 | 字面字符加引号 |
| `Control+9` 绑了但裸按 9 也触发 | 修饰符严格匹配 | 写两条 |
| 单独 Shift 切不了中英（小狼毫） | key_binder release 事件 modifier 比较缺陷 | 走 `ascii_composer/switch_key` |
| `Ctrl+Shift+9` 切不动标点 | 被系统 hotkey 抢 | 改 `Control+Shift+numbersign` |
| 新绑定不生效但旧的生效 | 追加位置不对/被前面的条目命中 | 用 `/+` 追加；具体条件排前，`always` 排后 |
| `send` 出去的键没反应 | 键名不存在于上下文 | 换成 `send_sequence` 组合，或用 X11 键名 |

## F. 词典与用户数据

| 症状 | 原因 | 处理 |
|---|---|---|
| 词典工具报错 | 输入法独占词典 | 退出输入法再操作 |
| 导入的词条检索不到 | 编码形式不是码表里的标准形式（简拼/异拼） | 用标准编码；或改成固态词典 |
| 导入后乱码 | 文件带 BOM | `UTF-8 无 BOM`（Vim `:set fenc=utf8 nobomb ff=unix`） |
| 导入后整行错位 | 分隔符不是 Tab | 必须 Tab 分隔 |
| 繁简不一致 | 字形与源码表不同 | 用 OpenCC 转换后再导入 |
| 扩展包查不到词 | 扩展包与主词典音节表不一致 | 用同一份音节表构建 |
| 自定义短语不生效 | 方案没启用自定义短语设置 | 朙月拼音已内置；自建方案需加两处代码 |
| 快照合并后词频异常 | 合并取较大值叠加 | 预期行为，非 bug |

## G. 环境与前端

| 症状 | 原因 | 处理 |
|---|---|---|
| 小狼毫设置面板里没有某选项 | 版本差异 | 0.9.16+ 才有 `app_options`；0.9.6+ 才有鼠鬚管配色 |
| macOS 上左右 Shift 行为一样 | 鼠鬚管不区分左右 Shift | 改用 `Caps_Lock`/`Control` |
| 某程序里中文自动开启 | 没配 `app_options` | Windows 用小写程序名；macOS 用 Bundle Identifier |
| `inline_preedit` 无效 | 只在 TSF 支持 | 旧版前端不支持 |

## H. 什么时候该跳出 Rime 思维

| 症状 | 说明 |
|---|---|
| 引擎段崩溃/断言 | 不是配置问题 → librime 源码与测试 |
| TSF 前端卡死/不响应 | 前端实现问题 → `WeaselUI` / `WeaselTSF` 专项规则 |
| 安装器行为异常 | 安装流程 → `WeaselSetup` 与 NSIS 配方 |
| 改 `.yaml` 生效但升级后丢失 | 你改的是发行版文件 → 移到 `user-custom/*.custom.yaml` |
