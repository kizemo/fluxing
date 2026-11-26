// R2 → 本地文件系统兼容层（路线 B · ADR 0079）。
//
// 目标：`env.BUCKET` 的四个动作（put / get / delete / list）在自建服务器上保持
// **可观察行为一致**，让 sync.ts / sync-buckets.ts / account.ts 一个字都不改。
//
// 被消费的 API 面（2026-11-26 逐文件回读）：
//   put(key, value, {httpMetadata, customMetadata})   写入（并发度由 r2-write.ts 控，本模块不管）
//   get(key) → { arrayBuffer() } | null               只有 arrayBuffer()，**不读 metadata / size**
//   delete(key | key[])                               数组形式（account.ts 清前缀）
//   list({ prefix, limit }) → { objects: [{ key }] }  只读 objects，**从不使用 cursor / truncated**
//
// 五处不能想当然的地方：
//   1. **原子替换**：先写 `<root>/.tmp/<随机>`，再 rename 覆盖目标。直接覆盖写会留下
//      「已存在但长度为 0」的窗口 —— 那正是 P0「假空」事故的形态，读侧会把 0 字节
//      当成合法空对象，进而让整库被判成「本地词典为空」。
//   2. **list 必须递归**：R2 的 key 可以含 `/`（`u/<userId>/<r2Key>`），而文件系统里
//      `/` 是目录分隔符。只扫一层会让 account.ts 的「清完自证为空」在**没清干净**时
//      也返回空 ⇒ 假绿。这是本模块唯一一处「错了会静默骗人」的地方。
//   3. **排序按 UTF-8 字节**：R2 的字典序是字节序，JS 的 `<` 是 UTF-16 code unit 序，
//      对 BMP 以上字符两者不一致。这里不用 `<`。
//   4. **不持久化 metadata**：无读路径消费它；存成 sidecar 文件反而会被 list 扫到，
//      污染上面那条自证。
//   5. **路径编码**：每段 `encodeURIComponent`，段首下划线与段尾的点额外转义
//      （Windows 会静默吃掉结尾的点、也会把 `con` 之类当设备名，两者都会让不同
//      key 落到同一文件）。编码**可逆**：`list` 侧还原成逻辑 key 再交出。
//
// 明确不做的：空目录清理（残留空目录既不影响 list，也不影响自证）、
// 路径长度重映射（Windows MAX_PATH 260；本仓库 key 形如 `u/<26 位>/b/<n>`，远未触及，
// 真触及时应报错而不是静默截断）。

import { dynamicImport } from './dynamic-import';

// ---------------------------------------------------------------------------
// node:fs/promises 的结构类型（自声明：零依赖、无 @types/node）
// ---------------------------------------------------------------------------

/** `readdir(..., {withFileTypes:true})` 的元素。 */
export interface FsDirentLike {
  name: string;
  isDirectory(): boolean;
  isFile(): boolean;
}

/** 本模块用到的 fs 面。 */
export interface FsPromisesLike {
  mkdir(path: string, options: { recursive: true }): Promise<string | undefined>;
  writeFile(path: string, data: Uint8Array): Promise<void>;
  readFile(path: string): Promise<Uint8Array>;
  rename(from: string, to: string): Promise<void>;
  rm(path: string, options: { recursive?: boolean; force?: boolean }): Promise<void>;
  readdir(path: string, options: { withFileTypes: true }): Promise<FsDirentLike[]>;
  stat(path: string): Promise<{ size: number }>;
}

/** 临时文件目录名（放在对象树根的**同级子树**里，不进 prefix 空间）。 */
const TMP_DIR_NAME = '.tmp';

/** list 的 limit 上界（R2 单页上限 1,000；account.ts 传 1000 / 1）。 */
const LIST_LIMIT_MAX = 1_000;

/** rename 覆盖的短暂重试次数（Windows 上目标被别的进程短暂占用会 EPERM）。 */
const RENAME_ATTEMPTS = 4;
const RENAME_RETRY_DELAY_MS = 25;

// ---------------------------------------------------------------------------
// key → 相对路径
// ---------------------------------------------------------------------------

