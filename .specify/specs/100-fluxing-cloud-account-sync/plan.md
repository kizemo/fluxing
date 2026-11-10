# 100 · Plan · 云端账号与多端同步（Cloudflare 后端 + 端到端加密）

> 配套 `spec.md`（用户视角）与 `tasks.md`（执行序）。技术选型、协议、算法、变更面、风险全在本文。
> **取代** spec `010-cloud-sync` 的技术结论（后端选型 / 客户端依赖库 / 认证方案三项均已变）。
>
> **修订记录**
>
> | 日期 | 修订范围 | 依据 |
> |---|---|---|
> | 2026-11-11 | §3.1 密钥派生（CNG → 自实现 PBKDF2/HKDF + salt 策略）· §4.4 邮箱验证码（$0 路线）· §5 模块与线程模型（落地为 18 个文件 + 快照通道）· §6.1 线上契约补充 · §6.2 D1 表（7 表 + D1–D6）· §9 变更面 · §13 验证策略 | 阶段 D 实施结果；ADR 0054；handoff 批次 AA / AB |
>
> **本次修订的授权**：用户 2026-11-11 拍板「`plan.md` / `tasks.md` 是否按实现回写 6 处偏差」→ **回写**。本文件自此与代码同源。

---

## 1. 技术上下文

| 层 | 选型 | 为什么 |
|---|---|---|
| 客户端语言 | C++ / Win32 / WTL（既有栈，不引入新框架） | 宪法 P3；UI 与既有 `QuickPanelDialog` / `AboutDialog` 同栈 |
| 客户端网络 | **WinHTTP**（`winhttp.lib`，系统自带） | **仓库当前零 HTTP 客户端**（实测 `WinHttp|WinINet|curl|libcurl` 全零命中）；010 设计的 libcurl 并不存在。WinHTTP 是操作系统组件 ⇒ 不违反 P3「不新增第三方依赖」，也满足 spec 100 NFR-004 |
| 客户端加密 | **Windows CNG**：`BCryptDeriveKeyPBKDF2` + `BCryptEncrypt(GCM)` | 同上，系统自带；不自己实现密码学 |
| 客户端凭据 | **Windows Credential Manager**（`CredWriteW` / `CredReadW`） | 系统自带；DPAPI 保护；010 已定此路 |
| 服务端运行时 | **Cloudflare Workers**（TypeScript，`wrangler`） | 用户指定 Cloudflare；Workers 免费版 10 万请求/天、10ms CPU/请求 |
| 服务端存储 | **R2**（密文对象）+ **D1**（账号 / 设备 / 索引元数据） | R2 免费 10 GB 存储、100 万次 Class A 写/月、1000 万次 Class B 读/月、**零出口费**；D1 免费 500 万行读/天、10 万行写/天、5 GB |
| 服务端密钥 | Workers **Secret**（服务端 pepper / 签名密钥） | 不进代码库、不进 D1 |
| 服务端代码位置 | 仓库新目录 `cloud/fluxing-sync/`（`wrangler.toml` + `src/` + `migrations/`） | 与 C++ 代码同仓、同许可（GPLv3） |

**KV 的排除理由**：Workers KV 免费额度为 **10 万次读/天但只有 1000 次写/天**，写额度会被同步热路径瞬间打穿 ⇒ KV 只可用于冷数据（如设备黑名单），不进同步路径。

## 2. 总体架构

```
┌──────────────── 设备 A ────────────────┐        ┌──────────────── 设备 B ────────────────┐
│ WeaselServer（唯一常驻进程）             │        │ WeaselServer（唯一常驻进程）             │
│  ├ WeaselServer/Cloud/CloudSyncEngine   │        │  ├ WeaselServer/Cloud/CloudSyncEngine   │
│  │   （后台线程；不在输入线程上）        │        │  │   （后台线程；不在输入线程上）        │
│  ├ WeaselServer/Cloud/CloudAccount       │        │  ├ WeaselServer/Cloud/CloudAccount       │
│  ├ WeaselServer/Cloud/CloudCredential    │        │  ├ WeaselServer/Cloud/CloudCredential    │
│  ├ WeaselServer/Cloud/CloudCrypto (CNG)  │        │  ├ WeaselServer/Cloud/CloudCrypto (CNG)  │
│  └ WeaselServer/Cloud/CloudAccountDialog │        │  └ WeaselServer/Cloud/CloudAccountDialog │
│         ↕ 进程内（不新增 IPC 协议）       │        │         ↕ 进程内（不新增 IPC 协议）       │
└────────────┬─────────────────────────────┘        └────────────┬─────────────────────────────┘
             │ HTTPS（WinHTTP）                              │ HTTPS（WinHTTP）
             └──────────────┬─────────────────────────────────┘
                            ▼
        Cloudflare Worker（官方统一端点，自有域名）
          ├ /v1/auth/register | login | refresh
          ├ /v1/sync/push | pull
          ├ /v1/devices | /v1/account（清空）
          ├ D1：users / devices / refresh_tokens / sync_items（**只存元数据**）
          └ R2：fluxing-sync 桶 —— 只存**密文对象**
```

