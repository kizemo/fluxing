# acceptance-matrix.md — 验收用例全跑（T601）

> 批次 AN · 2026-11-11 实测 · 逐条跑 AC-01 ~ AC-14，**无「待定」**：每条给「通过 / 不通过」+ 证据路径 + 缺口。
> 环境：线上 `https://api.aiec.fun`（`MAIL_MODE="http"`，Version ID `29d4e572-49d0-43c2-b091-dda3029edf00`）·
> 客户端装机版 `D:\Program Files\fluxing\weasel`（`WeaselServer.exe` PID 257348）· 分支 `feat/100-cloud-sync`。
> 证据文件在 `tmp/`（按任务书「证据不入库」不入 Git），结论同时落在本文件与收尾 handoff。

## 0. 判读说明

- **通过**：有可复跑的判别性证据（命令 + 输出）。
- **不通过**：证据缺失、证据为负、或**前置条件不具备**（需两台设备 / 需登录态 / 需用户在场）⇒ 一律记「不通过」并写明缺什么，**不留「待定」**。
- 服务端侧「已实现」不等于「AC 通过」：AC 的判据含客户端与端到端行为。

## 1. 矩阵

| AC | 判据摘要 | 结论 | 证据 | 缺口 / 失败原因 |
|---|---|---|---|---|
| **AC-01** | 注册 → 登录；口令错有中文提示 | **不通过（服务端已通，真机未闭环）** | 服务端 `register` **200** 且返回 `account_id=d42756f4-866c-465f-a9d4-aa6d47464426`（`tmp/AN-register-qq.txt`）；AK 批次真机状态栏已变「注册成功：请在验证码框输入 6 位验证码完成邮箱验证。」（handoff AK） | ① 用户**尚未亲手用原口令登录** `kizemo@163.com`（服务端已 `email_verified=1`，口令明文不存 ⇒ 无法代登）；② 探针侧「verify→login」需 6 位码回贴后补跑 |
| **AC-02** | 免登：重启系统与服务后仍登录态、续期自动完成 | **不通过** | 服务端已实现 refresh token（`ACCESS_TOKEN_TTL=3600` / `REFRESH_TOKEN_TTL=2592000`，`wrangler.toml`）；`auth.ts:427` 校验 `revoked_at` | 未实测：需在**登录态**下重启输入法服务与系统后观察自动续期 |
| **AC-03** | 双机词库：A 加词 → B 同步 → B 能打出并继续累积词频 | **不通过** | 合并语义结论已落盘 `docs/research/fluxing-cloud-2026-11/userdict-merge-semantics.md`；客户端推拉走 T102 快照通道 | **需两台设备**（本轮单机，条件不具备） |
| **AC-04** | 双机短语：B 的短语界面可见同一条 | **不通过** | 同上 | **需两台设备** |
| **AC-05** | 双机配置：A 改快捷键 → B 生效 | **不通过** | 配置项已纳入推拉集合（AF 批次） | **需两台设备** |
| **AC-06** | 断网降级：断网期间输入正常、无重试风暴、恢复后自动补同步 | **不通过** | 代码层已有：退避第 4 档 60 分钟（AF 批次修）、维护态 `StartMaintenance`/`EndMaintenance` 包裹、节流 5 分钟 + 全局 15 分钟 | 未实测：需真机断网（按 exe 路径防火墙出站阻断）+ 恢复后观察补同步；**需用户在场** |
| **AC-07** | 服务端存储与传输抓包均无明文词条 / 短语 / 配置 | **✅ 通过**（存储侧线上实测 + 传输侧代码级） | **R2 实测密文**：75 B 对象四项判据全过（长度==`cipher_len` / `cjk=0` / `printable ratio=0.3067` / ASCII 连续串 0 段）· **D1 实测无明文**：`item_key` 全部 64 位小写 hex、非 hex **0 行**、无明文残留行 · **线上条目身份** `id=cd3cc4e9af…` 长度 64 且 ≠ 明文键 · 证据 `tmp/AO-t603-r2-check.txt`、`tmp/AO-probe-closure-5.log` | **批次 AO 已修（方案 K，ADR 0059）**：线上身份改送 HMAC 后 64 位 hex，明文键搬进 AEAD 内长度前缀封包 ⇒ D1 与传输侧同时不再有明文。**真实客户端 HTTPS 抓包未做**（补强项，不改结论） |
| **AC-08** | 并发编辑不同条目 → 两端并集且一致 | **不通过** | 服务端 HLC 守卫 + `skipped` 语义已上线；客户端**本轮修好**消费 `skipped`（AH-4 ⑤，见下） | **需两台设备** |
| **AC-09** | 并发改同一条目 → 最终同版本，不出现永久不一致 | **不通过** | 同上 | **需两台设备** |
| **AC-10** | 登出后本机/云端仍在；「清空云端数据」后服务端无可恢复内容 | **不通过** | 服务端 `src/account.ts` 注释与实现：清空只清同步内容，**不删账号**（users / devices / refresh_tokens / email_verifications 保留） | 未实测：需真机点「清空云端数据」后回查 D1/R2 |
| **AC-11** | 三数字未破线；输入延迟无可感知变化 | **不通过（内存归因已闭环 ⇒ 非云同步回归；输入延迟仍未测）** | 进程数 **1** ✅ · **归因 A/B（批次 AP，`tmp/AP-ctrl4.txt`）**：同机 / 同用户数据 / 同脚本 / 同窗口下 A 含云 **24.8 MB** vs B pre-cloud **26.3 MB** ⇒ **云模块常驻开销 ≈ 0 MB**；空闲 CPU 同窗口 A **0.1192 %** vs B **0.1786 %**（56.5 s 窗口 B 0.1106 % vs A 0.1192 %）⇒ CPU 亦**不归因云模块**；对照件 = 装机目录 `WeaselServer.exe.bakL`，经 6 个云标记（`/v1/sync/push`、`/v1/auth/login`、`CloudSyncEngine`、`fluxing-itemkey-mac-v1`、`CloudAccountDialog`、`/v1/sync/pull`）判别**全部 False**，`tmp/AP-logcheck.txt` 复核 B 组四方案 `dictionary is ready`、**0 次** `missing input schema` ⇒ 对照有效；口径裁定见 ADR 0060 | ① 138.5 MB 与 ADR 0030 的 40 MB 属**口径错配**（40 MB = 无 TSF 会话稳态 ≈ 25 MB；138.5 MB = **TSF 会话 + 方案 / 词库加载**态，`tmp/AP-session-hist.txt`：AN 采样窗口 48 次 `focus_in`，对照测量期间 0 次）⇒ **不是云同步回归**；② **带 TSF 会话**的 A/B 差值仍未实测（自动化无法建立 TSF 会话）；③ 输入延迟两组对照未测（需登录态 + 用户在场） |
| **AC-12** | 未登录全流程出向请求数 = 0 | **✅ 通过** | `compliance-check.md` §1：未登录态下 `Get-NetTCPConnection -State Established` 对 WeaselServer PID 257348 = **0 条**；全局到 Cloudflare 网段 = **0 条** | — |
| **AC-13** | 新设备登录后同步恢复词库 / 短语 / 配置 | **不通过** | — | **需第二台设备**登录同一账号后拉取（并依赖 AC-01 登录态闭环） |
| **AC-14** | 移除设备后该设备续期请求被拒绝 | **不通过** | 服务端**已实现**：`src/devices.ts` 提供 `GET /v1/devices` / `DELETE /v1/devices/:id`；`src/auth.ts:427` 续期时校验 `revoked_at` | 未实测：需两台设备，一台移除另一台后用旧 refresh token 续期 |

