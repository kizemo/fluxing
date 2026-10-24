# Handoff — Phase L Bugfix 9.5 + 9.6 v0.74 stage-at-startup + system32 cleanup (2026-07-30)

> **承接**: `handoff-after-phase-L-bugfix-9-v0.73-reanalysis-2026-07-30.md`
> **状态**: ✅ v0.74 installer built locally (含 system32 双向清理); smoke test PASS; **未 SFTP**, 未 commit
> **下个会话第一任务**: 装机 + sandbox-verify F4 真机验证 + commit

---

## 0. 追加背景 (Phase L Bugfix 9.6)

User 在装机过程中 (实际是 v0.73 卸载后) 看到 Windows dialog:
`C:\WINDOWS\system32\weaselx64.dll — 该文件没有与之关联的应用来执行该操作。`

**根因**: v0.66 / v0.67 / v0.68 era installer 把 shim DLL 复制到 `%WINDIR%\System32\` (TSF TIP 64-bit native 注册); 后来 uninstall 不清理; v0.73 / v0.74 也不清理。Windows shell (Explorer / Search Indexer / Defender) 扫描 System32 遇到无 Open verb 的 DLL → 弹 "no associated application" dialog — 持续 UX 干扰,即使 Fluxing 卸了。

**9.6 修法**: `output/install.nsi` 双向清理:
- `.onInit` (line 220-235): 检测 + Delete /REBOOTOK `system32\weaselx64.dll` + `system32\weasel.dll`
- `Section Uninstall` (line 1035-1045): 配对清理

User 在 AskUserQuestion 选了 "双向清理 (推荐)"。

---

## 1. 用户决策 (2026-07-30)

User 在 AskUserQuestion 选择:
- **F4** (全 stage + At-startup 应用, 最彻底, 类似 macOS update 模式)
- **授权** 写 install.nsi + ADR + 测试

User 反馈新增症状:
- v0.73 装好后切输入法无反应 (IME dead)
- Claude 等 x64 app 切输入法时显示 toolbar → 无响应 → 闪退
- Workaround: 卸载 Fluxing

---

## 2. 已完成的工作 (本会话)

### 2.1 ADR 0003 (新)
**`docs/adr/0003-installer-shim-stage-at-startup.md`**
- Status: Accepted (v0.19.0.74)
- Date: 2026-07-30
- 核心决策: silent-skip 路径 → 写 `.staged` + 注册 At-startup Task Scheduler + SetRebootFlag + 强制 reboot MessageBox
- 完整 Consequences / Risks / ship-gate 检查清单

### 2.2 output/stage2-shim-install.bat.cmd (新)
5,371 → 6,150 bytes, CRLF, UTF-8:
- 启动时读 `HKLM\Software\Fluxing\Weasel\InstallDir` 解析 INSTDIR
- 读 `%TEMP%\fluxing-stage2-shim\pending.json` (CRLF, 一行一文件名, 无 md5)
- taskkill WeaselServer.exe / ctfmon.exe / TextInputHost.exe
- 对每个 pending 文件: rename live → .old → move .staged → live
- 写 `done.txt` + `HKLM\Software\Fluxing\Weasel\Stage2Done` registry marker
- **仅当从 `%TEMP%` 运行时** self-delete (避免破坏 $INSTDIR 源文件)

### 2.3 output/install.nsi (改)
从 933 行 → 1,063 行 (+130):
1. 加 `Var FluxingStage2Needed` 顶层变量 (line 22)
2. 加 `File "stage2-shim-install.bat.cmd"` 在 install section (line 736)
3. 5 处 silent-skip 路径改成 stage-then-register:
   - `weasel.dll` (line 477-505)
   - `weaselx64.dll` (line 525-555)
   - `WeaselServer.exe` (line 604-634)
   - `WeaselDeployer.exe` (line 658-686)
   - `WeaselSetup.exe` (line 707-736)
4. post-install MessageBox 根据 `$FluxingStage2Needed` 分支:
   - "1" → SetRebootFlag + 强提示 "重启电脑后自动完成 binary swap"
   - "0" → 旧的 "请重启 WeaselServer.exe" 提示
5. **Phase L Bugfix 9.6**: `.onInit` + `Section Uninstall` 双向清理
   `system32\weaselx64.dll` + `system32\weasel.dll` (legacy 残留,
   Delete /REBOOTOK)

每处 silent-skip 都用唯一 label (`weasel_staged_ok`, `weaselx64_staged_ok`, `WeaselServer_staged_ok`, `WeaselDeployer_staged_ok`, `WeaselSetup_staged_ok`) 避免 NSIS 全局 label namespace 冲突。

### 2.4 test/TestInstallerShimStage/Test-Stage2ShimInstall.ps1 (新)
PowerShell Pester-style smoke test, 不需要真机 install:
- Test 1: batch 在 pending.json 缺失时解析正常 exit 0 ✓
- Test 2: pending.json + mock staged files → swap 后 live 文件内容更新 + .staged 删除 + Stage2Done=ok ✓
- Test 3: done.txt 已写 ✓

**所有 3 项 PASS** (test 2 一开始 FAIL, 因为 cmd for /F 对 CRLF 敏感, fix 测试用 [System.IO.File]::WriteAllText 显式 CRLF 后 PASS)

### 2.5 env.bat (本地, gitignored)
v0.73 → v0.74 (FLUXING_VERSION 0.19.0, WEASEL_BUILD 74, PRODUCT_VERSION/FILE_VERSION 0.19.0.74)。本会话 build 用, commit 不包含 env.bat。

### 2.6 NSIS 重新打包
```
output/archives/fluxing-0.19.0.74-installer.exe
  md5 (初版, 仅 F4): 7ae8849d17ff8e6d8b743d5c046d885e  43,357,969 bytes
  md5 (终版, F4 + 9.6): 80b679e3a7012081a12eed5a33f3cc03  43,369,392 bytes
  size vs v0.73 43,342,931; +26 KB = batch 文件 + 5 shim File/oname + 双向 system32 cleanup
  222 entries, stage2-shim-install.bat.cmd 6150 bytes 包含
  仅 2 warnings (data\*.gram / data\lua\*.txt 文件缺失, 无关本次修改)