**D1 与 R2 的分工**：D1 存「哪个用户、哪个条目、什么版本、对象在哪」的索引（可按版本做增量查询）；R2 存密文本体。D1 **不存**任何同步内容。

## 3. 认证（用户拍板：邮箱 + 密码，客户端输入）

### 3.1 密钥派生（客户端本地，PBKDF2-HMAC-SHA256 → HKDF-SHA256）

> **2026-11-11 修订（ADR 0054）**：原式写「via CNG」，实做**自实现** PBKDF2 / HKDF —— CNG 的
> `BCryptDeriveKeyPBKDF2` **默认 PRF = SHA-1**（不显式指定就是 SHA-1，与 WebCrypto 的
> PBKDF2-HMAC-SHA256 不对齐），且 CNG **没有** HKDF 的 extract-then-expand 语义。
> **AES-256-GCM 仍走 CNG**（复用 `include/FluxingCloudEnvelope.h` 的 `crypto::AesGcmSeal/Open`）。

```
salt        = SHA256("fluxing-enc-v1|" + lower(email))        // 定值、可由客户端自算
master      = PBKDF2-HMAC-SHA256(password, salt, iters=600000) // 32 B，PRF = HMAC-SHA256
auth_key    = HKDF-SHA256(master, info="fluxing-auth-v1")     // 32 B，登录凭证
enc_key     = HKDF-SHA256(master, info="fluxing-enc-v1")      // 32 B，同步加密密钥
```

- HKDF 细节：salt = **32 字节全零**、输出**单块 32 字节**（`PRK = HMAC(zeros32, ikm)`；`OKM = HMAC(PRK, info || 0x01)`）。
- **salt 策略（ADR 0054）**：服务端**从不用 salt 做校验**（只比 `HMAC(pepper, auth_key)`），而客户端必须在登录**之前**算出 `auth_key` ⇒ 拿不到服务端随机 salt ⇒ salt 只能是**按邮箱确定性派生**的。服务端响应里的 `salt` 字段只作**诊断记录**，不参与派生。
- **salt 不是熵**：它只提供域分离（防同一密钥跨用途复用），口令强度仍是唯一防线（见 §3.4）。
- `iters=600000` 依据 OWASP 对 PBKDF2-HMAC-SHA256 的现行建议；**必须在后台线程跑**（绝不能放在 UI 线程）。
- **为什么不用 Argon2id**：仓库无第三方依赖政策下不引入密码学库；PBKDF2 / HKDF 是**标准化原语**（RFC 8018 / RFC 5869），可用**官方向量**钉死，而 Argon2id 无官方向量、参数结构复杂 ⇒ 自己实现的风险显著更高。代价是抗 GPU 弱于 Argon2id —— 用迭代次数 + 口令强度要求 + 可选独立「同步口令」补偿（见 §3.4）。
- **为什么不用 PAKE（OPAQUE/SRP）**：客户端要实现一套密码学协议，同样违反 NFR-004，且实现风险远大于收益。取舍记录在 ADR 0039。

### 3.2 服务端看到什么

| 值 | 是否上传 | 服务端存什么 |
|---|---|---|
| 明文口令 | ❌ 永不上传 | 无 |
| `auth_key` | ✅ 仅在注册 / 登录请求体 | `verifier = HMAC-SHA256(server_pepper, auth_key)` |
| `enc_key` | ❌ 永不上传 | 无 |
| 同步内容 | ✅ 但已加密 | R2 中的密文 |

