# Fluxing 云同步能不能做到「免费」—— 取证结论

> 结论日期：2026-11-11 · 取证方式：`web_fetch` 抓官方文档 `index.md` 原文（页脚 `dateModified: 2026-10-02`）
> 本机 `web_search` 当前不可用（HTTP 402，套餐余额耗尽），因此**所有价格结论只有单源**，
> 未做跨源交叉验证。文中标 `[原文]` 的是官方文档原句，标 `[推断]` 的是我基于原句的推算。

---

## 一句话结论

**分两半：存数据、跑端点 —— 能，$0。发验证码邮件 —— 不能，$5/月起。**

如果你接受把验证码邮件换成第三方免费额度的 HTTPS 邮件 API，那**整体可以压到 $0**，
代价是多一个第三方账号/API key，且违反 T101 自定的「同厂商」边界。

---

## 1. 官方原文（一手摘录）

### 1.1 Workers 计划与最低消费 `[原文]`

> The Workers Paid plan includes Workers, Pages Functions, Workers KV, Hyperdrive, and Durable
> Objects usage for a **minimum charge of $5 USD per month for an account**.

⇒ 关键事实：**$5 是「账号级最低消费」，不是「每百万请求」的价格**。哪怕一个月零流量，账单也是 $5。

另有一句必须注意：

> Workers Paid plan is separate from any other Cloudflare plan (Free, Professional, Business).

⇒ 域名侧就算你在 Free 计划，也不会抵扣这 $5；反过来也一样。

### 1.2 Workers Free 档额度 `[原文]`

| 项 | Free 档 |
|---|---|
| Requests | **100,000 / 天** |
| CPU time | **10 ms / 次调用** |
| Duration | 不收费 |

⇒ `[推断]` 100,000 请求/天 ≈ 300 万/月，是本方案的**主力额度**。
10 ms CPU/次 的限制对「读 D1 → 拼 JSON → 返回」这种路径足够；但如果哪天要在 Worker 里做
大批量加密或全量合并，会撞墙。**本方案把合并放在客户端做（阶段 D），正是为了避开这条。**

### 1.3 D1（数据库）：**Free 档可用** `[原文]`

| 项 | Free | Paid |
|---|---|---|
| 行读取 | **5,000,000 / 天** | 250 亿 / 月 |
| 行写入 | **100,000 / 天** | 5,000 万 / 月 |
| 存储 | **5 GB 总额** | 5 GB 含，超出 $0.75/GB-月 |
| 数据库数量 | **10 个** | 100,000 个含 |

⇒ `[推断]` 「5 亿→5 GB」对账号/设备/同步条目这类元数据是**极其宽裕**的量级。
假设每条同步条目平均 2 KB，5 GB ≈ 250 万条；按「一个用户 1 万条」算能装 250 个用户。

### 1.4 R2（对象存储）：**Free 档可用** `[原文]`

| 项 | Free 档 |
|---|---|
| 存储 | **10 GB-月** |
| Class A（写/列举） | **1,000,000 请求 / 月** |
| Class B（读） | **10,000,000 请求 / 月** |
| 出站流量（egress） | **免费** |

⇒ `[推断]` 词库快照这类大对象放 R2，同步拉取的出站流量不花钱 —— 这是 R2 相对 S3 的**结构性优势**，
也是本方案选 R2 而非 KV 的理由之一。

### 1.5 Workers Logs `[原文]`

Free 档也有：**200,000 事件/天，保留 3 天**。⇒ 内测期排障不需要额外买可观测性。

### 1.6 其它顺带确认可用（Free 档均存在）`[原文]`

- **Queues**：10,000 operations / 天
- **Hyperdrive**：100,000 数据库查询 / 天
- **Durable Objects**：Free 可用（**仅 SQLite 后端**），requests 100,000/天、duration 13,000 GB-s/天

⇒ 本方案**不需要**这三项（阶段 C 用的是 D1 + R2 + Worker 本体），
但「Free 档也有 Durable Objects」意味着**将来要做实时同步/在线状态**时不必升级。

### 1.7 邮件：**这是唯一不能免费的那块** `[原文]`

Cloudflare Email Service 官方页面 `meta description` 原文：

> Send transactional email and route incoming email with Cloudflare Email Service.
> **Arbitrary-recipient sending requires Workers Paid**, while routing supports Workers Free and Paid.

⇒ 拆开读：
- **入站 routing**（收信、转发）→ Free 档可以；
- **向任意收件人发信**（我们要的就是这个：给新注册用户发验证码）→ **必须 Workers Paid**。

配合 T101 结论（`docs/research/fluxing-cloud-2026-11/email-verify-channel.md` §3）：
- 首版方案 = Cloudflare Email Service + Workers `send_email` 绑定；
- **Free 档不能出站**；
- Paid 档每月含 **3,000 封**，超出 **$0.35 / 1,000 封**。

⇒ `[推断]` 注册验证码场景下 3,000 封/月 = 3,000 个新注册（或重发）。**远超内测需求**。
也就是说：**这 $5 买的是「能不能发信」这个开关，不是邮件量。**

---

## 2. 三段推演

### 2.1 存储与端点：$0 成立 ✅

本方案在 Cloudflare 上消耗的资源只有三类：

