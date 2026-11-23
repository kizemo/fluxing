// 统一错误面：错误一律「状态码 + 稳定字符串码 + 中文原因」。
// constitution P2/P5：用户可见字符串保持简体中文。

/**
 * 上游状态码的诊断响应头：值为第三方发信服务返回的 HTTP status（纯数字字符串）。
 * 只有**真的发起了上游调用**才会有这个头（配置缺失 / 预检失败 ⇒ 没有上游状态可报）。
 */
export const PROVIDER_STATUS_HEADER = 'X-Fluxing-Mail-Provider-Status';

/**
 * 回滚结果诊断头：值为 'ok'（失败注册 / 重发留下的残留已清干净）或 'failed'（回滚本身失败，
 * 库里可能仍有残留）。没有这个头 = 本次请求根本没进入回滚路径（例如校验失败就没写过库）。
 */
export const ROLLBACK_HEADER = 'X-Fluxing-Mail-Rollback';

/**
 * HTTP 头值必须是 ASCII（Fetch 规范）：非 ASCII 在浏览器里会直接抛 TypeError，
 * WinHTTP 侧也可能解析异常。诊断头因此统一降级成 ASCII（中文记进服务端日志，不进头）。
 */
function toAsciiHeaderValue(text: string): string {
  return text.replace(/[^\x20-\x7e]/g, '?');
}

/**
 * 回滚降级 / 失败诊断头：值是**脱敏后**的失败原因（ASCII、截断 160 字）。
 * 'degraded'（DELETE 被拒、已用 UPDATE 让行失效化）与 'failed'（两条路都失败）都会带这个头，
 * 帮助在拿不到服务端日志时定位 D1 侧的原始错误。只进诊断头，不进错误体。
 */
export const ROLLBACK_ERROR_HEADER = 'X-Fluxing-Mail-Rollback-Error';

/**
 * D1「每日行写入配额耗尽」的错误特征（Cloudflare code 7500）。
 *
 * Cloudflare 有两条通道抛出同一个故障，但**错误文本不一致**：
 *   · REST API（`wrangler d1 execute` / Cloudflare API）⇒ 文本尾部带 `[code: 7500]`；
 *   · Worker 内 D1 绑定（`env.DB.prepare(...).run()`）⇒ 文本是 `D1_ERROR: <同一句话>`，
 *     **不带** `[code: 7500]`。
 * 只认 `[code: 7500]` ⇒ Worker 内的写路径（登录 / 续期 / 推送）全都命中不了 ⇒
 * 明明是「UTC 午夜自动重置」的暂时性平台故障，却对外报笼统 500 ⇒
 * 客户端与运维都会误判成代码缺陷（2026-11-23 批次 BR 生产实测坐实）。
 * 因此判定同时接受两种形态。
 */
