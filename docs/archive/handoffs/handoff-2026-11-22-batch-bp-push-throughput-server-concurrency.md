# 批次 BP 交接文档 · 推送批量吞吐修复 12002 —— 服务端 R2 并发化 + 客户端批量 32 / 超时 30 s（ADR 0074）

- 日期：2026-11-22
- 批次：BP（接 BO；本批 = BO 遗留的 **12002 根因修复落地**，用户拍板 **S3 全套**）
- 分支：`feat/100-cloud-sync`（远程 `kizemo`）
- 性质：**产品修复 + 服务端生产部署 + 全门禁 + 出包装机 + 装机实证 + 真机复验 + 提交**
- 边界：全批次**未改** `librime/`、**未改** IPC 协议消息类型与 `include/WeaselIPCData.h`、**未新增**第三方依赖、**未改** `.clang-format`、**未 bump** 版本号（装机版仍 1.3.8.0，判定一律用 sha16 / 字节数）
- 授权：用户已授权 `cloud/fluxing-sync/**` 改动 + `wrangler deploy` **直达生产** `api.aiec.fun`（无 staging；`workers_dev=false` / `preview_urls=false`）

---

## 1. 当前状态（一段话）

BO 批把 12002 定案为「服务端串行 R2 put ⇒ 64 条 = 40 071 ms ≫ 客户端 15 000 ms 接收超时」，但只留下根因、没动码。**本批已把修复落地并上线生产**：服务端 R2 阶段改为**波式并发**（`R2_WRITE_CONCURRENCY = 6`，写序「R2 先、D1 后」不变，`MAX_PUSH_ITEMS = 64` / HLC 守卫 / 幂等语义**一字未动**），`wrangler deploy` 直达生产（Version `fcdd38ed`）；同一探针复测 **n=64 由 40 071 ms 降到 10 330 ms**（3.88×，**低于旧客户端 15 s 超时**），n=16 由 11 300 降到 3 568 ms，n=200 仍被服务端 400 `too_many_items` 快速拒绝（闸门未松动），四档 `accepted` 与 D1 落库行数逐档吻合 ⇒ **HLC 守卫 / 幂等 / 写序未变**。客户端同步把接收超时 **15 000 → 30 000 ms**、推送单批 **64 → 32**（保留 `kServerMaxPushItems = 64` 与 `static_assert`），并新增**无条件**诊断行 `[cloudsync] push batch items=<n> elapsed_ms=<ms> http=<status>` ⇒ 下次再遇 12002 可直接读出「卡在哪一批、多少条、耗时多少」。全门禁绿（服务端 typecheck + check **4 文件 / 65 用例**；客户端 `BUILD_EXIT=0` **0 警告 0 错误**；云单测 **PASS=708 FAIL=0**；L0 **15/15**；clang-format 定点 **5 文件 0 漂移**；改动文件 **U+FFFD=0**），已出包装机、装机实证（sha16 `BB5415258CB96A27` 与构建产物逐字相等）、真机复验成功（1017 = **「最近同步：2026-11-22 16:17:11 · 成功（推送 0 条，拉取 1 条；云端有 1 条格式非法的条目已跳过）」**，日志**无 12002**）。**唯一未闭环**：客户端**批量推送路径未在真机压到**（真实账号本地无改动 ⇒ 推送 0 条 ⇒ 批次循环未进入 ⇒ 新诊断行按设计不打印，真机日志 0 命中），批量行为目前只有单测层证据。

---

## 2. 必读文档（全部为绝对路径）

