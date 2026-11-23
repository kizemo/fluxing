# 批次 BO 交接文档 · 客户端推送分批对齐服务端上限 64 条（ADR 0073）＋ 12002 根因定案

- 日期：2026-11-22
- 批次：BO（接 BN；本文件已就地改写两次 —— 第一次 = BO 产品修复收口，第二次 = 12002 根因定案）
- 分支：`feat/100-cloud-sync`（远程 `kizemo`）
- 性质：BO 前半 = **产品修复 + 门禁 + 出包装机 + 装机实证 + 真机复验 + 提交**；BO 后半（本次追加）= **纯取证与根因判定**（**零产品代码改动**）
- 边界：全批次**未改** `librime/`、**未改** IPC 协议消息类型、**未新增**第三方依赖、**未改** `.clang-format`、**未 bump** 版本号（装机版仍 1.3.8.0，判定一律用 sha16 / 字节数）
- 提交实况：BO 产品修复已提交并推送（`ccf6477` 代码 + ADR 0073 + CHANGELOG；`a68bfd4` 文档；`a68bfd4`…`78726e7` 为 BN 批提交，均已推 `kizemo/feat/100-cloud-sync`）。**本次 12002 根因取证轮尚未提交任何内容**（只有未跟踪的探针脚本与报告 + 本文件/接力 prompt/CLAUDE.md 指针的就地改写），HEAD 仍为 `0e2ebdc`。

---

## 1. 当前状态（一段话）

BO 批前半已把客户端推送单批从 200 降到 64 并装机验证，`too_many_items` 拒绝消失；但真机三轮（14:11:16 / 14:13:50 / 14:15:27）同步全部停在 `网络请求失败：接收响应失败：错误码 12002`，当时只能推断为「链路/代理超时、与批量修复正交」。**本次追加轮用一次性探针账号直连 `/v1/sync/push` 做了带令牌的逐档计时，已把这个推断推翻并给出定案**：带令牌的 push **100% 到达服务端**（每档 `accepted` 数与 D1 落库行数逐档吻合、`server_seq` 递增），而服务端处理时长**随条目数线性增长**（≈ 626–706 ms/条）—— **64 条 = 40 071 ms**，远大于客户端 `CloudTransport.h:72` 的 15 000 ms 接收超时 ⇒ **必然 `12002 ERROR_WINHTTP_TIMEOUT`**。根因落在服务端 `cloud/fluxing-sync/src/sync.ts:155-201` 的**串行 `await env.BUCKET.put(...)`** 循环（写序「R2 先、D1 后」，每条一次 R2 往返）。推论：**BO 的「客户端批量 = 64」不足以修复 12002**，它把「服务端 400 快速拒绝」换成了「客户端自己 15 s 超时」；ADR 0073 的决策 5（只改客户端、不动服务端）需要重新评估。**代码修复尚未实施**，下一步必须先写 ADR 0074（服务端并发化 / 客户端批量与超时重估）再动码。**本批不登记成功侧 PASS、不登记回归、不登记「已修」**。

---

## 2. 必读文档（全部为绝对路径）

| 用途 | 路径 |
|---|---|
| **本批真源（先读这份）** | `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-22-batch-bo-push-batch-align-server-limit.md` |
| 进站第 1 份 · 身份/模块/不变量 | `F:\soft\00selfmade\rime_claude\CONTEXT.md` |
| 进站第 2 份 · 操作手册（§1 首屏 / §4 危险区 / §5 checklist / §9 交接规则） | `F:\soft\00selfmade\rime_claude\AGENTS.md` |
| 进站第 3 份 · 法则层（R1–R9 / P1–P8） | `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md` |
| 进站第 4 份 · 部署 + NSIS 配方 | `F:\soft\00selfmade\rime_claude\.specify\memory\project-knowledge.md` |
| 进站第 5 份 · 工程技能消费契约 | `F:\soft\00selfmade\rime_claude\docs\agents\domain.md` |
| BO 决策依据（**将被 ADR 0074 修订**） | `F:\soft\00selfmade\rime_claude\docs\adr\0073-client-push-batch-aligns-with-server-limit.md` |
| 规格 100（本批未改） | `F:\soft\00selfmade\rime_claude\.specify\specs\100-fluxing-cloud-account-sync\spec.md` |
| 上一批真源（BN） | `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-22-batch-bn-sync-round-notices-visible.md` |
| 再上一批（BM） | `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-22-batch-bm-malformed-cloud-entry-skip.md` |
| 服务端真源（根因所在） | `F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\src\sync.ts`、`F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\src\wire.ts`、`F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\src\auth.ts` |
| 客户端真源（超时/批量） | `F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\CloudTransport.h`、`F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\CloudTransport.cpp`、`F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\CloudSyncService.h`、`F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\CloudSyncService.cpp` |
| 本轮探针脚本（**明文 `auth_key` 在 state 文件里，禁止入库**） | `F:\soft\00selfmade\rime_claude\tmp\bo-push-probe.ps1`、`F:\soft\00selfmade\rime_claude\tmp\bo-push-probe-state.json` |
| 本轮探针报告（五档计时原始数据） | `F:\soft\00selfmade\rime_claude\reports\fluxing-bo-12002\push-probe-report.txt` |
| 官方界面探针（1016/1017 定位） | `F:\soft\00selfmade\rime_claude\reports\fluxing-bk-syncstatus\probe-sync-status.ps1` |

