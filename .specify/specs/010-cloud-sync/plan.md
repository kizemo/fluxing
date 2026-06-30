# 010 · Plan · 跨设备云同步

## 1. 技术上下文

- **Vercel** Serverless Functions（Node.js） + KV / Postgres（task 评估）。
- **客户端**：C++ 17 / libcurl（已有 vendored 经验）。
- **认证**：邮箱 + 密码 + WebAuthn Passkey（自实现或 webauthn-cpp vendored）。
- 不引入新 GUI 框架；复用 spec 006 托盘面板。
- **数据格式**：`sync-package.json`（spec 010 design.md §1.4 详细 schema）。

## 2. Architecture

- **客户端 module**：新增 `RimeWithWeasel/Sync/{SyncClient,SyncScheduler,ConflictResolver}.{h,cpp}`。
- **触发**：
  - 启动时拉取：WeaselServer 启动 → SyncClient::Pull → 合并本地。
  - 增量 push：输入停止 5s 后，mtime 检测有变更 → push。
- **冲突策略**：LWW（last-writer-wins），version vector = `{device, timestamp}`。
- **降级**：网络失败 → 继续用本地缓存，不阻断输入。

## 3. Constitution Check

| Rule | Status | Notes |
|---|---|---|
| I. Intent | OK | spec.md §1.1 明确"多设备共享自造资产" |
| II. Test | OK | TestSyncClient（mock Vercel API）+ TestConflictResolver |
| III. Spec-Artifact | OK | 3 件套齐全 + design.md §1.4 详细 schema |
| IV. Clarification | OK | 用户已拍板账户系统（Q3=A） |
| V. Incremental | OK | v2 P2 仅设计；v2.1+ 实施 |
| R1-R9 | OK | 引用 spec 004 §1.1 同步范围 |
| P1-P8 | OK | P3 云同步 brand-fork scope 内 |

## 4. 风险

- **R1**：Vercel 免费额度有限（每日 100K 请求 + 100GB 带宽）；高频 push 用户可能超限。
- **R2**：LWW 冲突策略可能丢更新（如 A / B 同时编辑同一短语）。
- **R3**：WebAuthn Passkey 在 Windows 10 老版本不支持（task 评估 fallback）。
- **R4**：隐私合规（GDPR / 中国数据安全法）— 用户数据走 Vercel 海外节点需用户知情同意。