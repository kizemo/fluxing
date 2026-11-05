# ASCII-only on purpose: PS 5.1 ParserError on UTF-8-no-BOM + CJK (lessons L181)
# WinForms host with a REAL Win32 EDIT control so an IME (Sogou) can attach a text context.
param(
  [string]$HwndFile = "F:\soft\00selfmade\rime_claude\tmp\spec099-probe\host-hwnd.txt"
)

$ErrorActionPreference = 'Continue'

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$form = New-Object System.Windows.Forms.Form
$form.Text = "FluxingIMEProbeHost"
$form.Width = 700
$form.Height = 400
$form.StartPosition = "Manual"
$form.Location = New-Object System.Drawing.Point 300, 200
$form.TopMost = $true

$tb = New-Object System.Windows.Forms.TextBox
$tb.Multiline = $true
$tb.AcceptsReturn = $true
$tb.Font = New-Object System.Drawing.Font("Microsoft YaHei", 14)
$tb.Dock = "Fill"
$form.Controls.Add($tb)

# NOTE: no Add_Shown handler -- an exception there killed the previous run.
# The trigger script discovers the EDIT child by class-name pattern instead.
$form.Add_Shown({ $tb.Focus() })

# write the form hwnd from a background watcher (no exception risk in the event handler)
$job = Start-Job -ScriptBlock {
  param($path)
  for ($i = 0; $i -lt 100; $i++) {
    $p = Get-Process -Id $PID -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 200
    if (-not $p) { break }
  }
} -ArgumentList $HwndFile

[void]$form.ShowDialog()
