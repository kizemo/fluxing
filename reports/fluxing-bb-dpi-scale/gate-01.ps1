$ErrorActionPreference = 'Continue'
$repo = 'F:\soft\00selfmade\rime_claude'
$log = Join-Path $repo 'reports\fluxing-bb-dpi-scale\gate-01.log'
$out = New-Object System.Collections.Generic.List[string]
function L([string]$s) { $out.Add($s) }

L "=== GATE-01 START $(Get-Date -Format o) ==="

$files = @(
  'WeaselServer\Cloud\CloudAccountUi.h',
  'WeaselServer\Cloud\CloudAccountDialog.cpp',
  'test\TestFluxingCloud\TestFluxingCloud.cpp'
)

# ---------- 0. locate clang-format ----------
$fmtScript = Join-Path $repo 'clang-format.ps1'
$fmtText = if (Test-Path $fmtScript) { [System.IO.File]::ReadAllText($fmtScript) } else { '' }
L "--- clang-format.ps1 exists=$(Test-Path $fmtScript) bytes=$($fmtText.Length) mentions_lines=$($fmtText -match 'lines') ---"
L (($fmtText -split "`r?`n" | Select-Object -First 45) -join "`n")

$cf = Get-Command clang-format -ErrorAction SilentlyContinue
$exe = $null
if ($cf) { $exe = $cf.Source; L "CLANG_FORMAT_FROM_PATH=$exe" }
else {
  $hit = Get-ChildItem -Path $repo -Recurse -Filter 'clang-format*.exe' -ErrorAction SilentlyContinue |
         Where-Object { $_.FullName -notlike '*\librime\*' } | Select-Object -First 1
  if ($hit) { $exe = $hit.FullName; L "CLANG_FORMAT_FROM_REPO=$exe" }
}
L "CLANG_FORMAT_EXE=$exe"

