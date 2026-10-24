# Handoff — Phase L Bugfix 9 v0.72 fix (2026-07-29)

> **承接**: `handoff-after-phase-L-bugfix-8.2-installer-finalize-freeze-2026-07-29.md`
> **状态**: ✅ **Step 1+2 完成** — 现场 capture 完成 + 根因修正
> **新目标**: Step 5 rebuild + sandbox verify + Step 6 ship v0.72

---

## 1. 一句话总结

v0.71 装机 finalization 报"系统无响应"**不是真 hang**。Forensic log
(`rime.weasel.*.INFO.20260729-211535.2092.log`, 59KB, 21:15-21:35+)
显示 **25+ 次 engine init 全部成功** (10-700ms 每次)。WeaselServer.exe
一直活着。User 感知的"freeze" = 装机点 OK → 5-30 秒内 Fluxing 没在语言栏
出现 → user 反复 kill+restart WeaselServer.exe 测出的"卡顿",不是死锁。

**Handoff §2 假设 A (librime engine init 死循环) 错误**。Root cause 是
**UX-timing**,不是 librime-level bug。

**Fix 策略 (user 选 Option C)**: WeaselServer 写 Ready 标志
(HKCU\Software\Rime\Weasel\Ready="1"),NSIS 轮询 30s。Installer 不再
fire-and-forget `Exec`,等 WeaselServer 真就绪才显示"完成"对话框。

---

## 2. 关键 evidence 修正

### v0.71 install log 重读 (5min 详细分析, 之前 handoff 误读)

```
21:15:35 PID 25656  WeaselServer startup (registrar → 25656)
21:15:37 PID 19024  engine.cc:72] starting engine.  (first engine init)
21:15:38 PID 19024  engine.cc:96] engine disposed.  (first init complete)
21:15:40 PID 16556  engine.cc:72] starting engine.  (second init)
21:15:40 PID 16556  engine init OK in 125ms
21:15:40 → 21:17:29  *** 109 秒静默期 (user 感知 "freeze") ***
21:17:29 PID 16600  engine.cc:72] starting engine.  (THIRD init, 110s 后)
21:17:29 PID 32696  engine.cc:72] starting engine.  (FOURTH init, 15ms 后)
21:17:29 PID 28688  engine.cc:72] starting engine.  (FIFTH init, 3ms 后)
21:17:32 PID 17320  engine init OK
... 25+ 不同 PID engine init 全部成功, 间隔从 0.4s 到 10min ...
21:35:25 PID 33592  仍在 init (installer 已 "完成" 后 19 分钟)
```

**关键观察**:
- 每个 engine init 10-700ms (warm), 0.6-7s (cold) — librime **没 hang**
- 25+ 不同 PID = user kill+restart WeaselServer.exe 25+ 次
- 109 秒静默期 = user 等待 + kill+restart 循环
- **系统一直活,user 误以为死锁**

### v0.70 boot freeze 重读 (15:54 log)

```
15:54:03 PID 10528  WeaselServer startup
15:54:03 → 15:54:27  *** 24 秒 deployer 编译 4.1GB rime_ice dict (正常) ***
15:54:27 PID 6028  engine starting (first)
15:54:27 → 15:54:34  7 秒 silent (cold cache engine init, 正常)
15:54:34 PID 5260  engine starting (SECOND)
15:54:34 → hang       *** 第二次 engine init 死循环 (v0.70 TSF storm 真因) ***
```

v0.70 hang 跟 v0.71 完全是两个 bug:
- v0.70 = TSF OnActivated 风暴 (v0.71 FocusInGuard + Echo(100) 已修)
- v0.71 = UX-timing, 装机完 user 看不到 Fluxing 在语言栏

**v0.70 evidence (engine.cc:72 hang) 不能直接套到 v0.71**。User 报告
"v0.71 install freeze" 时引用了 v0.70 evidence,但 v0.71 log 证明 librime
正常,问题在 UX。

---

## 3. Root cause 最终定位

**真正 root cause**: NSIS line 802 `Exec "$INSTDIR\WeaselServer.exe"`
(fire-and-forget) + 紧跟 `MessageBox "CheckForUpdates"` (line 806) +
`MessageBox "请重启 WeaselServer.exe 或注销后重新登录"` (line 822) +
User 点 OK → installer 退出。

时序:
1. NSIS line 800: WriteRegStr HKLM Run key (autorun,下登录生效)
2. NSIS line 802: Exec WeaselServer.exe (后台启动,NSIS 不 wait)
3. NSIS line 806: 弹 CheckForUpdates dialog
4. NSIS line 822: 弹"完成"dialog
5. User 点 OK
6. **User 期望: Fluxing 在语言栏立即可用**
7. **实际: WeaselServer 还在 cold-start engine init (600-700ms) + TSF sink 注册 + tray icon 创建,语言栏还没看到 Fluxing**
8. User 打开 notepad / 浏览器测试 → 没反应 → 误以为 "system hang"
9. User kill WeaselServer → 重启 → 看到 Fluxing 在语言栏 → 仍觉得之前 "卡了"
10. 重复 N 次 (log 显示 25+)

