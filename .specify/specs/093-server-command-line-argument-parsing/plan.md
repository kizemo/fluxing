# plan 093 — WeaselServer 命令行参数解析加固

> 配套 `spec.md`（缺陷与验收标准）、`tasks.md`（任务分解与执行结果）。

## 0. 一句话

把 `WeaselServer.exe` 的命令行选项判定从**内联的整串 `wcscmp`**（对 WinMain 原始 `lpCmdLine`
恒为假 ⇒ 7 个选项全是死选项）改成**共享头里的归一化解析**（宪法 R7 生产/测试同源），
并用新建的 `test/TestServerCmdLine/` 行为断言 + 调用点守卫钉死回归。

---

## 1. Constitution Check

| 条款 | 适用性 | 本次如何满足 |
|---|---|---|
| **P1** Code Style | 适用 | 新增 `WeaselServer/ServerCmdLine.h` 用 `#ifndef FLUXING_SERVER_CMDLINE_H_` 头卫（宪法明许的两种形式之一）；函数 `PascalCase`（`Parse` / `FirstToken` / `Normalize`）、枚举 `Option` 用 `PascalCase` 值名、局部变量 `camelBack`；`clang-format` 只对本次改动文件运行，**不全仓**（spec 092 已记录 `clang-format.ps1 -n` 不可全仓跑）。 |
| **P2** Windows-Specific Constraints | 适用 | **无新增用户可见字符串**（本改动不产生任何面向用户的文案）；**无新 IPC 消息类型**（见 P8 三问第 2 条）；TSF 回调零改动 ⇒ 无阻塞 I/O 风险。 |
| **P3** Build & Dependencies | 适用 | **不引入新第三方依赖**（只用 C++ 标准库 `<cwchar>`）；**VS solution 是工程成员真源** ⇒ `weasel.sln` 已登记 `TestServerCmdLine`（工程条目 + 4 条 `ProjectConfigurationPlatforms` 映射，GUID `{93F1B7C2-6E48-4D05-A1B3-8C27D4E95A16}`）；同时提供 `test/TestServerCmdLine/xmake.lua` 镜像文件。**已披露**：root `xmake.lua` 只 `includes` 了 `test/TestWeaselIPC` 与 `test/TestResponseParser` 两个测试目录，其余 20+ 测试工程同样**不在** root `xmake.lua` 里；本项刻意**不**改动 root `xmake.lua` 的构建图（避免把新目标塞进产品默认构建），准入门禁是 msbuild 套件。 |
| **P4** Commit & PR | 适用 | Conventional Commits：`fix(WeaselServer): ...`（type ∈ 白名单，scope `WeaselServer` ∈ 白名单）；CHANGELOG 已更新（见 P8 三问第 3 条）；提交前 `git status -sb` + `git diff --stat` 复核，挡住 `weasel.props` / `env.bat` / `*.obj` / `librime` 误提交。 |
| **P5** UX / 用户体验 | 适用 | 命令行选项从"完全无效"变为"按设计生效"，属**用户可感知**的行为变化 ⇒ 必须有 CHANGELOG 条目（已加）。不改任何默认键位 / 方案行为 / YAML。 |
| **P6** Backwards Compatibility | 适用 | **不触碰**用户数据路径（`%AppData%\Rime` / `%AppData%\Fluxing`），不改 loader，不改注册表 ⇒ 不涉及 P6 waiver 的任何条款（见 P8 三问第 1 条）。 |
| **P7** Subprocess Lifecycle | 适用 | **不新增长驻进程**；`WeaselServer` 仍是唯一 supervisor；`/quit` 修复后走的是**既有的** `weasel::Client::ShutdownServer()`（那段代码一直是对的，只是永远进不去），本项只是让控制流能走到它。 |
| **P8** Brand Fork: Fluxing | 适用 | 见下方"P8 三问"。 |

### P8 三问（Required plan.md disclosures）

1. **是否触及旧用户数据路径？是否援引 P6 waiver？**
   **否，且不援引任何 P6 waiver 条款。** 本改动只涉及 `WeaselServer.exe` 启动时的命令行字符串解析，
   不读写 `%AppData%\Rime` 或 `%AppData%\Fluxing` 下任何路径，不改注册表、不改互斥体名。
   P6 waiver 的三条（`HKCU\Software\Rime\*` 归一、`WeaselUserDataPath()` 默认值与一次性迁移、
   单互斥体改名 + 旧互斥体过渡探测）**均未被触及**。

2. **是否引入新的 IPC 消息类型？**
   **否。** `/quit` 走既有的 `weasel::Client::ShutdownServer()`，`/ascii` / `/nascii` 走既有的
   `TrayCommand(ID_WEASELTRAY_ENABLE_ASCII / ID_WEASELTRAY_DISABLE_ASCII)`。
   **IPC 线协议零变更** ⇒ 不触发 P2"双端更新"要求，也不触发 CLAUDE.md 的
   "server / TSF / deployer 三端 + ADR + `test/TestWeaselIPC/*` 一次性提交"要求。

