# 接力链计划：BR-①g 桶化双写后续批次（2026-11-27 起）

> 本文件是接力链每一环的**唯一计划入口**。启动器已把它写进台账 `plan_doc`，每一环读同一个文件。
> **权威进度源不是本文件**，而是 handoff 顶节：
> `docs/archive/handoffs/handoff-2026-11-23-batch-br-write-amplification-and-zero-write-register.md`
> （检索 `^## [A-Z]+`，**不要加 `$` 锚点**；最新顶节 = 本批最新事实；**最新顶节 = 文件中最后一个 `## [A-Z]` 顶节**（不引用固定节号，避免过期））。
> 接力 prompt 文件：`docs/archive/prompts/prompt-next-session-2026-11-23-batch-br-write-amplification-and-zero-write-register.md`（就地改写，不另建）。

## ★ 本项目覆盖（**优先于通用接力协议**，每环必读）

- **git 写操作在本仓不是人类检查点**：通用协议里「git 写操作（commit / push / tag / stash / checkout）是人类检查点」一条**在本仓不适用**。本仓宪法 P4 与本批规约要求**每阶段收尾必须 commit + push**，因此链内环可在**门禁绿**之后自行 commit + push 到 `kizemo/feat/100-cloud-sync`，无需等待人工确认。
- commit 纪律照旧：Conventional Commits + 模块前缀（`WeaselServer | WeaselTSF | WeaselDeployer | cloud | docs | fluxing | installer | ci`）；**按路径显式 `git add`，禁 `git add .`**；提交前必跑 `git status -sb` + `git diff --stat`；`librime/`、`release/`、`env.bat`、`msbuild*.log`、`reports/**`、`scripts/verify/*.ps1`、`tmp/**` **不入库**。
- push 硬判据 = `git ls-remote kizemo refs/heads/feat/100-cloud-sync` 与 `git rev-parse HEAD` **逐字相等**；远端通道 = **SOCKS5 代理 `socks5h://127.0.0.1:7897`**（2026-11-27 实测：直连 `Recv failure: Connection was reset` / connect timeout 均为证伪；**HTTP 代理形式 `http://127.0.0.1:7897` 同样不通**，只有 SOCKS5 形式 200）⇒ 仓库级 `http.proxy`/`https.proxy` 已设为 socks5h ⇒ **裸 `git push` / `git ls-remote` 即可**，**不要**再带 `-c http.proxy= -c https.proxy=`。
- **仍属人类检查点（必须停链、回写台账 blocked）**：推送失败或硬判据不成立；需要 force-push / 打 tag / 删分支 / 改远端默认分支；需要改生产 D1 / R2 / Cloudflare 的数据；阶段 6 的切读与放行范围；任何超出本文件 §2 写集与 §1 红线的改动。
- 通用协议其余条款照旧：台账只走闸门（只追加、不改写他人条目）、锁归本环持有并定期 renew、**每环只做一个阶段**、产物落盘、**不伪造验收证据**、缺料时 `blocked_on_append` + `set_status=blocked` + 释放锁。

## 0. 背景（每环先读这些，不必从头翻仓库）

- 本批 = BR-①g 客户端桶化（ADR `docs/adr/0077-bucket-level-sync-storage-architecture.md`）+ PG 切源四段（W7）+ P0 词典完整性（ADR `docs/adr/0078-*`）。
- 现役写入端 = Cloudflare Worker / D1：`sync_items` 40,069 行 + `sync_buckets` 1,000 行（Σ`item_count` = 92,353）。
- ECS 上 `fluxing_sync`（PG 18.0）已灌入 9 表快照（2026-11-26 23:36），**已落后于 D1**；服务监听 `127.0.0.1:8787`，对外未暴露。
- 客户端桶化阶段状态机 0–4（ADR 0077 `:238-254`）：0 老端点单跑 / 1 双写 / 2 双写+交叉校验 / 3 切读 / 4 老端点退役。
- 2026-11-26 23:59:53 起客户端已**常驻阶段 1（双写）**：User 级 `FLUXING_CLOUD_SYNC_BUCKET_PHASE=1`，`WeaselServer` 已重启为新进程。
- **AC-09 仍 FAIL，不得改记**（只有日志出现 `AC09_VERDICT=PASS` 才可改）。`AC-212` / `AC-215②` **未判定**；`AC-213` / `AC-214` 永久退役。

## 1. 红线（违反即停链，回写台账 blocked）

