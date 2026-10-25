# Test-PeArch.ps1
# 校验 PE 文件架构 (x86 / x64 / ARM64),防止 L14 坑
# (x64 Weasel*.exe + x86 rime.dll → 0xC000007B 启动崩溃)
#
# ★★ 2026-10-24 修正: 局部变量名不能叫 $expected ★★
#   原写法:
#       $actual   = Get-PeArch -Path $path
#       $expected = $Expected[$file]
#   PowerShell 变量名大小写不敏感 => $expected 与参数 $Expected 是同一个变量,
#   这一行把 [hashtable]$Expected 参数直接覆写成字符串 ('x86' / 'x64'),
#   触发 "Cannot convert the "x86" value of type System.String to type
#   System.Collections.Hashtable" 致命错误, 整个验证脚本当场终止。
#   而且即便不报错, $Expected 变成字符串后 "$Expected.Keys" 取不到东西,
#   foreach 提前结束 => 6 个文件只校验了 1 个, 架构校验形同虚设。
#   之前没暴露, 是因为第一轮沙箱里文件根本没装出来, 循环走 continue 分支,
#   从未执行到这一行。
#   修法: 局部变量改名 $expectedArch, 不再与参数同名。

function Get-PeArch {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Path)

    if (-not (Test-Path $Path)) { return $null }

    $b = [System.IO.File]::ReadAllBytes($Path)
    if ($b.Length -lt 0x40) { return $null }

    # PE 头偏移在 e_lfanew (0x3C 处)
    $peOff = [BitConverter]::ToInt32($b, 0x3C)
    if ($peOff -le 0 -or ($peOff + 6) -gt $b.Length) { return $null }

    # Machine 在 PE\ImageFileHeader.Machine (PE+4)
    $machine = [BitConverter]::ToUInt16($b, $peOff + 4)
    switch ($machine) {
        0x014C { return 'x86' }
        0x8664 { return 'x64' }
        0xAA64 { return 'ARM64' }
        default { return "0x$($machine.ToString('X4'))" }
    }
}

function Test-PeArch {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][hashtable]$Expected,
        [Parameter(Mandatory)][string]$Root
    )

    $failures = @()
    foreach ($file in $Expected.Keys) {
        $path = Join-Path $Root $file
        if (-not (Test-Path $path)) {
            $failures += "missing: $file"
            continue
        }
        $actual = Get-PeArch -Path $path
        $expectedArch = $Expected[$file]        # ← 不再与参数同名
        if ($actual -ne $expectedArch) {
            $failures += "$file arch = $actual (expected $expectedArch) - L14 mismatch"
        }
    }
    return $failures
}
