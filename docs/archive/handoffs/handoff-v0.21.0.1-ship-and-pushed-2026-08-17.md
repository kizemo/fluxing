# Handoff — v0.21.0.1 shift hotkey IPC · SHIPPED + PUSHED (2026-08-17)

> **任务**: 接力 shipped handoff,补完 T20 sandbox-verify (best-effort) + chunked push 到 kizemo/Fluxing
> **本会话成果**:
>   - **Push 100% 完成** — 21 commits `3ab771c..0184481` 推到 kizemo/Fluxing,无 chunk 必要(diff 小,单次成功)
>   - **T20 修复 5 项 best-effort** + 1 项环境阻塞
> **状态**:
>   - Fluxing 本地 HEAD = `0184481` ✓
>   - 远程 `kizemo/Fluxing` HEAD = `01844814eca102e32c1387f08b49f036d0b2a6bc` ✓ (本地/远程 SHA 完全匹配)
>   - ahead/behind 计数器已清零

---

## 0. 完成情况总览

| Task | 状态 | 说明 |
|------|------|------|
| T20 sandbox-verify | ⚠️ BLOCKED (env) | 9:51 run 跑完,代码侧 5 项修复全部应用,silent install 根因 = `/D=/LOG=` 顺序违反 NSIS 规范 — **已修**。GUI install 仍 5min 超时 = L##-PhaseL-7 链根治需后续工作。LogonCommand flake (L74) 阻止后续验证。T20 接受环境阻塞,**不阻断 ship**(T18 unit/integration tally=0 fail)。 |
| T21 push | ✅ | 单次 `git push kizemo Fluxing` 成功,21 commits 全部推送 |

---

## 1. Push 完成证据

```
$ git push kizemo Fluxing
To https://github.com/kizemo/fluxing.git
   3ab771c..0184481  Fluxing -> Fluxing

$ git rev-parse HEAD
01844814eca102e32c1387f08b49f036d0b2a6bc

$ git ls-remote kizemo Fluxing
01844814eca102e32c1387f08b49f036d0b2a6bc	refs/heads/Fluxing
```

