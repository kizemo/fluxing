// putAllWithConcurrency 单测（ADR 0074）。
//
// 判据：
//   1. 全部目标都被写入，key 与 metadata 原样透传；
//   2. 任意时刻在飞 put 数 ≤ 并发度上界（并发不会被突破）；
//   3. 并发度 > 1 时确实并发（最大在飞数 > 1）—— 防止「退化成串行」这种假修复；
//   4. 并发度 = 1 时严格串行（最大在飞数 = 1）；
//   5. 空目标列表不触发任何 put；
//   6. 任一 put 失败时整个调用抛错（不静默吞掉）。

import { describe, expect, it, vi } from 'vitest';
import {
  deleteAllQuietly,
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

// ---------------------------------------------------------------------------
// deleteAllQuietly（ADR 0080 语义 3：旧版本回收必须「绝不抛错」）
//
// 判据刻意分两档，因为「没有异常」既可能是真的被吞掉、也可能是根本没人调用：
//   · 失败用例除了断言不 reject，还必须断言**留痕**（orphan 日志逐键一条）；
//   · 成功用例断言整批只发一次 delete 且回传键数，防止退化成逐键请求。
// ---------------------------------------------------------------------------

interface DeleteRecord {
  keys: string[];
}

function makeDeletableBucket(mode: 'ok' | 'throw'): {
  bucket: R2WriteBucket;
  calls: DeleteRecord[];
} {
  const calls: DeleteRecord[] = [];
  const bucket: R2WriteBucket = {
    async put() {
      return undefined;
    },
    async delete(keys) {
      calls.push({ keys: [...keys] });
      if (mode === 'throw') throw new Error('r2 delete unavailable');
      return undefined;
    },
  };
  return { bucket, calls };
}

describe('deleteAllQuietly', () => {
  it('整批一次 delete，去重后按键数回传', async () => {
    const { bucket, calls } = makeDeletableBucket('ok');
    const warn = vi.spyOn(console, 'warn').mockImplementation(() => {});
    const n = await deleteAllQuietly(bucket, ['k/a', 'k/a', '', 'k/b']);
    expect(n).toBe(2);
    expect(calls).toHaveLength(1);
    expect(calls[0]!.keys).toEqual(['k/a', 'k/b']);
    expect(warn).not.toHaveBeenCalled();
    warn.mockRestore();
  });

  it('delete 抛错时不上抛，返回 0 且逐键留孤儿日志（已提交批次不得被判失败）', async () => {
    const { bucket, calls } = makeDeletableBucket('throw');
    const warn = vi.spyOn(console, 'warn').mockImplementation(() => {});
    await expect(deleteAllQuietly(bucket, ['k/a', 'k/b'])).resolves.toBe(0);
    expect(calls).toHaveLength(1);
    const orphanLines = warn.mock.calls
      .map((args) => String(args[0]))
      .filter((line) => line.includes('[fluxing-sync] orphan object left key='));
    expect(orphanLines).toEqual([
      '[fluxing-sync] orphan object left key=k/a',
      '[fluxing-sync] orphan object left key=k/b',
    ]);
    // 失败本身也要有一条可检索的批量失败留痕（区分「删不掉」与「没删」）
    expect(
      warn.mock.calls.some((args) => String(args[0]).includes('orphan delete batch failed')),
    ).toBe(true);
    warn.mockRestore();
  });

  it('桶不实现 delete ⇒ 不抛错、返回 0，并逐键留痕（退化路径必须可见）', async () => {
    const bucket: R2WriteBucket = { async put() { return undefined; } };
    const warn = vi.spyOn(console, 'warn').mockImplementation(() => {});
    await expect(deleteAllQuietly(bucket, ['k/a'])).resolves.toBe(0);
    expect(warn.mock.calls.map((args) => String(args[0]))).toContain(
      '[fluxing-sync] orphan object left key=k/a',
    );
    warn.mockRestore();
  });

  it('空列表 / 全空键不触发 delete', async () => {
    const { bucket, calls } = makeDeletableBucket('ok');
    await expect(deleteAllQuietly(bucket, [])).resolves.toBe(0);
    await expect(deleteAllQuietly(bucket, [''])).resolves.toBe(0);
    expect(calls).toHaveLength(0);
  });
});
