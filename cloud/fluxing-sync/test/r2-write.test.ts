// putAllWithConcurrency 单测（ADR 0074）。
//
// 判据：
//   1. 全部目标都被写入，key 与 metadata 原样透传；
//   2. 任意时刻在飞 put 数 ≤ 并发度上界（并发不会被突破）；
//   3. 并发度 > 1 时确实并发（最大在飞数 > 1）—— 防止「退化成串行」这种假修复；
//   4. 并发度 = 1 时严格串行（最大在飞数 = 1）；
//   5. 空目标列表不触发任何 put；
//   6. 任一 put 失败时整个调用抛错（不静默吞掉）。

import { describe, expect, it } from 'vitest';
import {
  putAllWithConcurrency,
  R2_WRITE_CONCURRENCY,
  type R2WriteBucket,
  type R2WriteTarget,
} from '../src/r2-write';

interface PutRecord {
  key: string;
  value: R2WriteValueLike;
  options: unknown;
}

type R2WriteValueLike = ArrayBuffer | Uint8Array | string;

interface FakeBucket {
  bucket: R2WriteBucket;
  puts: PutRecord[];
  maxInFlight: number;
}

/** 假桶：记录每次 put，并统计「同时在飞」的峰值。 */
function makeFakeBucket(delayMs = 0, failOnKey?: string): FakeBucket {
  const puts: PutRecord[] = [];
  let inFlight = 0;
  let maxInFlight = 0;
  const bucket: R2WriteBucket = {
    async put(key, value, options) {
      inFlight += 1;
      if (inFlight > maxInFlight) maxInFlight = inFlight;
      puts.push({ key, value, options });
      try {
        if (delayMs > 0) {
          await new Promise((resolve) => setTimeout(resolve, delayMs));
        } else {
          await Promise.resolve();
        }
        if (failOnKey !== undefined && key === failOnKey) {
          throw new Error(`boom:${key}`);
        }
        return undefined;
      } finally {
        inFlight -= 1;
      }
    },
  };
  return {
    bucket,
    puts,
    get maxInFlight() {
      return maxInFlight;
    },
  };
}

function makeTargets(count: number): R2WriteTarget[] {
  return Array.from({ length: count }, (_unused, i) => ({
    key: `u/uid/k${String(i).padStart(3, '0')}`,
    value: new Uint8Array([i & 0xff]),
    options: {
      httpMetadata: { contentType: 'application/octet-stream' },
      customMetadata: { type: 'userdb', hlc_ms: String(1000 + i) },
    },
  }));
}

describe('putAllWithConcurrency', () => {
  it('默认并发度是 6（Cloudflare 同时打开连接数上限）', () => {
    expect(R2_WRITE_CONCURRENCY).toBe(6);
  });

  it('全部目标都被写入，key 与 metadata 原样透传', async () => {
    const fake = makeFakeBucket();
    const targets = makeTargets(12);
    await putAllWithConcurrency(fake.bucket, targets);
    expect(fake.puts.length).toBe(12);
    expect(fake.puts.map((p) => p.key)).toEqual(targets.map((t) => t.key));
    expect(fake.puts[0]!.options).toEqual(targets[0]!.options);
    expect(fake.puts[11]!.options).toEqual(targets[11]!.options);
  });

  it('并发度上界不被突破：12 条 / 上界 6 ⇒ 峰值在飞 = 6', async () => {
    const fake = makeFakeBucket(1);
    await putAllWithConcurrency(fake.bucket, makeTargets(12), 6);
    expect(fake.maxInFlight).toBe(6);
  });

  it('确实并发（峰值在飞 > 1），不是退化串行', async () => {
    const fake = makeFakeBucket(1);
    await putAllWithConcurrency(fake.bucket, makeTargets(12), 4);
    expect(fake.maxInFlight).toBeGreaterThan(1);
    expect(fake.maxInFlight).toBeLessThanOrEqual(4);
  });

  it('并发度 = 1 时严格串行（峰值在飞 = 1）', async () => {
    const fake = makeFakeBucket(1);
    await putAllWithConcurrency(fake.bucket, makeTargets(5), 1);
    expect(fake.maxInFlight).toBe(1);
  });

  it('并发度大于目标数时，峰值在飞 = 目标数', async () => {
    const fake = makeFakeBucket(1);
    await putAllWithConcurrency(fake.bucket, makeTargets(3), 32);
    expect(fake.maxInFlight).toBe(3);
  });

  it('空目标列表不触发任何 put', async () => {
    const fake = makeFakeBucket();
    await putAllWithConcurrency(fake.bucket, []);
    expect(fake.puts.length).toBe(0);
  });

  it('任一 put 失败时抛错（不静默吞掉）', async () => {
    const fake = makeFakeBucket(0, 'u/uid/k002');
    await expect(putAllWithConcurrency(fake.bucket, makeTargets(5), 2)).rejects.toThrow(
      'boom:u/uid/k002',
    );
  });
});
