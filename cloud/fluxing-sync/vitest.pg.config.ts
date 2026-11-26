/**
 * W5 测试底座的第二套配置：Node 环境 + 真 PostgreSQL（路线 B）。
 *
 * 与 `vitest.config.ts` 的关系：**并列，不是替换**。
 *   · `vitest.config.ts`    → `npm test`        → `@cloudflare/vitest-pool-workers`（workerd）；
 *   · `vitest.pg.config.ts` → `npm run test:pg` → Node 26 + 真 PG 18.4 + 本地对象树。
 *
 * 三条刻意的配置：
 *   1. `fileParallelism: false` —— 每个测试文件独占一个 PG schema（`t_<slug>`），
 *      但 schema 的 drop/create 必须串行，否则两个文件会互删对方的表；
 *   2. `include` 只认 `test-pg/**` —— `test/**` 里的文件 import `cloudflare:test`，
 *      在 Node 环境里解析不到，误纳入会直接红；
 *   3. 超时给足 —— 第一次跑要建 schema + 执行 9 表 22 索引的 DDL（本机约数十毫秒），
 *      但 CI 上的冷 PG 可能明显更慢；PBKDF2 60 万轮单次也在百毫秒量级。
 */

import { defineConfig } from 'vitest/config';

export default defineConfig({
  test: {
    include: ['test-pg/**/*.test.ts'],
    environment: 'node',
    fileParallelism: false,
    testTimeout: 30_000,
    hookTimeout: 60_000,
  },
});