# ---------- 1. targeted clang-format on changed hunks only ----------
foreach ($f in $files) {
  $abs = Join-Path $repo $f
  $rel = $f.Replace('\', '/')
  $d = & git -C $repo diff -U0 -- $rel
  $ranges = @()
  foreach ($line in $d) {
    if ($line -match '^@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@') {
      $start = [int]$Matches[1]
      $cnt = if ($Matches[2]) { [int]$Matches[2] } else { 1 }
      if ($cnt -gt 0) { $ranges += "$start`:$($start + $cnt - 1)" }
    }
  }
  L "FORMAT_TARGET $f hunks=$($ranges.Count) ranges=$($ranges -join ',')"
  if ($ranges.Count -eq 0) { L "FORMAT_SKIP $f no-diff"; continue }
  if ($exe) {
    $cargs = @('-i')
    foreach ($r in $ranges) { $cargs += "--lines=$r" }
    $cargs += $abs
    $fo = & $exe @cargs 2>&1
    foreach ($x in $fo) { L "  fmt-out: $x" }
    L "FORMAT_EXIT $f = $LASTEXITCODE (mode=lines)"
  } else {
    $fo = & $fmtScript -i $abs 2>&1
    foreach ($x in $fo) { L "  fmt-out: $x" }
    L "FORMAT_EXIT $f = $LASTEXITCODE (mode=whole-file)"
  }
}
L '--- numstat after format ---'
foreach ($f in $files) {
  $n = & git -C $repo diff --numstat -- $f.Replace('\', '/')
  L "NUMSTAT $f => $n"
}

# ---------- 2. incremental full build ----------
$msb = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe'
$bl = & $msb (Join-Path $repo 'weasel.sln') /t:Build /p:Configuration=Release /p:Platform=Win32 "/p:SolutionDir=$repo\" /nologo /v:minimal 2>&1
L "MSBUILD_EXIT=$LASTEXITCODE"
$errLines = @(); $warnLines = @()
foreach ($x in $bl) {
  $s = [string]$x
  if ($s -match '(?i)error C\d|error LNK\d|error MSB\d|: error|错误') { $errLines += $s }
  elseif ($s -match '(?i)warning C\d|warning LNK\d|: warning|警告') { $warnLines += $s }
}
L "BUILD_ERROR_LINES=$($errLines.Count) BUILD_WARNING_LINES=$($warnLines.Count)"
$errLines | Select-Object -First 40 | ForEach-Object { L "  BUILD-ERR: $_" }
$warnLines | Select-Object -First 20 | ForEach-Object { L "  BUILD-WARN: $_" }
$bl | Select-Object -Last 12 | ForEach-Object { L "  build-tail: $_" }

# ---------- 3. cloud unit test ----------
# ⚠️ TestFluxingCloud **不在** weasel.sln 里（sln 构建完 exe mtime 仍是旧的）
#    ⇒ 必须显式构建该工程，否则跑的是上一批的 exe（本批新判据根本没执行）。
$tcProj = Join-Path $repo 'test\TestFluxingCloud\TestFluxingCloud.vcxproj'
$tcOut = & $msb $tcProj /t:Build /p:Configuration=Release /p:Platform=Win32 "/p:SolutionDir=$repo\" /nologo /v:minimal 2>&1
L "TESTCLOUD_MSBUILD_EXIT=$LASTEXITCODE"
$tcOut | Where-Object { $_ -match '(?i)error|警告|错误' } | Select-Object -First 20 | ForEach-Object { L "  TC-ERR: $_" }
$exeTest = Join-Path $repo 'Release\TestFluxingCloud.exe'
if (Test-Path $exeTest) {
  $srcM = (Get-Item (Join-Path $repo 'test\TestFluxingCloud\TestFluxingCloud.cpp')).LastWriteTime
  $exeM = (Get-Item $exeTest).LastWriteTime
  L "TEST_EXE_MTIME=$exeM SRC_MTIME=$srcM EXE_NEWER=$($exeM -ge $srcM)"
  $tu = & $exeTest 2>&1
  $tuExit = $LASTEXITCODE
  $tu | ForEach-Object { L "TEST: $_" }
  L "TEST_EXIT=$tuExit"
  $tu | Where-Object { $_ -match 'PASS=|FAIL=|VERDICT|FAILURES' } | ForEach-Object { L "TEST_SUMMARY: $_" }
} else {
  L 'TEST_EXE_MISSING'
}

# ---------- 4. other repo unit tests ----------
foreach ($t in @('Release\TestDefaultHotkeys.exe', 'Release\TestResponseParser.exe', 'Release\TestWeaselIPC.exe', 'Release\TestOrphanRecovery.exe')) {
  $p = Join-Path $repo $t
  if (Test-Path $p) {
    $o = & $p 2>&1
    $e = $LASTEXITCODE
    $s = ($o | Where-Object { $_ -match 'PASS=|FAIL=|VERDICT|OK$' } | Select-Object -Last 4) -join ' | '
    L "OTHERTEST $t EXIT=$e SUMMARY=$s"
  } else { L "OTHERTEST $t MISSING" }
}

# ---------- 5. L0 static check ----------
$l0 = Get-ChildItem -Path $repo -Recurse -Filter 'L0-static.ps1' -ErrorAction SilentlyContinue |
      Where-Object { $_.FullName -notlike '*\librime\*' } | Select-Object -First 1
if ($l0) {
  L "L0_SCRIPT=$($l0.FullName)"
  $lo = & pwsh -NoProfile -ExecutionPolicy Bypass -File $l0.FullName 2>&1
  $le = $LASTEXITCODE
  $lo | Select-Object -Last 25 | ForEach-Object { L "  L0: $_" }
  L "L0_EXIT=$le"
} else { L 'L0_SCRIPT_NOT_FOUND' }

L "=== GATE-01 END $(Get-Date -Format o) ==="

$utf8 = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($log, (($out -join "`r`n") + "`r`n"), $utf8)

'--- SUMMARY (stdout) ---'
$out | Where-Object { $_ -match 'CLANG_FORMAT_EXE=|FORMAT_EXIT|BUILD_ERROR_LINES|BUILD_WARNING_LINES|BUILD-ERR|MSBUILD_EXIT|TEST_SUMMARY|TEST_EXIT=|TEST_EXE_MTIME|OTHERTEST|L0_EXIT|L0_SCRIPT|NUMSTAT' }
