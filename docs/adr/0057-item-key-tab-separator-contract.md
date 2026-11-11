# 0057. item_key 的 TAB 分隔符是客户端既有契约，服务端字符集必须放行 TAB

- 状态：已接受 (accepted)
- 日期：2026-11-11
- 关联：ADR 0039（云端账号与多端同步总体架构）、ADR 0054（客户端密钥派生）、ADR 0055（同步引擎调度与收敛）、spec 100 `tasks.md` T502 / T503、批次 AH 收尾记录
- 代码：`cloud/fluxing-sync/src/wire.ts`（改）、`cloud/fluxing-sync/test/api.test.ts`（改）
- 测试：`cloud/fluxing-sync/test/api.test.ts`（TAB 放行 + 换行仍拒 2 条）、`test/TestFluxingCloud/TestFluxingCloud.cpp`（跨实现固定向量 +4 条）
- 探针：`scripts/cloud/live-e2e-probe.mjs`（新建，线上闭环 19/19 断言）

## Context

批次 AH 为取 spec 100 一直挂着的**线上判据**（`push` 响应的 `skipped`、`pull` 顶层 `server_seq`、AC-01 服务端侧、AC-07 密文抽查），写了一个可复跑的线上闭环探针 `scripts/cloud/live-e2e-probe.mjs`。探针**首跑**在 `push` 一步拿到 **HTTP 400**，逐字段比对后定位到根因：

| 一侧 | 事实 | 出处 |
|---|---|---|
| 客户端 | `item_key` 恒为 `"<词典名>\t<词条>"`（**TAB = 0x09**），`SplitDictItemKey` 按 TAB 拆回；sidecar 另有 `EscapeKey` / `UnescapeKey` 专门处理 TAB | `WeaselServer/Cloud/CloudUserDictIo.cpp` |
| 服务端 | `assertHeaderString` 用 `/[\u0000-\u001f\u007f]/` **一律拒绝所有 C0 控制字符**，TAB 也在内 | `cloud/fluxing-sync/src/wire.ts` |

服务端这条规则的 rationale 是「`item_key` 与 `type` 要能安全拼进行式协议 ⇒ 禁行分隔符」，**但把 TAB 一并禁掉属于过严** —— TAB 在本契约里是**字段分隔符**，不是行分隔符。

两边各自都没错，但合起来 ⇒ **真机每一次 userdb / phrases 推送都会被服务端 400 拒绝**。这不是探针构造出来的边角情况：真实客户端的词库推送**每一次**都命中同一个 400，用户侧表现将是「云同步一直失败但没有明确原因」。

## Decision

1. **放行 TAB，保留其余控制字符禁令。** 服务端 `assertHeaderString` 的正则由 `/[\u0000-\u001f\u007f]/` 改为 `/[\u0000-\u0008\u000a-\u001f\u007f]/` —— **TAB (0x09) 合法**；`\n` (0x0A) / `\r` (0x0D) 及其它 C0 控制字符、0x7F **仍拒**。

2. **契约源头在客户端，不动客户端。** 键格式是客户端既有且已固化的行为（`MakeDictItemKey` / `SplitDictItemKey` / sidecar 的 TAB 转义），改客户端键格式要迁数据、改 sidecar 编码、改合并逻辑，代价与风险都远大于放宽服务端一条正则 ⇒ **服务端适配客户端**。

3. **不动 `accepted` / `skipped` 的响应形状。** T503 已裁定 `skipped` 为**字符串数组**（与 `accepted` 同构），本决策不触碰响应契约。

4. **补服务端回归用例钉死边界**：① `item_key` 含 TAB **必须放行**；② 含换行 **仍必须被拒** —— 防止后续把正则一路放宽成「全放行」。

## Consequences

- ✅ 真机 userdb / phrases 推送不再被 400 拒绝 —— 这是本批修掉的**唯一真机可见的生产缺陷**（探针实测 **400 → 200**）。
- ✅ 修复面最窄：1 条正则 + 1 段注释 + 2 条用例，**不涉及数据迁移、不涉及客户端改动、不涉及协议形状**。
- ✅ 契约此后写在服务端代码注释里：**TAB 是 `item_key` 的合法字符**；任何后续收紧 `item_key` 字符集的改动都必须先回到客户端键格式核对。
- ⚠️ **遗留**：客户端 `Push` 解析**不消费** `skipped`（只读 `server_seq`）—— 不在本批验收口径内（那需要真机双机判定两端并集一致性），已记为遗留风险项。
- ⚠️ **遗留**：此前服务端「禁控制字符」与客户端键格式之间**没有任何一处对齐说明** —— 同类不一致（两侧对同一字段的字符集假设不同）应视为**契约缺陷**候选，未来的契约类改动两端要一起看。
- ✅ 可回退：把正则改回 `/[\u0000-\u001f\u007f]/` 即恢复旧行为（但会把真机推送重新打回 400）。
