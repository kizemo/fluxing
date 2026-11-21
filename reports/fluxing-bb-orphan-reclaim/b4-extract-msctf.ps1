# B-4 path2 prep: extract TSF interface method signatures (with line numbers) from
# both local Windows SDK copies of msctf.h. Read-only w.r.t. the SDK.
$ErrorActionPreference = 'Stop'

$out = 'F:\soft\00selfmade\rime_claude\reports\fluxing-bb-orphan-reclaim\b4-msctf-signatures-raw.txt'
$want = @(
    'ITfThreadMgr', 'ITfThreadMgrEx', 'ITfDocumentMgr',
    'ITfInputProcessorProfiles', 'ITfInputProcessorProfileMgr',
    'ITfActiveLanguageProfileNotifySink', 'ITfSource'
)
$guids = @(
    'CLSID_TF_ThreadMgr', 'CLSID_TF_InputProcessorProfiles',
    'IID_ITfThreadMgr', 'IID_ITfInputProcessorProfiles',
    'IID_ITfInputProcessorProfileMgr', 'IID_ITfActiveLanguageProfileNotifySink',
    'IID_ITfSource'
)

$sb = New-Object System.Text.StringBuilder

foreach ($ver in '10.0.19041.0', '10.0.26100.0') {
    $h = "C:\Program Files (x86)\Windows Kits\10\Include\$ver\um\msctf.h"
    $lines = Get-Content -LiteralPath $h
    $hdr = '################ ' + $ver + '  (' + $h + ')  lines=' + $lines.Count + ' ################'
    [void]$sb.AppendLine($hdr)

    $cur = '(file scope)'
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $L = $lines[$i]

        if ($L -match 'MIDL_INTERFACE\("([0-9a-fA-F\-]+)"\)') {
            $guid = $Matches[1]
            $j = $i + 1
            while ($j -lt $lines.Count -and $lines[$j].Trim() -eq '') { $j++ }
            if ($lines[$j] -match '^\s*(I\w+)\s*:\s*public\s+(\w+)') {
                $cur = ('{0} : {1}   [MIDL_INTERFACE GUID {2}] declL{3}' -f $Matches[1], $Matches[2], $guid, ($i + 1))
            }
        }

        $iname = ($cur -split ' ')[0]

        if ($iname -in $want -and $L -match 'STDMETHODCALLTYPE\s+(\w+)\s*\(') {
            $meth = $Matches[1]
            $sig = New-Object System.Collections.Generic.List[string]
            $k = $i
            while ($k -lt $lines.Count -and ($k - $i) -le 6) {
                $t = $lines[$k]
                $t = ($t -replace '/\*[^*]*\*/', '')
                $t = ($t -replace '__RPC__\w*', '')
                $t = ($t -replace '_(Outptr_|out_ptr|Out_|In_)\w*', '')
                $t = ($t -replace '\[annotation\]\[iid_is\]\[out\]', '')
                $t = ($t -replace '_Outptr_result_maybenull_|_Outptr_opt_|_In_opt_|_In_|_Out_|_Outptr_', '')
                $sig.Add($t.Trim())
                if ($t.Trim() -match '[;=]\s*$') { break }
                $k++
            }
            $one = (($sig -join ' ') -replace '\s+', ' ').Trim()
            $line = ('[{0}] L{1}: {2}' -f $iname, ($i + 1), $one)
            [void]$sb.AppendLine($line)
        }

        foreach ($g in $guids) {
            if ($L -match ('EXTERN_C const \w+\s+' + $g + '\b')) {
                [void]$sb.AppendLine('[globals] L' + ($i + 1) + ': ' + $L.Trim())
            }
            if ($L -match ('\b' + $g + '\s*=\s*\{')) {
                $body = $L.Trim() + ' ' + ($lines[($i + 1)..([Math]::Min($i + 2, $lines.Count - 1))] -join ' ')
                [void]$sb.AppendLine('[guiddef] L' + ($i + 1) + ': ' + (($body -replace '\s+', ' ').Trim()))
            }
        }
    }
    [void]$sb.AppendLine('')
}

$sb.ToString() | Set-Content -LiteralPath $out -Encoding UTF8
Write-Output ('WROTE ' + $out + ' bytes=' + (Get-Item $out).Length)
