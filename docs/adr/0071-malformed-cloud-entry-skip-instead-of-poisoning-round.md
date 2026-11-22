# ADR 0071 — 云端形态非法的旧条目不再毒化整轮同步

- **Status**: Accepted
- **Date**: 2026-11-22
- **Spec**: [100-fluxing-cloud-account-sync](../../.specify/specs/100-fluxing-cloud-account-sync/spec.md) —— **不改规格**：FR-016 只要求界面显示「最近一次同步时间与结果」；本 ADR 修的是**同步引擎内部的数据健壮性**，不动 FR 编号与措辞
- **Revises**: —（补齐 ADR 0039 / 0055 / 0062 / 0070 之后的同步客户端健壮性缺口，不推翻既有决策）
- **Lessons**: `.specify/memory/lessons-learned.md`
- **Supersedes / superseded by**: —

---

## 背景

### 事实 1 — 云端一条 payload 无制表符的旧条目毒化整轮同步

批次 BL 真机复验（2026-11-22 00:30 / 00:31）把 1017「最近同步」行的报错从「访问令牌已过期，请用刷新令牌续期。」**前移**为更靠后的本地阶段错误：

```
最近同步：2026-11-22 00:31:xx · 失败（词典条目载荷格式不正确。）
```

「词典条目载荷格式不正确。」在**全仓唯一**的产点是 `WeaselServer/Cloud/CloudUserDictIo.cpp` 的 `BuildUserDictSnapshots`（`SplitDictPayload` 失败 ⇒ 整体 `return false`）。调用链：

```
CloudSyncCoordinator::SyncOnce
  → OpenItemsTolerant        ← 只做「解密 / 拆封 / MAC 复核」，**不校验 payload 形态**
  → MergeLocalAndRemote      ← 形态非法的云端条目进入合并集
  → EntriesToDictRows
  → BuildUserDictSnapshots   ⇒ 整轮失败（写回阶段）
```

⇒ 一条**云端**的旧条目（payload 不含制表符）进入合并集后，在**写回阶段**让整轮同步失败 ⇒ 此后每一轮都失败。

### 事实 2 — 本地已被证伪

当轮导出的用户词典快照共 94 607 行，其中数据行 94 601 行**全部 3 列**；6 行非 3 列全部是 `#` 元信息 / 头行。⇒ 当轮元凶在**云端**，不在本地。

### 事实 3 — 脏云端条目**无法**在现有 API 下清除

服务端 `cloud/fluxing-sync/src/index.ts` 只有 `/v1/health`、`/v1/auth/*`、`/v1/sync/push`、`/v1/sync/pull`、`/v1/devices`、`/v1/account/data`，**没有删条目 API**。⇒ 只能靠客户端**跳过**。

### 事实 4 — `JoinDictRow` 的空串返回是潜在 API 陷阱（**当前调用链不可达**）

`JoinDictRow`（`CloudUserDictIo.cpp`）在形态非法时返回**空串**，调用方 `BuildUserDictSnapshots` 会把它写成**空行**。

> **更正**：BL handoff §14 与 `reports/fluxing-bl-syncstatus/analysis-20261122.md` 称这是「当前正在丢数据」——**不准确**。
> `BuildUserDictSnapshots` 的**第一轮**分组循环就已经用 `SplitDictItemKey` + `SplitDictPayload` 校验并 `return false`，第二轮的 `JoinDictRow` 必然成功 ⇒ 空串路径在当前调用链上**走不到**。
> 它是**公开 API 层面的陷阱**，不是当轮丢数据。改签名是消除陷阱，**不得**宣称为「当前正在丢数据」的修复。

### 事实 5 — 本地侧存在同类结构洞

`ParseUserDictSnapshots` 对数据行只要求 `cols.size() >= 2`；**2 列**行会产出**无制表符**的 payload（`entry.payload = cols[0]`，不补 `"\t" + cols[2]`），同样会在写回阶段炸掉整轮。
本机当前快照无此行（事实 2 已证伪当轮元凶），但结构上是同类洞。

### 事实 6 — 判定口径与写回口径是两套代码

形态合法性此前**只在写回阶段**被检查（`BuildUserDictSnapshots` 第一轮）。开箱侧（`OpenItems` / `OpenItemsTolerant`）与解析侧（`ParseUserDictSnapshots`）**都不检查** ⇒ 「谁能进合并集」与「谁能把合并集写回去」两套口径漂移，脏数据得以穿过去。这是本缺陷的**结构性根因**。

### 事实 7 — 分页拉取的计数会被**逐页覆盖**（本批自测发现）

`OpenItemsTolerant` 保留「单次调用把出参清零」的语义（`*undecryptable = 0;` / `*malformed = 0;`），
而 `CloudSyncCoordinator::SyncOnce` 的拉取循环（`kMaxPullPages = 10`，`kPullPageLimit = 100`）
把**同一个**变量逐页传进去 ⇒ 第 2 页会把第 1 页的计数**清零覆盖**。
⇒ 多页拉取时，本轮 message 只报**最后一页**的条数。

