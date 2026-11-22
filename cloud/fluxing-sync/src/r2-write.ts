// R2 批量写并发化（ADR 0074）。
//
// 背景：2026-11-22 真机同步失败码 12002（WinHTTP ERROR_WINHTTP_TIMEOUT）根因定案于
// 服务端串行写 R2 —— 单条 put ≈ 626–706 ms，一批 64 条 = 40 071 ms，超过客户端
// 15 s 接收超时。本模块只改「R2 阶段的内部并发度」，**不改变**任何不变量：
//   · 写序仍是「R2 先、D1 后」（调用方在本函数全部返回后才写 D1）；
//   · 条目数上限仍是 wire.ts 的 MAX_PUSH_ITEMS = 64（网关层判，本模块不参与）；
//   · 每个对象的 key 与 metadata 由调用方给出，本模块原样透传；
//   · 幂等语义不变（同 key 覆盖）。
//
// 并发度取 6：Cloudflare Workers 同时打开的连接数上限为 6，超出会触发
// `Network connection lost`。64 条 / 6 ≈ 11 波，单波 ≈ 0.63 s ⇒ 预期 ≈ 7 s，
// 低于旧客户端（已装机版本）的 15 s 接收超时。

export const R2_WRITE_CONCURRENCY = 6;

/** R2 写入元数据（本模块只关心同步路径实际使用的两个字段）。 */
export interface R2WriteMetadata {
  httpMetadata?: { contentType?: string };
  customMetadata?: Record<string, string>;
}

/** R2 写入载荷。 */
export type R2WriteValue = ArrayBuffer | Uint8Array | string;

/** 一次 R2 写入的目标。 */
export interface R2WriteTarget {
  key: string;
  value: R2WriteValue;
  options?: R2WriteMetadata;
}

/**
 * R2 桶的最小结构契约。
 *
 * 刻意用结构类型而不是 `R2Bucket`：这样单测可以传假桶，不必依赖
 * `@cloudflare/workers-types` 的具体类型；生产调用点传 `env.BUCKET` 即可
 * （`R2Bucket.put` 是方法签名，参数双向兼容，赋值成立）。
 */
export interface R2WriteBucket {
  put(key: string, value: R2WriteValue, options?: R2WriteMetadata): Promise<unknown>;
}

/**
 * 以受控并发度批量写 R2。
 *
 * 按「波」推进：每波最多 `concurrency` 个 put，`Promise.all` 等整波落盘后再放下一波。
 * 选「波」而不是「滑动窗口」的理由：
 *   · 实现简单、并发度上界严格可证（任意时刻在飞 put 数 ≤ concurrency）；
 *   · 波与波之间天然形成背压，不会一次性把 64 个请求全部压给 R2；
 *   · 失败语义与串行版一致 —— 抛错即中断，调用方按「本批什么都没写完」处理。
 *
 * @param bucket R2 桶
 * @param targets 写入目标（顺序即调用方期望的写入顺序）
 * @param concurrency 并发度，默认 R2_WRITE_CONCURRENCY；小于 1 时按 1 处理
 */
export async function putAllWithConcurrency(
  bucket: R2WriteBucket,
  targets: readonly R2WriteTarget[],
  concurrency: number = R2_WRITE_CONCURRENCY,
): Promise<void> {
  if (targets.length === 0) return;
  const limit = Math.max(1, Math.min(Math.floor(concurrency), targets.length));
  for (let start = 0; start < targets.length; start += limit) {
    const wave = targets.slice(start, start + limit);
    await Promise.all(
      wave.map((target) => bucket.put(target.key, target.value, target.options)),
    );
  }
}
