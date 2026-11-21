# 重启后测试清单 · 1.3.6（批次 AY）

> 装机前置：`WeaselServer.exe` 是 **PPL**（Protected Process Light）⇒ **必须注销或重启**再装，
> 否则仍在跑旧映像；`taskkill` 被禁，停服只能 `WeaselServer.exe /quit`。
> **磁盘哈希不能证明「在跑的是新码」**⇒ 每条判据都写明怎么确认。
> 本清单只列 1.3.6 新增项；**1.3.5 遗留项（双机 AC 7 条 / restore 端到端 / 输入延迟两组对照 /
> 闸门正反判据）见 [`restart-checklist-1.3.5.md`](restart-checklist-1.3.5.md)**，不在此复述。

## 0. 装对了没有

```powershell
(Get-Item 'D:\Program Files\fluxing\weasel\WeaselServer.exe').VersionInfo.FileVersion   # 期望 1.3.6.0
```

服务进程映像版本 = 验收口径（不是安装时刻）：

```powershell
Get-Process WeaselServer | Select-Object Id,StartTime,@{n='ver';e={$_.MainModule.FileName}}
(Get-Process WeaselServer).MainModule.FileVersionInfo.FileVersion   # 期望 1.3.6.0
```

## 1. 账号页「换钥后果」常驻提示（本批新增，AX §6.8 用户拍板项）

打开方式 = 托盘右键 →「账号与同步」（脚本等价 = 向**服务自身主窗**发 `WM_COMMAND(40024)`；
**发给托盘窗口无效**，`scripts\verify\verify-cloud-account-window.ps1 -Mode Discover` 已改为双窗都试）。

| # | 判据 | 期望 |
| --- | --- | --- |
| A-1 | 已登录态出现常驻说明行 | 「重置登录口令后，云端旧条目将无法再解开，可在「设备与云数据」清空云端数据。」 |
| A-2 | 提示行**不被裁切**、不重叠 | 单行就显示一行；若折行必须两行都在窗内可见（`h=36` 只是留余量的设计值，**折行宽度未实测**） |
| A-3 | 「关闭」按钮位置 | 位于提示行下方，完整可见且可点 |
| A-4 | 未登录态 | **不出现**该行（`AccountVisibleIds` 按登录态过滤） |
| A-5 | 未登录态底部留白可接受 | `AccountClientSize` 按窗口角色统一取高度 ⇒ 两态同高 290 px，未登录态会有留白（设计取舍，非缺陷） |

取证：`PrintWindow` 截窗 + 目视。判据不可用「状态文案」（自绘 + 刷新滞后）。

### 实测结果（批次 BA · 2026-11-20 · 本机 150% 缩放 `dpi=144` · 控件级取证，非人工目视）

| 条目 | 结论 | 依据 |
|---|---|---|
| A-1 | **PASS** | 控件 `id=1035` 实取文本 37 字符，与期望文案逐字相同（存盘可机检 diff） |
| A-2 | **PASS**（**仅 150% 及以下成立**） | 控件 456×36；按该控件真实字体（宋体 @144dpi，行高 18 px）需 2 行 = **36 px ⇒ 余量 0 px**；像素剖面两条墨迹带 `0..16` / `18..34`，第 35 行无墨 ⇒ 两行均完整；与 1006 状态行、1010 关闭钮矩形无重叠 |
| A-3 | **PASS** | 屏幕坐标 1035 `y277..313` → 1006 `319..359` → 关闭钮 1010 `371..399`，窗底 `422` ⇒ 关闭钮在提示行下方且完整可见；「可点」沿用批次 AX 真机 3 轮开/关成功，本批未做合成点击 |
| A-4 / A-5 | **未测试** | 需**未登录态**；本批探针不自行登出账号 |

> **⚠️ 新缺陷 C（本批量出，未修）**：`CloudAccountUi.h:159` 的提示行高度 `36` 是**未随 DPI 缩放的物理像素字面量**，而 `WeaselServer.exe` 是 PerMonitorV2。
> 150% 下余量已为 **0 px** ⇒ **任何 >150% 缩放的机器必裁该提示行末行**（175% 需 42 px、200% 需 48 px，均 > 36 px；线性推算，未在那两种缩放真机验证）。
> 修法候选见 `docs/archive/handoffs/handoff-2026-11-20-batch-ba-a1-note-wrap-measured.md` §4；取帧与数据原件在 `reports/fluxing-ba-a1-note-wrap/`。