1. **桶阶段 3 / 4 永不启用**（`FLUXING_CLOUD_SYNC_BUCKET_PHASE` = 3 切读 / 4 老端点退役；编号口径见 `docs\relay\stage-2-rulings-20261127.md` §0）；**接力阶段**（本文件 §2 的 1–6 段）不得自动递增（ADR 0077 D12）。回滚口径 = 设 User 级 `FLUXING_CLOUD_SYNC_BUCKET_PHASE=0` + 重启一轮，不改代码、不重装。
2. **规则 1 盲区未修前不得切读**（`WeaselServer/Cloud/CloudSyncService.cpp:2001-2005` + `:2108` 的死分支）。
3. **凭据值绝不进日志 / 输出 / 提交物**（只报键名 + 长度 + sha16 前 16 位）；令牌不拼命令行与 URL；带令牌时禁 verbose；令牌不常驻 User 级环境变量。
4. **生产 D1 / R2 / Cloudflare 只读**，除非 handoff 明确授权写入；任何写操作必须先写探针、留证、限定行数。
5. **不动用户词典数据**：本批不清洗、不重建用户词典；`RimeUserDir`(HKCU) 与 `InstallDir`(HKLM) 都**不得清**。
6. **多个 runner 绝不并发**（都杀 `WeaselServer`，互重置 `startup_pending_`）；起跑前 `Test-Path` 自检，`Restart` 先起后杀。
7. **不改 / 不入库**：`librime/`、`release/`、`env.bat`、`msbuild*.log`；`reports/**`、`scripts/verify/*.ps1`、`tmp/**` 不入库。
8. 提交用 Conventional Commits + 模块前缀（`WeaselServer | WeaselTSF | WeaselDeployer | cloud | docs | fluxing | installer | ci`）；**按路径显式 `git add`，禁 `git add .`**。
9. 提交前必跑 `git status -sb` + `git diff --stat`（挡 `weasel.props` / `env.bat` / `*.obj` 误提交）。
10. 远端走 **SOCKS5 代理 `socks5h://127.0.0.1:7897`**（直连与 HTTP 代理形式均已证伪；仓库级代理已设好 ⇒ 裸命令即可，**不要**带 `-c http.proxy=`），硬判据 = `git ls-remote` 与 `git rev-parse HEAD` **逐字相等**。
11. **台账数字一律盘上回读**（不抄摘要）；收口条目自带完整背景 / 目标 / 进度 / 计划 / 裁决，指针只作补充且须全路径可解析。
12. 用户可见字符串保持简体中文（constitution P2/P5）；产出文档与日志中文，装置脚本 ASCII。

## 2. 阶段清单（6 段，按序推进；阶段不得自动前进，验收不过就停在当前段）

> **阶段序列变更通知（2026-11-27 15:47 · 用户裁定「摘掉阶段 3，先修 P0 假空再切源」）**
>
> - **原阶段 3（D1 写预算阈值口径落盘 80,000 → 20,000 + `code: 7500` 负样本自证）整体摘除。**依据三条：① Cloudflare D1 已于 2026-11-26 由用户拍板「现在就切 jury.xin」（`docs/research/fluxing-cloud-2026-11/port-to-pg-recon.md:5`）⇒ D1 判退役，其专属记账工作随之作废；② `docs/archive/handoffs/handoff-2026-11-23-batch-br-write-amplification-and-zero-write-register.md:2665` 明文「换底座作废 6 项」、`:2671` 推荐「冻结 A 的 D1 专属工作、不进阶段 3」；③ 同文件 `:2663` —— 自建 PG 没有 D1 的写预算刹车 ⇒ **必须先修 P0 假空再切底座**。
> - **新阶段 3 槽位改载「P0 假空修复」，并升为切源硬前置**（写入阶段 6 硬前置，见下）。阶段 4 / 5 / 6 的段序与内容不变（R2 对象搬运 / PG 重灌 / 切源 W7）。
> - **段数仍为 6、编号不复位**：`docs/relay.json` 的 `max_stages=6` / `current_stage` / `stages[]` 与 `start-relay.ps1` 的 bootstrap 均按 1–6 编号，重编号会破坏台账一致性。
> - **台账口径**：`docs/relay.json` 的 `stage_titles[2]` 仍写旧文案（「写预算阈值落盘 20000 + 超限负样本自证」），而闸门 `docs\relay\relay-ledger.ps1` 的动作只有 `status` / `commit`、可写 key 白名单**不含 `stage_titles`** ⇒ 无法经闸门改写，且禁止手改 ⇒ **阶段语义一律以本文件为准；运行时台账里的旧标题作为历史文案保留，不作为执行依据。**

### 阶段 1 — 确认环：上一链阶段 1 结论的零改动复核（**不出包、不写代码**）

- **为什么改名**：`start-relay.ps1` 的 bootstrap **硬写 `current_stage = 1`**，启动器**无**「从阶段 N 起跑」参数 ⇒ 归档重跑后新链必从阶段 1 起跑；而上一链阶段 1（A-0 事实核对 + B 定性 + F3 诊断行 + 规则 1 盲区修复）已 `done` 并入库（commit `1d86d8c`）⇒ 本段重定义为**确认环**，避免重做修复轮。裁定依据 = `docs\relay\stage-2-rulings-20261127.md` §7.3。
- **前情（已完成，不再重做）**：见 `docs\relay\stage-1-inventory.md` 与 handoff `## AH`；判据④ 的 v2 替代口径与四道判据 v2 逐字见 `docs\relay\stage-2-rulings-20261127.md`。
- **目标（三件同批，全部只读）**：
  1. **祖先确认** —— `git merge-base --is-ancestor 1d86d8c HEAD` 退出码 0（阶段 1 交付仍在 HEAD 祖先链上）。
  2. **门禁四行复跑** —— 先单独 msbuild `test\TestFluxingCloud\TestFluxingCloud.vcxproj`（**显式 `/p:SolutionDir=`**；`weasel.sln` 不含该工程 ⇒ 跑旧 exe 会假绿），再跑 `reports\fluxing-br-1g\gate-p0-run.ps1`；**跑前 `$env:FLUXING_CLOUD_SYNC_BUCKET_PHASE='0'`**（否则测试进程继承 User 级相位 ⇒ 成批假失败）。判绿 = `PASS>=1014` 且 `FAIL=0` 且 `UFFFD_COUNT=0` 且 `CLANGFORMAT_HUNK_DRIFT=0`（**不认 `$LASTEXITCODE`**）。
  3. **阶段 1 结论仍成立** —— `git log 1d86d8c..HEAD -- WeaselServer/Cloud test/TestFluxingCloud` 无改动，或改动逐条比对 `stage-1-inventory.md` 后不推翻其结论。