| 用途 | 路径 |
|---|---|
| **本批真源（先读这份）** | `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-22-batch-bp-push-throughput-server-concurrency.md` |
| 进站第 1 份 · 身份/模块/不变量 | `F:\soft\00selfmade\rime_claude\CONTEXT.md` |
| 进站第 2 份 · 操作手册（§1 首屏 / §4 危险区 / §5 checklist / §9 交接规则） | `F:\soft\00selfmade\rime_claude\AGENTS.md` |
| 进站第 3 份 · 法则层（R1–R9 / P1–P8） | `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md` |
| 进站第 4 份 · 部署 + NSIS 配方 | `F:\soft\00selfmade\rime_claude\.specify\memory\project-knowledge.md` |
| 进站第 5 份 · 工程技能消费契约 | `F:\soft\00selfmade\rime_claude\docs\agents\domain.md` |
| **本批决策依据（已 Accepted）** | `F:\soft\00selfmade\rime_claude\docs\adr\0074-push-batch-throughput-server-concurrency-and-client-timeout.md` |
| 被本批修订的上一决策（**正文不改**，决策 5 由 0074 回链修订） | `F:\soft\00selfmade\rime_claude\docs\adr\0073-client-push-batch-aligns-with-server-limit.md` |
| 规格 100（本批未改） | `F:\soft\00selfmade\rime_claude\.specify\specs\100-fluxing-cloud-account-sync\spec.md` |
| 上一批真源（BO · 12002 根因定案） | `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-22-batch-bo-push-batch-align-server-limit.md` |
| 再上一批（BN） | `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-22-batch-bn-sync-round-notices-visible.md` |
| 服务端真源（并发化落点） | `F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\src\r2-write.ts`、`F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\src\sync.ts` |
| 服务端单测（新增） | `F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\test\r2-write.test.ts` |
| 客户端真源（超时/批量/诊断行） | `F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\CloudTransport.h`、`CloudTransport.cpp`、`CloudSyncService.h`、`CloudSyncService.cpp` |
| 客户端单测（新增 4 条断言） | `F:\soft\00selfmade\rime_claude\test\TestFluxingCloud\TestFluxingCloud.cpp` |
| 本轮探针脚本（**明文 `auth_key` 在 state 文件里，禁止入库**） | `F:\soft\00selfmade\rime_claude\tmp\bo-push-probe.ps1`、`F:\soft\00selfmade\rime_claude\tmp\bo-push-probe-state.json` |
| 本轮探针报告（五档计时原始数据） | `F:\soft\00selfmade\rime_claude\reports\fluxing-bo-12002\push-probe-report.txt` |
| 官方界面探针（1016/1017 定位） | `F:\soft\00selfmade\rime_claude\reports\fluxing-bk-syncstatus\probe-sync-status.ps1` |
| 本轮真机报告 | `F:\soft\00selfmade\rime_claude\reports\fluxing-bk-syncstatus\sync-status-20261122-161712.txt` |

---

## 3. 本批逐项进度（全部已完成）

### 3.1 服务端并发化（新增 2 文件 + 改 1 文件）

| 位置 | 事实 |
|---|---|
| `cloud/fluxing-sync/src/r2-write.ts`（**新增**，3108 B，LF-only，U+FFFD=0） | 导出 `R2_WRITE_CONCURRENCY = 6`、`R2WriteMetadata` / `R2WriteValue` / `R2WriteTarget` / `R2WriteBucket`（**结构类型** ⇒ 单测可传假桶）、`putAllWithConcurrency(bucket, targets, concurrency)`（**按「波」推进**，每波 ≤ limit 个 `Promise.all` ⇒ 并发上界严格可证） |
| `cloud/fluxing-sync/src/sync.ts:149-169` | 原**串行** `await env.BUCKET.put(...)` 循环 → `await putAllWithConcurrency(env.BUCKET, accepted.map((item, i) => ({ key: staged[i]!.r2Key, value: item.cipherBytes, options: {...} })))`；`staged[i]` ↔ `accepted[i]` **索引对齐不变**；`:171-203` 的 **D1 阶段一字未动** |
| `cloud/fluxing-sync/test/r2-write.test.ts`（**新增**，4223 B） | **8 条断言**：默认并发 6、key/metadata 透传、12 条时上界 6 峰值 = 6、确实并发 > 1、并发 = 1 严格串行、并发 > 目标数时峰值 = 目标数、空列表不 put、失败抛错 |

**不变量保持**：写序「R2 先、D1 后」不变 · `MAX_PUSH_ITEMS = 64` 不变 · HLC 守卫不变 · 幂等语义不变 · D1 阶段不变。

### 3.2 服务端门禁与生产部署