---

## 3. 本阶段逐任务进度

**done（已实测）**

1. `wrangler tail` 通道可用性与**观测力边界**认定：通道自证靠「打一条 `/v1/health` 看是否产出 invocation JSON」（`--format json` 下无连接横幅）；**tail 对已发生的 15:18:41 失败零回溯观测力**。
2. 客户端侧代理假设**排除**：`ProxyEnable=0`、`AutoConfigURL` 空、`netsh winhttp show proxy` = `Direct access`、`/v1/health` 5 轮 200。
3. 客户端 15 s 超时来源坐实：`CloudTransport.h:72 timeout_ms = 15000`，`CloudTransport.cpp:207-208` 四相共用同一 timeout ⇒ 接收阶段超时；`CloudTransport.cpp:318-320` 输出「接收响应失败：」；`12002 = ERROR_WINHTTP_TIMEOUT`。
4. 「14:13 约 2.6 s 就失败」的矛盾已结案：那是一轮早已在飞行中的请求在 14:13:50 满 15 s 才落字。
5. 服务端端点活着、鉴权层不慢：`GET /v1/health` 200（1219 ms）；未带令牌的 `POST /v1/sync/push` → `401 missing_token`（空体 267 ms / 2498 B 体 262 ms）⇒ 卡点在鉴权之后。
6. 探针账号打通：`register`（恒 503 `mail_send_failed`，但 `users` 行已落）→ 用 D1 把探针邮箱 `email_verified=1` → `login` 200（`email_verified=true` 回显；服务端 `auth.ts:517` 登录不校验已验证）。
7. **决定性：带令牌五档 push 计时**（§4）⇒ (A) 排除 / (B) 证实 / 根因定位。
8. 服务端闸门仍是 64（n=200 → `too_many_items`，1373 ms 快速拒绝 ⇒ 该检查在 R2 写之前，`sync.ts:98-103`）。
9. BO 轮的构建 / 单测 704 / L0 15/15 / clang-format 定点 0 漂移 / 装机 sha16 一致（`1E82FA7F30DBB263`）仍然有效。

**in-flight**

- 本 handoff 的就地改写 + 接力 prompt 的就地改写 + `CLAUDE.md` §2 的 BO 指针补写（AGENTS.md §9 第 3 项，BO 批此前漏写，本次补上）——**均未提交**。

**pending**

- ADR 0074（服务端并发化 / 客户端批量与超时重估）；服务端改码 + `wrangler deploy` + 复测；客户端侧参数重估；真机成功侧复验；`/v1/auth/register` 503 修复；探针数据清理；本轮改动的 git 提交。

---

## 4. 决定性证据：带令牌的 push 五档计时（2026-11-22 15:51:13）

命令：`pwsh -NoProfile -File tmp/bo-push-probe.ps1 -Mode Tiers` ⇒ **`EXIT=0`**（`REUSE_ACCOUNT` 复用已落盘账号，未再新建账号）。

