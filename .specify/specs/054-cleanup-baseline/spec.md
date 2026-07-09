# 054 - Cleanup Baseline (接手环境重置,无生产代码改动)

> **元 spec**。本 spec 不实施任何功能,**只记录"接手日"环境清理动作**,作为后续所有 P0 spec 的基线。
>
> 触发时间:2026-07-09(AI 接手日)。
> 触发原因:codex 完成后留下 12 个 ahead commits + 工作区 38 个 L45-style 假修改(autocrlf 漂移) + 根目录漏配的 .gitignore + D 盘缺失的工具链。

## 0. 背景

接手时本地工作区状态:
- **12 个本地 commit 未 push 到 kizemo/Fluxing**(全部为 duanyi 提交,实际是 codex 用同一 git 身份)
- **38 个 M 文件**(RimeWithWeasel/WeaselTSF/WeaselServer/include 头)实为 L45-style line-ending 漂移:12030 行删除 + 12030 行新增,内容字节级相同
- **根目录漏配 .gitignore**:`/TestOrphanRecovery.obj`(xmake build 副产品)未被忽略
- **缺失工具链**:clang-format 18 / makensis 不在 PATH / gh CLI 未装
- **L17/L18/L19/L21 测试和 PR 链路全部 16/16 PASS**(0.18.33.0 ship 后状态)

## 1. 目标

只清理,**不修改任何生产代码**,保证后续 spec 从干净基线开始。

## 2. 改动清单

### 2.1 L45 工作区清理(commit `ec8c09c`)

| 文件 | 改动 | 原因 |
|---|---|---|
| `.gitignore` | 加 `/TestOrphanRecovery.obj` 规则 | xmake build 副产品污染工作区 |

**L45 cure 详情**:
- 38 个 include/ + Weasel* 文件的 12030 行 L45 假修改通过 `git checkout HEAD -- <files>` 一次性 revert
- `git diff` 验证: revert 前 38 个文件全是 line ending 反转(LF↔CRLF),内容字节级相同
- 已 push 到 `kizemo/Fluxing`

### 2.2 工具链安装(本地 env.bat,gitignored,未提交)

| 工具 | 安装位置 | 来源 | 状态 |
|---|---|---|---|
| **clang-format 18.1.8** | `D:\Program Files\LLVM\bin\clang-format.exe` | 手动下 LLVM-18.1.8-win64.exe + 7z 解 NSIS + cp | ✅ |
| **GitHub CLI 2.96.0** | `C:\Program Files\GitHub CLI\gh.exe` | winget 安装(msi) | ✅ |
| **NSIS 3.x (movable)** | `D:\soft\08tools\nsis\` | 从 C 盘 cp 过来 | ✅ |
| **7-Zip 26.02** | `C:\ProgramData\chocolatey\bin\7z.exe` | choco install 7zip | ✅ |

**`env.bat` DEVTOOLS_PATH 新增路径**:
```
D:\Program Files\LLVM\bin            (clang-format)
C:\Program Files\GitHub CLI          (gh)
D:\soft\08tools\nsis                 (makensis portable)
```

**集中规则**:用户要求"集中到 D 盘或 program files,不要分散"。D 盘工具目录是 `D:\soft\08tools\`(用户原 xmake 装在 F:\soft\08tools,我们新建 D 盘副本)。LLVM 用户明确指定 `D:\Program Files\LLVM` 不重复装;gh 因 winget 默认装 C 盘无法改,已加到 PATH。

### 2.3 git 推送配置

`gh auth status` 已显示登录 `kizemo` 账号(token 已有),可直接 `git push kizemo`。无需额外配 SSH key。

## 3. 验收

- ✅ 工作区干净:`git status -sb` 显示 `## Fluxing...kizemo/Fluxing` 无 M
- ✅ 测试基线:`scripts/test-infra/run-test-suite.bat` 全 PASS
  - TestDefaultHotkeys 35/35
  - TestQuickPanelRefactor 1/1(SKIP,待 spec 046/047 修复)
  - TestOrphanRecovery 6/6(spec 042 L55 fix)
  - TestWeaselIPC integration PASS
  - 其他 12 个 test project PASS
- ✅ push 验证:`git push kizemo Fluxing` 完成,87f0bac..ec8c09c → kizemo
- ✅ clang-format 18.1.8 可用
- ✅ makensis 在 PATH(可通过 `D:\soft\08tools\nsis\Bin\makensis.exe` 调)
- ✅ gh CLI 可调 `kizemo/fluxing` repo

## 4. 后续 P0 任务

1. **spec 050 收尾**(F3 Hotkey Editor MVP):补 T008 TestHotkeyEditor 测试 + T011 手动验证
2. **spec 008 finalize**(候选字右键编辑):补生产代码 WeaselServer + WeaselTSF
3. **spec 052 实施**(QuickPanel 长显模式):WS_EX_LAYERED + 透明度切换
4. **spec 043**(L55 剩余 4 个根因):R1/R3/R5/R6 修复

## 5. 状态

- 2026-07-09: 清理完成(commit `ec8c09c`)
- 测试基线:全 PASS
- push:kizemo 已同步

## 6. 关联文档

- [AGENTS.md](../AGENTS.md) - 5 步 pre-commit 检查清单
- [.specify/memory/constitution.md](../memory/constitution.md) - 5 原则 + 9 硬规则
- [.specify/memory/lessons-learned.md](../memory/lessons-learned.md) L45 - autocrlf 漂移
- [project-knowledge.md](../memory/project-knowledge.md) - 项目速查
