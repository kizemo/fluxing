# 071 - Install-Freeze Fix

## 0. Background

v0.74 ship-gate 5 lessons-learned hang-pattern fixes (L##-PhaseL-7/8/9/9.5/9.6),
但真机 Win11 24H2 上 install + reboot 后切输入法仍然无响应 几分钟~17 分钟。
Log forensics (`$TEMP\rime.weasel.INFO.*.log`) + System32 legacy DLL 残留 + librime
6+ PID engine init 链 + sandbox 验证脚本 `_check_system32_cleanup.ps1`
发现 L##-PhaseL-9.6 cleanup 模式 ship 时未覆盖 backup 文件命名空间:

- `.onInit` 的 9.6 cleanup 只识别精确文件名 (`weasel.dll` / `weaselx64.dll`),
  System32 残留 11 个 v0.66/67/68 era backup DLLs:
  `weasel.old.dll` / `weasel.dll.bak` / `weasel.dll.old` /
  `weasel.dll.old.10` / `weasel.dll.old.135039` / `weasel.dll.v465` /
  `weasel.dll.v466bak-20260623-095658` / `weaselx64.dll.old.133147` /
  `weaselx64.ime` / `weaselx64.ime.old.133147`. Windows shell loader / CTF
  enumeration 见到这些 unassociated DLL → "no associated application" 持久
  UX 干扰 → 用户感知为"切输入法无响应"。
- bundled data 文件布局与 librime 运行时读路径不一致:installer 装到
  `$INSTDIR\data\opencc\` 但 librime 实际读 `$INSTDIR\weasel\data\opencc\`。
  每次 engine init 报 `opencc config not found: emoji.json` ERROR。
  6+ PID 重启 (用户反复 ctrl+shift 切输入法) → 总耗时几分钟。

stage-at-startup F4 路径还引入 5 个同名 schtasks race,silent install
`SetErrorLevel 6001` + `Quit` 缺原因,Ready-flag 30s timeout 缺用户感知。
这 3 项是 verify + UX polish,不是 freeze 真因,但跟 freeze 修复一并 ship。

## 1. Goals

- v0.74.1 install + reboot 后切输入法不 hang:Ready-flag 30s 内观察到,
  cold init 上限保留 30 s,装好即可用
- System32 不残留 v0.66/67/68 era legacy shim DLLs(glob 模式覆盖全部 backup)
- librime 启动 0 个 `opencc config not found` ERROR log line

## 2. User stories

| ID | 标题 | 优先级 | 验收信号 |
|---|---|---|---|
| US001-A | 真机装机不 freeze (1.5 分钟内 ready) | P1 | 真机 sandbox-verify 5 binary md5 ≠ v0.74 + Ready-flag ≤ 30s + 切输入法 ≤ 1s |
| US002-A | System32 0 个 weasel* 残留 | P1 | `_check_system32_cleanup.ps1 -Mode Audit` 输出 0 FAIL;真机 Live Mode 0 FAIL |
| US003-A | librime init 0 opencc-emoji ERROR | P1 | `Find /C /V "" rime.weasel.INFO.*.log | find "opencc config not found"` 命中 0 |
| US004-A | 5 个 unique schtasks (race free) | P2 | `schtasks /Query /FO LIST /TN FluxingStage2ShimSwap*` 输出 5 unique |
| US005-A | silent Abort 6001 携带原因 | P2 | silent install + staged + verify.log 含 "reboot required deferred" |
| US006-A | Ready-flag 30s timeout 用户感知 | P3 | 真机 `reg add` 故意不写 Ready 字段,install 弹出 MessageBox |

## 3. Functional requirements

| ID | 内容 |
|---|---|
| FR-001 | install.nsi `.onInit` 与 Uninstall 的 9.6 cleanup 块改为 glob `weasel*.dll` + `weasel*.ime` 模式 (含 `.old` / `.bak` / `.v###` / `.v###bak` / `.<num>` timestamp 后缀) |
| FR-002 | install.nsi Section "Fluxing" 的 opencc data 块:在现有 `$INSTDIR\data\opencc\` File 之外,**新增** `$INSTDIR\weasel\data\opencc\` SetOutPath + 同样 `data\opencc\*.json` + `data\opencc\*.ocd*`,让 librime 找得到 |
| FR-003 | install.nsi 5 个 stage blocks 的 schtasks task name 加 shim 后缀 (`FluxingStage2ShimSwap_weasel` / `FluxingStage2ShimSwap_weaselx64` / `FluxingStage2ShimSwap_server` / `FluxingStage2ShimSwap_deployer` / `FluxingStage2ShimSwap_setup`),stage2 batch 接受 SHIM env var 或 arg 决定单 shim 操作 |
| FR-004 | install.nsi silent 路径 `SetErrorLevel 6001` + `Quit` 之前先 `DetailPrint "Fluxing v0.74.1: deferred shim swap pending; silent exit 6001 (sandbox/CI: detect reboot required)"` |
| FR-005 | install.nsi Ready-flag 30s timeout:`IfSilent skip_warning` 之内,nonsilent 显示 `MessageBox MB_OK\|MB_ICONWARNING "火流猩输入法安装完成,但 WeaselServer 没在 30s 内 Ready..."`;silent 路径保留 `DetailPrint` |

## 4. Non-goals

1. 不修 librime / plum / librime-lua submodule pointer
2. 不 revert 9.6 cleanup 的 `Delete /REBOOTOK` 基础机制(继续用 REBOOTOK)
3. 不改 QuickPanelDialog / 浮动栏 UI(TODO spec 072,用户优先级 P1)
4. 不 push 到 GitHub (P3 等 Task #6 PASS + 网络恢复)
5. 不改 Task #10 范围

## 5. Acceptance criteria

| ID | 内容 |
|---|---|
| SC-001-1 | `_check_system32_cleanup.ps1 -Mode Audit` PASS, 0 FAIL |
| SC-001-2 | `_check_system32_cleanup.ps1 -Mode Live` 在主机输出 0 FAIL |
| SC-002-1 | 真机 sandbox-verify v0.74.1,5 binary md5 ≠ v0.74 |
| SC-002-2 | 真机 sandbox-verify v0.74.1,Ready-flag 30 s 内观察到 |
| SC-002-3 | 真机 sandbox-verify v0.74.1,切输入法 ≤ 1 s 响应 |
| SC-003-1 | 真机装后 `Find /C /V "" rime.weasel.INFO.*.log | find "opencc config not found"` 命中 0 |
| SC-004-1 | `schtasks /Query /FO LIST /TN FluxingStage2ShimSwap*` 输出 5 unique task name |
| SC-005-1 | silent install + 故意 staged + verify.log 含 "reboot required deferred" 字段 |
| SC-006-1 | 真机 install + skip Ready write → install 弹 MessageBox |
| SC-007-1 | 5 binary md5 ≠ v0.74 + `_check_install_v3.ps1` PASS |

## 6. Risks

### R-001 - 9.6 glob 误伤同前缀 DLL

**风险**: `$WINDIR\System32\weasel*.dll` glob 模式可能跟第三方 DLL 同前缀。
**缓解**:
- 仅 `weasel*.dll` 与 `weasel*.ime` 两个 glob,先 enumerate 列出 candidates
- 加 size threshold (> 1 MB) 防御,确保不影响 Windows built-in 小 DLL
- 加 trace DetailPrint 每个 candidate 文件路径 + size + delete 状态

### R-002 - librime 数据路径变更破坏用户数据

**风险**: FR-002 新装位置不影响用户数据,但 uninstall 时两种路径都要清
(`$INSTDIR\data\opencc\` 与 `$INSTDIR\weasel\data\opencc\`),否则老路径残留。
**缓解**: uninstall 段 mirror 删两条路径,no silent skip。

### R-003 - schtasks 重命名 ship 时污染老任务

**风险**: v0.74 → v0.74.1 升级时,已 ship 的 `FluxingStage2ShimSwap` 老任务
如果在 stage 列表里,新 task name 注册会有 race。
**缓解**: pre-install 段显式 `schtasks /Delete /TN FluxingStage2ShimSwap /F`
+ 5 个新 task name 各 Delete 一次(防御性,即使不存在也不报错)。
stage2 batch 自清理 +5 个新 task name (line 137 改)。

### R-004 - DetailPrint 在 `Quit` 前可能异步

**风险**: NSIS DetailPrint 在某些 terminal 不 flush;SetErrorLevel 6001 + Quit
可能让 DetailPrint 丢失。
**缓解**: DetailPrint 后跟一行 `FileWrite $9` 同内容到 `$INSTDIR\fluxing-install.log`
确保落盘。即使 terminal 看不到,verify 抓 log 仍能命中。

### R-005 - MessageBox 在 silent install 阻塞 automation

**风险**: silent install 路径误调 MessageBox 阻塞 sandbox-verify / pinyin CI。
**缓解**: MessageBox 必须包在 `IfSilent skip_warning` jump 之内,只有
`IfSilent 0 +5` 跳过(走了下面的 `IfSilent 0 +5` continue path,已经 silent-safe)。
silent 路径走 `DetailPrint` 不阻塞。

## 7. Dependencies

- Task #9 (诊断, ✓ done, 2026-07-30)
- env.bat / weasel.props bump v0.74 → v0.74.1
- `_check_system32_cleanup.ps1` 已有,扩展 glob anchor
- ADR 0003 F4 设计意图,需 amend 5 unique schtasks + glob cleanup 章节
- lessons-learned 加 L##-PhaseL-9.7 (9.6 glob 不足) + L##-PhaseL-9.8 (opencc 路径错位)

## 8. Out of scope

1. QuickPanelDialog / 浮动栏 UI polish (spec 072)
2. librime / plum submodule change
3. GitHub push (P3 等 Task #6 + 网络)
4. uninstall.exe readiness 检测 (留 v0.75)
5. .nsi 在 v0.74 已 ship 部分 (`.onInit` taskkill retry、reboot prompt 等) 增量修改
