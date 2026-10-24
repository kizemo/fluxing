# SOURCE：语料来源与校验记录

> 本技能的每一条内容都可回溯到具体来源。下面是来源清单与**当时做的校验**。

## 一、上游 Rime 官方 Wiki（证据层）

抓取方式：**直接 clone wiki 的 git 仓库**（GitHub Wiki 本身是 git 仓库），
不用网页另存——避免 HTML 渲染层丢表格、丢缩进、丢锚点。

| 仓库 | 地址 | 页数 | 本地位置 |
|---|---|---|---|
| rime/home | `https://github.com/rime/home.wiki.git` | 25 | `E:\obsidian\rime\` 根目录 |
| rime/librime | `https://github.com/rime/librime.wiki.git` | 3 | `E:\obsidian\rime\librime\` |
| rime/squirrel | `https://github.com/rime/squirrel.wiki.git` | 7 | `E:\obsidian\rime\squirrel\` |

### 校验记录（2026-10-22）

| 校验项 | 方法 | 结果 |
|---|---|---|
| 完整性 | 本地克隆 HEAD vs `git ls-remote` 远端 HEAD | 均为 `4f4a82dcbd12bf3d872213402ec1099e7303f462`，**一致** |
| 逐字节 | 25 份文件逐份 MD5 与上游克隆比对 | **0 处不符** |
| 页面数 | 线上 HTML 侧栏清单 vs git 仓库文件清单 | 均为 **25**，无遗漏 |
| 《定製指南》 | 33,198 字节，目录 47 行 + 正文 855 行 | **无截断** |

### 页面清单（25 页）

`Home` `Introduction` `UserGuide` `CustomizationGuide` `GettingStarted` `Downloads` `FAQ`
`Recipes` `MoodCollection` `UserQuotes` `UserData` `SharedData` `Configuration`
`RimeWithSchemata` `SpellingAlgebra` `ComboPinyin` `DictionaryPack` `RimeWithTheDesign`
`RimeWithTheCode` `RimeWithIBus` `RimeWithSquirrel` `RimeWithWeasel` `Rime-in-Emacs`
`WeaselReleaseManual` `SquirrelReleaseManual`

> 另外 10 页：`librime/{Home,Sample,_Footer}`、`squirrel/{Home,Shortcut-to-Sync-User-Dir,
> squirrel.yaml-配置指南,在Squirrel使用上思源字體,鼠鬚管：如何查找合用字體之名,
> 实现Shift_L-…,禁用-Squirrel-英文模式…}`。

## 二、中文消化层（加工层）

`E:\obsidian\rime\digest\` 下 9 篇，是本次为 GBrain 摄入而写的结构化笔记：

`01-Rime文档总索引` `02-Rime架构总览` `03-配置与补靪机制` `04-定制指南速查`
`05-方案与词典体系` `06-用户数据与部署` `07-常见问题与坑` `08-学习路线与文档地图`
`09-附加Wiki导读`

本技能的 `chapters/` 主要由这一层 + 上游原文合并提炼而来。

## 三、本项目既有资料（项目层）

| 文件 | 内容 | 用到哪 |
|---|---|---|
| `Rime-设置说明书-重点快捷键.md`（41 KB） | 本项目的权威：配置语法、6 种 when、5 种 action、12 种 editor action、§9 本机现状、§10 排错 | ch03/ch08、references/troubleshooting.md |
| `.research/rime-docs/Rime_description.md`（35 KB） | `Schema.yaml` 詳解（HTML 转存），设置项逐条说明 | references/settings-reference.md |
| `.research/rime-docs/Rime-customization-digested.md`（28 KB） | 引擎四组流水线、schema 全景、dict 全景、实战路线 | ch01/ch04 |
| `.research/rime-docs/鼠鬚管介面配置指南.md` | 鼠鬚管 Style / preset_color_schemes | ch08 |
| `output/data/user-custom/*.custom.yaml` | 项目真实补靪样例（rime_ice / radical_pinyin / double_pinyin_sogou） | ch02、references/librime-traps.md |
| `.specify/memory/lessons-learned.md` | 项目事故溯源 | references/librime-traps.md |

> **权威分层**：本项目的具体行为以 `Rime-设置说明书-重点快捷键.md` 为准；
> 本技能提供**通用机制 + 通用处方**，不替代它。两者冲突时以项目文档为准。

## 四、链接规范约定（踩过的坑）

| 系统 | `[[A|B]]` 含义 | 裸 `[[name]]` 解析 |
|---|---|---|
| GitHub Wiki | A=显示名，**B=目标页** | 按页面名 |
| Obsidian | **A=目标页**，B=显示名 | 按文件名 |
| GBrain | A=显示名，**B=目标页** | 按 **slug 路径**（basename 全局解析是 opt-in） |

→ 跨工具搬运时**一律用单段式 `[[页名]]`**。上游 38 条链接全是 GitHub 别名式：
GBrain 能解析，Obsidian 侧显示为未解析——所以中文消化层才用单段式重写。

## 五、GBrain 入库状态

| 项 | 值 |
|---|---|
| source id | `rime` |
| local_path | `E:\obsidian\rime` |
| 页数 / chunks | 44 / 393 |
| 链接边 | 166（117 markdown + 34 语义边 + 15 路径解析边） |
| 孤立页 | **0** |
| 检索 | `gbrain search <kw> --source rime`（该脑 `embedding_disabled: true`，**纯关键词**；`gbrain query` 问句不可用） |

用法见 [GBRAIN.md](GBRAIN.md)。

## 六、维护

上游 wiki 变动了，用同一套流程刷新：

```powershell
git -C E:\obsidian\rime pull            # 拉取上游（若配了 remote）
gbrain sync --source rime               # 重新入库
# 然后更新本技能的 chapters/references，并在此记录新的校验结果
```
