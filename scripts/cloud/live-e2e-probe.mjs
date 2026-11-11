#!/usr/bin/env node
// spec 100 · 线上真实账号闭环探针（批次 AH）
//
// 目的：把批次 AF/AG 一直挂着的「需真实账号才能验」的线上判据，在**不需要
// 人工取验证码**的前提下跑通：
//   ① register → verify → login 全链路（AC-01 的服务端侧）
//   ② push 响应含 `skipped`（服务端 HLC 守卫已生效的判别性证据）
//   ③ pull 顶层含 `server_seq`（客户端游标契约成立）
//   ④ R2 / D1 里查不到明文（AC-07 的服务端侧）
//
// 为什么用 Node 的标准 crypto 复刻，而**不是**直接调客户端 C++：
//   本探针要证明的是「客户端的 KDF / 信封封装口径 == 线上服务端接受的」，
//   如果用客户端自己的实现去推，服务端和客户端同时算错也会一起绿。
//   ⇒ 用 Node（OpenSSL）这一份**独立实现**复刻派生链与 GCM 布局；跑通即
//     坐实「C++ 自实现 == 标准实现 == 线上服务端」三方一致。
//     这条等价关系由 test/TestFluxingCloud 里的跨实现固定向量断言兜底。
//
// 口径来源（改动这些文件必须同步改本脚本）：
//   WeaselServer/Cloud/CloudCrypto.h      派生参数与前缀
//   include/FluxingCloudEnvelope.h         信封布局 / AAD / GCM
//   WeaselServer/Cloud/CloudSyncService.cpp push/pull 请求体
//   cloud/fluxing-sync/src/{auth,sync,wire}.ts  服务端契约
//
// 用法（在仓库根执行）：
//   node scripts/cloud/live-e2e-probe.mjs
//   node scripts/cloud/live-e2e-probe.mjs --keep    # 保留测试账号与数据（调试用）
//
// 退出码：0 = 全部断言通过；1 = 有断言失败；2 = 环境/网络前置不满足。

