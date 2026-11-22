# 批次 BO 交接文档 · 客户端推送分批对齐服务端上限 64 条（ADR 0073）

- 日期：2026-11-22
- 批次：BO（接 BN）
- 分支：`feat/100-cloud-sync`（远程 `kizemo`）
- 性质：**产品修复 + 门禁 + 出包装机 + 装机实证 + 真机复验 + 文档 + 提交**
- 边界：**未改** `librime/`、**未改** IPC 协议消息类型、**未新增**第三方依赖、**未改** `.clang-format`、**未 bump** 版本号（装机版仍 1.3.8.0，判定一律用 sha16 / 字节数）
- 提交：`ccf6477` `fix(WeaselServer)`（代码 + ADR 0073 + CHANGELOG + 5 份探针/诊断证据，9 文件 +381/−2）；`a68bfd4` `docs(fluxing)`（本 handoff + 接力 prompt）；回填本次提交实况为第 3 个提交。三者均已推 `kizemo/feat/100-cloud-sync`，推送后 `git status -sb` 无 ahead。

---

## 1. 本批要解决的问题（真机复验暴露的独立缺陷）

BN 收口后按用户选定顺序（**A→B：先补真机复验再 push**）跑官方探针，1017「最近同步」给出：

```
最近同步：2026-11-22 11:0x:xx · 失败（单次推送条目数上限为 64（实收 200）。）
```

这不是 BK–BN 那批修复的回归，而是**客户端与服务端推送批量硬契约错配**：

| 侧 | 位置 | 值 |
|---|---|---|
| 服务端 | `cloud/fluxing-sync/src/wire.ts:33` | `export const MAX_PUSH_ITEMS = 64;` |
| 服务端 | `cloud/fluxing-sync/src/sync.ts:98-103` | 超限 `throw fail.badRequest(..., 'too_many_items')` |
| 客户端（修复前） | `WeaselServer/Cloud/CloudSyncService.cpp` 匿名 namespace | `constexpr size_t kPushBatchSize = 200;` |

后果链：本地待推 ≥ 65 条 ⇒ 第一批 200 条超限 ⇒ 服务端 400 ⇒ `push_ok = false` ⇒ **整轮同步失败**；下一轮仍从 0 开始切 200 ⇒ **同一批数据永远切不成小份** ⇒ 不是偶发失败，而是**永久卡死**（待推条目数不降、本地改动继续累积）。

对照取证：拉取侧 `kPullPageLimit = 100` 与服务端 `MAX_PULL_LIMIT = 100` **一致** ⇒ 缺陷是孤立的推送口径错配，**不是**「客户端整体对服务端约束无感知」。

---

## 2. 修法（ADR 0073，5 条决策）

文件：`docs/adr/0073-client-push-batch-aligns-with-server-limit.md`

1. `kPushBatchSize` **200 → 64**，与服务端常量逐值对齐（不取更小：更小只多几次往返，不增加正确性，还会让「130 条改动」这类正常体量多跑一批）。
2. 两个常量**上移到头文件** `WeaselServer/Cloud/CloudSyncService.h`：`kServerMaxPushItems = 64`（标注服务端真源）+ `kPushBatchSize = 64` ⇒ 模块级**单一真相源**；`.cpp` 删除本地定义、改一行注释指向头文件。两处定义分叉正是本次事故的成因。
3. 护栏用**编译期** `static_assert(kPushBatchSize <= kServerMaxPushItems, "推送单批不得超过服务端 MAX_PUSH_ITEMS，否则整批被拒")` ⇒ 「把 `kPushBatchSize` 调大」**直接编译失败**。运行时用例抓不住这一点：假后端不执行服务端校验，常量被改大后多数用例仍然通过。
4. **刻意不加**行为级用例「130 条本地改动 ⇒ 恰好 3 次 push 且每次 ≤ 64」：`MockTransport` 按 FIFO 回预设 reply、不模拟服务端 64 校验 ⇒ 只能断言批次数，对「常量被调大」没有独立于 `static_assert` 的拦截力；要让它有拦截力就得把服务端规则抄进测试 = **第三处真相源**。将来假后端引入协议级校验再补。
5. **只改客户端**：不动 `cloud/fluxing-sync/**`（改它需重新部署 Worker、放弃写放大保护，且已装机旧客户端仍按自己的值发）；不动 IPC 协议（`CloudSyncService.h/.cpp` 属 `WeaselServer/Cloud/` 模块内部，**不涉** `include/WeaselIPCData.h` ⇒ **不触发四边同步红线**）；不动控件 ID / 布局表 / 版本号。

