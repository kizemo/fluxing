# 批次 BM · 云端形态非法的旧条目不再毒化整轮同步

- **日期**：2026-11-22
- **分支**：`feat/100-cloud-sync`（远端 `kizemo`，**无 `origin`**）
- **规格**：`.specify/specs/100-fluxing-cloud-account-sync/spec.md` —— **不改规格**（FR-016 只要求界面显示「最近一次同步时间与结果」；本批修的是同步引擎内部的数据健壮性）
- **ADR**：`docs/adr/0071-malformed-cloud-entry-skip-instead-of-poisoning-round.md`（Accepted）
- **上一批**：[批次 BL · 同步路径补 401→续期→重试](handoff-2026-11-22-batch-bl-sync-token-refresh-retry.md)
- **性质**：产品修复 + 门禁 + 出包装机 + 装机实证 + 真机复验 + 提交
- **不改**：`librime/` / IPC 协议消息类型 / 控件 ID / 布局表 / 新增依赖 / **不 bump 版本号**（装机版仍 **1.3.8.0**，判定一律用 **sha16 / 字节数**，版本号无判别力）

---

## 1. 根因（源码级坐实）

「词典条目载荷格式不正确。」在**全仓唯一**的产点是 `WeaselServer/Cloud/CloudUserDictIo.cpp` 的 `BuildUserDictSnapshots`（`SplitDictPayload` 失败 ⇒ 整体 `return false`）。调用链：

```
CloudSyncCoordinator::SyncOnce
  → OpenItemsTolerant        ← 只做「解密 / 拆封 / MAC 复核」，**不校验 payload 形态**
  → MergeLocalAndRemote      ← 形态非法的云端条目进入合并集
  → EntriesToDictRows
  → BuildUserDictSnapshots   ⇒ 整轮失败（写回阶段）
```

⇒ 云端一条 **payload 无制表符**的旧条目进入合并集后，在**写回阶段**让整轮同步失败 ⇒ 此后每一轮都失败（批次 BK 真机复现）。

**结构性根因**：形态合法性此前**只在写回阶段**被检查；开箱侧与解析侧都不检查 ⇒「谁能进合并集」与「谁能把合并集写回去」两套口径漂移。

**本地已被证伪**：当轮导出快照 94 607 行，其中数据行 94 601 行**全部 3 列**，6 行非 3 列全是 `#` 元信息 ⇒ 元凶在**云端**。

**云端脏数据无法清除**：服务端 `cloud/fluxing-sync/src/index.ts` 只有 `/v1/health`、`/v1/auth/*`、`/v1/sync/push`、`/v1/sync/pull`、`/v1/devices`、`/v1/account/data`，**没有删条目 API** ⇒ 只能靠客户端**跳过**。

---

## 2. 修法（ADR 0071，9 条决策）

1. **单一真相源**：新增纯函数
   `bool IsValidUserDictEntryShape(const std::string& item_key, const std::string& payload)`
   （`WeaselServer/Cloud/CloudUserDictIo.h`），内部复用 `SplitDictItemKey` + `SplitDictPayload`
   —— 与写回阶段 `BuildUserDictSnapshots` 的校验**完全一致**（宪法 R7）。
2. **开箱侧主修**：`OpenItemsTolerant` 对 **userdb** 类条目做形态校验；非法者**跳过**并**单独计数**（`malformed`）。
   - **只对 userdb 生效**：`phrases` / `config` 的 payload 不是词典行形状，拿词典口径去卡会误伤（单测 ⑤ 正控制）。
   - **不改 `OpenItems` 的既有签名与语义**（沿用 tasks.md 既有约定）：形态校验只加在 tolerant 层；`OpenItems` 的生产唯一调用方就是 tolerant 层 ⇒ 效果等价。