该形态**同时**命中 `undecryptable`（既有计数，非本批引入）与 `malformed`（本批引入）。
它**不影响**核心行为（脏条目照样被跳过、整轮照样不失败），只影响 message 里**条数的准确性**。

---

## 决策

1. **单一真相源**：新增纯函数
   `bool IsValidUserDictEntryShape(const std::string& item_key, const std::string& payload)`
   （`WeaselServer/Cloud/CloudUserDictIo.h`），内部复用 `SplitDictItemKey` + `SplitDictPayload`
   —— 与写回阶段 `BuildUserDictSnapshots` 的校验**完全一致**（宪法 R7：生产与单测同源、判定口径不得分叉）。

2. **开箱侧主修**：`OpenItemsTolerant` 对 **userdb** 类条目做形态校验；非法者**跳过**并**单独计数**（`malformed`），不再进入合并集。
   - **只对 userdb 生效**：`phrases` / `config` 的 payload 不是词典行形状，**不能**拿词典口径去卡它们（否则会误伤短语条目）。
   - **不改 `OpenItems` 的既有签名与语义**（沿用 `.specify/specs/100-fluxing-cloud-account-sync/tasks.md` 的既有约定）：形态校验只加在 tolerant 层；`OpenItems` 的生产唯一调用方就是 tolerant 层 ⇒ 修复效果等价。
   - 沿用既有先例：旧密钥加密行本就按 `undecryptable` **跳过**，本次是同一机制的第二种「不可用」。

3. **本地侧收窄**：`ParseUserDictSnapshots` 用同一个函数跳过形态非法的数据行 ⇒ 本地也不再能毒化整轮。

4. **`JoinDictRow` 改为报错**：签名改为
   `bool JoinDictRow(const CloudDictEntry&, std::string* out, std::string* err)`，
   形态非法 ⇒ false + 中文原因，**不再返回空串**（消除事实 4 的公开 API 陷阱）。

5. **写回阶段保持 fail loud**：`BuildUserDictSnapshots` 遇到形态非法仍然**整体失败**，不改成跳过。
   靠「开箱侧 + 本地侧」双关卡保证合并集里不会有非法行，从而维持它的 **canary 价值**（真出现新的形态漂移时，写回阶段仍然会响）。

6. **计数与文案分开**：`undecryptable` 保持原义（解不开 / 类型不可识别）；
   形态非法**单独计数**并写进本轮 message（「云端有 N 条格式非法的条目已跳过」）。
   **不占用** `items_skipped`（其原义是 HLC 守卫跳过）。
   **不新增** `CloudSyncRoundResult` 字段。

7. **续期路径升级为直证**：`CloudSyncService` 记录 `token_refreshed()`；协调器把它写进本轮 message（「访问令牌已自动续期」）
   ⇒ 把 BL §13「续期路径在真机生效」的**强推断**升级为**人眼可见的直证**（1017 行）。
   同时在续期分支与「跳过形态非法条目」分支各落一条**无条件**的诊断行
   （`fluxing::diag::AppendLine`，**文件型**，不依赖调试器 —— ADR 0066 已坐实 `OutputDebugStringW` 无调试器时抓不到）。
   诊断行**不含任何条目明文**（云端 item_key 是 MAC；解密后的明文键是用户词条，**不落盘**）。

8. **不动 IPC 协议 / 不动控件 ID / 不动布局表 / 不加控件 / 不改 `librime/` / 不新增第三方依赖 / 不 bump 版本号**。

9. **分页计数必须累加**（事实 7）：`SyncOnce` 的拉取循环改为「每页用**局部**计数器接收 `OpenItemsTolerant` 的出参，再**累加**到轮次总量」，
   `OpenItemsTolerant` 的「单次调用清零」语义**保持不变**（单测直接调用它时仍能拿到该页的干净计数）。
   ⇒ 同时修好 `malformed`（本批引入）与 `undecryptable`（既有）两个计数。

---

## 后果

- ✅ 一条云端脏条目不再让**每一轮**同步失败；同步恢复可用。
- ✅ 脏数据的存在**如实可见**（本轮 message 带跳过条数），不是静默吞掉。
- ✅ 「谁进合并集」与「谁能写回去」的口径**合流**，不再漂移（结构性根因被移除）。
- ⚠️ **脏云端条目不会被清除**（事实 3：服务端无删条目 API）：它们仍在云端，每轮都会被拉取、被跳过。
  要真正清除，用户需显式「清空云端数据」后全量重推。**本决策只保证「跳过不毒化整轮」，不承诺「清除云端脏数据」。**
- ⚠️ `OpenItemsTolerant` 增加出参 `malformed`、`JoinDictRow` 签名变化 —— 两者都**无外部调用者**（生产各 1 处、单测 0 处），影响面仅限本模块。
- ⚠️ 1017 仍是**人眼观测面**，**不升级为闸门判据**（沿用 ADR 0069 / 0070 口径）；「维护闸门是否放行 = 导出文件 mtime 是否前进」的机读口径继续有效。
- ♻️ 本决策**可逆**：撤销两处形态校验即可回到旧行为（但会恢复「一轮失败」的老症状）。
