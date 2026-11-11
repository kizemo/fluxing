# 验证码发信通道 · Resend 一键上线脚本
#
# 用法（从仓库根目录执行）：
#   1) 把 Resend API key 存成纯文本文件（一行，无多余空格）：
#        notepad F:\soft\00selfmade\rime_claude\tmp\resend-key.txt
#   2) 执行：
#        pwsh -NoProfile -ExecutionPolicy Bypass -File scripts\cloud\resend-go-live.ps1 -TestEmail kizemo@163.com
#
# 脚本做四件事：
#   ① 校验并注入 Workers Secret MAIL_HTTP_API_KEY（**绝不回显 key**）
#   ② 把 wrangler.toml 的 MAIL_MODE 从 outbox 切成 http
#   ③ wrangler deploy（输出 Version ID）
#   ④ GET https://api.aiec.fun/v1/health 巡活 + 可选直接调 Resend API 发一封自检信
#
# 幂等性：② 检测到已是 http 则跳过并提示；① 重复执行只是覆盖同一个 Secret。
# 回滚：把 wrangler.toml 的 MAIL_MODE 改回 "outbox" 再 deploy 即可。

[CmdletBinding()]
param(
    [string]$KeyFile = 'F:\soft\00selfmade\rime_claude\tmp\resend-key.txt',
    [string]$TestEmail = '',
    [switch]$SkipTestMail
)

$ErrorActionPreference = 'Stop'

$repo = 'F:\soft\00selfmade\rime_claude'
$pkg = Join-Path $repo 'cloud\fluxing-sync'
$wranglerJs = Join-Path $pkg 'node_modules\wrangler\bin\wrangler.js'
$tomlPath = Join-Path $pkg 'wrangler.toml'

function Step([string]$m) { Write-Host "==> $m" }
function Ok([string]$m) { Write-Host "    [OK] $m" }
function Warn([string]$m) { Write-Host "    [!!] $m" }

# ---------- ① 读取并校验密钥 ----------
if (-not (Test-Path -LiteralPath $KeyFile)) {
    throw "找不到密钥文件：$KeyFile —— 请先把 Resend API key 存进去（一行纯文本）"
}
$key = ([IO.File]::ReadAllText($KeyFile)).Trim()
if ($key.Length -lt 20) { throw '密钥内容异常：长度过短' }
if (-not $key.StartsWith('re_')) { throw '密钥内容异常：应以 re_ 开头' }
Ok "已读取密钥文件，长度 $($key.Length)，前缀 re_"

Set-Location $pkg
if (-not (Test-Path -LiteralPath $wranglerJs)) {
    throw "找不到 wrangler：$wranglerJs —— 先在该目录执行 npm install"
}

# ---------- ② 注入 Workers Secret ----------
Step '注入 Workers Secret：MAIL_HTTP_API_KEY'
$key | & node $wranglerJs secret put MAIL_HTTP_API_KEY
if ($LASTEXITCODE -ne 0) { throw "secret put 失败，退出码 $LASTEXITCODE" }
Ok 'Secret 已写入 Worker（值不在输出里）'

# ---------- ③ 切换 MAIL_MODE = http ----------
Step '切换 wrangler.toml 的 MAIL_MODE = http'
$text = [Text.Encoding]::UTF8.GetString([IO.File]::ReadAllBytes($tomlPath))
if ($text -match 'MAIL_MODE\s*=\s*"http"') {
    Warn 'MAIL_MODE 已是 http，跳过切换'
} elseif ($text -match 'MAIL_MODE\s*=\s*"outbox"') {
    $new = [Text.RegularExpressions.Regex]::Replace($text, 'MAIL_MODE\s*=\s*"outbox"', 'MAIL_MODE = "http"')
    [IO.File]::WriteAllBytes($tomlPath, [Text.Encoding]::UTF8.GetBytes($new))
    Ok 'MAIL_MODE 已切为 http'
} else {
    throw 'wrangler.toml 里既不是 outbox 也不是 http，请人工确认后重跑'
}

# ---------- ④ 部署 ----------
Step 'wrangler deploy'
& node $wranglerJs deploy
if ($LASTEXITCODE -ne 0) { throw "deploy 失败，退出码 $LASTEXITCODE" }
Ok '部署完成（Version ID 见上方输出）'

# ---------- ⑤ 巡活 ----------
Step '巡活 GET https://api.aiec.fun/v1/health'
$health = Invoke-WebRequest -Uri 'https://api.aiec.fun/v1/health' -SkipHttpErrorCheck
Write-Host "    HEALTH_STATUS=$($health.StatusCode)"
if ($health.StatusCode -ne 200) { throw "健康检查失败：$($health.StatusCode)" }
Ok '服务在线'

# ---------- ⑥ 可选：直接用 Resend API 发一封自检信 ----------
if ($TestEmail -and -not $SkipTestMail) {
    Step "直接调 Resend API 向 $TestEmail 发一封自检信"
    $payload = @{
        from    = '火流猩输入法 <no-reply@aiec.fun>'
        to      = @($TestEmail)
        subject = '火流猩输入法 · 发信通道自检'
        text    = "这是一封发信通道自检邮件。收到即说明 Resend 通道已通。`n时间：$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
    } | ConvertTo-Json -Depth 5
    $resp = Invoke-WebRequest -Uri 'https://api.resend.com/emails' -Method Post `
        -Headers @{ Authorization = "Bearer $key"; 'content-type' = 'application/json' } `
        -Body ([Text.Encoding]::UTF8.GetBytes($payload)) -SkipHttpErrorCheck
    Write-Host "    RESEND_STATUS=$($resp.StatusCode)"
    Write-Host "    RESEND_BODY=$($resp.Content)"
    if ($resp.StatusCode -ne 200) {
        Warn '自检信未发送成功：上面的响应体里有原因（403 多半是域名未验证 / key 无权）'
        exit 1
    }
    Ok '自检信已交给 Resend，请查收邮件（含垃圾箱）'
}

Write-Host ''
Write-Host '全部步骤完成。剩余验证：客户端「账号与同步」页注册一次，确认验证码邮件收到。'
Write-Host '注意：切到 http 后 mail_outbox 不再写明文验证码，旧版 live-e2e-probe 的取码步骤已失效。'
