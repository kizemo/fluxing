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
  /**
   * 每账号每 UTC 日的 D1 行写入预算上限（ADR 0076 D2-3）。
   * 缺省 80,000 行；读不到 / 非法值时回落到代码内常量（见 writeBudgetRowsPerDay）。
   */
  WRITE_BUDGET_ROWS_PER_DAY?: string;
}

/**
 * 每日写预算护栏的默认上限（行 / 账号 / UTC 日）。
 *
 * 取值依据（ADR 0076 Context 1.1）：D1 免费额度按**行写入**计，实测一次全量推
 * （40,061 条）≈ 80k 行写入 ≈ 当日额度的 40% ⇒ 80,000 行 ≈「允许一次全量重推、
 * 但不允许第二次」。真值随 D1 套餐变化，改额度时改环境变量即可（不改代码）。
 */
export const DEFAULT_WRITE_BUDGET_ROWS_PER_DAY = 80_000;

/** 上限的可配置区间：1 行 ~ 10,000,000 行。超出区间视为配置非法 ⇒ 回落默认值。 */
const WRITE_BUDGET_ROWS_PER_DAY_MIN = 1;
const WRITE_BUDGET_ROWS_PER_DAY_MAX = 10_000_000;

/**
 * 读取「每账号每日 D1 行写入预算上限」。
 *
 * 口径与 auth.ts 的 intVar 一致：解析失败 / 越界 ⇒ 回落默认常量，**不** fail closed。
 * 理由：护栏是「保护额度」而不是「授权」—— 配置缺失不该让正常同步全挂；
 * 真要临时放开，把环境变量调大即可。
 */
export function writeBudgetRowsPerDay(env: Env): number {
  const raw = env.WRITE_BUDGET_ROWS_PER_DAY;
  if (typeof raw !== 'string' || raw.trim().length === 0) {
    return DEFAULT_WRITE_BUDGET_ROWS_PER_DAY;
  }
  const parsed = Number.parseInt(raw, 10);
  if (
    !Number.isFinite(parsed) ||
    parsed < WRITE_BUDGET_ROWS_PER_DAY_MIN ||
    parsed > WRITE_BUDGET_ROWS_PER_DAY_MAX
  ) {
    return DEFAULT_WRITE_BUDGET_ROWS_PER_DAY;
  }
  return parsed;
}