| 档位 | cipher 字节 | body 字节 | HTTP | elapsed_ms | 每条均摊 | 服务端返回 |
|---|---|---|---|---|---|---|
| 空 items（鉴权自证） | — | — | **400** | 1 894 | — | `bad_items`「items 不能为空。」⇒ **带令牌请求确实到达服务端**（不是 401） |
| n=1 | 64 | 290 | **200** | **2 579** | 2 579 | `accepted` 1 条，`server_seq=4` |
| n=16 | 64 | 4 497 | **200** | **11 300** | 706.2 | `accepted` 16 条 |
| n=64 | 64 | 17 985 | **200** | **40 071** | 626.1 | `accepted` 64 条 |
| n=200 | 64 | 56 501 | **400** | 1 373 | — | `too_many_items`「单次推送条目数上限为 64（实收 200）。」 |
| n=1 × 4 MiB | 4 194 304 | 5 592 615 | **200** | **8 186** | 8 186 | `accepted` 1 条，`server_seq=85` |

落库核对：`D1_AFTER count=82 max_id=85`（口径 = `SELECT COUNT(*), MAX(id) FROM sync_items WHERE user_id = '<探针 uid>'`，见 `tmp/bo-push-probe.ps1:246`）⇒ **82 = 1+16+64+1 = 四档 accepted 条目数之和，逐档吻合** ⇒ 每一档的条目都**真实落库**（不是「到达但没处理」）。`D1_ROW id=4 key=probe-bo12002-20261122154955-n1-b64-0 type=userdb cipher_len=64`。

### 4.1 结论

- **(A)「带令牌的 push 没到达服务端 / 到达但无响应」= 排除。** 每档都有 HTTP 状态码、有 `accepted` 数组、有 D1 行、有递增的 `server_seq`。
- **(B)「到达了但服务端处理超过客户端 15 s 超时」= 证实。** 处理时长随条目数**线性增长**（16 条 11.3 s → 64 条 40.1 s，每条均摊 626–706 ms），64 条必然超过 15 000 ms ⇒ 客户端在 `WinHttpReceiveResponse` 阶段超时 ⇒ `12002`。
- **根因位置**：`cloud/fluxing-sync/src/sync.ts:155-201` —— 先 `for` 循环**串行** `await env.BUCKET.put(...)`，同循环收集 `INSERT INTO sync_items`，循环结束后才做 D1 batch（写序「R2 先、D1 后」，`sync.ts:1-7` 三条不变量）。每条一次 R2 往返 ≈ 0.63 s ⇒ 条目数是唯一的一阶放大因子。
- **单条基线 ≈ 2.6 s**（n=1，64 字节密文）⇒ 与条目数无关的固定成本（请求体读取 + 鉴权 + 1 次 R2 put + D1 batch）本身就吃掉 15 s 预算的 17%。
- **15 s 预算下的安全单批上限 ≈ 16–20 条**（16 条 = 11.3 s；按 626–706 ms/条线性外推 ≈ 21 条 ≈ 15 s ⇒ 取 16 更稳）。
- **大体积不是主因**：单条 4 MiB（body 5.59 MB）= 8 186 ms，**未超 15 s** ⇒ 「用户词典快照很大导致超时」站不住（但 8 MiB 上限条目未实测，见 §8）。

### 4.2 对 BO / ADR 0073 的后果（必须重新评估）

| 项 | ADR 0073 决策 | 本轮实测后的判断 |
|---|---|---|
| 客户端 `kPushBatchSize = 64` | 「与服务端上限逐值对齐」 | **对齐了上限，但没有对齐「15 s 能处理多少」** ⇒ 64 条 = 40 s ⇒ 必然超时；批量对齐 ≠ 批量可行 |
| 决策 5「只改客户端，不动服务端」 | 「改服务端需重部署 Worker、放弃写放大保护」 | **该理由在本轮失效**：不改服务端 ⇒ 客户端无论怎么调批量都逃不掉 15 s（除非把批量降到 ≤16，那要动客户端；或加长超时，也要动客户端）⇒ **服务端并发化是必须评估的候选** |
| 判据「装新包后 1017 变成功态」 | 未达成 | **不是环境问题，是批量可行性问题** ⇒ 修法方向要换 |

---

## 5. 对本文件早期版本的更正（必须以本节为准）

BO 轮 §8「推断」栏写的「`12002` 是本机到 Worker 的链路/代理超时（非 4xx/5xx、非鉴权失败），因此与本次批量修复正交」—— **被本轮实测推翻**。正确表述：**`12002` 是服务端单批处理时长（条目数线性放大）超过客户端 15 s 接收超时所致**，与链路健康度无关（`/v1/health` 直连 200、代理已排除、tail 通道自证通过）。BO 轮 §10 第 1 项（「待推 ≥65 条 ⇒ 分多批推成功」）的阻塞原因也随之改写：**不是「缺网络可用窗口」，而是「64 条这一批本身就超预算」**。

