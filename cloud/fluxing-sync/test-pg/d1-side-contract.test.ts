/**
 * W-5.4 · **D1 侧契约对照**（真 D1 运行时，不是替身）。
 *
 * 缺口：shim 侧（PG 底座）的 `.first()` 无行返回 `null` 已实测，但**D1 侧从未跑过对照**
 * —— handoff §W-5.4 一直挂着「两端等价未证实」。
 *
 * 手段：本机 `miniflare` 起 workerd + 真 D1 绑定（与 `@cloudflare/vitest-pool-workers`
 * 同一套 D1 实现，与生产 D1 同一实现），把 D1 的行为逐条打出来，
 * 再由**真 PG 底座**（`changes-normalization.test.ts` / `d1-shim-pg.test.ts`）对照。
 * 探针本体在 `d1-side-probe.mjs`（手工脚本，不被 vitest 收录），本文件只做**断言与固化**。
 *
 * 四条对照：
 *   1. `.first()` 无匹配行 ⇒ D1 返回 **null**（不是 pg 原生的 undefined）⇒ 两端等价；
 *   2. `.first('列名')` 重载无匹配行 ⇒ 同样 **null**；
 *   3. `meta.changes` 在 DDL / UPDATE 0 行 / INSERT / DELETE 四条路径上**恒为 number**
 *      （DDL 也是 0）⇒ shim 的 `rowCount ?? 0` 归一与 D1 语义一致；
 *   4. `batch()` 遇唯一约束冲突 ⇒ 整批回滚（第一条也不落库）⇒ 与 shim 的事务语义一致。
 *
 * ⚠ 探针必须放在 `cloud/fluxing-sync/` 目录树内才能解析 `node_modules/miniflare`。
 */

import { execFileSync } from 'node:child_process';
import { existsSync } from 'node:fs';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';

import { describe, expect, it } from 'vitest';

const HERE = fileURLToPath(import.meta.url);
const PROJECT_ROOT = join(HERE, '..', '..');
const PROBE = join(HERE, '..', 'd1-side-probe.mjs');

/** 跑一次 D1 侧探针，返回按 `KEY=VALUE` 解析出的字典。 */
function runProbe(): Record<string, string> {
  expect(existsSync(PROBE), `D1 探针缺失：${PROBE}`).toBe(true);
  const stdout = execFileSync(process.execPath, [PROBE], {
    encoding: 'utf8',
    cwd: PROJECT_ROOT,
    timeout: 120_000,
  });
  const out: Record<string, string> = {};
  for (const line of stdout.split('\n')) {
    const at = line.indexOf('=');
    if (at > 0) out[line.slice(0, at)] = line.slice(at + 1);
  }
  return out;
}

describe('W-5.4 · D1 侧真运行时对照', () => {
  it(
    '探针跑通：miniflare 5 能起出真 D1 绑定',
    () => {
      const out = runProbe();
      expect(out.PROBE_EXIT).toBe('0');
      // 记录实际采用的构造形态（miniflare 5 的签名会漂 ⇒ 证据里必须留痕）。
      expect(out.PROBE_SHAPE).toBeTruthy();
    },
    120_000,
  );

  it(
    '`.first()` 无匹配行 ⇒ D1 侧返回 null（不是 undefined）⇒ 与 shim 侧等价',
    () => {
      const out = runProbe();
      expect(out.D1_MISS_TYPE).toBe('null');
      expect(out.D1_MISS_IS_NULL).toBe('true');
      expect(out.D1_MISS_IS_UNDEFINED).toBe('false');
      // 有行时返回整行对象（不是 null）—— 证明上面的 null 不是「永远返回 null」。
      expect(out.D1_HIT_TYPE).toBe('object');
    },
    120_000,
  );

  it(
    '`.first(\'列名\')` 重载无匹配行 ⇒ 同样返回 null',
    () => {
      const out = runProbe();
      expect(out.D1_MISS_COL_TYPE).toBe('null');
    },
    120_000,
  );

  it(
    'D1 的 meta.changes 在 DDL / UPDATE 0 行 / INSERT / DELETE 四条路径上恒为 number',
    () => {
      const out = runProbe();
      expect(out.D1_DDL_CHANGES).toBe('0 type=number');
      expect(out.D1_UPD0_CHANGES).toBe('0 type=number');
      expect(out.D1_INS_CHANGES).toBe('1 type=number');
      expect(out.D1_DEL_CHANGES).toBe('1 type=number');
    },
    120_000,
  );

  it(
    'D1 的 batch() 遇唯一约束冲突 ⇒ 整批回滚（第一条也不落库）',
    () => {
      const out = runProbe();
      expect(out.D1_BATCH_DUP).toContain('THREW');
      expect(out.D1_BATCH_DUP_ROWS).toBe('0');
    },
    120_000,
  );
});
