# 批次 BE · `/verify-client-pid` 端到端 PASS（§7 第 1–4 项闭环）+ 配方 `-2` 根因改正

> **本批性质** = 取证 + 文档批次（**产品代码零改动**：`git diff --stat` 仅 `docs/release/restart-checklist-1.3.6.md` + 本 handoff + 接力 prompt + `reports/fluxing-be/` 证据；未改 `librime/`、未改 IPC 协议消息类型、未新增依赖、未出包、未装机）
> **上一批** = [批次 BD · B-5 旧客户端端到端 `PASS_E2E` + B-4 路径 2 真 TSF 宿主签名 + R7 `client_pid` 解析同源](handoff-2026-11-21-batch-bd-b5-e2e-b4-signature-and-r7.md)
> **开工指令** = 「请按 AGENTS.md §9 接力 批次 BD §7 的有序计划继续推进（下一批暂名 BE）… B-4 已由用户 2026-11-21 拍板按宿主退出口径闭环，不要重跑」
> **装机版仍 1.3.8.0**，收工时运行中 `WeaselServer` 唯一 = **pid 1025692 / start 2026-11-21T13:38:10.6 / ver 1.3.8.0**

---

## 1. 一句话结论

**BD §7 第 1 项 push、第 3 项 `/verify-client-pid`、第 4 项「`record_client_pid` 走生产解析器」由推断升实测 —— 三项闭环。** 其中第 3 项**首跑判 `-2` 不是环境竞态，而是 restart-checklist §5 配方缺步骤**（`/verify-client-pid` 是纯客户端模式，管道上必须先有一个服务端应答 `Echo`），已按源码坐实根因、把配方改正，改正后**一次通过**。

---

## 2. §7 前 4 项逐条对账

| # | 项 | BD 原状态 | BE 结果 |
|---|---|---|---|
| 1 | push（`af0195b` + `fbf643f` + docs commit） | 待推 | ✅ **已推**：`78416a8..9f013ca feat/100-cloud-sync -> feat/100-cloud-sync`，`PUSH_EXIT=0`，`rev-list --left-right --count` = `0 0` |
| 2 | B-4 触发源口径 | 待拍板 | ✅ **BD 内已闭环**（用户 2026-11-21 选「按宿主退出口径闭环」）⇒ **本批未重跑** |
| 3 | `/verify-client-pid` | 待用户在场（环境受限） | ✅ **实测 PASS**（配方改正后） |
| 4 | 「`record_client_pid` 走生产解析器」覆盖面 | 仅编译 + 单元级 19 断言 | ✅ **升为端到端实测**（随第 3 项闭环） |
| 5 | A-4 / A-5 未登录态 | 需登出授权 | ⏸ 已获授权，**未开始**（见 §6 下一批第 1 项） |

---

## 3. `/verify-client-pid` 实测数据（**已验证**）

### 3.1 首跑 `-2` 与归因

- **现象**（`reports/fluxing-be/verify-client-pid-raw.txt`）= `/quit` 停服 → 等 2 s → 直接跑 `/verify-client-pid` ⇒ `EXIT=-2` + `[CLIENTPID] connect failed`，`REC_EXISTS=False`。
- **根因（读源码坐实，不是推断）** = `/verify-client-pid` 是**纯客户端模式**：`verify_client_pid_main()`（`test/TestWeaselIPC/TestWeaselIPC.cpp:584-625`）自己只做 `Client::Connect()` → `Echo()` → 读 `client_pid.txt`；而 `client_pid.txt` 是**服务端侧**写的 —— `TestRequestHandler::AddSession()`（同文件 `:768-774`）调 `record_client_pid()`（`:554-575`）解析请求体并落盘到 **exe 同目录**（`GetModuleFileNameW`，`:530-540`）。那个服务端就是同一个 exe 的 `/start` 模式 `server_main()`（`:876-893`，阻塞在 `server.Run()`）。restart-checklist §5 原配方**只写了「`/quit` → 跑探针 → 拉起」，从没要求起 mock** ⇒ 管道上没有任何服务端应答 ⇒ `Connect()` 成功但 `Echo()` 无会话 ⇒ 返回 `-2`。**配方缺步骤，属真缺陷。**
- **顺带否证一条 BD 推断**：BD 登记的阻塞前提「真实 `WeaselServer` 在跑时 mock 抢不到管道 ⇒ 假红」中，「宿主自动拉起真服务端抢管道」这一半**未发生** —— 本批全程窗口内测得 `SRV_DURING_MOCK=0`，mock 独占管道并正常应答。⇒ 需要 `/quit` 的理由是「真服务端占着管道」，**不是**「宿主会拉起抢管道」。

### 3.2 改正后的完整配方（**已实测一次通过**）