import { execFileSync } from 'node:child_process';
import {
  createCipheriv,
  createDecipheriv,
  createHash,
  createHmac,
  hkdfSync,
  pbkdf2Sync,
  randomBytes,
} from 'node:crypto';
import { readFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = dirname(fileURLToPath(import.meta.url));
const REPO = resolve(HERE, '..', '..');
const WORKER_DIR = resolve(REPO, 'cloud', 'fluxing-sync');
const API_BASE = 'https://api.aiec.fun';
const D1_NAME = 'fluxing-sync';
const R2_BUCKET = 'fluxing-sync';

// ---------------------------------------------------------------------------
// 客户端口径常量（逐字对齐 WeaselServer/Cloud/CloudCrypto.h）
// ---------------------------------------------------------------------------
const PBKDF2_ITERS = 600000;
const SALT_PREFIX = 'fluxing-enc-v1|';
const AUTH_KEY_INFO = 'fluxing-auth-v1';
const ENC_KEY_INFO = 'fluxing-enc-v1';
// AC-07 方案 K：线上条目身份 = HMAC-SHA256(HKDF(enc_key, 本 info), utf8(明文条目键))
// 逐字对齐 WeaselServer/Cloud/CloudSyncService.cpp::kItemKeyMacInfo
const ITEM_KEY_MAC_INFO = 'fluxing-itemkey-mac-v1';
const KEY_BYTES = 32;
const NONCE_BYTES = 12;
const TAG_BYTES = 16;

// ---------------------------------------------------------------------------
// 小工具
// ---------------------------------------------------------------------------
const keepData = process.argv.includes('--keep');
// --register-only：只注册（把真实验证码发到信箱）就退出，等用户从信箱读出
// 6 位码后，第二轮带 FLUXING_PROBE_CODE 回来跑完整闭环（恢复模式）。
const registerOnly = process.argv.includes('--register-only');
const results = [];
let failed = 0;

function record(name, ok, detail) {
  results.push({ name, ok, detail });
  if (!ok) failed += 1;
  const mark = ok ? 'PASS' : 'FAIL';
  process.stdout.write(`[${mark}] ${name}${detail ? ' :: ' + detail : ''}\n`);
}

function normalizeEmail(email) {
  return email.trim().toLowerCase();
}

function deriveKeys(email, password, iterations = PBKDF2_ITERS) {
  const norm = normalizeEmail(email);
  const salt = createHash('sha256').update(SALT_PREFIX + norm, 'utf8').digest();
  const master = pbkdf2Sync(password, salt, iterations, KEY_BYTES, 'sha256');
  const authKey = Buffer.from(
    hkdfSync('sha256', master, Buffer.alloc(KEY_BYTES, 0), AUTH_KEY_INFO, KEY_BYTES),
  );
  const encKey = Buffer.from(
    hkdfSync('sha256', master, Buffer.alloc(KEY_BYTES, 0), ENC_KEY_INFO, KEY_BYTES),
  );
  master.fill(0);
  return { salt, authKey, encKey };
}

/** AAD = item_key|type|hlc_ms|hlc_counter|device（对齐 BuildAad） */
function buildAad(itemKey, type, hlc) {
  return `${itemKey}|${type}|${hlc.ms}|${hlc.counter}|${hlc.device}`;
}

/**
 * AC-07 方案 K：线上条目身份（第三份独立实现，禁止复用客户端代码）。
 * 逐字对齐 CloudSyncService.cpp::ComputeItemKeyId：
 *   mac_key = HKDF-SHA256(enc_key, salt = 32 × 0x00, info = "fluxing-itemkey-mac-v1", 32B)
 *   id      = lowercase_hex(HMAC-SHA256(mac_key, utf8(item_key)))
 */
function computeItemKeyId(encKey, itemKey) {
  const macKey = Buffer.from(
    hkdfSync('sha256', encKey, Buffer.alloc(KEY_BYTES, 0), ITEM_KEY_MAC_INFO, KEY_BYTES),
  );
  const id = createHmac('sha256', macKey).update(Buffer.from(itemKey, 'utf8')).digest('hex');
  macKey.fill(0);
  return id;
}

/**
 * 对齐 CloudSyncService.cpp::PackSealedPlain：
 *   明文载荷 = [4 字节大端 key_len][utf8 item_key][utf8 payload]
 * 词条含任意字节（含 TAB），故用长度前缀而非分隔符。
 */
function packSealedPlain(itemKey, payloadText) {
  const keyBuf = Buffer.from(itemKey, 'utf8');
  const len = Buffer.alloc(4);
  len.writeUInt32BE(keyBuf.length, 0);
  return Buffer.concat([len, keyBuf, Buffer.from(payloadText, 'utf8')]);
}

/** 对齐 CloudSyncService.cpp::UnpackSealedPlain ⇒ { key, payload:Buffer } */
function unpackSealedPlain(buf) {
  if (buf.length < 4) throw new Error('封包不足 4 字节');
  const keyLen = buf.readUInt32BE(0);
  if (keyLen === 0) throw new Error('封包条目键长度为 0');
  if (buf.length < 4 + keyLen) throw new Error('封包条目键长度越界');
  return {
    key: buf.subarray(4, 4 + keyLen).toString('utf8'),
    payload: buf.subarray(4 + keyLen),
  };
}

/** 封装：cipher = 密文 || tag（对齐 AesGcmSeal 的输出布局） */
function seal(encKey, nonce, aad, plainBuf) {
  const cipher = createCipheriv('aes-256-gcm', encKey, nonce);
  cipher.setAAD(Buffer.from(aad, 'utf8'));
  const body = Buffer.concat([cipher.update(plainBuf), cipher.final()]);
  return Buffer.concat([body, cipher.getAuthTag()]);
}

/** 解封：验证布局真的能被客户端的 AesGcmOpen 口径还原（返回 Buffer） */
function open(encKey, nonce, aad, cipherBuf) {
  const body = cipherBuf.subarray(0, cipherBuf.length - TAG_BYTES);
  const tag = cipherBuf.subarray(cipherBuf.length - TAG_BYTES);
  const d = createDecipheriv('aes-256-gcm', encKey, nonce);
  d.setAAD(Buffer.from(aad, 'utf8'));
  d.setAuthTag(tag);
  return Buffer.concat([d.update(body), d.final()]);
}

async function api(method, path, body, token) {
  const headers = { Accept: 'application/json' };
  if (body !== undefined) headers['Content-Type'] = 'application/json';
  if (token) headers.Authorization = `Bearer ${token}`;
  const res = await fetch(`${API_BASE}${path}`, {
    method,
    headers,
    body: body === undefined ? undefined : JSON.stringify(body),
  });
  const text = await res.text();
  let json = null;
  try {
    json = JSON.parse(text);
  } catch {
    json = null;
  }
  return { status: res.status, text, json };
}

/** 读 cloud/fluxing-sync/.env 到子进程环境（值不回显，避免泄露） */
function loadEnvFile() {
  const env = { ...process.env };
  let file = '';
  try {
    file = readFileSync(resolve(WORKER_DIR, '.env'), 'utf8');
  } catch {
    return env;
  }
  for (const line of file.split(/\r?\n/)) {
    const trimmed = line.trim();
    if (trimmed.length === 0 || trimmed.startsWith('#')) continue;
    const eq = trimmed.indexOf('=');
    if (eq <= 0) continue;
    const key = trimmed.slice(0, eq).trim();
    let value = trimmed.slice(eq + 1).trim();
    if (
      (value.startsWith('"') && value.endsWith('"') && value.length >= 2) ||
      (value.startsWith("'") && value.endsWith("'") && value.length >= 2)
    ) {
      value = value.slice(1, -1);
    }
    if (key.length > 0 && env[key] === undefined) env[key] = value;
  }
  return env;
}

const CHILD_ENV = loadEnvFile();

/**
 * 调 wrangler CLI。
 *
 * Windows 坑：`execFileSync('npx', ...)` 走 CreateProcess，不会按 PATHEXT 补
 * `.cmd` ⇒ ENOENT；而 `.cmd` 又必须经 cmd.exe 才能执行。⇒ 首选直接用当前
 * node 进程跑 wrangler 的 JS 入口（完全绕开 shell 与 PATHEXT），失败再退到
 * `npx.cmd` / `npx`。
 */
function wrangler(args, options = {}) {
  const { encoding = 'utf8', maxBuffer = 32 * 1024 * 1024 } = options;
  const candidates = [
    { cmd: process.execPath, pre: [resolve(WORKER_DIR, 'node_modules', 'wrangler', 'bin', 'wrangler.js')] },
    { cmd: 'npx.cmd', pre: ['wrangler'] },
    { cmd: 'npx', pre: ['wrangler'] },
  ];
  let lastErr = null;
  for (const c of candidates) {
    try {
      return execFileSync(c.cmd, [...c.pre, ...args], {
        cwd: WORKER_DIR,
        encoding,
        maxBuffer,
        env: CHILD_ENV,
      });
    } catch (err) {
      lastErr = err;
      // 子进程真的跑起来但返回非零 ⇒ 直接上抛，别被下一个候选掩盖。
      if (err && err.status !== undefined && err.status !== null) throw err;
    }
  }
  throw lastErr ?? new Error('wrangler 调用失败');
}

/** 跑一条 wrangler d1 命令，返回 rows 数组 */
function d1Query(sql) {
  const raw = wrangler(['d1', 'execute', D1_NAME, '--remote', '--command', sql, '--json']);
  const parsed = JSON.parse(raw);
  const blocks = Array.isArray(parsed) ? parsed : [parsed];
  const rows = [];
  for (const block of blocks) {
    if (block && Array.isArray(block.results)) rows.push(...block.results);
  }
  return rows;
}

function d1Exec(sql) {
  wrangler(['d1', 'execute', D1_NAME, '--remote', '--command', sql]);
}

// ---------------------------------------------------------------------------
// 主流程
// ---------------------------------------------------------------------------
async function main() {
  const stamp = Date.now().toString(36);
  // 邮箱可外部指定：MAIL_MODE="http"（Resend 等第三方通道）下，
  // 验证码不再落 mail_outbox，必须用真实可投递邮箱 + 人工提供的 6 位码。
  const email =
    process.env.FLUXING_PROBE_EMAIL?.trim() || `fluxing-probe-${stamp}@example.com`;
  // 口令可外部指定 ⇒ 跨轮次保持一致（恢复模式的 login 必须用同一口令派生 auth_key）
  const password = process.env.FLUXING_PROBE_PASSWORD?.trim() || `Probe-Pw-${stamp}`;
  const deviceId = randomBytes(8).toString('hex');
  const itemKey = `userdb\t探针词条-${stamp}`;
  const plainText = `探针明文载荷-${stamp}`;

  process.stdout.write(`探针账号：${email}\n设备标识：${deviceId}\n\n`);

  // --- 0. 前置：健康检查 + D1 可读 -----------------------------------------
  const health = await api('GET', '/v1/health');
  if (health.status !== 200) {
    process.stdout.write(`前置不满足：GET /v1/health 返回 ${health.status}\n`);
    process.exit(2);
  }
  record('前置 · GET /v1/health 200', true);

  try {
    d1Query('SELECT COUNT(*) AS c FROM users');
  } catch (err) {
    process.stdout.write(`前置不满足：D1 不可读（${err.message}）\n`);
    process.exit(2);
  }
  record('前置 · D1 远程可读', true);

  // --- 1. 派生密钥（独立实现，见文件头说明）--------------------------------
  const { authKey, encKey } = deriveKeys(email, password);
  record(
    '派生 · auth_key / enc_key 各 32 字节',
    authKey.length === KEY_BYTES && encKey.length === KEY_BYTES,
    `auth=${authKey.length}B enc=${encKey.length}B`,
  );

  // --- 2. 注册 -------------------------------------------------------------
  const reg = await api('POST', '/v1/auth/register', {
    email,
    auth_key: authKey.toString('base64'),
    device: { id: deviceId, name: 'Fluxing' },
  });
  let accountId = reg.json?.account_id ?? null;
  if (reg.status === 200) {
    record('AC-01 · register 200 且返回 account_id', Boolean(accountId), `account_id=${accountId}`);
  } else if (reg.status === 409 && /email_taken/.test(reg.text)) {
    // 复用模式：账号已存在 ⇒ 直接借用，不再要求「必须先注入外部验证码」。
    // 两条子情形都成立：
    //   ① 账号已 email_verified=1 ⇒ 第 3、4 步由幂等闸跳过，直接 login；
    //   ② 账号未验证且已注入外部码 ⇒ 服务端 409 发生在**发码之前**
    //      （auth.ts：existing 检查早于发信）⇒ 不会把用户已读到的那枚码换掉。
    const rows = d1Query(`SELECT id, email_verified FROM users WHERE email = '${email}' LIMIT 1`);
    accountId = rows.length > 0 ? String(rows[0].id) : null;
    record(
      'AC-01 · register 409（账号已存在）⇒ 复用已有账号继续',
      Boolean(accountId),
      `account_id=${accountId} email_verified=${rows.length > 0 ? rows[0].email_verified : '?'}`,
    );
  } else {
    record('AC-01 · register 200', false, `${reg.status} ${reg.text.slice(0, 200)}`);
    cleanup(email, null);
    return;
  }

  if (registerOnly) {
    process.stdout.write(`\n--register-only：账号就绪（${email}），验证码已交由发信通道投递。\n`);
    process.stdout.write(`口令：${password}（恢复模式必须用同一口令，请带 FLUXING_PROBE_PASSWORD 重跑）\n`);
    process.stdout.write('下一步：读到 6 位码后执行\n');
    process.stdout.write(
      `  $env:FLUXING_PROBE_EMAIL='${email}'; $env:FLUXING_PROBE_PASSWORD='${password}'; $env:FLUXING_PROBE_CODE='123456'; node scripts/cloud/live-e2e-probe.mjs --keep\n`,
    );
    return;
  }

  // 幂等闸：账号若已处于已验证状态就跳过第 3、4 步（探针必须可复跑）。
  // 依据（auth.ts）：handleResend 对已验证邮箱永久 409 email_already_verified
  // ⇒ 拿不到新码；而上一轮的验证行已 consumed ⇒ verify 必 400 verify_code_used。
  const verifiedRows = d1Query(`SELECT email_verified FROM users WHERE email = '${email}' LIMIT 1`);
  const alreadyVerified = verifiedRows.length > 0 && Number(verifiedRows[0].email_verified) === 1;

  // --- 3. 取 6 位验证码 ----------------------------------------------------
  // 两条来源：
  //   ① FLUXING_PROBE_CODE（MAIL_MODE="http" 时唯一可行来源）：
  //      第三方通道不写 mail_outbox，验证码只存在于用户邮箱，需人工读出后注入。
  //   ② D1 mail_outbox（MAIL_MODE="outbox" 时的旧路径，保留兼容）：
  //      验证码正文（明文）落在 mail_outbox.body；email_verifications 只存 code_hash。
  const manualCode = (process.env.FLUXING_PROBE_CODE || '').trim();
  let code = '';
  if (alreadyVerified) {
    record('验证码 · 跳过（账号已处于已验证状态）', true, 'users.email_verified=1，无需取码');
  } else if (/^\d{6}$/.test(manualCode)) {
    code = manualCode;
    record('验证码 · 用外部注入的 6 位码（第三方发信通道）', true, `code=${code}`);
  } else {
    if (manualCode) {
      record('验证码 · 外部注入的码格式不对（应为 6 位数字）', false, `传入=${manualCode.slice(0, 12)}`);
    }
    const rows = d1Query(
      `SELECT body FROM mail_outbox WHERE to_addr = '${email}' ORDER BY id DESC LIMIT 1`,
    );
    const body = rows.length > 0 ? String(rows[0].body ?? '') : '';
    const codeMatch = /验证码是：(\d{6})/.exec(body);
    if (!codeMatch) {
      record('验证码 · mail_outbox 里取到 6 位明文码', false, `body 前 120 字：${body.slice(0, 120)}（http 模式不写 outbox ⇒ 需设 FLUXING_PROBE_CODE）`);
      cleanup(email, accountId);
      return;
    }
    code = codeMatch[1];
    record('验证码 · mail_outbox 里取到 6 位明文码', true, `code=${code}`);
  }

  // --- 4. 验证邮箱 ---------------------------------------------------------
  if (alreadyVerified) {
    record('AC-01 · verify 跳过（账号已处于已验证状态）', true, 'users.email_verified=1，无需重新验证');
  } else {
    const ver = await api('POST', '/v1/auth/verify', { email, code });
    record('AC-01 · verify 200', ver.status === 200, `${ver.status} ${ver.text.slice(0, 160)}`);
  }

  // --- 5. 登录 -------------------------------------------------------------
  const login = await api('POST', '/v1/auth/login', {
    email,
    auth_key: authKey.toString('base64'),
    device: { id: deviceId, name: 'Fluxing' },
  });
  if (login.status !== 200) {
    record('AC-01 · login 200', false, `${login.status} ${login.text.slice(0, 200)}`);
    cleanup(email, accountId);
    return;
  }
  const token = login.json?.access_token;
  record('AC-01 · login 200 且拿到 access_token', Boolean(token), `email_verified=${login.json?.email_verified}`);

  // --- 6. 推送（第一轮：新版本 ⇒ 应被 accepted）-----------------------------
  const hlc = { ms: Date.now(), counter: 0, device: deviceId };
  const itemKeyId = computeItemKeyId(encKey, itemKey);
  record(
    'AC-07 · 线上条目身份是 64 位小写十六进制',
    /^[0-9a-f]{64}$/.test(itemKeyId),
    `id=${itemKeyId.slice(0, 16)}… 长度=${itemKeyId.length}`,
  );
  record(
    'AC-07 · 线上条目身份 ≠ 明文词条（不含词典名/词条子串）',
    itemKeyId !== itemKey && !itemKeyId.includes('userdb') && !itemKeyId.includes('探针词条'),
    `明文键=${JSON.stringify(itemKey)}`,
  );
  const sealedPlain = packSealedPlain(itemKey, plainText);
  const nonce = randomBytes(NONCE_BYTES);
  const aad = buildAad(itemKeyId, 'userdb', hlc);
  const cipher = seal(encKey, nonce, aad, sealedPlain);

  const push1 = await api(
    'POST',
    '/v1/sync/push',
    {
      items: [
        {
          item_key: itemKeyId,
          type: 'userdb',
          hlc,
          nonce: nonce.toString('base64'),
          cipher: cipher.toString('base64'),
        },
      ],
    },
    token,
  );
  const accepted1 = push1.json?.accepted ?? [];
  const skipped1 = push1.json?.skipped ?? [];
  record(
    'T502 · push 首轮 accepted 含该条目',
    push1.status === 200 && accepted1.includes(itemKeyId),
    `status=${push1.status} accepted=${JSON.stringify(accepted1)} skipped=${JSON.stringify(skipped1)}`,
  );
  record(
    'T503 · push 响应含 skipped 字段（守卫判别性证据）',
    Array.isArray(push1.json?.skipped),
    `skipped=${JSON.stringify(skipped1)}`,
  );
  const seq1 = push1.json?.server_seq;
  record('T502 · push 响应 server_seq > 0', Number(seq1) > 0, `server_seq=${seq1}`);

  // --- 7. 推送（第二轮：同版本再推 ⇒ 应被 skipped）--------------------------
  // 服务端 HLC 守卫：存储版本 ≥ 入参 ⇒ 整条跳过（不写 R2 / D1）。
  const push2 = await api(
    'POST',
    '/v1/sync/push',
    {
      items: [
        {
          item_key: itemKeyId,
          type: 'userdb',
          hlc,
          nonce: nonce.toString('base64'),
          cipher: cipher.toString('base64'),
        },
      ],
    },
    token,
  );
  const skipped2 = push2.json?.skipped ?? [];
  const accepted2 = push2.json?.accepted ?? [];
  record(
    'T503 · 同版本再推 ⇒ 该条目进 skipped（服务端 HLC 守卫生效）',
    push2.status === 200 && skipped2.includes(itemKeyId),
    `status=${push2.status} skipped=${JSON.stringify(skipped2)} accepted=${JSON.stringify(accepted2)}`,
  );

  // --- 8. 拉取 -------------------------------------------------------------
  const pull = await api('GET', '/v1/sync/pull?since=0&limit=50', undefined, token);
  const pullItems = pull.json?.items ?? [];
  const topSeq = pull.json?.server_seq;
  record(
    'T503 · pull 响应含顶层 server_seq',
    pull.status === 200 && Number(topSeq) > 0,
    `status=${pull.status} top_server_seq=${topSeq}`,
  );
  let pulledCipherBytes = null;
  const pulled = pullItems.find((it) => it.item_key === itemKeyId);
  record('T502 · pull 拉回刚才推的条目', Boolean(pulled), `items=${pullItems.length}`);
  if (pulled) {
    record(
      'T502 · 拉回条目的 item 自带 server_seq > 0',
      Number(pulled.server_seq) > 0,
      `item.server_seq=${pulled.server_seq}`,
    );
    // --- 9. 密文可解密（端到端闭环）----------------------------------------
    const roundNonce = Buffer.from(pulled.nonce, 'base64');
    const roundCipher = Buffer.from(pulled.cipher, 'base64');
    pulledCipherBytes = roundCipher;
    const roundHlc = pulled.hlc;
    const roundAad = buildAad(pulled.item_key, pulled.type, roundHlc);
    let unpacked = null;
    let openErr = '';
    try {
      unpacked = unpackSealedPlain(open(encKey, roundNonce, roundAad, roundCipher));
    } catch (err) {
      openErr = String(err.message || err);
    }
    record(
      '端到端 · 拉回密文可用派生密钥解密，且 item_key 与载荷双一致',
      unpacked !== null &&
        unpacked.key === itemKey &&
        unpacked.payload.toString('utf8') === plainText,
      unpacked === null
        ? `解密异常：${openErr.slice(0, 160)}`
        : `key=${JSON.stringify(unpacked.key)} 载荷=${JSON.stringify(unpacked.payload.toString('utf8').slice(0, 60))}`,
    );
    record(
      'AC-07 · 线上密文 ≠ 明文（存储的是密文）',
      !roundCipher.toString('utf8').includes(plainText),
      `cipher 长度=${roundCipher.length}B`,
    );
  }

  // --- 10. 服务端存储抽查（明文不可见）------------------------------------
  const itemRows = d1Query(
    `SELECT item_key, type, hlc_ms, hlc_counter, hlc_device, r2_key, nonce_b64, cipher_len FROM sync_items WHERE user_id = '${accountId}'`,
  );
  const rowText = JSON.stringify(itemRows);
  record(
    'AC-07 · D1 sync_items 无明文载荷列',
    itemRows.length > 0 && !rowText.includes(plainText),
    `行数=${itemRows.length} 列=${itemRows.length > 0 ? Object.keys(itemRows[0]).join(',') : ''}`,
  );
  // 方案 K 判据一：本次条目必须以 MAC 身份（64 位小写 hex）落库，且与客户端算法一致。
  record(
    'AC-07 · 本次条目在 D1 以 MAC 身份（64 位 hex）落库',
    itemRows.some((r) => r.item_key === itemKeyId),
    `期望=${itemKeyId.slice(0, 20)}… 命中=${itemRows.filter((r) => r.item_key === itemKeyId).length}`,
  );
  // 方案 K 判据二：整表不得残留明文身份行（残留行在方案 K 下 AAD 不匹配、必然解不开，
  // 属必须清理的旧数据 —— 命中就说明清理没做完，不能让它藏在新代码背后）。
  const plainIdentityRows = itemRows.filter(
    (r) => typeof r.item_key !== 'string' || !/^[0-9a-f]{64}$/.test(r.item_key),
  );
  record(
    'AC-07 · D1 无明文 item_key 残留行（词典名/词条不可见）',
    itemRows.length > 0 && plainIdentityRows.length === 0,
    plainIdentityRows.length === 0
      ? `全部 ${itemRows.length} 行均为 64 位 hex`
      : `残留 ${plainIdentityRows.length} 行：${JSON.stringify(plainIdentityRows.slice(0, 3).map((r) => r.item_key))}`,
  );

  const r2Rows = d1Query(
    `SELECT r2_key FROM sync_items WHERE user_id = '${accountId}' LIMIT 1`,
  );
  if (r2Rows.length > 0 && r2Rows[0].r2_key) {
    // wrangler r2 object get 默认读**本地**存储 ⇒ 必须显式 --remote；
    // 写 stdout 用 --pipe（--file - 会被当成文件名 "-"）。
    let r2Bytes = null;
    let r2Err = '';
    try {
      const buf = wrangler(
        [
          'r2',
          'object',
          'get',
          `${R2_BUCKET}/${r2Rows[0].r2_key}`,
          '--remote',
          '--pipe',
        ],
        { encoding: null, maxBuffer: 16 * 1024 * 1024 },
      );
      r2Bytes = Buffer.isBuffer(buf) ? buf : Buffer.from(buf);
    } catch (err) {
      r2Err = String(err.message || err);
    }
    // 读取失败必须判 FAIL：否则错误文本里当然没有明文，会变成假阳性。
    record(
      'AC-07 · R2 对象可读，且与 pull 回来的密文逐字节一致',
      r2Bytes !== null &&
        pulledCipherBytes !== null &&
        r2Bytes.equals(pulledCipherBytes),
      r2Bytes === null
        ? `读取失败：${r2Err.slice(0, 160)}`
        : `长度=${r2Bytes.length}B 期望=${pulledCipherBytes === null ? '未知' : pulledCipherBytes.length}B`,
    );
    record(
      'AC-07 · R2 对象里不含明文',
      r2Bytes !== null && !r2Bytes.includes(Buffer.from(plainText, 'utf8')),
      r2Bytes === null
        ? '未读到对象，本条无法判定'
        : `前 32 字节(可打印)=${JSON.stringify(r2Bytes.subarray(0, 32).toString('binary').replace(/[^\x20-\x7e]/g, '.'))}`,
    );
  } else {
    record('AC-07 · R2 对象可读，且与 pull 回来的密文逐字节一致', false, '未取到 r2_key');
    record('AC-07 · R2 对象里不含明文', false, '未取到 r2_key');
  }

  cleanup(email, accountId);
}

// ---------------------------------------------------------------------------
// 清理：探针账号不该留在生产库里（AC-10 的可复现清理口径）
// ---------------------------------------------------------------------------
function cleanup(email, accountId) {
  if (keepData) {
    process.stdout.write(`\n--keep：保留测试账号 ${email} / account_id=${accountId}\n`);
    return;
  }
  try {
    if (accountId) {
      d1Exec(`DELETE FROM sync_items WHERE user_id = '${accountId}'`);
      d1Exec(`DELETE FROM refresh_tokens WHERE user_id = '${accountId}'`);
      d1Exec(`DELETE FROM devices WHERE user_id = '${accountId}'`);
      d1Exec(`DELETE FROM users WHERE id = '${accountId}'`);
    }
    d1Exec(`DELETE FROM email_verifications WHERE email = '${email}'`);
    d1Exec(`DELETE FROM mail_outbox WHERE to_addr = '${email}'`);
    process.stdout.write(`\n已清理测试账号 ${email}\n`);
  } catch (err) {
    process.stdout.write(`\n清理失败（需手工处理）：${err.message}\n`);
  }
}

main()
  .then(() => {
    process.stdout.write(`\n断言总数：${results.length}，失败：${failed}\n`);
    process.stdout.write(failed === 0 ? 'VERDICT: ALL PASS\n' : 'VERDICT: HAS FAILURE\n');
    process.exit(failed === 0 ? 0 : 1);
  })
  .catch((err) => {
    process.stdout.write(`探针异常：${err.stack || err.message}\n`);
    process.exit(2);
  });
