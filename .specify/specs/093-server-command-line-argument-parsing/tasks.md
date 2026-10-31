# tasks 093 — WeaselServer 命令行参数解析加固

> 配套 `spec.md`（缺陷与 AC）、`plan.md`（Constitution Check + P8 三问）。

## 任务分解

- [x] **T001** 建共享头 `WeaselServer/ServerCmdLine.h`（header-only 纯函数：`Option` 枚举 / `Normalize` / `FirstToken` / `Parse`）
- [x] **T002** 建测试工程 `test/TestServerCmdLine/`：`TestServerCmdLine.cpp` + `.vcxproj` + `stdafx.h` + `stdafx.cpp` + `targetver.h` + `xmake.lua`（模板镜像 `TestOrphanRecovery`，含 `/utf-8` + `stdcpp17`）
- [x] **T003** 生产接线：`WeaselServer/WeaselServer.cpp` 改用 `fluxing::servercmd::Parse()`，保持控制流与语义不变
- [x] **T004** 登记：`scripts/test-infra/run-test-suite.bat`（构建 + 运行两处循环）、`weasel.sln`（工程条目 + 4 条配置映射）、`.gitignore`（`*.obj` 规则）
- [x] **T005** RED-1 证据：共享头不存在时构建失败，且失败原因正确
- [x] **T006** RED-2 证据：朴素基线导致**行为性**断言失败（证明测试有判别性）
- [x] **T007** GREEN 证据：6/6 用例全绿，退出码 0
- [x] **T008** 产品构建：`build.bat weasel` 成功（验证生产改动可编译）
- [x] **T009** 全量套件：`scripts/run-tests.bat` 退出码 0
- [x] **T010** `CHANGELOG.md` `[Unreleased]` 增 spec 093 条目（字节级编辑，段序/行尾/EOF 不得漂移）
- [x] **T011** `.specify/memory/lessons-learned.md` 增 `L160` 节（bugfix 分流）
- [x] **T012** 提交 + 推送（allowlist 门禁 + `git status -sb` / `git diff --stat` 复核）

## 执行结果（2026-10-31）

| 任务 | 结果 | 证据 |
|---|---|---|
| T001 | ✅ | `WeaselServer/ServerCmdLine.h`（4.5 KB）；`Parse` 归一化：去首尾空白、剥成对双引号、取首个记号 |
| T002 | ✅ | `test/TestServerCmdLine/` 6 文件；`ProjectGuid={93F1B7C2-6E48-4D05-A1B3-8C27D4E95A16}` |
| T003 | ✅ | `WeaselServer.cpp` diff：+1 include、7 处整串 `wcscmp` → `cmdOption` 判定；控制流/语义未变 |
| T004 | ✅ | `.bat` +2 处循环项；`.sln` +2（工程块）+4（配置映射）；`.gitignore` +1 |
| T005 | ✅ | `tmp/093-red.log`：`error C1083: 无法打开包括文件: "../../WeaselServer/ServerCmdLine.h"` |
| T006 | ✅ | `tmp/093-red2-run.log`：**16 FAILURE(S)**，命中 T1 全部 7 个选项 + T2 + T4×4 + T5×2 + T6×2；T3 保持绿 |
| T007 | ✅ | `tmp/093-green-run.log`：`=== ALL TESTS PASSED ===`，`RUN_EXIT=0` |
| T008 | ✅ | `tmp/093-product-build.log`：`weasel.sln` 构建 **0 个错误**（1 个既有 C4099 警告）；日志含 `TestServerCmdLine.vcxproj -> ...\Release\TestServerCmdLine.exe` |
| T009 | ✅ | `tmp/093-suite.log`：`=== ALL TESTS PASSED ===` + `SUITE_EXIT=0`；套件内含 `=== Running TestServerCmdLine.exe ===` 与 `Result: 12 passed, 0 failed` |
| T010 | ✅ | `CHANGELOG.md` `[Unreleased]` 新增 `### fix(fluxing): /quit 等 7 个命令行选项此前全部静默失效（spec 093）`；字节级校验 BOM 无 / 全 CRLF / EOF 不变（452,458 → 454,582 B） |
| T011 | ✅ | `lessons-learned.md` 增 `## L160`（BOM `EF BB BF` 保持、810,698 → 813,329 B、L160 计数 = 1）；本条定性为 bugfix ⇒ 走 lessons-learned 分流，不进 ADR |
| T012 | ✅ | 本次三提交组（allowlist 门禁 + `git status -sb` / `git diff --stat` 复核）；推送目标 `kizemo/Fluxing` |

## 附：T6 的必要性（判别性说明）

T1–T5 测的是共享头，**覆盖不到** `WeaselServer.cpp` 的调用点。若有人把调用点改回
`!wcscmp(L"/quit", lpstrCmdLine)`，T1–T5 依然全绿（共享头本身没坏）。T6 在运行时读
`WeaselServer/WeaselServer.cpp` 源码，断言它必须出现 `servercmd::Parse` 且不得出现任何
`wcscmp(L"/` —— 这是本工程唯一覆盖"生产接线"的守卫。
