$ErrorActionPreference = 'Stop'
$repo = 'F:\soft\00selfmade\rime_claude'
$enc = New-Object System.Text.UTF8Encoding($false)

function Norm([string]$s, [bool]$crlf) {
    $s = $s -replace "`r`n", "`n"
    if ($crlf) { $s = $s -replace "`n", "`r`n" }
    return $s
}

# ================= CHANGELOG bodies (inline after the bold prefix) =================
$gateBody = @'
云单测 `RESULT: PASS=585 FAIL=0 / VERDICT: ALL PASS`（基线 577 ⇒ **+8**；新增 `TestAccountPageUiDpiScale` 8 条判据：96dpi 逐像素不变 / 144dpi 等比缩放 / 换钥文案装得下 / 余量 ≥ 1 行；`exe.mtime 20:27:14 > src.mtime 20:21:15` ⇒ 确为新二进制、非假绿；IPC 回环 `PASS=11 FAIL=0`）· `build.bat weasel installer` **0 错误**（8 条既有 makensis `warning 7010` = ARM/ARM64 可选产物 + `data\*.gram` + `data\lua\*.txt`）· 定点 `clang-format` 3 文件 **0 漂移** · `L0-static.ps1` **15/15**（`version-consistency version=1.3.7 一致`、`artifact-version-resource 5 个产物 FileVersion 均为 1.3.7.0`）
'@

$artifactBody = @'
`output/archives/fluxing-1.3.7-installer.exe` = **42 975 921 B** · sha16 `F91A7A108252CF91`；内嵌 `output\Win32\WeaselServer.exe` = **2 785 280 B** · `FileVersion 1.3.7.0` · sha16 `38316DCEFC8C502E`；**令牌差集**（UTF-16LE 宽串）`dpi-mismatch system=` / `note-margin-short dpi=` = 新构建 **True** / 装机版 1.3.6 **False** ⇒ 本批代码确在产物内。
- **装机实证（本批关键闭环）**：`INSTALL_EXIT=0` ⇒ 运行中服务映像换成 **1.3.7.0**（pid 77332 / start 20:37:56 / sha16 `38316DCEFC8C502E` 与构建产物**逐字一致**）、**无 stage2 残留**、用户数据未动；复跑 BA 探针 ⇒ 控件 `1035` 由 `456×36` 变 **`684×54`**（= ×1.5）、客户区 `480×290` → `720×435`（整表等比缩放落地）⇒ 37 字提示**单行排下**（文本 w=666 ≤ 盒宽 684）、需求 **18 px** / 盒高 **54 px** ⇒ **余量 36 px**，墨迹带 `0–16` 单带且 `lastBandTouchesBottom=False` ⇒ **`VERDICT=A1_NOT_CLIPPED`（缺陷 C 关闭）**。
- **诊断日志实测**：`[acctdlg] dpi-mismatch system=144 font_px=21 derived=168` ⇒ 字体度量交叉校验**按设计告警**（反推 168dpi 会 +17% 超缩，这正是本批放弃「量字体反推 DPI」的直接原因）；全程**无** `note-margin-short` ⇒ 余量充足不裁。
'@

$gapsBody = @'
① **175% / 200% 缩放真机未验**（本机 144dpi 已实证不裁；>150% 的机器仍是推断）② **A-4 / A-5 未登录态未测**（需登出改账号状态，探针不自行发起）③ **AZ §6 第 2 项 B-1/B-2/B-3 孤儿会话回收端到端**未做（下一批第 1 项）④ AZ §6 第 3 项「已运行 TSF 宿主逐 pid 枚举模块验证」、AZ §6 第 4 项「`TestWeaselIPC.exe /verify-client-pid` 需先 `WeaselServer.exe /quit` 抢管道」未做 ⑤ 双机 AC 7 条 + restore 端到端 + 输入延迟两组对照 + 缺陷 A 反向判据 G-3/G-5 未做 ⑥ 本批 / AZ / BA 的文档改动**未 push**（push 需用户授权）
'@

