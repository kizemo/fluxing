# GBRAIN：把大脑当外置检索

> 本技能的 `chapters/` 与 `references/` 是**热路径**（自足、离线可用）。
> GBrain 里的 44 页 Rime 语料是**深水区**：需要逐字溯源、跨文档比对、查上游原文时用。

## 前提（踩过的坑，务必先读）

1. **必须带 `--source rime`**。不带会被路由到 `default` 源，报
   `sole non-default source 'rime' exists, but 'default' is non-empty — routing to 'default'`，
   然后对不存在的 slug 返回空——**看起来像"没有数据"，其实是源选错了**。
2. **该脑 `embedding_disabled: true`**，检索是**纯关键词**（tsvector）。
   `gbrain query "如何定制每页候选数"` 这种问句会返回 `keyword_zero`；
   `gbrain search page_size`、`gbrain search 補靪` 这种**词/标识符**才命中。
   → **验收用 `gbrain search`，不要用 `gbrain query`。**
3. **CLI 直连需要数据库独占权**。若 `gbrain` 报 `pglite_busy`，
   说明有另一个 `gbrain serve`（通常是 Claude Code 的 gbrain 插件）持锁；
   官方处置是停掉那个 serve，跑完命令后让客户端重启它。**不要删锁文件。**

## 常用检索

```powershell
# 查一页的入链（双链质量）
gbrain backlinks --source rime customizationguide

# 查一页的出链与邻域
gbrain graph --source rime customizationguide --depth 2

# 关键词检索（中文词与 ASCII 标识符都行）
gbrain search "simplifier uniquifier" --source rime
gbrain search "補靪" --source rime
gbrain search "menu/page_size" --source rime

# 孤立页（双链健康度）
gbrain orphans --json --source rime

# 边按来源统计
gbrain link-sources --source rime

# 源健康面板
gbrain sources status rime --json
```

## 什么时候值得查 GBrain

| 场景 | 查什么 |
|---|---|
| 需要**逐字**引用上游原文 | `gbrain search <关键词> --source rime` → 结果里带原文片段 |
| 想知道某页的**上下文网络** | `gbrain backlinks` / `gbrain graph` |
| 想确认某个配置项在**原文**怎么表述 | `gbrain search <配置项路径> --source rime` |
| 想知道**两个概念怎么关联** | `gbrain graph <slug> --depth 2` |

## 什么时候不必查

- 只是要一个改法 → 读 `references/cookbook.md`；
- 只是要一个配置项的含义 → 读 `references/settings-reference.md`；
- 遇到没见过的现象 → 读 `references/troubleshooting.md`；
- 改完要验证 → 看 `build/` 编译结果 + 日志。

> **GBrain 是补充，不是前置。** 本技能的 `brain_first: exempt` 就是在说这件事：
> 不依赖大脑也能完成绝大多数任务。

## 写入（谨慎）

```powershell
# 往大脑补一条带语义的边（动词必须在 gbrain-base-v2 的 17 个里）
gbrain link "digest/04-定制指南速查" customizationguide `
  --link-type derived_from --link-source rime-digest --source rime
```

可用动词：`derived_from` `discusses` `mentions` `relates_to` `sourced_from` `supersedes`
`redirects_to` `authored` `attributed_to` `partner_of` `owes_to` `attended`
`advises` `founded` `works_at` `invested_in` `awaiting_reply_from`。

> `mentions` 是自动边（markdown 里的 wikilink 抽取），`markdown` / `frontmatter` /
> `wikilink-resolved` 是保留字，`--link-source` 不能用它们。
