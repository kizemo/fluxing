-- Fluxing 云同步 · D1 初始结构
-- 基准：.specify/specs/100-fluxing-cloud-account-sync/plan.md §6.2
--
-- 与 §6.2 基线的刻意偏差（全部记进 docs/archive/handoffs/ 的本批次交接）：
--   D1. refresh_tokens 增加 kind 列（'access' | 'refresh'）。
--       理由：access token 也走「不透明随机串 + 服务端只存哈希」，这样撤销设备 / 清空账号
--       能立刻让已发出的令牌失效；代价是给 §6.2 加一列，换取「无签名密钥、无 JWT 校验库」。
--   D2. 新增 email_verifications 表。理由：§6.2 未覆盖邮箱验证码的生命周期
--       （过期 / 重发节流 / 错误次数上限），塞进 users 表会变成四五个可空列。
--   D3. sync_items.id 使用 AUTOINCREMENT，充当 §6.1 的 server_seq。
--       理由：AUTOINCREMENT 保证 SQLite 不复用 rowid，删除后再插不会回退序号，
--       客户端的「since 增量拉」才不会被漏推或重放。
--   D4. sync_items 增加 nonce_b64 列。理由：nonce 属于信封头而非同步内容，
--       放 D1 让 pull 有权威来源，不必依赖 R2 自定义元数据（元数据是尽力而为的）。
--   D5. 新增 mail_outbox 表，仅供 MAIL_MODE="outbox"（本地 / 测试）使用。
--       生产走 send_email 绑定，不写本表；因此生产代码里不存在「取验证码后门」。
--   D6. devices 表加 UNIQUE(user_id, device_id)，保证同一台机器重复登录只留一行。

CREATE TABLE users (
  id             TEXT PRIMARY KEY,
  email          TEXT NOT NULL UNIQUE,
  -- verifier = base64(HMAC-SHA256(AUTH_PEPPER, auth_key))，见 plan §3.2
  verifier       TEXT NOT NULL,
  -- salt = base64(16 字节随机)，客户端用它跑 PBKDF2；服务端不复用
  salt           TEXT NOT NULL,
  kdf_algo       TEXT NOT NULL,
  kdf_iters      INTEGER NOT NULL,
  email_verified INTEGER NOT NULL DEFAULT 0,
  created_at     INTEGER NOT NULL
);

CREATE TABLE devices (
  id         TEXT PRIMARY KEY,
  user_id    TEXT NOT NULL REFERENCES users(id),
  name       TEXT NOT NULL,
  device_id  TEXT NOT NULL,
  last_seen  INTEGER NOT NULL,
  revoked_at INTEGER,
  created_at INTEGER NOT NULL
);
CREATE UNIQUE INDEX idx_devices_user_device ON devices(user_id, device_id);

CREATE TABLE refresh_tokens (
  id         TEXT PRIMARY KEY,
  user_id    TEXT NOT NULL REFERENCES users(id),
  -- 指向 devices.id（行主键），不是客户端上报的 device_id
  device_id  TEXT NOT NULL,
  kind       TEXT NOT NULL,
  token_hash TEXT NOT NULL UNIQUE,
  expires_at INTEGER NOT NULL,
  revoked_at INTEGER,
  created_at INTEGER NOT NULL
);
CREATE UNIQUE INDEX idx_tokens_hash ON refresh_tokens(token_hash);
CREATE INDEX idx_tokens_user ON refresh_tokens(user_id);

CREATE TABLE sync_items (
  id          INTEGER PRIMARY KEY AUTOINCREMENT,
  user_id     TEXT NOT NULL REFERENCES users(id),
  item_key    TEXT NOT NULL,
  type        TEXT NOT NULL,
  hlc_ms      INTEGER NOT NULL,
  hlc_counter INTEGER NOT NULL,
  hlc_device  TEXT NOT NULL,
  r2_key      TEXT NOT NULL,
  nonce_b64   TEXT NOT NULL,
  cipher_len  INTEGER NOT NULL,
  updated_at  INTEGER NOT NULL
);
-- 每用户每 item_key 只留一行最新版本
CREATE UNIQUE INDEX idx_sync_user_item ON sync_items(user_id, item_key);
-- 增量拉：WHERE user_id = ? AND id > ? ORDER BY id
CREATE INDEX idx_sync_user_id ON sync_items(user_id, id);

CREATE TABLE email_verifications (
  id          TEXT PRIMARY KEY,
  email       TEXT NOT NULL,
  -- base64(HMAC-SHA256(AUTH_PEPPER, "email|code|created_at"))：
  -- 6 位码裸哈希可被 10^6 穷举，必须加 pepper
  code_hash   TEXT NOT NULL,
  attempts    INTEGER NOT NULL DEFAULT 0,
  expires_at  INTEGER NOT NULL,
  consumed_at INTEGER,
  created_at  INTEGER NOT NULL
);
CREATE INDEX idx_email_verify ON email_verifications(email, created_at);

CREATE TABLE mail_outbox (
  id         INTEGER PRIMARY KEY AUTOINCREMENT,
  to_addr    TEXT NOT NULL,
  subject    TEXT NOT NULL,
  body       TEXT NOT NULL,
  created_at INTEGER NOT NULL
);