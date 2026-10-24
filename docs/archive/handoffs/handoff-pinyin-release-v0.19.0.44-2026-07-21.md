# Handoff — pinyin 网站 v0.19.0.44 发版 (S-2026-07-21-02)

> 写于会话超 2h 红线 + stop hook 3 blocker 触发后, 给下个会话 (S-2026-07-21-03 或新日)
> 接手时**必读**。本文件覆盖 `task.md` Phase F + `memory/2026-07-21.md` 后半段。

## 1. TL;DR (5 行)

1. **核心发版目标 5/5 完成**: 3 件套更新 + 新 exe 上传 (本地 + 服务器) + 旧 exe 删 (本地 + 服务器)
2. **遗留 1 项未修**: `https://www.aiec.fun/pinyin/fluxing-0.18.5.0-installer.exe` 返回 404 (84KB WP 主题页), nginx-snippet.conf 设计的 rewrite 兜底未生效
3. **下次发版直接走 `feedback_pinyin_release_workflow.md` §1-11** (已在 auto-memory)
4. **本会话对 git 仓库净修改 = 0** (所有 modified files 是 user 前面积累)
5. **本会话撞墙**: 单会话 >2h + 6 步 + 大量 Bash/Read, 按 CLAUDE.md §撞墙征兆停

## 2. 已 ship 的事实 (有证据)

### 2.1 本地 web staging (`E:\办公文件\L1网站\pinyin\`)

| 文件 | 状态 | 关键字段 |
|---|---|---|
| `version.json` | ✅ 已改 | version=0.19.0.44, filename=fluxing-0.19.0.44-installer.exe, updated=2026-07-21, notes="UI 迭代 2: resize + drag reorder" |
| `appcast.xml` | ✅ 已改 (顶部插 item) | length=43194607, pubDate=Tue 21 Jul 2026 14:30:00 +0800 |
| `release-notes.html` | ✅ 已改 (顶部插 h2) | 5 条要点: resize / drag reorder / 列宽跟随 / 153 测试 / md5 |
| `fluxing-0.18.5.0-installer.exe` | ✅ 删 | `ls *.exe` 仅剩 0.19.0.44 |
| `stats.json` | ❌ 不动 | 已被 MySQL `{prefix}pinyin_downloads` 表接管 (`common.php:5` 注释明确) |
| `common.php` / `index.php` / `download.php` 等后端代码 | ❌ 不动 | user 维护, Claude 不 augment |

### 2.2 远程 SFTP (`/var/www/wordpress/pinyin/` on 47.120.26.175)

```
$ ls -la /var/www/wordpress/pinyin/
total 42304
-rwxr-xr-x  kizemo www-data    21081 Jul 21 17:12 appcast.xml
-rwxr-xr-x  kizemo www-data      246 Jul 21 17:12 version.json
-rwxr-xr-x  kizemo www-data    28324 Jul 21 17:12 release-notes.html
-rw-r--r--  kizemo www-data 43194607 Jul 21 17:13 fluxing-0.19.0.44-installer.exe
... (其余 common.php / index.php / .htaccess 等未动)
```

- 4 文件全部 MD5 双向一致 (server-side `md5sum` 比对)
- 旧 0.18.5.0 exe 已删 (ssh exec `sftp.remove`)
- 端到端下载测试: `curl https://www.aiec.fun/pinyin/download.php?file=fluxing-0.19.0.44-installer.exe` → HTTP 200 + 43,194,607 bytes (108s)

### 2.3 Memory (auto-memory + 仓库内 daily log)

| 文件 | 状态 |
|---|---|
| `C:\Users\Duanyi\.claude\projects\F--soft-00selfmade-rime-claude\memory\feedback_pinyin_release_workflow.md` | ✅ 新建, 11 步发版流水线 + 7 anti-patterns |
| `C:\Users\Duanyi\.claude\projects\F--soft-00selfmade-rime-claude\memory\MEMORY.md` | ✅ 加 1 行索引 |
| `F:\soft\00selfmade\rime_claude\memory\2026-07-21.md` | ✅ Append S-2026-07-21-02 段 (timeline + data points + anomalies) |
| `F:\soft\00selfmade\rime_claude\task.md` | ✅ Append "Phase F — pinyin 网站 v0.19.0.44 发版" 段 (5.1-5.5) |

