# gen-keysyms.ps1 -- regenerate keysyms_generated.inc from librime's key table.
#
# Why this exists
# ---------------
# The TSF hotkey acceptance gate (TestHotkeyBindings) has to turn a librime
# key name ("apostrophe", "Page_Up", "Shift+comma", ...) into the exact
# (keycode, modifier) pair librime will look up. librime resolves those
# names in librime/src/rime/key_table.cc:
#
#   RimeGetKeycodeByName(name)  -> walks keys_by_keyval[], first hit wins
#   RimeGetModifierByName(name) -> 1 << index in the modifier_name[] table
#
# Hand-copying that table would let it silently drift from librime. This
# script regenerates it mechanically from the same source file, then
# verifies the result against the canonical X11 keysym mapping so a
# parsing bug here cannot masquerade as a passing gate.
#
# ASCII only on purpose: PowerShell 5.1 reads BOM-less scripts as GBK, so
# a non-ASCII byte in this file would be decoded into mojibake.
#
# Usage (from anywhere):
#   powershell -File test\TestHotkeyBindings\gen-keysyms.ps1
#   powershell -File test\TestHotkeyBindings\gen-keysyms.ps1 -Check
#
# -Check regenerates into memory and compares with the committed .inc;
# it exits 1 when they differ (used by CI to prove no drift).

param(
  [switch]$Check
)

$ErrorActionPreference = 'Stop'

# --- locate the repo root (this script lives in test/TestHotkeyBindings) ---
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $here '..\..')).Path
$src = Join-Path $repoRoot 'librime\src\rime\key_table.cc'
$out = Join-Path $repoRoot 'include\keysyms_generated.inc'

if (-not (Test-Path $src)) { throw "key_table.cc not found: $src" }

$t = [System.IO.File]::ReadAllText($src, [System.Text.Encoding]::UTF8)

# ------------------------------------------------------------------
# 1. key_names[] -- the NUL separated list of every key name librime knows
# ------------------------------------------------------------------
$nmStart = $t.IndexOf('static const char key_names[] =')
if ($nmStart -lt 0) { throw 'key_names[] not found in key_table.cc' }
$nmEnd = $t.IndexOf(';', $nmStart)
$nmBlock = $t.Substring($nmStart, $nmEnd - $nmStart)
$quoted = [regex]::Matches($nmBlock, '"((?:[^"\\]|\\.)*)"')
$names = New-Object System.Collections.Generic.List[string]
foreach ($m in $quoted) {
  $s = $m.Groups[1].Value -replace '\\0', "`0" -replace '\\n', "`n" -replace '\\t', "`t" -replace '\\"', '"' -replace '\\\\', '\'
  foreach ($part in ($s -split "`0")) {
    if ($part.Length -gt 0) { $names.Add($part) }
  }
}

# Rebuild the flat byte image so the table offsets can be resolved.
$flat = New-Object System.Text.StringBuilder
foreach ($n in $names) {
  [void]$flat.Append($n)
  [void]$flat.Append("`0")
}
$flatText = $flat.ToString()

# ------------------------------------------------------------------
# 2. keys_by_keyval[] -- { keyval, offset into key_names }
# ------------------------------------------------------------------
$kvStart = $t.IndexOf('static const key_entry keys_by_keyval[] = {')
if ($kvStart -lt 0) { throw 'keys_by_keyval[] not found in key_table.cc' }
$kvEnd = $t.IndexOf('};', $kvStart)
$kvBlock = $t.Substring($kvStart, $kvEnd - $kvStart)
$entries = [regex]::Matches($kvBlock, '\{([^}]*)\}')

# name -> keycode. First occurrence wins, exactly like the linear scan in
# RimeGetKeycodeByName() (it returns the first keys_by_keyval entry whose
# name matches, and no name appears twice in that table).
$map = [ordered]@{}
# The same entries in librime's own table order: the first entry that
# carries a given key code is the name RimeGetKeyName() returns for it.
$ordered = New-Object System.Collections.Generic.List[object]
foreach ($e in $entries) {
  $nums = [regex]::Matches($e.Groups[1].Value, '0x[0-9a-fA-F]+|\d+')
  if ($nums.Count -lt 2) { continue }
  $raw = $nums[0].Value
  if ($raw.StartsWith('0x') -or $raw.StartsWith('0X')) {
    $keyval = [int][Convert]::ToInt32($raw.Substring(2), 16)
  } else {
    $keyval = [int]$raw
  }
  $off = [int]$nums[1].Value
  if ($off -lt 0 -or $off -ge $flatText.Length) { continue }
  $rest = $flatText.Substring($off)
  $end = $rest.IndexOf("`0")
  if ($end -lt 0) { continue }
  $name = $rest.Substring(0, $end)
  if (-not $map.Contains($name)) {
    $map[$name] = $keyval
    $ordered.Add([pscustomobject]@{ name = $name; keycode = $keyval })
  }
}

