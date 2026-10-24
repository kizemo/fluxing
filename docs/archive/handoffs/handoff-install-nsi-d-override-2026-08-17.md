# install.nsi /D= override fix — branch handoff (2026-08-17)

> **会话总览**: 承接 `handoff-v0.21.0.1-closure-2026-08-17.md` §5 目标 2。
> 单点修复 `output/install.nsi` use_reg 块的路径覆盖根因,新独立分支完成 + 新 installer build。
> **新增 commit**:
>   - rime_claude: `b3fba97` fix(installer) on branch `fix/install-nsi-d-override-2026-08-17`
> **基线状态**:
>   - rime_claude main HEAD = `0184481` (v0.21.0.1 ship,未污染)
>   - fix branch HEAD = `b3fba97` (未 push)

---

## 0. 必读资料

1. **本 handoff**
2. `handoff-v0.21.0.1-closure-2026-08-17.md` §5 目标 2 + §2.2 根因
3. Commit `b3fba97` 的 message (含行为矩阵验证)
4. `output/install.nsi` line 281-292 (use_reg 新逻辑)

---

## 1. 本会话交付

### 1.1 根因精准定位
NSIS InstallDirRegKey directive (line 374) + /D= 命令行 + use_reg block 三方契约。
spec 053 v3 「ALWAYS use the registry value」覆盖了 /D= 命令行,导致 silent-install 路径 clobber。

### 1.2 单点修复
- 文件: `output/install.nsi` line 281-292
- 变更: 删除 `StrCpy $INSTDIR $R0` + 更新注释 (spec 053 v3 → spec 077 follow-up)
- Net: 10 insertions, 10 deletions (line count 不变,行为完全替换)

### 1.3 Byte-level 校验 (通过)
- BOM: HAS BOM ✓
- CRLF count: 982
- LF-only count: 0 ✓
- md5: `851CC8F1FEF13472D309C69D76F2F766`

### 1.4 Installer rebuild (通过)
- 路径: `output\archives\fluxing-0.19.0.49-installer.exe`
- 大小: 42,816,511 bytes
- md5: `C32EB4D05EBA7F525EFD77FCE9657A34`
- mtime: 2026-08-17 21:48:54
- makensis exit: 0

**注意**: 版本号读自 Windows user-level env var (`FLUXING_VERSION=0.19.0`),不是 env.bat (`0.21.0`)。这是上次 build session 残留,与本 fix 无关 — fix 已编入 binary。

### 1.5 行为矩阵 (代码层验证)

| 场景 | InstallDirRegKey | /D= | use_reg 旧行为 | use_reg 新行为 |
|---|---|---|---|---|
| 新装 + 无 /D= | (no-op) | (无) | 不触发 | 不触发 ✓ |
| 新装 + /D=path | (no-op) | $INSTDIR=/D= | 不触发 | 不触发 ✓ |
| 升级(注册表有值)+ 无 /D= | $INSTDIR=reg | (无) | StrCpy (redundant) | Goto skip ✓ |
| **升级 + /D=path** | $INSTDIR=reg | **$INSTDIR=/D= (override)** | **StrCpy clobber ✗** | **Goto skip ✓ FIX** |
| 烟雾测试残留 | $INSTDIR=C:\TEMP\… | (无) | use_default 重置 | use_default 重置 ✓ |

新行为仅改变第四行(本 bug),其他场景行为完全不变。

---

## 2. 接力指令 (新会话第一句)

> 承接 `handoff-install-nsi-d-override-2026-08-17.md`。
> Branch `fix/install-nsi-d-override-2026-08-17` 已 commit `b3fba97`,main `0184481` 未污染。
> 下一步优先级:
> (1) **真机验证** install.nsi fix(最可靠,sandbox flake 风险高)
> (2) sandbox-verify 8 场景 T20(可选,目标 C:\FluxingTest\fluxing 真装入验证)
> (3) 8 场景全绿 → spec 077 completion + push fix branch
> (4) 评估是否 bump v0.21.0.2(只有真机/sandbox 验过才有意义)

---

## 3. 验证策略(避免 fix-and-break)

### 3.1 真机手动验证 (推荐路径)
不依赖 sandbox 环境,直接验证 install.nsi 的 5 种场景:

