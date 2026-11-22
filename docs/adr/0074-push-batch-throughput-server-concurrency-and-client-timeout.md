# ADR 0074 — 推送批量吞吐：服务端并发化 + 客户端批量/超时重估（修订 ADR 0073 决策 5）

- **Status**: Proposed（决策待用户拍板；拍板后改为 Accepted）
- **Date**: 2026-11-22
- **Deciders**: kizemo（仓库维护者）+ mavis
- **Related**: spec `100-fluxing-cloud-account-sync`（**不改规格**）；ADR 0073（被本文修订决策 5）；批次 BO 交接文档
- **Revises**: ADR 0073 的**决策 5**（"不动服务端"）与**决策 1** 的取值理由（"逐值对齐上限即可"）。**ADR 0073 正文不改**（历史决策留痕）。

---

## Context

### 事实 1 — `12002` 的根因已定案：**服务端单批处理时长线性超过客户端 15 s 接收超时**

批次 BO 收尾轮用一次性探针账号（带真实 `Authorization` 头）直连 `/v1/sync/push` 做五档计时
（原始数据：`F:\soft\00selfmade\rime_claude\reports\fluxing-bo-12002\push-probe-report.txt`，
脚本 `tmp\bo-push-probe.ps1 -Mode Tiers`，`EXIT=0`）：

| 档位 | cipher 字节 | body 字节 | HTTP | elapsed_ms | 每条均摊 | 服务端返回 |
|---|---|---|---|---|---|---|
| 空 items（鉴权自证） | — | — | 400 | 1 894 | — | `bad_items`（**不是 401** ⇒ 带令牌请求确实到达服务端） |
| n=1 | 64 | 290 | 200 | 2 579 | 2 579 | `accepted` 1 条，`server_seq=4` |
| n=16 | 64 | 4 497 | 200 | 11 300 | 706.2 | `accepted` 16 条 |
| n=64 | 64 | 17 985 | 200 | **40 071** | 626.1 | `accepted` 64 条 |
| n=200 | 64 | 56 501 | 400 | 1 373 | — | `too_many_items`「单次推送条目数上限为 64（实收 200）。」 |
| n=1 × 4 MiB | 4 194 304 | 5 592 615 | 200 | 8 186 | 8 186 | `accepted` 1 条，`server_seq=85` |

落库核对 `D1_AFTER count=82 max_id=85`（= 1+16+64+1，与四档 `accepted` 逐档吻合）⇒
**每一档的条目都真实落库**，不是"到达但未处理"。

⇒ **(A)「没到达服务端 / 到达但无响应」排除**；(B)「到达了但服务端处理超过客户端超时」**证实**。

### 事实 2 — 根因代码位置：服务端**串行**写 R2

`cloud/fluxing-sync/src/sync.ts:149-169`：

```ts
// 1) 先写 R2（同 key 覆盖，天然幂等）；跳过的条目不写
for (let i = 0; i < accepted.length; i += 1) {
  await env.BUCKET.put(target.r2Key, item.cipherBytes, { ... });   // ← 每条一次往返，串行
}
// 2) 再写 D1：DELETE + INSERT（env.DB.batch）
```

写序不变量（`sync.ts:1-7`）：**R2 先、D1 后**。每条一次 R2 往返 ≈ 0.63 s ⇒
**条目数是唯一的一阶放大因子**，与密文体积基本无关（单条 4 MiB 也只花 8.2 s）。

### 事实 3 — 客户端超时来源与"批量对齐"的失效

- `WeaselServer/Cloud/CloudTransport.h:72`：`timeout_ms = 15000`，四相（连接/发送/接收/…）**共用**同一超时；
  失败落在 `WinHttpReceiveResponse` 阶段 ⇒ `CloudTransport.cpp:318-320` 输出「接收响应失败：」⇒
  `12002 = ERROR_WINHTTP_TIMEOUT`。
- ADR 0073 决策 1 把 `kPushBatchSize` 从 200 改成 **64**，理由是"与服务端 `MAX_PUSH_ITEMS` 逐值对齐"。
  ⇒ **对齐了"条目数上限"，没有对齐"15 s 内能处理多少条"**。64 条这一批服务端要 40.1 s ⇒
  **必然**超时。批量对齐 ≠ 批量可行。

### 事实 4 — ADR 0073 决策 5 的前提已失效

ADR 0073 决策 5「不动服务端」的理由是：*"改 `MAX_PUSH_ITEMS` 需要重新部署 Worker、放弃写放大保护、
旧客户端仍按自己的值发 ⇒ 客户端改一个常量即可闭合"*。
本轮实测显示该理由不成立：**不改服务端，客户端无论把批量调到多少都逃不掉 15 s**
（降到 ≤16 条可行，但那是"每次同步等更久"；加长超时也只是把墙往后挪）。
⇒ **服务端并发化必须进入评估范围**。

