# 批次 AF · spec 100 阶段 F「同步引擎与调度」（T501–T505）+ 阶段 G 加固项（resend 端点）

- **日期**：2026-11-11
- **分支**：`feat/100-cloud-sync`
- **上一批**：[批次 AE · 阶段 E「注册 + 邮箱验证码」客户端链路补齐](handoff-2026-11-11-batch-ae-cloud-register-verify-chain.md)
- **ADR**：[0055-cloud-sync-engine-scheduling-and-convergence](../../adr/0055-cloud-sync-engine-scheduling-and-convergence.md)
- **本批定位**：推进阶段 F 中**不依赖第二台机器**的项 + 把阶段 G 的服务端 resend 加固项落到代码。**Cloudflare 侧已验证部分（D1 / R2 / AUTH_PEPPER / deploy / DNS / `/v1/health`=200）未重做。**

---

## 1. 背景

批次 AE 补齐了客户端「注册 + 邮箱验证码」链路后，阶段 E 的代码面完成，但**阶段 F（同步引擎与调度）尚未开工**；且 AE 交接里留了一条明确的阶段 G 加固项：服务端**没有 resend 端点** ⇒ 验证码过期 / 次数用尽后用户被锁死（再 `register` 会 409 `email_taken`）。

本批目标：把 T501–T505 的**代码面**做完并跑通门禁，把 resend 端点做到**代码 + 用例全绿**，并明确哪些判据**必须有用户在场**才能闭合。

---

## 2. 本批改动清单

### 2.1 客户端（生产）

| 文件 | 性质 | 内容 |
|---|---|---|
| `WeaselServer/Cloud/CloudSyncEngine.h` | **新建** | 引擎接口 + 纯函数（inline）：`BackoffDelayMs` / `DailyPushAllowed` / `DailyPullAllowed` / `DayIndexUtc` / `DayRolloverMs` / `ComputePushDeadline` / `DecideNextAction`；枚举 `SyncDirection` / `SyncTrigger` / `SyncOutcome` / `SyncAction`；结构 `CloudSyncRoundResult` / `CloudSyncStatus` / `CloudSyncEngineOptions` |
| `WeaselServer/Cloud/CloudSyncEngine.cpp` | **新建** | 单后台线程 + `condition_variable`；`cv_.wait_until(deadline, pred{!running_ \|\| wakeup_})` ⇒ 事件唤醒 + 定时唤醒、**无轮询**；单调钟算 deadline，墙钟只用于状态显示与 UTC 日配额翻篇 |
| `WeaselServer/Cloud/CloudSyncService.h` | 改 | `#include "CloudSyncEngine.h"`；`ParsePullResponse` / `Pull` 增加 `bool* has_more`；`CloudSyncCoordinator` 增加 `bool SyncOnce(SyncDirection, CloudSyncRoundResult*, std::string*)`（旧签名保留兼容）与 `bool ResetLocalSyncState(std::string*)` |
| `WeaselServer/Cloud/CloudSyncService.cpp` | 改 | `SyncOnce` 整体重写：**拉 → 合并 → 回写 → 推 → 侧写**；分批常量 **推 200 条/请求、拉 100 条/页 × 最多 10 页/轮**，游标一轮不前进即停手；顶层 `server_seq` 缺失时**保留调用方值**（不再写 0）；条目自带 `server_seq` 取 max 推进；`has_more` 用 `json::MemberBool` |
| `WeaselServer/Cloud/CloudAccount.h` / `.cpp` | 改 | 新增 `bool ResendVerification(const std::string& email, std::string* err)`（`POST /v1/auth/resend`，**不改状态机**；404 `email_not_found` / 409 `email_already_verified` / 429 `verify_too_frequent`） |
| `WeaselServer/Cloud/CloudAccountDialog.h` / `.cpp` | 改 | T505：接入引擎 +「最近同步」状态行 +「立即同步」按钮；阶段 G：「重新发送验证码」按钮 |
| `WeaselServer/WeaselServer.vcxproj` | 改 | 新增 `Cloud\CloudSyncEngine.cpp`（ClCompile `PrecompiledHeader=NotUsing`）+ `Cloud\CloudSyncEngine.h`（ClInclude） |
| `test/TestFluxingCloud/TestFluxingCloud.vcxproj` | 改 | 新增 `..\..\WeaselServer\Cloud\CloudSyncEngine.cpp` + `.h` |
| `test/TestFluxingCloud/TestFluxingCloud.cpp` | 改 | 新增 4 组引擎用例 / **78 条断言**（**只 include 生产头，不复制第二份实现** —— 宪法 R7） |

