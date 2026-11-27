# 接力 prompt — BR-①g 接力链 · 阶段 1（修复轮）

> 本文件只给指针，不复述 handoff 内容。背景 / 目标 / 进度 / 计划 / 裁决全文见第 1–3 号文件；**本轮人工裁决全文 = `docs\plan-relay.md` §5**。

## 当前状态（以盘上回读为准，勿凭转述）

- 台账 `F:\soft\00selfmade\rime_claude\docs\relay.json` = **新一轮链**（旧链因判据④ 红停链，台账已归档为 `reports\fluxing-br-1g\relay-ledger-stage1-blocked-20261127.json`）；`status=running` / `current_stage=1` / `max_stages=6`。
- 上一链裁定：四道判据 **①绿 ②绿 ③绿 ④红**；桶推路径**真机首次跑通**（`pushed=896` / `rows_pushed=1928` / `held_quota=0` / `held_unproven=0`）。
- 本链阶段 1 = **修复轮**（B 定性 + A-0 事实核对 + F3 诊断行 + 规则 1 死分支修复 + 判据④ 口径 ADR 草案）：**客户端侧、零生产写**。

## 必读（全路径，按序）

1. `F:\soft\00selfmade\rime_claude\docs\plan-relay.md` —— **§5 = 人工裁决全文（A/B/C）**；§2 = 六段阶段与各段门禁；§3 = 每阶段通用收尾动作；§4 = 已知工具坑（含「停链后不可直接续链」的换代路径）。
2. `F:\soft\00selfmade\rime_claude\docs\relay\stage-1-inventory.md` —— 上一环四判据逐条裁定 + F1–F4 附带发现 + §7 停链原因与原 A/B/C 选项。
3. `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-23-batch-br-write-amplification-and-zero-write-register.md` —— 顶节 `## AI` = 本轮裁决与换代记录（上一节 = `## AH`）。
4. `F:\soft\00selfmade\rime_claude\docs\relay.json` —— 台账（**只经 `docs\relay\relay-ledger.ps1` 闸门改写，禁手改**）。
5. `F:\soft\00selfmade\rime_claude\docs\AGENT-RELAY.md` + `F:\soft\00selfmade\rime_claude\docs\relay\RELAY-SOP.md` —— 接力协议与 SOP。
6. `F:\soft\00selfmade\rime_claude\docs\RELAY-ADAPTATION.md` —— **§6 = 本项目覆盖**（git commit/push 在本仓**不是**人工检查点：链内环在门禁绿后可自行 commit + push）。
7. `F:\soft\00selfmade\rime_claude\docs\adr\0077-bucket-level-sync-storage-architecture.md` —— `:436-443` 四道判据 / `:445-454` 回退条件 / `:411-412` 阶段 3 硬前置。
8. `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md` —— 法则层（R1–R9 / P1–P8）。
9. `F:\soft\00selfmade\rime_claude\reports\fluxing-br-1g\` —— 盘上原始证据（**不入库**）。

## 硬约束（红线，逐字执行）

- 阶段不得自动递增；阶段 3/4 **永不启用**；规则 1 盲区未修**且**判据④ 未重判前不得切读。
- 生产 D1/R2/Cloudflare **只读**；**不得**执行任何服务端迁移或写生产 D1（须人工单独授权）；不动用户词典数据；`AC-09` 仍 FAIL 不得改记；凭据值绝不进日志/输出/提交物。
- 推送走**仓库级 SOCKS5 代理**（`socks5h://127.0.0.1:7897`，已设好）：用**裸** `git push kizemo feat/100-cloud-sync` 与 `git ls-remote kizemo refs/heads/feat/100-cloud-sync`，**不要**带 `-c http.proxy=`；硬判据 = `ls-remote` 与 `rev-parse HEAD` 逐字相等。
- 取证 runner 必须只读、自带落盘判据 + 心跳；不得并发跑多个会杀 `WeaselServer` 的 runner；切阶段/重启必须让新进程拿到新环境块。

## 一句话

先读 1–3 号文件掌握背景、进度与**人工裁决全文（plan §5）**，再按 plan §2 的阶段 1 定义执行；证据落文件、对话只留指针与结论，**不伪造验收证据**。
