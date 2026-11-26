/**
 * 自建运行时的唯一自动启动点（路线 B）。
 *
 * 为什么这件事要单独一个文件：`node-server.ts` 只导出库函数、导入它**没有副作用**，
 * 这样才能在测试里起停而不污染进程；真正的「一 import 就 listen」只发生在这里。
 *
 * 为什么不用 top-level await：本包 `package.json` 没有 `"type": "module"`，
 * 顶层 await 会让打包产物在某些加载路径下直接语法报错。用 `.catch()` 挂尾即可，
 * 语义等价且没有这条隐式依赖。
 *
 * 启动方式（由部署脚本负责）：esbuild 打成单文件 mjs 后 `node dist/node-main.mjs`。
 * 详见 `src/node-server.ts` 文件头第 3 条边界说明。
 */

import { startFromEnvironment } from './node-server';

startFromEnvironment().catch((error: unknown) => {
  const message = error instanceof Error ? error.message : String(error);
  console.error('[fluxing-sync] 启动失败：' + message);
  const exit = (globalThis as unknown as { process?: { exit?(code?: number): void } }).process?.exit;
  if (exit !== undefined) exit(1);
});