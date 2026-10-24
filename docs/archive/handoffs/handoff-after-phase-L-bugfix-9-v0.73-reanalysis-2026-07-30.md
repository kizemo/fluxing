# Handoff — Phase L Bugfix 9 v0.73 REANALYSIS (2026-07-30)

> **承接**: `handoff-after-phase-L-bugfix-9-v0.73-test-2026-07-30.md` (v0.73 test build)
> **状态**: ⏳ **等 user 跑 5 个 PowerShell 命令抓证据** — 3 subagent 已分析 + 大白话复盘已输出
> **下个会话第一任务**: 接手 user 跑回的 PowerShell 输出 → 对照 v0.73 期望 md5 → 定 A/B/C 真因 → 写修复方案 (需 user 授权)

---

## 1. 用户反馈 (2026-07-30)

User 装 v0.73 test build (`release/fluxing-0.19.0.73-installer.exe` md5 `1430ffa5`) 后反馈:
- 截图显示 installer "未响应" + dialog "火流猩输入法已安装。请重启 WeaselServer.exe 或注销后重新登录"
- **关键新症状**: 切换火流猩输入法,**算法服务无法激活,无法输入中文,无法切换出设置栏 (浮动栏)**
- User 强烈情绪: "一个简单的功能设置,为什么无法实现?为什么还造成 bug 也无法修复?"
- User 明确指令: **必须先解决安装无响应的 bug**, 才能进行其它工作

---

## 2. 已完成的工作 (本会话)

### 2.1 我之前的误诊断 (已承认错误)
- 上轮复盘说"截图 '未响应' 是 NSIS UI 副作用,不影响装机结果" — **错的**
- 真相: NSIS 第 760 行 (ExecWait WeaselDeployer) 17min hang 没修 + 第 498-600 行 silent skip 让老 binary 残留 + 注册表 3 键可能没写对 = installer 自己都不知道 WeaselServer 装坏了,只显示"已装好"

### 2.2 3 subagent 独立分析 (完成)

3 subagent 从不同角度独立分析, **3 个一致结论 ≠ 单点意见**:

#### Subagent 1 (NSIS installer 侧) — 真因大白话
**v0.73 installer 把"文件写完"等同于"能用"** —
- `output/install.nsi:498-600` 三段 L72-fix silent skip 让 ctfmon 持 mmap 时老 binary 必残留
- `output/install.nsi:760` ExecWait 17min hang 链路未修 (v0.68 Fix-D1 + Fix-D3 没 ship 进 v0.73)
- `output/install.nsi:813` fire-and-forget Exec + `output/install.nsi:824` timeout-only-continue (不 abort)
- `output/install.nsi:853` "完成" dialog 永远显示 — 即便 Ready flag 30s timeout / 老 binary 残留 → user 看到"已装好"
- 读取文件: `output/install.nsi` line 498-855 (完整 section)

#### Subagent 2 (WeaselServer 启动路径) — 14 阶段定位
**v0.73 切输入法无反应不是 WeaselServer 启动 hang — 是 weasel.dll 在 user app 进程里没被 TSF 加载 (S11 失败), 3 个 TSF sink 全没注册, OnActivated 永不触发**。
- S1-S6: 进程启动 + IPC + librime init — 应该 OK
- S7: Ready flag 写完 — OK
- S8-S10: tray + 消息循环 — 应该 OK
- **S11-S14: TSF manager 加载 weasel.dll + ActivateEx + 3 sink 注册** — ❌ 失败

最可能真因: NSIS 注册表三键 (CTF\KnownClasses / CTF\Assemblies\0x00000804 / CLSID InprocServer32) 某条写错位置 (`.claude/rules/nsis-registry-writes.md` AP-L66-A 嵌套错模式复发), 或 `start_service.bat` 路径错。**诊断第一步不是看 librime log, 是 `reg query` 这三键 + 抓 weasel.dll 加载情况 (Process Monitor filter weasel.dll)**。
- 读取文件: `WeaselServer/WeaselServerApp.cpp:294-426`, `RimeWithWeasel/RimeWithWeasel.cpp:115-186`, `WeaselTSF/WeaselTSF.cpp:129-382`

