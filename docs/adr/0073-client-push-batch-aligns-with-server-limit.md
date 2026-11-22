# ADR 0073 — 客户端推送分批对齐服务端上限：单批 64 条 + 编译期护栏

- **Status**: Accepted
- **Date**: 2026-11-22
- **Spec**: [100-fluxing-cloud-account-sync](../../.specify/specs/100-fluxing-cloud-account-sync/spec.md) —— **不改规格**：spec 只要求同步可用与状态可见；本 ADR 修的是**实现与线协议的硬契约错配**
- **Revises**: —（补齐 ADR 0069 / 0070 / 0071 / 0072 之后暴露的传输层缺陷，不推翻既有决策）
- **Lessons**: `.specify/memory/lessons-learned.md`
- **Supersedes / superseded by**: —

---

## 背景

### 事实 1 — 服务端有**硬上限 64**，且是有意设计

`cloud/fluxing-sync/src/wire.ts:33`：

```
export const MAX_PUSH_ITEMS = 64;
```

`cloud/fluxing-sync/src/sync.ts:98-103`：

```
if (rawItems.length > MAX_PUSH_ITEMS) throw fail.badRequest(... 'too_many_items')
```

拒收原因文案（服务端下发、客户端原样透传到 1017）：

> 单次推送条目数上限为 64（实收 200）。

64 是**保护单请求写放大**的产品决策（一次请求 = 一次 D1 事务 + 一次 HLC 守卫扫描），
不是随手写的魔数。

### 事实 2 — 客户端按 200 分批，与服务端错配

`WeaselServer/Cloud/CloudSyncService.cpp` 的匿名 namespace 里：

```
constexpr size_t kPushBatchSize = 200;
```

推送循环（同文件 `SyncOnce` 第 5 步）按该常量切片：

```
for (size_t offset = 0; offset < push_indices.size(); offset += kPushBatchSize) {
  const size_t rest = push_indices.size() - offset;
  const size_t count = rest < kPushBatchSize ? rest : kPushBatchSize;
  ...
  service.Push(sealed, ...);
}
```

⇒ 只要本地待推条目 `>= 65`，**第一批就超限** ⇒ 服务端 400 ⇒ `Push` 返回 false ⇒
`push_ok = false` ⇒ **整轮同步失败**。且下一轮仍然从 0 开始切 200，
**同一批数据永远不会被切成小份** ⇒ 这不再是「偶发失败」，而是**永久卡死**：
待推条目数不降（推送从未成功）、本地改动继续累积，用户看到的是每一轮都失败。

### 事实 3 — 只有推送口径错配，拉取口径是对的

同一模块：`kPullPageLimit = 100`，服务端 `MAX_PULL_LIMIT = 100` —— **一致**。
⇒ 缺陷是孤立的常量错配，不是「客户端整体对服务端约束无感知」。

### 事实 4 — 该缺陷被 1017 观测面**如实暴露**

本缺陷此前不可见（1017 停在静态占位，见 ADR 0069），ADR 0069 补齐观测面后，
真机探针（`reports/fluxing-bk-syncstatus/probe-sync-status.ps1`）在批次 BO 首轮
即读到失败原因文本 ⇒ 观测面按设计生效。**本 ADR 不修改探针与 1017 文案机制。**

### 事实 5 — 为什么不改服务端

改 `MAX_PUSH_ITEMS` 需要：① 改 Worker 源码 ② 重新部署 Cloudflare Worker
③ 放弃「单请求写放大有上限」这条保护 ④ 已有旧客户端仍旧按自己的值发。
客户端改一个常量即可闭合，且**新旧客户端都能立刻受益于自己的值**。
⇒ **只改客户端**。

---

## 决策

### 决策 1 — 客户端单批 = 64，与服务端常量逐值对齐

`kPushBatchSize` 由 200 改为 64。不取更小值（如 32）：更小只是多几次往返，
并不增加正确性，且会让「130 条改动」这类正常体量的轮次多跑一批。

### 决策 2 — 常量上移到**头文件**，成为模块级单一真相源

原本 `kPushBatchSize` 定义在 `.cpp` 的匿名 namespace 里 —— 只有该编译单元可见，
无法被单测或其他编译单元引用，也无法与「服务端上限」这个外部事实并列书写。
现在两个常量一起放在 `CloudSyncService.h` 的 `namespace fluxing { namespace cloud {` 内、
`class ICloudTransport;` 前：

```
constexpr size_t kServerMaxPushItems = 64;   // 服务端真源：wire.ts MAX_PUSH_ITEMS
constexpr size_t kPushBatchSize = 64;        // 客户端分批口径

static_assert(kPushBatchSize <= kServerMaxPushItems,
              "推送单批不得超过服务端 MAX_PUSH_ITEMS，否则整批被拒");
```

`.cpp` 内**删除**本地定义，改为一行注释指向头文件 —— 避免两处定义分叉
（分叉是本次事故的成因：一个数字在服务端、一个数字在客户端，没有任何机制要求它们成对）。

### 决策 3 — 护栏用**编译期** `static_assert`，不靠运行时单测

「两个常量必须成对」是**编译期可判定的约束**。若只写运行时用例，
单测抓的是「某次构造的条目数」，而常量被改大后**多数用例仍然通过**
（假后端不执行服务端校验）。`static_assert` 让「把 kPushBatchSize 调大」这一动作
**直接编译失败**，这是最强、最省、最不可能被绕过的一层。

### 决策 4 — 不新增行为级用例，理由登记在此