⇒ **服务端数据库被脱库**（且拿不到 Workers Secret 里的 pepper）时：拿不到 `enc_key`，还原不了词库 / 短语 / 配置明文。
⇒ **服务端被完全攻陷**（含 Secret）时：可以冒充用户登录（拿到 `auth_key` 的人可以登录），但**仍然读不到同步内容**。这是「内容机密性」与「账号接管」分离的诚实边界，写入 ADR 与账号页说明。

### 3.3 令牌与凭据

| 项 | 形态 | 存放 |
|---|---|---|
| access token | 短期（默认 1 h），不透明随机串 | 仅内存 |
| refresh token | 长期（默认 30 d），服务端只存哈希 | 客户端存 **Windows Credential Manager**（DPAPI） |
| `enc_key` | 32 B | 客户端存 Credential Manager（DPAPI）；新设备由口令重新派生 |

### 3.4 口令与「同步口令」

首版 `enc_key` 由登录口令派生。已知取舍：**口令弱 ⇒ 云端内容可被离线字典攻击还原**（因为 `verifier` 在服务端，攻击者可以拿 `verifier` 离线验证口令猜测 —— 但注意没有 pepper 时需同时命中 `HMAC(pepper, auth_key)`，pepper 在服务端 Secret 里，单纯 DB 脱库**不能**离线验口令）。
⇒ 补偿措施：① 口令强度下限（长度 + 字符类，弱口令拒绝）；② 可选设置**与登录口令不同的「同步口令」**，`enc_key` 改由同步口令派生（登录口令被攻破也不影响内容）；③ 界面明示「口令丢失 = 云端数据不可恢复」。

## 4. 端到端加密与同步信封（用户拍板：全量 E2EE）

### 4.1 条目信封（每个数据项一条）

```
item_key : string   // 例 "config/default.custom.yaml" / "phrases/custom_phrase.txt"
type     : config | phrases | userdb
hlc      : { ms: uint64, counter: uint16, device: string }   // 混合逻辑时钟
nonce    : 12 B     // AES-GCM 随机 nonce
cipher   : base64   // AES-256-GCM(master=enc_key, nonce, aad=item_key||type||hlc)
```

- **AAD 绑定 `item_key|type|hlc`** ⇒ 条目被换位 / 改版本会被解密端拒绝（防降级与串位）。
- 载荷内容：
  - `config` = 文件明文（字节流）。
  - `phrases` / `userdb` = **条目化 TSV**，每行携带自己的 `hlc`：
    `key \t payload \t hlc_ms \t hlc_counter \t device`（分隔符与字段数由 `tasks.md` T201 的取证结论定）。

### 4.2 版本与定序：混合逻辑时钟（HLC）

多设备时钟不可信 ⇒ 不用裸时间戳做 LWW。每条目版本 = `(ms, counter, device)`：
- 本机任何写入：`ms = max(now, last_ms)`；`ms == last_ms` 时 `counter++`。
- 收到远端版本：`last_ms = max(last_ms, remote.ms)`；本机再写时 `counter` 继续自增。
- 全序比较：`ms` 大者胜 → `counter` 大者胜 → `device` 字典序大者胜（**保证两台设备算出同一顺序**）。

### 4.3 合并（用户拍板：按条目合并）

合并发生在**客户端**（服务端只见密文，无法按条目合并）：

| 类型 | 合并规则 |
|---|---|
| `config` | 整份取 HLC 较大者（不拆文件） |
| `phrases` / `userdb` | **按条目键合并**：同键取 HLC 较大者；不同键取并集；结果重新加密并**回传**（新 HLC），使两端收敛 |

- 词库条目的「键」与「词频如何合并」**不得凭推测**：先做取证（`tasks.md` T102）坐实导出 / 导入语义后再定实现（风险 R1）。
- 收敛性判据：任何一轮 push/pull 之后，两端条目集与取值必相同（AC-08 / AC-09）。

### 4.4 邮箱验证码（**2026-11-11 用户拍板：$0**）

首版**必须**验证邮箱归属（否则任何人都能抢注他人邮箱）。

**决策：走 $0 路线，不采购 Workers Paid。**

- 开发 / 内测：`MAIL_MODE="outbox"` —— 验证码只写 `mail_outbox` 表、**不发信**（服务端已实现，`src/mail.ts`）。
- 上线前：切到**第三方 HTTPS 邮件 API**（Workers Free 允许出站 `fetch`）—— 这**打破** T101「同厂商、不接第三方」的自定边界，代价 = 多一个第三方账号 + 一把 API key 的托管面；该第三方服务的免费额度与条款**尚未核价**（见 `docs/research/fluxing-cloud-2026-11/free-tier-verdict.md` 未取证清单）。
- **不选** Cloudflare Email Service：官方原文「Arbitrary-recipient sending requires Workers Paid」⇒ **账号级最低消费 $5/月**，与「$0」目标直接冲突。