- **写集**：**空**（零产品代码改动、零生产写；只允许写 `docs/relay/**` 与 `reports/fluxing-br-1g/**`）。
- **产出**：`docs\relay\stage-1-confirm-20261127.md`（三件判据 + 门禁四行原样落盘）+ handoff 新顶节 + 台账 `stage_done`。
- **红线**：不写产品代码、不出包、不碰生产；`AC-09` 仍 FAIL 不得改记；本段不得顺手修阶段 2 的代码（缺陷 ①-1 属阶段 2 前半段）。

### 阶段 2 — 复核轮：真机重跑 + 四道判据重判

- **目标**：等 kBoth 轮（轮间隔实测 ≈62 分钟；进程启动后第 1 轮恒为 `kPullOnly`、第 2 轮才是 `kBoth`）出证据，按（修订后）口径**重判四道判据**，并做侧车 `brow`/`row` 与生产 `sync_buckets` 的只读差分。
- **判据（v2 · 2026-11-27 裁定，逐字取自 `docs\relay\stage-2-rulings-20261127.md`）**：
  - ① **桶端点请求无客户端缺陷**：`bucket pull` 全部分页 **4xx 必须为 0**（4xx = 客户端请求构造 / 鉴权缺陷，出现任一 ⇒ 判红且当轮作废）；**5xx 逐条登记**（时间 / page / offset / 状态码 / 当轮是否早退），不单独判红。**判绿** = `4xx = 0` **且** 不存在"同一 page（同一 offset）连续两轮 5xx"（后者视为服务端持续性缺陷 ⇒ 判红）。**部分绿** = `4xx = 0` 且存在单轮单页 5xx ⇒ 判据① 记 PASS，但必须把 5xx 登记进 handoff **并**触发一次服务端可用性只读排查。
  - ② **桶拉完整性**：取取证批次内**最近一个**同时满足「`bucket pull` 全部分页 http 均为 2xx」与「最后一页 `has_more=0`」的轮次。该轮 `pulled_buckets` 必须等于**同期**生产只读回读的 `sync_buckets` 该用户行数，`pulled_items` 必须等于 `Σ item_count`；同期 = 同一取证批次 + 生产侧轮前轮后零写入自证。**判绿** = 逐字相等。**判红** = 该轮 `has_more=1` 或存在任一非 2xx 分页。**防作弊**：批次内**所有**轮次的分页状态必须全部落盘，不得只保留有利轮次。
  - ③ **桶推执行证明**：同一轮日志必须同时出现 ① 至少一行 `bucket quota ` 前缀诊断行（`FormatBucketQuotaLine`，字段 `held_unproven / held_quota / rows_pushed / rows_held / quota_rows`）② 同一轮 `bucket phase=` 行的 `pushed + skipped >= 1`。**判绿** = 两条同时成立 **且** `rows_pushed <= quota_rows`。**判红** = 出现 `bucket phase=` 行但同轮无 `bucket quota` 行（桶推函数未进入配额分支），或 `pushed + skipped = 0`（闸门全关，桶推路径未被验证）。**未判定** = 日志无 `bucket phase=` 行（轮未执行）。
  - ④ **桶侧不得旧于本地**：以本地侧车 `row` 全量键集为基准，逐 key 比较 `(ms, counter)` 偏序，输出四分类 `equal` / `bucket_newer` / `bucket_older` / `unmapped`。**判绿** = `unmapped = 0` **且** `bucket_older = 0`（按 1.3 归因后）。`bucket_newer > 0` 允许（HLC 单调语义下的正常前移），但 `bucket_newer > 1000` 须在 handoff 登记为观察项。**鉴别力**：同轮必须跑三份样本 —— 基线 / `badnewer` / `badolder`；`badolder` 的 `bucket_older` 必须相对基线精确 +N（本环实测 +500 ⇒ 506），`badnewer` 只增 `bucket_newer` 不得改动 `bucket_older`（实测保持 6），三份键集规模必须恒等（实测恒 94,589）。任一条不成立 ⇒ 判据本身作废，退回结果不可用。