---

## 6. 与真机现场（15:18:41 那次 12002）的对应关系

| 证据 | 内容 | 标注 |
|---|---|---|
| 客户端诊断日志 `%TEMP%\fluxing-profile-notify.log` | 15:17:06.833 / 15:17:11.500 两条 `[cloudsync] access token refreshed after 401; retry with new token` | **已验证** ⇒ 真实客户端的请求**到达了服务端**（服务端回了 401） |
| 同上 | 15:17:07.766 / 15:19:01.080 两条 `[cloudsync] skipped malformed cloud userdb entry (payload shape invalid)` | **已验证** ⇒ BM 的跳过逻辑在真机生效 |
| 同上 | 15:18:41 那次 12002 **无任何 `[cloudsync]` 行** | **已验证**（日志缺行不等于请求未到达） |
| BO 装机版 `kPushBatchSize = 64` | 服务端 pid 1077296（14:07:45 起）、`WeaselServer.exe` sha16 `1E82FA7F30DBB263` = BO 装机版记录 | **已验证** |
| 失败那轮实际条目数 | 客户端**无逐请求条目数日志** | **推断**：装机版按 64 条/批推 ⇒ 与实测「64 条 = 40.1 s」同机理；BO 轮服务端文案「实收 200」对应的是**修复前**的 200 条批量 |

⇒ 现场 `12002` 与本轮实测的机理**一致**，但「失败那轮确实推了 64 条」属推断而非直证（要直证需给客户端加一条「每批条目数 + elapsed_ms」诊断行 —— 建议列入 ADR 0074 的客户端侧改动）。

---

## 7. 独立缺陷（与 12002 无关，需另行登记）

`POST /v1/auth/register` 连续四次返回 **503 `mail_send_failed`**（1754 / 2884 / 5945 ms 等），且 `mail_outbox` **无任何行** ⇒ **当前新用户无法完成「注册 → 收码 → 验证」链路**（`users` 行会落，但验证码邮件发不出去）。与本批 12002 根因**无关**（push 不发信），但产品侧「注册验证」功能当前不可用。排查方向：`cloud/fluxing-sync/wrangler.toml` 的 `[vars]` 里 `MAIL_*` 配置（服务商 / 额度 / 密钥 / 出口网络）。

> **已由批次 BQ 修订（2026-11-22 · ADR 0075 · `docs/archive/handoffs/handoff-2026-11-22-batch-bq-register-mail-rejection-semantics.md`）**：本节「注册验证功能当前不可用」的**口径不成立**。BO 当时的 503 只由**保留域**探针（`example.com` = RFC 2606/6761 保留域、null MX）支撑；BQ 用对照实验（同上游、同时刻、同密钥，**只换收件人域**）证明：**真实域收件人注册返回 200**（body 含 `account_id` / `salt` / `kdf` / `need_email_verify=true`），被拒的是**保留域收件人**，且上游实际返回 **422**（永久拒绝），被服务端的错误映射伪装成「可重试的 503」。BQ 已把该映射改为 **422 `mail_recipient_rejected`**（客户端不读 code、422 不在自动重试集合 ⇒ 语义自动正确），并给失败注册加了回滚与诊断头。**结论**：注册链路对**真实邮箱可用**；对**保留域 / 被上游拒收的邮箱**，正确行为是「明确报错让用户换邮箱」，而不是重试到 429。**遗留**：BQ 的「零残留 / 立即重试 / 真实域端到端」三条判据被生产 D1 每日行写入配额耗尽（Cloudflare `code 7500`）阻塞，待配额恢复后复验。

---

## 8. 已验证 / 推断 / 未验证