**协议面不变**：服务端只**收**验证码结果、不参与发信 ⇒ 换发信通道**不改客户端协议**（T304 消费的 login 响应 `email_verified` 语义不变）。

未验证前的兜底不变：注册流程置 `pending`，**未验证账号不得写入同步数据**（服务端对 `push` 返回 403 `email_not_verified`）。

## 5. 客户端模块与线程模型

| 模块 | 落点 | 职责 |
|---|---|---|
| `include/FluxingCloudEnvelope.h` | 头文件、纯逻辑、`inline` | 信封编解码、base64、HLC 比较与自增、条目合并算法、版本序列化 —— **生产与单测同源**（宪法 R7） |
| `WeaselServer/Cloud/CloudJson.{h,cpp}` | 自包含严格 JSON | 请求体拼装（`json::Quote`）与响应解析；不引第三方 JSON 库 |
| `WeaselServer/Cloud/CloudCrypto.{h,cpp}` | **自实现 SHA-256 / HMAC / PBKDF2 / HKDF**（AES-GCM 走信封头的 CNG） | 派生 `auth_key` / `enc_key`；口令缓冲清零（ADR 0054） |
| `WeaselServer/Cloud/CloudTransport.{h,cpp}` | WinHTTP | 抽象成 `ICloudTransport`（可 mock）；超时、重试退避、代理自动探测；证书校验不关 |
| `WeaselServer/Cloud/CloudCredentialStore.{h,cpp}` | Credential Manager | 存 refresh token 与 `enc_key`；读失败 ⇒ 视为未登录 |
| `WeaselServer/Cloud/CloudAccount.{h,cpp}` | HTTP | 注册 / 登录 / 续期 / 登出 / 设备列表 / 清空云端；消费 `email_verified` |
| `WeaselServer/Cloud/CloudSyncService.{h,cpp}` | 纯逻辑 + 编排 | push / pull 信封封装（5 字段 AAD）、条目合并（复用 `MergeEntries`）、本地 HLC sidecar |
| `WeaselServer/Cloud/CloudSyncEngine.{h,cpp}` | 后台线程（**阶段 F**） | 调度（启动拉、变更延迟推、定时拉、立即同步）—— **阶段 D 未落地** |
| `WeaselServer/Cloud/CloudUserDictIo.{h,cpp}` | librime | 词库快照解析 / 生成 + 与引擎的互斥协调（见 §5.2） |
| `WeaselServer/Cloud/CloudAccountDialog.{h,cpp}` | **普通 Win32 modeless 窗口**（与 `AboutDialog` 同栈，不引 WTL） | 账号页：登录 / 登出 / 设备列表 / 清空云端 / 状态栏 |

### 5.1 线程与生命周期

- **不新增常驻进程**（NFR-001）：全部挂在既有 `WeaselServer.exe` 内。
- **不在输入线程做 I/O**（宪法 P2）：同步引擎跑在自己的工作线程；TSF 侧**完全不知道同步存在** ⇒ **不新增任何 IPC 消息**（见 §5.3）。
- 空闲开销：工作线程用事件等待 + 定时器唤醒（不使用轮询）⇒ NFR-002 的空闲 CPU 基线不被破坏。

### 5.2 词库读写的互斥（**2026-11-11 修订**）

用户词库是 leveldb，由常驻进程持有锁（CONTEXT.md §4 不变量 13）：
- **导出 / 导入原语用 T102 的取证结论**：`backup_user_dict`（导出快照）+ `restore_user_dict`（还原）。**不用** `export_user_dict` / `import_user_dict`（T102 已证伪：会丢词频、会覆盖同名词条）。
- **互斥经既有维护态机制**：`RimeWithWeaselHandler` 的 `StartMaintenance` / `EndMaintenance`，由 `CloudMaintenanceHost.h` 在 `WeaselServerApp` 构造期注入钩子；**不得**绕过后台直接调 `rime_api->finalize()` / `initialize()`。
- 导出 / 导入**只在后台线程**执行，绝不在输入 / TSF 回调里做（宪法 P2）。
- **已知局限**：快照通道**不传播墓碑** ⇒ **跨设备删除不保证生效**（阶段 G 加固项，已在 `tasks.md` 记为偏差）。

