# 088 · Tasks — 火流猩程序目录解析

> 单一垂直切片。优先级 P1/P2/P3，用户故事标签 `[US1]` `[US2]` `[US3]`。
> 状态标记：`[ ]` 未开始 · `[~]` 进行中 · `[x]` 完成。

## T1 — 单一事实源：程序目录解析器 `[P1] [US1] [US2] [US3]`

- **输入**：`include/FluxingUserPaths.h`（既有范式）、`output/install.nsi:25`（`WEASEL_ROOT = $INSTDIR\weasel`）、plan.md 解析契约表。
- **输出**：`include/FluxingInstallPaths.h`（新建，header-only）。
- **做什么**：
  - `BinaryRootFromInstallDir()`：安装根 → `<安装根>\weasel`（纯字符串映射）。
  - `LooksLikeWeaselRoot()`：目录内含 `WeaselServer.exe` 或 `start_service.bat` 才算「像程序目录」。
  - `ExpandRootDirs()` / `PickWeaselRoot()`：候选展开 + 筛选（全不通过时返回第一个非空候选）。
  - `WeaselServiceLaunchTarget()`：优先 `start_service.bat`，退回 `WeaselServer.exe`。
  - `InstallRootSources()`：4 条候选来源（fork 键优先、旧键兜底）。
  - `detail::ReadRegStringAnyView()`：**显式遍历** `KEY_WOW64_64KEY` / `KEY_WOW64_32KEY`。
  - `WeaselRootDirCandidates()` / `ResolveFluxingWeaselRoot()`：端到端解析。
- **验收**：同一份实现同时被 `WeaselTSF` 与单元测试使用（FR-006）；`WeaselTSF` 与测试工程都能直接包含该头（不拖 COM/wrl）。

## T2 — 前端改造：两个菜单入口 + 自动拉起 `[P1] [US1] [US2] [US3]`

- **输入**：T1 的头文件。
- **输出**：`WeaselTSF/LanguageBar.cpp`、`WeaselTSF/WeaselTSF.cpp`。
- **做什么**：
  - 删除 `LanguageBar.cpp` 里的 `GetWeaselRegName()`（32 位读上游键的错写法）。
  - `_GetRootDir()` 改走统一解析器；再兜底用「本 DLL 自身所在目录」（`GetModuleHandleExW` + `GetModuleFileNameW`）。
  - `_HandleLangBarMenuSelect()` 的 `ID_WEASELTRAY_RERUN_SERVICE` / `ID_WEASELTRAY_INSTALLDIR` 分支：解析失败 → 简体中文 `MessageBoxW`；不再静默跳过。
  - `WeaselTSF.cpp` 的「服务未运行自动拉起」改用统一「启动目标」选择器。
- **验收**：SC-001 / SC-002 / SC-003 可观察；不阻塞输入法线程（P2）。

## T3 — 回归测试 `[P1] [US1] [US2] [US3]`

- **输入**：T1 的头文件。
- **输出**：`test/TestFluxingComponents/TestFluxingInstallPaths.cpp`（挂进既有 `TestFluxingComponents` 工程，随其一起进全套件，避免新增工程要同步 `weasel.sln` 的 16 处平台配置）。
- **做什么**：
  - 纯逻辑断言：安装根 → 程序目录映射（含尾随分隔符、空串、盘符根）。
  - 候选筛选断言：第一个不像的候选要被跳过；全不像时返回第一个非空候选。
  - 启动目标断言：有 bat 取 bat、只有 exe 取 exe、都没有取空。
  - **旧逻辑对照断言**：复刻上游「只读 `WeaselRoot`」的解析，在本机必须解析不到 fork 的程序目录（负对照，证明测试有牙）。
  - 端到端断言：机器上存在安装根时，解析结果必须像程序目录。
  - 把新工程接进全套件的 build/run 列表。
- **验收**：全套件 `SUITE_RC=0`；负对照在本机为真。

## T4 — 门禁 `[P1]`

- **输入**：T1-T3 的改动。
- **输出**：clang-format 输出、构建与测试日志。
- **做什么**：
  - `clang-format` 仅作用于本次改动行（不外扩）。
  - `scripts/test-infra/run-test-suite.bat` 全套件。
  - `build.bat weasel installer` 完整构建。
- **验收**：全套件 exit 0；构建 exit 0；产物版本号一致。

## T5 — 版本线与载荷 `[P2]`

- **输入**：T4 通过的代码。
- **输出**：`env.bat`（不入仓）、`build.bat` / `xbuild.bat` 的 `VERSION_*`、`weasel.props`（生成物）、`release/fluxing-1.1.4-installer.exe`。
- **做什么**：版本升到 1.1.4，重建安装包，回填 SHA256 与大小。
- **验收**：安装包 Product 版本 = 1.1.4.0；`release/` 载荷与 `output/archives/` 一致。

## T6 — 文档收口 `[P2]`

- **输入**：T1-T5 的最终状态。
- **输出**：`CHANGELOG.md`（`[1.1.4]` + `### 主要更新`）、`.specify/memory/lessons-learned.md`（L159）、`docs/adr/0021-*.md`、`docs/archive/handoffs/handoff-v1.0.0-bare-shift-ascii-toggle-2026-10-28.md`（§23）。
- **做什么**：把「注册表视图」这个坑写进 lessons-learned（下次别再踩）；把「程序目录解析契约」写进 ADR。
- **验收**：文档与代码口径一致；CHANGELOG 有用户可见条目。

## T7 — 提交与推送 `[P2]`

- **输入**：T1-T6 全部落盘。
- **输出**：三笔提交（代码 / 文档 / 载荷）。
- **做什么**：按仓内惯例拆分，`git status -sb` + `git diff --stat` 先确认不带入 `weasel.props` / `env.bat` / `*.obj`，推到 `kizemo/Fluxing`。
- **验收**：推送后 HEAD == 远端 HEAD；工作区仅剩子模块与 `tmp/` 噪音。
