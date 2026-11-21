$ErrorActionPreference = 'Stop'
$ck = 'F:\soft\00selfmade\rime_claude\docs\release\restart-checklist-1.3.6.md'

$b = [System.IO.File]::ReadAllBytes($ck)
$hasBom = ($b.Length -ge 3 -and $b[0] -eq 0xEF -and $b[1] -eq 0xBB -and $b[2] -eq 0xBF)
$txt = [System.IO.File]::ReadAllText($ck)
$eol = "`n"
if ($txt.Contains("`r`n")) { $eol = "`r`n" }
$L = [System.IO.File]::ReadAllLines($ck)
$before = $L.Count
Write-Host ('CK_LINES_BEFORE=' + $before + ' CK_BOM=' + $hasBom + ' CK_CRLF=' + ($eol -eq "`r`n"))

function Find-One([string[]]$arr, [string]$needle, [string]$tag) {
    $idx = @()
    for ($i = 0; $i -lt $arr.Count; $i++) { if ($arr[$i].Contains($needle)) { $idx += $i } }
    if ($idx.Count -ne 1) { throw ('anchor ' + $tag + ' hits=' + $idx.Count + ' needle=[' + $needle + ']') }
    Write-Host ('ANCHOR_' + $tag + '_AT=' + $idx[0] + ' RAW=[' + $arr[$idx[0]] + ']')
    return $idx[0]
}

$iCrit4  = Find-One $L '| A-4 | 未登录态 |'                'CRIT4'
$iCrit5  = Find-One $L '| A-5 | 未登录态底部留白可接受 |'   'CRIT5'
$iResult = Find-One $L '| A-4 / A-5 | **未测试** |'        'RESULT'
if ($iResult -le $iCrit5) { throw 'row order unexpected: result row must come after criteria rows' }

$newCrit4 = '| A-4 | 未登录态 | 换钥提示行**不可见**。⚠️ 原判据「**不出现**该行（枚举不到即通过）」于批次 BF 被**否证**：`CloudAccountDialog.cpp:1221` 的 `ApplyLayout` 对不在布局表的 id 只 `ShowWindow(SW_HIDE)` 不销毁句柄 ⇒ 控件仍可被枚举到。有效判据 = 句柄存在但 `IsWindowVisible=false`**且**可见控件 id 集合逐字等于未登录布局表（两项合取，任缺其一即无判别力） |'

$newCrit5 = '| A-5 | 未登录态底部留白可接受 | `AccountClientSize` 按窗口角色统一取高度 ⇒ 两态**同高**（96dpi 设计值 290 px；批次 BF 在 168dpi 实测两态客户区均 `840×508`），未登录态底部大留白不得把「关闭」按钮挤出窗外或裁切。判据须用**实测 rect + 逐行墨迹剖面**，不得用「base 常量 ×DPI」反推冒充实测 |'

$newResult = '| A-4 / A-5 | **PASS**（批次 BF · 2026-11-21 · 本机 **175% 缩放 168dpi** · 真机控件级实测，**账号态已复原**） | A-4：登出后 `1035` 句柄仍在但 `IsWindowVisible=false`（`A4_1035_PRESENT_HIDDEN=True`）、可见 id 集合逐字 = 未登录 12 项表（`A4_MISSING=[] A4_EXTRA=[]`）、换布局后 **0.5 s** 内即隐藏 ⇒ 四独立轮次结论一致。A-5：两态客户区高**相等** `508`；`1006` 底 `329` → `1010` 顶 `438` = 留白 **109 px**、`1010` 底 `487` → 窗底 `508` = **21 px**，逐行墨迹最后墨行 `485` 距窗底 **22 行** ⇒ 高缩放未裁；12 项逐 id 几何 11 项逐字相符，`1018` 历史下拉高 `29` ≠ `MulDiv96(26,168)=46` 已源码定性为**探针期望表口径**（`CloudAccountDialog.cpp:1014-1015` combo 样式 `CBS_DROPDOWNLIST \| WS_VSCROLL` ⇒ 可见高由系统按字体自定、传入 height 不被遵守；x/y/w 相符且同 DPI 其余 11 项逐字相符）**非产品缺陷**。取证 `reports/fluxing-bf-a4-a5/`（`baseline-*.txt` / `logout-run.txt` / `relogin-run.txt` / `a5-run3.txt` / `a5-logout-*.txt` / `evidence-*.png`）；详见 `docs/archive/handoffs/handoff-2026-11-21-batch-bf-a4-a5-logout-measured.md` |'

$L[$iCrit4] = $newCrit4
$L[$iCrit5] = $newCrit5
$L[$iResult] = $newResult

if ($L.Count -ne $before) { throw ('line count changed: ' + $L.Count + ' vs ' + $before) }
$enc = New-Object System.Text.UTF8Encoding($hasBom)
[System.IO.File]::WriteAllText($ck, (($L -join $eol) + $eol), $enc)

$R = [System.IO.File]::ReadAllLines($ck)
$raw = [System.IO.File]::ReadAllText($ck)
Write-Host ('CK_LINES_AFTER=' + $R.Count + ' LINECOUNT_SAME=' + ($R.Count -eq $before))
Write-Host ('CK_HAS_A4VERDICT=' + $raw.Contains('A4_1035_PRESENT_HIDDEN=True'))
Write-Host ('CK_HAS_COMBO_NOTE=' + $raw.Contains('CBS_DROPDOWNLIST'))
Write-Host ('CK_HAS_CRITFIX=' + $raw.Contains('两项合取'))
Write-Host ('CK_OLD_RESULT_GONE=' + (-not $raw.Contains('| A-4 / A-5 | **未测试** |')))
Write-Host ('CK_OLD_CRIT4_GONE=' + (-not $raw.Contains('**不出现**该行（`AccountVisibleIds` 按登录态过滤） |')))
$b2 = [System.IO.File]::ReadAllBytes($ck)
Write-Host ('CK_BOM_PRESERVED=' + ($b2[0] -eq 0xEF -and $b2[1] -eq 0xBB -and $b2[2] -eq 0xBF) + ' BYTES=' + $b2.Length)
