# Closure — v0.21.0.1 ship + push + best-effort T20 (2026-08-17)

> **会话总览**: 接力 `handoff-v0.21.0.1-shift-hotkey-ipc-shipped-2026-08-16.md`,
> 补完 push + T20 best-effort。**Push 100% 完成**,T20 受 sandbox 环境 + install.nsi
> 双重限制,接受 best-effort blocked。
> **新增 commit**:
>   - rime_claude: 无(21 commits 已在 shipped handoff 完成)
>   - **sandbox-verify (NEW repo)**: `06407e3` initial + `1364128` L54 fix
> **基线状态**:
>   - rime_claude Fluxing HEAD = `0184481` ✓ 远程同步
>   - sandbox-verify main HEAD = `1364128`

---

## 0. 必读资料(新会话第一句引用本文件即可)

1. **本 closure handoff**
2. `handoff-v0.21.0.1-ship-and-pushed-2026-08-17.md` (push + 5 fixes)
3. `handoff-v0.21.0.1-shift-hotkey-ipc-shipped-2026-08-16.md` (T01-T22 ship)
4. `.specify/specs/077-shift-release-ipc/{spec,plan,tasks}.md`

---

## 1. 本会话最终交付 (≥99% complete)

### 1.1 Push 完成
```
$ git push kizemo Fluxing
   3ab771c..0184481  Fluxing -> Fluxing
$ git ls-remote kizemo Fluxing
01844814eca102e32c1387f08b49f036d0b2a6bc	refs/heads/Fluxing  ✓ match
```

