# 010 · 火流猩输入法 v2 · 跨设备云同步

> **状态：P2 仅设计**。本 spec 010 不在 v2.0.0 实施范围；产出 = 详细 PRD+TDD，评审后推到 v2.1.0 实施。
> 范围：让用户在多台 Windows 设备间同步用户词典 / 常用短语 / 自定义方案配置。

## 0. 上下文

- spec 009 已有 `phrases.json`（常用短语独立库）。
- spec 008 涉及用户词典（`user.db` / `<schema>.user_ignore.txt`）。
- spec 007 涉及方案 yaml。
- v2 P2 增量 = 上述 3 类数据**可同步**到云端，**多设备一致**。

## 1. 产品视角

### 1.1 同步范围

| 数据 | 同步？ | 理由 |
|---|---|---|
| **用户词典 `user.db`** | ✅ | 多设备累积；典型场景 = 公司电脑 + 家里电脑 |
| **常用短语 `phrases.json`** | ✅ | 同步 use_count 即可（无需重置） |
| **自定义方案 `<schema>.schema.yaml`** | ✅ | 用户 patch 的方案在多设备间一致 |
| **`default.yaml` 快捷键** | ✅ | 跨设备快捷键一致 |
| **`weasel.yaml` 样式** | ✅ | 跨设备视觉一致 |
| **输入历史** | ❌ | 隐私 + 数量巨大 + 无业务价值 |
| **临时状态** | ❌ | 当前输入、上屏缓冲等 |
| **OS 相关设置** | ❌ | DPI / 显示器等 |

### 1.2 账户与认证

- **账户系统**：邮箱 + 密码 + **WebAuthn Passkey**（v2 默认；用户拍板 Q3=A）。
- **不引入**：Google / Apple / 微信 / GitHub OAuth。
- **注册流程**：邮箱 → 6 位验证码 → 设密码 → 提示注册 Passkey（可跳过）→ 完成。
- **登录流程**：邮箱 + 密码 → 可选 Passkey 二次验证 → 完成。
- **Token 管理**：access token 1h，refresh token 30d，存 Windows Credential Manager（`CredWrite` API）。
- **设备名**：首次同步时让用户输入（默认 = `ComputerName`）；用于冲突解决。

### 1.3 同步触发

- **手动**：托盘面板"立即同步"按钮（spec 006 占位）。
- **自动 - 启动时拉取**：WeaselServer 启动时拉一次远端 → 合并本地。
- **自动 - 增量 push**：输入停止 5s 后，若本地有变更（mtime 检测），push。
- **冲突策略**：LWW（last-writer-wins），version vector = `{device, timestamp}`。
- **网络失败降级**：本地有缓存就继续用本地；不阻断输入。

### 1.4 同步数据流

```
本地数据
  ├─ user.db              → 导出 userdb.txt
  ├─ phrases.json         → 直接读
  ├─ <schema>.yaml        → 直接读
  ├─ default.yaml         → 直接读
  └─ weasel.yaml          → 直接读
  ↓ 打包
sync-package.json {
  version: int,           # 单调递增
  device: string,         # 设备名
  timestamp: ISO-8601,
  entries: {
    userdb: { hash, content },
    phrases: { hash, content },
    schemas: [{ schema_id, hash, content }],
    default_yaml: { hash, content },
    weasel_yaml: { hash, content }
  }
}
  ↓ 上传
Vercel API
  POST /api/v1/sync
  body: sync-package
  response: { server_version, conflict_strategy: "lww" }
```

### 1.5 用户故事

- **US7-A** [P2]：用户在公司电脑 A 首次输入"测试短语"加入常用短语 + 用户词典。
- **US7-B** [P2]：用户在家里电脑 B 安装 Fluxing v2.1.0 → 登录同一账户 → 自动拉取 → 输入"ceshi"出现"测试短语"候选。
- **US7-C** [P2]：两台电脑离线时均能正常工作；恢复网络后自动同步。

### 1.6 验收（仅设计参考）