候选用例「130 条本地改动 ⇒ 恰好 3 次 push 请求，且每次 <= 64 条」有判别力，
但当前 `MockTransport` 不模拟服务端上限校验：它按 FIFO 回预设 reply，
**无论收到多少条都成功** ⇒ 该用例只能断言「分批次数」，
对「常量被改大」这一真实回归**没有独立于 `static_assert` 的拦截力**。
要让它有拦截力，得在假后端里**复刻**服务端的 64 条校验 —— 那等于把服务端规则
抄一份到测试里，形成第三处真相源。⇒ **不加**。若将来假后端引入协议级校验，再补该用例。

### 决策 5 — 不动 IPC / 不动服务端 / 不加依赖 / 不 bump 版本号

- `CloudSyncService.h/.cpp` 属 `WeaselServer/Cloud/` 模块内部，**不涉及**
  `include/WeaselIPCData.h` ⇒ 不触发 IPC 四边同步红线。
- 服务端 `cloud/fluxing-sync/**` 一行未改 ⇒ 无需重新部署 Worker。
- 不新增第三方依赖、不改 `.clang-format`、不改 `librime/`。
- 装机版版本号仍 `1.3.8.0`；判定一律用产物 sha16 / 字节数。

---

## 后果

### 正面

- 本地待推 `>= 65` 条时不再永久失败；「令牌过期 → 续期 → 同步成功」这条端到端路径
  第一次具备被观测到的可能（此前被本缺陷挡住，**不是**续期功能没实现）。
- 常量错配这类缺陷有了编译期闸门，同类「客户端数字 vs 服务端数字」错配有可复制的护栏范式。

### 负面 / 取舍

- 单批变小 ⇒ 条目多时往返次数增加（1000 条 = 16 批而非 5 批）。这是服务端有意设计的
  写放大保护带来的成本，客户端无权绕过。
- 已装机旧版（BN 及更早）**仍然带 200**，会在待推 >= 65 时继续失败，直到装新包。
  ⇒ 本批必须重新打包装机，不能只交代码。

### 未覆盖

- 服务端其它上限（`MAX_PULL_LIMIT` 之外的字段长度、payload 大小）本次未逐一核对，
  只核对了本缺陷所在的推送条目数。
- 推送批次数与 `out->push_requests` 的关系仅靠代码阅读确认，未加用例断言（见决策 4）。

---

## 验证

- 构建：`build.bat release weasel installer` ⇒ `BUILD_EXIT=0`，weasel.sln Release|Win32
  **0 警告 0 错误**；makensis 仅 8 条**既有** `warning 7010`（与批次 BN 基线一致）。
- 单测：`TestFluxingCloud` ⇒ `RESULT: PASS=704 FAIL=0` / `VERDICT: ALL PASS`
  （与 BN 基线 704 **持平** —— 本批按决策 4 未新增用例；`exe.mtime = 14:06:17` 晚于源码
  ⇒ 非假绿）。
- 格式：`clang-format 18.1.8 --lines=起:止` 定点。头文件首轮抓到 **4 处中文宽字符列宽漂移**
  （`:56:97` / `:56:111` / `:57:1` / `:57:72`）⇒ `-i --lines=52:66` 就地修 ⇒ 重算区间复检
  `RECHECK_EXIT=0`；`.cpp` 侧 `CPP_EXIT=0`。
- L0 静态门禁：`scripts/gate/L0-static.ps1 -Mode all` ⇒ `checks=15 pass=15 fail=0 skip=0`。
- 产物与装机：`WeaselServer.exe` = 2 801 664 B / sha16 `1E82FA7F30DBB263`；installer =
  42 982 828 B / sha16 `5D30C15DB6EC6158`。静默装机后装机件 sha16 **与构建产物逐字相等**
  ⇒ `SHA_MATCH_BUILD_VS_INSTALLED=True`；服务 pid 193484（10:28:35，= BN 装机版）→
  1077296（14:07:45，安装器自己换掉）；`RimeUserDir` 未动，未登出 / 未登录 / 未改注册表。

### 真机复验（部分达成，如实登记）

修复前（BN 装机版，pid 193484）1017 读到：

> 最近同步：… · 失败（单次推送条目数上限为 64（实收 200）。）

修复后（BO 装机版，pid 1077296）三轮（14:11:16 / 14:13:50 / 14:15:27）均读到：

> 最近同步：… · 失败（网络请求失败：接收响应失败：错误码 12002）

⇒ **本 ADR 要消除的 `too_many_items` 拒绝已消失**（诊断日志同轮无该失败行），
但**同步本身仍未成功**：`12002` = WinHTTP `ERROR_WINHTTP_TIMEOUT`，与批量契约无关。

因此本文档早先写的判据「装新包后 1017 变为**成功态**」**未达成**，登记为**未判定**：

- **已证**：批量 64 超限拒绝不再发生（失败原因文本 + 诊断日志双向）。
- **未证**：待推 ≥65 条时能**分多批推成功**（成功态端到端）。缺的是网络可用窗口，
  不是代码路径。
- 探针自身判据（`VERDICT=SYNC_STATUS_OBSERVABLE`，即 1017 有时刻、有结果、非占位）
  `EXIT=0` 通过 —— 与「同步成功」是两件事，不互相替代。

⚠️ 同轮附带结论：14:11:03 探针 `EXIT=4`（`SYNC_STATUS_AFTER=` 空）是**探针侧假阴性**，
非产品缺陷 —— 独立诊断脚本 `tmp/bo-diag-1017.ps1` 直证同一控件两路读取
（`SendMessage(WM_GETTEXT)` 与 `GetWindowTextW`）均为 **54 字符**、点击后 3 s 内更新为新时刻，
且 `dlgAlive=True` / `hung=False` / 句柄未变。可能成因是探针在 AFTER 读取时复用了点击前
缓存的控件句柄；**本 ADR 不改探针**。