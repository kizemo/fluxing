请接手火流猩（Fluxing）仓库 `F:\soft\00selfmade\rime_claude` 的「用户登录 + 多端同步」阶段规划落地工作。
本批已完成的是**规划**（spec 100 三件套 + ADR 0039 + 010 取代指针 + 交接双产物），**尚未写任何实现代码**。

请按此顺序自行掌握背景、目标、进度与下阶段计划，不要从零重做已完成的选型与取证，也不要改写历史 handoff / prompt：

1. `F:\soft\00selfmade\rime_claude\CLAUDE.md`（进站导航与强制项）
2. `F:\soft\00selfmade\rime_claude\AGENTS.md`（§1 首屏 / §4 危险区 / §5 checklist / §9 交接双产物）
3. `F:\soft\00selfmade\rime_claude\CONTEXT.md`
4. `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md`（R1–R9 / P1–P8）
5. `F:\soft\00selfmade\rime_claude\.specify\memory\project-knowledge.md`（§A 部署 / §11 NSIS 配方）
6. `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-07-fluxing-cloud-sync-plan.md`（**本批交接正文**：状态、进度、证据、决策 D1–D12、执行序、工具坑）
7. `F:\soft\00selfmade\rime_claude\.specify\specs\100-fluxing-cloud-account-sync\spec.md` 与 `plan.md` 与 `tasks.md`（**三件套**，tasks.md 是执行序）
8. `F:\soft\00selfmade\rime_claude\docs\adr\0039-cloud-account-sync-architecture.md`

下一步按 `tasks.md` 阶段 A 开工：T102（词库导出·导入合并语义取证，**结论未落盘前禁止写合并实现**）、T101（邮箱验证码通道选型）、T103（云端域名可达性）三条并行推进；
完成任一阶段任务时按 `AGENTS.md` §9 同步更新 `docs/archive/handoffs/` 与 `docs/archive/prompts/` 的双产物并更新 `CLAUDE.md` §2 指针。
