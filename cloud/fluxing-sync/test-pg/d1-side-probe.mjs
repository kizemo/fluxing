/**
 * W6 残留核实 · D1 侧对照探针（**手工取证脚本**，不由 vitest 收录）。
 *
 * 目的：坐实「`.first()` 查询无匹配行时，D1 侧到底返回 `null` 还是 `undefined`」
 * —— 这是 handoff §W-5.4 的缺口：shim 侧已实测 `null`，但**D1 侧从未跑过对照**。
 *
 * 手段：用本机已装的 `miniflare`（与 `@cloudflare/vitest-pool-workers` 同一套 D1
 * 实现）单独起一个 D1 绑定，在**无匹配行**的查询上取 `.first()` 的返回值形态。
 * 跑的就是 Workers 侧的 D1 API 表面（与生产 D1 同一实现），不是 SQLite 的替身。
 *
 * 本文件放在 `test-pg/` 目录下、文件名**不以 `.test.ts` 结尾** ⇒ 不被 vitest 收录，
 * 但 `node_modules` 能解析到 `miniflare`（放在 `reports/` 下会解析失败）。
 *
 * ⚠ miniflare 5（`5.20260815.0-alpha`）的构造签名与 v4 不同：v5 强制 `workers` 数组，
 *   且每个 worker 必须是 `{ config: <wrangler 形态的解析后配置> }`。
 *   探针按**候选形态依次尝试**，第一个能构造成功的形态即被采用，并打印 `PROBE_SHAPE=`
 *   ⇒ 复现者能拿到同样的结果，不会因形态漂移而误判。
 *
 * 用法（在 cloud/fluxing-sync 目录下）：
 *   node test-pg/d1-side-probe.mjs
 *
 * 口径：
 *   · `null`      ⇒ D1 侧就是 `null` ⇒ shim 的归一化与 D1 **等价**（两端一致）。
 *   · `undefined` ⇒ D1 侧是 `undefined` ⇒ shim 把它**改**成了 `null` ⇒ 两端**不等价**。
 */

