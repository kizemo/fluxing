# 接力链计划：BR-①g 桶化双写后续批次（2026-11-27 起）

> 本文件是接力链每一环的**唯一计划入口**。启动器已把它写进台账 `plan_doc`，每一环读同一个文件。
> **权威进度源不是本文件**，而是 handoff 顶节：
> `docs/archive/handoffs/handoff-2026-11-23-batch-br-write-amplification-and-zero-write-register.md`
> （检索 `^## [A-Z]+`，**不要加 `$` 锚点**；最新顶节 = 本批最新事实；最新 = `## AE`，由 2026-11-26 23:59:53 的「进双写」轮追加）。
> 接力 prompt 文件：`docs/archive/prompts/prompt-next-session-2026-11-23-batch-br-write-amplification-and-zero-write-register.md`（就地改写，不另建）。

## ★ 本项目覆盖（**优先于通用接力协议**，每环必读）

- **git 写操作在本仓不是人类检查点**：通用协议里「git 写操作（commit / push / tag / stash / checkout）是人类检查点」一条**在本仓不适用**。本仓宪法 P4 与本批规约要求**每阶段收尾必须 commit + push**，因此链内环可在**门禁绿**之后自行 commit + push 到 `kizemo/feat/100-cloud-sync`，无需等待人工确认。
- commit 纪律照旧：Conventional Commits + 模块前缀（`WeaselServer | WeaselTSF | WeaselDeployer | cloud | docs | fluxing | installer | ci`）；**按路径显式 `git add`，禁 `git add .`**；提交前必跑 `git status -sb` + `git diff --stat`；`librime/`、`release/`、`env.bat`、`msbuild*.log`、`reports/**`、`scripts/verify/*.ps1`、`tmp/**` **不入库**。
- push 硬判据 = `git ls-remote kizemo refs/heads/feat/100-cloud-sync` 与 `git rev-parse HEAD` **逐字相等**；远端操作直连（`-c http.proxy= -c https.proxy=`）。
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

1. **阶段 3 / 4 永不启用**；阶段**不得自动递增**（ADR 0077 D12）。回滚口径 = 设 User 级 `FLUXING_CLOUD_SYNC_BUCKET_PHASE=0` + 重启一轮，不改代码、不重装。
2. **规则 1 盲区未修前不得切读**（`WeaselServer/Cloud/CloudSyncService.cpp:2001-2005` + `:2108` 的死分支）。
3. **凭据值绝不进日志 / 输出 / 提交物**（只报键名 + 长度 + sha16 前 16 位）；令牌不拼命令行与 URL；带令牌时禁 verbose；令牌不常驻 User 级环境变量。
4. **生产 D1 / R2 / Cloudflare 只读**，除非 handoff 明确授权写入；任何写操作必须先写探针、留证、限定行数。
5. **不动用户词典数据**：本批不清洗、不重建用户词典；`RimeUserDir`(HKCU) 与 `InstallDir`(HKLM) 都**不得清**。
6. **多个 runner 绝不并发**（都杀 `WeaselServer`，互重置 `startup_pending_`）；起跑前 `Test-Path` 自检，`Restart` 先起后杀。
7. **不改 / 不入库**：`librime/`、`release/`、`env.bat`、`msbuild*.log`；`reports/**`、`scripts/verify/*.ps1`、`tmp/**` 不入库。
8. 提交用 Conventional Commits + 模块前缀（`WeaselServer | WeaselTSF | WeaselDeployer | cloud | docs | fluxing | installer | ci`）；**按路径显式 `git add`，禁 `git add .`**。
9. 提交前必跑 `git status -sb` + `git diff --stat`（挡 `weasel.props` / `env.bat` / `*.obj` 误提交）。
10. 远端操作直连（`-c http.proxy= -c https.proxy=`），硬判据 = `git ls-remote` 与 `git rev-parse HEAD` **逐字相等**。
11. **台账数字一律盘上回读**（不抄摘要）；收口条目自带完整背景 / 目标 / 进度 / 计划 / 裁决，指针只作补充且须全路径可解析。
12. 用户可见字符串保持简体中文（constitution P2/P5）；产出文档与日志中文，装置脚本 ASCII。