# ------------------------------------------------------------------
# 3. modifier_name[] -- name -> 1 << index (NULL holes keep their index)
#    The table has NULL holes between "Button5" and "Super", so the
#    index must be counted over every element, quoted or not.
# ------------------------------------------------------------------
$mdStart = $t.IndexOf('static const char* modifier_name[] = {')
if ($mdStart -lt 0) { throw 'modifier_name[] not found in key_table.cc' }
$mdEnd = $t.IndexOf('};', $mdStart)
$mdBlock = $t.Substring($mdStart, $mdEnd - $mdStart)
$mdBody = $mdBlock.Substring($mdBlock.IndexOf('{') + 1)
$mdBody = [regex]::Replace($mdBody, '//[^\r\n]*', '')
$modTable = New-Object System.Collections.Generic.List[object]
$idx = 0
foreach ($el in $mdBody.Split(',')) {
  $el = $el.Trim()
  if ($el -match '^"((?:[^"\\]|\\.)*)"$') {
    $modTable.Add([pscustomobject]@{ name = $matches[1]; mask = [int](1 -shl $idx) })
  }
  $idx++
}

# Self verification -- librime's own mask constants (librime/src/rime/key_table.h)
$canonicalMods = [ordered]@{
  'Shift' = 0x1; 'Lock' = 0x2; 'Control' = 0x4; 'Alt' = 0x8
  'Mod2' = 0x10; 'Mod3' = 0x20; 'Mod4' = 0x40; 'Mod5' = 0x80
  'Button1' = 0x100; 'Button2' = 0x200; 'Button3' = 0x400
  'Button4' = 0x800; 'Button5' = 0x1000
  'Super' = 0x4000000; 'Hyper' = 0x8000000; 'Meta' = 0x10000000
  'Release' = 0x40000000
}
$modMap = @{}
foreach ($m in $modTable) { $modMap[$m.name] = [int]$m.mask }
$badMod = @()
foreach ($k in $canonicalMods.Keys) {
  if (-not $modMap.ContainsKey($k)) { $badMod += "$k missing"; continue }
  if ($modMap[$k] -ne [int]$canonicalMods[$k]) {
    $badMod += ("{0} = 0x{1:X}, expected 0x{2:X}" -f $k, $modMap[$k], [int]$canonicalMods[$k])
  }
}
if ($badMod.Count -gt 0) {
  throw ("modifier self-verification failed:`n  " + ($badMod -join "`n  "))
}

# ------------------------------------------------------------------
# 4. Self verification -- canonical X11 keysyms for the Latin-1 range.
#    If the offset arithmetic above were off by one, these would break.
# ------------------------------------------------------------------
$canonical = [ordered]@{
  'space' = 0x20; 'exclam' = 0x21; 'quotedbl' = 0x22; 'numbersign' = 0x23
  'dollar' = 0x24; 'percent' = 0x25; 'ampersand' = 0x26; 'apostrophe' = 0x27
  'parenleft' = 0x28; 'parenright' = 0x29; 'asterisk' = 0x2A; 'plus' = 0x2B
  'comma' = 0x2C; 'minus' = 0x2D; 'period' = 0x2E; 'slash' = 0x2F
  '0' = 0x30; '1' = 0x31; '2' = 0x32; '3' = 0x33; '4' = 0x34; '5' = 0x35
  '6' = 0x36; '7' = 0x37; '8' = 0x38; '9' = 0x39; 'colon' = 0x3A
  'semicolon' = 0x3B; 'less' = 0x3C; 'equal' = 0x3D; 'greater' = 0x3E
  'question' = 0x3F; 'at' = 0x40; 'A' = 0x41; 'Z' = 0x5A; 'bracketleft' = 0x5B
  'backslash' = 0x5C; 'bracketright' = 0x5D; 'asciicircum' = 0x5E
  'underscore' = 0x5F; 'grave' = 0x60; 'braceleft' = 0x7B; 'bar' = 0x7C
  'braceright' = 0x7D; 'asciitilde' = 0x7E
}
$bad = @()
foreach ($k in $canonical.Keys) {
  if (-not $map.Contains($k)) { $bad += "$k missing" ; continue }
  if ([int]$map[$k] -ne [int]$canonical[$k]) {
    $bad += ("{0} = 0x{1:X}, expected 0x{2:X}" -f $k, [int]$map[$k], [int]$canonical[$k])
  }
}
if ($bad.Count -gt 0) {
  throw ("keysym self-verification failed:`n  " + ($bad -join "`n  "))
}