### 实际改动清单

```
WeaselServer/Cloud/CloudSyncService.h  +13
WeaselServer/Cloud/CloudSyncService.cpp +4/-2   (含 :863 头文件指针注释、推送分批循环重排)
```

- `CloudSyncService.h:54-65`：两常量 + `static_assert` + 中文注释指向 `wire.ts` 与历史故障现象。
- `CloudSyncService.cpp:863`：删除本地常量，注释指向头文件。
- `CloudSyncService.cpp:1141-1170`：推送分批循环改 `size_t` 索引（避免无符号/有符号比较告警）。
- `kPullPageLimit` / `kMaxPullPages` **未动**。

---

## 3. 门禁（全部实测通过）

| 项 | 结果 |
|---|---|
| 构建 | `cmd /c "build.bat release weasel installer"` ⇒ **`BUILD_EXIT=0`**；`msbuild1.log`（8 064 968 B）/ `msbuild2.log`（880 926 B）均「已成功生成。0 个警告 0 个错误」；makensis 仅 **8 条既有 `warning 7010`**（与 BN 基线一致） |
| 云单测 | **`RESULT: PASS=704 FAIL=0`** / `VERDICT: ALL PASS` / `回环回归：PASS=11 FAIL=0`；`release\TestFluxingCloud.exe` mtime **14:06:17** 晚于源码 ⇒ 非假绿；与 BN 基线 **704 持平**（按决策 4 刻意不加用例） |
| L0 静态门禁 | `scripts/gate/L0-static.ps1 -Mode all` ⇒ `checks=15 pass=15 fail=0 skip=0` / **`L0_EXIT=0`** |
| clang-format | `C:\Python314\Scripts\clang-format.exe` **18.1.8**；头文件首轮 `--lines=52:66 --dry-run` 抓到 **4 处**中文宽字符列宽漂移（`:56:97` / `:56:111` / `:57:1` / `:57:72`）⇒ `-i --lines=52:66` 就地修 ⇒ `--lines=50:70` **`RECHECK_EXIT=0`**；cpp `--lines=858:872` **`CPP_EXIT=0`** |

顺序铁律遵守：**格式化 → 复检 → 构建 → 单测 → 打包 → 装机 → 记 sha**（改完格式产物 sha 会变）。

---

## 4. 产物与装机实证

- `output\Win32\WeaselServer.exe` = **2 801 664 B** / sha16 **`1E82FA7F30DBB263`** / mtime 2026-11-22 14:03:36
- `output\archives\fluxing-1.3.8-installer.exe` = **42 982 828 B** / sha16 **`5D30C15DB6EC6158`**（makensis `Total size: 42982828`、`CRC 0xE58DE77A`）
- 静默装机 `/S /D=D:\Program Files\fluxing` ⇒ 装机件 `D:\Program Files\fluxing\weasel\WeaselServer.exe` = **2 801 664 B / sha16 `1E82FA7F30DBB263`** ⇒ **`SHA_MATCH_BUILD_VS_INSTALLED=True`**
- 服务进程 `PRE_PID=193484@10:28:35`（= BN 装机版）→ **`POST_PID=1077296@14:07:45`**（ver 1.3.8.0，**安装器自己换掉**，属预期）
- `HKCU\Software\Fluxing\Weasel\RimeUserDir = D:\Program Files\fluxing\user1\fluxing` **未动**；**未登出 / 未登录 / 未改注册表**

---

## 5. 真机复验（部分达成，如实登记）

探针未改一行：`reports/fluxing-bk-syncstatus/probe-sync-status.ps1`（纯 ASCII · 483 行）。

| 轮次 | 报告 | 1017 文本 | 结论 |
|---|---|---|---|
| 修复前 | `sync-status-20261122-140113.txt` | `失败（单次推送条目数上限为 64（实收 200）。）` | 缺陷现象 |
| 修复后 #1 | `sync-status-20261122-141103.txt` | `SYNC_STATUS_AFTER=` **空** ⇒ `EXIT=4` | **探针侧假阴性**（见 §6） |
| 修复后 #2 | `sync-status-20261122-141529.txt` | `最近同步：2026-11-22 14:15:27 · 失败（网络请求失败：接收响应失败：错误码 12002）` | **`VERDICT=SYNC_STATUS_OBSERVABLE` / `EXIT=0`** |

