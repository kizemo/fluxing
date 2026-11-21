# ADR 0070 — 同步路径补 401→续期→重试：访问令牌过期不再让同步持续失败

- **Status**: Accepted
- **Date**: 2026-11-22
- **Spec**: [100-fluxing-cloud-account-sync](../../.specify/specs/100-fluxing-cloud-account-sync/spec.md) —— **不改规格**：FR-016 早已要求界面显示「最近一次同步时间与结果」；本 ADR 只补**同步路径的鉴权恢复实现**，不动 FR 编号与措辞
- **Revises**: —（补齐 ADR 0039 / 0055 之后的同步客户端鉴权缺口，不推翻既有决策）
- **Lessons**: `.specify/memory/lessons-learned.md`
- **Supersedes / superseded by**: —

---

## 背景

### 事实 1 — 账号服务的 401→续期→重试**只覆盖设备管理三处**

`WeaselServer/Cloud/CloudAccount.cpp` 里 `CloudAccountService::EnsureFreshToken`（= 401 后用刷新令牌续期）
的调用点只有三处：

| 端点 | 函数 |
|---|---|
| `GET /v1/devices` | `ListDevices` |
| `DELETE /v1/devices/<id>` | `RevokeDevice` |
| `DELETE /v1/account/data` | `DeleteCloudData` |

### 事实 2 — 同步路径（推 / 拉）**没有**这层

`WeaselServer/Cloud/CloudSyncService.cpp` 的 `CloudSyncService` 在构造时拷贝一份 `access_token_` 快照，
`SendJson` 把 `Bearer <token>` 写进请求头；**收到 401 直接返回失败**，没有任何续期与重试。

`CloudSyncCoordinator::SyncOnce`（生产路径）内部建两个 `CloudSyncService`（拉一个、推一个），
两者都拿协调器构造时那份令牌快照 ⇒ **同步轮次的所有请求都遇 401 即死**。

### 事实 3 — 真机实测：令牌一过期，同步就持续失败

批次 BK 真机两轮（`reports/fluxing-bk-syncstatus/`）都得到：

```
最近同步：2026-11-21 22:24:54 · 失败（访问令牌已过期，请用刷新令牌续期。）
```

⇒ **访问令牌一过期，自动同步与手动同步全部失败**，除非用户先触发一次设备列表刷新顺带续期。

### 事实 4 — 这个缺陷此前**不可见**，因为 1017 没有观测面

批次 BK 把「最近同步」状态行接到真实轮次结果上（ADR 0069）后，
上述失败才第一次被**人眼可见**地暴露出来 —— 这正是 FR-016 要的效果。

### 事实 5 — 凭据结构决定「谁能续期」

访问令牌有 TTL（`expires_in`），**刷新令牌**才是长期凭据；
刷新令牌只由 `CloudAccountService` 持有（Credential Manager / DPAPI）。
⇒ 同步客户端**不认识凭据**，只能「问账号服务要一个新令牌」。

---

## 决策

### 决策 1 — 续期能力以**回调**注入同步服务（可选，默认关闭）

`CloudSyncService` 新增：

```cpp
using AccessTokenRefresher =
    std::function<bool(std::string* out_token, std::string* err)>;
void SetAccessTokenRefresher(const AccessTokenRefresher& refresher);
```

**为什么不把账号服务直接塞进同步服务**：同步客户端要能脱离凭据单测（宪法 R7），
而账号服务持有凭据、涉及 DPAPI。回调把两者的边界固定在「一个字符串」上。

### 决策 2 — 401 时续期**一次**并用新令牌重发同一请求

`SendJson` 改为至多两次尝试：

1. 首发（带当前令牌）；
2. 若响应 401 且已注入回调 ⇒ 调回调拿新令牌 ⇒ **用新令牌重发同一个请求**。

- **最多重试一次，不循环**：服务端持续 401（如令牌被吊销）时如实报失败，不刷请求。
- 续期回调**失败** ⇒ 直接失败并把回调给的中文原因交给界面（不再重试）。
- 没有注入回调 ⇒ 行为与修复前完全一致（401 即失败）⇒ 不改变任何现有调用方的语义。

### 决策 3 — 协调器透传回调

`CloudSyncCoordinator::SetAccessTokenRefresher` 把回调交给内部创建的每个
`CloudSyncService`（拉一个、推一个）⇒ 推与拉**都**能续期重试。

### 决策 4 — 账号页负责注入：账号服务续期 → 取新令牌

`CloudAccountDialog.cpp` 的 `RunSyncRound`（后台线程，宪法 P2 不阻塞 UI）：

```cpp
coordinator.SetAccessTokenRefresher(
    [this](std::string* out_token, std::string* err) -> bool {
      std::string refresh_err;
      if (!service || !service->Refresh(&refresh_err)) { /* 中文原因 */ return false; }
      return service->GetAccessToken(out_token);
    });
```

- 刷新令牌也过期 ⇒ `CloudAccountService::Refresh` 失败并把状态转成 `kNeedsRelogin`
  ⇒ 用户重新登录；中文原因落到「最近同步」那一行。
- 「保存账号」未勾选的会话（`persisted=false`）**不写**凭据存储 —— 这是 `Refresh` 既有语义，本 ADR 不改。

### 决策 5 — 明确不做的事

不动 IPC 协议（不触发 IPC 四边同步红线）、不动控件 ID、不动布局表、不加控件、
不改 `librime/`、不新增第三方依赖、不 bump 版本号（装机版仍 1.3.8.0）。

### 决策 6 — 口径维持（两条并存）

- 1017 = **人眼可见**的观测面（ADR 0069）。
- 「维护闸门是否放行 = **导出文件 mtime 是否前进**」的机读口径**继续有效**。
- **1017 不升级为闸门判据**。

---

## 后果

### 正面

- 访问令牌过期**不再**让同步持续失败：一次 401 ⇒ 续期 ⇒ 重试 ⇒ 本轮正常完成。
- 续期与重试**都在同步服务内部**⇒ 推、拉、分页、多批次推送都自动受益，不需要逐处改。

### 代价 / 边界

- 每轮同步在 401 时**多一次** `POST /v1/auth/refresh`（只在 401 时发生，正常路径零开销）。
- 服务端持续 401 ⇒ 续期一次后如实失败（不循环、不刷配额）。
- 刷新令牌也过期 ⇒ 必须重新登录（本 ADR 不做静默的「重新登录」）。

### 覆盖范围

本批只补 **`/v1/sync/push` 与 `/v1/sync/pull`** 两个端点的鉴权恢复；
设备管理三处本就具备（事实 1），账号/注册类端点无鉴权语义。

---

## 验证（门禁见同批 handoff）

单测组 **BL**（`test/TestFluxingCloud/TestFluxingCloud.cpp`，判据全部来自生产实现）：

| # | 断言 |
|---|---|
| ① | 推送：首发 401 ⇒ 续期一次 ⇒ **重试请求带新令牌** ⇒ 成功（accepted / server_seq 被消费） |
| ② | 拉取：首发 401 ⇒ 续期一次 ⇒ 重试带新令牌 ⇒ 成功 |
| ③ | **没有**续期回调 ⇒ 401 直接失败、**只发一次请求**（不静默重发） |
| ④ | 续期失败 ⇒ 失败原因取自续期回调、**不再重试** |
| ⑤ | 重试后仍是 401 ⇒ **只续期一次**（不循环），失败原因仍来自服务端 |
| ⑥ | **协调器透传**：回调经协调器传到内部同步服务 ⇒ 拉取遇到 401 也能续期重试 |