# ------------------------------------------------------------------
# 5. Emit the .inc
# ------------------------------------------------------------------
$lines = New-Object System.Collections.Generic.List[string]
$lines.Add('// keysyms_generated.inc -- AUTO-GENERATED by gen-keysyms.ps1. DO NOT EDIT.')
$lines.Add('//')
$lines.Add('// Source of truth: librime/src/rime/key_table.cc')
$lines.Add('//   KeyNames()      -> librime RimeGetKeycodeByName() / RimeGetKeyName()')
$lines.Add('//   ModifierNames() -> librime RimeGetModifierByName() (1 << index)')
$lines.Add('//')
$lines.Add('// The key names keep librime''s own table order, so the first entry')
$lines.Add('// carrying a key code is the name RimeGetKeyName() returns for it.')
$lines.Add('//')
$lines.Add('// Regenerate with:  powershell -File test\TestHotkeyBindings\gen-keysyms.ps1')
$lines.Add('')
$lines.Add('#ifndef FLUXING_KEYSYMS_GENERATED_INC_')
$lines.Add('#define FLUXING_KEYSYMS_GENERATED_INC_')
$lines.Add('')
$lines.Add('namespace fluxing {')
$lines.Add('namespace keysyms {')
$lines.Add('')
$lines.Add('struct NamedKey { const char* name; int keycode; };')
$lines.Add('struct NamedModifier { const char* name; int mask; };')
$lines.Add('')
$lines.Add('// The key names librime knows, in librime''s own table order.')
$lines.Add('inline const NamedKey* KeyNames(size_t* count) {')
$lines.Add('  static const NamedKey table[] = {')
foreach ($k in $ordered) {
  $lines.Add('    {"' + $k.name + '", 0x' + ([int]$k.keycode).ToString('X') + '},')
}
$lines.Add('  };')
$lines.Add('  *count = sizeof(table) / sizeof(table[0]);')
$lines.Add('  return table;')
$lines.Add('}')
$lines.Add('')
$lines.Add('// The modifier names librime knows, with librime''s own masks.')
$lines.Add('inline const NamedModifier* ModifierNames(size_t* count) {')
$lines.Add('  static const NamedModifier table[] = {')
foreach ($m in $modTable) {
  $lines.Add('    {"' + $m.name + '", 0x' + ([int]$m.mask).ToString('X') + '},')
}
$lines.Add('  };')
$lines.Add('  *count = sizeof(table) / sizeof(table[0]);')
$lines.Add('  return table;')
$lines.Add('}')
$lines.Add('')
$lines.Add('}  // namespace keysyms')
$lines.Add('}  // namespace fluxing')
$lines.Add('')
$lines.Add('#endif  // FLUXING_KEYSYMS_GENERATED_INC_')
$text = ($lines -join "`r`n") + "`r`n"

if ($Check) {
  if (-not (Test-Path $out)) { throw "committed $out is missing" }
  $cur = [System.IO.File]::ReadAllText($out, [System.Text.Encoding]::UTF8)
  if ($cur -ne $text) {
    Write-Error 'keysyms_generated.inc is out of date -- run gen-keysyms.ps1'
    exit 1
  }
  Write-Host ('OK: keysyms_generated.inc is up to date ({0} key names, {1} modifiers)' -f $map.Count, $modTable.Count)
  exit 0
}

[System.IO.File]::WriteAllText($out, $text, (New-Object System.Text.UTF8Encoding($false)))
Write-Host ('wrote {0} key names and {1} modifiers to {2}' -f $map.Count, $modTable.Count, $out)