# ================= handoff §5 =================
$handoffGate = @'
- **云单测**：`RESULT: PASS=585 FAIL=0 / VERDICT: ALL PASS`（基线 577 ⇒ **+8**）；新增 `TestAccountPageUiDpiScale` 8 条判据（96dpi 逐像素不变 / 144dpi 等比缩放 / 换钥文案装得下 / 余量 ≥ 1 行 …）。`TEST_EXE_MTIME 20:27:14 > SRC_MTIME 20:21:15` ⇒ 判据确实跑在新二进制上，**非假绿**。IPC 回环 `PASS=11 FAIL=0`。
- **编译**：`build.bat weasel installer` ⇒ `BUILD_EXIT=0` / `BUILD_ERROR_LINES=0`；8 条 `warning 7010` 全部是既有的可选产物告警（`weaselARM*.dll` ×6、`data\*.gram`、`data\lua\*.txt`）。
- **格式**：定点 `clang-format`（`--lines=` 模式，仅本次改动区间）三文件 `exit=0` ⇒ **0 漂移**。
- **静态门禁**：`L0-static.ps1` **15/15** —— `version-consistency version=1.3.7 一致`、`artifact-version-resource 5 个产物 FileVersion 均为 1.3.7.0`、`nsis-compile` 通过。
  - ⚠️ 头一轮曾红在这一条：`weasel.sln` **只构建 Win32 平台**，`output\Win32\weaselx64.dll` 根本不存在 ⇒ x64 TSF 产物由 `build.bat` 权威路径产出（`output\weaselx64.dll`，现为 1.3.7.0）。**改用 `build.bat` 出包后复跑即恢复 15/15**。
- **其它单测**：`TestDefaultHotkeys` / `TestWeaselIPC` / `TestOrphanRecovery` 均 `EXIT=0`。（`TestResponseParser.exe` 本仓库无此产物，**缺失不等于失败**。）

### 5.1 产物与装机实证

- **安装包**：`output/archives/fluxing-1.3.7-installer.exe` = **42 975 921 B** · sha16 `F91A7A108252CF91`。
- **服务映像**：`output\Win32\WeaselServer.exe` = **2 785 280 B** · `FileVersion 1.3.7.0` · sha16 `38316DCEFC8C502E`（装机版 1.3.6 为 2 780 672 B / sha16 `3CF15A582B2453F1` ⇒ 尺寸与哈希均已变化）。
- **令牌差集**（本批诊断串是 `std::wstring` ⇒ **UTF-16LE** 宽串）：`dpi-mismatch system=` / `note-margin-short dpi=` = 新构建 **True** / 装机版 1.3.6 **False** ⇒ 本批代码确在产物内。
- **覆盖装机**：`INSTALL_EXIT=0`（`/S /D=D:\Program Files\fluxing`，`/D` 为最后一个参数）⇒ 运行中服务映像换成 **1.3.7.0**（pid 77332 / start 20:37:56 / sha16 与构建产物**逐字一致**），**无 `*.stage2` 残留**（无待开机换件），`HKLM\Software\Fluxing\Weasel` 与 `HKCU\Software\Fluxing` 未被删改。
- **BA 探针复跑**（`reports/fluxing-ba-a1-note-wrap/probe-a1-note-wrap.ps1`，判据是动态计算，不硬编码几何）：
  - 桌面守卫 `[OK] desktop-guard :: thread desktop = Default`（非影刀虚拟桌面）；
  - 前置 `[OK] precondition :: pid=77332 started=2026-11-20 20:37:56 ver=1.3.7.0`；
  - 控件 `1035` = `STATIC` / **`684×54`** / 37 字（**由 `456×36` 精确放大 ×1.5**），客户区 `480×290` → `720×435`；
  - 度量：单行 `w=666 h=18`；`wrapped-into-real-box(684) w=666 h=18` ⇒ **单行排下，不再折 2 行**；
  - 判定 `[PASS] judge-A1 :: TEXT NEEDS 18px, BOX GIVES 54px -> HEADROOM 36px -> NOT CLIPPED`；
  - 像素复核 `bands=0-16 bandCount=1 lastBandTouchesBottom=False` ⇒ 墨迹未触底；
  - **`VERDICT=A1_NOT_CLIPPED` ⇒ 缺陷 C 关闭**（BA 时期同一判据为 `A1_CLIPPED`）。
- **诊断日志实测**：`%TEMP%\fluxing-profile-notify.log` 出现 `[acctdlg] dpi-mismatch system=144 font_px=21 derived=168` ⇒ 字体度量交叉校验**按设计告警**（反推 168dpi 会 +17% 超缩 ⇒ 正是本批放弃「量字体反推 DPI」的直接原因）；全程**无** `note-margin-short` ⇒ 余量充足。
'@

# ================= handoff §9 =================
$handoffCommit = @'
本批按 Conventional Commits + 单一 scope 分三次提交（**未 push**，push 需用户授权）：

