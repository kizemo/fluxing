// 服务端密码学原语与编码工具。
// 全部使用 Workers 自带 WebCrypto（crypto.subtle / getRandomValues），不引入第三方密码学库。

const textEncoder = new TextEncoder();

export function utf8(s: string): Uint8Array {
  return textEncoder.encode(s);
}

export function randomBytes(n: number): Uint8Array {
  const out = new Uint8Array(n);
  crypto.getRandomValues(out);
  return out;
}

/** 在 [0, bound) 取均匀随机整数（拒绝采样，避免取模偏置）。 */
export function randomInt(bound: number): number {
  if (!Number.isInteger(bound) || bound <= 0 || bound > 0x100000000) {
    throw new Error('randomInt: bound 非法');
  }
  const limit = Math.floor(0x100000000 / bound) * bound;
  const buf = new Uint32Array(1);
  for (;;) {
    crypto.getRandomValues(buf);
    const v = buf[0] as number;
    if (v < limit) return v % bound;
  }
}

function toArrayBuffer(bytes: Uint8Array): ArrayBuffer {
  // 复制一份，确保拿到独立的 ArrayBuffer（避开 SharedArrayBuffer / 共享视图的歧义）
  const copy = new Uint8Array(bytes.byteLength);
  copy.set(bytes);
  return copy.buffer;
}

export async function sha256(bytes: Uint8Array): Promise<Uint8Array> {
  return new Uint8Array(await crypto.subtle.digest('SHA-256', toArrayBuffer(bytes)));
}

export async function sha256Hex(bytes: Uint8Array): Promise<string> {
  return toHex(await sha256(bytes));
}

export async function hmacSha256(key: Uint8Array, message: Uint8Array): Promise<Uint8Array> {
  const cryptoKey = await crypto.subtle.importKey(
    'raw',
    toArrayBuffer(key),
    { name: 'HMAC', hash: 'SHA-256' },
    false,
    ['sign'],
  );
  return new Uint8Array(await crypto.subtle.sign('HMAC', cryptoKey, toArrayBuffer(message)));
}

/** 定长常数时间比较；长度不同直接 false。 */
export function timingSafeEqual(a: Uint8Array, b: Uint8Array): boolean {
  if (a.length !== b.length) return false;
  let diff = 0;
  for (let i = 0; i < a.length; i += 1) {
    diff |= (a[i] as number) ^ (b[i] as number);
  }
  return diff === 0;
}

export function toHex(bytes: Uint8Array): string {
  let out = '';
  for (let i = 0; i < bytes.length; i += 1) {
    out += (bytes[i] as number).toString(16).padStart(2, '0');
  }
  return out;
}

export function toBase64(bytes: Uint8Array): string {
  let bin = '';
  const chunk = 0x8000;
  for (let i = 0; i < bytes.length; i += chunk) {
    bin += String.fromCharCode(...bytes.subarray(i, Math.min(i + chunk, bytes.length)));
  }
  return btoa(bin);
}

export function toBase64Url(bytes: Uint8Array): string {
  return toBase64(bytes).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
}

/** 严格 base64 / base64url 解码；任何非法输入返回 null（由调用方给出中文错误）。 */
export function fromBase64Strict(input: unknown, maxBytes = 16 * 1024 * 1024): Uint8Array | null {
  if (typeof input !== 'string' || input.length === 0) return null;
  if (!/^[A-Za-z0-9+/\-_]+={0,2}$/.test(input)) return null;
  const normalized = input.replace(/-/g, '+').replace(/_/g, '/').replace(/=+$/, '');
  if (normalized.length % 4 === 1) return null;
  const padded = normalized + '='.repeat((4 - (normalized.length % 4)) % 4);
  let bin: string;
  try {
    bin = atob(padded);
  } catch {
    return null;
  }
  if (bin.length > maxBytes) return null;
  const out = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i += 1) out[i] = bin.charCodeAt(i);
  return out;
}

/** 32 字节不透明令牌（base64url，无填充）。 */
export function newOpaqueToken(): string {
  return toBase64Url(randomBytes(32));
}

export function nowSeconds(): number {
  return Math.floor(Date.now() / 1000);
}