## 2. 汇总

| 结论 | 条数 | AC |
|---|---|---|
| ✅ 通过 | **2** | AC-07、AC-12 |
| ❌ 不通过（含「条件不具备」） | **12** | AC-01 ~ AC-06、AC-08 ~ AC-11、AC-13、AC-14 |

**结构性阻塞（批次 AP 更新）**：14 条里 **7 条**（AC-03/04/05/08/09/13/14）需要**第二台设备**，
AC-01/02/06/10 需要**登录态 + 用户在场**；这两类合计 11 条 ⇒ **单机 + 无人值守无法收敛到全绿**。
纯机端可推进的两条已收口其一：**AC-07 于批次 AO 由不通过转通过**（方案 K：`item_key` 改送不透明 MAC 身份，见
[ADR 0059](../../adr/0059-item-key-opaque-mac-identity.md)）；**AC-11 的内存归因于批次 AP 闭环**（单变量 A/B ⇒
云模块 ≈ 0 MB，138.5 MB 属会话态、与 40 MB 基线口径错配，**非云同步回归**），AC-11 仍记「不通过」的**唯一**剩余缺口是
**输入延迟两组对照**（需登录态 + 用户在场）。

## 2.1 批次 AQ 追加（2026-11-12）：含云构建「启动约 30.2 s 自退」已闭环

