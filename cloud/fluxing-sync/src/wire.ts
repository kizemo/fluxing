// 信封字段的线格式校验与规范化。
//
// 契约基准：include/FluxingCloudEnvelope.h（commit e395bda）的 8 行文本格式——
//   v1 / item_key / type / hlc_ms / hlc_counter / device / nonce / cipher
// 服务端**不解析**这 8 行文本，只按逐字段对应的 JSON 存取；对齐点是字段名 + 取值域。
// 因此本文件的每个常量都必须与头文件里的同名常量逐字对齐。

import { fail } from './errors';
import { fromBase64Strict, toBase64 } from './crypto';

/** FluxingCloudEnvelope.h: kNonceBytes */
export const NONCE_BYTES = 12;
/** FluxingCloudEnvelope.h: kTagBytes（GCM 认证标签） */
export const TAG_BYTES = 16;
/** FluxingCloudEnvelope.h: kKeyBytes（仅作说明，服务端永不见密钥） */
export const KEY_BYTES = 32;
/** FluxingCloudEnvelope.h: kHlcCounterMax */
export const HLC_COUNTER_MAX = 0xffff;
/** FluxingCloudEnvelope.h: kMaxPayloadBytes —— 明文上限 8 MiB */
export const MAX_PAYLOAD_BYTES = 8 * 1024 * 1024;
/** 密文 = 明文 + 16 字节认证标签 */
export const MAX_CIPHER_BYTES = MAX_PAYLOAD_BYTES + TAG_BYTES;

/** FluxingCloudEnvelope.h: ItemType */
export const ITEM_TYPES = ['config', 'phrases', 'userdb'] as const;
export type ItemType = (typeof ITEM_TYPES)[number];

/** item_key 上限（UTF-8 字节）。服务端只用它做索引键，R2 键是它的 SHA-256。 */
export const MAX_ITEM_KEY_BYTES = 512;
/** device 标识上限（UTF-8 字节） */
export const MAX_DEVICE_BYTES = 256;
/** 单次 push 条目数上限（保护单请求写放大，plan §6.1 未规定，服务端自定） */
export const MAX_PUSH_ITEMS = 64;

export interface Hlc {
  ms: number;
  counter: number;
  device: string;
}

/**
 * device 的字典序比较：按 **UTF-8 字节** 比，不按 JS 的字符串 `<`。
 *
 * 理由：客户端 `FluxingCloudEnvelope.h` 的 `CompareHlc` 用 `std::string::operator<`，
 * 语义是「按字节无符号比较」。JS 的 `<` 比的是 UTF-16 码元，代理对区间
 * （U+10000 及以上）与 U+E000–U+FFFF 的相对次序在两种编码下不同 ⇒ 会与服务端
 * 判定出不同的赢家。生产 device 是 16 位十六进制（纯 ASCII，两种序一致），
 * 但服务端接受任意 UTF-8 device，所以这里必须按字节比才能与客户端逐字一致。
 */
function compareUtf8Bytes(a: string, b: string): number {
  if (a === b) return 0;
  const bytesA = new TextEncoder().encode(a);
  const bytesB = new TextEncoder().encode(b);
  const shared = Math.min(bytesA.length, bytesB.length);
  for (let i = 0; i < shared; i += 1) {
    const x = bytesA[i] as number;
    const y = bytesB[i] as number;
    if (x !== y) return x < y ? -1 : 1;
  }
  if (bytesA.length === bytesB.length) return 0;
  return bytesA.length < bytesB.length ? -1 : 1;
}

/**
 * HLC 全序比较，返回 -1 / 0 / 1。
 *
 * **逐字镜像** `include/FluxingCloudEnvelope.h` 的 `CompareHlc`：
 *   ms → counter → device（device 字典序 tiebreak）。
 * 头文件原句：「`ms` → `counter` → `device` 字典序，**大者胜**」。
 * 大者胜的语义落在调用方：本函数只回答「谁更新」。
 */
export function compareHlc(a: Hlc, b: Hlc): number {
  if (a.ms !== b.ms) return a.ms < b.ms ? -1 : 1;
  if (a.counter !== b.counter) return a.counter < b.counter ? -1 : 1;
  return compareUtf8Bytes(a.device, b.device);
}

export interface ValidatedItem {
  itemKey: string;
  type: ItemType;
  hlc: Hlc;
  nonceB64: string;
  nonceBytes: Uint8Array;
  cipherBytes: Uint8Array;
}

