# PeVersion.ps1 —— PE 版本资源的两层读法（L175 的教训）
#
# 为什么必须读两层：
#   1) VS_FIXEDFILEINFO（二进制固定信息，签名 0xFEEF04BD）——链接器/rc 一起决定的数值版本；
#   2) StringFileInfo 的 FileVersion/ProductVersion —— rc 里 `VALUE "FileVersion", ...` 写入的字符串表；
#      .NET 的 (Get-Item x.exe).VersionInfo.FileVersion 读的**就是这一层**。
#
# L175 的缺陷恰好是「第 1 层对、第 2 层空」：只验固定版本信息的话，门禁永远绿，
# 而文档里那条核验命令永远返回空串。所以本文件把两层的值都吐出来，
# 让调用方自己决定断言哪一层（回归用例断言**两层都要对**）。
#
# 依赖：无（只用 System.IO / BitConverter / System.Diagnostics.FileVersionInfo）。

Set-StrictMode -Version Latest

# 直接解析 PE 的 VS_FIXEDFILEINFO，返回 'a.b.c.d'；无版本资源时返回 $null。
function Get-PeFixedVersion {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) { throw "PE not found: $Path" }
    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 64) { return $null }

    # Latin-1 让「字节 <-> 字符」一一对应，再用字符串 IndexOf 做原生加速搜索
    # （逐字节 PowerShell 循环在 2 MB 文件上要几秒，这里毫秒级）。
    $latin1 = [System.Text.Encoding]::GetEncoding(28591).GetString($bytes)
    # 签名 0xFEEF04BD 的小端字节序 = BD 04 EF FE
    $needle = [string]::new([char[]]@([char]0xBD, [char]0x04, [char]0xEF, [char]0xFE))
    $idx = $latin1.IndexOf($needle, [System.StringComparison]::Ordinal)
    if ($idx -lt 0 -or ($idx + 24) -gt $bytes.Length) { return $null }

    # 布局：dwSignature(0) dwStrucVersion(4) dwFileVersionMS(8) dwFileVersionLS(12) ...
    $ms = [System.BitConverter]::ToUInt32($bytes, $idx + 8)
    $ls = [System.BitConverter]::ToUInt32($bytes, $idx + 12)
    return ('{0}.{1}.{2}.{3}' -f (($ms -shr 16) -band 0xFFFF), ($ms -band 0xFFFF), (($ls -shr 16) -band 0xFFFF), ($ls -band 0xFFFF))
}

# 两层版本信息一起返回。
function Get-PeVersionInfo {
    param([Parameter(Mandatory = $true)][string]$Path)

    $fixed = Get-PeFixedVersion -Path $Path
    $vi = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($Path)

    # 空串与「只有空格」都算没写（L175 的表现是纯空串）。
    $strFile = ''
    if ($null -ne $vi.FileVersion) { $strFile = $vi.FileVersion.Trim() }
    $strProduct = ''
    if ($null -ne $vi.ProductVersion) { $strProduct = $vi.ProductVersion.Trim() }

    return [pscustomobject]@{
        Path                 = $Path
        FixedVersion         = $fixed
        StringFileVersion    = $strFile
        StringProductVersion = $strProduct
        HasVersionResource   = ($null -ne $fixed)
        LayersAgree          = ($null -ne $fixed) -and ($strFile -eq $fixed)
    }
}