### 5.3 IPC 影响评估

| 项 | 结论 |
|---|---|
| 新增 IPC 消息类型 | **无**。账号页与同步引擎同在 `WeaselServer` 进程内，直接函数调用。 |
| 是否触发 IPC 四边同步 + ADR + IPC 测试 | **不触发**（该成本仅在协议变更时发生）。 |
| 触发条件 | 若后续要在 `WeaselDeployer` 里加账号入口 → 必须走 IPC ⇒ **那时**才需要四边同步 + 新 IPC 测试 + ADR。首版明确不做（`tasks.md` T402 记为 out of scope）。 |

## 6. 服务端设计（Cloudflare）

### 6.1 接口契约

| 方法 | 路径 | 请求 | 响应 |
|---|---|---|---|
| POST | `/v1/auth/register` | `{email, auth_key, device}` | `{account_id, salt, kdf{algo,iters}, need_email_verify}` |
| POST | `/v1/auth/verify` | `{email, code}` | `{ok}` |
| POST | `/v1/auth/login` | `{email, auth_key, device}` | `{access_token, refresh_token, expires_in, salt, kdf, account_id}` |
| POST | `/v1/auth/refresh` | `{refresh_token}` | `{access_token, refresh_token, expires_in}` |
| POST | `/v1/sync/push` | `{items:[{item_key,type,hlc,nonce,cipher}]}` | `{accepted:[item_key], server_seq}` |
| GET | `/v1/sync/pull?since=<seq>&cursor=` | — | `{items:[...], next_cursor, has_more}` |
| GET | `/v1/devices` | — | `[{device_id, name, last_seen, created}]` |
| DELETE | `/v1/devices/:id` | — | `{ok}`（该设备续期失效） |
| DELETE | `/v1/account/data` | — | `{ok}`（清空 R2 对象 + D1 索引） |

> **线上补充（阶段 C 实测，2026-11-11 回写）**：
> ① `POST /v1/auth/login` 的响应**额外**含 `email_verified`（布尔）—— 对上表是 **additive** 扩展，客户端（T304）必须消费它来区分「需验证邮箱」与「口令错」；
> ② `GET /v1/health` 是**第 10 条运维路由**（不是契约端点），原表里的 `/v1/ping` **不存在**；
> ③ `DELETE /v1/devices/:id` 的 `:id` 同时接受 `devices.id` 与 `devices.device_id` —— 客户端传 **`device_id`**（`devices.id` 是库内自增，换库会变）。

### 6.2 D1 表（**2026-11-11 按实现回写**）

实做 **7 表 4 索引**（真相源 `cloud/fluxing-sync/migrations/0001_init.sql`）：

```sql
users(id, email UNIQUE, verifier, salt, kdf_algo, kdf_iters, email_verified, created_at)
devices(id, user_id, name, device_id, last_seen, revoked_at, created_at)   -- UNIQUE(user_id, device_id)
refresh_tokens(id, user_id, device_id, token_hash, expires_at, revoked_at, kind)  -- kind = access|refresh
sync_items(id, user_id, item_key, type, hlc_ms, hlc_counter, hlc_device,
           r2_key, cipher_len, nonce_b64, updated_at)   -- 索引，不含内容
email_verifications(id, user_id, code_hash, expires_at, consumed_at, created_at)
mail_outbox(id, to_addr, subject, body, created_at)   -- 仅 MAIL_MODE=outbox 时写
```

**对原式的 6 条偏差（D1–D6）**

| 编号 | 偏差 | 理由 |
|---|---|---|
| **D1** | `refresh_tokens` 加 `kind` 列（`access` / `refresh`） | access token 也走「不透明随机串 + 只存哈希」⇒ 换「无签名密钥 / 无 JWT 库」+ **撤销设备立即失效** |
| **D2** | 新增 `email_verifications` 表 | 首版必须邮箱验证码；`users` 里只留 `email_verified` 标志 |
| **D3** | `sync_items.id` AUTOINCREMENT 充当 `server_seq` | 免自建序列表；`server_seq` 取本次推送涉及 item_key 集合上的 `MAX(id)`，**明确弃用 `meta.last_row_id`**（并发下会串号） |
| **D4** | `sync_items` 加 `nonce_b64` 列 | GCM nonce 必须随密文一起回来，否则客户端解不开 |
| **D5** | 新增 `mail_outbox` 表，**仅 `MAIL_MODE=outbox` 时写** | Workers Free 档不能出站；outbox 让「验证码链路」在无 Paid 计划时可取证 |
| **D6** | `devices` 加 `UNIQUE(user_id, device_id)` | 同一设备重复注册应更新而非插新行 |

