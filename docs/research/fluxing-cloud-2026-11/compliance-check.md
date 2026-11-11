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

## 2. AC-07 · 服务端存储抽查（密文）—— ✅ 通过（R2 实测密文 + D1 身份已 MAC 化）

### 2.1 R2 侧：抽查**实测通过**（批次 AO，2026-11-11）

探针闭环跑通后线上 `sync_items` 有 **1 行**（`server_seq=3`），R2 有 **1 个 75 B 对象**。
抽查脚本改为 Node 版 `tmp/AO-t603-r2-check.mjs`（旧的 PowerShell 版用 `cmd /c "node … > file"` 下载对象，
在本机 pwsh 7 下内嵌引号被改写 ⇒ `The system cannot find the path specified.`，已废）：

```powershell
node tmp\AO-t603-r2-check.mjs
```

实测 **5 断言 / 0 失败 / `T603_VERDICT=PASS` / `R2_CHECK_EXIT=0`**（`tmp/AO-t603-r2-check.txt`）：

| 判据 | 实测 |
|---|---|
| 对象字节长度 == D1 `cipher_len` | **75 B vs 75 B** ✅ |
| UTF-8 解码后不含 CJK（`[\u4e00-\u9fff]`） | `cjk=0` ✅ |
| 可打印 ASCII 占比 ≈ 0 | `printable=23/75 ratio=0.3067`（阈值 <0.35；75 B 随机样本理论期望 95/256≈0.371，属正常波动）✅ |
| 无可读 ASCII 连续串（≥6 字符） | `0 段` ✅ |

对象头部 `HEAD_HEX = b4 80 cd 1e 55 a4 c2 7f 88 d1 16 4f c3 5c e6 38 …`（随机分布）。抽查后立即删除本地密文副本，不留盘。

### 2.2 服务端写入路径（代码级确认：R2 里是密文）

`cloud/fluxing-sync/src/sync.ts`：

- 推送时先 `env.BUCKET.put(target.r2Key, item.cipherBytes, ...)` —— **R2 里只有密文**；
- R2 键 = `u/<userId>/<itemKeyHashHex>`（`src/wire.ts::r2KeyFor`）—— 键本身不含词条；
- D1 `sync_items` 只存 `item_key` / `r2_key` / `cipher_len` 等**元数据**，不存密文本体。

### 2.3 ✅ D1 `sync_items.item_key` 明文问题已修复（批次 AO，方案 K）

**原缺陷**：客户端上报的 `item_key` 形状是 `<词典名>\t<词条>`（批次 AH 已确认 TAB 是分隔符，ADR 0057 要求服务端放行 TAB），
服务端把它**原样**写进 D1 的 `item_key` 列 ⇒ 能读 D1 的人即可看到「用户在哪本词典里加了哪些词条」；
同一份明文也随 JSON 请求体上行 ⇒ **传输侧同样可读**。这才是 AC-07 的真实失败点。

**修法（零服务端改动 · 零表结构改动 · 零重新部署）**：上一轮「会动表结构 + 三种逻辑」的评估被推翻 ——
服务端对 `item_key` **没有任何语义解析**（只 `assertHeaderString` + 等值比较 + 原样回传），把它换成任意不透明串天然合法。
客户端改为：

- 线上身份 = `lowercase_hex(HMAC-SHA256(HKDF(enc_key, "fluxing-itemkey-mac-v1"), item_key))`（**64 字符**）；
- 明文键搬进 AEAD 覆盖范围内的长度前缀封包 `[4B 大端 key_len][明文 item_key][payload]`（**不用分隔符**：词条可含 TAB，ADR 0057 已把 TAB 定为合法字符）；
- Pull 侧重算 MAC 自校验，防「A 的密文 + B 的身份」替换攻击。

**用 MAC 而非裸 SHA-256**：词条熵极低（`<词典名>\t<词条>` 候选空间小到可枚举），裸哈希可字典穷举反推明文。

**实测（`tmp/AO-probe-closure-5.log`、`tmp/AO-t603-r2-check.txt`）**：

- `sync_items.item_key` 全部为 **64 位小写十六进制**，非 hex **0 行**；
- **D1 无明文 `item_key` 残留行**；`sync_items` 列清单不含任何明文载荷列（`item_key,type,hlc_ms,hlc_counter,hlc_device,r2_key,nonce_b64,cipher_len`）；
- 线上条目身份 `id=cd3cc4e9afb595db… 长度=64` 且 `≠ 明文键`、不含 `userdb` / `探针词条` 子串；
- 拉回密文可用派生密钥解密，`item_key` 与载荷**双一致**（自校验未被误伤）。

**兼容性**：旧版本写入的明文身份行**一次性清空、不做兼容分支**（旧行 AAD 用明文身份，方案 K 下必然解不开）。
决策全文见 [ADR 0059](../../adr/0059-item-key-opaque-mac-identity.md)。

### 2.4 传输侧

客户端与服务端之间的载荷一律为信封密文（`FluxingCloudEnvelope.h`：8 行线格式 + AES-256-GCM，
批次 Y 落地），抓包侧无可读词条；账号口令走 PBKDF2-HMAC-SHA256（60 万次）+ HKDF，服务端
只存派生值，**不存明文口令**。

## 3. 结论

| 项 | 结论 |
|---|---|
| AC-12 未登录零出向请求 | ✅ **通过**（实测 0 条 ESTABLISHED + 代码路径解耦） |
| AC-07 R2 存储为密文 | ✅ **通过**（75 B 对象四项判据全过） |
| AC-07 D1 存储为密文 | ✅ **通过**（`item_key` = 64 位小写 hex MAC，无明文词典名 / 词条） |
| AC-07 传输侧无明文 | ✅ **通过（代码级）**：线上 `item_key` 已是 MAC，明文键在 AEAD 密封载荷内；口令走 PBKDF2-HMAC-SHA256（60 万次）+ HKDF，服务端只存派生值 |

⇒ **AC-07 由「不通过」转为「通过」**（存储侧为线上实测，传输侧为代码级 + 探针同形态断言）。

## 4. 遗留

1. ~~探针闭环后复跑 R2 抽查~~ ⇒ **✅ 已完成**（批次 AO，5 断言 PASS）；
2. ~~下一批处理 §2.3 的 D1 `item_key` 明文问题~~ ⇒ **✅ 已修复**（方案 K，ADR 0059）；
3. **真实客户端 HTTPS 抓包**仍未做 —— 本轮证据是「探针以同一信封逻辑发出的线上身份 = 64 位 hex MAC」+ 服务端存储抽查；
   若要更强证据，需在登录态客户端抓一次流量（**补强项，不改结论**）。