| 项 | 结论 | 证据 |
|---|---|---|
| 缺陷性质 | **不是优雅退出，是访问冲突崩溃 `0xC0000005`**（修复前必崩：30.6–30.9 s；minidump `fluxing-20261112-195402-c0000005.dmp`） | `tmp/AQ-trace-probe.txt`、`%LOCALAPPDATA%\fluxing\crash` |
| 归因 | **只由含云二进制产生**，与 PPL / 注册表 / 安装目录 / 工作区副本无关 | 探针 A/B：含云 30.2 s 崩 vs pre-cloud 存活 ≥ 70 s |
| 根因（源码 + trace 双证） | 云同步 `BeginMaintenance` → `StartMaintenance()` → `Finalize()` → `rime_api->finalize()` → `Registry::Clear()` 销毁 `userdb` ⇒ 紧随的 levers `user_dict_iterator_init` → `UserDb::Require("userdb")` 返回 nullptr → 空指针解引用；`RimeFindModule("levers")` 非空是因为 `UnloadModules` 只清 `loaded_`、`modules_` 保留 ⇒ 拿到已卸载模块空壳 ⇒ 调用点判空挡不住 | [ADR 0061](../../adr/0061-rime-lifecycle-paired-maintenance-and-seh-guards.md) |
| 修复 | ① `BeginMaintenance` = 命名信号量锁 → `StartMaintenance()` → **立即 `EndMaintenance()`**（恢复 rime 可用态）再导出/回写；② `EndMaintenance` 只释放锁；③ 5 个 **POD-only SEH 护栏**覆盖全部 levers 调用（访问冲突 ⇒ 本轮失败 + 中文错误，进程永不崩）；④ 新增 `include/FluxingRimeLifecycle.h`（Win32 命名信号量 `Local\Fluxing.RimeLifecycle`）做跨模块唯一互斥 | `WeaselServer/Cloud/CloudRimeUserDictBackend.cpp`、`include/FluxingRimeLifecycle.h` |
| 验证 | 修复后进程**存活 ≥ 60 s 未退出**（trace 诊断版 + 去 trace 发布件各一轮）；trace 走完整链路 `begin_maintenance_enter → begin_maintenance_done ok=1 → export_enter → export_levers acquired=1 → export_iter_init_ok → export_dicts n=1 → export_backup rime_ice ok=1 → export_done exported=1 → end_maintenance_enter → end_maintenance_done ok=1` ⇒ **备份路径首次真实跑通**；**修复件运行期间无新增 dump**（第 4 枚 dump 19:57:35 经 minidump 内嵌模块路径判定 = `D:\Program Files\fluxing\weasel\WeaselServer.exe` = **旧装机版 1.2.1**，拉起→崩溃间隔 ≈ 30 s ⇒ **同场次自然 A/B：修复件 ≥ 60 s 存活 / 旧装机版 30 s 必崩**） | `tmp/AQ-trace-probe.txt` |
| 门禁 | 云单测 **`PASS=453 FAIL=0`**（与 AO/AP 基线逐字一致）；`build.bat weasel` **0 警告 0 错误**；makensis 8 条 `warning 7010`（既有可选产物告警） | `tmp/AQ-cloudtest.log`、`tmp/AQ-build3.log`、`tmp/AQ-installer.log` |
| 发版 | **1.3.0 作废**（含崩溃缺陷；重出同名同版本 = L107 发布事故）⇒ 重新出包 **1.3.1**：`output/archives/fluxing-1.3.1-installer.exe` = 42 963 252 B · sha16 `71286E7671052F37` · 内嵌 `WeaselServer.exe` 2 700 800 B / FileVersion 与 ProductVersion 均 `1.3.1.0` | `tmp/AQ-installer.log` |
| 残留缺陷（未修） | 每轮同步仍 finalize + Initialize rime ⇒ **当前 TSF 会话被清空**（用户进行中的输入可能被中断）；彻底解法需「复制 userdb 目录后另开 leveldb」或「独立进程执行备份」（需另立 ADR）。**restore（回写）路径未在真实云数据上端到端验证**（需第二台设备产生云端版本更新的拉取场景）；代码侧已由 `SafeRestoreUserDict` 护栏覆盖 | ADR 0061「未闭环」段 |

