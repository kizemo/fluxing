/**
 * W-5 残留核实 · D1 API 表面**逐点静态审计**（纯源码扫描，不碰真 PostgreSQL）。
 *
 * 覆盖 handoff §W-5 的第 2 条（`.exec(` 归属）以及第 3 / 4 / 5 条的**枚举侧**：
 *   1. W-5.2 · `.exec(` 全仓逐点归属（recon §9.5 记「归属未确认」，ADR 0079 记「全是正则」，
 *      两处口径不一致，本文件用逐点行号把口径钉死）；
 *   2. W-5.3 · `.run()` 站点逐个取 SQL 首动词 ⇒ 全仓是否存在 DDL（DDL 在 PG 上 rowCount 为 null，
 *      是 `rowCount === null` 归一化唯一真正的触发面）；
 *   3. W-5.3 · `meta.changes` 与 `rowCount` 的**消费点收口**（改动面一旦扩大，本闸立即红）；
 *   4. W-5.4 · `.first()` 站点逐个确认「无一例传列名」（传列名时 D1 与 shim 的返回形态不同）；
 *   5. W-5.5 · `.batch()` **业务调用点**枚举（注释里的 `DB.batch(...)` 文字不算）。
 *
 * 扫描口径：先**抹掉注释**（保留换行以维持行号）——否则 `src/pg/d1-shim.ts` 头注释里的
 * `.exec()` 与 `DB.batch(stmts[])` 会被误计成调用点（台账 27 / 6 两个数字就是这么虚高的）。
 *
 * 判定表在 `console.log` 里逐行打印，vitest 输出即证据；断言部分把站点清单钉死，
 * 任何一处新增 / 移除 / 改行都会让本文件变红 ⇒ 强制重新逐点核实。
 */

import { readdirSync, readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

import { describe, expect, it } from 'vitest';

// `fileURLToPath(import.meta.url)` 走**字符串入参**：构造 `URL` 对象会撞上 undici-types
// 的 `URL` 与 node 的 `URL` 之间的类型分歧（`Symbol.dispose` 缺失）。
const SRC_ROOT = join(dirname(fileURLToPath(import.meta.url)), '..', 'src');

/** 盘上基线（2026-11-26 回读）：`.run(` 真实站点 26 个（台账 27，多出的是文档注释假阳性）。 */
const RUN_SITES = [
  'account.ts:91',
  'account.ts:96',
  'auth.ts:210',
  'auth.ts:224',
  'auth.ts:242',
  'auth.ts:296',
  'auth.ts:319',
  'auth.ts:389',
  'auth.ts:418',
  'auth.ts:436',
  'auth.ts:453',
  'auth.ts:629',
  'auth.ts:699',
  'auth.ts:709',
  'auth.ts:710',
  'auth.ts:811',
  'auth.ts:880',
  'auth.ts:897',
  'auth.ts:899',
  'auth.ts:905',
  'auth.ts:986',
  'auth.ts:996',
  'auth.ts:1057',
  'devices.ts:66',
  'devices.ts:67',
  'mail.ts:373',
];

/** `.first()` 站点 23 个（与台账一致）：无一例传列名。 */
const FIRST_SITES = [
  'auth.ts:202',
  'auth.ts:268',
  'auth.ts:292',
  'auth.ts:329',
  'auth.ts:599',
  'auth.ts:679',
  'auth.ts:726',
  'auth.ts:791',
  'auth.ts:831',
  'auth.ts:860',
  'auth.ts:891',
  'auth.ts:920',
  'auth.ts:961',
  'auth.ts:982',
  'auth.ts:1025',
  'devices.ts:35',
  'devices.ts:138',
  // ADR 0080 U1 落码（版本化 key + 提交后删旧对象）在两处写路径插入行 ⇒ 以下 4 个行号
  // 于 2026-11-26 按盘上真实行号回读刷新（站点数不变，仍 23 / 5）。改动写路径时必须同步。
  'sync-buckets.ts:144',
  'sync-buckets.ts:336',
  'sync-buckets.ts:344',
  'sync.ts:212',
  'sync.ts:437',
  'sync.ts:444',
];

/** `.batch()` 业务调用点 5 个（台账 6，多出的是 `d1-shim.ts` 头注释里的 `DB.batch(stmts[])`）。 */
const BATCH_SITES = [
  'devices.ts:143',
  'sync-buckets.ts:307',
  'sync-buckets.ts:425',
  'sync.ts:408',
  'sync.ts:507',
];

/** `.exec(` 站点 3 个：全部是 `RegExp.prototype.exec`，**无一**是 D1 的 `stmt.exec()`。 */
const EXEC_SITES = ['auth.ts:1007', 'index.ts:145', 'mail.ts:272'];

/**
 * 三处 `.exec(` 的接收者**尾部逐字钉死**。
 *
 * 接收者要么是**正则字面量**（以 `/` 或 `/flags` 收尾），要么是**正则常量**（以 `RE` 收尾）；
 * 若哪天真出现 D1 的 `stmt.exec()`，接收者会以标识符收尾 ⇒ 本断言立刻红。
 */
const EXEC_RECEIVER_TAILS: Record<string, string> = {
  'auth.ts:1007': '/^Bearer\\s+(\\S+)$/i',
  'index.ts:145': 'DEVICE_PATH_RE',
  'mail.ts:272': '/(\\d{6})/',
};

/** `meta.changes` 的消费点：只有 account.ts 的两处 DELETE 计数。 */
const CHANGES_SITES = ['account.ts:92', 'account.ts:97'];

interface Site {
  /** 相对 `src/` 的文件路径（统一用 `/`）。 */
  file: string;
  /** 1 基行号。 */
  line: number;
  /** 命中的原文片段。 */
  text: string;
}

function listTsFiles(dir: string, prefix = ''): string[] {
  const out: string[] = [];
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    const rel = prefix === '' ? entry.name : `${prefix}/${entry.name}`;
    if (entry.isDirectory()) {
      out.push(...listTsFiles(join(dir, entry.name), rel));
    } else if (entry.name.endsWith('.ts')) {
      out.push(rel);
    }
  }
  return out.sort();
}

