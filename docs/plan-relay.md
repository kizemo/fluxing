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

1. **阶段 3 / 4 永不启用**；阶段**不得自动递增**（ADR 0077 D12）。回滚口径 = 设 User 级 `FLUXING_CLOUD_SYNC_BUCKET_PHASE=0` + 重启一轮，不改代码、不重装。
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

### 阶段 1 — 修复轮：判据④ 红因收敛（客户端侧 · 零生产写）

- **前情**：上一链的阶段 1 取证收口已完成 —— 四道判据 **①绿 ②绿 ③绿 ④红**（`missing_in_bucket=926` / `hlc_mismatch=39043` / `http_errors=0`），桶推路径**真机首次跑通**（`pushed=896` / `rows_pushed=1928` / `held_quota=0` / `held_unproven=0`）；判据④ 红因 = 2026-11-26 桶化首导**整库重分配条目 HLC**（桶侧 HLC 与本地侧车逐字相同，老端点 D1 永久持 11-24 旧批次）⇒ 两路差异**不可自愈**。完整证据见 `docs\relay\stage-1-inventory.md` 与 handoff `## AH`；**本轮人工裁决全文见本文件 §5**。
- **目标（四件同批）**：
  1. **B 定性** —— 建立「侧车文本键 ↔ 云端 `item_key`」hash 映射口径，把 926 条拆成「本地已删僵尸」/「本地现存未上桶」/「不可判定」；**只读**（生产 D1 只 SELECT、侧车只读、本地词典只读）。
  2. **A-0 事实核对** —— 定位桶推路径的条目 HLC 分配点是否绕过 BR-①c 的 R1「远端 HLC 沿用」；核对侧车条目 HLC 的写回语义（pull 取 max 还是无条件沿用）；评估「桶侧 HLC == 本地侧车 HLC（逐 key）」能否作判据④ 的替代口径（切读安全性口径），并给出**鉴别力用例**（坏样本必须判红）；产出 A1 三条实现路线的代价对照。**不改判据、不改生产、不改侧车。**
  3. **F3 可观测性** —— 补客户端桶端点 HTTP 状态诊断行（现缺 ⇒ `bucket_pull_failed` 的 4xx/5xx 面查不到）。
  4. **规则 1 盲区修复** —— 让 `CloudSyncService.cpp:2108` 的整桶压制分支真正生效（现唯一调用点 `:1657-1659` 第 6 实参恒为 `bucket_scan_complete == true` ⇒ `if (!inventory_complete && BucketHasUnprovenEntry(...))` 恒不成立）；传参改为真实库存证据；新增**有鉴别力**的门禁用例（故意置 unproven 必须触发压制，好/坏样本各跑一次）。
- **写集（唯一）**：`WeaselServer/Cloud/**` + `test/TestFluxingCloud/**` + `docs/adr/**`（新 ADR 草案）+ `reports/fluxing-br-1g/**`（证据，不入库）。**不得**触碰服务端 `cloud/fluxing-sync/**`、生产 D1/R2、用户词典数据。
- **门禁**：先单独 msbuild `test\TestFluxingCloud\TestFluxingCloud.vcxproj`（**显式 `/p:SolutionDir=`**；`weasel.sln` 不含该工程 ⇒ 跑旧 exe 会假绿），再跑 `reports\fluxing-br-1g\gate-p0-run.ps1`；判绿 = `PASS=<n>` 且 `FAIL=0` + `UFFFD_COUNT=0` + `CLANGFORMAT_HUNK_DRIFT=0`（**不认 `$LASTEXITCODE`**）；改动 hunk 0 clang-format 漂移。
- **产出**：`docs\relay\stage-1-inventory.md`（**就地改写**：保留旧链取证节 + 追加「修复轮」节）+ 926 条分类表 + 判据④ 口径 ADR 草案 + handoff 新顶节 + 门禁输出落盘。
- **红线**：只改判定逻辑 / 诊断 / 测试；**不切读、不动阶段、不写生产**；`AC-09` 仍 FAIL 不得改记。