- `npm --prefix F:\soft\00selfmade\rime_claude\cloud\fluxing-sync run typecheck` ⇒ **TSC_EXIT=0**
- `npm --prefix F:\soft\00selfmade\rime_claude\cloud\fluxing-sync run check` ⇒ **CHECK_EXIT=0**，**4 文件 / 65 用例通过**（新增 8 条）
- 生产部署（**必须先设代理**）：
  ```powershell
  $env:HTTPS_PROXY='http://127.0.0.1:7897'; $env:HTTP_PROXY='http://127.0.0.1:7897'; $env:NODE_USE_ENV_PROXY='1'
  npm --prefix F:\soft\00selfmade\rime_claude\cloud\fluxing-sync run deploy
  ```
  ⇒ **DEPLOY_EXIT=0**，Version ID **`fcdd38ed-afb4-4ee2-989a-65b6453d9d81`**，路由 `api.aiec.fun (custom domain)`。

### 3.3 服务端实测（同一探针、**脚本未改**）

`pwsh -NoProfile -File tmp\bo-push-probe.ps1 -Mode Tiers` ⇒ **PROBE_EXIT=0**（探针前缀按运行时间戳生成 ⇒ 每轮唯一，不会被 HLC 守卫跳过 ⇒ 计时可比）

| 档位 | 改前 | 改后 | 倍数 |
|---|---|---|---|
| n = 1 | 2 579 ms | **2 061 ms** | 1.25× |
| n = 16 | 11 300 ms | **3 568 ms** | **3.17×** |
| n = 64 | 40 071 ms | **10 330 ms** | **3.88×** |
| n = 200 | 400 `too_many_items`（1 373 ms） | **400 `too_many_items`（1 325 ms）** | 闸门未松动 |
| n = 1 × 4 MiB | 8 186 ms | **5 547 ms** | 1.48× |

`accepted` 逐档 = 条目数；`D1_AFTER count=164 max_id=167` 与四档 accepted 之和**逐档吻合**；空 items 仍 400 `bad_items`（鉴权自证成立）。

### 3.4 客户端改动（worker 交付，禁 build / 禁 git add）

| 位置 | 事实 |
|---|---|
| `WeaselServer/Cloud/CloudTransport.h:71-73` | `timeout_ms = 30000`（原 15 000） |
| `WeaselServer/Cloud/CloudTransport.cpp:153-157` | 兜底 `options_.timeout_ms = 30000`（原 15 000） |
| `WeaselServer/Cloud/CloudSyncService.h:59-68` | `kServerMaxPushItems = 64`（保留）· `kPushBatchSize` **64 → 32** · `static_assert` 保留 |
| `WeaselServer/Cloud/CloudSyncService.h:270-276` + `.cpp:741-758` | `Push(...)` 增加带默认值出参 `int* http_status = nullptr`（清单外唯一增补，用于诊断行取真状态码；默认参数保证既有调用点无需改） |
| `WeaselServer/Cloud/CloudSyncService.cpp:121-130` | 新增 `TruncateUtf8()`（≤ 80 字节且**不切断 UTF-8**） |
| `WeaselServer/Cloud/CloudSyncService.cpp:1183-1209` | 推送批次循环新增**无条件**诊断行 `[cloudsync] push batch items=<n> elapsed_ms=<ms> http=<status>[ err=…]`（复用既有 `diag::AppendLine("cloudsync", …)` ⇒ 落 `%TEMP%\fluxing-profile-notify.log`） |
| `test/TestFluxingCloud/TestFluxingCloud.cpp:4481-4502` / `:4583-4585` | 新增 **4 条断言**：超时默认 = 30 000、兜底 = 30 000、批量 = 32、批量 ≤ 64 |

### 3.5 客户端门禁