/**
 * 编码单个路径段。
 *
 * `encodeURIComponent` 之后仍需要处理的三件事：
 *   · 段首的下划线：`'_con'` → `'%5Fcon'`。不处理的话，逻辑 `con`（会加下划线前缀）
 *     与逻辑 `_con`（原样保留）落到同一物理名，后者静默覆盖前者；
 *   · 段尾的点：`'a.'` → `'a%2E'`（Windows 会 strip 结尾的点与空格，制造 key 碰撞）；
 *   · Windows 保留设备名：`CON` / `PRN` / `AUX` / `NUL` / `COM1..9` / `LPT1..9`
 *     （含带扩展名形态）加前缀 `_` 破坏它。
 *
 * 三条合起来保证**单射且可逆**（`decodeSegment` 逐段还原，见 `list` 与 `decodeObjectKey`）。
 */
function encodeSegment(segment: string): string {
  const escaped = encodeURIComponent(segment).replace(/\.+$/g, (run) => '%2E'.repeat(run.length));
  if (escaped.length === 0) return escaped;
  const encoded = escaped.startsWith('_') ? '%5F' + escaped.slice(1) : escaped;
  if (/^(?:con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\.|%2E|$)/i.test(encoded)) {
    return '_' + encoded;
  }
  return encoded;
}

/**
 * 把 R2 key 编码成相对路径（分段，`/` 保留为分隔符）。
 *
 * 与 `decodeObjectKey` 互为逆运算。prefix 比对在编码侧做（两侧同一函数），
 * 所以当 key 段以保留设备名开头时，比逻辑前缀**略严**（前缀 `c` 匹配不到 `con`）；
 * 实际前缀都是 `users/<id>/` 这种形状，不含保留名，可接受。
 */
export function encodeObjectKey(key: string): string {
  return key.split('/').map(encodeSegment).join('/');
}

/**
 * 还原单个路径段（`encodeSegment` 的逆）。
 *
 * 物理名以 `_` 开头 ⇒ 去掉它（那是保留设备名的保护前缀）；其余原样做百分号解码。
 * 有了「段首下划线编码成 %5F」这条，逆变换无歧义。非法转义不抛，原样返回。
 */
function decodeSegment(segment: string): string {
  const body = segment.startsWith('_') ? segment.slice(1) : segment;
  try {
    return decodeURIComponent(body);
  } catch {
    return segment;
  }
}

/**
 * 还原整条 R2 key（`encodeObjectKey` 的逆）。
 *
 * `list` 必须交出**逻辑** key：调用方拿 `object.key` 直接回喂 `delete`
 * （见 account.ts:58 的清空前缀流程）。交物理名会造成二次编码 ⇒ 删不掉
 * ⇒ 自证探针永远看到残留 ⇒ 报「数据未完全清除」。
 */
export function decodeObjectKey(encoded: string): string {
  return encoded.split('/').map(decodeSegment).join('/');
}

/** 从错误对象上读 code（不依赖 node 类型）。 */
function errorCode(error: unknown): string | undefined {
  if (typeof error !== 'object' || error === null) return undefined;
  const code = (error as { code?: unknown }).code;
  return typeof code === 'string' ? code : undefined;
}

/** 按 UTF-8 字节序比较（与 R2 的字典序口径一致）。 */
const byteEncoder = new TextEncoder();

function compareUtf8(left: string, right: string): number {
  const a = byteEncoder.encode(left);
  const b = byteEncoder.encode(right);
  const shared = Math.min(a.length, b.length);
  for (let index = 0; index < shared; index += 1) {
    const x = a[index] as number;
    const y = b[index] as number;
    if (x !== y) return x - y;
  }
  return a.length - b.length;
}

/** 当前进程内的临时文件名序号（同毫秒内的多次写入不会撞名）。 */
let tempSequence = 0;

// ---------------------------------------------------------------------------
// 写入载荷归一
// ---------------------------------------------------------------------------

function toBytes(value: ArrayBuffer | Uint8Array | string): Uint8Array {
  if (typeof value === 'string') return new TextEncoder().encode(value);
  // Uint8Array 分支必须先判：Buffer 也是 Uint8Array，但 `new Uint8Array(buffer)`
  // 会走「按 ArrayBuffer 视图构造」的另一条语义（byteOffset 被忽略）。
  if (value instanceof Uint8Array) return value;
  return new Uint8Array(value);
}

function delay(milliseconds: number): Promise<void> {
  return new Promise((resolve) => {
    setTimeout(resolve, milliseconds);
  });
}

// ---------------------------------------------------------------------------
// 桶
// ---------------------------------------------------------------------------

/** 桶对象（list 的元素）。 */
export interface FsObjectLike {
  key: string;
  size: number;
}

/** get 的返回：只暴露 arrayBuffer（调用方只用它）。 */
export interface FsObjectBodyLike {
  key: string;
  size: number;
  arrayBuffer(): Promise<ArrayBuffer>;
}