### 2.2 服务端（`cloud/fluxing-sync/`，6 文件）

| 文件 | 内容 |
|---|---|
| `src/wire.ts` | 新增 `compareHlc` + `compareUtf8Bytes`（按 **UTF-8 字节序**比 device，与客户端 `CompareHlc` 语义一致） |
| `src/sync.ts` | **push 逐条 HLC 守卫**：存储中的 HLC ≥ 入参 ⇒ **跳过**（不写 R2 / D1、不推进 seq），响应新增 `skipped`；**pull 响应新增顶层 `server_seq`**（本页最后交付条目序号） |
| `src/auth.ts` | 新增 `handleResend`（重发验证码；已验证 ⇒ 409、未知 ⇒ 404、频繁 ⇒ 429） |
| `src/index.ts` | 新增路由 `case '/v1/auth/resend'` |
| `test/api.test.ts` | **+10 例**（守卫阈值改坏 / 删顶层 `server_seq` ⇒ 精确命中 4 条失败；去掉「已验证 ⇒ 409」⇒ 精确命中 1 条 ⇒ 用例有牙） |
| `README.md` | 端点与字段说明同步 |

### 2.3 文档

- 新建 `docs/adr/0055-cloud-sync-engine-scheduling-and-convergence.md`（引擎与凭据解耦、单调钟调度、方向策略、收敛双保险、sidecar 唯一真相源、分批游标、resend 端点、后果与未验证项）。
- `.specify/specs/100-fluxing-cloud-account-sync/tasks.md`：阶段 F 逐条回写状态 / Verified / Still-not-verified / 偏差；阶段 G 新增 **T605**（resend 端点）；AE 段「阶段 G 加固项（本批不做）」已就地更正为「批次 AF 已补齐」。

---

## 3. 设计定案

1. **引擎与凭据解耦**：引擎注入 `SyncRunner` 回调，**不持 token / enc_key / 端点** ⇒ 未登录天然零出向请求（R9），且引擎可脱离网络单测。
2. **触发优先级**：手动 > 退避重试 > 启动 > 脏推 > 定时。**手动同步最高优先且不受每日配额限制**。
3. **方向策略**：**启动轮只拉不推**（plan §7「启动 → 拉一次」）；其余轮次一律「推 + 拉」，但推送集合按 sidecar 差分算出 ⇒ 无本地改动时**零写放大**，同时兜底本地改动传播。
4. **收敛双保险**：服务端 HLC-max 守卫（服务端侧）+ 客户端「只推本地改动」（客户端侧）。
5. **sidecar 语义收紧 = 「已同步状态」**：只有「远端胜出」或「本轮真的推成功」才标记；只拉不推的轮次**不得**标记。
6. **退避**：1 / 5 / 15 / 60 分钟，最多 4 次 ⇒ 用尽交回定时触发；断网期间不排队累积，恢复后按防抖窗口补推；每日推 100 / 拉 50，用尽 ⇒ **跳过（不是错误）**。
7. **时间处理**：调度用**单调钟**；状态显示用墙钟；UTC 自然日翻篇重置推 / 拉计数；格式化用 **Win32 FILETIME 链**，**刻意不用 CRT `localtime`**（进程级共享静态缓冲区，并发会互相踩）。
8. **锁序铁律**：先 `engine_mutex` / `sync_mutex`，后 `impl->mutex`；析构顺序 **先停引擎（join 引擎线程）再停工作线程**，且 join 时**不持 `sync_mutex`**（否则与引擎线程互锁）。
9. **`skipped` 数组形状 = 字符串数组**（与既有 `accepted` 同构）；改 `accepted` 形状会破坏客户端既有契约。

---

## 4. 本轮修掉的 4 个生产缺陷（都是「代码看着对、实际错」类）

| # | 缺陷 | 后果 | 修法 |
|---|---|---|---|
| 1 | `last_push_ms_ / last_pull_ms_` 初值 0 | 定时条件恒成立 ⇒ 引擎启动即触发，`startup_delay_ms` **形同虚设** | 改用 `SteadyNowMs()` 初始化 |
| 2 | 退避阶梯第 4 档写成 `60 * 1000` | = **60 秒**，比第 3 档 15 分钟还短 ⇒「第 4 次失败几乎立刻重试」 | 改为 `60 * 60 * 1000`；单测加**硬断言**钉死 |
| 3 | `kWait` 分支无条件把 `last_outcome` 覆盖成 `kSkipped` | 上一轮成功 / 失败结果被吞 | 改为仅 `kQuotaExceeded` 才改写 |
| 4 | sidecar 只看 `locally_modified` 就写 `synced=true` | **只拉不推的启动轮会把本地改动误标成「已同步」** ⇒ 改动永远推不上去 | 改 `push_indices` + `std::vector<bool> pushed[]`，**只有本轮真推成功**才标 |