## 2. 孤儿 IPC 会话回收（ADR 0067，本批新增）

**场景**：宿主进程崩溃/被强杀而未走 `Deactivate` ⇒ 会话残留 ⇒ 闸门长期判「你在打字」。

| # | 判据 | 期望 |
| --- | --- | --- |
| B-1 | 杀掉宿主进程 | 后续云同步轮次**不再**因该残留会话被「跳过」 |
| B-2 | 诊断日志 | `%TEMP%\fluxing-profile-notify.log` 出现 `reclaim orphan session ipc_id=<n> client_pid=<pid> live=<m>` |
| B-3 | **关键边界**：回收时若活动会话属**另一个存活宿主** | 日志 `orphan_reclaim presence=kept`，**不得**出现「火流猩不在场」类状态变化、快捷面板不得被收起 |
| B-4 | 正常切换窗口 / 用户主动切走 IME | 表现与批次 AW 前一致（presence 与面板收起仍发生） |
| B-5 | 客户端版本兼容 | 旧客户端（不写 `session.client_pid=`）⇒ 服务端按「pid 未知」**不回收**，不得误杀 |

⚠️ 判据 B-3 是本轮主会话复核抓出的真缺陷所在：`RemoveSession` 带两个副作用（翻 presence、无条件清 `m_active_session`），
而回收发生在 `AddSession` 内紧接着注册新活动会话的当口 ⇒ 若不做区分，**宿主崩溃会被误读成用户切走**。

**批次 BD 状态注（2026-11-21，判据原文不改，只补口径）**

- **B-5 已闭环** = `PASS_E2E`（`reports/fluxing-bb-orphan-reclaim/b5-e2e.md`，目录 `b5-e2e-20261121-095546/`，
  `FAILURE_COUNT=0` / `EXIT=0`；阴性判据由父会话独立计数复算：日志尾 3000 行内 `reclaim orphan session` 命中 8、
  旧客户端 id `2829320193` 命中 **0** ⇒ 未被误杀）。
- **B-4 已按「宿主退出触发源」口径闭环**（用户 2026-11-21 拍板「按宿主退出口径闭环，不再真跑」）：真 TSF 宿主探针
  `test/TestTsFocusOutHost/TestTsFocusOutHost.cpp` + 门禁 `scripts/verify/b4-tsf-host.ps1` 复现四组完整签名。
  ⇒ 本表 B-4 原写的「**用户主动切走 IME**」这一触发源**登记为已知未覆盖面**（非 FAIL、非待拍板、BE 不重跑）。
- **真实消息到达序已实测更正** = 宿主退出时服务端实到 `focus_out param=2` → `remove_session` → `focus_out param=1`
  （三条夹心）。⇒ `reports/fluxing-bb-orphan-reclaim/b4-focusout.md` §2 原「先收 END_SESSION、后收 FOCUS_OUT」是**错的推断**，
  已就地更正；路径 1（合成探针）只覆盖服务端各 handler 逻辑、**不覆盖真实到达序**，真实到达序由路径 2 覆盖
  ⇒ **B-4「服务端路径已闭环」的结论不受影响**。

## 3. 账号窗缺陷 B 第三分支复现取证（AX 未复现项）

复发「第二次打开的窗关不掉」时：`Get-WindowInfo` 看 `IsWindow` / `IsVisible` / 窗是否启用；
`Get-Content "$env:TEMP\fluxing-profile-notify.log" -Encoding UTF8 -Tail 60` 看 `[acctdlg]` 行
（本批起该标签由 `OutputDebugStringW` **改接** `fluxing::diag::AppendLine` ⇒ **无需调试器即可取证**）。

## 4. 同步不受影响复核（沿用 1.3.5 口径）

闸门是否放行 = **维护导出文件 mtime** 是否前进（状态文案不可作判据）：

```powershell
Get-Item 'D:\Program Files\fluxing\user1\fluxing\sync\adbd9f3b-d5b9-4dfc-9929-85eac8509db6\rime_ice.userdb.txt' | Select-Object LastWriteTime
```