正式记录（`sync-status-20261122-141529.txt`，`TS=2026-11-22 14:15:18`，`-WaitSeconds 60`）：

```
DESKTOP=Default  SRV_PID=1077296  VER=1.3.8.0
ACCOUNT_WINDOW_HWND=4327226  CLASS=FluxingCloudAccountDialog_v1
ACCOUNT_WINDOW_OPENED_BY_PROBE=False        # 复用了 diag 留下的窗口
SYNC_STATUS_BEFORE=最近同步：2026-11-22 14:13:50 · 失败（… 12002）
SYNC_CLICK_SENT=True
SYNC_STATUS_AFTER =最近同步：2026-11-22 14:15:27 · 失败（… 12002）
SYNC_STATUS_CHANGED=True  HAS_TIMESTAMP=True  HAS_RESULT=True  IS_PLACEHOLDER=False
RESULT_KIND=FAILED  STATUS_LINE_1006_CHANGED=False
REASON=OK  VERDICT=SYNC_STATUS_OBSERVABLE  EXIT=0
```

### 已证 / 未证

- **已证**：`too_many_items` 拒绝**已消失**（诊断日志同轮无该失败行；三轮文本均为 `12002`）；1017 观测面判据通过、点击后 3 s 内更新。
- **未证**：**「待推 ≥65 条 ⇒ 分多批推成功」的成功侧端到端**。三轮（14:11:16 / 14:13:50 / 14:15:27）全部失败于 `网络请求失败：接收响应失败：错误码 12002`（WinHTTP `ERROR_WINHTTP_TIMEOUT`）⇒ 属**环境与链路**问题、与批量契约无关；缺的是网络可用窗口，不是代码路径。
- 同样仍未取得：「令牌过期 → 续期 → **同步成功**」的界面级单轮直证（日志级证据已有，见 §7）。
- ADR 0073 早先写的判据「装新包后 1017 变为**成功态**」**未达成** ⇒ ADR 与 CHANGELOG 均已改写为「已证 / 未证」两栏，登记 **未判定**，不写 PASS。

---

## 6. 同轮附带结论：14:11 的 `EXIT=4` 是探针侧假阴性

为排除「产品写了空串」，另写独立诊断脚本 `tmp/bo-diag-1017.ps1`（纯 ASCII；`Add-Type` 内含 `IsHungAppWindow` / `SendMessageTimeoutW(WM_GETTEXT)` / `GetWindowTextW` / `EnumChildWindows` / `GetWindowLongW(-12)` / `PostMessageW`），报告 `reports/fluxing-bk-syncstatus/diag-1017-20261122-141347.txt`（`DIAG_EXIT=0`）：

- `LOGONUI_PRESENT=False`（不受锁屏影响）、`SRV_PID=1077296 START=14:07:45`、`OPEN_BY_DIAG=True`
- `DLG=0x42073A CLASS=FluxingCloudAccountDialog_v1 VISIBLE=True HUNG=False`
- 全量子控件快照 19 项，其中 `id=1017 hwnd=0xA115DC cls=Static vis=True len=54`（另有 `1016 立即同步` / `1006` / `1020 25940969@qq.com` / `1035` / `1099 ListBox`）
- `BEFORE_DIRECT` 与 `BEFORE_TIMEOUT` **同值**（54 字符）、均 `ok=True`
- `CLICK_SENT=True CLICK_TS=14:13:47.370`；1 s 轮询 60 拍：`dlgAlive=True` / `dlgRecreated=False` / `hung=False` / **`freshStatus` 恒 `0xA115DC`** / `len=54` / `toOk=True`；**T4s 起**文本变为 `… 14:13:50 · 失败（… 12002）`

⇒ 产品未写空串、控件句柄未失效、窗口未挂起、UI 线程未阻塞 ⇒ **`EXIT=4` 不登记为产品缺陷、不登记 FAIL**。

**可能成因**：`probe-sync-status.ps1` 的 AFTER 读取复用点击前缓存的 `$statusCtl` 句柄，且报告不含 `IsWindow($statusCtl)`。**本批不改探针**（未做、也未在探针里加自证输出）。

---

## 7. 诊断日志（`%TEMP%\fluxing-profile-notify.log`）

