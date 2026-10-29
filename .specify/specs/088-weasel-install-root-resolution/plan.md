# 088 · Plan — 火流猩程序目录解析

> 配套 `spec.md`（WHAT 层，用户体验）+ `tasks.md`（执行清单）。

## 背景与取证

用户报障：装 1.1.3 后，在任务栏输入法图标菜单点「重启算法服务」「重新部署」，快捷设置栏仍会消失。诊断过程中顺带查出 fork 级真缺陷。

### 取证（2026-10-29，本机实测）

| # | 事实 | 取证方式 |
|---|---|---|
| E1 | 运行中的服务是 `D:\Program Files\fluxing\weasel\WeaselServer.exe`，Product **1.1.3.0** | `Get-Process WeaselServer` + 文件版本 + SHA256 |
| E2 | 64 位 CLSID 指向 `weaselx64.1.1.3.0.dll` | `reg query HKLM\SOFTWARE\Classes\CLSID /s /f weaselx64.1.1.3.0.dll /d` |
| E3 | 安装器**只写** `HKLM\Software\Fluxing\Weasel :: InstallDir` | `output/install.nsi:447` |
| E4 | 安装器**从不写** `WeaselRoot` | 全仓 grep：唯一写点 `WeaselSetup/imesetup.cpp:376`（上游键） |
| E5 | `InstallDir` 落在 **32 位视图**（`HKLM\SOFTWARE\WOW6432Node\Fluxing\Weasel`），64 位视图下该键**不存在** | `reg query` 双视图对比 |
| E6 | 二进制在 `<InstallDir>\weasel`，即 `output/install.nsi` 的 `WEASEL_ROOT = $INSTDIR\weasel` | `output/install.nsi:25` + 装机目录实测 |
| E7 | 前端取程序目录的代码读的是 `WeaselRoot` 值 | `WeaselTSF/LanguageBar.cpp:306`、`:315` |
| E8 | 前端取程序目录的代码在 32 位分支读的是**上游键** `Software\WOW6432Node\Rime\Weasel` | `WeaselTSF/LanguageBar.cpp:37-45` |
| E9 | 「重启算法服务」「程序文件夹」失败时**静默无提示**（`RegGetStringValue != ERROR_SUCCESS` ⇒ 整段跳过） | `WeaselTSF/LanguageBar.cpp:313-326` |
| E10 | 算法服务不在时前端自动拉起，走同一个 `_GetRootDir()` ⇒ 同样空路径 | `WeaselTSF/WeaselTSF.cpp:493` |
| E11 | 「重新部署」能工作：它走 `default:` 分支转发给服务端，服务端执行 `WeaselDeployer.exe /deploy` | `LanguageBar.cpp:352` + `WeaselServerApp.cpp:299-301` |

### 结论

- 「快捷设置栏消失」与本缺陷**无关**：常驻勾选项在**服务端托盘图标右键菜单**里（`ID_QUICKPANEL_ALWAYS_SHOW`，`WeaselServer/WeaselServer.rc:164`），不在任务栏输入法图标菜单，也不在面板内。未勾选 ⇒ 瞬时模式 ⇒ 2 秒 grace + 鼠标在外 1.5 秒自动隐藏，这是**预期行为**。
- 「重启算法服务」「程序文件夹」在 fork 下是**静默空操作**（E3+E4+E7+E9）。
- 算法服务意外退出后前端**无法自动拉起**（E7+E10）。
- 本 spec 只修 E3/E4/E7/E8/E9/E10 这一族缺陷。

## 技术方案

### 单一事实源

新增 `include/FluxingInstallPaths.h`（header-only，依赖仅 `windows.h` + `<filesystem>` + `<string>` + `<vector>`），
与既有 `include/FluxingUserPaths.h`（ADR 0020 用户数据目录）同一范式。

为什么单独成文件：`include/WeaselUtility.h` 依赖 `stdafx` 预置头并拖进 COM/wrl，
而 `WeaselTSF` 与单元测试工程都不适合直接包含它。

### 解析契约

| 优先级 | 注册表键 | 值 | 得到的目录 |
|---|---|---|---|
| 1 | `Software\Fluxing\Weasel` | `WeaselRoot` | 值本身 |
| 2 | `Software\Fluxing\Weasel` | `InstallDir` | `<值>\weasel`，再 `<值>` 兜底 |
| 3 | `Software\Rime\Weasel` | `WeaselRoot` | 值本身 |
| 4 | `Software\Rime\Weasel` | `InstallDir` | `<值>\weasel`，再 `<值>` 兜底 |

- 每个键都**显式遍历 32/64 两个系统视图**（`KEY_WOW64_64KEY` / `KEY_WOW64_32KEY`），
  不再依赖调用方进程的字数 —— E5 证明两种视图都真实存在。
- 候选目录按顺序筛选：第一个「像程序目录」的（内含 `WeaselServer.exe` 或 `start_service.bat`）胜出；
  一个都不像时返回第一个非空候选，供调用方把路径报给用户（FR-004）。