- Given Fluxing v2.1.0 已实施 spec 010
- When 用户在 A 设备添加 3 条常用短语并输入停止 5s
- Then A 设备的 WeaselServer 自动 push sync-package.json 到 Vercel
- And 当 B 设备启动时拉取 → 3 条短语出现在 B 设备的 phrases.json
- And B 设备的 use_count 仍为 0（push 时不带 use_count）

## 2. 技术视角

### 2.1 后端架构

- **Vercel Functions** (Node.js)：
  - `POST /api/v1/auth/email` — 发送验证码（用 Resend / Postmark）
  - `POST /api/v1/auth/register` — 邮箱 + 密码 + 验证码
  - `POST /api/v1/auth/login` — 邮箱 + 密码
  - `POST /api/v1/sync/push` — 上传 sync-package
  - `GET /api/v1/sync/pull?since=<version>` — 拉取
- **Vercel Postgres**：
  - `users (id, email, password_hash, created_at, passkey_id)`
  - `devices (id, user_id, device_name, last_seen)`
  - `sync_entries (id, user_id, device_id, type, hash, content, version, created_at)`
- **WebAuthn**：浏览器原生 API；服务端用 `@simplewebauthn/server`。
- **加密**：敏感字段（密码 hash 用 argon2id） + sync-package 在传输层 TLS 1.3，**不**做端到端加密（v2 范围）；v3 考虑 E2EE。

### 2.2 客户端架构

- **新模块 `FluxingCloudSync`**：
  - `Auth.{h,cpp}` — 注册 / 登录 / token 刷新（用 libcurl）
  - `SyncClient.{h,cpp}` — push / pull 调度
  - `ConflictResolver.{h,cpp}` — LWW 实现
  - `CredentialStore.{h,cpp}` — 包装 Windows Credential Manager
  - `NetworkMonitor.{h,cpp}` — 网络可用性检测（`InternetCheckConnection`）
- **触发点**：
  - 启动 → WeaselServer::OnStart → SyncClient::Pull
  - 输入停止 5s → 监听 RimeWithWeaselHandler::OnCommit → SyncClient::SchedulePush
  - 托盘面板"立即同步" → SyncClient::PushNow
  - 定时器每 1h 检查版本号 → 若有新远端版本 → 拉取

### 2.3 隐私边界

- 同步内容 = 用户词典 / 短语 / 方案 yaml，**不含**输入历史 / 上屏内容。
- 服务端不存 raw password（argon2id hash）。
- 服务端可看到 `email` + `device_name` + 同步包内容。
- 用户可在设置 UI（spec 007）"清空云端所有数据"。

### 2.4 风险

- **R1**：libcurl 在 Win10/11 网络栈偶尔 TLS 失败 → 加重试 + 退避。
- **R2**：Postgres 配额（Vercel 免费 256MB）→ 单用户数据 < 10MB 够用；定期归档冷数据。
- **R3**：WebAuthn 在 Windows 10 1809 以下不支持 → 退化为"邮箱 + 密码"单因素。
- **R4**：冲突 LWW 在多设备并发改同一短语时丢更新 → v2 接受；v3 引入 CRDT。
- **R5**：邮件发送被反垃圾拦截 → 提示用户"检查垃圾邮件" + 提供重发按钮。

## 3. Out of scope (v2.1)

- 不做端到端加密（E2EE）。
- 不做 CRDT / OT 冲突解决。
- 不做"输入历史跨设备回看"。
- 不做 Web 端访问云端数据（仅 Windows 客户端）。
- 不做"分享短语给朋友"。
- 不做"团队 / 组织账户"。

## 4. 完成定义（待 v2.1 启动时再细化）

> 本 spec 当前仅交付 design.md；评审通过后才会写 tasks.md + plan.md + spec.md。

- [ ] 后续：Vercel 仓库 + Postgres 部署
- [ ] 后续：`FluxingCloudSync` 客户端实现
- [ ] 后续：WebAuthn 集成
- [ ] 后续：UI（spec 007 同步页 + spec 006 状态显示）
- [ ] 后续：单测 + 集成测试
- [ ] 后续：手动验证 3 平台 3 DPI
- [ ] 后续：commit + release `fluxing-2.1.0-installer.exe`