**已验证**
1. 带令牌的 push 100% 到达服务端并落库（四档 `accepted` 与 D1 行数逐档吻合，`server_seq` 递增）。
2. 服务端单批处理时长随条目数线性增长：1 条 2 579 ms / 16 条 11 300 ms / 64 条 **40 071 ms**（每条 626–706 ms）。
3. 64 条这一批**必然**超过客户端 15 000 ms 接收超时 ⇒ `12002 ERROR_WINHTTP_TIMEOUT`（客户端超时来源已定位到 `CloudTransport.h:72` + `CloudTransport.cpp:207-208`）。
4. 服务端条目数闸门仍是 64，且**在 R2 写之前**快速拒绝（n=200 → 1 373 ms 返回 `too_many_items`）。
5. 单条 4 MiB 密文（body 5.59 MB）= 8 186 ms，**不构成超时** ⇒ 「词典快照太大」不是根因。
6. 服务端「R2 先、D1 后」写序（`sync.ts:1-7`）⇒ 一批若在 R2 阶段超时，D1 里**不会**留下该批任何行 —— 这解释了为什么真机现场看不到对应 D1 痕迹。
7. 客户端侧代理 / 网络栈假设排除；`wrangler tail` 通道本身可用，但对已发生的失败**无回溯观测力**。

**推断**
1. 现场 15:18:41 的 12002 = 装机版 64 条批次撞上 40 s 服务端处理时长（条目数无直证，见 §6）。
2. 根因代码位置 = `sync.ts:155-201` 的串行 R2 put 循环（线性放大 + 每条一次往返，是唯一与「条目数」一阶相关的服务端代码路径）。
3. 服务端并发化后 64 条可压到数秒级（未实测）。

**未验证**
1. 「服务端并发化 ⇒ 64 条 < 15 s」的实际耗时（需改码 + `wrangler deploy` 后用同一探针复测）。
2. 单条 8 MiB（`MAX_PAYLOAD_BYTES` 上限）是否逼近 15 s。
3. 「令牌过期 → 续期 → **同步成功**」的界面级单轮直证（BN §10 第 ⑤ 项仍记部分闭环）。
4. `has_more`（「云端还有更多条目，下次同步继续」）真机可见性（未触发）。
5. 1017 单行 STATIC 在多条提示叠加时是否显示不全（ADR 0072 已登记、刻意不做自动换行）。
6. 真机「待推 ≥65 条 ⇒ 分多批推成功」的成功侧端到端。

---

## 9. 下一阶段计划（有序 + 排序理由）

1. **先写 ADR 0074**（仓库惯例：先决策后改码）。要拍板的是**服务端并发化 vs 客户端降批量/加超时 vs 两者都做**：
   - **S1（服务端并发化 `sync.ts:155-201`）**：保留「R2 先、D1 后」写序不变，只把串行 `await BUCKET.put` 改为受控并发（`Promise.all` 或分片并发，注意 Worker 内存与并发上限）。优点：不动协议、不影响已装机旧客户端、客户端无需改动。代价：需 `wrangler deploy`；并发失败时 R2 孤儿对象会变多（现状孤儿已存在，pull 侧有自愈逻辑）。
   - **S2（客户端降批量 + 加超时）**：`kPushBatchSize` 降到 ≤16（15 s 预算内），`CloudTransport.h:72` 的 `timeout_ms` 提到 45–60 s。代价：整轮耗时线性变长（200 条 ≈ 13 批），用户等待久，且已装机旧客户端不受新常量影响 ⇒ 只能救新装包。
   - **S3（两者都做）**：S1 把服务端压到数秒级，客户端批量 32、超时 30 s，双保险。
   - **排序理由**：S1 是唯一能同时救「已装机旧客户端」和「大批量」的方向 ⇒ **优先评估 S1**；S2/S3 作为配套。ADR 0073 **不改正文**（历史决策留痕），由 ADR 0074 写明「修订 ADR 0073 决策 5」+ `CHANGELOG.md` 追加条目（CRLF-only，字节级写入）。
