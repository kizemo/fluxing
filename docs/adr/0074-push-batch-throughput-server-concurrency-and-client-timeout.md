# ADR 0074 — 推送批量吞吐：服务端并发化 + 客户端批量/超时重估（修订 ADR 0073 决策 5）

- **Status**: **Accepted**（2026-11-22 用户拍板 S3；服务端已 `wrangler deploy` 直达生产 `api.aiec.fun`，实测 n=64 档 **10 330 ms**）
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

### 拍板结果：**选 S3**（用户 2026-11-22 拍板）

被选中的路径：**S3 = 服务端并发化（S1）＋ 客户端批量/超时/诊断行（S2 配套）**。
一句话理由：S1 是唯一能同时救**已装机旧客户端**与**大批量**的方向；S2 的降批量在旧客户端上不生效 ⇒
只做 S2 等于把"同步永久失败"换成"同步极慢且仍需重装"；S3 在 S1 之上加客户端余量，
避免服务端将来再次退化时客户端立刻超时。

| 路径 | 结论 | 理由 |
|---|---|---|
| **S1** 服务端受控并发 | **已选（作为 S3 的前半）** | 根治吞吐，旧客户端立即受益；但客户端仍带 15 s 超时、没有余量 |
| **S2** 只降批量 + 加超时 | **已选（作为 S3 的后半，另加诊断行）** | 完全不动生产后端；只做它救不了已装机旧客户端 |
| **S3** S1 + S2 配套 | **✅ 被选中** | 根治吞吐 + 新客户端留足超时余量；代价 = 需 `wrangler deploy` 直达生产 + 客户端三处改动 |

> S1 与 S2 不是互斥选项：S3 = S1 + S2，故两条在 S3 里**同时成立**；
> "只做 S1"（不动客户端参数）与"只做 S2"（不动服务端）分别是本表的两个退化形态。

### 实施口径（S3 具体取值）

**服务端**（新增 `cloud/fluxing-sync/src/r2-write.ts` + 改写 `src/sync.ts:149-169`）：

1. 并发度常量 `R2_WRITE_CONCURRENCY = 6` —— 取 Cloudflare Workers「同时打开连接数上限 6」，
   再高会触发 `Network connection lost`；不引入新依赖。
2. 新增 `putAllWithConcurrency(bucket, targets, concurrency)`：按「波」推进（每波 ≤ `concurrency` 个 put，
   `Promise.all` 等整波落盘再放下一波），任意时刻在飞 put 数严格 ≤ 上界；写序不变（R2 全部落盘后才写 D1）。
3. `sync.ts` 的串行 put 循环替换为该 helper；`staged[i]` ↔ `accepted[i]` 索引对齐不变，
   put 选项（`httpMetadata.contentType='application/octet-stream'` 与
   `customMetadata{type, hlc_ms, hlc_counter, nonce_b64}`）原样透传。
4. **不改** `MAX_PUSH_ITEMS = 64`、**不改** D1 阶段、**不改** HLC 守卫、**不改** 幂等语义（同 key 覆盖）。

**客户端**（`WeaselServer/Cloud/*`）：

5. `CloudTransport.h:72` 与 `CloudTransport.cpp:153-154`：`timeout_ms` **15 000 → 30 000**（四相共用）。
6. `CloudSyncService.h`：`kPushBatchSize` **64 → 32**（保留 `kServerMaxPushItems = 64` 与 `static_assert`）。
7. `CloudSyncService.cpp` 推送批次循环新增**无条件**诊断行
   `[cloudsync] push batch items=<n> elapsed_ms=<ms> http=<status>`（失败也打），
   落 `%TEMP%\fluxing-profile-notify.log` —— 把"现场那轮推了几条"从推断变直证。

### 门禁判据（S3）

- 服务端：`npm run typecheck`（`tsc --noEmit` 零错误）+ `npm run check`（端点策略 + vitest 全绿）。
- 服务端实测：`wrangler deploy` 后同一探针 `-Mode Tiers` ⇒ **n=64 档 `elapsed_ms` < 15 000**
  （目标 ≤ 5 000），`accepted` 仍 = 64，`D1_AFTER count` 与四档 `accepted` 之和逐档吻合。
