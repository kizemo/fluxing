# 接力 Prompt · 下一会话（批次 BP 之后）

请先按 `F:\soft\00selfmade\rime_claude\CLAUDE.md` 的「进站阅读」顺序读齐 5 份材料，再读本批真源交接文档，并**不要重做已完成的取证与已提交的修复**：

- 本批真源（含 §0 复核补记、§8 复核明细与现场复验、§9 下一步计划、§10 文档漂移与工具坑，以及当前状态、已验证/推断/未验证、决定性证据、清理口径）：
  `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-22-batch-bp-push-throughput-server-concurrency.md`

配套材料（只给指针，内容请自行读原文）：

- 本批决策依据（ADR 0074 · 已 Accepted）：`F:\soft\00selfmade\rime_claude\docs\adr\0074-push-batch-throughput-server-concurrency-and-client-timeout.md`
- 被修订的上一决策（ADR 0073 · 正文不改，决策 5 由 0074 修订）：`F:\soft\00selfmade\rime_claude\docs\adr\0073-client-push-batch-aligns-with-server-limit.md`
- 规格 100（BP 批未改）：`F:\soft\00selfmade\rime_claude\.specify\specs\100-fluxing-cloud-account-sync\spec.md`
- 法则层（R1–R9 / P1–P8）：`F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md`
- 操作手册（含 §4 危险区 / §5 checklist / §9 交接规则）：`F:\soft\00selfmade\rime_claude\AGENTS.md`
- 部署 + NSIS 配方（§11；§0 分支/版本口径已过期）：`F:\soft\00selfmade\rime_claude\.specify\memory\project-knowledge.md`
- 上一批真源（BO · 12002 根因定案）：`F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-22-batch-bo-push-batch-align-server-limit.md`
- 再上一批真源（BN）：`F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-22-batch-bn-sync-round-notices-visible.md`
- 服务端真源：`F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\src\r2-write.ts`、`F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\src\sync.ts`、`F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\src\wire.ts`
- 服务端单测：`F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\test\r2-write.test.ts`
- 客户端真源：`F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\CloudTransport.h`、`F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\CloudTransport.cpp`、`F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\CloudSyncService.h`、`F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\CloudSyncService.cpp`
- 客户端单测：`F:\soft\00selfmade\rime_claude\test\TestFluxingCloud\TestFluxingCloud.cpp`
- 本轮探针脚本（Run / Tiers / Cleanup 三模式；**其状态文件含明文凭据，禁止入库**）：`F:\soft\00selfmade\rime_claude\tmp\bo-push-probe.ps1`
- 本轮探针报告（五档计时原始数据）：`F:\soft\00selfmade\rime_claude\reports\fluxing-bo-12002\push-probe-report.txt`
- 官方界面探针（按控件 ID 1016 / 1017 定位账号窗）：`F:\soft\00selfmade\rime_claude\reports\fluxing-bk-syncstatus\probe-sync-status.ps1`
- 本轮真机报告：`F:\soft\00selfmade\rime_claude\reports\fluxing-bk-syncstatus\sync-status-20261122-161712.txt`

请自行从上述文档掌握背景、目标、进度与下一步计划，不要凭本文档内容代替真源文档。