### 阶段 2 — 复核轮：真机重跑 + 四道判据重判

- **目标**：等 kBoth 轮（轮间隔实测 ≈62 分钟；进程启动后第 1 轮恒为 `kPullOnly`、第 2 轮才是 `kBoth`）出证据，按（修订后）口径**重判四道判据**，并做侧车 `brow`/`row` 与生产 `sync_buckets` 的只读差分。
- **判据**：① 桶端点请求零 4xx/5xx；② 桶拉桶数 = 服务端 `sync_buckets` 该用户行数；③ 侧车 `brow` = 已推成功桶数；④ 按新口径重判（**判别力**：坏样本必须判红）。
- **硬约束**：取证 runner 必须**只读**、自带落盘判据 + 心跳（detached runner 会被环境回收）；**不得并发**（都杀 `WeaselServer`，互重置 `startup_pending_`）；**切阶段/重启必须让新进程拿到新环境块**（User 级变量只写注册表，旧环境块拉起的进程读不到）。
- **产出**：`docs\relay\stage-2-inventory.md`（新建：四判据重判裁定）+ handoff 新顶节 + 证据文件清单。
- **红线**：不切读、不动阶段（阶段 3/4 仍禁）；生产只读。

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
- **停链后不可直接续链**：闸门 `relay-ledger.ps1` **拒收** `set_status=running`（`status_running_is_human_only`）；启动器对 `status=blocked` 的台账直接 `exit 3`（`reason=ledger_blocked`）；bootstrap 只在台账**不存在**时执行 ⇒ 恢复链的唯一受支持路径 = **人类换代动作**：归档旧台账（留证）→ 重跑 `start-relay.ps1` 重新 bootstrap。

## 5. 人工裁决（2026-11-27 · 依用户「持续推进 + 自动会话交接」授权，由上游会话代为行使人类裁量）

> 对上一链 `docs\relay\stage-1-inventory.md` §7.2 的 A/B/C 三项裁决全文。

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
- **阶段 3 / 4 / 5 / 6** = 写预算阈值口径落盘（80,000 → 20,000）· R2 对象体核对与搬运 · PG 快照重导重灌与一致性核对 · 切源 W7 四段落地与收口交接（**原 3–6 段内容不变**）。
- **为何把规则 1 死分支并进阶段 1**：它与 B / A-0 同一写集（`WeaselServer/Cloud/**` + `test/TestFluxingCloud/**`）⇒ 一次门禁、一次真机验证；分开会多等一轮 62 分钟。

### 本轮新增红线

- **任何服务端一次性迁移 / 写生产 D1 的动作（含 A1 的生产部分）必须先经人工单独授权**；链内环不得自行执行。
- 阶段 3/4 永不启用；规则 1 盲区未修**且**判据④ 未重判前不得切读；阶段不得自动递增（人工放行 = 本节）。
- 生产 D1/R2/Cloudflare 只读；不动用户词典数据；`AC-09` 仍 FAIL 不得改记；凭据值绝不进日志/输出/提交物。

### 仍待人工裁定（等阶段 1/2 结论，本链不自行决定）

1. A1 的生产部分是否执行（服务端一次性迁移 / 守卫放宽 / 侧车 HLC 回退）—— 三选一，均触及红线；
2. `WRITE_BUDGET_ROWS_PER_DAY` 盘上 80,000 vs §O 拍板 20,000 —— 排在阶段 3 落盘；
3. ECS `5432` 公网收口窗口与代执行授权范围；
4. 库内两条历史探针账号（`ak.probe.261111@example.com` / `ak.probe2.261111@example.com`）是否清理；
5. 切源 W7 的 `api.jury.xin` 放行范围（只放行 `api.jury.xin`，**不放行**全部 `*.jury.xin`）+ 橙云/灰云 + 证书路线（阶段 6 之前需定）。