```powershell
$repo='F:\soft\00selfmade\rime_claude'; $exe="$repo\Release\TestWeaselIPC.exe"   # ① 仓库根 Release\
$srv='D:\Program Files\fluxing\weasel\WeaselServer.exe'; $rec="$repo\Release\client_pid.txt"
Remove-Item $rec -ErrorAction SilentlyContinue
& $srv /quit 2>&1 | Out-Null                                    # ② 用户授权停服窗口
while((@(Get-Process WeaselServer -EA SilentlyContinue).Count) -gt 0){Start-Sleep -Milliseconds 200}  # ③ 轮询退出
$mock = Start-Process $exe -ArgumentList '/start' -PassThru -NoNewWindow `
        -RedirectStandardError <err> -RedirectStandardOutput <out>   # ④ 起 mock 服务端
Start-Sleep -Milliseconds 1500
(@($mock -ne $null), @(Get-Process WeaselServer -EA SilentlyContinue).Count)  # MOCK_ALIVE / SRV_DURING_MOCK
$p = Start-Process $exe -ArgumentList '/verify-client-pid' -PassThru -Wait -NoNewWindow `
        -RedirectStandardOutput <out> -RedirectStandardError <err>            # ⑤ 探针
"PROBE_EXIT=$($p.ExitCode)"
& $exe /stop; if($null -ne (Get-Process -Id $mock.Id -EA SilentlyContinue)){ Stop-Process -Id $mock.Id -Force }  # ⑥ 关 mock
Start-Process $srv                                              # ⑦ 重新拉起服务端
```

### 3.3 实测输出（`reports/fluxing-be/verify-client-pid-run2.txt`）

```
MOCK_ALIVE=True / MOCK_START=2026-11-21T13:38:05.9038550+08:00 / SRV_DURING_MOCK=0
PROBE_EXIT=0
[CLIENTPID] reported=1026796 self=1026796
[CLIENTPID] PASS: the request body carries session.client_pid= and the value is this process
REC_AFTER_LINES=1
REC_TAIL=1026796
MOCK_ALIVE_AFTER_STOP=False
```

- 整窗时间线：`13:38:03.9 SRV_LEFT` → `13:38:05.9 mock 起` → `13:38:07.4 PROBE_EXIT=0` → `13:38:12.3 SRV_PID=1025692 ver=1.3.8.0`（停服窗口 8.4 s）；`TESTWEASELIPC_LEFT=0`、`FINAL srv=1025692`。
- **判别力**：`record_client_pid()` 对「键缺失 / 值畸形」一律记 **0**，本探针自我 pid 为 1026796 ⇒ `reported=1026796 ≠ 0` 只能是「**生产解析器 `fluxing::session_reclaim::ParseClientPid` 真在请求体上命中**」。若只是 `atoi` 默认值或旧测试自填，`reported` 会是 0 或与 `self` 不等。

### 3.4 退出码语义（读码取齐，`test/TestWeaselIPC/TestWeaselIPC.cpp:584-625`）

| 码 | 含义 | 出处 |
|---|---|---|
| `-2` | `Client::Connect()` 失败 = 管道上无服务端（未 `/quit` 后起 mock、或真服务端没停） | `:587-591` |
| `-3` | `Echo()` 未进会话 | `:600-604` |
| `-4` | mock 从未记录 pid ⇒ 大概率真服务端抢了管道 | `:609-614` |
| `-5` | 记录的 pid 与本进程不符 | `:619-623` |
| `0` | 通道成立 | `:624` |

---

## 4. 工具坑（本批新增，**⚠️**）

1. **⚠️ 本批最大的方法论收获 —— `write` 工具会假成功**：本文件第一次写入时 `write` 返回 `Successfully wrote 9033 bytes to F:/soft/.../handoff-2026-11-21-batch-be-....md`，但随后 `git status` 里**该文件完全不出现**，`Test-Path` 绝对路径 = **`False`** —— 文件从未落盘。⇒ **规则升级**：任何「产出正本文件」的动作，写完必须**同轮 `Test-Path` + `Get-Item.Length` 回读**，写进 handoff 里「已提交/已落盘」的句子必须以回读为准，不得以工具返回文案为准。（与既有记忆「`commit_exit=0` 不算提交判据」同形：**工具自述不是证据**。）
2. **⚠️ 探针 exe 路径**：`TestWeaselIPC.exe` 在**仓库根 `Release\`**（checklist 原写 `test\TestWeaselIPC\x64\Release\` 是错的，已改正）。与 BD 第 6 条同源，本批再次踩到。
3. **⚠️ push 通道方向会反转**：BD 登记「直连可用、代理 `127.0.0.1:7897` 断流」⇒ 本批实测**恰好相反**（直连 timeout，走 git 默认代理配置 `7897` 一次成功）。⇒ **规则**：push 前先各发一个 `Invoke-WebRequest -Method Head https://github.com`（一次带 `-Proxy` 一次不带）再选通道，**不要照抄任一批次的旧配方**；两条都不通才回来找用户确认代理状态。
4. **⚠️ 服务真实安装目录**是 `D:\Program Files\fluxing\weasel\WeaselServer.exe`（不是 `D:\Program Files\fluxing\WeaselServer.exe`）。
5. **⚠️ IPC 管道名全局固定** = `\\.\pipe\<用户名>\WeaselNamedPipe`（`include/WeaselNamedPipe.h:8`），**没有环境变量可改写**（`GetPipeName()` 只读 `GetUserNameW`）⇒ 「换管道名并行跑 mock」这条路不存在，只能抢停服窗口。
6. **⚠️ `Start-Process -PassThru -Wait` 的 `$p.ExitCode`** 实测可靠（本批直接拿到 `PROBE_EXIT=0`）；但 mock 是长驻进程，**不能带 `-Wait`**（会挂死），用 `-PassThru` 不等待 + 事后 `/stop`。