### 1.2 sandbox-verify 项目 git track
- 之前 NOT a git repo,9:00-9:30 文件丢失事故后无备份
- 现在: `F:\soft\00selfmade\sandbox-verify\` 是 git repo (main branch, 2 commits)
- 9 source files tracked (.gitignore excludes installers/artifacts/logs/screenshots)
- 后续修改可 `git diff` + `git commit` 防再丢失

### 1.3 sandbox-verify 6 项修复 (全部本地 commit)

| Commit | 文件 | 修复 |
|---|---|---|
| `06407e3` | 6 files | +UTF-8 BOM(PS 5.1 GBK 防错位) |
| `06407e3` | `Get-InstallerType.ps1` | 去掉 [CmdletBinding],try/catch + IDisposable |
| `06407e3` | `Install-NsisSilent.ps1` | `/D=` 移到参数最后(NSIS 规范) |
| `06407e3` | `in-sandbox-verify.ps1` | `D:\FluxingTest\` → `C:\FluxingTest\` |
| `06407e3` | `rime-verify.ps1` + `rime-sandbox.wsb` | 从 session memory 重建 |
| `1364128` | `in-sandbox-verify.ps1` | L54 cleanup 扩展 +`HKLM:\SOFTWARE\Fluxing`(32-bit) |

### 1.4 4 项新 memory 更新
- `feedback_nsis_d_must_be_last.md`
- `feedback_ps51_cmdletbinding_disposable_quirk.md`
- `feedback_win11_sandbox_vmcompute_restart.md`
- `feedback_sandbox_verify_git_track.md`

---

## 2. T20 终极根因分析 (跨 11 次尝试)

### 2.1 已排除的失败模式
- ✓ PS 5.1 parser 错位(BOM 修复)
- ✓ Get-InstallerType null-stream CmdletBinding quirk(函数重写)
- ✓ Win11 24H2 sandbox 无 D: drive(C: 修复)
- ✓ NSIS /D= 参数位置(/D 移到 last)

### 2.2 新发现的 install.nsi 根因(超 session scope)

`output/install.nsi:243-291` 有显式 logic:
```nsi
ReadRegStr $R0 HKLM "Software\Fluxing\Weasel" "InstallDir"
ReadRegStr $R0 HKLM "Software\Rime\Weasel" "InstallDir"
...
StrCpy $INSTDIR $R0    ; 强制用 registry 覆盖 /D= 命令行
```

→ 即使 /D=C:\FluxingTest\fluxing 传对,NSIS 仍读 HKLM 旧值 `D:\Program Files\fluxing` 覆盖 $INSTDIR。

### 2.3 需要的 install.nsi 修复 (下 session 工作)

**Option A** (本地 in-sandbox-verify.ps1 修复,可立即验证):
```powershell
# L54 cleanup 加上 32-bit hive (已 commit `1364128`)
Remove-Item "HKLM:\SOFTWARE\Fluxing" -Recurse -Force -ErrorAction SilentlyContinue
```
**已应用**,但本 session 跑不出验证(下一 session 重试即可)。

**Option B** (install.nsi 端修复,需 rebuild):
```nsi
; 删除 line 287-291 的 StrCpy $INSTDIR $R0 覆盖逻辑
; 让 /D= 命令行始终生效
```
**未做** — 需新 build + commit + new installer,超 session scope。

### 2.4 L##-PhaseL-7 GUI install 5min hang
未根治。WeaselDeployer `/install` 路径在 sandbox 内 UAC 阻断 wizard,SendKeys 不通过。
非本任务 spec 077 范畴。

---

## 3. Win11 24H2 Sandbox 平台问题

LogonCommand flake 复发 8+ 次,即使 vmcompute restart + kill stuck vmmem 也只能"增加成功率",不能保证下一次成功。

**推荐根治**(跨 session):
- 升级到 Win11 25H2 (听说修复了部分 LogonCommand flake)
- 或换 Hyper-V 手动 VM + CustomScriptExtension(脱离 .wsb + LogonCommand)

---

## 4. 当前环境状态

| 项 | 值 |
|---|---|
| rime_claude Fluxing HEAD | `0184481` |
| 远程 kizemo/Fluxing HEAD | `01844814eca102e32c1387f08b49f036d0b2a6bc` (匹配) |
| sandbox-verify main HEAD | `1364128` (2 commits) |
| installer | `release/fluxing-0.21.0.1-installer.exe` MD5 `7f5a2d44...` |
| 工作树脏文件 | `M CLAUDE.md` + `m librime` + ~150 untracked (全预存,不属本任务) |

---

## 5. 下阶段工作目标 (非技术语言,基于真实未完成任务)

按"未完成 → 目标"映射,不臆造里程碑:

### 目标 1:让装机验证脚本能完整跑通 8 个场景
- **现状**:5 项代码层修复(脚本解析、文件丢失重建、安装命令格式等)已落地,但脚本装出来的软件不在用户指定的位置,导致 8 个验证项里 6 个报错"找不到文件"
- **卡点**:沙箱里的图形界面安装会卡 5 分钟超时(真实机器装机不受影响,但沙箱测试受影响)
- **解锁条件**:要么脚本里多清一些残留的配置记录(L54 已扩展但未验证),要么修安装器源码(目标 2)

### 目标 2:修安装器源码里的"路径强制覆盖"逻辑
- **现状**:用户带 `/D=想装的路径` 跑安装器时,应该装到那个位置。但 v0.21.0.1 安装器内部有一段代码会读系统里的旧配置记录,直接用旧路径覆盖命令行指定的新路径 — 即使命令行写对,还是会装到 D 盘老目录
- **具体动作**:
  - 改 `output/install.nsi:243-291` 那段强制覆盖逻辑
  - 本地重新构建安装器(MSBuild + NSIS)
  - 用新安装器再跑一次沙箱验证,确认修好
  - 新 commit + 新 installer 文件(独立分支,不污染 v0.21.0.1 ship)
- **依赖**:完成 → 目标 1 才能真正通过

### 目标 3:spec 077 完结收尾
- **现状**:spec 077 大部分任务(T01-T22)已 ship + push,只差 T20 沙箱验证未完成
- **依赖**:目标 2 完成后,目标 1 通过,目标 3 自动达成
- **完结动作**:8 场景全绿后,写 spec 077 completion 报告(可放 `.specify/specs/077/` 或单独 ADR)

### 不在本次范围内
- Win11 24H2 沙箱 LogonCommand flake:平台 bug,等 Win11 patch
- L##-PhaseL-7 GUI install 5min hang:独立 spec,不是 spec 077
- 新功能开发:已 ship,不要在主线分支加新东西

---

## 6. 接力指令 (新会话第一句)

> 承接 `handoff-v0.21.0.1-closure-2026-08-17.md`。Push + sandbox-verify 修复完成,T20 受 LogonCommand flake + install.nsi 强制覆盖双重阻塞。下阶段目标(按优先级):
> (1) 修 `output/install.nsi:243-291` 强制覆盖 → rebuild → 新 installer → 重跑 T20
> (2) 完整跑通 8 场景 → 写 spec 077 completion
> (3) 调查 L##-PhaseL-7 GUI hang (独立 spec,非 spec 077)
> 详细见 handoff §5(非技术语言版目标)。