3. **本地侧收窄**：`ParseUserDictSnapshots` 用同一个函数跳过形态非法的数据行。
4. **`JoinDictRow` 改为报错**：签名改为 `bool JoinDictRow(const CloudDictEntry&, std::string* out, std::string* err)`，形态非法 ⇒ false + 中文原因，**不再返回空串**。
5. **写回阶段保持 fail loud**：`BuildUserDictSnapshots` 遇形态非法仍整体失败 ⇒ 靠「开箱侧 + 本地侧」双关卡保证合并集无非法行，维持 canary 价值。
6. **计数与文案分开**：`undecryptable` 保持原义；形态非法**单独计数**并写进本轮 message（「云端有 N 条格式非法的条目已跳过」）。**不占用** `items_skipped`，**不新增** `CloudSyncRoundResult` 字段。
7. **续期路径升级为直证**：`CloudSyncService::token_refreshed()` ⇒ message 追加「访问令牌已自动续期」⇒ 把 BL §13 的**强推断**升级为**人眼可见的直证**；同时在续期分支与「跳过形态非法条目」分支各落一条**无条件**诊断行（`fluxing::diag::AppendLine`，tag `cloudsync`，文件 `%TEMP%\fluxing-profile-notify.log`，**不含任何条目明文**）。
8. **不动 IPC 协议 / 不动控件 ID / 不动布局表 / 不加控件 / 不改 `librime/` / 不新增第三方依赖 / 不 bump 版本号**。
9. **分页计数必须累加**（本批自测发现，见 §3）：`SyncOnce` 拉取循环改为「每页用**局部**计数器接收出参，再**累加**到轮次总量」。

### 🔴 一处**表述更正**（写在这里，避免下一批重犯）

BL handoff §14 与 `reports/fluxing-bl-syncstatus/analysis-20261122.md` 称「`JoinDictRow` 返回空串 ⇒ 静默写空行**丢数据**」——**不准确**：
`BuildUserDictSnapshots` 的**第一轮**分组循环就已经用 `SplitDictItemKey` + `SplitDictPayload` 校验并 `return false`，
第二轮的 `JoinDictRow` 必然成功 ⇒ **空串路径在当前调用链上走不到**。它是**公开 API 层面的陷阱**，不是当轮丢数据。
本批改签名是**消除陷阱**，**不得**宣称为「当前正在丢数据」的修复。

---

## 3. 本批自测发现的附带缺陷：分页计数被逐页覆盖

`OpenItemsTolerant` 保留「单次调用把出参清零」的语义（`*undecryptable = 0;` / `*malformed = 0;`），
而 `CloudSyncCoordinator::SyncOnce` 的拉取循环（`kMaxPullPages = 10`，`kPullPageLimit = 100`）
把**同一个**变量逐页传进去 ⇒ 第 2 页会把第 1 页的计数**清零覆盖** ⇒ 多页拉取时 message 只报**最后一页**的条数。

- 该形态**同时**命中 `malformed`（本批引入）与 `undecryptable`（**既有**，非本批引入）。
- **不影响**核心行为（脏条目照样被跳过、整轮照样不失败），只影响 message 里**条数的准确性**。
- 修法 = 决策 9；判别性用例 = 单测 ⑦（两页各 1 条脏条目 ⇒ message 报「2 条格式非法的条目已跳过」）。

---

## 4. 改动文件清单

| 文件 | 性质 | 说明 |
|---|---|---|
| `WeaselServer/Cloud/CloudUserDictIo.h` | 改 | `JoinDictRow` 新签名；新增 `IsValidUserDictEntryShape` 声明 |
| `WeaselServer/Cloud/CloudUserDictIo.cpp` | 改 | 形态判定实现；`ParseUserDictSnapshots` 跳过非法行；`BuildUserDictSnapshots` 第二轮 fail loud；`JoinDictRow` 改 `bool` |
| `WeaselServer/Cloud/CloudSyncService.h` | 改 | `OpenItemsTolerant` 新增 `int* malformed` 出参；`CloudSyncService` 新增 `token_refreshed()` |
| `WeaselServer/Cloud/CloudSyncService.cpp` | 改 | 形态校验 + 跳过计数 + 诊断行；401 续期置位 + 诊断行；message 两条文案；**分页计数累加** |
| `test/TestFluxingCloud/TestFluxingCloud.cpp` | 改 | BM 组 **33** 条断言（7 组） |
| `docs/adr/0071-malformed-cloud-entry-skip-instead-of-poisoning-round.md` | 新增 | ADR 0071 |
| `docs/adr/README.md` | 改 | 0071 索引行 |
| `CHANGELOG.md` | 改 | `## [1.3.8]` 下新增 BM 小节（**CRLF-only**） |
| `docs/archive/handoffs/handoff-2026-11-22-batch-bm-malformed-cloud-entry-skip.md` | 新增 | 本文件 |
| `docs/archive/prompts/prompt-next-session-2026-11-22-batch-bm-malformed-cloud-entry-skip.md` | 新增 | 接力 prompt（**只给全路径指针**） |
| `CLAUDE.md` | 改 | 「本批交接指针（BM）」行；BL 行降级为「上一批交接指针（BL）」 |

