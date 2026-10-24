<#
.SYNOPSIS
    给 librime 子模块的 deps/opencc 打 Windows 构建补丁。

.DESCRIPTION
    librime 的 deps/opencc 在 Windows 上构建时会走这段 CMake：

        find_library(LIBMARISA NAMES marisa)

    find_library 只按文件名找磁盘上的 marisa.lib，而 opencc 的
    marisa-trie 是以 CMake target 的形式随源码一起构建的，
    磁盘上并不存在 marisa.lib，于是 LIBMARISA 为空，链接阶段报
    "cannot find -lmarisa" 之类的错误。

    补丁把它改成：

        find_package(marisa)
        set(LIBMARISA marisa)

    这样 target_link_libraries() 拿到的是 CMake target 名，交给 CMake
    去解析依赖与链接顺序，而不是当成一个磁盘路径。

    这个补丁以前是手工打进 librime 子模块的，主仓库里没有任何记录。
    后果是：谁把子模块还原了（git checkout / 重新 clone），librime 就
    再也编译不出来，而且没有任何报错提示指向真正的原因。

    本脚本把补丁固化到主仓库的构建流程里：
      - 幂等：已经打过补丁就直接跳过
      - 可逆：附带 -Revert 参数把子模块还原成上游状态
      - 只改一个被跟踪文件，不碰任何未跟踪的构建产物

    见 .specify/memory/lessons-learned.md L121。

.PARAMETER Revert
    还原补丁（把 deps/opencc/CMakeLists.txt 恢复到上游状态）。

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File scripts\patch-librime-opencc-marisa.ps1
    powershell -ExecutionPolicy Bypass -File scripts\patch-librime-opencc-marisa.ps1 -Revert
#>
[CmdletBinding()]
param(
    [switch]$Revert
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$target   = Join-Path $repoRoot 'librime\deps\opencc\CMakeLists.txt'

# 上游写法（需要被替换的那一行）与补丁写法
$upstreamLines = @(
    '  find_library(LIBMARISA NAMES marisa)'
)
$patchedLines = @(
    '  find_package(marisa)',
    '  set(LIBMARISA marisa)'
)

if (-not (Test-Path -LiteralPath $target)) {
    Write-Error "opencc CMakeLists.txt not found: $target"
    Write-Error 'Run "git submodule update --init --recursive" first.'
    exit 1
}

$text = [System.IO.File]::ReadAllText($target)

$isPatched = $text.Contains('find_package(marisa)')
$isUpstream = $false
foreach ($l in $upstreamLines) {
    if ($text.Contains($l)) { $isUpstream = $true; break }
}

if ($Revert) {
    if (-not $isPatched) {
        Write-Host '[opencc-patch] already clean (no patch applied) - nothing to revert'
        exit 0
    }
    $newText = $text.Replace(($patchedLines -join "`r`n"), ($upstreamLines -join "`r`n"))
    if ($newText -eq $text) {
        Write-Error 'Patch marker found but the exact patched block was not matched; refusing to guess.'
        exit 1
    }
    [System.IO.File]::WriteAllText($target, $newText, (New-Object System.Text.UTF8Encoding($false)))
    Write-Host '[opencc-patch] reverted deps/opencc/CMakeLists.txt to upstream state'
    exit 0
}

if ($isPatched) {
    Write-Host '[opencc-patch] already applied - nothing to do'
    exit 0
}

if (-not $isUpstream) {
    Write-Error 'Neither the upstream find_library line nor the patch marker was found.'
    Write-Error 'Refusing to guess; inspect the file manually.'
    exit 1
}

$newText = $text.Replace(($upstreamLines -join "`r`n"), ($patchedLines -join "`r`n"))
if ($newText -eq $text) {
    Write-Error 'Replacement produced no change - the line endings may differ. Inspect manually.'
    exit 1
}
[System.IO.File]::WriteAllText($target, $newText, (New-Object System.Text.UTF8Encoding($false)))
Write-Host '[opencc-patch] applied: find_library(LIBMARISA NAMES marisa) -> find_package(marisa) + set(LIBMARISA marisa)'
exit 0
