# 阶段 2 · 写预算阈值超限负样本自证（2026-11-27）

> 对应裁定：`docs/relay/stage-2-rulings-20261127.md` §5.4 第 2 条
> 「落盘后必须做一次**阈值超限负样本自证**，否则无法证明 20,000 真的生效」
> 用户授权：「授权现在做自证」（生产写授权**仅**覆盖本条）
>
> **【2026-11-27 15:47 降级】** 本条证据的**对象**（D1 写预算阈值落 20,000 与超限负样本自证）已随接力阶段 3 摘除而**整体作废** —— D1 已由用户于 2026-11-26 拍板退役（切 jury.xin）。但本文的 `19,999 行 / 80,001 Rows written` 实测仍属**有效证据**（写放大 9× 的原始标定），仅作历史留存，**不再作为待办**。新阶段 3 = P0 假空修复，见 `docs/plan-relay.md` §2。

---

## 0 一句话结论

`FINAL_WB_VERDICT=PASS`：生产阈值 **20,000 行 / 账号 / UTC 日**双向自证成立 ——
**恰好 20,000 时放行（HTTP 200）**，**超 1 行时拒绝（HTTP 429 + `write_budget_exceeded`
+ D1 该账号行数零增长）**，1 条与整批 64 条两种粒度都被拦。

同时登记两项**必须在下一批处理**的发现：
① §5.4-2 写的「超限时 `code: 7500` 可复现」与实测不符（见 §4，口径冲突）；
② D1 实际计费行 ≈ 插入行 ×4（见 §6）⇒ 20,000 阈值的安全边际只有设计预期的 1/4。

---

## 1 前置（全部盘上回读，非摘要转抄）

| 项 | 值 | 来源 |
| --- | --- | --- |
| 阈值（部署配置） | `WRITE_BUDGET_ROWS_PER_DAY = "20000"` | `cloud/fluxing-sync/wrangler.toml:63` |
| 阈值（代码默认） | `DEFAULT_WRITE_BUDGET_ROWS_PER_DAY = 20_000` | `cloud/fluxing-sync/src/env.ts:50` |
| 测试口径 | `WRITE_BUDGET_ROWS_PER_DAY: '20000'` | `cloud/fluxing-sync/vitest.config.ts:37` |
| 桶行口径（未动） | `WRITE_BUDGET_BUCKETS_PER_DAY = "4000"` | `wrangler.toml:71` |
| 生产生效版本 | `7bb77f18-bc1c-4c3b-accd-4c2e741cd14d`（100% 流量） | 上一轮 `deployments status` + `versions view` 双口径 |
| 仓库 HEAD | `b4c7f27`（= 远端 `refs/heads/feat/100-cloud-sync`） | `rev-parse HEAD` / `ls-remote` |

§5.4 第 1 条（服务端生效值回读）与第 3 条（生产 Version 留证）已在上一轮完成；
本条（第 2 条负样本自证）为本轮交付。

---

## 2 探针设计：为什么要这三步，少一步就没有鉴别力

单点观测无法区分「阈值生效」和「护栏一律拒绝」。故设计成**三态夹逼**：

| 态 | `used`（当日已写） | 本次计划 | `used + planned` | 期望 |
| --- | --- | --- | --- | --- |
| 基线 | 0 | 种子 19,999 行（直灌） | — | 灌完 `COUNT(*)` = 19,999 |
| **正样本** | 19,999 | 1 条 | **20,000（= 阈值）** | **必须 200**（`evaluateWriteBudget` 用 `>` 才拒，见 `src/sync.ts:152-164`） |
| **负样本 1** | 20,000 | 1 条 | 20,001 | **必须 429 + `write_budget_exceeded` + 零写** |
| **负样本 2** | 20,000 | 整批 64 条 | 20,064 | **必须 429 + 整批零写** |