⚠️ **装机前置（必须）**：`WeaselServer.exe` 是 **PPL 签名进程** ⇒ 装机 1.3.1 前必须**注销 / 重启系统**，否则仍在跑旧映像（磁盘哈希不能证明在跑的是新码）。
当前装机版仍为 **1.2.1.0**（`D:\Program Files\fluxing\weasel\WeaselServer.exe`，sha16 `5891D9656792A129`，含云）⇒ **每启动约 30 s 崩溃一次**。

## 2.2 批次 AR 追加（2026-11-14）：残留缺陷「每轮同步清空当前 TSF 会话」已立 ADR 0062（**实现未做**）

| 项 | 结论 | 证据 |
|---|---|---|
| 缺陷性质 | **不是「组合串丢失」，是「按键被吞直到重新聚焦」**：`Finalize()` 清 `m_session_status_map`（`RimeWithWeasel.cpp:258-264`）⇒ `to_session_id()` 用 `operator[]` 返回 **session_id = 0**（`:234-236`）⇒ `ProcessKeyEvent()` 把 0 传给 `process_key`（`:482-492`）⇒ 客户端 `ipc_id` 成**孤儿**（`StartSession` 只在连接/激活时发一次、`WeaselClientImpl.cpp:196-201`；只有 `_Reconnect()` 才重建，`WeaselTSF.cpp:501-504`）⇒ 该窗口**哑到重新聚焦为止** | `docs/adr/0062-cloud-sync-maintenance-gate-and-session-preservation.md`「事实 1」 |
| 根因 | librime 硬约束：`OpenReadOnly()` 的 `readonly` **只影响 `create_if_missing`**（`level_db.cc:54-58`）⇒ 仍吃 LOCK；`UserDictManager::Backup` 自建 leveldb 实例（`user_dict_manager.cc:51-71`）⇒ 必须先 finalize ⇒ **「备份成功」与「会话存活」互斥** | 同上「事实 2」 |
| 决策 | **[ADR 0062](docs/adr/0062-cloud-sync-maintenance-gate-and-session-preservation.md)（Proposed）**：① 维护准入闸门（会话表中无 `session_id != 0` 才放行；只读 / 无 I/O / 不加锁）；② **跳过 ≠ 失败**（复用已有 `SyncOutcome::kSkipped`，不递增 `consecutive_failures_`、不进 60 min 退避，改 **5 min 短间隔重试**）；③ 手动「立即同步」**不绕过**闸门（中文提示 + 指引「请先切换到其他窗口」）；④ 驳回三替代方案（复制 userdb 另开 leveldb 无一致性保证 / 独立进程备份 / 改 submodule）；⑤ **口径变更为「机会性同步」**（用户可见）；⑥ 不新增 IPC 消息 ⇒ 不触发 IPC 四边同步红线 | ADR 0062 |
| 状态 | **决策已落盘，实现未做** ⇒ 本批**未改任何产品代码、未重跑构建与单测**（AQ 基线 `PASS=453 FAIL=0` / `build.bat weasel` 0 警告 0 错误 仍有效）；实现硬前置 = 先单独提交或 stash `RimeWithWeasel/RimeWithWeasel.cpp` 的 spec 045 脏改动（+118 行，未提交）；ADR 现为 **Proposed**，待用户确认「机会性同步」用户可见口径后转 Accepted | 本批 handoff §3 / §4 |
| 残留缺陷（未修） | 「每轮同步清空当前 TSF 会话」**仍未消除** —— 修复路径已定（闸门），但**代码未落地** ⇒ 运行时行为与 1.3.1 一致 | 同上 |

