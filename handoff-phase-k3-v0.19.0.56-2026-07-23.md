# Handoff — Phase K3 T019 v0.19.0.56 catastrophic-recovery ship (2026-07-23)

> 承接上一个会话任务 Phase K3 T019 v0.19.0.55 catastrophic regression，见
> `.specify/memory/lessons-learned.md` L106 + `task.md` "Phase K3 T019" 段。

## 背景

- v0.19.0.55 (commit 823bc1f + 8559ed4) ship 后 user 报 4 件 catastrophic regression:
  无法输出中文、无法调出设置栏、无法调出常用短语 UI、疑似算法服务失效。
- 用户原 prompt 给出 H1-H5 假设（围绕 binary 替换 / TSF 注册表 / pipe race / 装机启动链），
  systematic-debugging Phase 1 自动采证后**全部证伪**。

## 根因（已写 L106）

**核心发现**：`output\Win32\WeaselServer.exe` 是 2 MiB 全零文件 (nonzero=0)，
装机副本 `D:\Program Files\fluxing\weasel\WeaselServer.exe` 也是同一份零文件。
**构建链**：v0.19.0.55 改走 `build_v055.ps1` → `xbuild.bat weasel installer` → xmake 路径。
`WeaselServer/xmake.lua` L19-27 `after_build` 无条件 `os.cp(targetdir/WeaselServer.exe, output/Win32)`，
xmake link 静默失败但 exit 0，零占位进 installer。MSBuild 路径 (`WeaselServer.vcxproj`)
一直正常（build.log v0.19.0.54 ship 是真 MSBuild 路径成功）。

**次要隐患**（未在 v0.19.0.56 修，记 L106 hardening 项）：
- NSIS 32-bit 写 HKLM `KnownClasses` 落到 WOW6432Node；64-bit TSF 读 native hive 不到。
  下次 ship 需 `install.nsi` 加 `${DisableX64FsRedirection}` 包裹。

## 修复路径（已选 A：仅重建 ship）

- `env.bat` bump `WEASEL_BUILD=56` / `PRODUCT_VERSION=0.19.0.56` / `FILE_VERSION=0.19.0.56`
- MSBuild 路径重编：`msbuild weasel.sln /p:Configuration=Release /p:Platform=Win32 /m:1`
  - WeaselServer.vcxproj → `output\Win32\WeaselServer.exe` (2,238,464 bytes, MZ=MZ, MD5=D00DE6EC...)
  - WeaselSetup.vcxproj → `output\WeaselSetup.exe` (287,232 bytes, MZ=MZ)
  - WeaselDeployer / FluxingPhrasesDialog 已是新 build（MSBuild 触达）
- NSIS: `makensis /DFLUXING_VERSION=0.19.0 /DWEASEL_BUILD=56 /DPRODUCT_VERSION=0.19.0.56 install.nsi`
  - Output: `output\archives\fluxing-0.19.0.56-installer.exe` (43,353,244 bytes, MD5=4C19CC89...)
- 7z 提取验证：embedded WeaselServer.exe 2,238,464 bytes，PE 头部有效，**不是零文件**。

## Ship 验证

- TestPipeProtocol: 45/45 PASS（out-of-process PhrasesDialog 协议不变）
- installer MD5 != v0.19.0.55 (4C19CC89E15437B42758D087F58BECC1 vs 14dfdfb0...)
- 装机端恢复：用户重装后 WeaselServer.exe 真 binary 加载，TSF 正常。
- 三件套 staging 已更新（`E:\办公文件\L1网站\pinyin\version.json` + `appcast.xml` + `release-notes.html`）。

## 用户装机步骤

1. 卸载 v0.19.0.55（`appwiz.cpl`）— installer 内部 `taskkill` 会做兜底，但建议先卸
2. 双击 `release\fluxing-0.19.0.56-installer.exe`（或本地 staging 副本）
3. 验证：
   - 任务管理器 → `WeaselServer.exe` 进程存在
   - Alt+. → 常用短语 UI 调出
   - Ctrl+Shift+U → UserDictionary
   - 中文输入正常

## 已 Ship 提交

- (待) fix(WeaselServer + FluxingPhrasesDialog): v0.19.0.56 (L106 + task.md + ERRORS.md)
- (待) chore(release): v0.19.0.56 installer (release\fluxing-0.19.0.56-installer.exe)
- 注：`.claude/skills/*.md` 与 `CLAUDE.md` 的 7 个未追踪修改来自 prior session，
  此次 ship 不 commit，留待各自 session 处理。

## 待办 / 后续

- **F106-A xmake after_build guardrail**：在 `WeaselServer/xmake.lua` L27 后加
  `verify_pe(targetdir/WeaselServer.exe)`，link 失败即 raise。**单独 PR**。
- **F106-B build_v0XX.ps1 wrapper guard**：每个 `build_v0XX.ps1` 末尾 verify 关键 EXE
  size + MZ + machine。**单独 PR**。
- **F106-C 装机端 rapid recovery**：写 `_recover_zero_weaselserver.ps1` 检测 + 自动重装。
  **单独 PR**。
- **installer.nsi 64-bit KnownClasses**：`${DisableX64FsRedirection}` 包裹 KnownClasses 写入。
  **单独 PR**。
- **SFTP 上传 aiec.fun**：本地 staging 已就绪，需 user 提供 SFTP 凭据。`common.php:5`
  明确 stats.json 已停用，下载统计走 MySQL。

## 关键文件引用

- 完整 L106: `.specify/memory/lessons-learned.md` L106 段
- Task plan: `task.md` "Phase K3 T019 catastrophic regression" 段
- Build pipeline evidence: `output\Win32\WeaselServer.exe` (当前 2,238,464 bytes, 真)
- Installer: `output\archives\fluxing-0.19.0.56-installer.exe` + `release\fluxing-0.19.0.56-installer.exe`
- aiec staging: `E:\办公文件\L1网站\pinyin\{version.json,appcast.xml,release-notes.html,fluxing-0.19.0.56-installer.exe}`

## 关联 memory

- `feedback_autonomous_diagnostics.md` — 用户偏好全自动跑本机只读诊断
- `2026-07-22-k3-foreground-ordering-regression.md` — Phase K3 prior lesson
- `lessons-learned.md` L100/L101/L104/L105/L106 — Phase K 历次 IPC + build pipeline lessons