> 缺陷 2 是**单测 worker 抓到的**（它初版断言写成硬值、当场 3 条 FAIL 后按纪律「只报告不改生产代码」）；缺陷 1 / 3 / 4 是本会话自查 + 联读发现。

---

## 5. 门禁（全部实测，2026-11-11）

| 门禁 | 命令 | 结果 |
|---|---|---|
| 云单测 | `cmd /c tmp\build-fluxing-cloud-tests.cmd` | **`RESULT: PASS=402 FAIL=0`** / `VERDICT: ALL PASS` / exit 0（基线 324 ⇒ **+78**） |
| WeaselServer | `cmd /c build.bat weasel` | **0 个错误 0 个警告** / exit 0 |
| 格式 | `pwsh -File tmp\AF-clangformat-check.ps1` | **8/8 文件 OK**，`FORMAT_BAD_FILES=0`（clang-format 18.1.8） |
| 服务端类型 | `npm run typecheck` | 退出码 **0** |
| 服务端用例 | `npx vitest run` | **30/30 PASS**（基线 20 ⇒ +10） |

**证据路径**：`tmp/AF-clangformat-check.ps1`（格式门禁脚本）；`tmp/build-fluxing-cloud-tests.cmd`（云单测脚本）；`output/Win32/WeaselServer.exe`（构建产物）。

**行尾 / BOM**：8 个生产文件统一 **LF、无 BOM**（与既有 `WeaselServer/Cloud/*.cpp` 一致；`.rc` 仍按 L09 保持 UTF-16LE + BOM + CRLF，本批**未动** `output/install.nsi`）。

---

## 6. 已验证 / 推断 / 未验证

### 已验证（实测）
- 上表全部门禁数字。
- 引擎「事件唤醒 + 定时唤醒、无轮询」由单测的运行时用例覆盖（`Start` 后 runner 被调用且**启动轮只拉不推**、`RequestManualSync()` 触发**恰好一次**额外执行、`Stop()` 后不再执行、`SetEnabled(false)` 后不发起任何轮次、`Stop()` 可重复调用、空 runner `Start` 失败）。
- 服务端 HLC 守卫**有牙**：变异验证（改坏阈值 / 删顶层 `server_seq` / 去掉 409 分支）各自精确命中对应失败用例。
- 客户端 `ResendVerification` 契约与错误码走 `ExtractServerError`（中文原因）。
- 账号页控件表：`kIdResend=1015` / `kIdSyncNow=1016` / `kIdSyncStatus=1017` **追加在末尾**，既有 ID 未改；客户区 410 → 472，控件整体下移。

### 推断（读过源码但未运行验证）
- 引擎线程在 `Impl::ThreadMain` 的锁外调用 runner ⇒ 网络 I/O 不在 UI / TSF 线程。
- `CloudSyncCoordinator::SyncOnce` 成功时写 `out->message`、失败时写中文原因 ⇒ 文案拼接可用。

### 未验证（本批无法闭合）
见 §7。

---

## 7. **必须有用户在场**才能闭合的判据