**全仓调用点核对**：grep `OpenItemsTolerant|JoinDictRow|IsValidUserDictEntryShape` ⇒ 除本批改动处与文档提及外，**无其它生产或测试调用点**被签名变更遗漏。

---

## 5. 门禁

| 项 | 结果 |
|---|---|
| 主构建 | `build.bat release weasel installer` ⇒ **`BUILD_EXIT=0`**（weasel.sln Release\|Win32 **0 警告 0 错误**；makensis 仅 8 条**既有** `warning 7010` = ARM/ARM64 可选产物 + `data\*.gram` + `data\lua\*.txt`） |
| 云单测 | `Release\TestFluxingCloud.exe`（**仓库根目录**运行）⇒ **`PASS=686 FAIL=0 / VERDICT: ALL PASS`**（BL 基线 653 ⇒ **+33**；`exe.mtime ≥ src.mtime` ⇒ 非假绿） |
| L0 静态门禁 | `scripts/gate/L0-static.ps1` ⇒ **`checks=15 pass=15 fail=0 skip=0`** |
| clang-format | 18.1.8 **定点** `--lines=起:止`（区间取自 `git diff -U0` 的 hunk）⇒ 改动文件 **0 漂移**；整文件存在**既有**漂移，故**必须**按改动行区间检查 |
| 编码 / 行尾 | 5 个 C++/测试文件 = UTF-8 无 BOM + LF-only、**U+FFFD=0**；ADR/handoff/prompt = LF-only；`CHANGELOG.md` = **CRLF-only**（`crlf=N bareLF=0`） |

### BM 组单测（33 条断言，7 组）

| 组 | 条数 | 内容 |
|---|---|---|
| ① 形态判定 `IsValidUserDictEntryShape` | 3 | 合法 true / item_key 缺 TAB false / payload 缺 TAB false |
| ② `JoinDictRow` 新签名 | 6 | 合法：true + 行逐字 + err 空；非法：false + err 非空 + **不写出空行** |
| ③ `ParseUserDictSnapshots` 收窄 | 3 | 含 2 列脏行仍解析成功 + 脏行被跳过 + 合法行保留 |
| ④ `OpenItemsTolerant` 主修 | 5 | 封包前置 + 不失败 + `out.size()==1` + `malformed==1` + `undecryptable==0` |
| ⑤ 正控制（phrases 防误伤） | 5 | 封包前置 + 不被误伤 + `out.size()==1` + `malformed==0` + `undecryptable==0` |
| ⑥ 协调器级回归见证 | 7 | 同步成功 + err 空 + `result.ok` + message 含「格式非法」+ 含「1 条格式非法的条目已跳过」+ 脏条目**未**写回本地 + 合法条目**已**写回本地 |
| ⑦ 多页累加 | 4 | 两页各 1 条脏条目 ⇒ 本轮不失败 + 结果成功 + message 报「**2 条**格式非法的条目已跳过」 |

---

## 6. 产物与装机实证