```powershell
# 0. 清理注册表 + 残留
reg delete "HKLM\Software\Fluxing\Weasel" /f
reg delete "HKLM\Software\Rime\Weasel" /f
Remove-Item "HKLM:\SOFTWARE\Fluxing" -Recurse -Force -ErrorAction SilentlyContinue

# 1. 新装 + /D=C:\FluxingTest\scenario1
& "release\fluxing-0.19.0.49-installer.exe" /S /D=C:\FluxingTest\scenario1
Test-Path C:\FluxingTest\scenario1\fluxing\weasel.exe   # True (期望)

# 2. 新装 + /D=C:\FluxingTest\scenario2 (不同路径)
& "release\fluxing-0.19.0.49-installer.exe" /S /D=C:\FluxingTest\scenario2
Test-Path C:\FluxingTest\scenario2\fluxing\weasel.exe   # True (期望)
Test-Path C:\FluxingTest\scenario1\fluxing\weasel.exe   # False (新路径,旧路径应被覆盖或共存)

# 3. 升级(已装 scenario1) + 无 /D=
& "release\fluxing-0.19.0.49-installer.exe" /S
Test-Path C:\FluxingTest\scenario1\fluxing\weasel.exe   # True (升级保留)

# 4. 升级 + /D=C:\FluxingTest\scenario3 (FIX 验证!)
& "release\fluxing-0.19.0.49-installer.exe" /S /D=C:\FluxingTest\scenario3
Test-Path C:\FluxingTest\scenario3\fluxing\weasel.exe   # True (FIX 验证关键)
Test-Path C:\FluxingTest\scenario1\fluxing\weasel.exe   # False (升级到新路径)
```

### 3.2 Sandbox verify (T20 - 受 Win11 24H2 flake 限制)
只有真机验证后才考虑 sandbox(避免 sandbox flake 误报覆盖真机结果):
```powershell
Copy-Item output\archives\fluxing-0.19.0.49-installer.exe release\
F:\soft\00selfmade\sandbox-verify\rime-claude\rime-verify.ps1 `
    -InstallerPath release\fluxing-0.19.0.49-installer.exe `
    -Scenarios "1,2,2-prime,3,4,5,G,H"
```

### 3.3 不建议路径
- ❌ 改 env.bat / weasel.props 强制 bump 版本 — 本 fix 与版本无关
- ❌ 修改 in-sandbox-verify.ps1 — sandbox flake 是平台问题,不是脚本问题
- ❌ 复制 fix installer 到主分支 — 必须先验过才能 ship

---

## 4. 关键避坑

- **不要 push fix branch 到 kizemo** — 验过再 push,避免给主分支引入未验证的 installer
- **不要 commit installer binary** — `output/archives/*` + `release/*` 已 gitignored
- **不要 commit env.bat / weasel.props** — gitignored,本 fix 无需碰它们
- **不要在 fix branch 上做 spec 077 completion** — spec 077 completion 需独立 session 写完整报告
- **不要在 fix branch 上 bump version** — 验过 fix 后,在 main 分支独立 chore(release) commit
- **L108 lesson**: 任何 rebuild 必须用 `/t:Rebuild`,避免增量漏链 (本 fix 不涉及 .sln rebuild)

---

## 5. 当前环境状态

| 项 | 值 |
|---|---|
| main HEAD | `0184481` (v0.21.0.1,ship+push 状态) |
| fix branch HEAD | `b3fba97` (未 push) |
| Branch name | `fix/install-nsi-d-override-2026-08-17` |
| install.nsi md5 | `851CC8F1FEF13472D309C69D76F2F766` |
| Installer | `output\archives\fluxing-0.19.0.49-installer.exe` (gitignored) |
| Installer md5 | `C32EB4D05EBA7F525EFD77FCE9657A34` |
| Working tree | M CLAUDE.md + m librime + ~150 untracked (全预存,与本 fix 无关) |
| Env var 干扰 | `[User]FLUXING_VERSION=0.19.0` (上次残留,非 env.bat 当前值) |

---

## 6. 不在本 handoff 范围

- spec 077 spec.md/plan.md/tasks.md 文档完成状态 (已在 v0.21.0.1 ship commit 完成)
- L##-PhaseL-7 GUI install 5min hang (独立 spec)
- Win11 24H2 sandbox LogonCommand flake (平台 bug)
- 任何 yaml / IPC / spec 077 v0.21.0.1 后续功能增强