- 客户端：`cmd /c "build.bat release weasel installer"` ⇒ `BUILD_EXIT=0`（0 警告 0 错误）；
  `TestFluxingCloud` 显式 msbuild **且带 `/p:SolutionDir=F:\soft\00selfmade\rime_claude\`**、exe 在仓库根跑
  ⇒ 相对基线 **704** 不回归；`scripts/gate/L0-static.ps1 -Mode all` ⇒ **15/15**；
  clang-format **18.1.8** 定点 `--lines=起:止` ⇒ **0 漂移**；改动文件 **U+FFFD=0**。
- **顺序铁律：格式化 → 复检 → 构建 → 单测 → 打包 → 装机 → 记 sha。**

---

## Consequences

### 服务端实测（2026-11-22，生产 version `fcdd38ed-afb4-4ee2-989a-65b6453d9d81`）

| 档位 | 改前 elapsed_ms | 改后 elapsed_ms | 倍数 |
|---|---|---|---|
| n=1 | 2 579 | 2 061 | 0.80× |
| n=16 | 11 300 | 3 568 | **3.17×** |
| n=64 | **40 071** | **10 330** | **3.88×** |
| n=200（400 拒绝） | 1 373 | 1 325 | — |
| n=1 × 4 MiB | 8 186 | 5 547 | 1.48× |

- ✅ **64 条 10 330 ms < 客户端 15 000 ms** ⇒ `12002` 的服务端侧根因解除，**已装机的旧客户端无需重装**即受益。
- ⚠️ **未达成「≤ 5 000 ms」目标值**（实际 10 330 ms）。原因：并发度已被 Cloudflare「同时打开连接数 = 6」
  封顶，64 条 ⇒ 11 波、单波 ≈ 0.85 s ⇒ 理论下限 ≈ 9–10 s；再往上并发会触发 `Network connection lost`。
  **判据「< 15 000 ms」达成**，故不为追目标值而冒险提高并发。
- ✅ `accepted` 逐档 = 条目数、`D1_AFTER count=164`（82 + 82）与四档 `accepted` 之和逐档吻合
  ⇒ HLC 守卫、幂等覆盖、写序「R2 先、D1 后」全部未变。
- ✅ 已装机的 BN/BO 旧客户端（批量 64）**无需重装**即受益；新客户端另有 30 s 超时 + 批量 32 的余量，
  服务端将来退化时不会立刻炸。
- ⚠️ 客户端与服务端**必须协同演进**：只发服务端不改客户端参数，客户端仍带 15 s 超时（够用但只剩 ≈ 4.7 s 余量）；
  只改客户端不改服务端，批量降到 32 只是把墙往后挪。
- ⚠️ R2 孤儿对象（"D1 没写成、R2 已写"）在并发失败时增多；pull 侧已有自愈逻辑，验收需核对
  `sync_items` 与 R2 对象数的一致性。
- ⚠️ 生产部署无 staging ⇒ 部署前跑通服务端本地门禁，部署后用**同一探针**复测（本轮已做）。

> 以下两节为**未选路径**的后果，留档备查。

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

### 本轮（2026-11-22）实测结果

| 步骤 | 判据 | 结果 |
|---|---|---|
| 服务端 `npm run typecheck` | `tsc --noEmit` 零错误 | ✅ `TSC_EXIT=0` |
| 服务端 `npm run check` | 端点策略 + vitest 全绿 | ✅ `CHECK_EXIT=0`，4 文件 / **65** 用例通过（新增 `test/r2-write.test.ts` 8 条断言） |
| `wrangler deploy` | 生产 `api.aiec.fun` 生效 | ✅ version `fcdd38ed-afb4-4ee2-989a-65b6453d9d81` |
| 探针 `-Mode Tiers` 复测 | n=64 `elapsed_ms` < 15 000 | ✅ **10 330**（改前 40 071），`accepted`=64，`D1_AFTER count=164` 逐档吻合 |

---

## See also

- `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-22-batch-bo-push-batch-align-server-limit.md`
  —— 批次 BO 真源（§4 五档计时原始数据、§6 现场对应关系、§9 下一阶段计划）
- `F:\soft\00selfmade\rime_claude\docs\adr\0073-client-push-batch-aligns-with-server-limit.md`
  —— 被本文修订决策 5 的 ADR
- `F:\soft\00selfmade\rime_claude\reports\fluxing-bo-12002\push-probe-report.txt` —— 五档计时原始报告