## 2. 阶段清单（6 段，按序推进；阶段不得自动前进，验收不过就停在当前段）

### 阶段 1 — 阶段 1（双写）常驻取证收口

- **现状（2026-11-27 08:59 盘上回读）**：阶段 1 **已常驻生效**。User 级 `FLUXING_CLOUD_SYNC_BUCKET_PHASE=1`；`WeaselServer` pid **41800**，启动 2026-11-27 **08:02:34**；08:11:00 那轮 = `bucket phase=1 pushed=0 skipped=0 pulled_buckets=1000 pulled_items=92353`，同轮 reconcile `scan_complete=0 matched=92272 deferred=1311 divergent=10 pushed=0`。
- **下一轮 ≈09:13 = 该进程的第 2 轮 = `kBoth` = 唯一具备推桶能力的轮**（轮间隔实测 ≈62 分钟：00:03:42 / 01:06:12 / 02:08:32 / 03:10:40 / 08:11:00）。本阶段第一件事就是取这一轮的证据。
- **判据**（ADR 0077 `:436-443` 四道判据）：
  - ① 桶端点请求零 4xx/5xx；
  - ② 桶端点拉取桶数 = 服务端 `sync_buckets` 行数（已见 `pulled_buckets=1000` vs 1,000）；
  - ③ 侧车 `brow` 行数 = 已推成功桶数（已见 `brow` = 1,000）；
  - ④ 两路拉取条目集合逐条 HLC 一致（允许桶端点多，**不允许少**）——须**逐 key 比对**；计数相等不算数。⚠ 08:11:00 轮 `pulled_items=92353` 与 `matched=92272` **计数已不相等** ⇒ 口径存疑，**不得据此判绿**。
- **取证据的硬约束**：
  - 进程启动后第一轮恒为 `kPullOnly`（`CloudSyncEngine.cpp:205-213`），**第 2 轮才是 `kBoth`**；探针必须等**第 2 个**同 phase 行（不是第 1 个）。
  - **切阶段 / 重启必须让新进程拿到新环境块**：User 级变量只写注册表，**由旧环境块拉起的进程读不到**（2026-11-26 23:59:53 那次重启后连 4 轮仍是 `phase=0`；直到 08:02:34 新登录拉起的进程才读到 `phase=1`）。要立即生效：先在当前进程刷新 `$env:FLUXING_CLOUD_SYNC_BUCKET_PHASE` 再 `Start-Process`。
  - detached runner 会被环境回收（上一轮 runner pid 36928 已死、证据未落盘）⇒ 取证 runner 必须**自带落盘判据 + 心跳**，且不得并发（都杀 `WeaselServer`，互重置 `startup_pending_`）。
- **产出**：`reports/fluxing-br-1g/phase1-enter-20261127-evidence.log`（**当前不存在，需重新生成**）+ handoff 新顶节 + 生产 `sync_buckets` 差分（只读）。
- **门禁**：本段是取证段，不要求代码门禁，但 ADR 0077 `:436-443` 四道判据必须有盘上证据；全绿才进阶段 2。

### 阶段 2 — 规则 1 盲区修复（`inventory_complete` 死分支）

- **目标**：让 `CloudSyncService.cpp:2108` 的整桶压制分支**真正生效** —— 现在唯一调用点 `:1657-1659` 第 6 实参恒为 `bucket_scan_complete == true`，导致 `if (!inventory_complete && BucketHasUnprovenEntry(...))` 恒不成立。
- **要求**：传参改为真实的库存证据；新增**有鉴别力**的门禁用例（故意置 unproven 时必须触发压制，且好/坏样本各跑一次）；新增 ADR（编号顺延）；改动 hunk 0 clang-format 漂移 + U+FFFD = 0。
- **判据**：门禁 `PASS=<n> FAIL=0`；新用例在 unproven 场景下**压制生效**（不是只编译过）。
- **红线**：本段只改判定逻辑与测试，**不切读、不动阶段**。