### 事实 5 — 部署口径：改动**直达生产**，无 staging

`cloud/fluxing-sync/wrangler.toml`：`workers_dev = false`、`preview_urls = false`、
唯一路由 `routes = [{ pattern = "api.aiec.fun", custom_domain = true }]` ⇒
`wrangler deploy` **直接替换生产 Worker**（真实用户 `25940969@qq.com` 的同步数据在此后端），
仓库内**没有**第二套环境可供预演。⇒ 需要用户显式授权。

### 事实 6 — 已排除的其它可能

代理/网络栈假设排除（`ProxyEnable=0`、`netsh winhttp show proxy` = Direct、`/v1/health` 5 轮 200）；
鉴权层不慢（未带令牌的 push 267 ms 返回 401）；真机侧请求确实到达服务端
（诊断日志两条 `[cloudsync] access token refreshed after 401; retry with new token`，服务端回了 401）。

---

## Decision

### 共同子决策（三条路径都适用）

1. **ADR 0073 正文不改**，由本文修订其决策 5（README「Lifecycle」：accepted ADR 的正文不可编辑，
   只能被新的 ADR 取代并回链）。
2. **服务端写序不变量「R2 先、D1 后」保持不变**；并发化只改"R2 阶段内部的并发度"，不改"先 R2 后 D1"的次序。
3. **客户端必须补一条无条件诊断行**：每批的「条目数 + elapsed_ms + HTTP 状态」，
   落 `%TEMP%\fluxing-profile-notify.log`。理由：批次 BO §6 里"现场那一轮确实推了 64 条"至今是**推断**，
   这条诊断行把它变成**直证**，并且今后任何批量/超时改动都可被机读复核。
4. **不动 IPC 协议消息类型、不动 `include/WeaselIPCData.h`、不动 `librime/`、不新增第三方依赖、
   不 bump 版本号**（装机版仍 1.3.8.0，判定一律用 sha16 / 字节数）。
5. **`/v1/auth/register` 恒 503 `mail_send_failed` 是独立缺陷**（`mail_outbox` 无行 ⇒ 新用户无法完成
   "注册 → 收码 → 验证"），与 12002 无关，**另立批次**修，不在本 ADR 范围。

### 待拍板的三条路径

| | 做法 | 优点 | 代价 |
|---|---|---|---|
| **S1** | 服务端 `sync.ts:149-169` 改**受控并发**（分片并发，如 8 路一批，`Promise.all`），写序不变 | **已装机旧客户端（BN/BO 版，批量 64）也能受益**；大批量吞吐根治 | 需 `wrangler deploy` **直达生产**；并发失败时 R2 孤儿对象变多（现状孤儿已存在，pull 侧有自愈）；需改服务端单测 |
| **S2** | 只改客户端：`kPushBatchSize` ≤ 16（15 s 预算内），`timeout_ms` 提到 45–60 s | 完全不动生产后端 | 整轮耗时线性变长（200 条 ≈ 13 批 ≈ 2 分钟+）；**已装机旧客户端仍持续 12002 失败**，只能救新装包；等于把问题挪给用户等 |
| **S3** | **S1 + S2 配套**：服务端并发化把 64 条压到数秒级，客户端批量 32、超时 30 s，双保险 | 根治吞吐 + 新客户端留足超时余量 | 同 S1（需部署）+ 客户端改动面更大（批量/超时/诊断行三处） |

**推荐：S3**。理由：S1 是唯一能同时救**已装机旧客户端**与**大批量**的方向；S2 的降批量在旧客户端上
不生效 ⇒ 只做 S2 等于把"同步永久失败"换成"同步极慢且仍需重装"。S3 在 S1 之上加客户端余量，
避免服务端将来再次退化时客户端立刻超时。

### 拍板后本文将被改写为

- `**Status**: Accepted`；
- 「待拍板的三条路径」替换为**被选中的那一条**及其一句话理由，另外两条保留在表里并标注**未选中的拒绝理由**；
- 追加该路径的**具体实施口径**（并发度上限、超时取值、批量取值、诊断行文案）与**门禁判据**。

---

## Consequences

### 若选 S3（推荐）

- ✅ 64 条批量在服务端从 40.1 s 降到秒级 ⇒ 客户端 15 s 超时不再是常态 ⇒ `12002` 消失。
- ✅ 已装机的 BN/BO 旧客户端（批量 64）**无需重装**即受益。
- ✅ 新客户端另有 30 s 超时 + 批量 32 的余量，服务端将来退化时不会立刻炸。
- ⚠️ 客户端与服务端**必须协同演进**：若只发服务端不改客户端参数，客户端仍带 15 s 超时（够用但无余量）；
  若只改客户端不改服务端，批量降到 16 只是把墙往后挪。