| 项 | 值 |
|---|---|
| `output\Win32\WeaselServer.exe` | **2 797 568 B** / sha16 **`D9D4DB32962F9252`** / mtime 2026-11-22 09:16:16 |
| `output\archives\fluxing-1.3.8-installer.exe` | **42 989 923 B** / sha16 **`24B47CF1A6CBCD25`** / mtime 2026-11-22 09:18:34 |
| 装机后 `D:\Program Files\fluxing\weasel\WeaselServer.exe` | **2 797 568 B** / sha16 **`D9D4DB32962F9252`** / mtime 2026-11-22 09:16:16（安装器保留源 mtime） |
| **`SHA_MATCH_BUILD_VS_INSTALLED`** | **`True`** |
| 服务端 pid | 94448（BL 装机件，00:23:53 起）→ **678312**（09:19:33 起，**安装器自己换掉**，属**预期**不是宿主被杀） |
| `HKCU\Software\Fluxing\Weasel\RimeUserDir` | `D:\Program Files\fluxing\user1\fluxing`（装机前后**逐字相同** ⇒ **`RIMEUSERDIR_UNCHANGED=True`**） |

- 静默装机命令 = BL 同款：`Start-Process -FilePath <installer> -ArgumentList '/S','/D=D:\Program Files\fluxing'` + `Wait-Process -Name 'fluxing-1.3.8-installer' -Timeout 300`。
- **未**登出 / **未**登录 / **未**改注册表（除安装器自身写入的安装目录项）。
- 原始日志：`reports\bm-install-verify.log`（`*.log` 命中 `.gitignore` ⇒ **未入库**，只存本机工作区）。
- **版本号无判别力**：装机版仍是 **1.3.8.0**，与 BL 轮同版；一切判定一律用 **sha16 / 字节数**。

---

## 7. 真机复验

- 探针 = **复用** `reports/fluxing-bk-syncstatus/probe-sync-status.ps1`（按**控件 ID 1017 / 1016** 定位账号窗，**不按中文标题**；`-WaitSeconds 90 -PollSeconds 2`），**未改探针一行**。
- 运行环境：`DESKTOP=Default`（**未**进影刀虚拟桌面）、`LogonUI=NONE`（**未锁屏**）、`SRV_PID=678312`、`VER=1.3.8.0`、`ACCOUNT_WINDOW_CLASS=FluxingCloudAccountDialog_v1`。
- 结果：**`VERDICT=SYNC_STATUS_OBSERVABLE EXIT=0`**、`REASON=OK`、`POLL_SECONDS=56`、`SYNC_CLICK_SENT=True`、`STATUS_LINE_1006_CHANGED=True`。
- **1017 前后对照**：

  | 项 | 值 |
  |---|---|
  | `SYNC_STATUS_BEFORE` | `最近同步：尚未同步。` |
  | `SYNC_STATUS_AFTER` | `最近同步：2026-11-22 09:20:49 · 成功（推送 0 条，拉取 1 条）` |
  | `SYNC_STATUS_CHANGED` | `True` |
  | `SYNC_STATUS_HAS_TIMESTAMP` / `HAS_RESULT` | `True` / `True` |
  | `SYNC_STATUS_AFTER_IS_PLACEHOLDER` | `False` |
  | **`SYNC_STATUS_RESULT_KIND`** | **`SUCCESS`** |

- **与 BL 轮的对照（本批最硬的证据）**：BL 轮**同一台机器、同一云端账号、同一条脏条目仍在服务端**，1017 报 **「失败（词典条目载荷格式不正确。）」**；BM 轮报 **「成功」** ⇒ **整轮不再被一条 payload 非法的旧条目毒化**。
- **诊断行直证**（`%TEMP%\fluxing-profile-notify.log`，本轮新增 2 行；文件 334 251 B → 334 612 B，< 2 MB 上限）：
  - `[2026-11-22 09:19:57.471][cloudsync] access token refreshed after 401; retry with new token`
  - `[2026-11-22 09:19:58.378][cloudsync] skipped malformed cloud userdb entry (payload shape invalid)`
  - ⚠️ 同一文件里 **09:12–09:15** 的同款行来自**本批云单测 exe**（单测也走同一 `AppendLine` 通道），**不是**服务端；真机那一轮只有 09:19:57 / 09:19:58 这两行。**读日志时必须按时间戳甄别来源**。