/** list 的返回：`truncated` 保真，但调用方从不读它。 */
export interface FsListResultLike {
  objects: FsObjectLike[];
  truncated: boolean;
}

/**
 * 文件系统桶。
 *
 * 构造函数私有化效果由 `createFsBucket()` 承担（要先动态 import node:fs/promises）。
 */
export class FsBucket {
  readonly #fs: FsPromisesLike;
  readonly #root: string;
  #ready: Promise<void> | null;

  constructor(fs: FsPromisesLike, root: string) {
    this.#fs = fs;
    this.#root = root.replace(/\\/g, '/').replace(/\/+$/, '');
    this.#ready = null;
  }

  /** 对象树根（绝对路径，`/` 分隔）。 */
  get root(): string {
    return this.#root;
  }

  /** 幂等地建根目录与临时目录。 */
  ready(): Promise<void> {
    if (this.#ready === null) {
      this.#ready = (async () => {
        await this.#fs.mkdir(this.#root, { recursive: true });
        await this.#fs.mkdir(this.#root + '/' + TMP_DIR_NAME, { recursive: true });
      })();
    }
    return this.#ready;
  }

  /** 写入（同 key 覆盖，幂等）。 */
  async put(
    key: string,
    value: ArrayBuffer | Uint8Array | string,
    _options?: unknown,
  ): Promise<FsObjectLike> {
    await this.ready();
    const bytes = toBytes(value);
    const target = this.#absolute(key);
    const parent = target.slice(0, target.lastIndexOf('/'));
    if (parent.length > 0 && parent !== this.#root) {
      await this.#fs.mkdir(parent, { recursive: true });
    }

    tempSequence += 1;
    const temp =
      this.#root +
      '/' +
      TMP_DIR_NAME +
      '/w' +
      Date.now().toString(36) +
      '-' +
      tempSequence.toString(36) +
      '-' +
      Math.random().toString(36).slice(2, 8) +
      '.tmp';

    await this.#fs.writeFile(temp, bytes);
    try {
      await this.#renameWithRetry(temp, target);
    } catch (error) {
      // 临时文件必须收掉：否则一次失败就在 .tmp 里留一份密文残片。
      await this.#fs.rm(temp, { force: true }).catch(() => undefined);
      throw error;
    }
    return { key, size: bytes.byteLength };
  }

  /** 读取；不存在返回 null（调用方据此判「云端对象缺失 ⇒ 自愈」）。 */
  async get(key: string): Promise<FsObjectBodyLike | null> {
    const target = this.#absolute(key);
    let bytes: Uint8Array;
    try {
      bytes = await this.#fs.readFile(target);
    } catch (error) {
      if (errorCode(error) === 'ENOENT') return null;
      throw error;
    }
    // 复制到独立 ArrayBuffer：Buffer 可能是池化切片，直接 `.buffer` 会把别人
    // 的字节一起交出去（长度对、内容错，最难查的一类）。
    const copy = new Uint8Array(bytes.byteLength);
    copy.set(bytes);
    return {
      key,
      size: copy.byteLength,
      arrayBuffer: async () => copy.buffer,
    };
  }

  /** 删除（单 key 或数组）；不存在视为成功。 */
  async delete(keys: string | readonly string[]): Promise<void> {
    const list = typeof keys === 'string' ? [keys] : keys;
    for (const key of list) {
      try {
        await this.#fs.rm(this.#absolute(key), { force: true });
      } catch (error) {
        if (errorCode(error) === 'ENOENT') continue;
        throw error;
      }
    }
  }

  /**
   * 按前缀列举（**递归**，见文件头第 2 条）。
   *
   * limit 语义与 R2 一致：返回至多 limit 个，`truncated` 标记是否还有更多。
   * 顺序为 UTF-8 字节序。
   */
  async list(options: { prefix?: string; limit?: number } = {}): Promise<FsListResultLike> {
    await this.ready();
    const prefix = options.prefix ?? '';
    const encodedPrefix = encodeObjectKey(prefix);
    const requested = options.limit ?? LIST_LIMIT_MAX;
    const limit = Math.max(1, Math.min(Math.floor(requested), LIST_LIMIT_MAX));

    const matched: string[] = [];
    await this.#walk('', encodedPrefix, matched);

    matched.sort(compareUtf8);
    const page = matched.slice(0, limit);
    const objects: FsObjectLike[] = [];
    for (const relative of page) {
      let size = 0;
      try {
        const info = await this.#fs.stat(this.#root + '/' + relative);
        size = info.size;
      } catch {
        // 走到这里说明列举与 stat 之间有并发删除。size 归 0 即可 ——
        // 调用方只用 key，不用 size。
      }
      objects.push({ key: decodeObjectKey(relative), size });
    }
    return { objects, truncated: matched.length > page.length };
  }

  /**
   * 递归收集相对路径。
   *
   * @param relativeDirectory 当前目录的相对路径（根为 ''）
   * @param encodedPrefix 已编码的前缀
   * @param sink 收集器（收集**已编码**的相对路径；list 对外再还原成逻辑 key）
   */
  async #walk(relativeDirectory: string, encodedPrefix: string, sink: string[]): Promise<void> {
    const absoluteDirectory =
      relativeDirectory.length === 0 ? this.#root : this.#root + '/' + relativeDirectory;
    let entries: FsDirentLike[];
    try {
      entries = await this.#fs.readdir(absoluteDirectory, { withFileTypes: true });
    } catch (error) {
      // 根目录被外部删掉 ⇒ 空桶，不是错误。
      if (errorCode(error) === 'ENOENT') return;
      throw error;
    }

    for (const entry of entries) {
      const relative =
        relativeDirectory.length === 0 ? entry.name : relativeDirectory + '/' + entry.name;

      if (entry.isDirectory()) {
        // 临时目录永远不进 prefix 空间。
        if (relativeDirectory.length === 0 && entry.name === TMP_DIR_NAME) continue;
        if (this.#shouldDescend(relative, encodedPrefix)) {
          await this.#walk(relative, encodedPrefix, sink);
        }
        continue;
      }

      if (!entry.isFile()) continue;
      if (!relative.startsWith(encodedPrefix)) continue;
      sink.push(relative);
    }
  }

  /**
   * 目录剪枝：只有「前缀穿过本目录」或「前缀可能落在本目录子树内」才值得下去。
   *
   * 不剪枝的后果不是错，而是每次都全树扫（92k 对象 × 每轮），把一次自证变成一次 DDoS。
   */
  #shouldDescend(relativeDirectory: string, encodedPrefix: string): boolean {
    if (encodedPrefix.length === 0) return true;
    if (relativeDirectory.startsWith(encodedPrefix)) return true;
    // 前缀比当前目录更深：只在目录边界上成立（避免 'u/a' 误配 'u/ab'）。
    return encodedPrefix.startsWith(relativeDirectory + '/');
  }

  /** key → 绝对路径。 */
  #absolute(key: string): string {
    return this.#root + '/' + encodeObjectKey(key);
  }

  /**
   * rename 覆盖目标，带短暂重试。
   *
   * Windows 上目标文件被读侧短暂持有会 EPERM / EBUSY；不支持原子替换的
   * 文件系统则是 EEXIST / EXDEV。三次都没成，退化成「删目标再 rename」——
   * 这条退化路径有极小窗口目标不存在，但 get() 对 ENOENT 返回 null，
   * 调用方按「对象缺失 ⇒ 自愈重推」处理，**不会**把 0 字节读成合法空对象。
   */
  async #renameWithRetry(from: string, to: string): Promise<void> {
    let lastError: unknown;
    for (let attempt = 0; attempt < RENAME_ATTEMPTS; attempt += 1) {
      try {
        await this.#fs.rename(from, to);
        return;
      } catch (error) {
        lastError = error;
        const code = errorCode(error);
        if (code !== 'EPERM' && code !== 'EBUSY' && code !== 'EACCES' && code !== 'EEXIST' && code !== 'EXDEV') {
          throw error;
        }
        await delay(RENAME_RETRY_DELAY_MS * (attempt + 1));
      }
    }
    await this.#fs.rm(to, { force: true });
    await this.#fs.rename(from, to);
    if (lastError !== undefined) {
      // 到这里说明退化路径成功了；不打日志（正常路径的噪音只会淹没真事件）。
    }
  }
}

/**
 * 建桶。
 *
 * @param rootDir 对象树根目录（绝对路径）。不存在会自动建。
 */
export async function createFsBucket(rootDir: string): Promise<FsBucket> {
  if (typeof rootDir !== 'string' || rootDir.trim().length === 0) {
    throw new Error('FLUXING_OBJECT_ROOT 未配置：拒绝在未知目录上建对象树。');
  }
  const fs = (await dynamicImport('node:fs/promises')) as FsPromisesLike;
  const bucket = new FsBucket(fs, rootDir);
  await bucket.ready();
  return bucket;
}