<#
  Fluxing 托盘「中/英」输入指示器体检脚本

  用途：一次性判定「通知区域那个方框『中』/『英』图标还在不在」以及**若在，是谁注册的**。
  背景：这个图标在本项目反复被误判过三轮（批次 J / W / AD），根因每次都不同：
    - 批次 J  摘 TF_LBI_STYLE_SHOWNINTRAY  => 该标志微软标注「目前不支援」，是 no-op
    - 批次 W  改为「根本不注册语言栏项」  => 机制有效，但**只重建了 x64 链**，
             且装机时裸名 weasel.dll / weaselx64.dll 被在跑的进程占用、覆盖失败，
             旧镜像仍留在盘上并被旧进程持有 => 图标「又出现了」
    - 批次 AD 探针坐实：通知区里 OUR_TOOLTIP_HIT=1，前台宿主映射的是
             **12:24 的裸名旧 DLL**（注册表早已指向修复版 shim，但旧进程不换镜像）

  本脚本把「注册表指向谁 / 盘上有哪些字节 / 谁把它映射在内存里 / 通知区现在挂着谁」
  四件事一次算清，避免下一轮再靠猜。

  用法:
      powershell -NoProfile -ExecutionPolicy Bypass -File check-tray-input-indicator.ps1
      powershell -NoProfile -ExecutionPolicy Bypass -File check-tray-input-indicator.ps1 -SkipUia

  退出码:
      0 = 我方语言栏项已不在通知区（只剩 Windows 自带 IME 指示器）
      1 = 我方语言栏项仍在通知区（脚本会打印持有者与处置动作）
      2 = 环境不可用（UIA 不可用 / 安装目录不存在等）
#>
[CmdletBinding()]
param(
  [string]$InstallRoot = 'D:\Program Files\fluxing',
  [string]$RepoRoot    = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
  [switch]$SkipUia
)

$ErrorActionPreference = 'Continue'

function Say($m) { Write-Host $m }