**服务端从不用 salt 做校验**（只比 `HMAC(pepper, auth_key)`）—— 这是客户端 salt 策略（§3.1 / ADR 0054）的直接前提。

### 6.3 R2 布局

- 桶：`fluxing-sync`（单一桶 + 路径隔离；如规模上来再按账号分桶）。
- 对象键：`u/<account_id>/<sha256(item_key)>`；内容 = 密文；元数据（`hlc` 等）放对象自定义元数据 + D1 双写。
- 生命周期：删除账号 ⇒ 批量删对象（任务清单化，带重试）。

## 7. 同步调度与降级

| 触发 | 行为 |
|---|---|
| 启动 | 拉一次增量（不阻塞启动流程，失败静默） |
| 本地变更 | **防抖 5 分钟**后推一次（合并多次改动），且全局推频率上限 **15 分钟一次/设备** |
| 定时 | 每 60 分钟拉一次增量 |
| 手动 | 「立即同步」按钮，立刻推 + 拉 |
| 失败 | 指数退避（1 / 5 / 15 / 60 分钟），最多 4 次后转「等待下一次定时」，**不弹窗、不阻塞输入** |
| 断网 | 同上；不排队累积，恢复后按防抖窗口补推 |

**配额纪律**（免费额度）：推请求只带**变化条目**；一次推合并多条；每用户每日推次数上限 **100**（沿用 spec 010 的口径）；每日拉次数上限 50。

## 8. 界面与文案（用户可见，一律简体中文）

- **入口**：快捷设置栏第 4 槽（现「关于 / 账号说明」）升级为**账号页**，并保留「关于」分区。
- **账号页分区**：登录状态 / 账号邮箱 / 设备列表 / 同步开关与最近同步时间 / 立即同步 / 登出 / 清空云端数据。
- **必须修改的既有文案**（P5 ⇒ CHANGELOG 条目）：
  - `WeaselServer/AboutDialog.cpp` 现文案「本输入法不含云端账号。词库、配置与用户数据全部保存在本机，不上传、不同步。」**必须改写**，否则与新功能自相矛盾。
  - 需新增免责说明：「口令遗忘后云端数据无法找回」「同步内容在离开本机前已加密」。

## 9. 变更面清单（**2026-11-11 按实做更新**）

**已新增**：`include/FluxingCloudEnvelope.h`（阶段 B）· `WeaselServer/Cloud/*`（阶段 D **18 个文件** = 8 个模块的 `.h`/`.cpp` 共 16 个 —— `CloudJson` / `CloudCrypto` / `CloudTransport` / `CloudCredentialStore` / `CloudAccount` / `CloudSyncService` / `CloudUserDictIo` / `CloudAccountDialog` —— 外加 `CloudRimeUserDictBackend.cpp` + `CloudMaintenanceHost.h`）· `test/TestFluxingCloud/` · `cloud/fluxing-sync/*`（阶段 C）· `docs/adr/0039-*.md` · `docs/adr/0054-*.md`。

**已修改**：`weasel.sln` 与 `xmake.lua`（新增测试工程与系统库链接）· `WeaselServer/xmake.lua`（**`add_files("./Cloud/*.cpp")` + `add_links` 补 `bcrypt` / `winhttp` / `advapi32` / `crypt32`**）· `WeaselServer/WeaselServer.vcxproj`（**逐文件 `PrecompiledHeader=NotUsing`**（`stdafx.h` **无** `NOMINMAX`）+ 4 个系统库 + 全部 ItemDefinitionGroup 补 `/utf-8`）· `WeaselServer/WeaselServerApp.cpp`（注册云维护态宿主）· `test/TestFluxingCloud/TestFluxingCloud.vcxproj`（编入 7 个生产实现）· `CHANGELOG.md`。

**待改**：`WeaselServer/AboutDialog.cpp`（文案，阶段 E T403）· 快捷设置栏右键菜单接线（阶段 E T405）· `cloud/fluxing-sync/wrangler.toml`（D1 / R2 绑定 ID、域名路由、pepper secret —— 依赖外部资源）。