- 若阈值被误读成 0、或护栏无条件拒 ⇒ **正样本会 429** ⇒ 判负。
- 若阈值没生效、或变量被读成极大值 ⇒ **负样本会 200** ⇒ 判负。
- 阈值取**生产真实值 20,000**，明确**不接受**「临时调低阈值」替代（调低后自证的就不是生产那个数）。
- 负样本的 `item_key` 用新键，取证后直接 `SELECT` 该键是否存在 ⇒ 零写是可观测事实，不是「返回码好看」。

**取舍登记（必须记）**：种子走 `wrangler d1 execute --remote` 直灌，不走产品真实推送。
理由：`used` 是纯 `COUNT(*) FROM sync_items WHERE user_id=? AND updated_at>=utcDayStart`
派生（`src/sync.ts:202-214`），直灌与产品推送在预算判定上**逐字段等价**；
而真实推送要多花 20,000 行写 + 20,000 次 R2 写 + 313 次 HTTP，**不增加任何鉴别力**，
却会把当日 D1 额度彻底吃光。

---

## 3 执行时间线与判据（12 项 CHECK 全 PASS）

| 时刻 (UTC+8) | 动作 | 观测 | 判据 |
| --- | --- | --- | --- |
| 15:00:08 | 额度硬闸门 | `rows_written_24h=13956` | `quota_headroom` PASS（闸门 ≤50,000） |
| 15:00:09 | `GET /v1/health` | 200，`environment=dev`，`via=direct` | `health` PASS |
| 15:00:11 | `POST /v1/auth/register` | 200，`need_email_verify:true`，uid `a1b4e4d4-7ac5-497a-b21f-68f32ba66ab9` | 账号建立 |
| 15:00:22 | D1 `UPDATE users SET email_verified=1` | 1 行写（见 §5 为何走这条路） | 账号可登录 |
| 15:00:24 | `POST /v1/auth/login` | 200，`access_token_len=43`，`email_verified=True` | `auth` PASS |
| 15:00:30 | 空账号校验 | `count=0` | `fresh_account_empty` PASS |
| 15:00:30 | 生成种子 SQL | 5 条语句 / 19,999 行 | — |
| 15:00:44 | `wrangler d1 execute --file` | ok，D1 自报 **`Rows read: 20019` / `Rows written: 80001`** | 见 §6 |
| 15:00:52 | 种子回读 | `COUNT(*)` = 19,999 | `seed_19999` PASS |
| 15:01:06 | **正样本** push 1 条 `wbprobe/pos/boundary` | 200，`accepted`，`skipped=[]`，`server_seq=91700` | — |
| 15:01:17 | 正样本回读 | `count=20000`，键存在 | `positive_boundary_allowed` / `positive_row_written` **PASS** |
| 15:01:28 | **负样本 1** push 1 条 | **429** `{"error":{"code":"write_budget_exceeded","message":"今日云端写入已达上限，请明天再试或减少本机词典规模。"}}` | — |
| 15:02:04 | 负样本 1 回读 | `count=20000`（未变），新键 **不存在** | `negative_http_429` / `negative_code` / `negative_zero_write` **PASS** |
| 15:02:06 | **负样本 2** push 整批 64 条 | **429** 同码 | — |
| 15:02:19 | 负样本 2 回读 | `count=20000`，`batch_rows=0` | `negative_batch_429` / `negative_batch_zero_write` **PASS** |
| 15:02:25 | 收尾 | `rows_written_24h=93977`，`RUN_EXIT=0` | **`FINAL_WB_VERDICT=PASS`** |
| 15:04:13 | 独立 `-Stage readback` | `count=20000`，`rows_written_24h=93982` | 终值复核一致 |

**判据强度说明**：正样本这一条同时把 `used` 顶到阈值，因此负样本本身**不产生任何写入成本**，
且「放行」与「拒绝」用的是同一个键空间、同一把令牌、同一分钟内 —— 排除了时间漂移与配置改动干扰。

---

## 4 口径冲突登记：`code: 7500` 不可复现