## 3. 遗留未修 (下个会话必处理)

### 3.1 旧 .exe 链接 404 兜底

**症状**: `https://www.aiec.fun/pinyin/fluxing-0.18.5.0-installer.exe` → HTTP 404 + 84887 bytes (WordPress 404 主题页)

**期望行为** (per `E:\办公文件\L1网站\pinyin\nginx-snippet.conf` L8-10):
```
location ~ ^/pinyin/(?:fluxing|weasel)-[\d\.]+-installer\.exe$ {
    rewrite ^/pinyin/((?:fluxing|weasel)-[\d\.]+-installer\.exe)$ /pinyin/download.php?file=$1 last;
}
```
→ rewrite 到 `download.php?file=fluxing-0.18.5.0-installer.exe` → `resolve_installer_by_filename` 返回 null → 兜底 `resolve_installer()` → 返回 0.19.0.44 → send_installer_file

**实际行为**: 404 + 84KB WP 主题页。根因未确诊, 推测是 nginx server block 优先级 + WordPress 伪静态 `try_files $uri $uri/ /index.php?$query_string` 拦截了 rewrite。

**修法 2 选 1, 需 user 决定**:
1. (a) **nginx 修法**: server block 把 pinyin rewrite 提到 WP `try_files` 之前, `sudo nginx -t && sudo systemctl reload nginx` (需 root)
2. (b) **PHP 修法**: `download.php` 顶部加兜底
   ```php
   $installer = null;
   if (isset($_GET['file']) && is_string($_GET['file']) && $_GET['file'] !== '') {
       $installer = resolve_installer_by_filename($_GET['file']);
   }
   if ($installer === null) {           // ← 旧代码已经有这个兜底
       $installer = resolve_installer();
   }
   ```
   旧代码**已经**有这层兜底。所以 404 几乎肯定不是 PHP 问题, 是 nginx 问题。

### 3.2 task list 当前状态

| ID | 描述 | 状态 |
|---|---|---|
| #1 | 沉淀发版工作流到 feedback memory | ✅ completed |
| #2 | SFTP 同步三件套 + 新 exe 到 www.aiec.fun 服务器 | ✅ completed (但 "全部" 措辞过度乐观, 见 stop hook review) |
| #3 | 删除本地 fluxing-0.18.5.0-installer.exe | ✅ completed |
| #4 | 更新 version.json 到 v0.19.0.44 | ✅ completed |
| #5 | appcast.xml 顶部插 v0.19.0.44 item | ✅ completed |
| #6 | release-notes.html 顶部插 v0.19.0.44 h2 | ✅ completed |
| #7 (本会话新建) | 修旧 .exe 链接 404 兜底 (nginx rewrite 或 download.php 增强) | ⏳ pending |

## 4. SFTP 凭据与工具链

### 4.1 凭据位置 (user 主动告知)

`E:\办公文件\L1网站\.vscode\sftp.json`:
```json
{
  "host": "47.120.26.175",
  "port": 22,
  "username": "kizemo",
  "password": "<password 不在本 handoff 重复, 按 feedback §9 规则>",
  "protocol": "sftp",
  "remotePath": "/var/www/wordpress"
}
```

> **Warning**: 此 password 不写到本 handoff 也不写到 auto-memory。下次发版时按 feedback_pinyin_release_workflow.md §9 **重新从 sftp.json 读取一次性使用**。

### 4.2 工具链 (本机 Git Bash 探测)

| 工具 | 装了? | 用途 |
|---|---|---|
| `sshpass` | ❌ | 不可用, fallback |
| `lftp` | ❌ | 不可用, fallback |
| `expect` | ❌ | 不可用, fallback |
| `winscp.com` | ❌ | 不可用, fallback |
| `sftp` / `scp` | ✅ (OpenSSH) | 缺 password 交互, 不能直接用 |
| **Python paramiko 4.0.0** | ✅ | **fallback 走通, 见下** |