/**
 * 把注释抹成空格（**保留换行** ⇒ 行号不变）。
 *
 * 为什么要抹：`src/pg/d1-shim.ts` 的头注释里就写着 `.exec()` / `DB.batch(stmts[])`，
 * 不抹就会被计成调用点 —— 台账的 27 / 6 两个数字正是这么虚高的。
 */
function blankComments(text: string): string {
  const chars = text.split('');
  const n = chars.length;
  let i = 0;
  while (i < n) {
    const ch = chars[i]!;
    const next = chars[i + 1];
    if (ch === '/' && next === '/') {
      while (i < n && chars[i] !== '\n') {
        chars[i] = ' ';
        i += 1;
      }
      continue;
    }
    if (ch === '/' && next === '*') {
      chars[i] = ' ';
      chars[i + 1] = ' ';
      i += 2;
      while (i < n && !(chars[i] === '*' && chars[i + 1] === '/')) {
        if (chars[i] !== '\n') chars[i] = ' ';
        i += 1;
      }
      if (i < n) {
        chars[i] = ' ';
        chars[i + 1] = ' ';
        i += 2;
      }
      continue;
    }
    if (ch === "'" || ch === '"' || ch === '`') {
      // 字符串字面量整体跳过：里面的 `//`、`/*` 不是注释。
      const quote = ch;
      i += 1;
      while (i < n) {
        const c = chars[i]!;
        if (c === '\\') {
          i += 2;
          continue;
        }
        if (c === quote) {
          i += 1;
          break;
        }
        if (quote !== '`' && c === '\n') break;
        i += 1;
      }
      continue;
    }
    i += 1;
  }
  return chars.join('');
}

interface SourceFile {
  /** 相对 `src/` 的路径。 */
  file: string;
  /** 抹掉注释后的正文（行号与原文件一致）。 */
  text: string;
}

function loadSources(): SourceFile[] {
  return listTsFiles(SRC_ROOT).map((file) => ({
    file,
    text: blankComments(readFileSync(join(SRC_ROOT, file), 'utf8')),
  }));
}

function lineOf(text: string, index: number): number {
  let line = 1;
  for (let i = 0; i < index; i += 1) {
    if (text.charCodeAt(i) === 10) line += 1;
  }
  return line;
}

/**
 * 在全部源文件里找匹配 `pattern` 的调用点（返回 `文件:行号` 清单）。
 *
 * 排序按「文件名 → 行号**数值**」，**不是**字符串序 —— 字符串序会把 `auth.ts:1057` 排到
 * `auth.ts:210` 前面（`'1' < '2'`），与下面人工维护的清单对不上。
 */
function findSites(sources: SourceFile[], pattern: RegExp): string[] {
  const out: { site: string; file: string; line: number }[] = [];
  for (const source of sources) {
    const re = new RegExp(pattern.source, 'g');
    let match: RegExpExecArray | null;
    while ((match = re.exec(source.text)) !== null) {
      const line = lineOf(source.text, match.index);
      out.push({ site: `${source.file}:${line}`, file: source.file, line });
    }
  }
  return out
    .sort((a, b) => (a.file === b.file ? a.line - b.line : a.file < b.file ? -1 : 1))
    .map((entry) => entry.site);
}

