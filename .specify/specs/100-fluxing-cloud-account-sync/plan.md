# 100 · Plan · 云端账号与多端同步（Cloudflare 后端 + 端到端加密）

> 配套 `spec.md`（用户视角）与 `tasks.md`（执行序）。技术选型、协议、算法、变更面、风险全在本文。
> **取代** spec `010-cloud-sync` 的技术结论（后端选型 / 客户端依赖库 / 认证方案三项均已变）。

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

### 3.1 密钥派生（客户端本地，PBKDF2-HMAC-SHA256 via CNG）

```
salt        = SHA256("fluxing-enc-v1|" + lower(email))        // 定值、可由客户端自算
master      = PBKDF2-HMAC-SHA256(password, salt, iters=600000) // CNG: BCryptDeriveKeyPBKDF2
auth_key    = HKDF-SHA256(master, info="fluxing-auth-v1")     // 登录凭证
enc_key     = HKDF-SHA256(master, info="fluxing-enc-v1")      // 同步加密密钥
```

- `iters=600000` 依据 OWASP 对 PBKDF2-HMAC-SHA256 的现行建议；**必须在后台线程跑**（老 CPU 上 600k 次约 0.3–1.5 s，绝不能放在 UI 线程）。
- **为什么不用 Argon2id**：仓库无第三方依赖政策下不引入密码学库，自己实现 Argon2 属于「自己造轮子」，风险高于收益。代价是抗 GPU 弱于 Argon2id —— 用迭代次数 + 口令强度要求 + 可选独立「同步口令」补偿（见 §3.4）。
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

### 4.4 邮箱验证码

首版**必须**验证邮箱归属（否则任何人都能抢注他人邮箱）。验证码通道二选一，实施前定：
- Cloudflare 侧的事务性邮件能力（视其可用性与配额）；
- 外部事务性邮件服务（按量计费）。
未验证前的兜底：注册流程置 `pending`，**未验证账号不得写入同步数据**（只允许登录后待验证）。

## 5. 客户端模块与线程模型

| 模块 | 落点 | 职责 |
|---|---|---|
| `include/FluxingCloudEnvelope.h` | 头文件、纯逻辑、`inline` | 信封编解码、base64、HLC 比较与自增、条目合并算法、版本序列化 —— **生产与单测同源**（宪法 R7） |
| `WeaselServer/Cloud/CloudCrypto.{h,cpp}` | CNG | PBKDF2 / HKDF / AES-GCM；口令缓冲清零 |
| `WeaselServer/Cloud/CloudTransport.{h,cpp}` | WinHTTP | 抽象成 `ICloudTransport`（可 mock）；超时、重试退避、代理自动探测 |
| `WeaselServer/Cloud/CloudCredentialStore.{h,cpp}` | Credential Manager | 存 refresh token 与 `enc_key` |
| `WeaselServer/Cloud/CloudAccount.{h,cpp}` | HTTP | 注册 / 登录 / 续期 / 登出 / 设备列表 / 清空云端 |
| `WeaselServer/Cloud/CloudSyncEngine.{h,cpp}` | 后台线程 | 调度（启动拉、变更延迟推、定时拉、立即同步）、push / pull、合并、回传 |
| `WeaselServer/Cloud/CloudUserDictIo.{h,cpp}` | librime | 词库导出 / 导入 + 与引擎的互斥协调（见 §5.2） |
| `WeaselServer/Cloud/CloudAccountDialog.{h,cpp}` | WTL | 账号页：注册 / 登录 / 登出 / 设备列表 / 同步状态 / 清空云端 |

### 5.1 线程与生命周期

- **不新增常驻进程**（NFR-001）：全部挂在既有 `WeaselServer.exe` 内。
- **不在输入线程做 I/O**（宪法 P2）：同步引擎跑在自己的工作线程；TSF 侧**完全不知道同步存在** ⇒ **不新增任何 IPC 消息**（见 §5.3）。
- 空闲开销：工作线程用事件等待 + 定时器唤醒（不使用轮询）⇒ NFR-002 的空闲 CPU 基线不被破坏。

### 5.2 词库读写的互斥

用户词库是 leveldb，由常驻进程持有锁（CONTEXT.md §4 不变量 13）：
- 同步引擎**在本进程内**调用导出 / 导入接口（不走 `StartMaintenance` 那一套 —— 那是给**进程外**工具用的）。
- 但必须与引擎自身的读写**互斥**：`CloudUserDictIo` 在导出 / 导入期间挂起引擎对该用户库的访问（复用既有维护态机制），**且必须在 TSF 无关的后台线程完成**，不得在输入回调里做。
- 实施前必须实测（`tasks.md` T102）：导出格式、导入是否会覆盖同名词条、导入期间引擎的行为。

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

### 6.2 D1 表

```sql
users(id, email UNIQUE, verifier, salt, kdf_algo, kdf_iters, email_verified, created_at)
devices(id, user_id, name, device_id, last_seen, revoked_at, created_at)
refresh_tokens(id, user_id, device_id, token_hash, expires_at, revoked_at)
sync_items(id, user_id, item_key, type, hlc_ms, hlc_counter, hlc_device,
           r2_key, cipher_len, updated_at)   -- 索引，不含内容
```

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

## 9. 变更面清单（预估，实施时以实际为准）

**新增**：`include/FluxingCloudEnvelope.h`、`WeaselServer/Cloud/*`（7 组文件）、`test/TestFluxingCloud/`、`cloud/fluxing-sync/*`、`docs/adr/0039-*.md`。

**修改**：`weasel.sln` 与 `xmake.lua`（新增测试工程与 winhttp 链接）、`WeaselServer/WeaselServer.vcxproj`（链接 `winhttp.lib` / `advapi32.lib` / `bcrypt.lib`）、`WeaselServer/AboutDialog.cpp`（文案）、`WeaselServer/WeaselServerApp.cpp`（账号页接线）、`CHANGELOG.md`。

**不动**：`include/WeaselIPCData.h`（无 IPC 变更）、`librime/`、`.clang-format`、`output/install.nsi`（首版不新增文件到安装目录 ⇒ 安装包无需改；若后续加独立 exe 则必须改并跑 G-a/G-b/G-c 三道门禁）。

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
| 服务端 | `wrangler dev` + 本地 D1/R2 集成测试（`cloud/fluxing-sync/test/`） | 注册 / 登录 / 续期 / 推 / 拉 / 设备撤销 / 清空 |
| 端到端 | 两台设备（或两套用户数据目录）跑真实 Worker | AC-02 ~ AC-05、AC-08 ~ AC-09、AC-13 |
| 性能 | ADR 0030 三数字命令 + 输入延迟对照 | NFR-001 ~ NFR-003 |
| 合规 | 未登录态抓包计数 = 0；服务端存储抽查为密文 | AC-07、AC-12、R9 |
| 门禁 | `scripts/gate/run-all.ps1 -Level L0`；改动 ≥3 文件加 L4；发版前 L5 | 全绿 |
| 真机 | 装包后跑账号页全流程 | 用户在场 |

## 14. 与其他 spec 的边界

- **spec 098（云候选）**：同属「云」但方向相反 —— 098 是**打字当下**借用别人的词（毫秒级、输入热路径旁路）；100 是**同步用户自己的资产**（分钟级、可后台、可等待）。
  二者**共用**同一套网络边界口径（默认关 + 端点白名单 + 无埋点）与同一套后端账号体系，但 100 **不得**把 098 拖进输入热路径，反之亦然。
- **spec 099（快捷面板）**：账号页挂在快捷面板第 4 槽，但**不改**面板几何与状态真相源。
