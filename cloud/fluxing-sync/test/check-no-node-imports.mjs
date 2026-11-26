/**
 * 硬断言：`src/**` 里**零处静态 Node 内置模块导入**。
 *
 * 为什么需要它：`tsconfig.json` 的 `types` 同时含 `@cloudflare/workers-types` 与 `node`
 * （为了让 `test-pg/**` 里的 `process.env` / `node:fs` 有类型）⇒ `src/**` 也顺带看得见
 * node 全局类型 ⇒ 若有人在 Workers 侧业务代码里写静态 `import ... from 'node:fs'`，
 * **类型闸门不会拦**，但部署到 Cloudflare Workers 时运行期直接炸。
 *
 * 这个脚本就是那道被抵消的闸门：`src/**` 出现静态 `node:` 说明符 ⇒ 非零退出。
 *
 * **允许的既定模式（不要误伤）**：`src/**` 里存在 `dynamicImport('node:http')` /
 * `dynamicImport('node:fs/promises')` 这类**经 loader 间接加载**的写法
 * （见 `src/pg/dynamic-import.ts` 的设计说明：静态字面量 import 会让 Workers 侧
 * 构建把 node 内置模块打进包里）。它们不是静态导入，Workers 侧不打包 ⇒ 本检查放行。
 * 反过来，**字面量 `import('node:...')` / `require('node:...')` 是被禁的**（会绕过 loader）。
 *
 * 允许的例外（白名单，逐条有理由）：
 *   · `src/pg/**` —— 自建 PostgreSQL 路线的实现，**设计上**跑在 Node 上。
 *
 * 用法：`npm run check:no-node`（已串进 `npm run check`）。
 */

import { readdirSync, readFileSync, statSync } from 'node:fs';
import { join, relative, sep } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = fileURLToPath(new URL('..', import.meta.url));
const SRC = join(ROOT, 'src');

/** 允许出现静态 `node:` 导入的目录（相对仓库根，POSIX 风格）。 */
const ALLOWED_DIRS = ['src/pg'];

/**
 * 匹配**静态** node: 说明符：`from 'node:x'` / `import('node:x')` / `require('node:x')`。
 * 注意：`dynamicImport('node:x')` 不匹配（大小写不同且无 `import(` 紧邻字面量）⇒ 放行。
 */
const NODE_SPECIFIER = /(?:\bfrom\s*|\bimport\s*\(\s*|\brequire\s*\(\s*)['"](node:[^'"]+)['"]/g;

function walk(dir) {
  const out = [];
  for (const entry of readdirSync(dir)) {
    const full = join(dir, entry);
    if (statSync(full).isDirectory()) {
      out.push(...walk(full));
    } else if (entry.endsWith('.ts')) {
      out.push(full);
    }
  }
  return out;
}

function isAllowed(relPosix) {
  return ALLOWED_DIRS.some((prefix) => relPosix === prefix || relPosix.startsWith(`${prefix}/`));
}

const files = walk(SRC).sort();
const hits = [];
const indirect = [];

for (const file of files) {
  const relPosix = relative(ROOT, file).split(sep).join('/');
  const text = readFileSync(file, 'utf8');
  const lines = text.split(/\r?\n/);
  lines.forEach((line, index) => {
    const trimmed = line.trim();
    const isComment = trimmed.startsWith('//') || trimmed.startsWith('*') || trimmed.startsWith('/*');
    // 既定模式：经 loader 间接加载（放行，但记数，便于核对）。
    const indirectHit = /\bdynamicImport\s*\(\s*['"](node:[^'"]+)['"]/.exec(line);
    if (indirectHit && !isComment) {
      indirect.push({ file: relPosix, line: index + 1, specifier: indirectHit[1] });
    }
    if (isAllowed(relPosix)) return;
    if (isComment) return;
    NODE_SPECIFIER.lastIndex = 0;
    let match;
    while ((match = NODE_SPECIFIER.exec(line)) !== null) {
      hits.push({ file: relPosix, line: index + 1, specifier: match[1] });
    }
  });
}

if (hits.length > 0) {
  console.error('检查未通过：`src/**` 出现静态 Node 内置模块导入（Workers 路线运行期会炸）。');
  for (const hit of hits) {
    console.error(`  ${hit.file}:${hit.line} → ${hit.specifier}`);
  }
  console.error('白名单目录（允许 node: 导入）：' + ALLOWED_DIRS.join('、'));
  process.exit(1);
}

console.log(
  `检查通过：src/** 零处静态 node: 内置模块导入（扫描 ${files.length} 个 .ts 文件；` +
    `白名单 ${ALLOWED_DIRS.join('、')}；经 loader 间接加载 ${indirect.length} 处，放行）。`,
);
process.exit(0);
