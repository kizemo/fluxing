# 接力 prompt（下一会话用 · 2026-10-30）

> **用法**：把下面「一句话 prompt」整段复制到新会话的第一条消息即可。
> 文档内容不在此重复 —— 细节全在交接卡 §24 与各专项文档里。

---

## 一句话 prompt（复制这一段）

```
接手 Fluxing（火流猩输入法 / RIME 前端 fork，仓库 F:\soft\00selfmade\rime_claude，分支 Fluxing，远端名 kizemo）：
请先按顺序读完下面这些文件，掌握本项目的身份/不变量、操作手册与危险区、宪法条款、部署与 NSIS 配方、工程技能消费契约，
以及本轮交接正文 §24（7 条用户真机反馈的根因、改法、验收清单与下阶段计划），然后直接按 §24 推进，不要复述文档内容、不要从零重做已完成的事：
F:\soft\00selfmade\rime_claude\CLAUDE.md
F:\soft\00selfmade\rime_claude\CONTEXT.md
F:\soft\00selfmade\rime_claude\AGENTS.md
F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md
F:\soft\00selfmade\rime_claude\.specify\memory\project-knowledge.md
F:\soft\00selfmade\rime_claude\docs\agents\domain.md
F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-v1.0.0-bare-shift-ascii-toggle-2026-10-28.md   ← 交接正文在 §24
F:\soft\00selfmade\rime_claude\docs\adr\README.md
```

---

## 各文档用途（给新会话的导航，不含正文）

| 路径 | 作用 |
|---|---|
| `F:\soft\00selfmade\rime_claude\CLAUDE.md` | 进站指引：5 份必读、强制项、分流规则 |
| `F:\soft\00selfmade\rime_claude\CONTEXT.md` | 身份 / 模块 / 词汇 / 不变量 |
| `F:\soft\00selfmade\rime_claude\AGENTS.md` | 操作手册首屏 + 危险区 + checklist |
| `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md` | 宪法 R1–R9 / P1–P8（改动 ≥3 文件 ⇒ 必须 spec/plan/tasks） |
| `F:\soft\00selfmade\rime_claude\.specify\memory\project-knowledge.md` | 部署布局 + NSIS 配方 + 本机注册表事实 |
| `F:\soft\00selfmade\rime_claude\docs\agents\domain.md` | 工程技能消费契约 |
| `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-v1.0.0-bare-shift-ascii-toggle-2026-10-28.md` | **完整交接正文**（§1–§23 历史，**§24 = 本轮**） |
| `F:\soft\00selfmade\rime_claude\docs\adr\README.md` | ADR 索引（MADR 格式） |
| `F:\soft\00selfmade\rime_claude\docs\archive\prompts\` | 历次接力 prompt 存档 |

## 硬约束提醒（详见宪法，不在此展开）

- `librime/` 只读；不引新第三方依赖；**本机 Windows Sandbox 门禁跑不起来 ⇒ 不碰 `output/install.nsi` 与 `output/data/user-custom/**`**。
- 用户可见文案一律**简体中文**；TSF 回调内**禁止阻塞 I/O**。
- 提交用 Conventional Commits + 模块 scope；用户可见改动必须写 `CHANGELOG.md` 的 `### 主要更新`。
- 发布前同步 `weasel.props`（生成物）/ `env.bat`（不入仓）/ `build.bat` / `xbuild.bat` 的 `VERSION_*`。
- 推送：`git push kizemo Fluxing`（**没有 origin**）。
- 中间产物写文件，对话只留指针 + 结论；单轮工具调用 ≤10。