- **已知观测面缺口（本批未修 ⇒ 登记为下一批候选）**：成功态 1017 走 `CloudAccountDialog.cpp:380` 的 `AccountSyncStatusTextSuccess(when, pushed, pulled)`，**不消费 `out->message`** ⇒ message 里的「云端有 N 条格式非法的条目已跳过」与「访问令牌已自动续期」在**成功态界面上看不到**（失败态走 `AccountSyncStatusTextFailed(when, reason)`，reason 取 `out->message` ⇒ **失败态可见**）。根因在 **BK 批次 ADR 0069 的「成功态独立拼装」设计**，**不是 BM 引入**；BM 的直证由**诊断文件两行**兜住。

---

## 8. 证据路径

| 证据 | 路径 |
|---|---|
| **本批 handoff（真源）** | `docs/archive/handoffs/handoff-2026-11-22-batch-bm-malformed-cloud-entry-skip.md` |
| 接力 prompt | `docs/archive/prompts/prompt-next-session-2026-11-22-batch-bm-malformed-cloud-entry-skip.md` |
| ADR | `docs/adr/0071-malformed-cloud-entry-skip-instead-of-poisoning-round.md` |
| ADR 索引 | `docs/adr/README.md`（0071 行，Accepted） |
| 装机实证日志 | `reports\bm-install-verify.log`（**未入库**，`.gitignore` 挡 `*.log`） |
| 真机复验原始输出 | `reports/fluxing-bm-syncstatus/probe-run.log`、`reports/fluxing-bm-syncstatus/sync-status-20261122-092051.txt`（**未入库**） |
| 复用探针（**已入库，本批未改**） | `reports/fluxing-bk-syncstatus/probe-sync-status.ps1` |
| 诊断文件（**本机临时文件，不入库**） | `%TEMP%\fluxing-profile-notify.log` |
| 上一批真源（BL） | `docs/archive/handoffs/handoff-2026-11-22-batch-bl-sync-token-refresh-retry.md` |
| BL 真机复验报告 | `reports/fluxing-bl-syncstatus/analysis-20261122.md` |
| 缺陷现场回看（BK） | `docs/archive/handoffs/handoff-2026-11-21-batch-bk-sync-status-1017-visibility.md` |

---

## 9. ⚠️ 本轮工具坑（勿重走）

