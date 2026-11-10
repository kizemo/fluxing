// 每个测试文件开始前把 migrations/ 应用到本地 D1。
// vitest-pool-workers 的 isolatedStorage 会把「迁移完成后」的库状态作为每个用例的起点。

import { applyD1Migrations, env } from 'cloudflare:test';

await applyD1Migrations(env.DB, env.TEST_MIGRATIONS);