# compliance-check.md — 合规核查（T603）

> 批次 AN · 2026-11-11 实测 · 对应 AC-07（密文校验）/ AC-12（未登录零请求）
> 线上环境：`https://api.aiec.fun` · `MAIL_MODE="http"` · Version ID `29d4e572-49d0-43c2-b091-dda3029edf00`

## 1. AC-12 · 未登录零出向请求 —— ✅ 通过

**判据**：未登录状态下全流程出向请求数 = 0（FR-018）。

**实测**：客户端处于**未登录**状态（账号页从未登录成功，无凭据落地）时，对装机版
`WeaselServer.exe`（PID **257348**，启动 `2026-11-11 15:45:17`）做 10 s 窗口网络连接普查：

```powershell
$conns = Get-NetTCPConnection -State Established
$conns | Where-Object { $_.OwningProcess -eq 257348 }      # → 0 条
$conns | Where-Object { $_.RemoteAddress -eq '104.21.0.1' -or $_.RemoteAddress -eq '172.67.0.1' }  # → 0 条
```

| 观测 | 结果 |
|---|---|
| WeaselServer 进程数 | 1 |
| `ESTABLISHED` 出向连接（该 PID） | **0** |
| 全局指向 Cloudflare 网段的 `ESTABLISHED` | **0** |

**结构性依据**（AG 批次已落地）：`CloudSyncEngine` 与 `CloudCredentialStore` **解耦** ——
无凭据时引擎不进入推拉路径；`WeaselServerApp` 构造期即构造云账号控制器，是为了让
「同步不依赖用户是否打开过账号页」，**不等于**「未登录也会发请求」。

**证据**：`tmp/AN-t603-r2-check.txt`（同批网络普查）、`docs/archive/handoffs/handoff-2026-11-11-batch-ag-local-change-hook-and-server-deploy.md`。

## 2. AC-07 · 服务端存储抽查（密文）—— ⚠️ R2 侧就绪但本轮无对象可抽查；D1 元数据列存在明文词条

### 2.1 R2 侧：方法已就绪，本轮线上**无可抽查对象**

线上 `sync_items` 表当前为空（`SELECT user_id, item_key, r2_key, cipher_len FROM sync_items LIMIT 8`
⇒ `ROWS=0`），因为**探针闭环尚未完成**（`25940969@qq.com` 的 6 位验证码尚未回贴 ⇒ 没有登录态 ⇒ 没有推送 ⇒ R2 没有对象）。

抽查脚本已就绪，闭环后复跑即可出结论：

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File tmp\AN-t603-r2-check.ps1
```

脚本判据（四项同时成立才算「是密文」）：

1. 对象字节长度 == D1 `cipher_len`；
2. UTF-8 解码后**不含** CJK 字符（`[\u4e00-\u9fff]` 命中数 = 0）；
3. 可打印 ASCII 占比 ≈ 0；
4. 十六进制头部呈随机分布。

**证据**：`tmp/AN-t603-r2-check.txt`（本轮 `ROWS=0`）、`tmp/AN-t603-r2-check.ps1`。

### 2.2 服务端写入路径（代码级确认：R2 里是密文）

`cloud/fluxing-sync/src/sync.ts`：

- 推送时先 `env.BUCKET.put(target.r2Key, item.cipherBytes, ...)` —— **R2 里只有密文**；
- R2 键 = `u/<userId>/<itemKeyHashHex>`（`src/wire.ts::r2KeyFor`）—— 键本身不含词条；
- D1 `sync_items` 只存 `item_key` / `r2_key` / `cipher_len` 等**元数据**，不存密文本体。

### 2.3 ❗ 新发现的风险项：**D1 `sync_items.item_key` 明文存放词条**

客户端上报的 `item_key` 形状是 `<词典名>\t<词条>`（批次 AH 已确认 TAB 是分隔符，
服务端为此放行 TAB，见 ADR 0057）。服务端把它**原样**写进 D1 的 `item_key` 列：

```sql
CREATE TABLE sync_items (
  item_key TEXT NOT NULL,   -- ← 明文：含词条本身
  r2_key  TEXT NOT NULL,
  cipher_len INTEGER NOT NULL,
  ...
)
```

⇒ **AC-07 的字面判据「服务端存储与传输抓包中均无明文词条 / 短语 / 配置」在 D1 侧不满足**：
R2 满足（只有密文），D1 的 `item_key` 列**不是**密文。

影响面评估（本轮不做决策，留给下一批）：

- 词条本身经 PBKDF2 + AES-256-GCM 加密后才进 R2，**抓包可见的也是密文**（传输侧 OK）；
- 但 `item_key` 是**客户端上报的原文**，服务端没有再加密；能读 D1 的人能看到「用户在哪些词典里加了哪些词条」；
- 协议里**已经有**哈希形式（`itemKeyHashHex`，用于 R2 键）⇒ 存在把 D1 的 `item_key` 也改为**哈希列 + 独立可读名**的低成本改法；
- 已知代价：哈希后服务端**无法**再按 item_key 做可读排查（需要额外保留 display 名或只保留哈希）；
- 另一种可选路径：写 ADR 明确豁免「`item_key` 属元数据、非用户词条内容」—— 但这需要给出「词条确实在 `item_key` 里」的解释成本。

**本轮未改服务端**：改 `item_key` 语义会动 `sync_items` 表结构 + 推送/拉取/合并三处逻辑，
属独立的一批工作；本轮只把事实与影响面记录在案。

### 2.4 传输侧

客户端与服务端之间的载荷一律为信封密文（`FluxingCloudEnvelope.h`：8 行线格式 + AES-256-GCM，
批次 Y 落地），抓包侧无可读词条；账号口令走 PBKDF2-HMAC-SHA256（60 万次）+ HKDF，服务端
只存派生值，**不存明文口令**。

## 3. 结论

| 项 | 结论 |
|---|---|
| AC-12 未登录零出向请求 | ✅ **通过**（实测 0 条 ESTABLISHED + 代码路径解耦） |
| AC-07 R2 存储为密文 | ⚠️ **方法就绪，本轮无线上对象可抽查**（`sync_items` 为空 ⇒ 闭环后复跑脚本） |
| AC-07 D1 存储为密文 | ❌ **不满足**：`sync_items.item_key` 明文含词条 ⇒ **AC-07 整体不通过** |

## 4. 遗留

1. 探针闭环后复跑 `tmp/AN-t603-r2-check.ps1`，补齐 R2 四项判据；
2. 下一批处理 §2.3 的 D1 `item_key` 明文问题（哈希化改法 or ADR 豁免，需拍板）；
3. AC-07 的「传输抓包」项：需在客户端登录态下抓一次 HTTPS 流量（本轮未做，见 handoff 未闭环清单）。