**不动**：`include/WeaselIPCData.h`（无 IPC 变更）· `librime/` · `.clang-format` · `output/install.nsi`（首版不新增文件到安装目录 ⇒ 安装包无需改；若后续加独立 exe 则必须改并跑 G-a/G-b/G-c 三道门禁）。

## 10. 风险与对策

| # | 风险 | 影响 | 对策 |
|---|---|---|---|
| **R1** | 词库导出 / 导入的合并语义未坐实（合并行为、是否覆盖、锁冲突） | 高：做错会丢用户词库 | **先取证后实现**：T102 产出可复跑证据与结论，结论写进 ADR/plan 才允许写合并代码；实现必须有判别性单测 |
| **R2** | 600k 次 PBKDF2 在老 CPU 上耗时可观 | 中：登录卡顿 | 放后台线程 + 进度提示；必要时按设备能力自适应迭代次数（但**不得低于**存储的 KDF 参数） |
| **R3** | 服务端被攻陷后仍可冒充用户 | 中 | 明确威胁模型（内容机密 ≠ 账号隔离）；可选「同步口令」把内容密钥与账号解耦；设备列表可一键撤销 |
| **R4** | `WeaselServer.exe` 是 PPL ⇒ 新二进制要注销 / 重启才加载 | 中：验收节奏 | 验收流程前置「注销或重启」这一步；写进 `tasks.md` 的验收前置条件 |
| **R5** | 云端域名在境内的可达性与延迟 | 高：用户侧「同步一直失败」 | 用**自有域名**而非平台默认域名；首版保留「自定义端点」配置口子（即使 UI 不暴露）；失败一律降级不阻断；连通性进 T103 取证 |
| **R6** | 免费额度：Workers 10 万请求/天、R2 100 万次 Class A 写/月 | 中：用户涨上去就撞墙 | §7 的配额纪律 + 单用户推次数上限；官方端点按用量升级付费档（$5/月起含 1000 万请求/月）；成本估算写进运维文档 |
| **R7** | 端到端加密 + 口令遗忘 = 数据永久丢失 | 高：用户投诉 | UI 明示 + 提供「重置账号并清空云端」的出路；首版不做改密（避免全量重加密） |
| **R8** | Windows 代理 / 企业网环境导致 WinHTTP 失败 | 中 | 自动代理探测 + 超时 + 退避 + 明确中文错误；失败不影响本机使用 |
| **R9** | 引入网络能力后触碰「零出向请求」红线 | 高：合规 | 未登录 ⇒ 引擎不启动、账号页不发任何请求；补一条门禁断言（未登录时出向连接数为 0） |

## 11. Constitution Check

| 法则 | 状态 | 说明 |
|---|---|---|
| **I. 意图先行** | ✅ | `spec.md` §1–§2 写明「用户能得到什么」；每条 FR 都能映射到 US |
| **II. 测试支撑** | ✅ | `tasks.md` T201/T202/T203 给出判别性单测；合并算法、编解码、定序全部纯逻辑可单测 |
| **III. 规格纪律** | ✅ | spec / plan / tasks 三件套齐全（≥3 文件、跨会话 ⇒ 全套） |
| **IV. 结构化澄清** | ✅ | 四项前置决策由用户 2026-11-07 拍板；`spec.md` §8 保留 5 条待核实并给了取证任务 |
| **V. 增量交付** | ✅ | 阶段切片：取证 → 服务端 → 客户端底座 → 账号 UI → 同步引擎 → 双机验收，每片独立可验收 |
| **R2（spec 无技术栈词）** | ✅ | `spec.md` 不出现组件名；技术名只在 `plan.md` / `tasks.md` |
| **R3（任务优先级 + US 标签）** | ✅ | `tasks.md` 每条带 `[P1]/[P2]/[P3]` 与 `[US#]` |
| **R4（宪法检查表）** | ✅ | 本节 |
| **R6（完成要有证据）** | ✅ | `tasks.md` 每阶段写明「Done 判据 = 贴测试输出 / 可复跑证据路径」 |
| **R7（单一事实源）** | ✅ | 纯逻辑下沉 `include/FluxingCloudEnvelope.h`，生产与单测同源；`quickpanel.state` 等既有真相源不动；010 的技术结论明确作废 |
| **P2（不阻塞 TSF）** | ✅ | 同步全部在 `WeaselServer` 后台线程；TSF 侧零感知、零 IPC |
| **P3（不新增第三方依赖）** | ✅ | WinHTTP / CNG / Credential Manager 全是操作系统组件；服务端依赖只在 `cloud/` 子目录，不进 C++ 链接 |
| **P4（提交规范）** | ✅ | 提交用 `feat(WeaselServer)` / `feat(fluxing)` / `docs(...)` 单 scope |
| **P5（用户可见改动入 CHANGELOG）** | ✅ | 账号页上线、`AboutDialog` 文案改写都进 `CHANGELOG.md`「主要更新」 |
| **P6 / P8（路径与品牌）** | ✅ | 用户数据仍在既有用户数据目录；不新增迁移；品牌名沿用 Fluxing |
| **P7（常驻进程唯一）** | ✅ | NFR-001：不新增常驻进程 |