- **硬约束**：取证 runner 必须**只读**、自带落盘判据 + 心跳（detached runner 会被环境回收）；**不得并发**（都杀 `WeaselServer`，互重置 `startup_pending_`）；**切阶段/重启必须让新进程拿到新环境块**（User 级变量只写注册表，旧环境块拉起的进程读不到）。
- **产出**：`docs\relay\stage-2-inventory.md`（新建：四判据重判裁定）+ handoff 新顶节 + 证据文件清单。
- **红线**：不切读、不动阶段（**桶阶段** 3/4 仍禁 —— 主语是 `FLUXING_CLOUD_SYNC_BUCKET_PHASE`，**不是**接力阶段 3/4；接力阶段 3 已改载 P0 假空修复，见 §2 变更通知）；生产只读。

### 阶段 3 — P0 假空修复（切源硬前置三 · 客户端侧 + 门禁用例）

- **为什么占这个槽位**：原阶段 3（D1 写预算阈值落盘 + `code: 7500` 负样本自证）随 D1 判退役**整体作废**，不得以任何形式在本段重开（见 §2 变更通知与 §5 D-5）。本槽位改载换底座路线上**跨底座仍然必需**的客户端缺陷 —— handoff `:2665` 的六项作废清单核对下来，只剩 **P0 假空** 与 `AC-09` 两项。
- **缺陷本体**：本地用户词典被导出 / 快照链路读成空（或极小），而空快照是**覆盖式还原** ⇒ 另一台设备还原它就会清空本机词典。已实证但互相矛盾的两个现象：2026-11-26 phase 1/2 自证轮 `matched=92353`（**未复现**假空），而此后多轮本地词典**稳定只读出 67 条**（handoff `:2447`，两次独立轮次同值）。矛盾**未归因**（handoff `:2605`「机制未定」，同一文件 `:2570` 已把该项由「恒现」降为「本轮未复现」但**明确不撤销**）。
- **⚠ 第一步必须重读现码，不许照抄 handoff 的机制描述**：handoff `:2405-2411` 记的「`exported` 在空文件时也自增」是**旧口径**。ADR 0078 已把两侧护栏落码并门禁绿（`PASS=951`、包 `fluxing-1.3.11`），盘上实测：
  - 导出侧 `WeaselServer/Cloud/CloudRimeUserDictBackend.cpp:385` `FilterUserDictTextsByContent(...)` ⇒ `exported = kept.size()`（空文件 / 只有元信息行不计成功）；`:395` 诊断行 `userdict export_empty_skipped=<n>`；`:400-401` `exported == 0` 时 `return false`（err =「备份内容为空」/「没有可导出的词典」）。
  - 还原侧 `:438` 同一过滤器 ⇒ `:473` 诊断行 `userdict import_empty_rejected=<n>`；`WeaselServer/Cloud/CloudSyncService.cpp:1532` 在 `tsv_data_rows == 0` 时打 `userdict import_empty_rejected=1` 并拒绝覆盖。
  ⇒ **本段不是「加护栏」，而是「坐实剩余可达路径 + 把 67 vs 92,353 的矛盾归因 + 落能红能绿的用例」**。
- **目标（四件，按序）**：
  1. **机制定性（只读）**：盘上重读 `WeaselServer/Cloud/**` 的导出 / 快照 / 还原 / 语义指纹（`s2:`）四条链，产出「假空可达路径清单」+「67 条 vs 92,353 条的差异归因」。若结论 = 「现存护栏已闭合所有可达路径」，必须附**反例尝试记录**（试过哪些路径、为何不通），不得只写「没找到」。
  2. **失败复现用例（门禁内，必须先能红）**：在 `test/TestFluxingCloud/**` 加夹具自造的用例 —— 构造「内容为空 / 只有元信息行」的快照与载荷，断言导出侧不判成功、还原侧拒绝覆盖（断言锚点 = 上两条诊断行与 `return false` 分支）。**鉴别力要求**：把 ADR 0078 的两处护栏临时短路后该用例必须变红（短路仅限本地实验、**不入提交**）；未验证鉴别力的用例不算数。
  3. **修复**：按第 1 步定性结果执行 —— 定性为「护栏已闭合」⇒ 把结论固化为**常驻回归用例** + 补语义化诊断行；定性出新路径 ⇒ 按新路径修。红线见下。
  4. **门禁 + 出包装机 + 真机取证**：C++ 门禁须**单独** msbuild `test\TestFluxingCloud\TestFluxingCloud.vcxproj` 且**显式带 `/p:SolutionDir=`**（`weasel.sln` 不含该工程 ⇒ 否则跑的是旧 exe、假绿）；跑前 `$env:FLUXING_CLOUD_SYNC_BUCKET_PHASE='0'`。判绿 = `PASS >= 951`（含新增用例）且 `FAIL=0` 且 U+FFFD=0 且改动 hunk clang-format 0 漂移。若出包 ⇒ 升版 + `output/archives/fluxing-<ver>-installer.exe` + 装机后**构建 = 包内 = 装机后三处 sha256 全等**取证。