/** 取 `.run(` 站点所属语句的 SQL 字面量（取 `prepare(` 之后、`.run(` 之前的第一个字符串）。 */
function sqlOf(text: string, runIndex: number): string {
  const head = text.lastIndexOf('prepare(', runIndex);
  if (head < 0) return '';
  const between = text.slice(head + 'prepare('.length, runIndex);
  const literal = /^\s*(['"`])([\s\S]*?)\1/.exec(between);
  return literal === null ? '' : literal[2]!;
}

/** SQL 首动词（DDL / DML / 查询）。 */
function verbOf(sql: string): string {
  const match = /^\s*([A-Za-z]+)/.exec(sql);
  return match === null ? '（动态 SQL）' : match[1]!.toUpperCase();
}

const SOURCES = loadSources();

describe('W-5.2 · `.exec(` 归属逐点核实', () => {
  const sites = findSites(SOURCES, /\.exec\s*\(/g);

  it('全仓 3 处，逐点定位固定', () => {
    expect(sites).toEqual(EXEC_SITES);
  });

  it('三处接收者全是正则字面量 / 正则常量，无一为 D1 语句的 exec()', () => {
    const receivers = new Map<string, string>();
    for (const source of SOURCES) {
      const re = /\.exec\s*\(/g;
      let match: RegExpExecArray | null;
      while ((match = re.exec(source.text)) !== null) {
        const head = source.text.slice(Math.max(0, match.index - 80), match.index);
        receivers.set(`${source.file}:${lineOf(source.text, match.index)}`, head.trim().slice(-60));
      }
    }
    console.log(
      '[W-5.2] .exec( 接收者：\n' +
        [...receivers.entries()].map(([site, tail]) => `${site} ← …${tail}`).join('\n'),
    );
    expect([...receivers.keys()]).toEqual(EXEC_SITES);
    // 逐点钉死接收者的**尾部**。
    for (const [site, tail] of Object.entries(EXEC_RECEIVER_TAILS)) {
      const actual = receivers.get(site) ?? '';
      expect(
        actual.endsWith(tail),
        `站点 ${site} 的 .exec( 接收者应以「${tail}」收尾，实际：${actual}`,
      ).toBe(true);
    }
  });
});

describe('W-5.3 · `.run()` 站点逐点核实', () => {
  const runSites = findSites(SOURCES, /\.run\s*\(/g);

  it('真实调用点 26 个（台账 27 = 多算了 src/errors.ts 的文档注释）', () => {
    expect(runSites).toEqual(RUN_SITES);
  });

  it('26 个站点的 SQL 首动词逐点列出，且**全部**是 DML（25 个字面量 + 1 个动态 DELETE ⇒ 无 DDL ⇒ PG rowCount 恒为数字）', () => {
    const table: string[] = [];
    const verbs = new Set<string>();
    const dynamicSites: string[] = [];
    for (const source of SOURCES) {
      const re = /\.run\s*\(/g;
      let match: RegExpExecArray | null;
      while ((match = re.exec(source.text)) !== null) {
        const line = lineOf(source.text, match.index);
        const sql = sqlOf(source.text, match.index);
        const verb = verbOf(sql);
        verbs.add(verb);
        if (verb === '（动态 SQL）') dynamicSites.push(`${source.file}:${line}`);
        table.push(`${source.file}:${line}  ${verb.padEnd(8)} ${sql.replace(/\s+/g, ' ').slice(0, 78)}`);
      }
    }
    console.log('[W-5.3] .run( 站点逐点判定表：\n' + table.join('\n'));
    expect(table.length).toBe(26);
    // 唯一可能让 PG 的 rowCount 变成 null 的语句类别就是 DDL（CREATE / ALTER / DROP）。
    expect([...verbs].sort()).toEqual(['DELETE', 'INSERT', 'UPDATE', '（动态 SQL）']);
    // 动态 SQL 只有一个站点（auth.ts:418 的 deleteRow），由下一条用例逐点证明它只会是 DELETE。
    expect(dynamicSites).toEqual(['auth.ts:418']);
  });

  it('唯一的动态 SQL 站点（auth.ts:418）传入的 SQL 逐个是 DELETE ⇒ 全仓 .run( 无 DDL', () => {
    const auth = SOURCES.find((source) => source.file === 'auth.ts');
    expect(auth).toBeDefined();
    const calls: string[] = [];
    // 只匹配**调用点** `deleteRow(env, 'SQL', id)`；函数签名 `deleteRow(env: Env, sql, id)` 里
    // `env` 后面是 `:`，不匹配 ⇒ 不会把定义本身算进来。
    const re = /deleteRow\(\s*env\s*,\s*(['"`])([^'"`]*)\1/g;
    let match: RegExpExecArray | null;
    while ((match = re.exec(auth!.text)) !== null) {
      calls.push(`${lineOf(auth!.text, match.index)}  ${match[2]!.replace(/\s+/g, ' ').slice(0, 70)}`);
    }
    console.log('[W-5.3] 动态 SQL 站点（deleteRow）调用点：\n' + calls.join('\n'));
    expect(calls.length).toBe(2);
    for (const call of calls) {
      expect(call, `动态 SQL 站点只允许 DELETE：${call}`).toMatch(/^\d+\s+DELETE\s/i);
    }
  });

  it('`meta.changes` 只有两处消费点（account.ts 的两条 DELETE 计数）', () => {
    const sites = findSites(SOURCES, /\.meta\s*\??\.\s*changes/g);
    expect(sites).toEqual(CHANGES_SITES);
  });

  it('`rowCount` 只出现在 src/pg/d1-shim.ts 内（业务代码不直接碰 pg 的 rowCount）', () => {
    const sites = findSites(SOURCES, /\browCount\b/g);
    expect(sites.every((site) => site.startsWith('pg/d1-shim.ts:'))).toBe(true);
    expect(sites.length).toBeGreaterThan(0);
  });

  it('归一化只有一个实现点：PgCompatStatement.run()；PgCompatDatabase 不暴露 run()', () => {
    const shim = SOURCES.find((source) => source.file === 'pg/d1-shim.ts');
    expect(shim).toBeDefined();
    const text = shim!.text;
    const stmtStart = text.indexOf('export class PgCompatStatement');
    const dbStart = text.indexOf('export class PgCompatDatabase');
    expect(stmtStart).toBeGreaterThan(-1);
    expect(dbStart).toBeGreaterThan(stmtStart);

    const inStatement = text.slice(stmtStart, dbStart);
    const inDatabase = text.slice(dbStart);
    // 整个 shim 里 `async run(` 只应出现一次，且落在 PgCompatStatement 上。
    expect((text.match(/async run\s*\(/g) ?? []).length).toBe(1);
    expect((inStatement.match(/async run\s*\(/g) ?? []).length).toBe(1);
    expect((inDatabase.match(/async run\s*\(/g) ?? []).length).toBe(0);
    // 兼容层只复刻 D1Database 被真正消费的那一小块面：prepare + batch，没有 exec / dump。
    expect(inDatabase).toContain('prepare(');
    expect(inDatabase).toContain('batch(');
    expect(inDatabase).not.toContain('exec(');
    expect(inDatabase).not.toContain('dump(');
  });

  it('D1 专有的 exec() 面确实未被消费：`.raw()` / `.last_row_id` 全仓 0 命中', () => {
    expect(findSites(SOURCES, /\.raw\s*\(/g)).toEqual([]);
    expect(findSites(SOURCES, /\.last_row_id\b/g)).toEqual([]);
  });
});

describe('W-5.4 · `.first()` 站点逐点核实', () => {
  const firstSites = findSites(SOURCES, /\.first(?=\s*(<|\())/g);

  it('站点 23 个，逐点定位固定', () => {
    expect(firstSites).toEqual(FIRST_SITES);
  });

  it('无一例传列名（传列名时 shim 返回标量、D1 也返回标量，但本仓一律按整行对象用）', () => {
    const withColumn: string[] = [];
    for (const source of SOURCES) {
      const re = /\.first\s*(<[^>]*>)?\s*\(/g;
      let match: RegExpExecArray | null;
      while ((match = re.exec(source.text)) !== null) {
        const after = source.text.slice(match.index, match.index + match[0].length + 40);
        const close = after.indexOf(')');
        const args = close < 0 ? after : after.slice(0, close);
        if (/[,(]\s*['"`]/.test(args)) {
          withColumn.push(`${source.file}:${lineOf(source.text, match.index)}`);
        }
      }
    }
    console.log('[W-5.4] 传列名的 .first( 站点：' + JSON.stringify(withColumn));
    expect(withColumn).toEqual([]);
  });
});

describe('W-5.5 · `.batch()` 业务调用点逐点枚举', () => {
  const batchSites = findSites(SOURCES, /\.batch\s*\(/g);

  it('业务调用点 5 个，逐点定位固定', () => {
    expect(batchSites).toEqual(BATCH_SITES);
  });

  it('5 个调用点分别落在：设备撤销 / 条目推送 / 条目拉取自愈 / 桶推送 / 桶拉取自愈', () => {
    const table = BATCH_SITES.map((site) => {
      const [file, line] = site.split(':') as [string, string];
      const source = SOURCES.find((entry) => entry.file === file);
      expect(source).toBeDefined();
      const start = Number(line) - 1;
      const snippet = source!.text.split('\n').slice(start, start + 3).join(' ').replace(/\s+/g, ' ');
      return `${site}  ${snippet.slice(0, 96)}`;
    });
    console.log('[W-5.5] .batch( 业务调用点：\n' + table.join('\n'));
    expect(table.length).toBe(5);
  });
});
