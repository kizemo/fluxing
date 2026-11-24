-- Fluxing 云同步 · 桶级索引表（ADR 0077 D1）
-- 基准：docs/adr/0077-bucket-level-sync-storage-architecture.md D1 / D5 / D7 / D8
--
-- 目的：把 D1 的「一行 = 一个词条」改成「一行 = 一个（设备 × 桶）」，
-- 词条包（既有 8 行信封文本串接而成的 blob）整体落 R2 ⇒ **D1 行数与词条数解耦**。
--
-- 与 sync_items 的刻意差异（全部记进本批次交接）：
--   B1. 多一列 device_id，UNIQUE 键变成三元 (user_id, device_id, bucket_key)。
--       理由：ADR 0077 D5「每设备独立视图」—— 设备 A 与设备 B 推同一个 bucket_key
--       必须是**两行、两个 R2 对象**，否则后推者整桶覆盖前者（lost update）。
--       device_id 存 **devices.id（行主键）**，不是客户端上报的 HLC device 字符串；
--       口径与 refresh_tokens.device_id 一致（0001_init.sql 第 46-47 行原句：
--       「指向 devices.id（行主键），不是客户端上报的 device_id」）。
--       注意与 hlc_device 区分：hlc_device 是**信封内**的 HLC device 字段（客户端自由
--       取值，参与 compareHlc 的 tiebreak），device_id 是**服务端行归属**。
--   B2. 没有 nonce_b64 列。理由：ADR 0077 D3 明确**不做桶级二次加密**，桶 blob =
--       N 个既有条目级信封文本块直接串接，nonce 属于**条目级**信封头、已在 blob 内 ⇒
--       桶级既无 nonce 也无桶级 AAD。
--   B3. 多一列 item_count（该桶内的条目数）。理由：ADR 0077 D2「桶内条目上限 4096，
--       超限时客户端降级老端点逐条推」⇒ 服务端需要能校验并留存该上界判据；同时它
--       是运维观测「桶是否被写满」的唯一 D1 侧指标（服务端不解析 blob）。
--   B4. cipher_len 语义从「单条密文长度」变为「桶 blob 字节数」（条目级密文长度的
--       逐条校验仍由客户端在 blob 内的信封字段完成）。
--   B5. id 同样是 INTEGER PRIMARY KEY AUTOINCREMENT，但它是**桶侧独立的编号空间**：
--       ADR 0077 新增约束「server_seq 编号空间在两套端点间不同 ⇒ 侧车必须按协议版本
--       分区，禁止跨版本复用游标」⇒ 本表 id 与 sync_items.id 互不相干，绝不交叉比较。
--   B6. 额外建 (user_id, updated_at) 索引。理由：ADR 0077 D8 的写预算护栏 COUNT 换到
--       本表，与 0003_sync_items_updated_index.sql 对 sync_items 的同构处理保持一致。

CREATE TABLE sync_buckets (
  id          INTEGER PRIMARY KEY AUTOINCREMENT,
  user_id     TEXT NOT NULL REFERENCES users(id),
  -- 指向 devices.id（行主键），不是客户端上报的 device_id；见文件头 B1
  device_id   TEXT NOT NULL,
  -- bucket_key = MAC(enc_key, "bucket:" + dict_name + ":" + shard) 的十六进制串（ADR 0077 D2）
  -- 服务端只当不透明键用，绝不尝试反解词典名
  bucket_key  TEXT NOT NULL,
  hlc_ms      INTEGER NOT NULL,
  hlc_counter INTEGER NOT NULL,
  hlc_device  TEXT NOT NULL,
  r2_key      TEXT NOT NULL,
  cipher_len  INTEGER NOT NULL,
  item_count  INTEGER NOT NULL,
  updated_at  INTEGER NOT NULL
);
-- 每（用户 × 设备 × 桶）只留一行最新版本（ADR 0077 D5）
CREATE UNIQUE INDEX idx_sync_buckets_user_device_item
  ON sync_buckets(user_id, device_id, bucket_key);
-- 增量拉：WHERE user_id = ? AND id > ? ORDER BY id（ADR 0077 D7 的游标语义）
CREATE INDEX idx_sync_buckets_user_id ON sync_buckets(user_id, id);
-- 写预算护栏：WHERE user_id = ? AND updated_at >= ?（ADR 0077 D8）
CREATE INDEX idx_sync_buckets_user_updated ON sync_buckets(user_id, updated_at);