#### Subagent 3 (一周 ship chain 复盘) — 5 个失败模式
1. **Spec 假完成** (v0.66) — TSF sink 接口声明 + 函数实现但 AdviseSink 从未注册 → 18 ship 累积
2. **Deferred 设计债未解除** (v0.69) — v0.18.41.4 L69-fix `if (false && ipc_id > 0) { QuickPanelDialog::Show(); }` 临时禁 FocusIn panel trigger, 后续 4 ship 盲传
3. **Subagent 报告 + forensic log 双误判** — v0.67 QuickPanelDialog GDI+ claim 错 (5 ship 前已改纯 GDI), v0.71/v0.72 套 v0.70 旧 evidence
4. **stale binary 混进 installer** (v0.67) — Patch A/B 修改 WeaselTSF.cpp:240-258, 但 installer 里 weasel.dll md5 还是 v0.66 build, build→NSIS 阶段断链
5. **PPL + ctfmon mmap + ExecWait 死循环** — `.onInit` 5×taskkill 假成功, install.nsi:760 ExecWait 等 SCM RPC 17min timeout

**用户核心问题 "为什么多次修复没解决?" = 60% 过程失败 (verify 不严 + ship-gate 漏真机验证 + 误读证据) + 40% 架构失败 (TSF + ctfmon + PPL + NSIS 多层耦合洋葱 bug)**

---

## 3. 待 user 跑的命令 (5 项, 2 分钟)

新会话第一句指令是 user 跑这 5 个命令抓证据。基于真证据, 之前 subagent 推断才能升级到真因:

```powershell
# 1. 看 WeaselServer 用 log 路径
dir $env:TEMP\rime.weasel\*.log | Sort-Object LastWriteTime -Descending | Select-Object -First 3

# 2. 看最近 log 最后 80 行 (Ready flag + sink 注册 + engine init 状态)
Get-Content (Get-ChildItem $env:TEMP\rime.weasel\*.log | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName -Tail 80

# 3. 看 TSF 3 个关键注册表键 (决定 weasel.dll 是否被加载)
reg query "HKLM\SOFTWARE\Microsoft\CTF\KnownClasses" /v "{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}"
reg query "HKCU\Software\Microsoft\CTF\Assemblies\0x00000804"
reg query "HKLM\SOFTWARE\Classes\CLSID\{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}\InprocServer32"

# 4. 看机器里装的 binary 是不是 v0.73 (md5 验证 — 4 binary 全 MATCH 才算装好)
Get-FileHash "D:\Program Files\fluxing\weasel\WeaselServer.exe" -Algorithm MD5
Get-FileHash "D:\Program Files\fluxing\weasel\weasel.dll" -Algorithm MD5
Get-FileHash "D:\Program Files\fluxing\weasel\weaselx64.dll" -Algorithm MD5
Get-FileHash "D:\Program Files\fluxing\weasel\WeaselDeployer.exe" -Algorithm MD5

# 5. 看 weasel.dll 在哪些进程加载 (TSF manager 加载它才算 S11 成功)
Get-Process | Where-Object {$_.Modules.FileName -match 'weasel'} | Select-Object Id, ProcessName
```