- `cmd /c "build.bat release weasel installer"` ⇒ **BUILD_EXIT=0**，解决方案 **0 个警告 0 个错误**（NSIS 仅 8 条**既有** `warning 7010` = ARM/ARM64 可选产物 + `data\*.gram` + `data\lua\*.txt`）
- 云单测（显式 msbuild **且带 `/p:SolutionDir=F:\soft\00selfmade\rime_claude\`**，exe **在仓库根跑**）⇒ **RESULT: PASS=708 FAIL=0 / VERDICT: ALL PASS**（BO 基线 704 ⇒ **+4**）
- `scripts/gate/L0-static.ps1 -Mode all` ⇒ **15/15**（`checks=15 pass=15 fail=0 skip=0`）
- 定点 `clang-format 18.1.8 --style=file --lines=起:止` **5 个**改动文件 ⇒ **DRIFT_TOTAL=0**
- 改动文件 **U+FFFD=0**（9 个文件），全部 **LF-only**

### 3.6 出包装机 + 装机实证

| 项 | 值 |
|---|---|
| 安装包 | `output\archives\fluxing-1.3.8-installer.exe` = **42 984 891 B** / sha16 **`15B5DB6732B33E8C`** |
| 内嵌载荷 | `output\Win32\WeaselServer.exe` = **2 803 200 B** / sha16 **`BB5415258CB96A27`** |
| 静默装机 | `/S /D=D:\Program Files\fluxing` ⇒ **INSTALL_EXIT=0** |
| 装机件 | `D:\Program Files\fluxing\weasel\WeaselServer.exe` = **2 803 200 B** / sha16 **`BB5415258CB96A27`** ⇒ **`SHA_MATCH_BUILD_VS_INSTALLED=True`** |
| 服务进程 | pid 1 077 296（14:07:45）→ **285 340**（16:16:09，ver 1.3.8.0，**由安装器自己换掉**） |
| 用户数据 | `HKCU\Software\Fluxing\Weasel\RimeUserDir = D:\Program Files\fluxing\user1\fluxing` **未动**；**未登出、未登录、未改注册表** |

### 3.7 真机复验（成功侧闭环）

`pwsh -NoProfile -File reports\fluxing-bk-syncstatus\probe-sync-status.ps1`（**未改探针一行**）⇒ **EXIT=0** / `VERDICT=SYNC_STATUS_OBSERVABLE`

- `SYNC_STATUS_BEFORE=最近同步：尚未同步。`
- `SYNC_STATUS_AFTER=最近同步：2026-11-22 16:17:11 · 成功（推送 0 条，拉取 1 条；云端有 1 条格式非法的条目已跳过）`
- `SYNC_STATUS_CHANGED=True` / `HAS_TIMESTAMP=True` / `HAS_RESULT=True` / `IS_PLACEHOLDER=False` / `RESULT_KIND=SUCCESS` / `STATUS_LINE_1006_CHANGED=True`
- 诊断日志（`%TEMP%\fluxing-profile-notify.log`）本轮**无 12002**；命中 `[cloudsync] skipped malformed cloud userdb entry (payload shape invalid)`（BM 的跳过直证行仍在）
- 报告落盘：`reports\fluxing-bk-syncstatus\sync-status-20261122-161712.txt`

---

## 4. 已验证 / 推断 / 未验证

### 4.1 已验证（有命令输出或探针直证支撑）

1. 服务端并发化**已上线生产**（Version `fcdd38ed`），端点行为由同一探针逐档复测确认。
2. n=64 由 **40 071 ms → 10 330 ms**，**低于旧客户端 15 000 ms 超时** ⇒ 12002 的触发条件已被消除。
3. 服务端 64 条上限闸门**未松动**（n=200 仍 400 `too_many_items`）。
4. HLC 守卫 / 幂等 / 写序**未变**（`accepted` 与 D1 落库行数逐档吻合、`server_seq` 递增）。
5. 客户端新参数（超时 30 000 / 批量 32）**已进二进制并装机**（sha16 逐字相等）。
6. 真机同步**成功**、1017 可见、**无 12002**。
7. 全门禁绿：服务端 typecheck + 65 用例、客户端 0 警告 0 错误、708 用例、L0 15/15、clang-format 0 漂移、U+FFFD=0。

### 4.2 推断（有依据但未直接观测）

1. **旧客户端（15 s 超时 + 批量 64）在服务端修复后应不再触发 12002**：依据 = n=64 实测 10 330 ms < 15 000 ms（余量约 4.7 s）。**未在真机上跑旧客户端复测**（已被新包覆盖）。
2. **新客户端（30 s 超时 + 批量 32）单批耗时 ≈ 10 s 量级、余量约 20 s**：依据 = 服务端 n=32 落在 n=16（3 568 ms）与 n=64（10 330 ms）之间。**未在真机压到**（无待推条目）。
3. **单条 4 MiB 大体积不是 12002 的主因**：依据 = 单条 4 MiB 实测 5 547 ms 不超时。

### 4.3 未验证 / 未闭环（如实登记，**不得**写成已修）

1. **客户端批量推送路径未在真机压到** —— 真机日志中 `push batch` **0 命中**：真实账号本地无改动 ⇒ 推送 0 条 ⇒ 批次循环未进入 ⇒ 该行**按设计**不打印。批量行为目前**只有单测层**证据（4 条新断言）。
2. **用户词典无可备份路径** ⇒ 「登录空云端探针账号 ⇒ 客户端被迫推全量」造样本测试**未执行**（`userdb*` 在 `%APPDATA%` / `%LOCALAPPDATA%` / `D:\Program Files\fluxing`（深度 4）**均未找到**；`HKCU\Software\{Rime,Fluxing}` 下**无** `RimeUserDir`）⇒ 在找到可备份路径前**不得**执行该登录测试（会损坏用户个人词典）。
3. **服务端 ≤ 5 000 ms 目标未达成**（实测 10 330 ms）：Cloudflare「同时打开连接数 = 6」封顶 ⇒ 64 条 = 11 波、单波 ≈ 0.85 s ⇒ 理论下限 ≈ 9–10 s ⇒ **不再冒险上调并发**（会触发 `Network connection lost`）。
4. **`/v1/auth/register` 恒 503 `mail_send_failed`**（`mail_outbox` 无行）⇒ 新用户「注册 → 收码 → 验证」链路当前不可用（**独立缺陷**，另立批次）。
5. **云端脏条目仍在**（服务端**没有「删条目」API**）⇒ BM/BN 只保证「跳过不毒化整轮」，**不承诺清除云端脏数据**。
6. 双机 AC 7 条 / restore 端到端 / 输入延迟两组对照 / G-3 / G-5 真跑 —— **需两台机器 + 用户在场**。
7. 18 份 `run-*.log` 是否 `git add -f` 入库 —— **待用户定**；`tmp\bh-c2d-fmt-check.cpp`（83 767 B，本机删不掉）—— **待用户处置**。

---

## 5. 收尾清理（本轮**已执行**，结果如实登记）

命令：`pwsh -NoProfile -File tmp\bo-push-probe.ps1 -Mode Cleanup`（**CLEANUP_EXIT=0**，脚本自带的僵尸账号清理 scope 严格限定 `bo12002.probe.%@example.com`，不碰真实账号）。

| 项 | 状态 | 证据 |
|---|---|---|
| D1 `sync_items`（探针数据） | ✅ **已清空** | Cleanup 输出 `CLEANUP D1_ROWS=0` / `CLEANUP REMAIN=0`；复查 `SELECT user_id, type, cipher_len FROM sync_items` **只剩 1 行** = 真实账号 `d42756f4-866c-465f-a9d4-aa6d47464426`（`type=userdb`、`cipher_len=75`）—— 与 BM/BN 登记的「云端 1 条格式非法的条目」**吻合** |
| 3 个 `bo12002.probe.*` 僵尸账号 | ✅ **已删** | 清理前 `SELECT email FROM users WHERE email LIKE '%probe%'` 返回 5 个（`ak.probe.261111` / `ak.probe2.261111` / `bo12002.probe.20261122154416` / `...154614` / `...154809`）；清理后**只剩 2 个 `ak.probe.*`**，`users` 总数 = 4 |
| R2 孤儿对象（第一轮 82 + 第二轮 82 = 164 个） | 🔴 **无法删除** | ① 本批早前已 `DELETE FROM sync_items WHERE user_id='e3653ed8-…'`（rows_written=164）⇒ **D1 行不存在，Cleanup 的 `SELECT id, item_key, r2_key` 取不到 key**（Cleanup 实跑 `R2_DELETE_LINES=0`）；② 服务端 `r2KeyFor(userId, keyHash)` 的 key 形如 `u/<user_id>/<sha256hex(item_key)>`（`cloud/fluxing-sync/src/wire.ts:210-213`），**sha256 不可逆推**；③ 探针报告 `reports/fluxing-bo-12002/push-probe-report.txt` 只留了 **1 条** `D1_ROW` 样本 ⇒ **item_key 集合未留存**、无法重建；④ `wrangler r2 object` **无 `list` 子命令**，R2 侧无枚举入口 ⇒ **本批无法定位删除**，只能留作孤儿（每个 ≤ 4 MiB，多数 64 B）。**待用户定**：若能取回当时 item_key 集合，按 `sha256(item_key)` 重建 key 后逐个 `wrangler r2 object delete "fluxing-sync/u/<uid>/<hash>" --remote` 即可清掉 |
| 2 个 `ak.probe.*@example.com` 账号 | 🟡 **未删（刻意）** | 不属本批口径（脚本 scope 只含 `bo12002.probe.`）⇒ 属**更早批次**遗留，**无法确认是否仍被使用** ⇒ 不越权删除，**待用户定** |

口令纪律：探针账号口令 = `auth_key`（44 字符），明文只在 `tmp/bo-push-probe-state.json` 与 `tmp/AN-register-qq.txt` 内，**不入日志 / 报告 / 对话 / 提交**。

---

## 6. 本轮工具坑（勿重试）

1. **`Set-Location` 后写/删文件被硬安全策略拦** ⇒ 用 `npm --prefix <pkg>` 替代 `cd`；递归写目标必须落在工作区内（备份只落 `tmp/`）。
2. **`cmd //c` 会假绿**（被 MSYS 吞参数）⇒ 必须 `cmd /c`。
3. **`build.bat` 的 msbuild 日志 GBK/UTF-16 混杂** ⇒ 按 BOM 判编码（`Encoding.Unicode`），汇总行在 `msbuild2.log`。
4. **整文件 clang-format 有既有漂移** ⇒ **必须** `--lines=起:止` 定点查；`-i` 后**必须重算区间**再复检。
5. **改完格式 ⇒ 产物 sha 会变** ⇒ 顺序铁律「格式化 → 复检 → 构建 → 单测 → 打包 → 装机 → 记 sha」。
6. **`TestFluxingCloud` 不在 `weasel.sln`** ⇒ 须显式 msbuild **且带 `/p:SolutionDir=F:\soft\00selfmade\rime_claude\`**，exe **须在仓库根跑**。
7. **NSIS `Start-Process -Wait` 会挂死不返回** ⇒ 用 `Wait-Process -Timeout`；`/D=` **必须是最后一个参数且不加引号**。
8. **NSIS `InstallDirRegKey` 指向 32 位视图** `HKLM\Software\WOW6432Node\Fluxing\Weasel\InstallDir` ⇒ **无需清注册表**（清了反而丢 `RimeUserDir`）。
9. **装机后服务 pid 必变**（安装器按映像名杀旧进程）⇒ 属**预期**，不是宿主被杀。
10. **行尾分流**：`CHANGELOG.md` = **CRLF-only**（`crlf=7680 bareLF=0`）vs handoff / prompt / ADR = **LF-only** ⇒ 插条目必须**字节级**写入。
11. **`wrangler` 所有远端命令必须先设** `HTTPS_PROXY` / `HTTP_PROXY=http://127.0.0.1:7897` + `NODE_USE_ENV_PROXY=1`（直连 ETIMEDOUT 23.101.24.70:443）。
12. **派「只读取证 + 落盘报告」的子会话必须派 `worker`**（`explore` / `verifier` 严格只读 ⇒ 秒返 `ROLE_MISMATCH`）。
13. **图谱降级**：`search_graph` 两次 0 命中 ⇒ 按 `.claude/rules/codebase-graph.md` 降级 Read / Grep。
14. **凭据纪律**：只打印 `auth_key_len` / `sha16` / `token_len` / `sha16`；`access_token` / `refresh_token` 脱敏；明文口令只在 `tmp/AN-register-qq.txt` 与 `tmp/bo-push-probe-state.json` 内，**不入日志 / 报告 / 对话**。

---

## 7. 提交与推送口径

- Conventional Commits + **单 scope** + **模块前缀**（`WeaselServer` / `WeaselIPC` / `installer` / `docs` / `fluxing` / `ci` 等，见 CLAUDE.md 铁律）。
- **按路径显式 `git add`**（**禁** `git add .`），`git status -sb` + `git diff --stat` 复核后方可 commit（阻止 `weasel.props` / `env.bat` / `*.obj` 误提交）。
- 本批**未新增**依赖、未改 `include/WeaselIPCData.h` ⇒ **不触发** IPC 四边同步红线。
- `librime` 目录保持 dirty（`m`）是**既有**状态，**不要** `git add librime`。
- 远程：`kizemo/feat/100-cloud-sync`（push 失败先试直连 `-c http.proxy= -c https.proxy=`，再试默认代理，**两条路都要走**）。
