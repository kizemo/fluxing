# Copies the Windows-only shim headers (X11/keysym.h, utf8.h, darts.h) from
# tools/win-shims/include/ into librime/include/, where librime's CMake
# find_path() will discover them.
#
# librime/include/* is .gitignored (librime/.gitignore line "include/*"),
# so the shims live in this tracked directory in the parent repo and are
# copied into the (untracked) librime build tree at build time.
#
# Usage (from repo root):
#   powershell -ExecutionPolicy Bypass -File tools\win-shims\setup-shims.ps1
#
# Idempotent: re-running overwrites.

$ErrorActionPreference = 'Stop'

$scriptDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot   = Resolve-Path (Join-Path $scriptDir '..\..')
$srcInclude = Join-Path $scriptDir 'include'
$dstInclude = Join-Path $repoRoot 'librime\include'

if (-not (Test-Path $srcInclude)) {
    throw "Source shim directory not found: $srcInclude"
}
if (-not (Test-Path (Join-Path $repoRoot 'librime'))) {
    throw "librime submodule not found. Run: git submodule update --init --recursive"
}

New-Item -ItemType Directory -Force -Path $dstInclude | Out-Null

Copy-Item -Path (Join-Path $srcInclude 'X11')   -Destination $dstInclude -Recurse -Force
Copy-Item -Path (Join-Path $srcInclude 'utf8')  -Destination $dstInclude -Recurse -Force
Copy-Item -Path (Join-Path $srcInclude 'utf8.h')               -Destination $dstInclude -Force
Copy-Item -Path (Join-Path $srcInclude 'darts.h')              -Destination $dstInclude -Force
Copy-Item -Path (Join-Path $srcInclude 'COPYING.darts-clone')  -Destination $dstInclude -Force

Write-Host "Copied shim headers to $dstInclude"