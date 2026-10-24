# v0.19.0.66 装机验证指南 (2026-07-26)

> **承接**: handoff-after-phase-L-bugfix-3-ship-v0.19.0.66-2026-07-25.md
> **新发现 (本次会话)**: v0.19.0.66 metadata 已补发上 SFTP (本会话完成),但 **Cloudflare CDN cache 仍 HIT v0.19.0.58 (max-age=7200s)**,Sparkle 自动更新可能延迟生效 — 不影响手动下载

## 0. 装机前必做 (L13/L54/L66 强约束)

```powershell
# 杀所有相关进程 + 清 registry (L66 fix unconditional write 之前的干净状态)
taskkill /F /IM WeaselServer.exe /T
taskkill /F /IM FluxingPhrasesDialog.exe /T
taskkill /F /IM ctfmon.exe /T
taskkill /F /IM TextInputHost.exe /T
reg delete "HKLM\SOFTWARE\Fluxing\Weasel" /f
reg delete "HKCU\Software\Fluxing" /f
reg delete "HKCU\Software\Microsoft\CTF\Assemblies\0x00000804" /f
```

## 1. 装新 installer

```powershell
# silent install (L13/L17 强制 /D= 显式路径)
& "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.66-installer.exe" /S /D=D:\Program Files\fluxing
```

期望 30-60s 完成。

## 2. 装机后立刻跑 `_check_install_v2.ps1`

```powershell
powershell -ExecutionPolicy Bypass -File F:\soft\00selfmade\rime_claude\_check_install_v2.ps1
```

**期望 Module 1+2**:
| 模块 | 项 | 期望 |
|---|---|---|
| [1] Running WeaselServer | md5 | `B3BCB65967D6AD0B00A5D0460405EC73` (≠ v0.65 `BFC81FA7...`) |
| [2] Install path | md5 | `B3BCB65967D6AD0B00A5D0460405EC73` (MATCH [1]) |
| [2] | installer md5 | `B4733435610FE4F22C5F837A23036A7E` (= `_check_install_v2.ps1` L90 `$expectedInstallerMd5`) |
| [3] Registry | L66 4 keys | HKLM\KnownClasses + HKCU\...\0x00000804 + HKLM\...\CLSID\InprocServer32 全部存在 |

**如果有 mismatch** → 不要手动改 → 把整个 `_check_install_v2.ps1` 输出贴回来。

## 3. 4 user flow 手测 (核心验证)

| # | 操作 | 期望结果 |
|---|---|---|
| 1 | **Win+Space 切到火流猩输入法** | 浮动设置栏**自动弹出** ✓ (Phase L bugfix 3 真修路径) |
| 2 | **Win+Space 切到英文 (或其他 IME)** | 浮动设置栏**收** ✓ |
| 3 | **在浏览器/记事本/Word 切焦点到 text input** (已 Fluxing 激活时) | 浮动栏**弹** (TSF OnActivated 在每个 TSF thread 触发);若已切到英文 → 不弹 |
| 4 | **按 Alt+,** (toggle hotkey) | 浮动栏**显/隐切换** ✓ |

**如果 #1 不弹** → root cause 没修,跑 `_check_install_v2.ps1` 看 Module 5 file mtime (newest 应该是 7/25 23:05 install 时间,不是更老的 binary 时间)。

**如果 #4 Alt+, 死** → `weasel-install.log` (在 `%APPDATA%\Rime\`) 看 `[Run] s_fluxingHkl=...` + `[RegisterPhrasesHotkey] ImmGetDefaultIMEWnd(NULL) = 0x...` 两条埋点是否都出来。

## 4. 已知 issue 注意事项

### 4.1 Cloudflare CDN cache 滞后
- **症状**: 浏览器访问 `https://www.aiec.fun/pinyin/version.json` 仍显示 v0.19.0.58
- **真相**: SFTP server 上 v0.66 已就位,Cloudflare 缓存旧版本,`max-age=7200s` = 2 小时自然过期
- **cache-buster 验证**:
  ```powershell
  curl "https://www.aiec.fun/pinyin/version.json?cb=$(date +%s)"
  ```
  应返回 v0.19.0.66 内容
- **影响**:
  - 手动 URL 下载 installer (直接拼 `fluxing-0.19.0.66-installer.exe`) → 不受影响
  - Sparkle 自动更新检查 → 可能要等 cache 过期
  - 网站首页/version.json 显示 → 等 2h
- **解决**: user 在 Cloudflare dashboard → Caching → Purge Cache (Custom Purge) → 输入 `pinyin/version.json`、`pinyin/appcast.xml`、`pinyin/release-notes.html` 三个 URL

### 4.2 K7 GitHub push 持续 fail
- HTTP 500 / 408 / TLS eof (3 种 error 随机出现)
- Server-side 限制,client-side 无解
- **不影响装机**: SFTP www.aiec.fun 已成功

### 4.3 CHANGELOG.md 缺 v0.19.0.66 段
- repo 内 CHANGELOG.md `grep "0.19.0.66"` = 0 命中
- 应在下次 sessions 单独补 docs commit (commit-checklist P5 强制)

## 5. 反馈模板

跑完上面 4 步后,贴以下内容:

```
[1] WeaselServer.exe md5:        ____________
[2] Installer md5:                B4733435610FE4F22C5F837A23036A7E (期望)
[3] Flow 1 (Win+Space 切 Fluxing 弹):  PASS / FAIL
[4] Flow 2 (Win+Space 切英文 收):      PASS / FAIL
[5] Flow 3 (切 text input 弹):         PASS / FAIL (需说明 IME 状态)
[6] Flow 4 (Alt+, toggle):             PASS / FAIL
[7] weasel-install.log [Run] 行:       贴第一行 (s_fluxingHkl=?)
```

---

*机装验证不通过 → 不要立即改源码 → 把上面 7 项贴回,我走 systematic-debugging 重新定位。*