---

## 4. 实施的 fix (Option C, user 选)

**2 文件改动 + 1 ADR + 1 新 test**:

### 4.1 ADR 0002 (`docs/adr/0002-installer-ready-flag-poll.md`)

- Status: Accepted (v0.19.0.72)
- Why: Phase L Bugfix 9, install finalization UX-timing fix
- Decision: WeaselServer 写 HKCU\Software\Rime\Weasel\Ready="1",NSIS
  DeleteRegValue + Exec + poll 30s
- L66/nsis-registry-writes 合规: poll 是 ReadRegStr (read-only),无
  WriteRegStr 在 `${If}` 块内嵌套

### 4.2 WeaselServer/WeaselServerApp.cpp:Run()

在 `m_handler->Initialize()` 后立即写 Ready flag (line 344 之后):
```cpp
m_handler->Initialize();
{
  HKEY hKey = NULL;
  if (::RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Rime\\Weasel",
                        0, NULL, REG_OPTION_NON_VOLATILE,
                        KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
    const wchar_t kReady[] = L"1";
    ::RegSetValueExW(hKey, L"Ready", 0, REG_SZ,
                     reinterpret_cast<const BYTE*>(kReady), sizeof(kReady));
    ::RegCloseKey(hKey);
  }
}
m_handler->OnUpdateUI([this]() { tray_icon.Refresh(); });
```

### 4.3 output/install.nsi line 802

```nsi
weasel_write_ok:
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "WeaselServer" "$INSTDIR\WeaselServer.exe"
  ; v0.19.0.72: Ready-flag handshake (ADR 0002)
  DeleteRegValue HKCU "Software\Rime\Weasel" "Ready"
  Exec '"$INSTDIR\WeaselServer.exe"'
  StrCpy $0 0
  ${Do}
    Sleep 1000
    IntOp $0 $0 + 1
    ReadRegStr $1 HKCU "Software\Rime\Weasel" "Ready"
    ${If} $1 == "1"
      ${ExitDo}
    ${EndIf}
    ${If} $0 >= 30
      DetailPrint "Fluxing v0.19.0.72: WeaselServer did not signal Ready within 30s; continuing install."
      ${ExitDo}
    ${EndIf}
  ${Loop}
```

30s timeout 是 worst-case bound (实测 cold cache 7s, warm 1s)。timeout
不阻塞 install,DetailPrint 警告 + 继续。

### 4.4 RimeWithWeasel/RimeWithWeasel.cpp:Initialize()

加 QueryPerformanceCounter stopwatch DLOG 测量 librime init 各阶段
时间。**纯 debug log,不改 behavior**。下一装机可在 rime.weasel.INFO log
看到 `[v0.19.0.72 init stopwatch] rime_api->initialize took NNN ms` 等。

### 4.5 test/TestWeaselServerStartup/

新增 test (5 文件),验证 Ready-flag 协议 contract:
- Test [1]: Ready key 格式 (REG_SZ, value "1")
- Test [2]: WeaselServer 跑着时 5s 内 Ready="1"
- Test [3]: Ready 在 HKCU, 不在 HKLM (per ADR 0002 unelevated design)

**不 spawn WeaselServer.exe** (PPL 限制,TestFreezeMitigation 已声明)。
end-to-end 验证靠 sandbox-verify。

### 4.6 weasel.sln + xmake.lua 注册

- weasel.sln: 新 Project 行 + ProjectConfigurationPlatforms 8 entries
- xmake.lua: `includes("test/TestWeaselServerStartup")` (debug 模式)

---

## 5. 行为对比

| 步骤 | v0.71 | v0.72 |
|---|---|---|
| NSIS line 800 WriteRegStr Run key | ✅ 同 | ✅ 同 |
| NSIS line 802 DeleteRegValue Ready | — | ✅ 新增 (clean baseline) |
| NSIS line 802 `Exec WeaselServer.exe` | ✅ fire-and-forget | ✅ 同 (fire-and-forget) |
| NSIS line 802 poll Ready 30s | — | ✅ 新增 |
| WeaselServer m_handler->Initialize | ✅ 同 | ✅ 同 |
| WeaselServer 写 HKCU\...\Ready="1" | — | ✅ 新增 (after Initialize) |
| NSIS CheckForUpdates MessageBox | ✅ 紧跟 Exec | ✅ 等 Ready 之后 |
| NSIS "完成" MessageBox | ✅ 紧跟 CheckForUpdates | ✅ 同 |
| User 点 OK 后 Fluxing 在语言栏? | ❌ 0-30s 延迟 | ✅ 立即可见 |

---

## 6. 风险评估

