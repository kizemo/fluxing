# Get-InstallerType.ps1
# Identify EXE installer vendor by reading binary signature
# (NSIS / Inno Setup / InstallShield / Advanced Installer / Wise)
# Reads first 1MB byte-by-byte to avoid PowerShell GBK conversion (L01/A1)

function Get-InstallerType {
    param([Parameter(Mandatory)][string]$ExePath)

    if (-not (Test-Path $ExePath)) {
        throw "Installer not found: $ExePath"
    }

    try {
        $fs = [System.IO.File]::OpenRead($ExePath)
        $buf = New-Object byte[] (1048576)
        $read = $fs.Read($buf, 0, $buf.Length)
        $text = [System.Text.Encoding]::ASCII.GetString($buf, 0, $read)
        $fs.Dispose()
    } catch {
        return "Error:$($_.Exception.GetType().Name)"
    }

    # Order: most specific to most relaxed (Advanced Installer / Wise also common)
    if ($text -match 'Inno Setup')         { return 'InnoSetup' }
    if ($text -match 'Advanced Installer') { return 'AdvancedInstaller' }
    if ($text -match 'Wise Installer')     { return 'Wise' }
    if ($text -match 'InstallShield')      { return 'InstallShield' }
    if ($text -match 'Nullsoft Install')   { return 'NSIS' }
    return 'Unknown'
}
