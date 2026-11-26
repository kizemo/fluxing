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
  /**
   * 可选：删对象（ADR 0080 语义 3 的旧版本回收）。
   *
   * 声明为可选而不是必选，是为了让**只读/只写**的测试假桶继续满足契约；生产传
   * `env.BUCKET`（`R2Bucket.delete` 支持单键与键数组，参数双向兼容）。
   * 不实现 delete 的桶 ⇒ 回收退化为「留孤儿 + 日志」，不影响一致性。
   */
  delete?(keys: string[]): Promise<unknown>;
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

/**
 * 「尽力而为」地删一批 R2 对象（ADR 0080 语义 3：旧版本回收 / 孤儿兜底）。
 *
 * 三条硬约束，全部来自「删除失败不得污染已提交的那一批」：
 *   1. **绝不抛错**：本函数对调用方永不 reject。删不掉的对象只是一份空间债
 *      （读路径只认 D1 行内 r2_key，语义 5），但若让删除异常上抛，一次已提交的
 *      D1 batch 会被判成失败 ⇒ 客户端重推 ⇒ 真正的写放大。
 *   2. **单批一次 delete**：整批键一次性交给 `R2Bucket.delete(keys[])`，不给每条
 *      开一个请求；ADR 0074 的并发结论（并发 6 / 15 s）不受影响。
 *   3. **逐键留痕**：任何一个键没删掉都打一行
 *      `[fluxing-sync] orphan object left key=<key>`，供运维按前缀兜底 GC。
 *      桶不支持 delete 时同口径逐键留痕。
 *
 * @returns 实际「已交给 R2 且未抛错」的键数（仅用于诊断与单测断言，不参与业务判定）
 */
export async function deleteAllQuietly(
  bucket: R2WriteBucket,
  keys: readonly string[],
): Promise<number> {
  const unique = [...new Set(keys.filter((key) => key.length > 0))];
  if (unique.length === 0) return 0;

  const reportOrphans = (targets: readonly string[]): void => {
    for (const key of targets) {
      console.warn(`[fluxing-sync] orphan object left key=${key}`);
    }
  };

  if (typeof bucket.delete !== 'function') {
    reportOrphans(unique);
    return 0;
  }

  try {
    await bucket.delete(unique);
    return unique.length;
  } catch (error) {
    // 批量删整体失败 ⇒ 无法知道哪个键成功，按「一个都没删掉」登记（保守口径：
    // 多记孤儿不会造成数据问题，漏记则会留下无主对象且无人知晓）。
    const reason = error instanceof Error ? error.message : String(error);
    console.warn(`[fluxing-sync] orphan delete batch failed keys=${unique.length}: ${reason}`);
    reportOrphans(unique);
    return 0;
  }
}
