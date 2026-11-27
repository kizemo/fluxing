// 单测配置：vitest-pool-workers 把测试跑在真实的 workerd 里，
// 因此 D1 / R2 / crypto.subtle 的行为与生产一致，不使用 Node 侧替身。
//
// 迁移文件在 setupFiles 里应用到本地 D1，每个测试文件拿到的是「迁移后」的干净库。
//
// 注意：@cloudflare/vitest-pool-workers 0.23（vitest 4 分支）取消了
// `defineWorkersConfig` + `test.poolOptions.workers` 的写法，改为在 `plugins` 里
// 传一个 `cloudflareTest({...})` 插件；`singleWorker` 选项随之移除（默认单 worker）。

import { fileURLToPath } from 'node:url';
import { cloudflareTest, readD1Migrations } from '@cloudflare/vitest-pool-workers';
import { defineConfig } from 'vitest/config';

const MIGRATIONS_DIR = fileURLToPath(new URL('./migrations', import.meta.url));

// 测试专用 pepper：真实值走 `wrangler secret put AUTH_PEPPER`，绝不进代码库。
const TEST_PEPPER = 'fluxing-test-pepper-0123456789abcdef';

export default defineConfig({
  plugins: [
    cloudflareTest(async () => ({
      wrangler: { configPath: './wrangler.toml' },
      miniflare: {
        bindings: {
          TEST_MIGRATIONS: await readD1Migrations(MIGRATIONS_DIR),
          AUTH_PEPPER: TEST_PEPPER,
          ENVIRONMENT: 'test',
          // 测试不发真信：sendVerificationMail 只写 mail_outbox，用例从表里取码
          MAIL_MODE: 'outbox',
          ACCESS_TOKEN_TTL: '3600',
          REFRESH_TOKEN_TTL: '2592000',
          KDF_ALGO: 'PBKDF2-SHA256',
          KDF_ITERS: '600000',
          // 每账号每 UTC 日的 D1 行写入预算上限（ADR 0076 D2-3）。
          // 取生产同值（20,000）：既有用例（api.test.ts 等）的小批量推送不受护栏影响，
          // 超限路径由 test/write-budget.test.ts 单独注入小阈值验证。
          WRITE_BUDGET_ROWS_PER_DAY: '20000',
        },
      },
    })),
  ],
  test: {
    // 只收 workerd 底座。`test-pg/**` 是 Node + 真 PostgreSQL 底座，
    // 用 `npm run test:pg`（vitest.pg.config.ts）单独跑 —— 两者不能混在一个 runner 里。
    include: ['test/**/*.{test,spec}.ts'],
    setupFiles: ['./test/apply-migrations.ts'],
  },
});