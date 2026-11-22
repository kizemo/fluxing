# ADR 0072 — 成功态「最近同步」显示本轮提示：结构化 notices 通道

- **Status**: Accepted
- **Date**: 2026-11-22
- **Spec**: [100-fluxing-cloud-account-sync](../../.specify/specs/100-fluxing-cloud-account-sync/spec.md) —— **不改规格**：FR-016 只要求界面显示「最近一次同步时间与结果」；本 ADR 补齐的是**实现缺口**（成功态看不到本轮提示），不动 FR 编号与措辞
- **Revises**: —（补齐 ADR 0069 / 0070 / 0071 之后的观测面缺口，不推翻既有决策）
- **Lessons**: `.specify/memory/lessons-learned.md`
- **Supersedes / superseded by**: —

---

## 背景

### 事实 1 — 成功态 1017 独立拼装，**不消费**轮次 message

ADR 0069 把「最近一次同步时间与结果」写进 `kIdSyncStatus`(1017)；ADR 0071（批次 BM）又让轮次 message 带上
「云端有 N 条格式非法的条目已跳过」与「访问令牌已自动续期」两条人眼证据。

但成功态**没有**读 message。`WeaselServer/Cloud/CloudAccountDialog.cpp` 的 `RunSyncRound` 成功分支只调：

```
AccountSyncStatusTextSuccess(when, out->items_pushed, out->items_pulled)
```

—— 时间 + 两个计数，**`out->message` 被丢弃**。⇒ BM 刚做出来的两条证据，在**成功态**看不见；
只有**失败态**（`AccountSyncStatusTextFailed(when, reason)`）与**跳过态**消费 message。

### 事实 2 — 手动同步的 1006 状态行同样丢弃 message

`CloudAccountDialog.cpp` 的手动同步兜底路径（引擎没起来时直接跑一轮）也独立拼装
「同步完成：推送 N 条，拉取 M 条。」，同样丢弃 message。

### 事实 3 — 根因在 ADR 0069 的「成功态独立拼装」设计，**不是** BM 引入

message 是**中文 blob**。成功态若要显示它，只能靠**解析中文**（脆弱、与 message 文案强耦合，
改一个字就断）。ADR 0069 当时选了「成功态独立拼装」，ADR 0071 完全没触碰该路径 ⇒ 缺口一直存在，
只是此前**没有观测面**（1017 永远停在占位文案）而无人看见。

### 事实 4 — 结构体位置决定了本决策**不触发** IPC 四边同步红线

`CloudSyncRoundResult` 定义在 `WeaselServer/Cloud/CloudSyncEngine.h`（**模块内部头**），
**不在** `include/WeaselIPCData.h` ⇒ 新增字段不构成 IPC 协议变更，
无需 server / TSF / deployer 三端同步改动 + 协议 ADR + `test/TestWeaselIPC/*` 测试。

---

## 决策

1. **结构化 notices 通道**：`CloudSyncRoundResult` 新增
   `std::vector<std::string> notices;`（窄字符 UTF-8 中文短句）。
   **不**解析中文 message blob，**不**新增任何控件、**不**新增任何 IPC 消息类型。

2. **notices 与 message 同源**：在 `CloudSyncCoordinator::SyncOnce` 既有的 message 拼装处，
   每段可选信息**用同一个字符串变量**既拼进 message、又推进 notices
   （`count` 之类的数字也共用同一个 `std::string`）⇒ 单测可逐字断言
   「**每条 notice 都逐字出现在 message 中**」，两者不会漂移（宪法 R7 单一真相源）。

3. **message 逐字不变**：既有 message 的**每一个字节**保持原样（BM / ADR 0071 的期望值、
   诊断日志、既有探针期望值全部零影响）。`notices` 是**新增通道**，不是 message 的替代品。

4. **条件追加（保底不变量）**：`CloudAccountUi.h` 新增
   `AccountSyncStatusTextSuccessWithNotices(when, pushed, pulled, notices)`：
   - notices 为空 ⇒ 返回值与既有 `AccountSyncStatusTextSuccess` **逐字相同**；
   - notices 非空 ⇒ 在「成功（…）」括号内**追加**提示。
   ⇒ 既有 BK 单测断言（`L"最近同步：2026-11-21 22:30:00 · 成功（推送 3 条，拉取 7 条）"`）
   与既有真机探针的期望值**零影响**。

5. **分隔符用全角分号「；」**：多条提示之间用 `；` 连接（不是 `，`）——
   各条提示是**独立子句**，用逗号会与「推送 N 条，拉取 M 条」里的逗号混淆。

6. **手动同步 1006 一并消费**：手动同步兜底路径改用
   `AccountSyncManualDoneTextWithNotices(pushed, pulled, notices)`
   （基础串「同步完成：推送 N 条，拉取 M 条。」**逐字不变**，追加时句末「。」保留）。
   理由：同一轮同步的同一批提示，只在一处显示 = 口径漂移的入口。

7. **既有七个文案函数逐字不动**：本 ADR 只**新增**两个函数，
   不改动 `CloudAccountUi.h` 中既有七个文案函数的任何字节（探针期望值逐字比对它们）。

8. **不动 IPC 协议 / 不动控件 ID / 不动布局表 / 不加控件 / 不改 `librime/` /
   不新增第三方依赖 / 不 bump 版本号**（理由见事实 4）。

9. **单测护栏**（批次 BN 组）：保底不变量（无提示 ⇒ 逐字不变）、单条提示、多条提示、
   既有 BK 断言零改动、协调器级 notices/message 同源断言、
   「每条 notice 都逐字出现在 message 中」。

---

## 后果

- ✅ 「云端有 N 条格式非法的条目已跳过」「访问令牌已自动续期」在**成功态**1017 **可见**。
- ✅ 不靠解析中文 blob ⇒ message 文案可自由演进，界面提示不受影响。
- ✅ 无提示时界面文案**逐字不变** ⇒ 既有单测与探针零影响（回归风险最小）。
- ✅ 手动同步 1006 与最近同步 1017 **同一机制**，口径不分叉。
- ⚠️ 成功态文案变长（提示多时括号内会变长）—— 1017 是**单行** STATIC 控件，
  极端多提示时可能**显示不全**；本决策**不做**自动换行 / 省略号截断（避免引入新的视觉口径）。
- ⚠️ `CloudSyncRoundResult` 新增字段 —— 该结构体是模块内部类型，无外部消费者，影响面仅限本模块。
- ⚠️ 1017 仍是**人眼观测面**，**不升级为闸门判据**（沿用 ADR 0069 / 0070 / 0071 口径）。
- ♻️ 本决策**可逆**：删掉 notices 通道并让 1017 回到独立拼装即可（旧症状 = 成功态看不到提示）。
