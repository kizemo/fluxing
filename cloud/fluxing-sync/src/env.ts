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
  /** 发件人显示名（可选；MAIL_MODE="http" 时拼成「显示名 <地址>」） */
  MAIL_FROM_NAME?: string;
  /** 第三方 HTTPS 邮件 API 端点（MAIL_MODE="http"），默认 Resend */
  MAIL_HTTP_ENDPOINT?: string;
  /** 第三方邮件 API 密钥，走 Workers Secret：绝不进代码库 / D1 / 日志 */
  MAIL_HTTP_API_KEY?: string;
  /** 鉴权头名与前缀（默认 Authorization + "Bearer "） */
  MAIL_HTTP_AUTH_HEADER?: string;
  MAIL_HTTP_AUTH_PREFIX?: string;
  /** 生产发信绑定（[[send_email]] name = "EMAIL"），本地不存在 */
  EMAIL?: { send: (message: unknown) => Promise<unknown> };
}