§5.4 第 2 条原文：「超限时 `code: 7500` 可复现」。

**实测不可能同时成立**：
- 护栏在 20,000 逻辑行处先返回 429 ⇒ 正常路径**永远走不到** D1 的免费日额度上限（100,000 行）。
- 要复现 `code: 7500` 必须真的把当日 D1 写行打穿 10 万 ⇒ 会连带打掉**全库真实用户当日写入**，
  属于破坏性操作，**禁止**。

**实际采用并被历史接受的口径**（本轮判绿依据）：

```
HTTP 429
+ error.code == "write_budget_exceeded"
+ sync_items 该账号行数零增长（含 item_key 不存在）
```

**建议**：把 §5.4-2 的表述订正为上述三元素。本轮**不改裁定书文本**，只登记（文档改动需单独授权）。

---

## 5 认证路径取舍：为什么不读验证码，而是就地置位

首轮注册（日志 `wb-run-20261127-145816.log`）用 `@example.com` 被邮件服务拒绝：
`422 mail_recipient_rejected`（Resend 不收保留域）。盘上复核确认**零夹具落库**
（`SELECT ... FROM users WHERE email LIKE 'wb.probe.%'` = 0 行，会话文件不存在）。

读码这条路在生产**结构性不可用**：
- 生产 `MAIL_MODE=http`（Resend）⇒ `mail_outbox` **不写行**；
- 验证码只存 `codeHash(pepper, email, code, created_at)`（`src/auth.ts:695`），带 `AUTH_PEPPER` 无法本地复算；
- ADR 0076 D1-1/D1-2 规定验证码「纯生成与落库分离、**发信成功后才落库**」（`src/auth.ts:339-346`），
  `users` 行同样在发信成功后才建。

故改走：`@aiec.fun`（本项目自有域）注册 → D1 就地 `UPDATE users SET email_verified = 1`（**1 行写**）。
自证只需要一个「能登录的已认证账号」，不需要真的收到信 ⇒
**不伪造验证码、不读 outbox、不新增任何服务端后门**。
全程**未调用** `/v1/auth/refresh`（成功即轮换、撤销旧 `refresh_token`）。

---

## 6 新发现（重要）：D1 写行放大 ≈4×，阈值安全边际被高估

种子 19,999 行一条语句，D1 自报 **`Rows written: 80001`** ⇒ **≈4.000 计费行/插入行**
（表行 + `idx_sync_user_item` + `idx_sync_user_id` + `sqlite_sequence`）；
`DELETE` 按同结构推断 ≈3 行/行。

而护栏的 `planWriteRows`（`src/sync.ts:172-189`）按**条目数**估算：新条目 1 / 已存在条目 2。
⇒ 20,000「逻辑行」预算在最坏情况下可消耗 **≈60,000 ~ 80,000 D1 计费行**，
而全球免费额度只有 **100,000 行/UTC 日**。

连带订正一处量级认知：裁定书 §5.2 引的「单轮满推实测 ≈6,096 行」是
`rows_written_24h` 的**当日累计差分**口径，**不是**单条消息的计费口径，两者不可混用。

**建议（需拍板）**：把预算改成 D1 计费行口径（新 4 / 更新 3），或把阈值下调到 ~5,000 逻辑行。
本轮**不擅自改阈值**（未拍板的口径不擅改）。

---

## 7 额度账与清理排期：今日禁跑 cleanup

- 入口 `13,956` → 收尾 `93,977` → readback `93,982`；免费额度 100,000/UTC 日 ⇒ **今日余 ≈6,018 行写**。
- 清理需 `DELETE FROM sync_items WHERE user_id=<探针 uid>`，影响 20,000 行，按 §6 放大 ≈3×
  ⇒ **约 60,000 行写** ⇒ **今日执行必打穿 100,000**，触发 `code: 7500` 并影响全库真实用户当日写入。
