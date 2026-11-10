// 运行时绑定与变量。与 wrangler.toml 一一对应。

export interface Env {
  /** D1：账号 / 设备 / 令牌 / 同步索引（绝不含同步内容） */
  DB: D1Database;
  /** R2：密文对象（plan §6.3） */
  BUCKET: R2Bucket;
  /** 服务端 pepper，走 Workers Secret；缺失即 fail closed，绝不回退默认值 */
  AUTH_PEPPER?: string;
  ENVIRONMENT?: string;
  ACCESS_TOKEN_TTL?: string;
  REFRESH_TOKEN_TTL?: string;
  KDF_ALGO?: string;
  KDF_ITERS?: string;
  MAIL_MODE?: string;
  MAIL_FROM?: string;
  /** 生产发信绑定（[[send_email]] name = "EMAIL"），本地不存在 */
  EMAIL?: { send: (message: unknown) => Promise<unknown> };
}