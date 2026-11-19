# ADR 0064 — 服务端「忘记密码 / 重置口令」端点：两段式 + 独立重置码表

- **Status**: Proposed（批次 AT-2 客户端与服务端均已落码，**线上部署已完成且线上判别全绿**；真机验收与「重置后云端数据」产品决策未完成 ⇒ 待用户验收后转 Accepted）
- **Date**: 2026-11-19
- **Spec**: [100-fluxing-cloud-account-sync](../../.specify/specs/100-fluxing-cloud-account-sync/spec.md)（批次 AT · AT-2）
- **Revises**: —（新增；不改 ADR 0062 的「机会性同步」口径与文案，也不推翻 ADR 0039 的「首版不做找回密码」——那一条正是本 ADR 要推翻的旧结论）
- **Lessons**: `.specify/memory/lessons-learned.md` L222–L225
- **Supersedes / superseded by**: —（新增）

---

## 背景

ADR 0063 落了账号页的信息架构（两套互斥视图 + 注册窗 + 找回口令窗）。其中「找回口令」入口
在 AT-1 阶段是**临时耦合**：发送验证码复用了注册用的 `POST /v1/auth/resend`，重置口令打的是
占位路径 `POST /v1/auth/password-reset`（服务端当时**不存在**该端点 ⇒ 必然 404）。

本 ADR 记录 AT-2 把服务端补齐后的**契约与安全边界**。

### 事实 1 — 服务端此前完全没有「忘记密码」

`cloud/fluxing-sync/src/index.ts` 的 auth 路由只有 register / verify / resend / login / refresh；
全仓检索「password-reset」在 `cloud/**` 零命中。历史决策（spec 100 `spec.md:53`、`tasks.md:373`、
ADR 0039、handoff D11）明确「首版不做找回密码」，理由是「改密牵动全量重加密」。

### 事实 2 — 服务端只收派生值，永不见明文口令

`src/auth.ts`：`register`（:257-259）与 `login`（:512-514）都要求请求体带 `auth_key`，
`requireAuthKey`（:92-99）校验它是字符串、非空、不过长、无控制字符。
落库口径是 `users.verifier = base64(HMAC-SHA256(AUTH_PEPPER, auth_key))` —— **服务端不参与 KDF**，
口令的 PBKDF2/HKDF 派生全在客户端完成（ADR 0054）。

⇒ **重置口令不能走「把新口令发给服务端」的路子**，必须是同样的形态：客户端用新口令派生新
`auth_key`，服务端只把它当不透明串做 HMAC 落库。

### 事实 3 — 既有验证码表不能直接复用

`email_verifications` 表**没有 purpose 列**，且 `codeHash` 只覆盖码本身、不含用途；
`handleVerify`（:291）的副作用固定是 `SET email_verified=1`，**不可复用**于重置场景。

⇒ 直接复用会让「注册码可以当重置码用」，且注册/重置两条流程的副作用会互相污染。

### 事实 4 — 重置口令会改变 `enc_key`，旧云端密文解不开

客户端派生链（ADR 0054）：`salt = SHA256("fluxing-enc-v1|" + lower(email))`，
`master = PBKDF2-SHA256(pw, salt, 600000, 32)`，`auth_key` / `enc_key` = `HKDF-SHA256(master, info, 32)`。
盐只由邮箱决定 ⇒ **改口令后 `master` 变 ⇒ `auth_key` 与 `enc_key` 同时变**。
R2 上以旧 `enc_key` 加密的密文**永远解不开**（服务端永不见 `enc_key`，无法代解）。

---

## 决策

### 决策 1 — 两段式端点，与注册流程同形

| 端点 | 请求体 | 响应体 |
| --- | --- | --- |
| `POST /v1/auth/password-reset/start` | `{ email }` | `{ ok: true, email }` |
| `POST /v1/auth/password-reset/confirm` | `{ email, code, auth_key }` | `{ ok: true, email }` |

- 两个端点**都不需要鉴权**（用户此时没有有效令牌）。
- 响应体**不回显验证码、不回显 `auth_key` 派生值、不含任何口令相关字段**。
- 客户端侧对应 `CloudAccountService::StartPasswordReset` 与 `CloudAccountService::ResetPassword`。

### 决策 2 — 重置码落**独立表** `password_resets`，而不是给 `email_verifications` 加 purpose 列

- 新增 `migrations/0002_password_resets.sql`（**不改 `0001_init.sql`** —— 线上 D1
  `e4ea114a-29b4-4abd-87e6-4e6cb20d86e4` 已经应用过 0001，改历史 migration 不会重跑）。
- 隔离性做成**结构性的**：注册码与重置码在存储层就是两张表的不同行，注册流程**根本读不到**
  重置码，反之亦然 —— 不依赖任何 WHERE 条件写对。
- 副作用面为零：注册流程的表、查询、`handleVerify` 语义**一行未动**。

### 决策 3 — 重置码的 `codeHash` 含用途与创建时刻

```
codeHash = base64(HMAC-SHA256(AUTH_PEPPER, "password-reset-v1|" + email + "|" + code + "|" + createdAt))
```