function Get-Sha16([string]$p) {
  try { return (Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.Substring(0, 16) }
  catch { return '<none>' }
}

Say '==== Fluxing 托盘输入指示器体检 ===='

# ---------------------------------------------------------------- 0. 期望构建产物
$expect = @{}
foreach ($pair in @(@{ k = 'weasel.dll';    arch = 'x86' }, @{ k = 'weaselx64.dll'; arch = 'x64' })) {
  $p = Join-Path $RepoRoot ('output\' + $pair.k)
  if (Test-Path -LiteralPath $p) {
    $expect[$pair.arch] = [pscustomobject]@{ Path = $p; Sha16 = (Get-Sha16 $p); Len = (Get-Item $p).Length }
    Say ("[BUILD] {0,-4} {1,-46} {2,9} B  sha16={3}" -f $pair.arch, $pair.k, $expect[$pair.arch].Len, $expect[$pair.arch].Sha16)
  } else {
    Say ("[BUILD] {0,-4} {1} 不存在（先跑 build.bat weasel + x64 链）" -f $pair.arch, $pair.k)
  }
}

$engineDir = Join-Path $InstallRoot 'weasel'
if (-not (Test-Path -LiteralPath $engineDir)) {
  Say "[FATAL] 引擎目录不存在：$engineDir"
  exit 2
}

# ---------------------------------------------------------------- 1. 注册表指向（64 位 + 32 位两个视图）
Say ''
Say '--- 1. TIP CLSID 指向 ---'
$clsid = '{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}'
$regTargets = @()
foreach ($view in 'HKCR\CLSID', 'HKCR\Wow6432Node\CLSID') {
  $q = & reg.exe query "$view\$clsid\InprocServer32" 2>&1 | Out-String
  $m = [regex]::Match($q, '(?m)^\s*\(Default\)\s+REG_SZ\s+(.+)$')
  if ($m.Success) {
    $target = $m.Groups[1].Value.Trim()
    $regTargets += $target
    $sha = if (Test-Path -LiteralPath $target) { Get-Sha16 $target } else { '<missing>' }
    $fresh = $false
    foreach ($a in 'x86', 'x64') {
      if ($expect.ContainsKey($a) -and (Get-Sha16 $expect[$a].Path) -eq $sha) { $fresh = $true }
    }
    Say ("[REG] {0,-24} -> {1}  sha16={2}  {3}" -f $view, (Split-Path $target -Leaf), $sha, $(if ($fresh) { '= 当前构建产物 ✓' } else { '≠ 当前构建产物 ✗' }))
  } else {
    Say "[REG] $view 下未查到 InprocServer32"
  }
}

# ---------------------------------------------------------------- 2. 盘上的字节
Say ''
Say '--- 2. 安装目录内的 TSF 二进制 ---'
$staleOnDisk = @()
Get-ChildItem -LiteralPath $engineDir -Filter 'weasel*.dll' -ErrorAction SilentlyContinue | ForEach-Object {
  $sha = Get-Sha16 $_.FullName
  $isFresh = $false
  foreach ($a in 'x86', 'x64') {
    if ($expect.ContainsKey($a) -and $expect[$a].Sha16 -eq $sha) { $isFresh = $true }
  }
  if (-not $isFresh) { $staleOnDisk += $_.FullName }
  Say ("[DISK] {0,-26} {1,9} B  {2}  sha16={3}  {4}" -f $_.Name, $_.Length, $_.LastWriteTime.ToString('MM-dd HH:mm:ss'), $sha, $(if ($isFresh) { 'fresh' } else { 'STALE' }))
}
Say ("[DISK] 非当前构建产物的文件数 = {0}" -f $staleOnDisk.Count)

# ---------------------------------------------------------------- 3. 谁把它映射在内存里
Say ''
Say '--- 3. 运行中进程映射的 TSF 二进制（判「已加载镜像」，不是磁盘当前字节） ---'
# ★ 判据说明：Get-FileHash 读的是磁盘**当前**字节，而进程里映射的是**加载那一刻**的字节。
#   覆盖磁盘文件不会改变已加载镜像（Windows 的 section 已固定旧字节）⇒ 用 sha256 比对必然误报。
#   正确判据：裸名 DLL（weasel.dll / weaselx64.dll）若在进程启动**之后**被替换过，
#   该进程就仍持有替换前的旧字节 ⇒ OLD-IMAGE。
#   版本化 shim（weasel.<ver>.<build>.<seq>.dll）不可能被「原地替换」（内容按版本号唯一），
#   按 sha16 判定即可。
$fixedShimSha = @('415FE7C5C348D2D9', 'AB623A41EE447C5D')   # 批次 W 修复版 / 当前构建产物
function Get-ImageState($proc, $mod) {
  $isBare = ($mod.ModuleName -ieq 'weasel.dll' -or $mod.ModuleName -ieq 'weaselx64.dll')
  $started = $null; $diskMtime = $null; $sha = '<none>'
  try { $started = $proc.StartTime } catch { }
  try { $diskMtime = (Get-Item -LiteralPath $mod.FileName).LastWriteTime } catch { }
  try { $sha = (Get-FileHash -LiteralPath $mod.FileName -Algorithm SHA256).Hash.Substring(0, 16) } catch { }
  if ($isBare) {
    if ($null -ne $started -and $null -ne $diskMtime -and $started -lt $diskMtime) {
      return @{ State = 'OLD-IMAGE'; Sha = $sha; Reason = ("裸名 DLL 于 {0} 被替换，本进程启动于 {1} ⇒ 仍持有替换前的旧字节" -f $diskMtime.ToString('MM-dd HH:mm:ss'), $started.ToString('MM-dd HH:mm:ss')) }
    }
    return @{ State = 'BARE-NEW'; Sha = $sha; Reason = '裸名 DLL，但本进程启动晚于替换时刻 ⇒ 加载的是修复版' }
  }
  if ($fixedShimSha -contains $sha) { return @{ State = 'SHIM-FIXED'; Sha = $sha; Reason = '版本化 shim，内容 = 修复版' } }
  return @{ State = 'SHIM-UNKNOWN'; Sha = $sha; Reason = '版本化 shim，内容不是已知修复版' }
}
$staleMapped = @()
Get-Process -ErrorAction SilentlyContinue | ForEach-Object {
  $p = $_
  try {
    $p.Modules | Where-Object { $_.FileName -like "$engineDir\weasel*.dll" } | ForEach-Object {
      $st = Get-ImageState $p $_
      if ($st.State -eq 'OLD-IMAGE') {
        $staleMapped += [pscustomobject]@{ Proc = $p.ProcessName; Pid = $p.Id; File = $_.FileName; Reason = $st.Reason }
      }
      Say ("[MEM] {0,-22} pid={1,-7} {2,-26} sha16={3}  {4,-12} {5}" -f $p.ProcessName, $p.Id, $_.ModuleName, $st.Sha, $st.State, $st.Reason)
    }
  } catch { }
}
Say ("[MEM] 仍持有**修复前镜像**的进程数 = {0}" -f $staleMapped.Count)

# ---------------------------------------------------------------- 3.5 当前前台宿主
Say ''
Say '--- 3.5 当前前台窗口宿主映射的 TSF 二进制 ---'
try {
  if (-not ('FluxingFgHost' -as [type])) {
    Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class FluxingFgHost {
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern int GetWindowTextW(IntPtr hWnd, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern int GetWindowThreadProcessId(IntPtr hWnd, out int pid);
}
"@
  }
  $hwnd = [FluxingFgHost]::GetForegroundWindow()
  $pidFg = 0
  [void][FluxingFgHost]::GetWindowThreadProcessId($hwnd, [ref]$pidFg)
  $sb = New-Object System.Text.StringBuilder 512
  [void][FluxingFgHost]::GetWindowTextW($hwnd, $sb, 512)
  $procFg = Get-Process -Id $pidFg -ErrorAction SilentlyContinue
  Say ("[FG] hwnd={0} pid={1} proc={2} title='{3}'" -f $hwnd, $pidFg, $(if ($procFg) { $procFg.ProcessName } else { '<gone>' }), $sb.ToString())
  if ($procFg) {
    try {
      $mods = @($procFg.Modules | Where-Object { $_.FileName -like "$engineDir\weasel*.dll" })
      if ($mods.Count -eq 0) {
        Say '[FG] 该前台进程没有映射任何 TSF 二进制 —— 当前 TSF 宿主不是它（语言栏项可能是上一任宿主注册后残留）'
      } else {
        foreach ($m in $mods) {
          $st = Get-ImageState $procFg $m
          Say ("[FG]   -> {0}  sha16={1}  {2}  {3}" -f $m.ModuleName, $st.Sha, $st.State, $st.Reason)
        }
      }
    } catch { Say "[FG] 读取模块失败：$($_.Exception.Message)" }
  }
} catch {
  Say "[FG] 取前台窗口失败：$($_.Exception.Message)"
}

# ---------------------------------------------------------------- 4. 通知区实况
$ourHit = -1
$indCount = -1
if (-not $SkipUia) {
  Say ''
  Say '--- 4. 通知区域（UIA 枚举） ---'
  try {
    Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes -ErrorAction Stop
    $root = [System.Windows.Automation.AutomationElement]::RootElement
    $cc = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ClassNameProperty, 'Shell_TrayWnd')
    $tray = $root.FindFirst([System.Windows.Automation.TreeScope]::Children, $cc)
    $btnCond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ControlTypeProperty, [System.Windows.Automation.ControlType]::Button)
    $btns = $tray.FindAll([System.Windows.Automation.TreeScope]::Descendants, $btnCond)
    $names = @()
    for ($i = 0; $i -lt $btns.Count; $i++) { $names += ($btns.Item($i).Current.Name -replace "`r?`n", ' / ') }
    $ind = @($names | Where-Object { $_ -match '输入指示|Input Indicator' })
    $ours = @($names | Where-Object { $_ -match '左键切换模式' })
    $indCount = $ind.Count
    $ourHit = $ours.Count
    Say ("[UIA] 通知区「任务栏输入指示」按钮数 = {0}" -f $indCount)
    $ind | ForEach-Object { Say ("[UIA]   - {0}" -f $_) }
    Say ("[UIA] 我方语言栏项命中（tooltip 含「左键切换模式」）= {0}" -f $ourHit)
  } catch {
    Say "[UIA] 枚举失败：$($_.Exception.Message)"
  }
} else {
  Say ''
  Say '--- 4. 通知区域：已跳过（-SkipUia） ---'
}

# ---------------------------------------------------------------- 5. 结论
Say ''
Say '==== 结论 ===='
if ($ourHit -eq 0) {
  Say 'VERDICT=PASS_OUR_ITEM_ABSENT  我方语言栏项不在通知区；剩下的那个是 Windows 自带 IME 指示器（本仓库代码管不了）。'
  exit 0
}
if ($ourHit -gt 0) {
  Say 'VERDICT=FAIL_OUR_ITEM_STILL_REGISTERED  我方 ITfLangBarItemButton 仍被注册，托盘方框「中/英」因此仍在。'
  Say ''
  Say '处置（按这个顺序做，不要跳步）：'
  if ($staleMapped.Count -gt 0) {
    Say '  1) 下列进程还持有**旧镜像**，它们的 TSF 前端跑的是修复前的代码 —— 关闭它们（重开即可载入新件）：'
    $staleMapped | ForEach-Object { Say ("       - {0} (pid={1})  <-  {2}" -f $_.Proc, $_.Pid, (Split-Path $_.File -Leaf)) }
  } else {
    Say '  1) 没有进程持有旧镜像 —— 请确认注册表是否指向了修复版 shim（上表 [REG]）。'
  }
  Say "  2) 旧件被占用时装机只写 versioned shim（install.nsi 的 locked 分支），所以裸名文件会一直是旧字节；"
  Say "     必要时先结束占用进程再重装一次，让裸名文件也被刷新。"
  Say '  3) 重装后重跑本脚本，确认 OUR_TOOLTIP_HIT=0。'
  exit 1
}
Say 'VERDICT=INCONCLUSIVE（UIA 不可用，跳过通知区判定）'
exit 2
