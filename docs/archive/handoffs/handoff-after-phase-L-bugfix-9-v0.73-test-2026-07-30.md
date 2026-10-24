# Handoff — Phase L Bugfix 9 v0.73 TEST BUILD (2026-07-30)

> **承接**: `handoff-after-phase-L-bugfix-9-v0.72-sftp-2026-07-29.md`
> **状态**: ✅ **v0.73 test build 完成** — 5 binaries 重建 + release/ 已清理
> **下一步**: User 装机实测 Ready-flag handshake 行为 (本机 5 项测试)

---

## 1. 一句话总结

v0.19.0.73 test build (no source change vs v0.72) ready for **真机 end-to-end 装机实测**:
- Installer `release/fluxing-0.19.0.73-installer.exe` (43,342,931 bytes md5 1430ffa5)
- release/ 已清理 (107 → 1 file)
- _check_install_v2.ps1 md5 更新 (commit fb922d2)

---

## 2. v0.73 build md5 (5 binaries + installer)

| Binary | Path | md5 (v0.73) | vs v0.72 | Note |
|---|---|---|---|---|
| WeaselServer.exe | output\Win32\ | `0424EB54380D5CC0EA5B1CA583BF008B` | 8BE1670B...→0424EB54 | rebuild (含 Ready flag 写) |
| WeaselDeployer.exe | output\Win32\ | `F8D635342128364056CA748116DCEECD` | 8DF17FE9...→F8D63534 | rebuild |
| WeaselSetup.exe | output\ | `BB4440D6C38A885EE33213D393BF1AB7` | (新增) | rebuild |
| weasel.dll | output\ | `231FD0CFED97FBE6E40C1498CB10A1F1` | 749F55E9...→231FD0CF | rebuild (含 DLOG stopwatch) |
| weaselx64.dll | output\ | `EA4DECE5B4C76A01704204472C2AA5F4` | (same) | L##-PhaseL-8 stale (x64 build blocked) |
| Installer | output\archives\ | `1430FFA5489FC7FB5A4B620F9B69B2F7` | 43,341,637→43,342,931 bytes | rebuild NSIS |

**Build timestamp**: WeaselServer.exe 08:33, weasel.dll 08:33, NSIS 08:39 (2026-07-30)
**Content 与 v0.72 相同**,仅 PRODUCT_VERSION 0.19.0.72→0.19.0.73 (timestamp 变 → binary md5 微变)。

---

## 3. release/ 目录清理 (107 → 1 file)

**清理前**: 107 文件 = 21 Test*.exe + 62 .pdb/.exp/.lib + 22 旧 installer (v0.5–v0.67) + 1 v0.71
**清理后**: 1 文件 = `fluxing-0.19.0.73-installer.exe` (43,342,931 bytes)

User 取这一文件装机测试。

---

## 4. 真机装机测试 5 项 (核心目标: 验证 Ready-flag handshake)

### Test 1: 装机完成 timing (no freeze)
- **步骤**: 在干净 Win11 机器 (或覆盖 v0.71/v0.72) 双击 `fluxing-0.19.0.73-installer.exe`
- **预期**: 5-10 秒 copy + 5-30 秒 deployer /install + /deploy (deploy 包含 4.1GB rime_ice dict compile,可 30s+ 慢,**正常**) + 5-7 秒 WeaselServer cold init + CheckForUpdates dialog
- **成功标准**: 整个装机流程 < 60 秒,无 freeze/hang,**user 不需要 Ctrl+Alt+Del**
- **失败 indicator**: 必须 taskkill WeaselServer.exe 才能继续 (v0.71 旧 behavior)

### Test 2: Ready flag timing (核心 Ready-flag 验证)
- **步骤**:
  1. 装机前,user 打开 PowerShell 跑:
     ```powershell
     # 装机开始前清 Ready key
     Remove-ItemProperty -Path 'HKCU:\Software\Rime\Weasel' -Name 'Ready' -ErrorAction SilentlyContinue
     # 监控 Ready 出现 timing
     $start = Get-Date
     while (-not (Get-ItemProperty -Path 'HKCU:\Software\Rime\Weasel' -Name 'Ready' -ErrorAction SilentlyContinue)) {
         Start-Sleep -Milliseconds 200
     }
     $elapsed = (Get-Date) - $start
     Write-Host "Ready flag observed after $elapsed"
     ```
  2. **同时**开始装机 (user 自己双击 installer.exe)
- **预期**:
  - Ready flag 出现 timing = 5-7 秒 (cold cache dict + schemas) 或 < 1 秒 (warm)
  - 关键: 30 秒内必出现 (NSIS poll loop timeout 上限)
  - **若 Ready flag 永远不出现**: NSIS poll 30s 后 DetailPrint 警告 + 继续,user 仍可装完 (行为兼容 v0.71)
- **成功标准**: Ready flag 出现 timing 在 0.5-30 秒之间

