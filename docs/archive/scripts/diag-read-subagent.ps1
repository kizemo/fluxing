$agents = @(
    @{id='af60513d9a1435fde'; name='alt-comma-regression'},
    @{id='a4eed3ff43ddd524e'; name='focusin-reality'},
    @{id='a9a6e38a0f3885abf'; name='quickpanel-trigger-matrix'}
)
foreach ($a in $agents) {
    $path = "C:\Users\Duanyi\.claude\projects\F--soft-00softmade-rime-claude\634bc624-36ec-4afe-9f8f-b3569bae7690\subagents\agent-$($a.id).jsonl"
    if (-not (Test-Path $path)) {
        $path = "C:\Users\Duanyi\.claude\projects\F--soft-00selfmade-rime-claude\634bc624-36ec-4afe-9f8f-b3569bae7690\subagents\agent-$($a.id).jsonl"
    }
    Write-Host ""
    Write-Host "=========================================="
    Write-Host "AGENT: $($a.name) ($($a.id))"
    Write-Host "FILE: $path"
    Write-Host "=========================================="
    $lines = Get-Content $path -Tail 5
    foreach ($line in $lines) {
        try {
            $d = $line | ConvertFrom-Json
            $content = $d.message.content
            if ($content -is [array]) {
                foreach ($c in $content) {
                    if ($c.type -eq 'text' -and $c.text) {
                        $text = $c.text
                        if ($text.Length -gt 8000) { $text = $text.Substring(0, 8000) + '...[TRUNCATED]' }
                        Write-Host '---ASSISTANT TEXT---'
                        Write-Host $text
                    }
                    elseif ($c.type -eq 'tool_use') {
                        Write-Host ("---TOOL_USE: " + $c.name + "---")
                        Write-Host ($c.input | ConvertTo-Json -Depth 3 -Compress).Substring(0, [Math]::Min(500, ($c.input | ConvertTo-Json -Depth 3 -Compress).Length))
                    }
                }
            }
            elseif ($content -is [string]) {
                if ($content.Length -gt 4000) { $content = $content.Substring(0, 4000) + '...[TRUNCATED]' }
                Write-Host '---MESSAGE STRING---'
                Write-Host $content
            }
            Write-Host ("---END type:" + $d.type + "---")
        }
        catch {
            Write-Host "ERR: $($_.Exception.Message)"
        }
    }
}