### 阶段 3 — 写预算阈值口径落盘

- **目标**：把 §O 拍板值与盘上值的差异订正落盘 —— 盘上 `WRITE_BUDGET_ROWS_PER_DAY=80000` vs §O 拍板 **20,000**（拍板已定、未落盘，差异登记在 handoff）。
- **判据**：ECS 服务 env 回读 = 拍板值；重启后生效；差异订正依据写进 handoff（含「以何者为准」的明确裁决）。
- **注意**：阈值收紧只降不升 ⇒ 属安全方向；若用户另给口径，以最新裁定为准。

### 阶段 4 — R2 对象体核对与搬运（切读硬前置一）

- **目标**：核对 `r2_key` 指向的**对象体**是否真在 ECS 上 —— 现在 `r2_key` 只是指针，`FLUXING_OBJECT_ROOT=/var/lib/fluxing-sync/objects` 内容**未核对 / 未搬运** ⇒ 切读必空。
- **判据**：逐 key 存在性核对 + 可解密回读；缺项清单落盘；R2 与 ECS 两端 key 集合差分 = 0（缺项以清单形式留证，不静默补写）。

### 阶段 5 — PG 快照重导重灌与一致性核对（切读硬前置二）

- **目标**：PG 侧仍是 2026-11-26 23:36 快照且持续落后 ⇒ 重导重灌（幂等前提 = **目标表先清空**）。
- **判据**：9 表行数与 D1 逐表一致；孤儿行 = 0；Σ`item_count` 一致；迁移 0001–0004 四行齐；`GET /health` 预期 404（该路由本就不存在，不是故障）。
- **注意**：PG 只是镜像，**D1 仍是唯一现役写入端**；本段不改客户端指向。

### 阶段 6 — 切源 W7 四段落地与收口交接

- **目标**：按 `docs/research/fluxing-cloud-2026-11/port-to-pg-recon.md` `:144` 执行 W7（改 `WeaselServer/Cloud/CloudAccountDialog.cpp:60-61` 的 `kDefaultBaseUrl` + 出包装机），四段「双写 → 校验 → 切读 → 退役」，**不得跳段**；回滚闸门 = 改回 `api.aiec.fun` 一个字符串，不得同批删旧端点代码。
- **硬前置**（任一不满足 ⇒ 本段停在 `awaiting_user`，不硬切）：阶段 2 完成且盲区闭合；阶段 4 + 5 完成；`api.jury.xin` 放行范围已裁定（**只放行 `api.jury.xin`，不放行全部 `*.jury.xin`**）；ECS `5432` 公网收口已授权；库内两条历史探针账号是否清理已裁定。
- **收口**：门禁全绿 + 出包装机 + 装机实证 + handoff 双产物 + 台账 `done`。

## 3. 每阶段通用收尾动作（缺一不可）

1. **数字盘上回读**：所有入台账的值（节号 / 字节数 / sha / 棒次号 / 测试数 / 包名 / 行号）当场回读盘上产物；从摘要抄的值默认当未知；落盘后再独立回读复核，不信生成装置自己的断言。
2. **handoff 追加新顶节**（`## AF` / `## AG` …，UTF-8 无 BOM + 全 LF），正文自带完整背景 / 目标 / 进度 / 计划 / 裁决；**不改写既有顶节字节**。
3. **就地改写接力 prompt** `docs/archive/prompts/prompt-next-session-2026-11-23-batch-br-write-amplification-and-zero-write-register.md`（只给全路径指针，不复述 handoff 内容）。
4. **台账回写走闸门**：`docs\relay\relay-ledger.ps1 -Action status` 取在盘 sha256 → `-Action commit -ExpectSha256 <sha>`（先 `-DryRun`）。**禁止手工编辑 `docs/relay.json`**。
5. **commit + push** 到 `kizemo/feat/100-cloud-sync`，硬判据 = `git ls-remote` 与 `git rev-parse HEAD` 逐字相等。
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