**fallback 模板** (Python heredoc 一次性):
```python
import paramiko, hashlib, os, sys
HOST='47.120.26.175'; PORT=22; USER='kizemo'
PASS='<从 sftp.json 读>'  # 不存, 临时
REMOTE='/var/www/wordpress/pinyin'
LOCAL='E:/办公文件/L1网站/pinyin'
# ... 4 文件 put + server-side md5sum 验证 + sftp.remove
```

**下次发版时复用**: 只需替换 `PASS = '...'` 4 字符 + `UPLOAD_FILES` 列表 + `DELETE_FILES` 列表。

## 5. 下次发版 (新会话 S-2026-07-21-03+ 或 v0.19.0.45 装机时)

按 `feedback_pinyin_release_workflow.md` §1-11 走, 11 步:

1. 确认 installer 实体 (`release\fluxing-<ver>-installer.exe` md5)
2. 拷贝到 `E:\办公文件\L1网站\pinyin\` (如未拷贝)
3. 用 Read + Edit 更新 3 件套 (不 Write 整文件)
4. 起草 release notes 来源: (a) `git log <prev>..HEAD --oneline` + (b) `git show <commit> --format=%B`
5. notes 字段一句话总结
6. appcast.xml item: `length=<bytes>` (不要 md5), `pubDate` RFC 822 + +0800
7. 删旧 exe 策略按 user 指示
8. **不要**写 stats.json (MySQL 已接管)
9. SFTP/SCP 上传: 读 sftp.json password, 一次性用, fallback 走 paramiko
10. 服务器端删旧 exe
11. **追踪台账**: task.md append 发版台账段 (Phase F-2, F-3...) + `memory/<YYYY-MM-DD>.md` append 增量 + 写 handoff

## 6. 装机命令 (user 端, v0.19.0.44)

```powershell
taskkill /F /IM WeaselServer.exe /T
reg delete "HKLM\SOFTWARE\Fluxing\Weasel" /f
reg delete "HKCU\Software\Fluxing" /f
& "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.44-installer.exe" /S /D=D:\Program Files\fluxing
& "F:\soft\00selfmade\rime_claude\_check_install_v2.ps1"
```

期望 `_check_install_v2.ps1` Module 1+2 md5 = `e0612f56e030ab1299b746e629e090c0` (v0.19.0.44 装机后 source build)
installer md5: `0ce7ae135a33cdbb8b4311a964157266`

## 7. 关键文件路径索引 (下次会话直接 Read)

| 路径 | 用途 |
|---|---|
| `F:\soft\00selfmade\rime_claude\task.md` | 项目活跃 todo (含 Phase F 段) |
| `F:\soft\00selfmade\rime_claude\memory\2026-07-21.md` | daily log (含 S-2026-07-21-01 + S-2026-07-21-02) |
| `F:\soft\00selfmade\rime_claude\handoff-pinyin-release-v0.19.0.44-2026-07-21.md` | 本文件 |
| `F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.44-installer.exe` | v0.19.0.44 装机包 (43,194,607 bytes, md5 0ce7ae13...) |
| `E:\办公文件\L1网站\pinyin\version.json` | v0.19.0.44 站点版本号 |
| `E:\办公文件\L1网站\pinyin\appcast.xml` | v0.19.0.44 Sparkle feed |
| `E:\办公文件\L1网站\pinyin\release-notes.html` | v0.19.0.44 详细更新说明 |
| `E:\办公文件\L1网站\.vscode\sftp.json` | **SFTP 凭据源** (password 字段) |
| `E:\办公文件\L1网站\pinyin\nginx-snippet.conf` | nginx rewrite 规则 (旧链接兜底设计) |
| `E:\办公文件\L1网站\pinyin\common.php` | PHP 后端 (L5 注释 stats.json 不再写) |
| `C:\Users\Duanyi\.claude\projects\F--soft-00selfmade-rime-claude\memory\feedback_pinyin_release_workflow.md` | **发版规则 (auto-memory, 下次会话自动加载)** |