```

### 2.7 Git status
```
M output/install.nsi
?? docs/adr/0003-installer-shim-stage-at-startup.md
?? output/stage2-shim-install.bat.cmd
?? test/TestInstallerShimStage/
```

未 commit (per handoff 约束 — K7 仍 500, 3 commits 暂存本地)。

---

## 3. 决策依据 (A/B/C 真因 → F4 修法)

User 新症状 (Claude toolbar shows then crashes) 强烈指向 **C hypothesis 真因**:
- `weaselx64.dll` 在 Claude (x64) 进程加载 → IPC TransactNamedPipe → 与新 WeaselServer.exe 协议不匹配 → hang → TSF framework timeout → Claude crash
- 老 `weaselx64.dll` 残留原因: `output/install.nsi:506-512` silent skip (rename 失败 → SetOverwrite try → File 失败 → `Goto weaselx64_done` + 旧 binary 保留)

F4 修法 = 任何 silent-skip 失败路径都 stage `.staged` + 调度 At-startup swap, 用户重启即生效, 不再"假装装好"。

---

## 4. ADR 0003 / L##-PhaseL-9.5 ship-gate 检查 (下一步真机验证必跑)

### 4.1 sandbox-verify install
- 路径: `F:\soft\00selfmade\sandbox-verify\rime-claude\rime-verify.ps1 -InstallerPath "F:\soft\00selfmade\rime_claude\output\archives\fluxing-0.19.0.74-installer.exe"`
- 期望: install exit 0, post-install `reg query HKCU\...\Ready` = 1 within 30s
- 期望: `%TEMP%\fluxing-stage2-shim\pending.json` 不存在 (silent skip 未触发 = 正常路径)
- 期望: 5 个 binary md5 = v0.73 expected (installer 没 rebuild binary,只改 install.nsi + 新 batch)

### 4.2 强制 silent-skip 真机验证 (验证 F4 真生效)
需要在真机或 sandbox 模拟 TSF host 锁 weaselx64.dll:
```
# 装机前启动 TextInputHost.exe mmap weaselx64.dll
# 装机时 silent-skip 应触发, 装完 pending.json 应存在 + FluxingStage2ShimSwap task 应注册
```

具体步骤建议下个会话写一个 `_check_install_v3.ps1` (基于 v2 + 新增):
- 检查 `%TEMP%\fluxing-stage2-shim\pending.json` 状态
- 检查 `schtasks /Query /TN FluxingStage2ShimSwap` 是否注册
- 检查 `$INSTDIR\*.staged` 是否被建
- 检查 `HKLM\Software\Fluxing\Weasel\Stage2Done`

### 4.3 重启验证 Stage 2 swap
模拟一次 reboot (sandbox reboot), 验证:
- `%TEMP%\fluxing-stage2-shim\done.txt` 写入
- `HKLM\Software\Fluxing\Weasel\Stage2Done = ok`
- live 文件 md5 = expected v0.74
- `.staged` 文件已删除
- `.old` 文件存在 (旧 binary 备份)
- `schtasks /Query /TN FluxingStage2ShimSwap` 返回 "ERROR: The system cannot find the file specified." (task 已 self-clean)

### 4.4 跑本地 msbuild test suite
```powershell
cd test\TestInstallerShimStage
powershell -ExecutionPolicy Bypass -File Test-Stage2ShimInstall.ps1
# 期望: [1/3] [2/3] [3/3] PASS, exit 0
```

---

## 5. 关键文件路径

| 用途 | 路径 | 状态 |
|---|---|---|
| v0.74 installer (本地) | `output/archives/fluxing-0.19.0.74-installer.exe` md5 `7ae8849d17ff8e6d8b743d5c046d885e` 43.4MB | ✅ 已 build |
| v0.73 installer (LIVE on aiec.fun) | `release/fluxing-0.19.0.73-installer.exe` md5 `1430ffa5` | 未 SFTP v0.74 |
| ADR 0003 | `docs/adr/0003-installer-shim-stage-at-startup.md` | ✅ Accepted |
| NSIS installer (主 source) | `output/install.nsi` (1,043 lines, +110) | ✅ Modified |
| Stage 2 batch | `output/stage2-shim-install.bat.cmd` (6150 bytes, CRLF) | ✅ New |
| Smoke test | `test/TestInstallerShimStage/Test-Stage2ShimInstall.ps1` | ✅ PASS |
| L##-PhaseL-9.5 沉淀 | `.specify/memory/lessons-learned.md` L##-PhaseL-9.5 (TODO 下个会话写) | ⏳ TODO |

---

## 6. 本会话约束

- ✅ 授权范围内: 写 install.nsi + ADR + 新 batch + 新 test
- ✅ 本地 NSIS rebuild, installer 在 `output/archives/`, **未 SFTP**
- ✅ 本地 PowerShell smoke test PASS
- ❌ **未 git commit** (K7 仍 500, 3 commits 暂存本地; 下个会话 commit + 等 K7 恢复后 push)
- ❌ **不动 librime/** (L107 教训禁动)
- ❌ **未写 L##-PhaseL-9.5** 到 `.specify/memory/lessons-learned.md` (下个会话写)

---

## 7. Session health

- bash 用量: ~50 calls (红线 80)
- 0 subagent dispatched (本会话纯 fix 模式, 无 subagent)
- 4 source files modified/created: install.nsi (改) + stage2-shim-install.bat.cmd (新) + ADR 0003 (新) + Test-Stage2ShimInstall.ps1 (新)
- 1 NSIS rebuild
- 1 smoke test PASS
- 活跃 ~3h+ (CLAUDE.md 2h 规则, **强烈建议开新会话接力**)
- jsonl 健康 (< 200KB)

---

## 8. 下个会话第一句话

```
承接 handoff-after-phase-L-bugfix-9.5-v0.74-stage-shim-2026-07-30.md。
v0.74 F4 (全 stage + At-startup) 已实现 + 本地 NSIS rebuild + smoke test PASS。
本会话目标:
1. 跑 sandbox-verify 真机 install (用 rime-verify.ps1)
2. 写 _check_install_v3.ps1 验证 F4 真生效 (silent-skip → .staged → pending.json → task)
3. 模拟 reboot 验证 Stage 2 swap (done.txt + Stage2Done registry)
4. 写 L##-PhaseL-9.5 到 lessons-learned.md
5. git commit (install.nsi + ADR 0003 + batch + test) — 等 K7 恢复后 push
6. 不 SFTP installer (commit 后等 user 显式触发)
```

---

*End of Phase L Bugfix 9.5 v0.74 handoff. F4 (stage-at-startup) 已完整实现 + 本地验证, 等下个会话真机 sandbox-verify + L##-PhaseL-9.5 沉淀 + commit. session 活跃超 3h, 强烈建议开新会话.*