- 含 `createdAt` ⇒ 同一邮箱 60 秒内重发会得到**不同**的 `codeHash` ⇒ 旧码即便同秒也不能复用。
- 含用途前缀 `password-reset-v1` ⇒ 与注册码的哈希域**天然不重叠**。

### 决策 4 — 限流与时效沿用注册口径

`CODE_TTL_SECONDS=600` / `RESEND_INTERVAL_SECONDS=60` / `MAX_VERIFY_ATTEMPTS=5`
（`src/auth.ts:29-31`），与注册验证码**同一套阈值**，不为重置另开一套。

### 决策 5 — confirm 成功改 `users.verifier`、**吊销该账号全部令牌**、**不签发令牌**

`UPDATE users SET verifier = base64(HMAC-SHA256(AUTH_PEPPER, auth_key)) WHERE email = ?`
（`auth_key` 是客户端送来的**新口令派生值**）。

- 与既有 `handleVerify` 语义一致：验证通过**不**自动登录 ⇒ 重置通过后用户需要用**新口令**再登录一次。
- 服务端不签发令牌 ⇒ 不用在重置链路里引入新的令牌语义，也避免「改密瞬间自动拿到新会话」的边界问题。
- **吊销该账号全部令牌**（其它设备上的会话一并失效）：改密是「怀疑口令泄露」的典型场景，
  旧会话继续有效等于没改。代价是其它设备要重新用新口令登录 —— 这是安全侧的取舍。

### 决策 6 — 服务端红线保持不变

- 服务端**永不见明文口令、永不见 `enc_key`、不参与任何 KDF**。
- `AUTH_PEPPER` 缺失 / 过短 ⇒ fail closed（500），绝不回退到内置默认值。
- 响应体不含任何可用于离线爆破的派生值回显。

---

## 落码状态（2026-11-19 · AT-2）

| 端点 | 请求体 | 响应体 |
| --- | --- | --- |
| `POST /v1/auth/password-reset/start` | `{ email }` | `{ ok: true, email }` |
| `POST /v1/auth/password-reset/confirm` | `{ email, code, auth_key }` | `{ ok: true, email }` |

- 新错误码：`reset_too_frequent`(429) / `reset_no_code` / `reset_code_used` /
  `reset_code_expired` / `reset_code_locked` / `reset_code_mismatch` / `reset_no_account`（均 400）。
- 服务端改动：`migrations/0002_password_resets.sql`（新增，独立表，**未改 0001**）、
  `src/auth.ts`（+152）、`src/mail.ts`（+102/−38）、`src/index.ts`（+16）、
  `src/errors.ts`（**未改**，错误码按既有风格写在调用点）、
  `test/password-reset.test.ts`（新增 14 例）、`test/mail-http.test.ts`（+1 例）。
- 客户端改动：`CloudAccount.{h,cpp}`（`StartPasswordReset` 新增、`ResetPassword` 端点改
  `/confirm`）、`CloudAccountDialog.{h,cpp}`（`SubmitStartPasswordReset` 新增、找回口令窗
  「发送验证码」改走 reset 端点）。
- 门禁（**独立复跑**，非子会话自报）：服务端 typecheck 0 / vitest **57 passed**（基线 42 ⇒ +15）/
  `check:endpoint` 通过；客户端 `build.bat weasel` **0 警告 0 错误** / 云单测 **PASS=492 FAIL=0**。
- **已部署（2026-11-19）**：D1 `0002` 迁移已应用（线上表 `password_resets` 实测存在）；
  Worker Version ID `40b2f9d9-4655-497b-842b-266e1bf27af9`；线上六条判别全绿
  （真实重置邮件已投递到 `25940969@qq.com`）；两个账号 `users.verifier` 前后逐字未变。

---

## 未决（阻塞 AT-2 收尾）

**重置口令后 `enc_key` 变化 ⇒ R2 上的旧密文解不开**（见事实 4）。这需要一个产品级决策，
本 ADR **不预设**，等用户拍板：

| 方案 | 做法 | 代价 |
| --- | --- | --- |
| **A（推荐）** | 重置即清空云端 | 不可逆（用户云端数据删除），需用户明确同意 |
| B | 只换 `auth_key`、保留 `enc_key` | **违反红线**（服务端要存 `enc_key`），不可取 |
| C | 两段式客户端重推 | 需要本地保留明文词条，改动面大 |

客户端侧在决策落地前**只实现到「confirm 成功 → 提示用户用新口令重新登录」**，不擅自清数据。

---

## 已驳回的备选方案

1. **在 `email_verifications` 上加 `purpose` 列** —— 驳回：隔离要靠 WHERE 条件写对，
   而 `handleVerify` 的副作用固定为 `SET email_verified=1`，复用同一张表会引入「注册码当重置码用」
   与副作用串味的风险；独立表的隔离是结构性的。
2. **把新口令直接发给服务端** —— 驳回：违反「服务端永不见明文口令」红线（ADR 0054 / 事实 2）。
3. **重置成功后由服务端代解并重加密 R2 数据** —— 驳回：服务端没有 `enc_key`，物理上做不到。
4. **复用 `POST /v1/auth/resend` 发重置码** —— 驳回：它发的是注册用途的码，
   而 `handleVerify` 的副作用会把 `email_verified` 置 1 ⇒ 拿重置码去注册流程会把账号「验证」掉，
   语义污染。