| # | 提交信息 | scope | 内容 |
|---|---|---|---|
| 1 | `docs(fluxing): 补齐 AZ/BA 批次交接文档与 1.3.6 重启后测试清单` | `docs` | AZ / BA 两份 handoff + 两份接力 prompt + `docs/release/restart-checklist-1.3.6.md`（上一批遗留、此前未提交） |
| 2 | `fix(fluxing): 账号页换钥提示 DPI 感知布局 —— 修缺陷 C（>150% 缩放裁切）` | `fluxing` | `WeaselServer/Cloud/CloudAccountUi.h`、`WeaselServer/Cloud/CloudAccountDialog.cpp`、`test/TestFluxingCloud/TestFluxingCloud.cpp`、`docs/adr/0068-account-ui-dpi-proportional-scaling.md`、`docs/adr/README.md`、`CHANGELOG.md`、`CLAUDE.md`、`version.txt`、批次 BB handoff + 接力 prompt |
| 3 | `ci(fluxing): 账号页换钥提示折行与 DPI 取证探针` | `ci` | `reports/fluxing-ba-a1-note-wrap/probe-a1-note-wrap.ps1`、`reports/fluxing-bb-dpi-scale/*.ps1` 及门禁日志 |

- **哈希**：见文末「提交哈希补记」小节（提交落库后由收口提交补入）。
- **禁提交项**：`librime/` 子模块工作区脏（`m librime`）⇒ 全程**按路径显式 `git add`**，**未** `git add .`，子模块改动未入库；`docs/design/quickpanel-v6/design-demos/*.html`（他任务遗留）与 `reports/` 下的中间产物未入库。
- **本批未改**：`librime/`、IPC 协议（`include/WeaselIPCData.h` 未动 ⇒ 不触发四边同步红线）、未新增第三方依赖。
'@

# ================= handoff §10 =================
$handoffNext = @'
1. **AZ §6 第 2 项：B-1 / B-2 / B-3 孤儿会话回收端到端**（ADR 0067）—— 宿主崩溃后服务端会话表里的孤儿 `ipc_id` 应被惰性清扫；需真机强杀宿主进程后取证 `live_sessions` 归零与闸门放行。
2. **A-4 / A-5 未登录态判据** —— 需用户在场并明确授权**登出**（改账号状态，探针不自行发起），再复跑 `probe-a1-note-wrap.ps1` 的未登录分支。
3. **175% / 200% 缩放真机复验** —— 改系统缩放比例后复跑同一探针，判据仍是 `VERDICT=A1_NOT_CLIPPED`。
4. **AZ §6 第 3 / 4 项** —— 逐 pid 枚举已运行 TSF 宿主的模块以验证实际加载的 shim 版本；`TestWeaselIPC.exe /verify-client-pid` 需先 `WeaselServer.exe /quit` 让出命名管道（真服务端在跑时 mock 必假红）。
5. **双机 AC 7 条 + restore 端到端 + 输入延迟两组对照 + 缺陷 A 反向判据 G-3/G-5** —— 用户在场时执行。
6. **提交与 push** —— 本批只做了本地提交；`feat/100-cloud-sync` → `kizemo/feat/100-cloud-sync` 的 push 需用户授权后执行。
'@

# ================= apply =================
$chPath = Join-Path $repo 'CHANGELOG.md'
$ch = [System.IO.File]::ReadAllText($chPath)
$ch = $ch.Replace('<!-- FILL:GATE -->', (Norm $gateBody $true))
$ch = $ch.Replace('<!-- FILL:ARTIFACT -->', (Norm $artifactBody $true))
$ch = $ch.Replace('<!-- FILL:GAPS -->', (Norm $gapsBody $true))
[System.IO.File]::WriteAllText($chPath, $ch, $enc)

$hoPath = Join-Path $repo 'docs\archive\handoffs\handoff-2026-11-20-batch-bb-account-ui-dpi-scale.md'
$ho = [System.IO.File]::ReadAllText($hoPath)
$ho = $ho.Replace('<!-- FILL:GATE -->', (Norm $handoffGate $false))
$ho = $ho.Replace('<!-- FILL:COMMIT -->', (Norm $handoffCommit $false))
$ho = $ho.Replace('<!-- FILL:NEXT -->', (Norm $handoffNext $false))
[System.IO.File]::WriteAllText($hoPath, $ho, $enc)

# ================= verify =================
foreach ($p in @($chPath, $hoPath)) {
    $t = [System.IO.File]::ReadAllText($p)
    $left = ([regex]::Matches($t, 'FILL:')).Count
    $bytes = [System.IO.File]::ReadAllBytes($p)
    $bom = ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF)
    $crlf = ([regex]::Matches($t, "`r`n")).Count
    $bare = ([regex]::Matches($t, "(?<!`r)`n")).Count
    "VERIFY $([System.IO.Path]::GetFileName($p)) FILL_LEFT=$left BOM=$bom bytes=$($bytes.Length) crlf=$crlf bareLF=$bare"
}
$chLines = [System.IO.File]::ReadAllLines($chPath)
"CHANGELOG top2:"
$chLines[0..1] | ForEach-Object { "  $_" }
'BACKFILL_OK'