- 启动目标：优先 `start_service.bat`，缺失时退回 `WeaselServer.exe`（FR-005）。

### 调用点改造

| 文件 | 现状 | 改造后 |
|---|---|---|
| `WeaselTSF/LanguageBar.cpp:37-45` | `GetWeaselRegName()`：32 位读上游键、64 位读 fork 键 | **删除**，改由统一解析器承担双视图 |
| `WeaselTSF/LanguageBar.cpp:304-308` | `_GetRootDir()` 读 `WeaselRoot` | 走统一解析器；再兜底用「本 DLL 自身所在目录」 |
| `WeaselTSF/LanguageBar.cpp:313-326` | 读不到就整段跳过（静默） | 解析失败 → 简体中文 `MessageBoxW` 提示 |
| `WeaselTSF/WeaselTSF.cpp:493` | 空路径 → `ShellExecuteW` 拼出 `\start_service.bat` | 用统一「启动目标」选择器；目标为空则跳过拉起（不再执行坏路径） |

### 为什么不改安装器

- 改 `output/install.nsi` 会触发 AGENTS.md §2.5 的 Windows Sandbox harness 门禁，**本机跑不起来**（Hyper-V vSwitch 异常）。
- 契约侧已经自洽（spec 002 FR-006）：安装器只写 `InstallDir`，前端按该契约解析即可。
- ⇒ 本次**不碰** `output/install.nsi` 与 `output/data/user-custom/**`。

## Constitution Check

| 原则 | 结论 | 说明 |
|---|---|---|
| I. Intent Before Implementation | ✅ | 本 plan 顶部即「背景与取证」，意图=让托盘菜单两个入口真的生效，验收=SC-001~SC-003 可观察 |
| II. Test-Backed Change | ✅ | 新增 `test/TestFluxingInstallPaths`（纯逻辑单测 + 旧逻辑对照断言），并入全套件门禁 |
| III. Spec-Artifact Discipline | ✅ | 本 spec 触及 ≥3 文件 ⇒ 建 `spec.md` + `plan.md` + `tasks.md` |
| IV. Structured Clarification | ✅ | 无 `[NEEDS CLARIFICATION]`：根因已由 E1-E11 取证坐实，无需用户补充信息 |
| V. Incremental Delivery | ✅ | 单一垂直切片：解析器 → 两个调用点 → 测试 → 门禁 → 打包 |

### P 规则逐条

| 规则 | 结论 | 说明 |
|---|---|---|
| P1 代码风格 | ✅ | `#pragma once`；函数 PascalCase、常量 kCamelCase；改完跑 clang-format（仅本次改动行） |
| P2 Windows 约束 | ✅ | 用户可见提示为简体中文；启动动作放独立线程，不在输入法回调线程做阻塞 I/O；不改进程间通信语义 |
| P3 构建与依赖 | ✅ | 不引入第三方依赖；只动 `WeaselTSF` 与测试工程；`weasel.sln` 保持项目成员同步 |
| P4 提交与 PR | ✅ | `fix(WeaselTSF): ...` + `docs(fluxing): ...` + `chore(fluxing): ...`；`CHANGELOG.md` 补「主要更新」 |
| P5 UX | ✅ | 不改任何默认快捷键、方案行为、用户可见文案语义（只新增失败提示） |
| P6 向后兼容 | ✅ | 不改用户数据路径；旧键读取继续可用（spec 002 FR-008） |
| P7 子进程生命周期 | ✅ | 不新增长驻进程；仍由 `WeaselServer` 监督 |
| P8 fork 披露 | ✅ | 见下 |

### P8 必披露项

1. **是否触及旧用户数据路径**：**不触及**。本次只解析**安装根**（程序目录），完全不碰 `%AppData%\Rime` / `%AppData%\fluxing` 用户数据目录，也不触发 P6 豁免条款。
2. **是否引入新的进程间通信消息类型**：**不引入**。`include/WeaselIPCData.h` 不动，服务端不改通信语义。
3. **`CHANGELOG.md` 条目**：需要（用户可见：托盘菜单两个入口从「静默无反应」变为「可用 / 有中文提示」），记在 `[1.1.4]` 的「### 主要更新」。

## 风险与回滚

| 风险 | 缓解 |
|---|---|
| 32 位视图/64 位视图读错 | 显式双视图遍历 + 文件系统校验（内含 `WeaselServer.exe`/`start_service.bat`） |
| 用户自定义安装目录 | 契约来自安装器写入的 `InstallDir`，与自定义目录无关 |
| 老机器上 `WeaselRoot` 存在但目录已删 | 校验不通过 ⇒ 落到下一个候选（`InstallDir` 推导） |
| 回滚 | 单笔提交可整体回退；不改安装器、不改用户数据 ⇒ 无数据迁移需要回滚 |

## 复杂度跟踪

- 未引入新依赖（header-only + 标准库）。
- 未改安装器、未改服务端、未改通信协议 ⇒ 影响面限于输入法前端。