**复杂度追踪**：本变更触碰面大（新增 7 组客户端文件 + 完整服务端 + 新测试工程），
但被拆成 6 个可独立验收的阶段，每阶段 ≤3 个文件的改动面（R5 的粒度要求在任务级满足）。

## 12. 备选方案与否决理由

| 方案 | 否决理由 |
|---|---|
| 沿用 spec 010 的 Vercel + Postgres + libcurl | libcurl **不在仓库里**（须新增第三方依赖，违反 P3 / NFR-004）；Vercel 与用户指定的 Cloudflare 不符 |
| 沿用 010 的「服务端见明文」 | 用户拍板全量端到端加密 |
| 客户端引入 libcurl / OpenSSL | 违反 P3、NFR-004；且 WinHTTP 在 Windows 上的代理 / TLS 行为更可预期 |
| 客户端实现 Argon2id / 自己写密码学 | 「自己造轮子」的风险高于收益；系统自带能力足够 |
| 自建 PAKE（OPAQUE / SRP） | 实现复杂度与安全风险都远超收益；已用「可选独立同步口令」补偿 |
| 服务端做按条目合并 | 服务端只见密文 ⇒ 物理上做不到；合并必须落在客户端（§4.3） |
| 新增独立同步进程 | 违反 NFR-001（不新增常驻进程）与 spec 098 的性能基线 |
| 复用 Workers KV 存同步内容 | 免费额度写只有 1000 次/天 ⇒ 同步热路径必被打穿 |

## 13. 验证策略

| 层 | 手段 | 判据 |
|---|---|---|
| 纯逻辑 | `test/TestFluxingCloud`（新工程，x86 Release） | HLC 全序、条目合并、编解码、冲突收敛；**必须含反例断言**（乱序 / 同版本 / 空 / 超长输入） |
| 服务端 | **vitest-pool-workers**（真 workerd 内跑）+ 本地 D1/R2 集成测试（`cloud/fluxing-sync/test/`）—— **不用 `wrangler dev`**（交互式进程 + 端口 + 手工 curl，产物不可复跑） | 注册 / 登录 / 续期 / 推 / 拉 / 设备撤销 / 清空；实测 **20/20 用例 PASS** + 变异验证 |
| 端到端 | 两台设备（或两套用户数据目录）跑真实 Worker | AC-02 ~ AC-05、AC-08 ~ AC-09、AC-13 |
| 性能 | ADR 0030 三数字命令 + 输入延迟对照 | NFR-001 ~ NFR-003 |
| 合规 | 未登录态抓包计数 = 0；服务端存储抽查为密文 | AC-07、AC-12、R9 |
| 门禁 | `scripts/gate/run-all.ps1 -Level L0`；改动 ≥3 文件加 L4；发版前 L5 | 全绿 |
| 真机 | 装包后跑账号页全流程 | 用户在场 |

## 14. 与其他 spec 的边界

- **spec 098（云候选）**：同属「云」但方向相反 —— 098 是**打字当下**借用别人的词（毫秒级、输入热路径旁路）；100 是**同步用户自己的资产**（分钟级、可后台、可等待）。
  二者**共用**同一套网络边界口径（默认关 + 端点白名单 + 无埋点）与同一套后端账号体系，但 100 **不得**把 098 拖进输入热路径，反之亦然。
- **spec 099（快捷面板）**：账号页挂在快捷面板第 4 槽，但**不改**面板几何与状态真相源。