function isPlainObject(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

function byteLength(s: string): number {
  return new TextEncoder().encode(s).length;
}

/**
 * 信封头里的字符串字段共用的字符集约束：禁竖线（8 行格式的行内分隔符）、
 * 禁换行等控制字符（8 行格式的行分隔符）。
 *
 * **制表符（0x09）放行**：客户端的词典条目键就是 "<词典名>\t<词条>"
 * （WeaselServer/Cloud/CloudUserDictIo.h 的 MakeDictItemKey），TAB 既不是
 * 8 行格式的行分隔符、也不进 AAD 的分隔位（`|`）。若按「禁一切控制字符」
 * 处理，真机每一次 userdb / phrases 推送都会被 400 拒绝（批次 AH 线上探针
 * 实测：register→verify→login 全绿，首推即 400「不得包含控制字符或换行」）。
 */
function assertHeaderString(raw: unknown, field: string, maxBytes: number, label: string): string {
  if (typeof raw !== 'string') throw fail.badRequest(`${label}（${field}）必须是字符串。`);
  const value = raw;
  if (value.length === 0) throw fail.badRequest(`${label}（${field}）不能为空。`);
  // eslint-disable-next-line no-control-regex
  if (/[\u0000-\u0008\u000a-\u001f\u007f]/.test(value)) {
    throw fail.badRequest(`${label}（${field}）不得包含控制字符或换行。`);
  }
  if (value.includes('|')) {
    throw fail.badRequest(`${label}（${field}）不得包含竖线字符。`);
  }
  if (byteLength(value) > maxBytes) {
    throw fail.badRequest(`${label}（${field}）过长（上限 ${maxBytes} 字节）。`);
  }
  return value;
}

function toSafeInteger(raw: unknown, field: string, label: string): number {
  if (typeof raw !== 'number' || !Number.isInteger(raw) || !Number.isSafeInteger(raw)) {
    throw fail.badRequest(`${label}（${field}）必须是整数。`);
  }
  return raw;
}

/**
 * 解析 hlc。规范形态是 `hlc: { ms, counter, device }`；
 * 同时接受扁平别名 `hlc_ms` / `hlc_counter` / `device`，避免客户端拿错形态就整批失败。
 */
function parseHlc(raw: Record<string, unknown>, index: number): Hlc {
  const label = `第 ${index + 1} 个条目的 HLC`;
  const nested = raw.hlc;
  let container: Record<string, unknown>;
  if (isPlainObject(nested)) {
    container = nested;
  } else if (nested === undefined) {
    container = {
      ms: raw.hlc_ms,
      counter: raw.hlc_counter,
      device: raw.device,
    };
  } else {
    throw fail.badRequest(`${label} 必须是对象 { ms, counter, device }。`);
  }

  const ms = toSafeInteger(container.ms, 'ms', label);
  if (ms < 0) throw fail.badRequest(`${label} 的 ms 不能为负。`);

  const counter = toSafeInteger(container.counter, 'counter', label);
  if (counter < 0 || counter > HLC_COUNTER_MAX) {
    throw fail.badRequest(`${label} 的 counter 超出范围（0..${HLC_COUNTER_MAX}）。`);
  }

  const device = assertHeaderString(container.device, 'device', MAX_DEVICE_BYTES, label);
  return { ms, counter, device };
}

/** 校验单条待推送条目；任何字段不合法都抛 400 中文错误，并指明是第几条。 */
export function validatePushItem(raw: unknown, index: number): ValidatedItem {
  if (!isPlainObject(raw)) throw fail.badRequest(`第 ${index + 1} 个条目必须是 JSON 对象。`);

  const itemKey = assertHeaderString(raw.item_key, 'item_key', MAX_ITEM_KEY_BYTES, `第 ${index + 1} 个条目的 item_key`);

  const typeRaw = raw.type;
  if (typeof typeRaw !== 'string' || !(ITEM_TYPES as readonly string[]).includes(typeRaw)) {
    throw fail.badRequest(
      `第 ${index + 1} 个条目的 type 必须是 ${ITEM_TYPES.join(' / ')} 之一。`,
    );
  }
  const type = typeRaw as ItemType;

  const hlc = parseHlc(raw, index);

  const nonceBytes = fromBase64Strict(raw.nonce, 1024);
  if (nonceBytes === null) {
    throw fail.badRequest(`第 ${index + 1} 个条目的 nonce 不是合法 base64。`);
  }
  if (nonceBytes.length !== NONCE_BYTES) {
    throw fail.badRequest(
      `第 ${index + 1} 个条目的 nonce 长度必须是 ${NONCE_BYTES} 字节（实收 ${nonceBytes.length} 字节）。`,
    );
  }

  const cipherBytes = fromBase64Strict(raw.cipher, MAX_CIPHER_BYTES);
  if (cipherBytes === null) {
    throw fail.badRequest(
      `第 ${index + 1} 个条目的 cipher 不是合法 base64，或超过上限 ${MAX_CIPHER_BYTES} 字节。`,
    );
  }
  if (cipherBytes.length < TAG_BYTES) {
    throw fail.badRequest(
      `第 ${index + 1} 个条目的 cipher 至少 ${TAG_BYTES} 字节（实收 ${cipherBytes.length} 字节）。`,
    );
  }

  return {
    itemKey,
    type,
    hlc,
    // 统一按标准 base64 规范化落库，避免客户端 base64 / base64url 混用导致回拉内容不一致
    nonceB64: toBase64(nonceBytes),
    nonceBytes,
    cipherBytes,
  };
}

/** R2 对象键：u/<account_id>/<sha256(item_key)>（plan §6.3）。 */
export async function r2KeyFor(userId: string, itemKeyHashHex: string): Promise<string> {
  return `u/${userId}/${itemKeyHashHex}`;
}