import { mkdtemp, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

const { Miniflare } = await import('miniflare');

const SCRIPT = 'export default { fetch() { return new Response("ok"); } }';

const root = await mkdtemp(join(tmpdir(), 'fluxing-d1-probe-'));
const workerJs = join(root, 'probe-worker.js');
await writeFile(workerJs, SCRIPT, 'utf8');

const shape = (value) => {
  if (value === null) return 'null';
  if (value === undefined) return 'undefined';
  return typeof value;
};

/** D1 侧的五组判据（全部打在 stdout 上）。 */
async function probe(db, label) {
  await db.prepare('CREATE TABLE probe_t (id TEXT PRIMARY KEY, v TEXT NOT NULL)').run();
  await db.prepare('INSERT INTO probe_t (id, v) VALUES (?, ?)').bind('exists', '1').run();

  // ① 有行
  const hit = await db.prepare('SELECT id FROM probe_t WHERE id = ?').bind('exists').first();
  // ② 无行（核心判据）
  const miss = await db.prepare('SELECT id FROM probe_t WHERE id = ?').bind('missing').first();
  // ③ 无行 + 传列名（D1 的这个重载真实存在）
  const missCol = await db
    .prepare('SELECT id, v FROM probe_t WHERE id = ?')
    .bind('missing')
    .first('id');

  console.log('PROBE_SHAPE=' + label);
  console.log('D1_HIT_TYPE=' + shape(hit));
  console.log('D1_MISS_TYPE=' + shape(miss));
  console.log('D1_MISS_COL_TYPE=' + shape(missCol));
  console.log('D1_MISS_IS_NULL=' + String(miss === null));
  console.log('D1_MISS_IS_UNDEFINED=' + String(miss === undefined));

  // ④ D1 的 meta.changes 形态（对照 shim 的 rowCount===null 归一）
  const ddl = await db.prepare('CREATE TABLE probe_ddl (id INTEGER)').run();
  const upd0 = await db
    .prepare('UPDATE probe_t SET v = ? WHERE id = ?')
    .bind('x', 'missing')
    .run();
  const ins = await db.prepare('INSERT INTO probe_t (id, v) VALUES (?, ?)').bind('two', '2').run();
  const del = await db.prepare('DELETE FROM probe_t WHERE id = ?').bind('two').run();
  console.log('D1_DDL_CHANGES=' + String(ddl.meta.changes) + ' type=' + typeof ddl.meta.changes);
  console.log('D1_UPD0_CHANGES=' + String(upd0.meta.changes) + ' type=' + typeof upd0.meta.changes);
  console.log('D1_INS_CHANGES=' + String(ins.meta.changes) + ' type=' + typeof ins.meta.changes);
  console.log('D1_DEL_CHANGES=' + String(del.meta.changes) + ' type=' + typeof del.meta.changes);

  // ⑤ D1 batch 原子性对照（唯一约束冲突 ⇒ 整批回滚）
  try {
    await db.batch([
      db.prepare('INSERT INTO probe_t (id, v) VALUES (?, ?)').bind('dup', '1'),
      db.prepare('INSERT INTO probe_t (id, v) VALUES (?, ?)').bind('dup', '2'),
    ]);
    console.log('D1_BATCH_DUP=NO_ERROR');
  } catch (error) {
    console.log('D1_BATCH_DUP=THREW ' + (error instanceof Error ? error.message : String(error)));
  }
  const after = await db
    .prepare('SELECT COUNT(*) AS n FROM probe_t WHERE id = ?')
    .bind('dup')
    .first();
  console.log('D1_BATCH_DUP_ROWS=' + String(after ? after.n : 'null'));
}

/** 候选形态：第一个能构造成功的即被采用。 */
const MANIFEST = {
  mainModule: 'index.js',
  modulesRoot: root,
  modules: { 'index.js': { type: 'esm', contents: SCRIPT } },
};

const CANDIDATES = [
  {
    label: 'v5-config-manifest-env-d1-name',
    build: () => ({
      workers: [
        {
          config: {
            name: 'probe',
            type: 'worker',
            compatibilityDate: '2026-08-15',
            manifest: MANIFEST,
            env: { DB: { type: 'd1', name: 'probe' } },
          },
        },
      ],
    }),
  },
  {
    label: 'v5-config-manifest-no-modulesRoot-env-d1-name',
    build: () => ({
      workers: [
        {
          config: {
            name: 'probe',
            type: 'worker',
            compatibilityDate: '2026-08-15',
            manifest: {
              mainModule: 'index.js',
              modules: { 'index.js': { type: 'esm', contents: SCRIPT } },
            },
            env: { DB: { type: 'd1', name: 'probe' } },
          },
        },
      ],
    }),
  },
  {
    label: 'v5-config-manifest-env-d1-id',
    build: () => ({
      workers: [
        {
          config: {
            name: 'probe',
            type: 'worker',
            compatibilityDate: '2026-08-15',
            manifest: MANIFEST,
            env: { DB: { type: 'd1', id: 'probe' } },
          },
        },
      ],
    }),
  },
  {
    label: 'v5-config-manifest-env-d1-bare',
    build: () => ({
      workers: [
        {
          config: {
            name: 'probe',
            type: 'worker',
            compatibilityDate: '2026-08-15',
            manifest: MANIFEST,
            env: { DB: { type: 'd1' } },
          },
        },
      ],
    }),
  },
  {
    label: 'v5-config-manifest-env-d1-bare-no-manifest',
    build: () => ({
      workers: [
        {
          config: {
            name: 'probe',
            type: 'worker',
            compatibilityDate: '2026-08-15',
            env: { DB: { type: 'd1' } },
          },
        },
      ],
    }),
  },
];

let done = false;

for (const candidate of CANDIDATES) {
  let mf;
  try {
    mf = new Miniflare(candidate.build());
    // v5 的 getD1Database 需要 worker 名；先试不带名，再试带名。
    let db;
    try {
      db = await mf.getD1Database('DB');
    } catch {
      db = await mf.getD1Database('DB', 'probe');
    }
    await probe(db, candidate.label);
    done = true;
  } catch (error) {
    console.log(
      'PROBE_SHAPE_FAILED=' +
        candidate.label +
        ' :: ' +
        (error instanceof Error ? error.message : String(error)),
    );
  } finally {
    if (mf !== undefined) await mf.dispose().catch(() => undefined);
  }
  if (done) break;
}

await rm(root, { recursive: true, force: true });

if (done) {
  console.log('PROBE_EXIT=0');
} else {
  console.error('PROBE_FAILED: 所有候选形态都无法构造 miniflare 5 实例');
  console.log('PROBE_EXIT=1');
  process.exitCode = 1;
}
