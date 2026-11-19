-- Fluxing 云同步 · D1 迁移 0002：独立的「重置口令验证码」表
--
-- 取舍：不给 email_verifications 加 purpose 列，而是新建独立的 password_resets 表。
--   理由 1（注册主路径零改动）：handleVerify 一行都不用改 ⇒ 注册语义逐字不变，
--           既有注册验证码的 code_hash 公式也不用动（老码继续可用）。
--   理由 2（结构性隔离）：两类码物理分表 ⇒ 注册码永远查不到重置码那一侧，反之亦然。
--           不依赖「查询时记得加 WHERE purpose='reset'」这种人工约定，天然互不通用。
--   代价：验证码生命周期（TTL / 错误次数 / 消费标记 / 重发节流）在两处各写一份。
--         这是本批刻意接受的重复，换取注册主路径零风险。
--
-- 列语义与 email_verifications（见 0001 D2）逐一对齐，便于两套逻辑互相参照。

CREATE TABLE password_resets (
  id          TEXT PRIMARY KEY,
  email       TEXT NOT NULL,
  -- base64(HMAC-SHA256(AUTH_PEPPER, "password-reset-v1|email|code|created_at"))：
  -- 6 位码裸哈希可被 10^6 穷举，必须加 pepper；前缀串与注册码不同
  -- （注册码是 "email-verify-v1|..."）⇒ 即使同一秒、同一邮箱、恰好同一个 6 位码，
  -- 两类码的哈希也不同，绝不可能跨用途通用。
  code_hash   TEXT NOT NULL,
  attempts    INTEGER NOT NULL DEFAULT 0,
  expires_at  INTEGER NOT NULL,
  consumed_at INTEGER,
  created_at  INTEGER NOT NULL
);
-- 取最新一条重置码：WHERE email = ? ORDER BY created_at DESC LIMIT 1
CREATE INDEX idx_password_resets_email ON password_resets(email, created_at);