- 14:06:26 多条 `[cloudsync] access token refreshed after 401; retry with new token`
- 14:06:26 多条 `[cloudsync] skipped malformed cloud userdb entry (payload shape invalid)`
- 14:08:17 / 14:09:43 / 14:12:19 / **14:13:54** 各 1 条 `skipped malformed`
- **无 `too_many_items` 失败行**

⇒ BL 的 401→续期与 BM 的跳过脏条目在**日志级**持续生效；界面级「续期后同步成功」仍缺（§5）。

---

## 8. 已验证 / 推断 / 未验证

**已验证**
1. 服务端上限 64 与客户端 200 的错配是「待推 ≥65 ⇒ 整轮永久失败」的充分原因（代码 + 服务端文案 + 修复前后 1017 文本三段互证）。
2. 改客户端到 64 后，`too_many_items` 拒绝不再出现（三轮探针 + 诊断日志双向确认）。
3. 构建 / 单测 704 / L0 15/15 / clang-format 定点 0 漂移全部实测通过。
4. 装机件与构建产物逐字相等（sha16 `1E82FA7F30DBB263`）。
5. 14:11 `EXIT=4` 的原因在探针侧而非产品侧（diag 直证两路读取同值 54 字符且点击后更新）。

**推断**
- `12002` 是本机到 `fluxing-sync` Worker 的链路/代理超时（非 4xx/5xx、非鉴权失败），因此与本次批量修复正交。
- 探针 AFTER 复用旧句柄是其假阴性最可能成因（未加 `IsWindow` 自证，故只能算推断）。

**未验证**
- 「待推 ≥65 条 ⇒ 3 批推成功」的真实成功侧端到端（被 12002 挡住）。
- 「令牌过期 → 续期 → 同步成功」的界面级单轮直证。
- `has_more`（「云端还有更多条目，下次同步继续」）真机可见性（未触发）。
- 1017 单行 STATIC 在多条提示叠加时是否显示不全（ADR 0072 已登记、刻意不做自动换行）。

---

## 9. 本轮工具坑（新踩到的，按可复用价值排序）

1. **云单测不在 sln**：`TestFluxingCloud` 须显式 msbuild `test\TestFluxingCloud\TestFluxingCloud.vcxproj` **且带 `/p:SolutionDir=F:\soft\00selfmade\rime_claude\`**，exe **须在仓库根运行**。本轮第一次编译**假绿**：`$msbuild` 解析为空但 `MSB_EXIT=0`、exe mtime 仍是旧值 ⇒ 必须用 `vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe'`（**不带 `-products *` 会解析空**），并核对 **`exe.mtime > src.mtime`**。
2. **`cmd //c` 会假绿**（MSYS 吞参数）⇒ 必须 `cmd /c`。
3. **`build.bat` 日志 GBK/UTF-16 混杂**：`msbuild1.log` / `msbuild2.log` 直接 `Get-Content` 搜「警告 / 错误」一个都搜不到 ⇒ 按 BOM 判 `Encoding.Unicode`，否则 `Encoding.GetEncoding(936)`；约 8 MB 先过滤汇总行。
4. **整文件 clang-format 存在既有漂移** ⇒ 必须 `--lines=起:止` 定点；`-i` 后**必须重算区间**再复检。中文注释的宽字符列宽会触发 `:行:列` 形式的额外漂移（本轮头文件 4 处全在中文注释行）。
5. **`CHANGELOG.md` CRLF-only**（本轮实测 `BYTES=704015 CRLF=7665 bareLF=0`）vs handoff / ADR / prompt **LF-only** ⇒ 插条目必须字节级写入 + LF→CRLF，写后复检 `bareLF=0`（本轮写后 `BYTES=710660 CRLF=7680 bareLF=0`、`FFFD=0`）。**不要**用 `Get-Content | Set-Content`。
6. **`/D=` 必须是最后一个参数且不加引号**；`Start-Process -Wait` 对 NSIS **挂死不返回** ⇒ 用 `Wait-Process -Id $proc.Id -Timeout 300`。
7. **NSIS `InstallDirRegKey` 指向 32 位视图** `HKLM\Software\WOW6432Node\Fluxing\Weasel\InstallDir` ⇒ **无需清注册表**（清了反而丢 `RimeUserDir`）。
8. **装机后服务 pid 必变**（安装器按映像名杀旧进程），属预期不是宿主被杀。
9. **同一进程内窗口关掉再开 `HWND` 会变**（14:15 探针复用了 diag 留下的窗口，`ACCOUNT_WINDOW_OPENED_BY_PROBE=False`）⇒ 报告里的 hwnd 不能跨运行当稳定键。
10. **单会话内重复 `Add-Type` 同名类型会失败** ⇒ 每个含 `Add-Type` 的脚本用独立 `pwsh -NoProfile -File` 跑。
11. **派「只读取证 + 落盘报告」的子会话必须派 `worker`**（`explore` / `verifier` 严格只读，秒返 `ROLE_MISMATCH`）；派子会话 **`effort` 一律不传**。
12. **bash 风格命令在本机 pwsh 下全失效**（`cd /f/...`、`2>/dev/null`、`f=$TEMP/...`）；带 `cd` 后再 `Set-Content` 写工作区文件会被本地安全策略拦 ⇒ 改文件用 `edit` / 专用脚本文件。