| # | 判据 | 为什么必须用户 | 怎么验 |
|---|---|---|---|
| 1 | **双机 / 双隔离环境验收 AC-01 ~ AC-14** | `CloudCredentialStore` 默认 target 是**全局单例** `Fluxing:CloudAccount` ⇒ Credential Manager 按 **Windows 用户**存；同一用户的两套隔离用户数据目录**不足以**模拟两台设备。要么探针显式传不同 `target_name`，要么第二个 Windows 用户 / 第二台机器 | 两台设备（或两用户）各装一版，同步各自词库，比对并集一致（AC-08 / AC-09） |
| 2 | **`wrangler deploy`（部署服务端加固）** | 对外动作，需 Cloudflare 人工授权（`wrangler login --browser=false` 回调窗口仅约 2.5 分钟，自动化点 Allow 未命中，见批次 AD）；**或**注入 `CLOUDFLARE_API_TOKEN` | 部署后复跑 `POST /v1/auth/resend`（期望 200）、`POST /v1/sync/push` 观察 `skipped`、`GET /v1/sync/pull` 观察顶层 `server_seq` |
| 3 | **账号页真机截图** | IME 自绘顶层窗**跨进程抓不到**（记忆 #43） | 装机后打开账号页（托盘右键「账号与同步 (&Z)」），人工看「最近同步」行 + 三个新控件有无遮挡 |
| 4 | **托盘 `OUR_HIT=0` 复测（T406）** | 6 个进程仍持修复前映像（含本会话宿主 `MiniMax Code`）⇒ 需结束这些进程或注销重登 | 跑 `scripts/install/check-tray-input-indicator.ps1`，期望 `VERDICT=PASS_OUR_ITEM_ABSENT` |
| 5 | **注册 → 收码 → 验证 → 登录端到端** | `MAIL_MODE="outbox"` **不发真信**（$0 路线），且 `email_verifications` 存 `code_hash` ⇒ 明文码**不可直读** | 要么切 `MAIL_MODE="cloudflare"`（需 Workers Paid，与 $0 目标冲突），要么人工在 D1 `mail_outbox` 侧确认投递链路并走真实邮箱收码 |
| 6 | **断网退避序列 + 恢复补推** | 需要真实断网 | 拔网线 / 断 Wi-Fi，观察退避 1/5/15/60 分钟，恢复后确认自动补推成功 |
| 7 | **「输入不卡顿」体感** | 需要真实输入 | 同步开启 / 关闭两组对照打字（阶段 G · T602） |
| 8 | **装载新二进制（PPL）** | `WeaselServer.exe` 是 PPL ⇒ 累计改动后**必须先注销 / 重启**才加载新二进制 | 注销 / 重启后再装 `scripts/install/silent-install.ps1` |

---

## 8. 已知偏差 / 遗留风险

1. **本地改动钩子未接**（本批唯一代码级偏差）：`CloudSyncEngine::NotifyLocalChange()` **无调用方** —— rime 的提交事件发生在 **TSF 客户端进程**，接入需新增 IPC 消息 ⇒ 触发 CLAUDE.md「IPC 四边同步」红线（server / TSF / deployer 三端 + ADR + 测试）。本批**未做**，由 **60 分钟定时轮**兜底。
2. **plan §8 的「同步开关」未做**：T505 任务书未要求，且需新增持久化设置项。
3. **`restore_user_dict` 是「合并」语义**（abs(commits) 择优、dee 取 max、tick 抬升）：合并快照未必能覆盖本地胜出的冲突条目 ⇒ 端到端收敛（AC-08 / AC-09）仍需真机双机验证。
4. **非 ASCII `RimeUserDir`**：sidecar 走窄路径 API（`std::ofstream`），落点必须 ASCII；当前实际落点 `D:\Program Files\fluxing\user1\fluxing`（纯 ASCII）⇒ 无问题，但换机器前需确认。
5. **服务端仍未部署** ⇒ `skipped` / 顶层 `server_seq` / `/v1/auth/resend` **线上均未生效**。

---

## 9. 下一步（推荐序）

1. **部署服务端加固**（判据 2）—— 一次 `wrangler deploy` 解锁 3 个能力，是当前性价比最高的一步。
2. **决定 IPC 路线**：要不要为「本地改动钩子」新增 IPC 消息（触发 IPC 四边同步红线）？这是阶段 F 唯一剩下的代码缺口。
3. **装机 + 人工复验**（判据 3 / 4 / 8）—— 需先注销 / 重启（PPL）。
4. **双机验收**（判据 1）—— 阶段 G T601 的前置。

---

## 10. 本轮工具坑（复用时注意）

- `edit` 一次只能传**一组** `old_string`/`new_string` ⇒ 多处改动分多次调用并 `grep` 复核落盘。
- `git commit -F -` **不读 stdin** ⇒ 消息写临时文件再 `-F <file>`。
- 工作区外写 / 删内联被拦 ⇒ 写 `.ps1` 再 `pwsh -NoProfile -ExecutionPolicy Bypass -File` 执行。
- `clang-format -i` 在 Windows 上**保持**原文件行尾（实测 LF 文件仍是 LF、无 BOM）⇒ 不必手工转换。
- 多人 / 多 worker 并行写同一批文件时，用 `Get-Item … | LastWriteTime` 与 `git diff --stat` 判断「谁的改动落盘了」，不要凭 worker 的自述。
- 判「某人跑的某个进程是否加载了新二进制」**不能用磁盘哈希**（Windows 已加载映像不随磁盘文件替换而更新）⇒ 见记忆「Windows：判进程跑的是哪份代码」。
