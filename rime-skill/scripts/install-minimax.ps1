<#
    install-minimax.ps1 — 把 rime-skill 部署到 MiniMax Code 的技能目录。

    用法:
        powershell -ExecutionPolicy Bypass -File scripts\install-minimax.ps1
        powershell -ExecutionPolicy Bypass -File scripts\install-minimax.ps1 -Verify
        powershell -ExecutionPolicy Bypass -File scripts\install-minimax.ps1 -Target <path>

    注意（本机实测坑，改造前必读）:
        1) 不能用 $env:USERPROFILE 推导数据目录。本机 USERPROFILE = C:\Users\Duanyi，
           但 C:\Users\Duanyi\.minimax\.minimax 是空壳（无 v2 运行时、skills 为空），
           真正的活跃数据目录在 E:\Users\Duanyi\.minimax\.minimax（有 v2\sessions、40 个技能）。
           脚本改为自动探测 + 打分择优。
        2) 本文件含中文，必须存成 UTF-8 **带 BOM** + CRLF。Windows PowerShell 5.1
           读取无 BOM 的 .ps1 时按 ANSI(GBK) 解码，中文字符会被打散成非法 token，
           脚本在解析阶段就报 "Unexpected token"。改完本文件请重新执行：
               powershell -ExecutionPolicy Bypass -File scripts\inormalize-encoding.ps1
           或直接用仓库内的 node 脚本修字节。
        3) Copy-Item -Path "$src\*" -Destination $target 在 $target 不存在时会把
           子目录拍平（chapters\ 里的文件直接落到目标根），并在后续目录上报
           "Container cannot be copied onto existing leaf item"。正确做法是先建目标目录，
           再逐个子项 Copy-Item。

    为什么用「复制」而不是 junction:
        多数技能扫描器用 readdir(withFileTypes) + dirent.isDirectory() 判断技能目录；
        Windows 上 junction 被报为符号链接，isDirectory() 返回 false，可能被跳过。
        复制是确定可发现的。改完技能后重跑本脚本即可同步。
#>
param(
    [string]$Target = "",
    [switch]$Verify
)

$ErrorActionPreference = 'Stop'

$source = Split-Path -Parent $PSScriptRoot            # rime-skill\

if (-not (Test-Path (Join-Path $source 'SKILL.md'))) {
    throw "源目录不完整：找不到 $source\SKILL.md"
}

function Resolve-MiniMaxDataDir {
    $candidates = New-Object System.Collections.Generic.List[string]

    if ($env:MINIMAX_DATA_DIR) { $candidates.Add($env:MINIMAX_DATA_DIR) }
    $candidates.Add((Join-Path $env:USERPROFILE '.minimax\.minimax'))

    foreach ($d in (Get-PSDrive -PSProvider FileSystem)) {
        if (-not $env:USERNAME) { continue }
        $p = Join-Path $d.Root "Users\$env:USERNAME\.minimax\.minimax"
        if (Test-Path $p) { $candidates.Add($p) }
    }

    $best = $null
    $bestScore = -1
    $seen = @{}
    foreach ($c in $candidates) {
        if (-not $c) { continue }
        $full = (Resolve-Path -LiteralPath $c -ErrorAction SilentlyContinue)
        if (-not $full) { continue }
        $full = $full.Path
        if ($seen.ContainsKey($full)) { continue }
        $seen[$full] = $true

        $score = 0
        if (Test-Path (Join-Path $full 'v2')) { $score += 1000 }   # 有运行时状态 = 活跃目录
        $skillDir = Join-Path $full 'skills'
        if (Test-Path $skillDir) {
            $score += (Get-ChildItem $skillDir -Directory -Force -ErrorAction SilentlyContinue).Count
        }
        if ($score -gt $bestScore) {
            $bestScore = $score
            $best = $full
        }
    }

    if (-not $best) { throw "找不到 MiniMax Code 数据目录（候选均不存在）" }
    return $best
}

if (-not $Target) {
    $dataDir = Resolve-MiniMaxDataDir
    $Target = Join-Path $dataDir 'skills\rime-skill'
}

Write-Host "source : $source"
Write-Host "target : $Target"

# 目标存在就先清空（只删技能目录本身，不碰其他技能）
if (Test-Path $Target) {
    Write-Host "清理旧副本..."
    Remove-Item -LiteralPath $Target -Recurse -Force
}

# 先建目标目录，再逐个子项复制，避免 Copy-Item 把子目录拍平
New-Item -ItemType Directory -Path $Target -Force | Out-Null
Get-ChildItem -Path $source -Force | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $Target -Recurse -Force
}

# ---- 验证 ----
$srcRoot = (Resolve-Path $source).Path
$dstRoot = (Resolve-Path $Target).Path

$srcFiles = Get-ChildItem -Path $srcRoot -Recurse -File -Force
$dstFiles = Get-ChildItem -Path $dstRoot -Recurse -File -Force

$srcRel = $srcFiles | ForEach-Object { $_.FullName.Substring($srcRoot.Length + 1) } | Sort-Object
$dstRel = $dstFiles | ForEach-Object { $_.FullName.Substring($dstRoot.Length + 1) } | Sort-Object

Write-Host ""
Write-Host "文件数   : $($srcFiles.Count) -> $($dstFiles.Count)"

$diff = Compare-Object -ReferenceObject $srcRel -DifferenceObject $dstRel
if ($diff) {
    Write-Host "路径差异 :"
    $diff | ForEach-Object { Write-Host "  $($_.SideIndicator) $($_.InputObject)" }
    throw "文件清单不一致"
}
Write-Host "路径清单 : 一致"

# 全量哈希比对
$mismatch = @()
foreach ($f in $srcFiles) {
    $rel = $f.FullName.Substring($srcRoot.Length + 1)
    $a = (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash
    $b = (Get-FileHash -LiteralPath (Join-Path $dstRoot $rel) -Algorithm SHA256).Hash
    if ($a -ne $b) { $mismatch += $rel }
}
if ($mismatch.Count -gt 0) {
    $mismatch | ForEach-Object { Write-Host "  HASH MISMATCH: $_" }
    throw "内容不一致：$($mismatch.Count) 个文件"
}
Write-Host "SHA256   : 全部一致"

$skillMd = Join-Path $dstRoot 'SKILL.md'
if (-not (Test-Path $skillMd)) { throw "部署失败：$skillMd 不存在" }
$fm = [System.IO.File]::ReadAllText($skillMd)
$name = if ($fm -match '(?m)^name:\s*(\S+)') { $Matches[1] } else { '?' }
$hasDesc = if ($fm -match '(?m)^description:') { 'ok' } else { 'MISSING' }
$hasTrigger = if ($fm -match '(?m)^triggers:') { 'ok' } else { 'MISSING' }
Write-Host "SKILL.md : name=$name  description=$hasDesc  triggers=$hasTrigger"

if ($name -ne 'rime-skill') { throw "frontmatter name 与预期不符：$name" }
if ($hasDesc -ne 'ok') { throw "frontmatter 缺少 description" }

Write-Host ""
Write-Host "已部署到 MiniMax Code 技能目录。"
Write-Host "新会话即可用 'rime-skill' 触发；当前会话的技能目录是启动时扫描的，需要重开会话。"