反向判据（**G-3 / G-5**，见 [`regression-checklist-cloud-sync.md`](regression-checklist-cloud-sync.md) §二，仍未自动验证）：
打字 10 s 内、有未上屏组合串 60 s 内，同步**应被拦住**（导出文件 mtime 不动）。

## 5. 环境受限项：`/verify-client-pid`

IPC 管道名**全局固定**（`include/WeaselIPC.h:490-496` ⇒ `\\.\pipe\<用户名>\WeaselNamedPipe`）
⇒ 真实服务端在跑时 mock server 抢不到管道，必然假红。要跑必须先 `/quit` 真实服务。

**⚠️ 本节配方 1.3.6 原文有缺陷（批次 BE 2026-11-21 实测坐实并改正）**：
`/verify-client-pid` 是**纯客户端**（`test/TestWeaselIPC/TestWeaselIPC.cpp:584-591`，只做 `Client::Connect()`），
记录文件 `client_pid.txt` 由**服务端侧**写入（`TestRequestHandler::AddSession` → `record_client_pid()` :771/:554-575，
落到 **exe 同目录** :539）。原配方里 `/quit` 之后**没有任何进程在管道上服务端** ⇒ 必然 `EXIT=-2 [CLIENTPID] connect failed`。
那个服务端 = 同一个 exe 的 `/start` 模式（`server_main()` :876-893，阻塞在 `server.Run()`）。

```powershell
$exe = 'F:\soft\00selfmade\rime_claude\Release\TestWeaselIPC.exe'   # 云单测/探针 exe 在仓库根 Release\，不是 test\TestWeaselIPC\x64\Release\
& 'D:\Program Files\fluxing\weasel\WeaselServer.exe' /quit
while ((@(Get-Process WeaselServer -EA SilentlyContinue).Count) -gt 0) { Start-Sleep -Milliseconds 200 }   # 等真服务真的退出
$mock = Start-Process $exe -ArgumentList '/start' -PassThru -NoNewWindow      # ← 原配方漏掉的 mock 服务端
Start-Sleep -Milliseconds 1500
& $exe /verify-client-pid; "EXIT=$LASTEXITCODE"        # 期望 EXIT=0
& $exe /stop                                           # 关 mock（走 ShutdownServer）
Start-Process 'D:\Program Files\fluxing\weasel\WeaselServer.exe'   # 重新拉起
```

退出码语义（读码所得，勿混用）：`-2` = 管道上**没有任何服务端**（配方漏起 mock，或真服务没停干净）；
`-3` = `Echo()` 会话失败；`-4` = mock 从未记录 pid ⇒ **真服务端抢走了管道**；`-5` = 记录到的 pid 与本进程不符。
判别性：`record_client_pid` 对「键缺失/畸形」记 **0**，所以 `reported=self≠0` 才证明生产解析器真在请求体上命中。

**批次 BE 状态注（2026-11-21）**：本项**已实测通过** = `EXIT=0` +
`[CLIENTPID] reported=1026796 self=1026796` + `[CLIENTPID] PASS: the request body carries session.client_pid= and the value is this process`；
`SRV_DURING_MOCK=0`（窗口内无真服务端）⇒ 首跑的 `-2` **不是竞态而是配方缺步骤**。
证据：`reports/fluxing-be/verify-client-pid-run2.txt`（整窗时间线 13:38:03.9→13:38:12.3，停服 8.4 s，跑完服务恢复 1.3.8.0 唯一）
+ 同目录 `verify-client-pid-raw.txt`（首跑 `-2` 留证）。判据由「推断」升「实测」的口径见 BE handoff §3。

## 6. 「不是缺陷」的已知现象（勿误报）

- **云单测 exe 在仓库根 `Release\`**，不在 `output\`；`build.bat weasel` **不构建**它。
- **同步轮次的状态文案自绘且刷新滞后** ⇒ 只用导出文件 mtime 作判据。
- 未登录态账号页底部留白（见 A-5）是高度统一的设计取舍。
- `weasel.props` 是 `build.bat` 生成的、不被 git 跟踪 ⇒ 它出现在脏文件列表里属正常。
- makensis 的 8 条 `warning 7010`（ARM/ARM64 产物、`data\*.gram`、`data\lua\*.txt`）是既有可选产物告警。