**期望对照 v0.73**:
- WeaselServer.exe md5 = `0424EB54380D5CC0EA5B1CA583BF008B`
- weasel.dll md5 = `231FD0CFED97FBE6E40C1498CB10A1F1`
- weaselx64.dll md5 = `EA4DECE5B4C76A01704204472C2AA5F4` (L##-PhaseL-8 stale, 但 v0.73 仍是这个)
- WeaselDeployer.exe md5 = `F8D635342128364056CA748116DCEECD`
- 3 个注册表键都返回 `Fluxing Text Service` + `0x00000804` + `weaselx64.dll`
- weasel.dll 在 notepad / explorer 进程里加载

---

## 4. 3 种典型结果 + 修法

| 结果 | 真因 | 修法 |
|---|---|---|
| **A** binary md5 不对 (老的) | 老 binary 残留 (`install.nsi:498-600` silent skip) | `install.nsi:498-600` 改 v0.68 Fix-D3 模式 + 装机前手动 taskkill ctfmon |
| **B** 注册表键缺失 | `install.nsi:707` regsvr32 失败 + WriteRegStr 嵌套错 (L66 AP-L66-A 复发) | 修 `install.nsi:707/738` + 装机后跑 `regsvr32 weaselx64.dll` 手动补 |
| **C** binary 对 + 注册表对,但进程列表无 weasel.dll | explorer.exe 没重启 / TSF 没扫描新 shim | 注销或重启电脑 (L66 验证段已要求) |

**A 或 B 任意一个中** = 必须改 `output/install.nsi` (项目雷区 L09/L17/L66, 必须 user 明确授权才会动)

---

## 5. 防止再发生的 ship-gate 硬门禁 (Subagent 3 建议)

| # | 硬门禁 | 能拦下之前的 bug |
|---|---|---|
| **1** | 任何 ship 前必跑 `7z e installer.exe *.dll *.exe` + md5 对照 build output, 任一不匹配 = FAIL | 拦下 v0.67 stale binary |
| **2** | sandbox-verify 必须双模式 (silent + GUI) 测, GUI 必须 5min hard timeout | 拦下 v0.66/67 17min hang |
| **3** | 任何 `if (false)` `m_disabled` 早返 `if(disabled)return` sink 注册必须同 commit 加 4 节点 DLOG | 拦下 v0.66 sink 假完成 + v0.65 FocusIn 早返 |

**建议加进 CLAUDE.md §2 (必须强制) 或 §4 (专项规则) — 等 user 授权**

---

## 6. 关键文件路径

| 用途 | 路径 |
|---|---|
| v0.73 installer (本地) | `release/fluxing-0.19.0.73-installer.exe` md5 `1430ffa5` 43.3MB |
| v0.72 LIVE on aiec.fun | commit `f07a1ad` post-SFTP, **v0.73 没 SFTP** (per handoff v0.73 §8) |
| NSIS installer (主 source) | `output/install.nsi` line 498-855 |
| WeaselServer 启动路径 | `WeaselServer/WeaselServerApp.cpp:294-426` (S1-S10) |
| librime init | `RimeWithWeasel/RimeWithWeasel.cpp:115-186` |
| TSF sink 注册 | `WeaselTSF/WeaselTSF.cpp:129-260` (S12-S14) |
| L## 沉淀 | `.specify/memory/lessons-learned.md` L##-PhaseL-4/5/6/7/8/9 |
| NSIS 安全网规则 | `.claude/rules/nsis-registry-writes.md` (L66) |
| Build toolchain 规则 | `.claude/rules/build-toolchain.md` (L09/L17/L10) |

---

## 7. 本会话约束

- ❌ **不主动改 `output/install.nsi`** (L09/L17/L66 雷区 + subagent 报告需 user 真证据验证)
- ❌ **不主动改 `RimeWithWeasel.cpp`** / `WeaselServer/*.cpp` / `WeaselTSF/*.cpp` (P2 边界 + IPC 4 边同步需 ADR)
- ❌ **不 git push** (K7 仍 500, 3 commits 暂存本地: `fb922d2` / `f07a1ad` / `fbd09ca`)
- ❌ **不动 librime/ 子模块** (L107 教训禁动, user 授权才行)
- ✅ **可以** 读文件, grep, 写 handoff, 写 ADR, 写 test
- ✅ **可以** 跑 build + NSIS 增量 rebuild (5 binaries), 但 installer 不 SFTP

---

## 8. Session health

- bash 用量: ~30 calls (红线 80)
- 3 subagent dispatched (general-purpose, read-only)
- 0 source file modified
- 1 handoff written (本文件)
- 活跃 ~2h+ (CLAUDE.md 2h 规则, **强烈建议开新会话接力**)
- jsonl 健康 (< 200KB)

---

## 9. 下个会话第一句话

```
承接 handoff-after-phase-L-bugfix-9-v0.73-reanalysis-2026-07-30.md。
v0.73 装机后切输入法无反应 (IME dead)。
本会话目标:
1. 等 user 跑 5 个 PowerShell 命令抓证据 (见 handoff §3)
2. 对照 v0.73 期望 md5 (0424EB54 / 231FD0CF / EA4DECE5 / F8D63534) 
   + 3 个 TSF 注册表键, 定位 A/B/C 真因
3. 写 ADR + 修复方案 (需 user 授权 install.nsi)
4. 加 ship-gate 硬门禁 (subagent 3 建议 3 条, user 选哪几条)
5. 不 git push (K7 仍 500), 3 commits 暂存本地
```

---

*End of Phase L Bugfix 9 v0.73 reanalysis handoff. v0.73 test build 装机失败, 
3 subagent 已分析 + 大白话复盘已输出, 等 user 跑 5 个 PowerShell 命令 
抓真证据升级 subagent 推断到真因. 下个会话接力修 installer / binary / 
registry 路径. session 活跃超 2h, 强烈建议开新会话.*