- 结论：**cleanup 今日不执行**（探针脚本已把 cleanup 做成显式独立 stage，不会误触发）。
- 排期：**UTC 日重置后（北京时间 2026-11-28 08:00 之后）**执行一次：

```
pwsh -NoProfile -File 'F:\soft\00selfmade\rime_claude\reports\relay-stage2-wb\wb-probe.ps1' -Stage cleanup
```

- 未清残留：探针账号 `wb.probe.1127150009@aiec.fun`（uid `a1b4e4d4-7ac5-497a-b21f-68f32ba66ab9`）
  + `sync_items` 20,000 行（19,999 种子 + 1 正样本 `wbprobe/pos/boundary`）+ R2 1 个 32 B 对象。
  注意 `used` 按 `updated_at >= utcDayStart` 计 ⇒ UTC 重置后该账号预算**自动归零**，
  夹具不影响任何真实用户，只占 D1 存储与读行。
- 历史待清探针账号：`ak.probe.261111@example.com`、`ak.probe2.261111@example.com`、`wb.probe.*@aiec.fun`。

---

## 8 凭据处置（已发生泄漏的补救）

- 事故：首版脚本把 login 响应体前 160 字符整段打进日志 ⇒ 完整 `access_token` / `refresh_token`
  曾落入 `reports/relay-stage2-wb/wb-run-20261127-150002.log`。
- 已处置：两个日志内该两字段值替换为 `<redacted>`；逐文件复核
  `LITERAL_TOKEN_HITS=0`（2 个日志 + evidence json + seed sql 全 0）。
  脚本 `:123` 改为只打 `status` + `error.code`（`PARSE_ERRORS=0`，13,756 B）。
- 会话凭据仅存 `%TEMP%\fluxing-wb-probe-session.json`（**不在仓库**，不进提交物）；
  `access_token` 1 小时自然过期，`refresh_token` 未使用、将随账号清理失效。
- **教训（拟入坑表）**：任何 `body=` 打印必须先过字段白名单；登录/注册响应禁止整段截断打印 ——
  截断只保证长度，不保证不泄密。

---

## 9 未闭环 / 待办

1. **cleanup 待排期**（§7，UTC 次日；今日额度不足）。
2. **D1 计费行口径复核立项**（§6：改口径 vs 下调阈值到 5,000）。
3. **§5.4-2 文案订正**（§4，需单独授权改裁定书）。
4. **handoff 条目待并入**：本轮结论只落本文件 —— 接力链在 stage 2 持锁运行
   （holder `mvs_f16dfeac5e2445b4a34f60f39d1dfc25`），`handoff-2026-11-23-batch-br-write-amplification-and-zero-write-register.md`
   有并发写风险，链释放锁后再追加。
5. **AC-09 仍 FAIL（未改记）**；`AC-212` / `AC-215②` 未判定；桶阶段 3/4 仍禁。
6. 本轮**未改任何产品代码**，HEAD 仍为 `b4c7f27`。

---

## 10 证据指针（可复现）

| 用途 | 路径 |
| --- | --- |
| 探针（四段 `auth\|run\|cleanup\|readback`） | `reports/relay-stage2-wb/wb-probe.ps1` |
| 首轮注册失败日志 | `reports/relay-stage2-wb/wb-run-20261127-145816.log` |
| 自证轮完整日志（令牌已擦除） | `reports/relay-stage2-wb/wb-run-20261127-150002.log` |
| 结构化验证 | `reports/relay-stage2-wb/wb-evidence-20261127-150002.json` |
| 种子 SQL（19,999 行 / 5 语句） | `reports/relay-stage2-wb/seed-20261127-150002.sql` |
| 判定代码 | `cloud/fluxing-sync/src/sync.ts:152-164`（`evaluateWriteBudget`）、`:172-189`（`planWriteRows`）、`:202-214`（`used` / `utcDayStart`）、`:339-341`（429 出口） |
| 阈值配置 | `cloud/fluxing-sync/wrangler.toml:63`、`cloud/fluxing-sync/src/env.ts:50` |