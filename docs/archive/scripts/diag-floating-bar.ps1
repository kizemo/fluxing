# diag-floating-bar.ps1 — v0.19.0.68 bugfix-phase2 装机 diag
#
# 目标: 让装机 user 用 5 步 (无 PowerShell 知识) 抓 4 节点 instrumentation 日志,
#       协助定位 root cause A / B / C / D.
#
# 输出日志路径: %APPDATA%\Rime\weasel-install.log (与 installer log 共享)
#   - "[Run]" / "[RegisterPhrasesHotkey]" / "[ImeLangSubclass]" / "[QP-DIAG]" / DLOG glog
#   - DLOG 在 stderr / glog sink, 装机 main 进程默认无 console, 由 sandbox-verify
#     抓取或 user 从 logdir copy.
#
# 流程 (5 步):
#   Step 1. (script) 备份当前 WeaselServer.exe (output 路径), 准备还原
#   Step 2. (user) 跑 v0.19.0.68 instrumentation installer (装机路径不变)
#   Step 3. (user) 切到 Fluxing, 然后按 Alt+,; 1s 后再切走
#   Step 4. (script) 抓 %APPDATA%\Rime\weasel-install.log 末尾, 输出到 console
#   Step 5. (user) 把 console 输出贴回 assistant, 由 assistant 判别 root cause
#
# 注意: PPL WeaselServer.exe 无法 taskkill (L17), 装机时新 WeaselServer.exe 由
#       installer 替换即可. 如果 installer 拒绝覆盖, 先在 "Task Manager > Details"
#       右键结束 WeaselServer.exe (普通 End Task) 或注销.
#
# 禁止: 本脚本不修改 git, 不 push, 不 ship. 仅本地 diag 工具.

$ErrorActionPreference = "Stop"

$installDir   = "F:\soft\00selfmade\rime_claude\output"
$backupDir    = Join-Path $installDir "diag-floating-bar-bak"
$weaselExe    = Join-Path $installDir "WeaselServer.exe"
$weaselTsfDll = Join-Path $installDir "WeaselTSF.dll"
$logPath      = Join-Path $env:APPDATA "Rime\weasel-install.log"
$timestamp    = Get-Date -Format "yyyyMMdd-HHmmss"

function Step($n, $msg) {
  Write-Host ""
  Write-Host "=== Step $n : $msg ===" -ForegroundColor Cyan
}

# --- Step 1: 备份当前二进制 (user 已装机 v0.68) ---
Step 1 "Backup current WeaselServer.exe / WeaselTSF.dll"
if (-not (Test-Path $weaselExe)) {
  throw "WeaselServer.exe not found at $weaselExe"
}
if (-not (Test-Path $backupDir)) {
  New-Item -ItemType Directory -Path $backupDir | Out-Null
}
$backupExe = Join-Path $backupDir "WeaselServer.exe.$timestamp.bak"
$backupDll = Join-Path $backupDir "WeaselTSF.dll.$timestamp.bak"
Copy-Item $weaselExe $backupExe -Force
if (Test-Path $weaselTsfDll) {
  Copy-Item $weaselTsfDll $backupDll -Force
}
Write-Host "  backed up: $backupExe"
Write-Host "  backed up: $backupDll"

# --- Step 2: 提示 user 跑 instrumentation installer (装机 user 已知路径) ---
Step 2 "Run v0.19.0.68 instrumentation installer (user action)"
Write-Host "  -> run installer that contains the 4-node instrumentation"
Write-Host "     (the binary that built just now, with DLOG / [QP-DIAG] probes)"
Write-Host "  -> installer 路径由 user 已知; 此脚本不动 installer"
Write-Host "  -> 按 installer 默认参数装机 (覆盖 v0.19.0.68)"
Write-Host "  -> installer 完成后按 [Enter] 继续"
Read-Host "  Press [Enter] after installer finishes"

# --- Step 3: 提示 user 操作两条路径 ---
Step 3 "Trigger both paths (user action)"
Write-Host "  Path A (TSF):  Win+Space 切到 Fluxing -> 等 1s -> Win+Space 切到 en-US"
Write-Host "  Path B (Hotkey): 在任意 edit field 焦点里按 Alt+, (浮动栏应弹)"
Write-Host "  每条路径触发后等 2s, 让 log 写入"
Write-Host "  按 [Enter] 表示已做完两条路径"
Read-Host "  Press [Enter] after both paths exercised"

# --- Step 4: 抓 log 末尾 ---
Step 4 "Tail weasel-install.log"
if (-not (Test-Path $logPath)) {
  Write-Host "  [ERROR] log file not found: $logPath" -ForegroundColor Red
  Write-Host "  这意味着 instrumentation 根本没运行. 检查 installer 是否真的覆盖了 exe."
  exit 1
}
$logBytes = (Get-Item $logPath).Length
Write-Host "  log file: $logPath ($logBytes bytes)"
Write-Host "  ---- 末尾 80 行 (从底向上) ----" -ForegroundColor Yellow
Get-Content $logPath -Tail 80 -Encoding Unicode | ForEach-Object { $i = 0 } {}
Get-Content $logPath -Tail 80 -Encoding Unicode

# --- Step 5: 还原备份 (避免污染 user 当前装机) ---
Step 5 "Restore backup (in case user wants to revert)"
$restore = Read-Host "  还原 backup? (y/N)"
if ($restore -eq 'y' -or $restore -eq 'Y') {
  Copy-Item $backupExe $weaselExe -Force
  if (Test-Path $backupDll) { Copy-Item $backupDll $weaselTsfDll -Force }
  Write-Host "  还原完成. 重启 WeaselServer (Task Manager) 生效."
  Write-Host "  backup 仍保留在 $backupDir"
} else {
  Write-Host "  跳过还原. backup 保留在 $backupDir (instrumented binary 仍生效)."
}

Write-Host ""
Write-Host "=== diag 完成 ===" -ForegroundColor Green
Write-Host "把上面 Step 4 的 weasel-install.log 末尾 80 行贴回 assistant,"
Write-Host "assistant 会比对 4 节点 instrumentation 输出, 定位 root cause A/B/C/D."