## 3. 证据索引

| 证据 | 路径 |
|---|---|
| 线上注册 200 + `account_id` | `tmp/AN-register-qq.txt` |
| Resend key 权限边界（列表 API 401 `restricted_api_key`） | `tmp/AN-resend-check.txt` |
| 云单测门禁（基线 PASS=446 FAIL=0） | `tmp/AN-finish-ah4.txt` |
| AH-4 ⑤ 有牙验证（`TEETH_VERDICT=PASS`） | `tmp/AN-finish-ah4.txt`、`tmp/AN-mutate-ah4.txt` |
| 生产构建（0 警告 0 错误） | `tmp/AN-build-weasel.txt` |
| 线上探针闭环（23 断言 / 失败 0 / `VERDICT: ALL PASS`） | `tmp/AO-probe-closure-5.log` |
| T603 R2 对象内容抽查（75B 密文，5 断言全 PASS / `T603_VERDICT=PASS`） | `tmp/AO-t603-r2-check.txt`、`tmp/AO-t603-r2-check.mjs` |
| AC-07 客户端侧断言（`id` 64 位 hex 且 ≠ 明文键） | `tmp/AO-probe-closure-5.log` |
| 云单测门禁（基线 446 ⇒ **PASS=453 FAIL=0**，+7 条 AC-07 断言） | `tmp/AO-ut-run.log`、`tmp/AO-ut-build.log` |
| 生产构建（0 警告 0 错误） | `tmp/AO-build-weasel.log` |
| 验证码自助还原（按 `AUTH_PEPPER` 重算 `code_hash`，退出码 0 = FOUND） | `tmp/AO-pepper-verify.log`、`tmp/AO-pepper-verify.mjs` |
| T603 R2 抽查（**历史**：线上 `sync_items` 为空 ⇒ `ROWS=0`，批次 AN） | `tmp/AN-t603-r2-check.txt` |
| T602 三数字实测 | `docs/research/fluxing-cloud-2026-11/perf-baseline.md` |
| 合规核查全文 | `docs/research/fluxing-cloud-2026-11/compliance-check.md` |
| 历史批次证据（AH 的 19/19 断言、AK 的四段判据、AM 的发信上线） | `docs/archive/handoffs/handoff-2026-11-11-batch-*.md` |
| **AC-11 归因 A/B 实测**（A 含云 vs B pre-cloud，峰值 PrivMB / 单核 CPU / ΔCPU 节拍） | `tmp/AP-ctrl4.txt` |
| **对照件甄别**（云标记 6 项全 False = 真 pre-cloud；1.2.1 归档件证伪） | `tmp/AP-ctrl4.txt`、`tmp/AP-ctrl3.txt`、`tmp/AP-probe2.txt` |
| **对照组有效性**（四方案 `dictionary is ready` / 0 次 `missing input schema`） | `tmp/AP-logcheck.txt` |
| **会话证据**（AN 窗口 48 次 `focus_in`；对照期 0 次） | `tmp/AP-session-hist.txt` |
| **T604 出包产物**（1.3.0 installer 大小 / sha16 / 内嵌 WeaselServer 版本与云标记） | `tmp/AP-verify.txt` |
| **门禁全量输出** | `tmp/AP-gate.txt` |
| **ADR 0060 口径裁定**（占单核 / 窗口 ≥ 16 s / 归因必须 A/B 对照） | `docs/adr/0060-idle-cpu-single-core-window-and-attribution-method.md` |
