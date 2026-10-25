# New-SandboxConfig.ps1 - 生成 Windows Sandbox 的 .wsb 配置
#
# 为什么需要它: 之前的 rime-claims-sandbox.wsb 把主机路径写死了
# (F:\soft\00selfmade\...)。本机跑得起来, CI 上跑不了 —— CI runner 的
# 工作区路径完全不同, 而且工装要跟着仓库一起 checkout。
# 所以 .wsb 改成运行时生成: 路径由参数给进来, 模板在这里。
#
# 用法:
#   .\New-SandboxConfig.ps1 -ToolingDir <工装目录> -RepoRoot <仓库> `
#                          -StagingRoot <暂存目录> -OutputPath <生成的 .wsb>
#
# 沙箱内的路径是固定的 (WDAGUtilityAccount 的桌面), 主机侧的路径是变量。

[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ToolingDir,
    [Parameter(Mandatory)][string]$RepoRoot,
    [Parameter(Mandatory)][string]$StagingRoot,
    [Parameter(Mandatory)][string]$OutputPath,
    [int]$MemoryInMB = 12288,
    [int]$CpuCount   = 8
)

$ErrorActionPreference = 'Stop'

function Xml-Escape([string]$s) {
    if ($null -eq $s) { return '' }
    return $s.Replace('&', '&amp;').Replace('<', '&lt;').Replace('>', '&gt;').Replace('"', '&quot;')
}

$desktop = 'C:\Users\WDAGUtilityAccount\Desktop'

# 六条挂载。前五条只读, 最后一条可写 (沙箱往回传日志和截图)。
#   installers -> 待验安装包 (新包 + 升级基线)
#   scripts    -> 工装 (in-sandbox-claims.ps1 等)
#   lib        -> 共享模块 (装包/校验/PE 架构)
#   buildout   -> 仓库的 output\ (用来比对「注册指向的二进制 == 本次构建」)
#   tests      -> 主机侧测试 exe
#   artifacts  -> 沙箱内写日志/截图, 自动回传到主机
$mounts = @(
    @{ host = (Join-Path $StagingRoot 'installers'); sandbox = "$desktop\installers"; ro = $true },
    @{ host = $ToolingDir;                   sandbox = "$desktop\scripts";    ro = $true },
    @{ host = (Join-Path $ToolingDir 'lib');  sandbox = "$desktop\lib";        ro = $true },
    @{ host = (Join-Path $RepoRoot 'output');  sandbox = "$desktop\buildout";   ro = $true },
    @{ host = (Join-Path $StagingRoot 'tests'); sandbox = "$desktop\tests";     ro = $true },
    @{ host = $StagingRoot;                   sandbox = "$desktop\artifacts";  ro = $false }
)

foreach ($m in $mounts) {
    if (-not (Test-Path $m.host)) {
        throw "挂载源不存在: $($m.host)"
    }
}

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine('<Configuration>')
[void]$sb.AppendLine('  <MappedFolders>')
foreach ($m in $mounts) {
    [void]$sb.AppendLine('    <MappedFolder>')
    [void]$sb.AppendLine("      <HostFolder>$(Xml-Escape $m.host)</HostFolder>")
    [void]$sb.AppendLine("      <SandboxFolder>$(Xml-Escape $m.sandbox)</SandboxFolder>")
    [void]$sb.AppendLine("      <ReadOnly>$($m.ro.ToString().ToLower())</ReadOnly>")
    [void]$sb.AppendLine('    </MappedFolder>')
}
[void]$sb.AppendLine('  </MappedFolders>')
[void]$sb.AppendLine("  <MemoryInMB>$MemoryInMB</MemoryInMB>")
[void]$sb.AppendLine("  <CpuCount>$CpuCount</CpuCount>")
# 沙箱需要联网来跑 E.4 那条断言 (外网可达性), 所以这里保持 Enable。
[void]$sb.AppendLine('  <Networking>Enable</Networking>')
[void]$sb.AppendLine('  <LogonCommand>')
[void]$sb.AppendLine('    <Command>cmd /c "powershell -NoProfile -ExecutionPolicy Bypass -File &quot;C:\Users\WDAGUtilityAccount\Desktop\scripts\in-sandbox-claims.ps1&quot; 1&gt;&quot;C:\Users\WDAGUtilityAccount\Desktop\artifacts\logs\boot.log&quot; 2&gt;&amp;1"</Command>')
[void]$sb.AppendLine('  </LogonCommand>')
[void]$sb.AppendLine('</Configuration>')

$outDir = Split-Path $OutputPath -Parent
if (-not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir -Force | Out-Null }
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[IO.File]::WriteAllText($OutputPath, $sb.ToString(), $utf8NoBom)

Write-Host "[OK] sandbox config written: $OutputPath" -ForegroundColor DarkGray
foreach ($m in $mounts) {
    Write-Host ("     {0,-11} <- {1}" -f (Split-Path $m.sandbox -Leaf), $m.host) -ForegroundColor DarkGray
}