const D1_ROW_WRITE_QUOTA_PATTERNS: readonly RegExp[] = [
  /\[code:\s*7500\]/,
  /exceeded D1[’']s free tier daily row write limit/i,
  /D1_ERROR[\s\S]{0,240}?(daily row write limit|free tier)/i,
];

/**
 * 抽取任意抛出值的可读文本。
 *
 * Worker 侧的 D1 绑定错误**不一定**是 Error 实例（可能是普通对象）⇒ 不能只读 `.message`；
 * 这里把 Error（含 cause）/ 字符串 / 普通对象（message / text / cause）统一成一段文本。
 */
export function errorText(error: unknown): string {
  if (error instanceof Error) {
    const cause = (error as { cause?: unknown }).cause;
    const causeText = cause === undefined ? '' : errorText(cause);
    return causeText ? `${error.message} ${causeText}` : error.message;
  }
  if (typeof error === 'string') return error;
  if (typeof error === 'object' && error !== null) {
    const record = error as { message?: unknown; text?: unknown; cause?: unknown };
    const parts: string[] = [];
    if (typeof record.message === 'string') parts.push(record.message);
    if (typeof record.text === 'string') parts.push(record.text);
    if (record.cause !== undefined) parts.push(errorText(record.cause));
    if (parts.length > 0) return parts.join(' ');
    try {
      return JSON.stringify(error);
    } catch {
      return String(error);
    }
  }
  return String(error);
}

/**
 * 是否 D1「每日行写入配额耗尽」⇒ 503 storage_quota_exceeded（暂时性故障，UTC 午夜重置）。
 */
export function isD1RowWriteQuotaError(error: unknown): boolean {
  const text = errorText(error);
  return D1_ROW_WRITE_QUOTA_PATTERNS.some((pattern) => pattern.test(text));
}

export class ApiError extends Error {
  constructor(
    readonly status: number,
    readonly code: string,
    message: string,
    /**
     * 上游 HTTP 状态码（如第三方发信服务返回的 status）。
     * undefined = 本次请求没有拿到上游 HTTP 响应（配置缺失 / 预检失败 / 网络异常）。
     */
    readonly providerStatus?: number,
    /**
     * 回滚结果：'ok' = 残留已清干净，'degraded' = DELETE 被拒但已用 UPDATE 让行失效化，
     * 'failed' = 回滚自身彻底失败（库里可能仍有可用残留）。
     * undefined = 本次请求未进入回滚路径。
     */
    readonly rollbackStatus?: 'ok' | 'degraded' | 'failed',
    /**
     * 回滚失败的原始原因（已脱敏 + 截断），仅供诊断，不进错误体。
     */
    readonly rollbackError?: string,
  ) {
    super(message);
    this.name = 'ApiError';
  }
}

export const fail = {
  badRequest: (message: string, code = 'bad_request') => new ApiError(400, code, message),
  unauthorized: (message: string, code = 'unauthorized') => new ApiError(401, code, message),
  forbidden: (message: string, code = 'forbidden') => new ApiError(403, code, message),
  notFound: (message: string, code = 'not_found') => new ApiError(404, code, message),
  conflict: (message: string, code = 'conflict') => new ApiError(409, code, message),
  tooLarge: (message: string, code = 'payload_too_large') => new ApiError(413, code, message),
  tooFrequent: (message: string, code = 'too_frequent') => new ApiError(429, code, message),
  /**
   * 422：请求本身合法，但业务上无法按此内容处理。
   * 与本文件其它工厂的参数顺序**相反**（code 在前、message 在后）——
   * 422 一律是「特定业务原因 + 特定错误码」的组合，code 是必填项。
   */
  unprocessable: (code: string, message: string) => new ApiError(422, code, message),
  serverMisconfigured: (message: string) => new ApiError(500, 'server_misconfigured', message),
  internal: (message: string, code = 'internal_error') => new ApiError(500, code, message),
  unavailable: (message: string, code = 'unavailable') => new ApiError(503, code, message),
};

export interface ErrorBody {
  error: {
    code: string;
    message: string;
  };
}

export function jsonResponse(body: unknown, status = 200, extraHeaders?: HeadersInit): Response {
  const headers = new Headers(extraHeaders);
  headers.set('content-type', 'application/json; charset=utf-8');
  headers.set('cache-control', 'no-store');
  headers.set('x-content-type-options', 'nosniff');
  return new Response(JSON.stringify(body), { status, headers });
}

export function toErrorResponse(error: unknown): Response {
  if (error instanceof ApiError) {
    const body: ErrorBody = { error: { code: error.code, message: error.message } };
    const headers: Record<string, string> = {};
    // 只有拿到过上游 HTTP 响应时才带 PROVIDER_STATUS_HEADER（诊断用，纯数字）。
    if (typeof error.providerStatus === 'number') {
      headers[PROVIDER_STATUS_HEADER] = String(error.providerStatus);
    }
    // 回滚结果（残留是否清干净）也是诊断头，不进错误体，避免污染用户可见文案。
    if (error.rollbackStatus !== undefined) {
      headers[ROLLBACK_HEADER] = error.rollbackStatus;
    }
    // 回滚失败原因（脱敏 + 截断）只进诊断头，帮助在拿不到服务端日志时定位。
    if (error.rollbackError !== undefined && error.rollbackError.length > 0) {
      headers[ROLLBACK_ERROR_HEADER] = toAsciiHeaderValue(error.rollbackError).slice(0, 160);
    }
    return jsonResponse(body, error.status, Object.keys(headers).length > 0 ? headers : undefined);
  }
  // 未知异常：不外泄内部细节，但把原因留在服务端日志里。
  // 例外：D1 每日行写入配额耗尽（Cloudflare code 7500）是**暂时性**故障（UTC 午夜重置），
  // 语义上必须报 503 而不是笼统 500 —— 否则客户端与运维都会以为是代码缺陷。
  const detail = errorText(error);
  if (isD1RowWriteQuotaError(error)) {
    console.error('[fluxing-sync] D1 行写入配额耗尽（code 7500）：', detail);
    const body: ErrorBody = {
      error: {
        code: 'storage_quota_exceeded',
        message: '服务端存储写入额度已用尽，请稍后重试。',
      },
    };
    return jsonResponse(body, 503);
  }
  console.error('[fluxing-sync] 未处理异常：', error);
  const body: ErrorBody = {
    error: { code: 'internal_error', message: '服务端内部错误，请稍后重试。' },
  };
  return jsonResponse(body, 500);
}

/** 读取并解析 JSON 请求体；空体 / 非对象 / 语法错误统一转成 400 中文错误。 */
export async function readJsonObject(request: Request): Promise<Record<string, unknown>> {
  let text: string;
  try {
    text = await request.text();
  } catch {
    throw fail.badRequest('无法读取请求体。');
  }
  if (text.trim().length === 0) throw fail.badRequest('请求体不能为空。');
  let parsed: unknown;
  try {
    parsed = JSON.parse(text);
  } catch {
    throw fail.badRequest('请求体不是合法 JSON。');
  }
  if (typeof parsed !== 'object' || parsed === null || Array.isArray(parsed)) {
    throw fail.badRequest('请求体必须是 JSON 对象。');
  }
  return parsed as Record<string, unknown>;
}