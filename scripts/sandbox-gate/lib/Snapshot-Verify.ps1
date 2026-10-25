# Snapshot-Verify.ps1
# 沙箱内截图 + 写日志到挂载目录 (Sandbox 关机即丢,必须把 artifact 写回主机)

function Save-Screenshot {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Path
    )

    Add-Type -AssemblyName System.Windows.Forms
    Add-Type -AssemblyName System.Drawing

    $bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $bmp    = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
    $g      = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
    $bmp.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose()
    $bmp.Dispose()

    return (Get-Item $Path).Length
}

function Write-VerifyLog {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$Message,
        [ValidateSet('INFO','WARN','ERR','PASS','FAIL')]
        [string]$Level = 'INFO'
    )

    $line = "[$(Get-Date -Format 'HH:mm:ss')] [$Level] $Message"
    Write-Host $line
    # v6 fix (2026-10-24): the host side tails this log every 10 s while the
    #   sandbox is appending to it. Get-Content opens the file WITHOUT
    #   FileShare.Write, so one colliding read makes Add-Content throw
    #   "being used by another process" -- and the script-level trap then used
    #   to abandon the whole phase (6 unit-test assertions were lost that way).
    # Fix: open with FileShare.ReadWrite and retry on IOException.
    $bytes = (New-Object System.Text.UTF8Encoding($false)).GetBytes($line + [Environment]::NewLine)
    $written = $false
    for ($i = 0; $i -lt 20 -and -not $written; $i++) {
        try {
            $fs = New-Object System.IO.FileStream($Path, [System.IO.FileMode]::Append, [System.IO.FileAccess]::Write, [System.IO.FileShare]::ReadWrite)
            try {
                $fs.Write($bytes, 0, $bytes.Length)
                $fs.Flush()
            } finally { $fs.Dispose() }
            $written = $true
        } catch [System.IO.IOException] {
            Start-Sleep -Milliseconds (50 * ($i + 1))
        } catch {
            break
        }
    }
    if (-not $written) { Write-Warning ("verify log write failed: " + $Path) }
}

function Save-Artifact {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Source,
        [Parameter(Mandatory)][string]$DestDir,
        [string]$Name
    )

    if (-not (Test-Path $Source)) {
        Write-Warning "Artifact missing: $Source"
        return $null
    }
    if (-not (Test-Path $DestDir)) { New-Item -ItemType Directory -Path $DestDir -Force | Out-Null }
    if (-not $Name) { $Name = [System.IO.Path]::GetFileName($Source) }
    $dest = Join-Path $DestDir $Name
    Copy-Item -Path $Source -Destination $dest -Force
    return $dest
}