3. **是否需要 CHANGELOG 条目？**
   **是。** 用户可感知的行为变化（7 个命令行选项此前完全无效，`/quit` 反而会重启服务，
   现在按设计生效）。已在 `CHANGELOG.md` 的 `[Unreleased]` 段登记条目。

### 决策分流（CLAUDE.md §3）

- **性质**：bugfix ⇒ 归档到 `.specify/memory/lessons-learned.md` 的 `L160` 节。
- **不进 ADR**：无架构决策（不改 PPL、不改 IPC 协议、不换构建方式）。

---

## 2. 实施步骤

### 2.1 共享头（生产 + 测试同源，R7）

新建 `WeaselServer/ServerCmdLine.h`（header-only、纯函数、不依赖 ATL/Windows API）：

- `enum class Option { None, UserDir, WeaselDir, Ascii, Nascii, Quit, Update }`
- `detail::IsSpace` / `detail::Normalize`（去首尾空白、剥成对双引号）
- `FirstToken`（取首个记号）
- `Parse`（记号 → `Option`）

**为什么是共享头而不是测试里重写一份**：测试若自己实现一份解析逻辑，生产与测试会各自漂移
（spec 092 刚清掉一批"测试与生产不同源"的假绿探针）。R7 要求共用。

### 2.2 生产接线

`WeaselServer/WeaselServer.cpp`：

- 增 `#include "ServerCmdLine.h"`；
- 用 `const fluxing::servercmd::Option cmdOption = fluxing::servercmd::Parse(lpstrCmdLine);`
  取代 4 处内联 `wcscmp` 判定（`/userdir`、`/weaseldir`、`/ascii`|`/nascii`、`/q`|`/quit`、`/update`）；
- **保持控制流与语义不变**（判定顺序、`/update` 的位置、`/ascii` 与 `/nascii` 的开关方向）。

### 2.3 专用测试工程

新建 `test/TestServerCmdLine/`（`vcxproj` + `cpp` + `stdafx.h` + `stdafx.cpp` + `targetver.h` + `xmake.lua`），
模板镜像 `test/TestOrphanRecovery`（同款 L31 `OutDir` 修正、同款 `/utf-8` + `stdcpp17` 工具链）。

6 个用例：

| # | 用例 | 作用 |
|---|---|---|
| T1 | 带前导空格时 7 个选项全部可达 | **缺陷回归** |
| T2 | `/q` 与 `/quit` 等价；大小写保持精确匹配 | 语义不漂移 |
| T3 | 空 / 空白 / 未知选项 → `None` | 负例 |
| T4 | 尾随空白、多余参数、Tab → 仍取首个记号 | 记号化 |
| T5 | **缺陷见证**：旧整串比较对原始 `lpCmdLine` 恒为假 | 锁死根因 |
| T6 | **调用点守卫**：`WeaselServer.cpp` 必须调用 `servercmd::Parse`，且不得残留 `wcscmp(L"/` | 覆盖单元测试够不到的生产接线 |

> T6 存在的必要性：T1–T5 测的是共享头，覆盖不到 `WeaselServer.cpp` 的调用点。
> 若有人把调用点改回 `wcscmp(L"/quit", lpstrCmdLine)`，T1–T5 依然全绿 —— T6 就是堵这个洞的。

### 2.4 登记

- `scripts/test-infra/run-test-suite.bat`：构建循环 + 运行循环各加一项（该脚本直接构建
  `%%P\%%~nP.vcxproj`，**不走 `weasel.sln`**，所以新工程不依赖 sln 也能进套件）。
- `weasel.sln`：工程条目 + 4 条 `ProjectConfigurationPlatforms` 映射。
- `.gitignore`：`/test/TestServerCmdLine/*.obj`。

---

## 3. TDD 执行序列（已实际跑通，见 tasks.md 证据表）

| 阶段 | 动作 | 观测结果 |
|---|---|---|
| RED-1 | 只写测试（共享头尚不存在） | `error C1083: 无法打开包括文件: "../../WeaselServer/ServerCmdLine.h"` —— 失败原因正确（功能缺失，非笔误） |
| RED-2 | 落**朴素基线**（整串 `wcscmp`，复刻修复前行为） | **16 条行为性断言失败**，精确命中缺陷行；T3（`None` 场景）保持绿 ⇒ 证明测试有判别性、不是恒绿探针 |
| GREEN | 换成归一化实现 + 改生产调用点 | 6/6 用例全绿，退出码 0 |

## 4. Quality Gates（提交前）

1. ✅ `clang-format` 应用于改动文件（不全仓）
2. ⏳ `build.bat weasel` 构建成功（生产改动可编译）
3. ⏳ `scripts/run-tests.bat` 全量套件退出码 0（含新增 `TestServerCmdLine`）
4. ✅ `CHANGELOG.md` 已更新（`[Unreleased]` 段）
5. ⏳ Conventional Commits + 白名单 scope + 提交前 `git status -sb` / `git diff --stat` 复核

## 5. 回滚

本项改动自包含：`WeaselServer/ServerCmdLine.h` + `WeaselServer.cpp` 的 1 处判定改造 +
独立测试工程。回滚 = `git revert` 对应提交即可，无数据迁移、无协议变更、无用户状态残留。