- **判据（逐字对齐 handoff `:2760` 的假空闸门）**：`kDefaultBaseUrl` 改指新域名之前，**必须已有** P0 假空的**失败复现用例** + 修复后的**通过用例**，且两者在门禁内可复跑。
- **写集**：`WeaselServer/Cloud/**` + `test/TestFluxingCloud/**`（若出包另加 `version.txt` / `CHANGELOG.md`）。**不动** `cloud/fluxing-sync/**`（D1 侧已退役）、**不动** `docs/relay.json`、不动用户词典数据。
- **产出**：`docs\relay\stage-3-p0-fakeempty.md`（机制定性 + 用例清单 + 门禁四行 + 出包与本机取证）+ handoff 新顶节 + 台账 `stage_done`。
- **红线**：① **不得用「提阈值 / 吞异常 / 跳过断言 / 放宽比较 / 只加日志」当修复**；② 不得放宽或删除既有断言与用例；③ 真机取证只读侧车与日志，不写用户词典；④ `AC-09` 按日志原样记（仅当日志出现 `AC09_VERDICT=PASS` 才可改记）；⑤ **桶阶段相位冻结 0**；⑥ 生产写默认不需要，若确需仍须单独申请人工授权；⑦ 原阶段 3 的 D1 项目（`WRITE_BUDGET_ROWS_PER_DAY` 20,000 / `write_usage` 计费行 / `code: 7500` 负样本 / 曾拟的 ADR 0082 与 spec 104）**一律作废，不得在本段重开**。

### 阶段 4 — R2 对象体核对与搬运（切读硬前置一）

- **目标**：核对 `r2_key` 指向的**对象体**是否真在 ECS 上 —— 现在 `r2_key` 只是指针，`FLUXING_OBJECT_ROOT=/var/lib/fluxing-sync/objects` 内容**未核对 / 未搬运** ⇒ 切读必空。
- **判据**：逐 key 存在性核对 + 可解密回读；缺项清单落盘；R2 与 ECS 两端 key 集合差分 = 0（缺项以清单形式留证，不静默补写）。

### 阶段 5 — PG 快照重导重灌与一致性核对（切读硬前置二）

- **目标**：PG 侧仍是 2026-11-26 23:36 快照且持续落后 ⇒ 重导重灌（幂等前提 = **目标表先清空**）。
- **判据**：9 表行数与 D1 逐表一致；孤儿行 = 0；Σ`item_count` 一致；迁移 0001–0004 四行齐；`GET /health` 预期 404（该路由本就不存在，不是故障）。
- **注意**：PG 只是镜像，**D1 仍是唯一现役写入端**；本段不改客户端指向。

### 阶段 6 — 切源 W7 四段落地与收口交接

- **目标**：按 `docs/research/fluxing-cloud-2026-11/port-to-pg-recon.md` `:144` 执行 W7（改 `WeaselServer/Cloud/CloudAccountDialog.cpp:60-61` 的 `kDefaultBaseUrl` + 出包装机），四段「双写 → 校验 → 切读 → 退役」，**不得跳段**；回滚闸门 = 改回 `api.aiec.fun` 一个字符串，不得同批删旧端点代码。
- **硬前置（2026-11-27 增补第四条；同日 15:47 增补第五条并重排）**：阶段 2 完成且盲区闭合；**阶段 3 完成 —— P0 假空：失败复现用例 + 修复后通过用例，两者均在门禁内可复跑（闸门 1，逐字依据 handoff `:2760`；同文件 `:2737` / `:2776` 亦记该硬闸门）**；阶段 4 + 5 完成；`api.jury.xin` 放行范围已裁定（**只放行 `api.jury.xin`，不放行全部 `*.jury.xin`**）；ECS `5432` 公网收口已授权；库内两条历史探针账号是否清理已裁定；**桶阶段相位冻结 0/1**（本段全程 `FLUXING_CLOUD_SYNC_BUCKET_PHASE` ∈ {0, 1}，建议 0 —— 本段切的是 HTTP 端点主机，**不是**桶阶段相位）。
- **收口**：门禁全绿 + 出包装机 + 装机实证 + handoff 双产物 + 台账 `done`。

## 3. 每阶段通用收尾动作（缺一不可）

1. **数字盘上回读**：所有入台账的值（节号 / 字节数 / sha / 棒次号 / 测试数 / 包名 / 行号）当场回读盘上产物；从摘要抄的值默认当未知；落盘后再独立回读复核，不信生成装置自己的断言。
2. **handoff 追加新顶节**（`## AF` / `## AG` …，UTF-8 无 BOM + 全 LF），正文自带完整背景 / 目标 / 进度 / 计划 / 裁决；**不改写既有顶节字节**。
3. **就地改写接力 prompt** `docs/archive/prompts/prompt-next-session-2026-11-23-batch-br-write-amplification-and-zero-write-register.md`（只给全路径指针，不复述 handoff 内容）。
4. **台账回写走闸门**：`docs\relay\relay-ledger.ps1 -Action status` 取在盘 sha256 → `-Action commit -ExpectSha256 <sha>`（先 `-DryRun`）。**禁止手工编辑 `docs/relay.json`**。
5. **commit + push** 到 `kizemo/feat/100-cloud-sync`（**裸命令，不要加 `-c http.proxy=`**；仓库级 `http.proxy` = `socks5h://127.0.0.1:7897` 已设好），硬判据 = `git ls-remote` 与 `git rev-parse HEAD` 逐字相等。
6. **转交下一环**：按 `docs/AGENT-RELAY.md` 的协议在目标项目内 `session_create` 下一环 + `relay-lock.ps1 -Action adopt` 转交锁 + `session get` 回读三要素（`workspace_dir` / `effective_model` / `status`）。

