// 运行时动态 import（路线 B · ADR 0079）。
//
// 为什么不用 `await import('pg')` 或 `import type { Pool } from 'pg'`：
//   · 本仓库运行时**零 dependencies**（package.json 的 dependencies 为空），
//     `tsc --noEmit` 与 wrangler 构建都在没有 `pg` / `@types/node` 的环境里跑；
//   · `src/**` 是 Workers 打包范围（wrangler 从 src/index.ts 起静态追踪），
//     一旦出现静态 `import 'pg'` / `import 'node:fs'`，Workers 侧构建与 typecheck
//     会同时失败 —— 而这两个模块只给自建服务器用（src/node-*.ts），与 Workers 无关。
//
// 因此把 specifier 藏进 `new Function`：TypeScript 不解析它，打包器也不静态追踪它，
// 运行时才真正 import。代价是拿回来的模块要按本文件调用方自己声明的**结构类型**使用
// （见 d1-shim.ts 的 PgModuleLike / r2-fs.ts 的 FsPromisesLike），编译器不再替我们
// 校验上游 API 形状 —— 这是自觉付出的成本，换来的是「零依赖 + 零类型冲突」。
//
// 不要把它替换成 `createRequire` 或 `module.createRequire`：同样需要 node 类型。

/** 动态 import 的最小契约。 */
export type DynamicImport = (specifier: string) => Promise<unknown>;

/**
 * `new Function` 造出来的运行时 import。返回的模块形状由调用方声明的结构类型决定。
 *
 * 注意：`new Function` 在 Workers 运行时里是被禁的（eval 类），但本模块只被
 * src/pg/** 与 src/node-*.ts 引用，从不进入 Workers 打包图。
 */
let loader: DynamicImport = new Function(
  'specifier',
  'return import(specifier)',
) as DynamicImport;

/**
 * 运行时 import。返回的模块形状由调用方声明的结构类型决定，本函数不做校验。
 *
 * 这里只是**转发**给 `loader`，不为抽象而抽象：Vitest 的 ESM 运行器不给
 * `new Function` 里的 `import()` 配 import callback，调用会抛
 * `TypeError: A dynamic import callback was not specified.`，而测试底座（test-pg/）
 * 必须能真的加载 pg 与 node:fs/promises。留这个注入口后，测试侧换成原生 import 即可，
 * 生产路径仍是 `new Function`，行为逐字不变。
 */
export const dynamicImport: DynamicImport = (specifier) => loader(specifier);

/**
 * **测试专用**：替换运行时 import 实现。生产代码不要调用。
 *
 * 本文件不出现任何静态 `import(...)` 字面量，所以交换实现不会把 pg / node:fs
 * 泄漏进 Workers 打包图。
 */
export function setDynamicImport(next: DynamicImport): void {
  loader = next;
}