---

## 10. 未闭环（按可做性分组）

**单机可做，缺条件/缺窗口**
1. 「待推 ≥65 条 ⇒ 分多批推成功」的成功侧端到端（本批被 `12002` 挡住）。
2. 「令牌过期 → 续期 → **同步成功**」界面级单轮直证（BN §10 第 ⑤ 项仍记**部分闭环**）。
3. `has_more`（「云端还有更多条目，下次同步继续」）真机可见性。
4. `probe-sync-status.ps1` AFTER 读取的**句柄自证加固**（在 AFTER 读取前重新 `Find-ChildById` 取新句柄 + 报告输出 `IsWindow($statusCtl)`）——**未做、未登记**，本批已登记为遗留。

**需两台机器 + 用户在场（承接 BN）**
5. 双机 AC 7 条。
6. restore 端到端。
7. 输入延迟两组对照。
8. G-3 / G-5 真跑。

**待用户拍板（承接前批）**
9. 18 份 `run-*.log` 是否 `git add -f`（被 `.gitignore` 挡住）。
10. `tmp\bh-c2d-fmt-check.cpp`（83 767 B，本机策略删不掉）处置。

**已知产品侧后果（刻意不做）**
11. 1017 是单行 STATIC，多提示叠加可能显示不全（ADR 0072 后果段已登记，刻意不做自动换行）。

---

## 11. 证据路径索引

| 用途 | 路径 |
|---|---|
| 决策 | `docs/adr/0073-client-push-batch-aligns-with-server-limit.md` |
| 修复前现象 | `reports/fluxing-bk-syncstatus/sync-status-20261122-140113.txt` |
| 假阴性 | `reports/fluxing-bk-syncstatus/sync-status-20261122-141103.txt` |
| 决定性取证 | `reports/fluxing-bk-syncstatus/diag-1017-20261122-141347.txt`（同轮早一次：`diag-1017-20261122-141325.txt`） |
| 正式通过 | `reports/fluxing-bk-syncstatus/sync-status-20261122-141529.txt` |
| 诊断脚本 | `tmp/bo-diag-1017.ps1` |
| 云单测脚本 | `tmp/bo-run-cloud-test.ps1`（日志 `tmp/bo-cloud-build.log`、`tmp/bo-cloud-test-run.log`） |
| 装机 + L0 脚本 | `tmp/bo-install-gate.ps1`（日志 `tmp/bo-l0.log`） |
| CHANGELOG 插入脚本 | `tmp/bo-insert-changelog.ps1` |
| 服务端真源 | `cloud/fluxing-sync/src/wire.ts:33`、`cloud/fluxing-sync/src/sync.ts:98-103` |

---

## 12. 下一批建议起点

1. 先确认网络可用（`Invoke-WebRequest https://fluxing-sync.<domain>/health` 或直接跑一次手动同步看 1017 是否仍 `12002`）——**这是本批两条遗留判据的前置条件**。
2. 网络通了就一次性收口两条：① 待推 ≥65 条的多批成功侧（可先在本地造 130 条改动，看诊断日志出现 3 次 push 且 1017 变成功态）；② 令牌过期→续期→同步成功的界面单轮直证。
3. 顺手给 `probe-sync-status.ps1` 补句柄自证（`IsWindow` + 重取句柄），把「`EXIT=4` 到底是产品还是探针」永久钉死。