1. **`TestFluxingCloud` 不在 `weasel.sln`** ⇒ 必须显式 msbuild **且带 `/p:SolutionDir="F:\soft\00selfmade\rime_claude\"`**（否则 `FluxingLocalChangeHook.h` 找不到）；`Release\TestFluxingCloud.exe` **必须在仓库根跑**（在 `release\` 下跑会假红）。
2. **协调器级假后端必须导出可解析快照文本**：`ParseUserDictSnapshots("")` 返回 **false**（空快照 = 失败 ≠ 没有本地条目）⇒ 假后端要导出最小可解析快照。
3. **拉取游标吃「条目级」`server_seq`**（顶层 `server_seq` 只是「条目都没带」时的兜底，`CloudSyncService.cpp:267-276`）⇒ 多页用例**必须逐页给出递增值**，否则第二轮会因「游标没前进」提前收手。
4. **clang-format 必须 `--lines=起:止` 定点查**（区间从 `git diff -U0` 的 hunk 取）；整文件有**既有**漂移；`-i` 修完**区间失效，必须重算**再复检。
5. **顺序铁律**：格式化 → 复检 → 构建 → 单测 → 打包 → 装机 → 记 sha（改完格式产物 sha 会变）。
6. `build.bat` 只 `del msbuild*.log`，**不清 `release/`** ⇒ 与 `TestFluxingCloud` 的显式构建**不冲突**，可并行。
7. **静默装机**：`Start-Process -FilePath <installer> -ArgumentList '/S','/D=D:\Program Files\fluxing'` + `Wait-Process -Timeout 300`；`/D=` **必须是最后一个参数且不加引号**；`Start-Process -Wait` 对 NSIS **挂死不返回**。
8. NSIS `InstallDirRegKey` 指向 **32 位视图** `HKLM\Software\WOW6432Node\Fluxing\Weasel\InstallDir` ⇒ `/D=` 生效 ⇒ **无需清注册表**（清了反而丢 `RimeUserDir`）。
9. 装机后服务 pid **必然变**（NSIS 按映像名杀旧服务进程），属**预期**不是宿主被杀。
10. **行尾策略**：`CHANGELOG.md` = **CRLF-only**（字节级写入，LF → CRLF）；`docs/archive/handoffs/*.md`、`docs/archive/prompts/*.md`、`docs/adr/*.md` = **LF-only**。
11. **图谱降级**：`codebase-memory-mcp` 的 `search_graph` 两次均 **0 命中**（query=`OpenItems tolerant decrypt entries` / name_pattern=`.*OpenItems.*`）⇒ 按 `.claude/rules/codebase-graph.md`「`search_graph` 返回 0 命中且符号理应在 scope」**降级**到 Read/Grep。
12. **Bash 安全策略**：`2>$null` 之类重定向会被判为写目标；用 `git -C <repo>` 与绝对路径代替 `cd`。
13. **PowerShell 插值坑**：`"$s:$x"` 会把 `$s:` 当驱动器限定变量而报错 ⇒ 用 `'--lines={0}:{1}' -f $s, $end`。
14. **派「只读取证 + 落盘报告」的子会话必须派 `worker`**（`explore` / `verifier` 是严格只读，秒返 `ROLE_MISMATCH`）。

---

## 10. 未闭环

1. **真机端到端「令牌过期 → 续期 → 同步成功」本轮已观测到**（诊断行 09:19:57.471 + 1017 成功）⇒ BL §13 的「未闭环」项**本批顺带闭环**；但**仍不是受控实验**（本轮令牌是**自然过期**，不是人为造出来的「已过期访问令牌」），**不升级为闸门判据**。
2. **观测面缺口（下一批候选，BM 未修）**：成功态 1017 **不消费 `out->message`** ⇒ 「云端有 N 条格式非法的条目已跳过」「访问令牌已自动续期」在**成功态界面不可见**（失败态可见）。根因在 BK 批次 ADR 0069 的「成功态独立拼装」设计，**不是 BM 引入**；修它要动 BK 的共享真相源 `CloudAccountUi.h` 的 `AccountSyncStatusTextSuccess`（该文件注释明写「**逐字不得改动**：账号页文案与探针期望值逐字比对」）+ 对应逐字单测 ⇒ **需独立批次 + ADR**，本批**不扩范围**。详见 §7 末条。
3. **云端脏条目仍在**（服务端**没有「删条目」API**）⇒ BM 只保证「跳过不毒化整轮」，**不承诺清除云端脏数据**；跳过计数会随脏条目数增长。
4. **BL §8 原计划（优先级让位给 BM，尚未开始）**：双机 AC 7 条 + restore 端到端 + 输入延迟两组对照 + G-3/G-5 真跑（**需两台机器 + 用户在场**）。
5. **图谱降级未消除**：`codebase-memory-mcp` 的 `search_graph` 两次 **0 命中**（query=`OpenItems tolerant decrypt entries` / name_pattern=`.*OpenItems.*`）⇒ 本批全程用 Read/Grep（降级依据 `.claude/rules/codebase-graph.md`）。
6. **待用户拍板（沿自 BL §11，不阻塞本批）**：18 份 `run-*.log` 是否 `git add -f` 入库；`tmp\bh-c2d-fmt-check.cpp`（83 767 B）本机策略删不掉、需用户手动处置。

---

## 11. 待用户拍板（不阻塞本批）

1. 工作区里 18 份 `run-*.log` 被 `.gitignore` 挡住（`*.log`），是否 `git add -f` 入库。
2. `tmp\bh-c2d-fmt-check.cpp`（83 767 B）本机策略删不掉，需用户手动处置。
3. BL §8 原计划：**真机令牌过期 → 续期 → 同步成功**端到端直证（需制造**已过期访问令牌** + 点「立即同步」，判据用 BK 探针按控件 ID 1017 定位）；双机 AC 7 条 + restore 端到端 + 输入延迟两组对照 + G-3/G-5 真跑（**需两台机器 + 用户在场**）。
