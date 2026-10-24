<#
    normalize-encoding.ps1 - normalize .ps1 files in this skill to UTF-8 (with BOM) + CRLF.

    WHY:
        Windows PowerShell 5.1 reads a .ps1 file as ANSI (GBK on this machine) unless
        the file starts with a UTF-8 BOM. Any CJK string in a BOM-less script becomes
        mojibake and breaks the parser with "Unexpected token" before a single line
        of the script can run.

    USAGE:
        powershell -ExecutionPolicy Bypass -File scripts\normalize-encoding.ps1
        powershell -ExecutionPolicy Bypass -File scripts\normalize-encoding.ps1 -DryRun

    NOTE: this file is intentionally ASCII-only so it can run before the skill tree
          has been normalized.
#>
param(
    [string]$Root = "",
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'

if (-not $Root) {
    $Root = Split-Path -Parent $PSScriptRoot
}

function ConvertTo-Utf8BomCrlf([byte[]]$bytes) {
    $out = New-Object System.Collections.Generic.List[byte]
    for ($i = 0; $i -lt $bytes.Length; $i++) {
        if ($bytes[$i] -eq 0x0A -and -not ($i -gt 0 -and $bytes[$i - 1] -eq 0x0D)) {
            $out.Add(0x0D)
        }
        $out.Add($bytes[$i])
    }
    $body = $out.ToArray()
    if (-not ($body.Length -ge 3 -and $body[0] -eq 0xEF -and $body[1] -eq 0xBB -and $body[2] -eq 0xBF)) {
        $withBom = New-Object byte[] ($body.Length + 3)
        $withBom[0] = 0xEF; $withBom[1] = 0xBB; $withBom[2] = 0xBF
        [Array]::Copy($body, 0, $withBom, 3, $body.Length)
        $body = $withBom
    }
    return $body
}

$files = Get-ChildItem -Path $Root -Recurse -File -Include '*.ps1', '*.psm1' -Force
foreach ($f in $files) {
    $bytes = [System.IO.File]::ReadAllBytes($f.FullName)
    $fixed = ConvertTo-Utf8BomCrlf $bytes
    $same = ($fixed.Length -eq $bytes.Length)
    if ($same) {
        for ($i = 0; $i -lt $bytes.Length; $i++) {
            if ($bytes[$i] -ne $fixed[$i]) { $same = $false; break }
        }
    }
    if ($same) {
        Write-Host "SKIP  $($f.Name) (already UTF-8 BOM + CRLF)"
        continue
    }
    if (-not $DryRun) {
        [System.IO.File]::WriteAllBytes($f.FullName, $fixed)
    }
    Write-Host "FIX   $($f.Name)  ($($bytes.Length) -> $($fixed.Length) bytes)"
}

Write-Host ""
Write-Host ("Processed {0} file(s).{1}" -f $files.Count, $(if ($DryRun) { ' (dry run)' } else { '' }))