---

## 5. 状态：已验证 / 推断 / 未验证

- **已验证**：① push 完成且 `ahead=behind=0` ② `/verify-client-pid` 端到端 `PASS`（整窗日志 `verify-client-pid-run2.txt`）③ `reported=self=1026796` 且记录文件仅一行、无 `0` 残留 ④ 停服窗口内无真服务复活（`SRV_DURING_MOCK=0`）⑤ mock 已清理（`MOCK_ALIVE_AFTER_STOP=False` / `TESTWEASELIPC_LEFT=0`）⑥ 服务面已复原（唯一 pid 1025692 / ver **1.3.8.0**）⑦ `-2` 根因有源码行号支撑（`:584-625` / `:768-774` / `:530-540` / `:876-893`）。
- **推断**：无（本批结论全部有实测或行号支撑）。
- **未验证**：见 §6；`-3 / -4 / -5` 三条退出码分支**只读码未触发**（本批只走到 `0` 与 `-2`）。

---

## 6. 未闭环 → 下一批（BF）有序计划

1. **§7 第 5 项 A-4 / A-5 未登录态**（判据原文 [`restart-checklist-1.3.6.md`](../../release/restart-checklist-1.3.6.md) §1 第 32–33 行）：A-4 = 未登录态账号页**不出现**换钥提示行；A-5 = 底部留白正常不空一大块。**用户已授权登出**（原话「授权：届时登出验证后重新登录」）。配方要点：打开账号窗 = 向**服务自身主窗**发 `WM_COMMAND(40024)`（发托盘窗无效，AX 已实测）；控件定位按 `GetDlgCtrlID` 数字 id 而非文本（BD/BA 口径）；探针须先过默认桌面守卫；**口令不落档**，取证值当场回读文件带校验位。
2. **双机 AC 7 条 + restore 端到端 + 输入延迟两组对照 + 缺陷 A 反向判据 G-3/G-5**：需两台机器 + 用户在场。
3. **`-3 / -4 / -5` 退出码分支**：可选加固，成本低（各自构造一次即可），优先级低于前两项。
4. 本批文档改动提交 + 推送。

---

## 7. 交付物清单

- [`docs/release/restart-checklist-1.3.6.md`](../../release/restart-checklist-1.3.6.md) §5 —— **就地改正配方**（补 `/start` mock 与 `/stop`、轮询服务退出、exe 路径、退出码语义表、判别性口径）+ BE 状态注（§5 第 77 行）。
- 本 handoff + [`prompt-next-session-2026-11-21-batch-be-verify-client-pid-pass.md`](../prompts/prompt-next-session-2026-11-21-batch-be-verify-client-pid-pass.md)。
- 证据：[`reports/fluxing-be/verify-client-pid-run2.txt`](../../../reports/fluxing-be/verify-client-pid-run2.txt)（PASS 整窗）· [`verify-client-pid-raw.txt`](../../../reports/fluxing-be/verify-client-pid-raw.txt)（首跑 `-2`）· `mock2-{stdout,stderr}.txt` / `probe2-{stdout,stderr}.txt`。
- **产品代码零改动**；未出包、未装机、未重启、未改注册表持久项。

---

## 8. 正本更新（CLAUDE.md 指针）

在 [`CLAUDE.md`](../../../../CLAUDE.md) 「本批交接指针」加 BE 一条、BD 降级为「上一批」，要点：`/verify-client-pid` **已实测 PASS**（`PROBE_EXIT=0`，`reported=self=1026796`）⇒ BD 登记的「环境受限未跑」覆盖面修正已解除、§7 第 1–4 项闭环；**⚠️ 工具坑** = ① `write` 会假成功（报 9033 字节已写、`Test-Path=False`）⇒ 产出正本必须同轮回读 ② 探针 exe 在**仓库根 `Release\`** ③ push 代理/直连可用性**会随网络状态反转**，动手前两条都探一次再选。