### Test 3: Fluxing 立即在语言栏可见 (核心 UX-timing 修复)
- **步骤**: 装机点 OK 后,**立刻** (≤ 5 秒) 打开 notepad / 浏览器 / 任何文本输入框
- **预期**:
  - 语言栏已有火流猩 (Win+Space 切能选)
  - 切到 Fluxing,**立即可输入中文**(无 0-30 秒 "no response" 感知)
- **成功标准**: 装机后 5 秒内 Fluxing 可正常输入中文,**0 等待**
- **失败 indicator**: 装机后 30+ 秒看不到 Fluxing,或切到 Fluxing 无响应 → Ready flag handshake 失效

### Test 4: Alt+, toggle QuickPanel (回归验证, 防止 Ready flag 改动 break 既有功能)
- **步骤**: 装机完,按 Alt+,
- **预期**: QuickPanel 浮动设置栏 toggle (按一次弹,再按一次收)
- **成功标准**: Alt+, work 同 v0.71
- **失败 indicator**: Alt+, 死了 / 浮动栏永不弹 → FocusInGuard 或注册路径 break 了

### Test 5: Win+Space 切 Fluxing 自动弹 QuickPanel (spec 070 + v0.66 sink 注册回归)
- **步骤**: 装机完,按 Win+Space 切到 Fluxing
- **预期**: 浮动设置栏自动弹出 (OnActivated → FocusIn → QuickPanelDialog::Show 链)
- **成功标准**: Win+Space 切 Fluxing → QuickPanel 弹 ≤ 1 秒
- **失败 indicator**: QuickPanel 不弹 → v0.66 sink 注册 break (与 Ready flag 无关,但要测无回归)

---

## 5. 测试后判定标准

### Pass (v0.73 ship)
- 5/5 测试 PASS
- Ready flag 在 ≤ 7 秒出现 (cold) 或 ≤ 1 秒 (warm)
- 无 freeze/hang 感知
- → User 决定 ship v0.73 到 aiec.fun (走 sftp_sync, 类似 v0.72 ship 流程)

### Conditional Pass
- 4/5 PASS,但 Test 2 (Ready flag) timing > 10 秒
- → 改 NSIS poll timeout 到 60 秒 (per L##-PhaseL-7 工程经验), 改 1 line install.nsi

### Fail
- Test 1 失败 (装机 freeze) → 立即回滚到 v0.72 ship, 标记 v0.73 unstable
- Test 4 或 Test 5 失败 (回归) → 立即回滚, 标 regression bug

### 数据回收 (如有 5 项 PASS)
- 把 Ready flag observed timing 写进 lessons-learned.md (L##-PhaseL-9 续)
- 把 5 binaries md5 + installer md5 跟 v0.72 对比,确认无 binary corruption

---

## 6. release/ 取用方式

```bash
# 从 user 端取 v0.73 installer (有 git checkout)
cp F:/soft/00selfmade/rime_claude/release/fluxing-0.19.0.73-installer.exe <target>

# 或 PowerShell
Copy-Item 'F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.73-installer.exe' <target>
```

文件大小: 43,342,931 bytes, md5 1430ffa5489fc7fb5a4b620f9b69b2f7

---

## 7. Session health

- ~50 tool calls (after this handoff)
- ~25 bash calls
- jsonl ~180KB
- ~1.2h 活跃
- 0 subagent
- 1 commit (fb922d2 expected md5)

---

## 8. 避坑提示

- **release/ 是 local test build staging**, NOT SFTP ship dir
- **v0.73 不 SFTP ship** per user prompt ("测试用" — test only)
- **不要 revert v0.72 SFTP** (aiec.fun 仍是 v0.72 LIVE)
- **如果 Test 1 失败 (freeze)**: 立即 taskkill WeaselServer.exe 保留现场,把 log 贴回来诊断
- **如果 Test 4/5 失败 (回归)**: 检查 Ready flag write 是否影响 IPC 启动 race,可能是 m_handler->Initialize 完成前 WriteRegValueEx 失败
- **weaselx64.dll 仍 stale** (L##-PhaseL-8), v0.73 ship test 不需 x64 shim, 同 v0.72

---

## 9. 下个会话第一句话

```
承接 handoff-after-phase-L-bugfix-9-v0.73-test-2026-07-30.md。
v0.73 test build ready (43.3MB md5 1430ffa5 @ release/fluxing-0.19.0.73-installer.exe)。
本会话:
1. user 装机实测 5 项 (Test 1-5 见 handoff §4)
2. Test pass → user 决定 ship v0.73 走 sftp_sync (类似 v0.72 ship 流程
   Step 6): 跑 sftp_sync.py --version=0.19.0.73, 3件套 + cross-source verify
3. Test fail → revert env.bat v0.72, rebuild + 新 handoff 标 regression
4. 不 git push (K7 仍 500), commit 暂存本地
5. 处理 weaselx64.dll stale (L##-PhaseL-8): x64 MSBuild pass 恢复后
   单独 ship patch
```

---

*End of Phase L Bugfix 9 v0.73 test build handoff. v0.73 ready for
真机 end-to-end 装机实测. 5 项 test plan + Pass/Fail 判定标准
+ 下个会话指引. release/ 已清理, 只留 v0.73 installer 1 文件供
user 取用.*