## 4. 已知工具坑（每环照抄，别重新踩）

- `Split-Path -LiteralPath <p> -Parent` 在 PS7 报参数集错误 ⇒ 用 `[IO.Path]::GetDirectoryName()`。
- 顶节检索用 `(?m)^## [A-Z]+$` 只命中 2 个 ⇒ **去掉 `$` 锚点**。
- PowerShell 插值里写 `"$repo\tmp\cm$i.txt"` 会「变量引用无效」⇒ 改 `("cm" + $k + ".txt")`。
- `npx` 跨目录调用失败 ⇒ `cmd /c cd /d <dir> && npx --no-install wrangler …`。
- `cd` / `Set-Location` 后写相对路径被权限门拦截 ⇒ 一律绝对路径。
- 出包必须 VS2022 `vcvars32.bat`（MSVC 14.44.35207）+ `call env.bat` + `xmake f -a x86 -m release -c` + `xmake -r` + `xbuild.bat weasel installer`；用 VS18 会 `LNK2019`。
- `weasel.sln` **不含** `TestFluxingCloud` 工程 ⇒ 门禁必须单独 msbuild 且显式带 `/p:SolutionDir=`，否则跑的是上一轮旧 exe（PASS 恒定不变 = 假绿）。
- 真实安装路径 = `D:\Program Files\fluxing\weasel\`；诊断日志真身 = `%TEMP%\fluxing-profile-notify.log`（须共享读 `[IO.File]::Open($diag,'Open','Read','ReadWrite')`）。
- 中文 `.ps1` 给 PS 5.1 执行必须 UTF-8 BOM；装置脚本一律纯 ASCII 无 BOM。
- `claude` / CLI 旗标必须写在 prompt **之前**（MCode 运行时用 `session_create`，无此坑）。
- **User 级环境变量对「旧环境块拉起」的进程不可见** ⇒ 「改完变量重启进程」≠ 生效；必须新登录，或先在当前进程刷新 `$env:` 再 `Start-Process`。
- **detached runner 会被环境回收** ⇒ 取证 runner 必须自带落盘判据 + 心跳，不能只靠「后台进程还活着」当证据。
- 起环 / 跨环交接时，`docs/relay.json` 与 `docs/relay/.relay.lock` 是**运行时状态**，**不入库**；入库的只有装置本体（`docs/relay/*.ps1`、`AGENT-RELAY.md`、`RELAY-SOP.md`、`ACTIVATION-PROMPT.txt`、`install-report.md`）+ `docs/RELAY-ADAPTATION.md` + `docs/plan-relay.md` + `AGENTS.md` 家规块 + `docs/relay/adaptation-answers.json`。
- **GitHub 推送唯一可用通道 = SOCKS5**：`socks5h://127.0.0.1:7897` —— 2026-11-27 实测：`http://127.0.0.1:7897`（HTTP 形式）curl 返回 **000**；`--socks5-hostname 127.0.0.1:7897` 返回 **200**；直连 `Recv failure: Connection was reset` / `Failed to connect ... after 21070 ms`；`curl --resolve` 边缘 IP 全 000；`curl -6` 000；`api.github.com` 直连 200 而 `github.com` 不通 ⇒ 阻断在 `github.com` 域，非整机网络。**已处置**：仓库级 `http.proxy`/`https.proxy` = socks5h（仅本仓 `.git/config`，未动用户全局；全局那条恰是失效的 HTTP 形式）⇒ 裸 `git push` / `git ls-remote` 可用，**硬判据照旧** = `ls-remote` 与 `rev-parse HEAD` 逐字相等。
- **停链后不可直接续链**：闸门 `relay-ledger.ps1` **拒收** `set_status=running`（`status_running_is_human_only`）；启动器对 `status=blocked` 的台账直接 `exit 3`（`reason=ledger_blocked`）；bootstrap 只在台账**不存在**时执行 ⇒ 恢复链的唯一受支持路径 = **人类换代动作**：归档旧台账（留证）→ 重跑 `start-relay.ps1` 重新 bootstrap。 **2026-11-27 已执行换代**：旧台账归档为 `docs\relay\archive\relay-20261127-stage2-blocked.json`（`status=blocked` / `current_stage=2` 原样留证）→ 重跑 `start-relay.ps1` 重新 bootstrap（新链 `current_stage=1`）→ 阶段 1 重新定义为**确认环**（见 `docs\relay\stage-2-rulings-20261127.md` §7）。

## 5. 人工裁决（2026-11-27 · 依用户「持续推进 + 自动会话交接」授权，由上游会话代为行使人类裁量）

> 对上一链 `docs\relay\stage-1-inventory.md` §7.2 的 A/B/C 三项裁决全文（A/B 两项中与之冲突的部分已被本节 D 的 2026-11-27 七项裁定取代；**裁定全文** = `docs\relay\stage-2-rulings-20261127.md`）。

### A（桶推 HLC 口径）→ **先事实核对；本轮既不改判据、也不动生产**

- **已坐实的事实链**：桶侧 HLC 与**本地侧车**逐字相同（样本 `legacy_hlc=1795339039468:14944:c7cc…` vs `bucket_hlc=1795565189774:14945:c7cc…`，桶侧 `ms` 恒为 1795565189774、counter 逐条 +1）⇒ **桶推既没篡改也没重分配 HLC**；是 2026-11-26 桶化首导时**本地侧车被整库重分配**了 HLC，老端点 D1 永久持 11-24 旧批次 ⇒ 两路差异**不可自愈**。
- **不采纳 A1（桶推沿用老端点 HLC）**：桶推能沿用的三个来源此刻存的**都已是新值**（本地侧车 / 远端桶 / 远端 legacy 拉取值）⇒ 要两路逐字相等只能三选一：① 回退侧车 93,278 行条目 HLC（本地状态大规模重写，触碰「不动用户词典数据」边界）② pull 时让 legacy 旧 HLC 覆盖本地新 HLC（**违反 HLC 单调语义**，且削弱服务端防陈旧写守卫）③ 一次性迁移桶表把 HLC 改回 legacy 值（**写生产 D1**）⇒ 三条都超红线，**本链不做**。
- **暂不采纳 A2（修订判据④）**：改判据必须先有 ① 926 条的真实构成（否则 = 用改口径掩盖真实漏数据）② 桶推 HLC 分配点的代码事实（是否绕过 R1）⇒ 现在改口径 = 把「还没查清」提前写成「已接受」。
- **本轮裁定 = A-0（事实核对，零改判据、零生产写）**，核对清单：① 桶推路径的条目 HLC 分配点是否绕过 BR-①c 的 R1；② 侧车条目 HLC 的写回语义（pull 取 max 还是无条件沿用远端）；③ 「桶侧 HLC == 本地侧车 HLC（逐 key）」能否作判据④ 的替代口径（**切读安全性口径**：桶端点不得旧于本地）并给出**鉴别力用例**（坏样本：桶侧旧于本地 ⇒ 必须判红；好样本：相等 ⇒ 判绿）；④ 若最终仍要两路逐字相等，给出 A1 三条路线的**真实代价对照**，交人工再裁定。
- **ADR**：阶段 1 修复轮只产出**判据④ 口径 ADR 草案**（含鉴别力用例 + 采纳/不采纳 A1 的代价对照），**不改 ADR 0077**；正式修订待人工看过草案再落。

### B（926 条缺口定性）→ **立即开工，并入阶段 1 修复轮**（不单设批次）

- **理由**：926 条里若含「本地现存但未上桶」= **真实漏数据**，优先级高于 HLC 口径；且它与规则 1 盲区（「库存未证实不推送」死分支）同属「库存未证实」家族 ⇒ 先定性再修，避免两盲区叠加放大（叠加会让半库存桶被回推 ⇒ 服务端 `DELETE+INSERT` 静默删条目）。
- **交付**：① 云端 `item_key` 的 hash 口径（客户端侧生成函数的**确切算法**，含规范化与编码）②「侧车文本键 ↔ 云端 `item_key`」映射实现 ③ 926 条**逐条分类表**（本地已删僵尸 / 本地现存未上桶 / 不可判定）④ 据此重跑判据④ 的可复跑脚本。
- **边界**：**只读** —— 生产 D1 只 SELECT、侧车只读、本地词典只读；不写任何生产表、不动本地词典数据。

### C（是否放行下一段）→ **放行，但重排链的阶段序列**（新链 6 段）

- **阶段 1（修复轮 · 客户端侧 · 零生产写）** = 上面 A-0 + B + F3 + 规则 1 死分支修复 + 判据④ 口径 ADR 草案。
- **阶段 2（复核轮 · 真机）** = 等 kBoth 轮出证据，按（修订后）口径重判四道判据 + 侧车 `brow`/`row` 差分 + 生产 `sync_buckets` 只读差分 + 阶段 1 盘点就地改写。
- **阶段 3 / 4 / 5 / 6** = **P0 假空修复（切源硬前置三，2026-11-27 15:47 用户裁定换载）** · R2 对象体核对与搬运 · PG 快照重导重灌与一致性核对 · 切源 W7 四段落地与收口交接（**阶段 4–6 段内容不变**）。~~写预算阈值口径落盘（80,000 → 20,000）~~ **同日 15:47 摘除、整条作废**（D1 已判退役，依据见 §2 变更通知）。
- **为何把规则 1 死分支并进阶段 1**：它与 B / A-0 同一写集（`WeaselServer/Cloud/**` + `test/TestFluxingCloud/**`）⇒ 一次门禁、一次真机验证；分开会多等一轮 62 分钟。

### D（2026-11-27 七项裁定）→ **取代 A/B/C 中与之冲突的部分**

> 权威全文 = `docs\relay\stage-2-rulings-20261127.md`（§0–§9）。本节只登记裁定与落点，理由见原文；**不在此复制全文**（避免第二真相源）。
> 执行顺序不可颠倒：**改判据文本 → 归档旧台账 → 重跑 `start-relay.ps1`**（重跑必须最后，否则新链仍按 v1 判据判）。

1. **判据④ 采纳替代口径「桶侧不得旧于本地」**：以本地侧车 `row` 全量键集为基准，逐 key 比较 `(ms, counter)` 偏序，四分类 `equal / bucket_newer / bucket_older / unmapped`；判绿 = `unmapped = 0` 且 `bucket_older = 0`（按四准入归因后）；`bucket_newer > 1000` 登记为观察项；鉴别力 = 三样本（基线 / `badnewer` / `badolder`）键集恒等且坏样本精确偏移。**ADR 0081 升 `Proposed`**；本节 A 项「暂不采纳 A2」**作废**；`extra_in_bucket` 明文退出判据④。
2. **判据③ 改写为「桶推执行证明」**：同轮须见 `bucket quota ` 行 + `pushed + skipped >= 1` + `rows_pushed <= quota_rows`；有 `bucket phase=` 而无 `bucket quota` ⇒ 判红；无 `bucket phase=` ⇒ 未判定。**放弃** `pushed + skipped + held = 计划桶数`；**不取**「历史累计 1,024/1,024」的有利读法（`brow` 是桶级账，不是本轮推送计数）。
3. **判据② 补口径**：取取证批次内**最近一个**同时满足「全部分页 2xx」与「最后一页 `has_more=0`」的完整轮；`PASS_WITH_CAVEAT` 升 **PASS**；陷阱 = 503 页的 `has_more=0` 是错误响应体解析结果；批次内所有轮次分页状态必须全部落盘。
4. **判据① 改写**：`4xx = 0`（出现任一 ⇒ 判红且当轮作废）/ `5xx` 逐条登记不单独判红 / 「同一 page 连续两轮 5xx」才判红；本环 ① 记 **PASS**；新登记客户端缺陷 **①-1**（`bucket pull` 无有限重试 ⇒ 单次 5xx 即整轮早退）归**接力阶段 2 前半段**，实现要求 = 同 page 5xx 重试 ≤ 2 次（退避 1s / 3s）、4xx 不重试、仍失败打印 `retry=<n>`。
5. **写预算落 20,000**：口径维持每账号；「≈2,000 行/轮」订正为 **6,096 行/轮** ⇒ 20,000 ≈ 3 轮满推；**执行是一次生产写（改 Worker env + deploy 换 Version），必须单独申请人工授权**。**→ 2026-11-27 15:47 用户改判：D1 已判退役（B 路线「现在就切 jury.xin」），本条含其授权前提整体作废；接力阶段 3 槽位改载 P0 假空修复，见 §2。该阈值不再调整、不再部署、不再负样本自证。**
6. **接力阶段 6 不属桶阶段切读禁令**：红线主语是 `FLUXING_CLOUD_SYNC_BUCKET_PHASE` = 3 / 4；阶段 6 期间桶阶段相位**冻结 0/1**，产出须写明「切的是 HTTP 端点主机，不是桶阶段相位」（已落 §2 阶段 6 硬前置与 §4 红线）。
7. **恢复链 = 改判据文本 → 归档旧台账 → 重跑**；因 bootstrap 硬写 `current_stage=1`，新链阶段 1 = **确认环**（零代码改动、不出包），阶段 2 内部段序 = `2A` 修 ①-1 → 门禁出包装机 → `2B` 等 kBoth → 四判据 v2 重判 → `2C` 补记。
### 本轮新增红线

- **任何服务端一次性迁移 / 写生产 D1 的动作（含 A1 的生产部分）必须先经人工单独授权**；链内环不得自行执行。
- **桶阶段** 3/4 永不启用（`FLUXING_CLOUD_SYNC_BUCKET_PHASE` = 3 切读 / 4 老端点退役 —— **不是**接力阶段 3/4/6；接力阶段 3/4/6 未被禁止）；规则 1 盲区未修**且**判据④ 未按 v2 重判前不得切读；阶段不得自动递增（人工放行 = 本节）。
- 生产 D1/R2/Cloudflare 只读；不动用户词典数据；`AC-09` 仍 FAIL 不得改记；凭据值绝不进日志/输出/提交物。

### 仍待人工裁定（等阶段 1/2 结论，本链不自行决定）

1. ~~A1 的生产部分是否执行（服务端一次性迁移 / 守卫放宽 / 侧车 HLC 回退）~~ → **2026-11-27 已裁定：不执行**（三条路线全部越红线，见本节 A 与 `stage-2-rulings-20261127.md` §1.5）。
2. ~~`WRITE_BUDGET_ROWS_PER_DAY` 盘上 80,000 vs §O 拍板 20,000~~ → **2026-11-27 已裁定：落 20,000**（执行需单独授权，排在接力阶段 3，见本节 D-5）。**→ 同日 15:47 用户改判：摘掉该阶段、整条作废（D1 判退役）；接力阶段 3 槽位改载 P0 假空修复，见 §2。**
3. ECS `5432` 公网收口窗口与代执行授权范围；
4. 库内两条历史探针账号（`ak.probe.261111@example.com` / `ak.probe2.261111@example.com`）是否清理；
5. 切源 W7 的 `api.jury.xin` 放行范围（只放行 `api.jury.xin`，**不放行**全部 `*.jury.xin`）+ 橙云/灰云 + 证书路线（阶段 6 之前需定）。