- ⚠️ R2 孤儿对象（"D1 没写成、R2 已写"）在并发失败时增多；pull 侧已有自愈逻辑，但需在验收里核对
  `sync_items` 与 R2 对象数的一致性。
- ⚠️ 生产部署无 staging ⇒ 部署前必须跑通服务端本地门禁（`npm run check` = 端点策略 + vitest；
  `npm run typecheck`），部署后必须用**同一探针**复测。

### 若选 S2（只改客户端）

- ✅ 零生产风险；新装包在 15 s 预算内可完成 ≤16 条的推送。
- ⚠️ **已装机用户继续失败**（BO 装机版批量 64），必须发新包才能修复 ⇒ 与本项目的"出包装机"节奏绑定。
- ⚠️ 200 条的同步轮次耗时 ≈ 13 批 × ≈ 11 s ≈ 2 分钟以上 ⇒ 用户侧等待显著变长。
- ⚠️ 根因（服务端串行 R2 写）**留在代码里**，下一个 65+ 条的批次仍会以另一种方式复发。

### 若选 S1（只改服务端）

- ✅ 根治吞吐，且旧客户端立即受益。
- ⚠️ 客户端仍带 15 s 超时：服务端压到秒级后够用，但**没有余量**——服务端将来任何退化都会立刻表现为
  `12002`，且排查成本回到"猜服务端"。

---

## 验证口径（拍板后执行，按此顺序）

**服务端（S1/S3 涉及）**

1. `cd cloud\fluxing-sync` ⇒ `npm run check` ⇒ 端点策略测试（`test/check-endpoint-policy.mjs`，
   禁止默认 workers.dev 域名字面量）+ vitest 全绿。
2. `npm run typecheck` ⇒ `tsc --noEmit` 零错误。
3. `wrangler deploy`（**仅在用户授权后**）⇒ 生产路由 `api.aiec.fun` 生效。
4. 用**同一探针**复测：`pwsh -NoProfile -File tmp\bo-push-probe.ps1 -Mode Tiers`（脚本不改，可比）。
   **判据：n=64 档 `elapsed_ms` < 15 000（目标 ≤ 5 000），且 `accepted` 仍 = 64，
   `D1_AFTER count` 与四档 `accepted` 之和逐档吻合。**

**客户端（S2/S3 涉及）**

5. 诊断行生效：真机一轮同步后，`%TEMP%\fluxing-profile-notify.log` 能读到「每批条目数 + elapsed_ms + HTTP 状态」。
6. 门禁照批次 BO 口径：`cmd /c "build.bat release weasel installer"` ⇒ `BUILD_EXIT=0`（0 警告 0 错误）；
   `TestFluxingCloud` 须显式 msbuild **且带 `/p:SolutionDir=F:\soft\00selfmade\rime_claude\`**、
   exe **在仓库根跑** ⇒ 相对基线 **704** 不回归；`scripts/gate/L0-static.ps1 -Mode all` ⇒ **15/15**；
   clang-format **18.1.8** 定点 `--lines=起:止` ⇒ **0 漂移**；改动文件 **U+FFFD=0**。
   **顺序铁律：格式化 → 复检 → 构建 → 单测 → 打包 → 装机 → 记 sha。**
7. 真机成功侧复验：真实账号 `25940969@qq.com` 登录态点「立即同步」⇒ 1017 变**成功态**且诊断日志无 `12002`。
8. 收尾：`pwsh -NoProfile -File tmp\bo-push-probe.ps1 -Mode Cleanup`（脚本先删 R2 对象再删 D1 行再删账号，
   **顺序正确，别手工改**）+ 按 `SELECT email FROM users WHERE email LIKE '%probe%'` 清 3 个僵尸账号。
9. 提交：`git status -sb` + `git diff --stat` 后**按路径显式** `git add`（`git add .` 会扫进
   `weasel.props` / `env.bat` / `*.log` / 含明文凭据的 state 文件）。

---

## See also

- `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-22-batch-bo-push-batch-align-server-limit.md`
  —— 批次 BO 真源（§4 五档计时原始数据、§6 现场对应关系、§9 下一阶段计划）
- `F:\soft\00selfmade\rime_claude\docs\adr\0073-client-push-batch-aligns-with-server-limit.md`
  —— 被本文修订决策 5 的 ADR
- `F:\soft\00selfmade\rime_claude\reports\fluxing-bo-12002\push-probe-report.txt` —— 五档计时原始报告
