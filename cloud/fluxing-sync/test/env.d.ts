// `cloudflare:test` 里 `env` 的类型扩展。
//
// 本文件不参与 `npm run typecheck`（tsconfig 只 include src/**/*.ts），
// 只用于编辑器提示与将来开启测试类型检查时的接线。

import type { Env } from '../src/env';

declare module 'cloudflare:test' {
  interface ProvidedEnv extends Env {
    /** 由 vitest.config.ts 的 readD1Migrations 注入，setupFiles 里消费 */
    TEST_MIGRATIONS: { name: string; queries: string[] }[];
  }
}