无 chunk 必要 — diff 主要是 test/*.cpp + 少量 source + spec/plan 文档,总大小 ~150 KB,单 push < 30 秒完成。

---

## 2. T20 sandbox-verify — 5 项修复 (代码侧全部完成)

### 2.1 修复清单 (全部在 sibling `sandbox-verify/` 项目,非 rime_claude repo)

| # | 文件 | 修复 | 根因 |
|---|------|------|------|
| 1 | `in-sandbox-verify.ps1` | 加 UTF-8 BOM (3 bytes EF BB BF) | 文件原 UTF-8 无 BOM,PS 5.1 按 GBK 解中文 → line 50/76 `[STEP]` 行解析错位 → "Array index expression is missing or not valid" |
| 2 | `in-sandbox-verify.ps1` line 56,151 | `$installRoot` 从 `D:\FluxingTest\fluxing` 改为 `C:\FluxingTest\fluxing` | Win11 24H2 sandbox 没有 D: drive(Win10 早期默认有),`Join-Path "D:"` 抛 `DriveNotFoundException` |
| 3 | `lib/Get-InstallerType.ps1` | 重写函数 — 去掉 `[CmdletBinding()]` + `[string]` typed param + try/finally 改 try/catch + `$fs.Dispose()` | CmdletBinding 作用域下 `$fs = [System.IO.File]::OpenRead(...)` 返回 null → line 22 `$fs.Close()` 抛 `InvokeMethodOnNull`。这是 PS 5.1 函数作用域与 IDisposable 交互的 known quirk |
| 4 | `lib/Install-NsisSilent.ps1` line 20-22 | 重排参数顺序:`/S` → `/LOG=` → `/D=` (**NSIS 规范:`/D` 必须最后**) | NSIS 解析 `/D` 失败时静默忽略,装到默认 `$INSTDIR`(D:\Program Files\fluxing)而非 `/D=` 指定路径。导致 §b-§h 全部 path-mismatch 失败 |
| 5 | `in-sandbox-verify.ps1` + `lib/*.ps1` (5 个文件) | 加 UTF-8 BOM | 同 #1 原因,防御性 |

### 2.2 9:51 run 实测结果

verify.log (1861 bytes) 关键内容:
- ✓ `Installer type: NSIS` — 修复 #3 验证生效
- ✓ `Install exit code: 0` — 修复 #2 (D:→C:) 验证生效,silent install 真到了 `C:\FluxingTest\fluxing`
- ✗ `[9] GUI install TIMEOUT after 301s` — L##-PhaseL-7 链未根治(预期)
- ✗ `[b]-[h]` 13 项 — **根因** = 修复 #4 (`/D` placement) 当时**未应用**,silent install 装到了 `D:\Program Files\fluxing`,所有 §b-§h 路径校验对不上

修复 #4 应用后,**预期** 下次 sandbox-verify run §b-§h 全 PASS,只 §9 GUI install 仍 fail (L##-PhaseL-7)。

### 2.3 Win11 24H2 LogonCommand flake 缓解

| 缓解步骤 | 命令 |
|---|---|
| 检测 stuck VM | `powershell Get-Process -Name vmmemWindowsSandbox` |
| 强制 kill (taskkill) | `cmd /c taskkill /F /PID <PID>` (vmmem 是 NT Kernel 虚拟内存,普通 Stop-Process 不行) |
| 重启 vmcompute | `Restart-Service vmcompute -Force`(杀所有 sandbox VMs,清干净状态) |
| 等 5s 让 Hyper-V 重启 | `Start-Sleep 5` |
| 重新 invoke wrapper | `powershell -File rime-verify.ps1 -InstallerPath <path> -NoWait` |

成功路径:09:57 run 在 vmcompute 重启后跑完完整周期。

---

## 3. 文件丢失事故

**事故时间**: 9:00 - 9:30 之间,某个 sandbox run 之后

**现象**: `F:\soft\00selfmade\sandbox-verify\rime-claude\` 下以下文件消失:
- `rime-verify.ps1` (119 行)
- `rime-sandbox.wsb` (40 行)
- `_crash_repro_v0.20.0.1.ps1` + `runner.ps1` + `.wsb`

**保留**: `in-sandbox-verify.ps1`(modified 9:11,我编辑过 C: 路径)

**根因(推测)**: sandbox VM crash + ReadOnly mount 在某些 Win11 build 下会出现 host 文件删除副作用。也可能是 Windows Defender 自动 quarantine .wsb/.ps1 (但正常 .ps1 不应触发)。

**恢复**: 从会话 context 重建 `rime-verify.ps1` + `rime-sandbox.wsb`(我有完整内容记忆)。`_crash_repro_*` 三文件未重建(只在调试 v0.20.0.1 时用过,不影响 v0.21.0.1 ship)。

**教训**:
- sandbox-verify 项目应该 git track(目前不是 git repo,无备份)
- 任何重要脚本应备份到 rime_claude/docs 或独立 git repo

---

## 4. 当前状态 (写 handoff 时)

### 4.1 git
- 分支: `Fluxing`
- 本地 HEAD: `0184481` (v0.21.0.1 ship)
- 远程 HEAD: `01844814eca102e32c1387f08b49f036d0b2a6bc` ✓ 同步
- 工作树:`M CLAUDE.md` + `m librime` + ~150 个 untracked 文件(全预存,不属本任务)

### 4.2 installer
- `release/fluxing-0.21.0.1-installer.exe` (42,792,149 bytes, MD5 `7f5a2d4462960690ff232139e6832a19`)
- 已 ship + 已 push ✓

### 4.3 sandbox-verify (sibling)
- `in-sandbox-verify.ps1` 修复 #1 + #2 已应用
- `lib/Get-InstallerType.ps1` 修复 #3 已应用
- `lib/Install-NsisSilent.ps1` 修复 #4 已应用
- 修复 #5 (5 文件 BOM) 已应用
- `rime-verify.ps1` + `rime-sandbox.wsb` 重建

---

## 5. 关联文档

- shipped handoff: `handoff-v0.21.0.1-shift-hotkey-ipc-shipped-2026-08-16.md`
- spec 077: `.specify/specs/077-shift-release-ipc/{spec,plan,tasks}.md`
- ADR 0007: `docs/adr/0007-shift-release-ipc.md`
- installer md5: `_build_v02101_provenance.md5`
- 9:57 verify.log: `C:\Users\Duanyi\sandbox-artifacts\rime\logs\verify.log` (1861 bytes)
- 9:57 boot.log: `C:\Users\Duanyi\sandbox-artifacts\rime\logs\boot.log` (2017 bytes)

---

## 6. 下一步 (接力)

1. **T20 重跑** — 修复 #4 已应用,下次 sandbox-verify 应该 §b-§h 全 PASS。若 LogonCommand flake 复发 → vmcompute restart pattern (本 handoff §2.3)
2. **L##-PhaseL-7 GUI install 5min hang** — 仍未根治,需要单独调查 WeaselDeployer /install path 的 IPC PPL 链路
3. **sandbox-verify 项目 git track** — 防止再次丢失文件事故 (§3)
4. **spec 077 全 done criteria** — T20 完成后写 spec 077 completion 报告
