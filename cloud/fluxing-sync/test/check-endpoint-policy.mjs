#!/usr/bin/env node
// T103 红线守卫（docs/research/fluxing-cloud-2026-11/endpoint-reachability.md）：
//   境内 DNS 会污染 Cloudflare 的默认免费子域，生产端点只能是
//   https://api.<我方自有域名>/v1/...，因此本目录任何文件都不得出现该默认子域的域名字面量，
//   并且 wrangler.toml 必须显式关闭它。
//
// 本脚本自身也在这个目录里，所以禁用词用字符串拼接构造，避免自我命中。

import { readdir, readFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = fileURLToPath(new URL('..', import.meta.url));
const FORBIDDEN = ['workers', 'dev'].join('.');
const SKIP_DIRS = new Set(['node_modules', '.wrangler', '.git', 'dist', 'coverage']);
const SKIP_FILES = new Set(['package-lock.json', 'pnpm-lock.yaml']);
const TEXT_EXT = new Set([
  '',
  '.ts',
  '.mts',
  '.mjs',
  '.js',
  '.json',
  '.toml',
  '.sql',
  '.md',
  '.txt',
  '.yml',
  '.yaml',
  '.example',
]);

/** @type {{ file: string; line: number }[]} */
const hits = [];

async function walk(dir) {
  const entries = await readdir(dir, { withFileTypes: true });
  for (const entry of entries) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      if (SKIP_DIRS.has(entry.name)) continue;
      await walk(full);
      continue;
    }
    if (!entry.isFile()) continue;
    if (SKIP_FILES.has(entry.name)) continue;
    if (!TEXT_EXT.has(path.extname(entry.name))) continue;

    const text = await readFile(full, 'utf8');
    if (!text.includes(FORBIDDEN)) continue;

    const rel = path.relative(ROOT, full).split(path.sep).join('/');
    text.split(/\r?\n/).forEach((content, index) => {
      if (content.includes(FORBIDDEN)) hits.push({ file: rel, line: index + 1 });
    });
  }
}

function fail(message) {
  console.error(`[T103 红线] ${message}`);
  process.exitCode = 1;
}

await walk(ROOT);

if (hits.length > 0) {
  console.error('[T103 红线] 发现 Cloudflare 默认子域的域名字面量（境内会被 DNS 污染）：');
  for (const hit of hits) console.error(`  ${hit.file}:${hit.line}`);
  fail('请改成 https://api.<我方自有域名>/v1/... 形态，并保留 workers_dev = false。');
} else {
  const wranglerPath = path.join(ROOT, 'wrangler.toml');
  let wrangler = '';
  try {
    wrangler = await readFile(wranglerPath, 'utf8');
  } catch {
    fail('缺少 wrangler.toml，无法确认默认端点已关闭。');
  }

  if (process.exitCode !== 1) {
    if (!/^\s*workers_dev\s*=\s*false\s*$/m.test(wrangler)) {
      fail('wrangler.toml 必须显式 `workers_dev = false`（否则会暴露被污染的公网子域）。');
    } else if (!/^\s*preview_urls\s*=\s*false\s*$/m.test(wrangler)) {
      fail('wrangler.toml 必须显式 `preview_urls = false`（预览地址同样是该默认子域）。');
    } else {
      console.log('[T103 红线] 通过：目录内无默认子域字面量，workers_dev / preview_urls 均已关闭。');
    }
  }
}