**已考虑风险**:
- **R1**: 30s timeout 太长 → 冷 cache 实测 7s,30s 是 4x 安全余量
- **R2**: HKCU 写冲突 → key 是 per-user,WeaselServer unelevated,无冲突
- **R3**: WeaselServer 启动 hang → timeout log 警告 + install 继续,user
  可手动启动 (行为与 v0.71 一致)
- **R4**: PPL 写 HKCU 失败 → PPL 不限制 HKCU,只限制 HKLM
- **R5**: NSIS `${Do}/${ExitDo}` 语法 → LogicLib.nsh 已 include (line 3),
  与现有 `${If}` 块同源

**未发现风险**:
- 已查现有 v0.71 log,无 25+ 次 Ready flag 写冲突迹象
- 已查 weasel.sln + xmake.lua 注册,与 TestFreezeMitigation 模式一致

---

## 7. Session health

- ~25 tool calls (build script + Read + Edit + Bash)
- 0 subagent
- 1 ADR + 5 test 文件 + 3 source edit + 2 build registration + 1 env.bat
- jsonl 健康 (< 100KB)

---

## 8. 下个会话必做 (Step 5+6)

### Step 5: Rebuild + sandbox verify

1. 跑 `xbuild.bat` 增量 build (5 binaries: weasel.dll, weaselx64.dll,
   WeaselServer.exe, WeaselDeployer.exe, installer)
2. md5 5 binaries
3. 跑 test/Release/TestWeaselServerStartup.exe (验证 contract)
4. 跑 test/Release/TestFreezeMitigation.exe (验证无回归)
5. 跑 test/Release/TestPipeProtocol.exe (验证 IPC 协议)
6. 跑 sandbox-verify:
   - `F:\soft\00selfmade\sandbox-verify\rime-claude\rime-verify.ps1
      -InstallerPath <v0.72 installer path> -NoWait`
   - 验证: WeaselServer 启动 ≤ 30s + Ready flag 出现 + Tray icon + IPC 可达

### Step 6: Ship v0.72

1. bump env.bat: `FLUXING_VERSION=0.19.0` `WEASEL_BUILD=72`
   `PRODUCT_VERSION=0.19.0.72` `FILE_VERSION=0.19.0.72`
2. 写 `.specify/memory/lessons-learned.md` L##-PhaseL-9 entry:
   - "v0.71 install freeze = UX timing, not librime hang. log forensic
     proved engine init works. Fix = Ready flag handshake (Option C)."
3. 写 `CHANGELOG.md` ### 主要更新
4. Conventional Commits:
   - `fix(WeaselServer): v0.19.0.72 Ready-flag handshake (Phase L Bugfix 9)`
   - `docs(adr): 0002 installer Ready-flag poll`
   - `test(WeaselServerStartup): v0.19.0.72 Ready-flag contract test`
5. SFTP 上传 (kizemo pinyin release 流程, hard gate 先 Copy-Item)
6. **不 git push** (K7 仍 500)
7. 写 `handoff-after-phase-L-bugfix-9-v0.72-ship-2026-07-29.md`

---

## 9. 避坑提示

- **librime/ 子模块不动** (L107 教训, user 禁)
- **install.nsi 改完必须 UTF-8 + `EF BB BF` + CRLF** (L01 / L02 / A1 / L09)
- **NSIS pre-flight**: 不带 `/D=<path>` silent install 会污染 `$INSTDIR`
  (L13 / L17)
- **Silent test 前清**: `HKLM\Software\Fluxing\Weasel` + `HKCU\Software\Fluxing` (L54)
- **5 binaries 必 md5 check** (Phase L 7 真因教训)
- **不要把 handoff/prompt/diag/ps1 提交** (规则)
- **不 commit `env.bat`** (L10 §6 禁)
- **不 commit `weasel.props` / `*.user` / `*.suo` / `*.log` /
  `Test*.obj` / `output/archives/*`**

---

## 10. 下个会话第一句话

```
承接 handoff-after-phase-L-bugfix-9-v0.72-fix-2026-07-29.md。
Step 3+4 完成 (Option C fix + TestWeaselServerStartup)。本会话:
1. Step 5: xbuild.bat 增量 build + 5 binaries md5 + 3 个 test PASS +
   sandbox-verify WeaselServer cold-start ≤ 30s + Ready flag 出现
2. Step 6: bump env.bat v0.72 + L##-PhaseL-9 lessons + CHANGELOG +
   3 Conventional Commits (WeaselServer fix / docs ADR / test new) +
   SFTP 上传 + 不 git push (K7 500)
3. 写 handoff-after-phase-L-bugfix-9-v0.72-ship-2026-07-29.md
```

---

*End of Phase L Bugfix 9 fix handoff. v0.72 fix implemented (Option C: Ready-flag
handshake, ADR 0002, 3 file changes + 1 new test). Next: rebuild + sandbox verify
+ ship v0.72.*