2. **实施 + 部署 + 复测**：`wrangler deploy` 后用**同一探针**跑 `-Mode Tiers`（脚本未改，直接可比）。判据：**64 条档 elapsed_ms < 15 000**（建议目标 ≤ 5 000），且 `accepted` 数仍 = 64。
3. **客户端侧参数重估**（与 ADR 0074 一起定）：`CloudSyncService.h` 的 `kPushBatchSize` 与 `CloudTransport.h:72` 的 `timeout_ms`；建议同时加一条诊断行「每批条目数 + elapsed_ms + HTTP 状态」⇒ 把 §6 的推断变成直证。
4. **真机复验（成功侧）**：真实账号 `25940969@qq.com` 登录态点「立即同步」，判据 = 1017 变成功态且诊断日志无 `12002`；或用探针账号 + 本地造 130 条改动，走客户端多批路径。
5. **门禁（照 BO 批口径）**：`cmd /c "build.bat release weasel installer"` ⇒ `BUILD_EXIT=0`（0 警告 0 错误）；`TestFluxingCloud` 须显式 msbuild **且带 `/p:SolutionDir=F:\soft\00selfmade\rime_claude\`**、exe **在仓库根跑** ⇒ 相对当前基线 **704** 不回归；`scripts/gate/L0-static.ps1 -Mode all` ⇒ **15/15**；clang-format **18.1.8** 定点 `--lines=起:止` ⇒ **0 漂移**；改动文件 **U+FFFD=0**。顺序铁律：**格式化 → 复检 → 构建 → 单测 → 打包 → 装机 → 记 sha**。
6. ~~**独立批次修注册 503**（§7）：查 `MAIL_*` 配置 / 服务商额度 / 出口网络，修好前「注册验证」链路不可用。~~ **已由批次 BQ 完成（2026-11-22 · ADR 0075）**：根因 = 上游对**保留域收件人**（`example.com` = RFC 2606/6761 保留域、null MX）返回**永久拒绝 422**，被旧错误映射伪装成「可重试的 503」；对照实验（同上游 / 同时刻 / 同密钥，**只换收件人域**）证明**真实域收件人注册返回 200** ⇒ BO §7 的「注册验证链路不可用」口径**过宽**，已在 §7 就地修订。BQ 已上线生产（Version `96facaba-9277-4c77-b9f9-4df5cb6de41e`）：上游失败改为**三类分流**（4xx 永久拒绝 ⇒ 422 `mail_recipient_rejected`；401/403 与配置类 name ⇒ 500 `server_misconfigured`；429 ⇒ 503 `mail_rate_limited`；409/5xx/网络 ⇒ 503 `mail_send_failed`），失败注册**回滚**（DELETE 优先，被拒则降级 UPDATE 失效化 ⇒ 三态 `ok`/`degraded`/`failed`），新增诊断头 `X-Fluxing-Mail-Provider-Status` / `X-Fluxing-Mail-Rollback` / `X-Fluxing-Mail-Rollback-Error`。**遗留**：BQ 门禁判据「零残留 / 立即重试 / 真实域端到端」三条**被生产 D1 每日行写入配额耗尽**（Cloudflare `code 7500`）阻塞，待配额恢复后复验 —— 详见 `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-22-batch-bq-register-mail-rejection-semantics.md`。
7. **收尾**：探针数据清理（§13）+ `git status -sb` + `git diff --stat` + 提交（`docs` 前缀）+ 待用户拍板项（§12）。

---

## 10. 证据路径索引

| 用途 | 路径 |
|---|---|
| **五档计时原始报告（本轮决定性证据）** | `F:\soft\00selfmade\rime_claude\reports\fluxing-bo-12002\push-probe-report.txt` |
| 本轮探针脚本（Run / Tiers / Cleanup 三模式） | `F:\soft\00selfmade\rime_claude\tmp\bo-push-probe.ps1` |
| 探针账号状态（**含明文 `auth_key`，禁止入库**） | `F:\soft\00selfmade\rime_claude\tmp\bo-push-probe-state.json` |
| BO 决策（将被 0074 修订） | `F:\soft\00selfmade\rime_claude\docs\adr\0073-client-push-batch-aligns-with-server-limit.md` |
| BO 前半：修复前现象 | `F:\soft\00selfmade\rime_claude\reports\fluxing-bk-syncstatus\sync-status-20261122-140113.txt` |
| BO 前半：假阴性轮 | `F:\soft\00selfmade\rime_claude\reports\fluxing-bk-syncstatus\sync-status-20261122-141103.txt` |
| BO 前半：直证 1017 两路读取 | `F:\soft\00selfmade\rime_claude\reports\fluxing-bk-syncstatus\diag-1017-20261122-141347.txt`（同轮早一次 `diag-1017-20261122-141325.txt`） |
| BO 前半：12002 首现轮 | `F:\soft\00selfmade\rime_claude\reports\fluxing-bk-syncstatus\sync-status-20261122-141529.txt` |
| 客户端诊断日志（15:17–15:20 段） | `C:\Users\Duanyi\AppData\Local\Temp\fluxing-profile-notify.log` |
| 服务端根因代码 | `F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\src\sync.ts:155-201`（串行 put）、`:98-103`（too_many_items）、`:1-7`（三条不变量） |
| 服务端常量 | `F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\src\wire.ts:33`（`MAX_PUSH_ITEMS = 64`） |
| 客户端超时 | `F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\CloudTransport.h:72`、`.cpp:207-208`、`.cpp:318-320` |
| 客户端批量 | `F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\CloudSyncService.h:54-65`（`kServerMaxPushItems` / `kPushBatchSize` / `static_assert`） |

---

## 11. 环境事实与工具坑（不可从仓库推导）

1. **判别 (A)/(B) 的唯一手段 = 带令牌的 push 计时**：`wrangler tail` 对**已发生**的失败零回溯观测力（15:18:41 那次零事件）；`wrangler r2 object` **没有 `list` 子命令**（只有 `get` / `put` / `delete`）⇒ R2 对象无法枚举。
2. **探针 `Post-Json` 必须用 `-Token` 参数挂 `Authorization` 头**；用 `-Headers` 手写会漏（前一轮五档全 `401 missing_token`，计时数据全部作废）。带令牌请求的最小自证 = **空 items ⇒ 400 `bad_items`**（不是 401）。
3. `Invoke-D1` 解析 wrangler pretty-print JSON **必须**用 `[regex]::Match($raw,'(?s)\[\s*\{.*\}\s*\]')`；输出前还有含 `[` 的 `▲ [WARNING] Proxy environment variables detected.` 行。
4. **D1 SQL 即使解析失败也已在服务端执行** ⇒ 不要把「解析失败」误判成「UPDATE 没生效」。
5. `wrangler` 一切远端命令**必须先设** `HTTPS_PROXY` / `HTTP_PROXY` = `http://127.0.0.1:7897` 且 `NODE_USE_ENV_PROXY=1`（直连 `connect ETIMEDOUT 23.101.24.70:443`）；`wrangler tail` 通道要靠「打一条 `/v1/health` 看是否产出 invocation JSON」自证，**不能靠 banner**（`--format json` 下不打印）。
6. ~~`POST /v1/auth/register` 恒 503 `mail_send_failed` ⇒ 探针改走「注册后用 D1 把探针邮箱置 `email_verified=1`」+ `login`（`auth.ts:517` 登录不校验已验证，`auth.ts:543` 回显）。~~ **BQ 已修订（2026-11-22 · ADR 0075）**：该「恒 503」只在**保留域收件人**上成立（上游对 `example.com` 实际返回 **422 永久拒绝**，BQ 已改为 422 `mail_recipient_rejected` + 中文文案「该邮箱地址无法接收验证码，请更换邮箱地址后重试。」）；**真实域收件人注册返回 200**，探针可直接走正常注册 → 收码 → verify，不必再用 D1 置位。**但**保留域探针若仍要拿「可用账号」，本条的 D1 置位法继续有效 —— 前提是 D1 `UPDATE` 可用，**每日行写入配额耗尽期间会被拒**（Cloudflare `code 7500`，见 BQ 交接 §8 的 P0）。
7. **`auth_key` 必须生成即落盘**（`tmp/bo-push-probe-state.json`）：任一 FATAL 都会留下永不可登录的僵尸账号（本轮此前已产生 3 个）。
8. 每档 push **必须独立 `item_key` 前缀**（`{prefix}-n{n}-b{bytes}`），否则互相 upsert、计时与条数都会失真。
9. 探针凭据纪律：只打印 `auth_key_len` / `auth_key_sha16` / `token_len` / `token_sha16`；响应体 `access_token` / `refresh_token` 脱敏为 `<masked>`；`auth_key` 明文只进 state 文件，**不进日志/报告/对话/入库**。
10. 旧坑沿用（BO 轮已记，此处不重复）：`cmd //c` 假绿必须 `cmd /c`；`build.bat` 日志 GBK/UTF-16 混杂须按 BOM 判编码；整文件 clang-format 有既有漂移必须 `--lines=起:止`；`CHANGELOG.md` 是 CRLF-only 而 handoff/ADR/prompt 是 LF-only；NSIS `InstallDirRegKey` 指向 32 位视图（**无需清注册表**，清了丢 `RimeUserDir`）；`/D=` 必须是最后一个参数且不加引号；`Start-Process -Wait` 对 NSIS 挂死 ⇒ 用 `Wait-Process -Timeout`；装机后服务 pid 必变属预期；同进程内窗口 HWND 会变不能当稳定键；`Add-Type` 同名类型单会话内重复会失败 ⇒ 每个含 `Add-Type` 的脚本用独立 `pwsh -NoProfile -File`；派「只读取证 + 落盘报告」子会话**必须派 `worker`**（`explore`/`verifier` 秒返 `ROLE_MISMATCH`）；派子会话 `effort` **一律不传**。

---

## 12. 需要用户本人在场 / 待用户拍板

**需两台机器 + 用户在场（承接 BN/BO）**
1. 双机 AC 7 条。
2. restore 端到端。
3. 输入延迟两组对照。
4. G-3 / G-5 真跑。

**待用户拍板（承接 BL/BM/BN/BO）**
5. 18 份 `run-*.log` 是否 `git add -f` 入库（被 `.gitignore` 的 `*.log` 挡住）。
6. `tmp\bh-c2d-fmt-check.cpp`（83 767 B，本机策略删不掉）如何处置。
7. 是否授权**改服务端 `cloud/fluxing-sync/**` 并 `wrangler deploy`**（S1 方向的前提；BO 批 ADR 0073 曾以「不部署」为理由否决，本轮需重新拍板）。

**已知产品侧后果（刻意不做）**
8. 1017 是单行 STATIC，多提示叠加可能显示不全（ADR 0072 后果段已登记，刻意不做自动换行）。

---

## 13. 探针与云端数据现状（收尾需清理）

- 探针账号：`bo12002.probe.20261122154809@example.com`，`account_id = e3653ed8-ec8c-4dcc-be50-a2a5ded88e6b`（`users` 表 1 行、`email_verified=1`、`auth_key_sha16 = 3C14BD5EC4AEDB35`）。
- 云端遗留：探针 uid 下 `sync_items` **82 行**（ids 4–85）+ R2 对应 **82 个对象**；`refresh_tokens` / `devices` 若干行。
- **清理命令**：`pwsh -NoProfile -File tmp/bo-push-probe.ps1 -Mode Cleanup` ⇒ 脚本先按 `sync_items.r2_key` 逐个 `wrangler r2 object delete "fluxing-sync/<r2_key>" --remote`（`:116-126`），再删 D1 行（`:130`），最后按 `bo12002.probe.%@example.com` 删探针账号（`:141-144`）—— **顺序正确，别手工改**。
- **注意**：此前失败轮次留下的 **3 个僵尸账号**邮箱前缀可能不匹配 `bo12002.probe.%` ⇒ 收尾时按 `SELECT email FROM users WHERE email LIKE '%probe%'` 核对后手工清。
- **本轮故意未清理**：下一阶段要用同一账号 + 同一脚本复测服务端并发化效果（登录 → 五档再跑 → 比对 `elapsed_ms`）；**复测通过后立刻清理**。

---

## 14. Git 与工作区现状（接手时先看这里）

- 分支 `feat/100-cloud-sync`（跟踪 `kizemo/feat/100-cloud-sync`），HEAD `0e2ebdc`；BO 批提交 `ccf6477`（代码 + ADR 0073 + CHANGELOG）、`a68bfd4`（文档）已推。
- **本轮尚未提交任何内容**。新增/改写：`docs/archive/handoffs/handoff-2026-11-22-batch-bo-push-batch-align-server-limit.md`（就地改写）、`docs/archive/prompts/prompt-next-session-2026-11-22-batch-bo-push-batch-align-server-limit.md`（就地改写）、`CLAUDE.md`（补 BO 指针）。
- **未跟踪且不得入库**：`tmp/bo-push-probe-state.json`（含明文 `auth_key`）、`tmp/bo-push-probe.ps1`、`reports/fluxing-bo-12002/`；其余未跟踪项沿用前批（`release/`、`docs/design/quickpanel-v6/design-demos/*.html`、`scripts/verify/*.ps1`、`reports/fluxing-bh-ui/probe-bh.ps1.bak-pass8`）；`librime` 子模块 dirty（`m`）。
- 装机现场：`WeaselServer.exe` pid **1077296**（14:07:45 起），`D:\Program Files\fluxing\weasel\WeaselServer.exe`，sha16 `1E82FA7F30DBB263`（= BO 装机版）；`RimeUserDir` 未动、未登出/未登录/未改注册表；本机未锁屏。
