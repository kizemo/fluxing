// Worker 入口：路由分发 + 统一错误面。
//
// 契约来源 plan §6.1（端点 9 条，另加 GET /v1/health 供巡活、POST /v1/auth/resend 供重发验证码）：
//   POST   /v1/auth/register      POST /v1/auth/verify
//   POST   /v1/auth/resend        POST /v1/auth/login      POST /v1/auth/refresh
//   POST   /v1/sync/push          GET  /v1/sync/pull
//   GET    /v1/devices            DELETE /v1/devices/:id
//   DELETE /v1/account/data
//
// 约定：
//   · 路由手写 switch + 一条正则，不引入框架：端点固定，依赖越少越好。
//   · 方法不符 ⇒ 405（不设 Allow 头：统一错误面不带自定义 header，保持响应逐字可预期）。
//   · 路径尾斜杠归一化（/v1/devices/ 与 /v1/devices 等价），不做大小写折叠。
//   · 不做 CORS：客户端是原生进程，不存在浏览器源。

import type { Env } from './env';
import { ApiError, fail, jsonResponse, readJsonObject, toErrorResponse } from './errors';
import { nowSeconds } from './crypto';
import {
  authenticate,
  handleLogin,
  handleRefresh,
  handleRegister,
  handleResend,
  handleVerify,
} from './auth';
import { handlePull, handlePush } from './sync';
import { handleDeviceList, handleDeviceRevoke } from './devices';
import { handleDeleteAccountData } from './account';

const DEVICE_PATH_RE = /^\/v1\/devices\/([^/]+)$/;

function assertMethod(request: Request, expected: string): void {
  if (request.method !== expected) {
    throw new ApiError(405, 'method_not_allowed', `该接口只支持 ${expected} 请求。`);
  }
}

function normalizePath(pathname: string): string {
  if (pathname.length > 1 && pathname.endsWith('/')) {
    const stripped = pathname.replace(/\/+$/, '');
    return stripped.length === 0 ? '/' : stripped;
  }
  return pathname;
}

async function route(request: Request, env: Env): Promise<Response> {
  const url = new URL(request.url);
  const path = normalizePath(url.pathname);

  switch (path) {
    case '/v1/health': {
      assertMethod(request, 'GET');
      return jsonResponse({
        ok: true,
        service: 'fluxing-sync',
        environment: env.ENVIRONMENT ?? 'unknown',
        time: nowSeconds(),
      });
    }

    case '/v1/auth/register': {
      assertMethod(request, 'POST');
      return jsonResponse(await handleRegister(env, await readJsonObject(request)));
    }

    case '/v1/auth/verify': {
      assertMethod(request, 'POST');
      return jsonResponse(await handleVerify(env, await readJsonObject(request)));
    }

    case '/v1/auth/resend': {
      assertMethod(request, 'POST');
      return jsonResponse(await handleResend(env, await readJsonObject(request)));
    }

    case '/v1/auth/login': {
      assertMethod(request, 'POST');
      return jsonResponse(await handleLogin(env, await readJsonObject(request)));
    }

    case '/v1/auth/refresh': {
      assertMethod(request, 'POST');
      return jsonResponse(await handleRefresh(env, await readJsonObject(request)));
    }

    case '/v1/sync/push': {
      assertMethod(request, 'POST');
      const auth = await authenticate(env, request);
      return jsonResponse(await handlePush(env, auth, await readJsonObject(request)));
    }

    case '/v1/sync/pull': {
      assertMethod(request, 'GET');
      const auth = await authenticate(env, request);
      return jsonResponse(await handlePull(env, auth, url));
    }

    case '/v1/devices': {
      assertMethod(request, 'GET');
      const auth = await authenticate(env, request);
      return jsonResponse(await handleDeviceList(env, auth));
    }

    case '/v1/account/data': {
      assertMethod(request, 'DELETE');
      const auth = await authenticate(env, request);
      return jsonResponse(await handleDeleteAccountData(env, auth));
    }

    default:
      break;
  }

  const deviceMatch = DEVICE_PATH_RE.exec(path);
  if (deviceMatch) {
    assertMethod(request, 'DELETE');
    const auth = await authenticate(env, request);
    return jsonResponse(await handleDeviceRevoke(env, auth, deviceMatch[1] as string));
  }

  throw fail.notFound('没有这个接口。', 'not_found');
}

export default {
  async fetch(request: Request, env: Env): Promise<Response> {
    try {
      return await route(request, env);
    } catch (error) {
      return toErrorResponse(error);
    }
  },
} satisfies ExportedHandler<Env>;

export type { Env } from './env';