| 资源 | 本方案用途 | Free 档额度 | 余量估计 |
|---|---|---|---|
| Workers Requests | 9 条契约端点 + `/v1/health` | 10 万/天 | `[推断]` 1000 活跃用户 × 20 次/天 = 2 万/天 ⇒ **5 倍余量** |
| D1 行读写 | 账号、设备、同步条目、验证码 | 500 万读 / 10 万写 每天 | `[推断]` 写入是瓶颈：10 万/天 ÷ 20 条/用户天 = **5,000 用户** |
| R2 存储/请求 | 词库快照密文 | 10 GB / 100 万 A 类月 | `[推断]` 单快照 2 MB ⇒ **5,000 份** |

⇒ 结论：**内测与个人使用，服务端成本 $0**。真正的天花板是 **D1 写 10 万行/天**，
大约支撑「5,000 个日活用户 × 每人每天写 20 条」。超过这个量级才需要 $5 档。

### 2.2 验证码发信：$0 不成立 ❌（只要坚持 Cloudflare）

这条是**官方明文的计划门禁**，不是额度耗尽，**没法靠省着用绕开**：
Free 档连「发一封」都不允许。

所以「能不能免费」这个问题的答案，本质上等价于另一个问题：
**「验证码邮件能不能不用 Cloudflare 发？」**

### 2.3 绕开路径：换成第三方 HTTPS 邮件 API ⇒ 回到 $0 ⚠️

技术可行性 `[推断，但依据充分]`：Workers Free 档**允许 HTTPS 出站 `fetch`**，
这是 Workers 的基础能力，不需要 Paid。所以在 Worker 里 `fetch('https://<第三方>/emails', ...)`
把验证码转发出去，是 Free 档能做的事。

阶段 C 的代码结构**已经为这个留了口子**：`src/mail.ts` 有 `MAIL_MODE` 开关，
现在是 `console`（dev 回显）/ `cloudflare`（Email Service 绑定）。
加一个 `MAIL_MODE=resend`（或 `brevo`）**只需改 `src/mail.ts` 一个文件 + 加一个 Secret**，
不动任何端点契约、不动 D1 表、不动客户端。

代价（必须记账）：
1. **多一个第三方服务账号 + API key**，多一个泄密面（key 存 Workers Secret，不进仓库）；
2. **违反 T101 自定的「同厂商、不接第三方账号/API key」边界** ⇒ 需要一次方案变更记录（ADR 或 plan 修订）；
3. **第三方免费额度是别人的政策**，随时可能改（收紧额度、要求绑卡、加验证码）⇒ 属外部依赖。

### 2.4 第三条路（不需要任何外部服务）

`MAIL_MODE=console` 已经实现：验证码打到 Workers 日志（Free 档 20 万事件/天，保留 3 天）。
内测期让用户从日志里取码，**成本 $0、零第三方**，但**不能对外**。

⇒ 建议：**开发与内测期走这条**，把「A 还是 B」的决策推到真正要对外的时候。

---

## 3. 与 T101 五项未验证的对照

T101 文档 `email-verify-channel.md` §6 列了 5 项未验证，本轮取证结果：

| # | T101 遗留问题 | 本轮结果 |
|---|---|---|
| 1 | Workers Paid 月费具体多少 | ✅ **已坐实**：`minimum charge of $5 USD per month for an account`（§1.1） |
| 2 | Paid 含 3,000 封是否准确 | ⚠️ **未核到原文**。`email-routing/pricing/index.md` 会重定向到 HTML 首页，拿不到结构化价目表。仍是 T101 的二手结论 |
| 3 | 超出后 $0.35/1,000 封 | ⚠️ 同上，未核到原文 |
| 4 | Resend / Brevo 免费额度与条款 | ❌ **仍未核价**（`web_search` 402）。这是选方案 B 前必须补的一手证据 |
| 5 | 是否有更便宜的发信路径 | ⚠️ 未穷举。已知 Cloudflare 内部没有更低档；外部待核 |

⇒ 诚实结论：**「Free 不能出站」与「Paid 最低 $5/月」两条已坐实**（这两条决定了结论的方向）；
**具体每封价格与第三方额度未坐实**（这两条只影响「选 A 还是 B」的成本细账，不影响 "$0 能否成立" 的判断）。

---

## 4. 最终答复（对应你的问题）

**问：按这个方案，可以做到免费实现吗？**

**答：分档。**

- **服务端存储 + 全部 9 条契约端点 + 日志 + 客户端本地运算** ⇒ **可以，$0**。
  Workers Free（10 万请求/天）+ D1 Free（500 万读/10 万写每天、5 GB）+ R2 Free（10 GB、100 万 A 类/月）
  完全覆盖，且余量在 1–2 个数量级。
- **验证码邮件** ⇒ **不行，$5/月起**。Cloudflare 官方原文「Arbitrary-recipient sending requires
  Workers Paid」，这是计划门禁不是额度问题，省着用也没用。
- **除非**你把发信换成第三方 HTTPS 邮件 API ⇒ **整体 $0**，但要接受多一个第三方账号 + 一次方案变更记录。
- **或者**开发/内测期先用 `MAIL_MODE=console`（读日志取码）⇒ **$0、零第三方**，但不可对外。

配套操作步骤见 `docs/ops/fluxing-cloud-external-prerequisites.md` §4。