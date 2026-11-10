// 统一错误面：错误一律「状态码 + 稳定字符串码 + 中文原因」。
// constitution P2/P5：用户可见字符串保持简体中文。

export class ApiError extends Error {
  constructor(
    readonly status: number,
    readonly code: string,
    message: string,
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
    return jsonResponse(body, error.status);
  }
  // 未知异常：不外泄内部细节，但把原因留在服务端日志里。
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