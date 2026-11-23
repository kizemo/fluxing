# 接力 Prompt · 下一会话（批次 BQ 之后）

请先按 `F:\soft\00selfmade\rime_claude\CLAUDE.md` 的「进站阅读」顺序读齐 5 份材料，再读本批真源交接文档，并**不要重做已完成的取证、已改完的代码与已上线的部署**：

- 本批真源（含 §0 接力结论速览、§4 根因定案、§5 代码改动落点、§6 生产复验实测、§7 门禁、§8 P0 阻塞、§9 未闭环清单、§11 工具坑、§12 待用户拍板、§13 清理口径、§14 Git 现状）：
  `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-22-batch-bq-register-mail-rejection-semantics.md`

配套材料（只给指针，内容请自行读原文）：

- 本批决策依据（ADR 0075 · 已 Accepted · 含实测回填）：`F:\soft\00selfmade\rime_claude\docs\adr\0075-register-mail-rejection-semantics-and-failed-registration-rollback.md`
- ADR 索引：`F:\soft\00selfmade\rime_claude\docs\adr\README.md`
- 上一批真源（BP · 推送吞吐 / 12002）：`F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-22-batch-bp-push-throughput-server-concurrency.md`
- 再上一批真源（BO · 12002 根因定案；其 §7 / §9-6 / §11-6 的「注册不可用」口径已由 BQ 修订）：`F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-22-batch-bo-push-batch-align-server-limit.md`
- 规格 100（BQ 批未改）：`F:\soft\00selfmade\rime_claude\.specify\specs\100-fluxing-cloud-account-sync\spec.md`
- 法则层（R1–R9 / P1–P8）：`F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md`
- 操作手册（含 §4 危险区 / §5 checklist / §9 交接规则）：`F:\soft\00selfmade\rime_claude\AGENTS.md`
- 部署 + NSIS 配方（§11）：`F:\soft\00selfmade\rime_claude\.specify\memory\project-knowledge.md`
- 服务端真源（BQ 改动落点）：`F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\src\mail.ts`、`F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\src\auth.ts`、`F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\src\errors.ts`、`F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\src\index.ts`
- 服务端单测（BQ 新增 + 追加）：`F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\test\register-mail-failure.test.ts`、`F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\test\mail-http.test.ts`
- 客户端真源（BQ 批禁改，仅供理解语义）：`F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\CloudAccount.cpp`、`F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\CloudTransport.cpp`、`F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\CloudAccountUi.h`
- 部署配置（路由 / D1 / R2 / vars）：`F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\wrangler.toml`
- D1 schema：`F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\migrations\0001_init.sql`、`F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\migrations\0002_password_resets.sql`
- 本批复验脚本（**凭据只在内存 / tmp，禁止入库**）：`F:\soft\00selfmade\rime_claude\tmp\bq-verify.ps1`、`F:\soft\00selfmade\rime_claude\tmp\bq-e2e-mailtm-v4.ps1`、`F:\soft\00selfmade\rime_claude\tmp\bq-v4-deploy-verify.ps1`、`F:\soft\00selfmade\rime_claude\tmp\bq-d1-cleanup.ps1`

请自行从上述文档掌握背景、目标、进度与下一步计划，不要凭本文档内容代替真源文档。

补充指针（2026-11-23 08:15 定时轮复验后更新，**内容仍以真源文档为准**）：本批真源新增 **§8.1（P0 定时轮复核实录：配额实测、BQ 判据③④ 复验结果、额度主因取证）**、**§8.2（额度判定非单调的实测矩阵、429 成因的源码级定案、失败注册「零写入」BR 候选、残留现状与下一轮动作）** 、**§8.3（配额已恢复 · BQ 判据③④ 全部达成 · 复验证据表 · 探针残留清理结果 · P0 状态变化）** 与 **§8.4（额度有余实测 · 残留零复发 · §13 构成更正 · 写放大第二来源 · 多行 SQL 坑）**；§0 / §9 未闭环清单 / §12 拍板项 / §13 清理口径均已随之更新；**BQ 批已于 2026-11-23 09:15 按用户拍板入库（fix + docs 两个 commit，未 push）**，BR 批下轮开工（ADR 0076）；复核脚本、探针残留清单与后续复验步骤见真源文档 §8.1 / §8.2 / §8.3 / §8.4 / §13。
