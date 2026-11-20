$ErrorActionPreference = 'Stop'
$repo = 'F:\soft\00selfmade\rime_claude'
$report = New-Object System.Collections.Generic.List[string]
function L([string]$s) { $report.Add($s) }

function Read-Bytes([string]$p) { [System.IO.File]::ReadAllBytes($p) }
function Detect-Bom([byte[]]$b) { ($b.Length -ge 3 -and $b[0] -eq 0xEF -and $b[1] -eq 0xBB -and $b[2] -eq 0xBF) }
function Detect-Eol([string]$t) { if ($t.Contains("`r`n")) { "`r`n" } else { "`n" } }

# ---------------- 1. version.txt ----------------
$vp = Join-Path $repo 'version.txt'
$vb = Read-Bytes $vp
$vbom = Detect-Bom $vb
$vt = [System.IO.File]::ReadAllText($vp)
$vold = $vt.Trim()
L "version.txt OLD=[$vold] BYTES=$($vb.Length) BOM=$vbom"
if ($vold -ne '1.3.6') { throw "version.txt 不是 1.3.6，停止：$vold" }
[System.IO.File]::WriteAllText($vp, '1.3.7', (New-Object System.Text.UTF8Encoding($vbom)))
$vafter = Read-Bytes $vp
L "version.txt NEW=[$([System.IO.File]::ReadAllText($vp))] BYTES=$($vafter.Length) BOM=$(Detect-Bom $vafter)"

# ---------------- 2. env.bat (set 行精确替换，注释不动) ----------------
$ep = Join-Path $repo 'env.bat'
$eb = Read-Bytes $ep
$ebom = Detect-Bom $eb
$et = [System.IO.File]::ReadAllText($ep)
$eeol = Detect-Eol $et
$envPairs = @(
  @{ old = 'set "FLUXING_VERSION=1.3.6"'; new = 'set "FLUXING_VERSION=1.3.7"' },
  @{ old = 'set "PRODUCT_VERSION=1.3.6.0"'; new = 'set "PRODUCT_VERSION=1.3.7.0"' },
  @{ old = 'set "FILE_VERSION=1.3.6.0"'; new = 'set "FILE_VERSION=1.3.7.0"' },
  @{ old = 'set "VERSION_PATCH=6"'; new = 'set "VERSION_PATCH=7"' }
)
foreach ($pr in $envPairs) {
  $cnt = ([regex]::Matches($et, [regex]::Escape($pr.old))).Count
  L "env.bat '$($pr.old)' hits=$cnt"
  if ($cnt -ne 1) { throw "env.bat 键位命中数异常：$($pr.old) => $cnt" }
  $et = $et.Replace($pr.old, $pr.new)
}
[System.IO.File]::WriteAllText($ep, $et, (New-Object System.Text.UTF8Encoding($ebom)))
$eafter = Read-Bytes $ep
L "env.bat NEWBYTES=$($eafter.Length) BOM=$(Detect-Bom $eafter) EOL=$(if($eeol -eq "`r`n"){'CRLF'}else{'LF'})"
foreach ($pr in $envPairs) {
  $t2 = [System.IO.File]::ReadAllText($ep)
  $c = ([regex]::Matches($t2, [regex]::Escape($pr.new))).Count
  L "env.bat after '$($pr.new)' hits=$c"
  if ($c -ne 1) { throw "env.bat 写入后键位异常：$($pr.new) => $c" }
}

# ---------------- 3. weasel.props ----------------
$pp = Join-Path $repo 'weasel.props'
$pb = Read-Bytes $pp
$pbom = Detect-Bom $pb
$pt = [System.IO.File]::ReadAllText($pp)
$propPairs = @(
  @{ old = '<VERSION_PATCH>6</VERSION_PATCH>'; new = '<VERSION_PATCH>7</VERSION_PATCH>' },
  @{ old = '<PRODUCT_VERSION>1.3.6.0</PRODUCT_VERSION>'; new = '<PRODUCT_VERSION>1.3.7.0</PRODUCT_VERSION>' },
  @{ old = '<FILE_VERSION>1.3.6.0</FILE_VERSION>'; new = '<FILE_VERSION>1.3.7.0</FILE_VERSION>' }
)
foreach ($pr in $propPairs) {
  $cnt = ([regex]::Matches($pt, [regex]::Escape($pr.old))).Count
  L "weasel.props '$($pr.old)' hits=$cnt"
  if ($cnt -ne 1) { throw "weasel.props 命中数异常：$($pr.old) => $cnt" }
  $pt = $pt.Replace($pr.old, $pr.new)
}
[System.IO.File]::WriteAllText($pp, $pt, (New-Object System.Text.UTF8Encoding($pbom)))
$pafter = Read-Bytes $pp
L "weasel.props NEWBYTES=$($pafter.Length) BOM=$(Detect-Bom $pafter)"

# ---------------- 4. touch 4 个 .rc ----------------
$rcs = @(
  'WeaselServer\WeaselServer.rc',
  'WeaselDeployer\WeaselDeployer.rc',
  'WeaselSetup\WeaselSetup.rc',
  'WeaselTSF\WeaselTSF.rc'
)
foreach ($r in $rcs) {
  $p = Join-Path $repo $r
  if (-not (Test-Path $p)) { throw "缺 .rc：$r" }
  $before = (Get-Item $p).LastWriteTime
  (Get-Item $p).LastWriteTime = Get-Date
  $after = (Get-Item $p).LastWriteTime
  L "TOUCH $r before=$before after=$after"
}

# ---------------- 5. 复核读回 ----------------
L '--- readback ---'
L "version.txt = [$([System.IO.File]::ReadAllText($vp))]"
foreach ($line in (Get-Content $ep | Where-Object { $_ -match '^\s*set\s+"(FLUXING_VERSION|PRODUCT_VERSION|FILE_VERSION|VERSION_MAJOR|VERSION_MINOR|VERSION_PATCH|RELEASE_BUILD)=' })) { L "env.bat: $line" }
foreach ($line in (Get-Content $pp | Where-Object { $_ -match 'VERSION' })) { L "props: $($line.Trim())" }

$log = Join-Path $repo 'reports\fluxing-bb-dpi-scale\bump-version-137.log'
[System.IO.File]::WriteAllText($log, (($report -join "`r`n") + "`r`n"), (New-Object System.Text.UTF8Encoding($false)))
'BUMP_OK'
$report
