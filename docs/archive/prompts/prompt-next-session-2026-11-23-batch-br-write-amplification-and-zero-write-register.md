# 接力 prompt — BR-①g 接力链 · 阶段 1（双写常驻取证收口）已停链

> 本文件只给指针，不复述 handoff 内容。背景/目标/进度/计划/裁决全文见第 1–2 号文件。

## 当前状态（以盘上回读为准，勿凭转述）

- 台账 `F:\soft\00selfmade\rime_claude\docs\relay.json`：`status=blocked`、`current_stage=1`（**未前进**）、`stages` 有 id=1 一条（`status=done`、`verdict=FAIL`）、`blocked_on` / `notes` / `sessions` 已登记本环会话 `mvs_81e9325c7a9d4d2b8a78d3c515c862b8`。
- 阶段 1 四道判据裁定：**① 绿 / ② 绿 / ③ 绿 / ④ 红** ⇒ 依 `docs\plan-relay.md:57`「四道判据全绿才进阶段 2」门禁不过 ⇒ 依 `docs\plan-relay.md:41`「阶段不得自动前进，验收不过就停在当前段」，**本链停在阶段 1**。
- **relay-02of06 未创建、下一环未起**（停链，非静默跳过）。
- 判据④ 红的事实：`missing_in_bucket=926` / `hlc_mismatch=39043` / `http_errors=0`；根因已坐实 = 2026-11-26 桶化首导**整库重分配条目 HLC**（桶侧 `ms=1795565189774`，老端点 `ms=1795339039468`）⇒ 两路差异**不可自愈**。
- 链锁已 release（`docs\relay\relay-lock.ps1 -Action release`），新环可正常起。

## 必读（全路径，按序）

1. `F:\soft\00selfmade\rime_claude\docs\relay\stage-1-inventory.md` — 阶段 1 最终裁定：§0 裁定表 / §1 现场 / §2 四判据逐条 / §3 回退条件核查 / §4 附带发现 F1–F4 / §5 已验证·推断·未验证 / §6 证据清单 / §7 停链与 A/B/C / §8 阶段 2 前置。
2. `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-23-batch-br-write-amplification-and-zero-write-register.md` — 顶节 `## AH` = 本环完整交接（上一环 = `## AG-PROXY`）。
3. `F:\soft\00selfmade\rime_claude\docs\relay.json` — 台账（**只经 `docs\relay\relay-ledger.ps1` 闸门改写，禁手改**）。
4. `F:\soft\00selfmade\rime_claude\docs\plan-relay.md` — 阶段门禁原文（`:41` / `:57` / `:86`）。
5. `F:\soft\00selfmade\rime_claude\docs\RELAY-ADAPTATION.md` — 项目覆盖条款（§6：git commit/push 在本仓**不是**人工检查点）。
6. `F:\soft\00selfmade\rime_claude\docs\AGENT-RELAY.md` + `F:\soft\00selfmade\rime_claude\docs\RELAY-SOP.md` — 接力 SOP。
7. `F:\soft\00selfmade\rime_claude\docs\adr\` — 桶化 ADR（id 前缀 `0077`；关注 `:440` 四道判据、`:445-454` 回退条件、`:411-412` 阶段 3 硬前置）。
8. `F:\soft\00selfmade\rime_claude\docs\agents\domain.md` — 工程技能消费契约。
9. `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md` — 法则层（R1–R9 / P1–P8）。
10. `F:\soft\00selfmade\rime_claude\reports\fluxing-br-1g\` — 盘上原始证据（判据④ md/txt、两轮取证日志、503 诊断、比对脚本 v3）；`reports\**` 不入库。

## 需人工裁决三项（详见 inventory §7.2，建议按 A → B → C 顺序）

- **A（桶推 HLC 口径）**：A1 改桶推口径（本地已有老端点 HLC 则沿用，代价 = 协议 + 侧车 + 重推一轮）/ A2 修订判据④ 口径（独立 ADR 修订 ADR 0077 `:440`）/ A3 接受现状并把 `:451` 回退条件改写为「桶端点 ⊉ 老端点**活跃**条目集合」。
- **B（926 条缺口定性）**：是否先补「侧车文本键 ↔ 云端 `item_key`」hash 映射口径（独立小批次），再重跑判据④。未补则无法区分「本地已删僵尸」与「本地现存未上桶」。
- **C（是否授权进接力链阶段 2）**：阶段 2 写集仅 `inventory_complete` 死分支，**不含** HLC 口径与 926 条定性 ⇒ **先进阶段 2 不会让判据④ 转绿** ⇒ 建议先按 A/B 立项，再决定放行阶段 2。

## 红线（不变）

阶段 3/4 永不启用；阶段不得自动递增；规则 1 盲区未修前不得切读；生产 D1/R2/Cloudflare 只读；不动用户词典数据；AC-09 仍 FAIL 不得改记；凭据值绝不进日志/输出/提交物；阶段 2 的进入须人工显式放行。

## 一句话

先读上面 1–7 号文件掌握背景与判定口径；**未获人工对 A/B/C 的明确裁定前，不要推进阶段 2、不要起下一环、不要改产品代码**。
