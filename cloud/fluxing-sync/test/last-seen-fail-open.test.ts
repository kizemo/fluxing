// 设备 last_seen 刷新的「读路径 fail-open」单测（ADR 0077 A-0 止血项 2）。
//
// 背景（2026-11-24 生产实测坐实）：
//   authenticate() 在**鉴权读路径**上会顺带刷新 devices.last_seen。
//   D1 每日行写入配额耗尽时（code 7500）这一行写会抛错，而未捕获的异常会把
//   **整个请求**（包括纯读的 GET /v1/sync/pull）一起打成 503
//   ⇒ 表现为「额度一满，全站同步与登录一起挂」，而故障面与「同步」本身无关。
//
// 本文件锁住三件事：
//   1. 写失败（配额错误）**不阻断**鉴权：authenticate 仍正常返回用户上下文；
//   2. 刷频窗口 = 3600 秒（导出常量直接断言，避免用真实时钟做边界断言而抖动）；
//   3. 窗口内不重复写、超窗口才写一次。
//
// 注意：isolatedStorage 默认开启 ⇒ 多步流程必须写在同一个 it() 里。

import { describe, expect, it } from 'vitest';
import { authenticate, LAST_SEEN_REFRESH_SECONDS } from '../src/auth';
import type { Env } from '../src/env';

const TOKEN = 'test-access-token';

/** 造一个只响应鉴权所需查询的 D1 桩，并记录所有执行过的写语句。 */
interface StubOptions {
  /** 设备上次被见到的秒时间戳（NULL = 首次见到该设备）。 */
  deviceLastSeen: number | null;
  /** 让「UPDATE devices」抛出的错误（不填 = 写成功）。 */
  devicesWriteError?: Error;
}

function makeStub(options: StubOptions): { env: Env; writes: string[] } {
  const writes: string[] = [];
  const db = {
    prepare(sql: string) {
      const stmt = {
        bind(..._args: unknown[]) {
          return stmt;
        },
        async first<T>() {
          return {
            user_id: 'user-1',
            device_id: 'device-1',
            expires_at: Math.floor(Date.now() / 1000) + 86400,
            revoked_at: null,
            kind: 'access',
            email_verified: 1,
            device_revoked_at: null,
            device_last_seen: options.deviceLastSeen,
          } as unknown as T;
        },
        async run() {
          writes.push(sql);
          if (options.devicesWriteError) throw options.devicesWriteError;
          return { success: true };
        },
      };
      return stmt;
    },
  };
  return { env: { DB: db } as unknown as Env, writes };
}

function authRequest(): Request {
  return new Request('https://fluxing-sync.test/v1/sync/pull', {
    headers: { authorization: `Bearer ${TOKEN}` },
  });
}

function lastSeenWrites(writes: string[]): number {
  return writes.filter((sql) => sql.includes('UPDATE devices SET last_seen')).length;
}

/** Worker 内 D1 绑定形态的配额耗尽错误（无 [code: 7500] 后缀的真实形态）。 */
const QUOTA_ERROR = new Error(
  "D1_ERROR: Your account has exceeded D1's free tier daily row write limit. Upgrade to a paid plan or wait until tomorrow (midnight UTC) to continue.",
);

describe('设备 last_seen 刷新：读路径 fail-open', () => {
  it('写失败（配额耗尽）⇒ 不抛错，鉴权仍然放行', async () => {
    const stub = makeStub({ deviceLastSeen: null, devicesWriteError: QUOTA_ERROR });
    const context = await authenticate(stub.env, authRequest());
    expect(context.userId).toBe('user-1');
    expect(context.deviceRowId).toBe('device-1');
  });

  it('写失败时确实尝试写过（证明 fail-open 不是「压根没写」而是真的失败被吞掉）', async () => {
    const stub = makeStub({ deviceLastSeen: null, devicesWriteError: QUOTA_ERROR });
    await authenticate(stub.env, authRequest());
    expect(lastSeenWrites(stub.writes)).toBe(1);
  });

  it('写失败 ⇒ 不会把请求打成 503（resolve 而非 reject）', async () => {
    const stub = makeStub({ deviceLastSeen: null, devicesWriteError: QUOTA_ERROR });
    await expect(authenticate(stub.env, authRequest())).resolves.toBeTruthy();
  });

  it('写成功 ⇒ 同样放行（fail-open 不影响正常路径）', async () => {
    const stub = makeStub({ deviceLastSeen: null });
    const context = await authenticate(stub.env, authRequest());
    expect(context.userId).toBe('user-1');
    expect(lastSeenWrites(stub.writes)).toBe(1);
  });
});

describe('设备 last_seen 刷频窗口 = 3600 秒', () => {
  it('常量值 = 3600（1 小时 ⇒ 24 行/设备/日，替代原 288 行/日）', () => {
    expect(LAST_SEEN_REFRESH_SECONDS).toBe(3600);
  });

  it('窗口内（now - last_seen = 60 秒）⇒ 不重复写', async () => {
    const stub = makeStub({ deviceLastSeen: Math.floor(Date.now() / 1000) - 60 });
    await authenticate(stub.env, authRequest());
    expect(lastSeenWrites(stub.writes)).toBe(0);
  });

  it('超窗口（now - last_seen = 3660 秒）⇒ 写一次', async () => {
    const stub = makeStub({ deviceLastSeen: Math.floor(Date.now() / 1000) - 3660 });
    await authenticate(stub.env, authRequest());
    expect(lastSeenWrites(stub.writes)).toBe(1);
  });

  it('last_seen 为 NULL ⇒ 写一次（首次见到该设备）', async () => {
    const stub = makeStub({ deviceLastSeen: null });
    await authenticate(stub.env, authRequest());
    expect(lastSeenWrites(stub.writes)).toBe(1);
  });
});
