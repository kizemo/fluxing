# Fluxing v2 · Technical Design Document / Test Strategy (TDD)

> **项目级 TDD 策略**。定义测试金字塔、各层测试覆盖目标、CI 集成方案、当前缺口。
> 与子 spec 的关系：子 spec `plan.md` 的"风险"段列了各自的单测点；本文是"v2 整体测试架构"，子 spec 是局部实施。
> 撰写人：AI 助手（基于宪法 R6「evidence before assertion」+ AGENTS.md §2.5 + ci.yml 现状 + T011 评审发现的 CI 缺口）。
> 维护周期：每次新增 / 调整测试时同步；每月评审覆盖率。

---

## 0. 项目当前测试现状（2026-07-01 体检）

| 测试层 | 现状 | 工具 |
|---|---|---|
| **静态分析 / lint** | ✅ `clang-format.sh` 已在 ci.yml 跑 | clang-format 18 |
| **构建** | ✅ `xbuild.bat` / `build.bat` 在 ci.yml 跑（msbuild + xmake 双矩阵） | msbuild 2022, xmake 2.9.4+ |
| **单元测试** | ⚠️ 3 个 test project 存在但**只有 1 个能跑** | cl.exe 手动编译 |
| **集成测试** | ❌ 0 个 | 缺 |
| **E2E 测试** | ❌ 0 个；仅 manual 验证 | 缺 |
| **覆盖率** | ❌ 未测量 | 缺 |
| **CI test job** | ❌ ci.yml 缺 test job；测试完全靠人肉跑 | 缺 |

**关键缺口**：
1. **TestDefaultHotkeys**（spec 005 配套，25/25 PASS）有 Release exe，但**没 vcxproj，不在 sln 里**。
2. **TestResponseParser / TestWeaselIPC** 有 vcxproj + 在 sln 里，但 **Release exe 不存在**（需要先 msbuild）。
3. **没有任何 test job** 出现在 `.github/workflows/ci.yml`。

---

## 1. 测试金字塔（v2 目标架构）

```
        ┌─────────────────────┐
        │  E2E (manual 验证清单) │  ← spec 005/008/009 各列 manual 步骤
        │  集成 (mock librime)  │  ← 新增：test/Integration/
        │  单元 (现有 3 套 + 5 新套) │
        └─────────────────────┘
        ↑ 速度  ↑ 隔离度   ↑ 真实度
```

**比例目标**（v2.0.0 release 前）：
- 单元测试 70%：8 个 test project 全部能跑；覆盖率 ≥ 60% 行覆盖（关键路径：key_binder / CandidateEdit / PhrasesStore / YamlRoundTrip / DarkModeBridge / Bootstrapper）
- 集成测试 20%：librime mock 框架下 4-6 个集成场景（spec 005 binding 解析、008 user_dict_update、009 phrases round-trip、006 dark-mode 广播）
- E2E / manual 20%：spec 005-009 各列 manual 验证清单（3 平台 × 3 DPI × 暗/亮色）

---

## 2. 单元测试策略

### 2.1 现状测试套件

| Project | vcxproj | 在 sln | Release exe | 现状 | 修复 |
|---|---|---|---|---|---|
| TestDefaultHotkeys | ❌ | ❌ | ✅ | 25/25 PASS（手 cl 编译） | 加 vcxproj + 加 sln |
| TestResponseParser | ✅ | ✅ | ❌ | 未运行 | ci.yml 加 test job 自动 build |
| TestWeaselIPC | ✅ | ✅ | ❌ | 未运行 | 同上 |

### 2.2 v2 新增测试套件

| 配套子 spec | Test project | 覆盖内容 | 优先级 |
|---|---|---|---|
| 005 | TestDefaultHotkeys (扩展) | Shift+space 切中英 + L18 回归 + 25 项断言 | P1（已 25/25 PASS） |
| 006 | TestDarkModeBridge | mock WM_SETTINGCHANGE 验证订阅者通知 + 200ms 渐变 | P1 |
| 006 | TestFluxingIPCClient | mock WeaselServer IPC 协议 | P1 |
| 007 | TestYamlRoundTrip | 注释保留 + key 顺序保留 round-trip | P1 |
| 007 | TestConflictChecker | 与系统 / IDE 已知快捷键冲突检测 | P2 |
| 008 | TestCandidateEdit | mock 候选 + mock user_dict_update + 防抖 100ms | P1 |
| 009 | TestPhrasesStore | phrases.json 读写 + 并发安全（10 线程） | P1 |
| 009 | TestPinyinHint | text → 拼音首字母（如"邮箱"→"yx"） | P1 |
| 010 | TestSyncClient | mock Vercel API + LWW 冲突解决 | P2（v2.1+ 实施） |
| 011 | TestBootstrapper | mock NSIS silent 模式 + 路径合法化 | P3（v2.2+ 实施） |

**总目标**：v2.0.0 release 前至少 8 个 test project 全部纳入 sln + 加 CI test job。

### 2.3 单元测试约定

- **每个 test project 必须**有 `.vcxproj` + 加入 `weasel.sln`。
- **每个 test project 编译输出**固定到 `Release` 目录（与 x86 矩阵一致，spec 005 L10 §3）。
- **每个 test project 必须有** `main()` 入口，返回 0 = pass / 非 0 = fail；CI 检查 exit code。
- **断言**用标准 C++ `assert()` 或自己写的 `EXPECT_*` / `ASSERT_*` 宏（不引入 GoogleTest 等大依赖）。

---

## 3. 集成测试策略（v2 新增）

### 3.1 测试目录

```
test/
├── Integration/
│   ├── TestBindingResolution.cpp  # 解析 spec 005 yaml 实际 binding 是否被 librime 接受（L18 关键）
│   ├── TestUserDictUpdate.cpp     # spec 008 mock RimeUserDict 验证 user_dict_update(-1) 真的删词
│   ├── TestPhrasesRoundTrip.cpp   # spec 009 phrases.json write → read 一致性
│   ├── TestDarkModeBroadcast.cpp  # spec 004 §9 WM_SETTINGCHANGE → 所有订阅者回调
│   ├── TestYamlRoundTripE2E.cpp   # spec 007 实际 default.yaml → YamlRoundTrip → 写回 → diff 无变更
│   └── TestBootstrapperStdio.cpp  # spec 011 NSIS silent pipe JSON 协议 round-trip
```

### 3.2 集成测试原则

- **mock librime**：`librime/src/rime/...` 子模块用本地实现的 stub（C++ 头文件 + 实现 stub 文件），避免依赖实际 librime 二进制。
- **集成测试编译目标**：`test/Integration/Release/`（与单元测试目录对齐）。
- **不依赖 RIME 引擎 dll**：集成测试是用户态，不需要 `rime.dll` 加载。

### 3.3 集成测试在 CI 中的位置

- ci.yml 加 `test-integration` job：先 build 单元测试 exe，再跑集成测试 exe。
- 集成测试失败 = ci.yml red = merge block。

---

## 4. E2E / Manual 验证清单（v2 整体 hard gate）

每份子 spec 的 `design.md §2.3 / §2.5` 列了 manual 验证步骤。v2 整合时必须跑：

| 场景 | 配套子 spec | 步骤数 | 验证人 | 频率 |
|---|---|---|---|---|
| 翻页 + Shift 选候选 + Shift+space 切中英 + L18 回归 | 005 | 4 步 | 用户 | 每次 release |
| 托盘面板启动 + Alt+, + 失焦 1s 关闭 | 006 | 3 步 | 用户 | 每次 release |
| yaml UI 改快捷键立即生效 | 007 | 1 步 | 用户 | 每次 release |
| 右键候选删除不弹窗 | 008 | 1 步 | 用户 | 每次 release |
| 常用短语 Alt+K 列表 + Enter 上屏 | 009 | 2 步 | 用户 | 每次 release |
| 暗色主题 200ms 渐变 | 004 §9 | 1 步 | 用户 | 每次 release |
| 三平台 × 三 DPI 一致 | 全部 | 9 步 | 用户 | 每次 release |

**Manual 验证 = v2 release 的 hard gate**。任何一项失败 = release block。

---

## 5. AGENTS.md §2.5 静默安装 smoke test（已落地）

来源：AGENTS.md §2.5；本节确认其与本 TDD 的集成。

- **触发**：每次 `xbuild.bat installer` 后必跑。
- **内容**：13 项 invariants（exit code / 安装目录布局 / HKLM InstallDir / HKCU RimeUserDir / rime.dll 大小 / prebuilt dicts / PE arch 等）。
- **CI 集成**：可加 `ci.yml` 步骤自动跑（在 windows-2022 镜像装 `fluxing-X.Y.Z-installer.exe` 到 `C:\TEMP\smoke` + 跑 PS 脚本验证）。
- **L13 / L14 / L17 防御**：smoke test 正是 0.17.5-0.18.2 path-force 漏 4 个版本的"事后诸葛亮"。**永远跑**。

---

## 6. CI 集成方案（修复 R-008）

### 6.1 当前 ci.yml 的缺口

`lint` job + `build` job（msbuild + xmake 双矩阵），但**完全没 test job**。结果：
- 单测 PASS 状态 = 不可观测
- 集成测试 = 不存在
- 任何 commit 都能 merge 不会红

### 6.2 推荐 ci.yml 新增 test job（在 build 之后）

```yaml
  test:
    needs: build
    runs-on: windows-2022
    strategy:
      matrix:
        variant: [msbuild, xmake]
    steps:
      - name: Build test executables
        shell: pwsh
        run: |
          $ErrorActionPreference = 'Stop'
          msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32
          # 单独编译 TestDefaultHotkeys（无 vcxproj 的）
          cd test/TestDefaultHotkeys
          cl /EHsc /std:c++17 /utf-8 /I ../../include TestDefaultHotkeys.cpp /Fe:Release/TestDefaultHotkeys.exe
          cd ../..

      - name: Run unit tests
        shell: pwsh
        run: |
          $ErrorActionPreference = 'Stop'
          test\TestDefaultHotkeys\Release\TestDefaultHotkeys.exe output\data\default.yaml
          if ($LASTEXITCODE -ne 0) { throw "TestDefaultHotkeys failed" }
          test\TestResponseParser\Release\TestResponseParser.exe
          if ($LASTEXITCODE -ne 0) { throw "TestResponseParser failed" }
          test\TestWeaselIPC\Release\TestWeaselIPC.exe
          if ($LASTEXITCODE -ne 0) { throw "TestWeaselIPC failed" }

      - name: Run integration tests
        shell: pwsh
        run: |
          $ErrorActionPreference = 'Stop'
          # (v2.1+ 实施) test/Integration/Release/*.exe 各跑一次
          # 当前 v0.18.5 阶段集成测试未落地，本步空操作
          Write-Host "Integration tests: not yet implemented (v2.1+ milestone)"
```

### 6.3 优先级

- **P1 (v0.18.6)**：把 TestDefaultHotkeys vcxproj 加上 + 加 sln + 加 test job（最低限度 CI 跑单测）
- **P1 (v2.0.0)**：6 个新 test project 全部 ship + 集成测试 4 个场景
- **P2 (v2.1+)**：覆盖率门槛 60% 行覆盖
- **P3 (v2.2+)**：Fuzz 测试（librime 边界条件）+ property-based testing

---

## 7. 覆盖率策略（v2.1+ 目标）

### 7.1 测量工具

- **Windows**：OpenCppCoverage（开源，cmake-friendly） + GitHub Action `esbenbritt/OOpenCppCoverage`。
- **生成报告**：`coverage.xml`（Cobertura 格式）+ 覆盖率徽章。

### 7.2 目标

- **v2.1+**：关键模块（`output/data/default.yaml` 相关 / `RimeWithWeasel/` / `WeaselUI/WeaselPanel.cpp`）≥ 60% 行覆盖。
- **v2.2+**：≥ 80% 行覆盖 + ≥ 70% 分支覆盖。

### 7.3 关键模块优先级

1. **`output/data/default.yaml` 解析路径**（spec 005）— L18 测试 gap
2. **`RimeWithWeasel/CandidateEdit.cpp`**（spec 008）— 删除 / 屏蔽逻辑
3. **`RimeWithWeasel/DarkModeBridge.cpp`**（spec 006）— 跨切关键路径
4. **`FluxingConfigEditor/YamlRoundTrip.cpp`**（spec 007）— 数据完整性
5. **`FluxingPersonalShortcuts/PhrasesStore.cpp`**（spec 009）— 并发读写

---

## 8. 已知测试 gap（与 lessons-learned 联动）

| Gap | 关联 lesson | 状态 |
|---|---|---|
| `key_binder` binding 是否真被 librime 接受（vs 仅字符串包含） | L16 / L18 | **CLOSED v0.18.10.0** - TestBindingResolution 6/6 PASS (spec 017 L24 link-probe + spec 018 L25 mock pattern) |
| `WM_SETTINGCHANGE` → 候选面板实际切色 | L17 关联 | **CLOSED v0.18.22.0 + v0.18.23.0** - TestPanelDarkModeSubscribe 3/3 + TestDarkModeBroadcast 14/14 PASS (spec 032 + spec 034) |
| `user_dict_update(-1)` 真删词条（vs 仅 API 调用成功） | L10 关联 | **CLOSED v0.18.17.0** - TestUserDictUpdate 4/4 PASS (spec 028) |
| `rime_deployer --debug` deploy 实际通过（vs 仅文件存在） | L10 关联 | **SPEC-NEEDED** - rime_deployer.exe does not exist in librime dist; WeaselServer uses rime_api->deploy directly. spec 004 SC-005 wording needs update; deferred to a future spec |

**L19 待加 lessons-learned 章节**：L18 修复不完整（用户 2026-07-01 实测发现 `shift+Enter` 仍切中英）— 根因诊断 + 完整修复方案（见 PRD.md §7 R-007 升级）。

---

## 9. 文档组织（与 PRD.md 对齐）

- `PRD.md`：产品视角全局视图（已写）
- `TDD.md`：技术视角全局视图（本文件）
- `constitution.md`：5 原则 + 9 硬规则 + P1-P8
- `lessons-learned.md`：L01-L18（待加 L19）踩坑库
- `specs/NNN-*/{spec,plan,tasks,design}.md`：局部 3 件套 + 详细设计
- `AGENTS.md §2.5`：smoke test 配方（与 TDD.md §5 联动）
- `AGENTS.md §5`：pre-commit 5 步检查清单（与 TDD.md §6 CI 集成）

---

## 10. 状态快照（2026-07-01）

- **TDD v1.0**（本文）首次撰写。
- 当前测试套件：3 个（TestDefaultHotkeys 25/25 PASS / TestResponseParser 未跑 / TestWeaselIPC 未跑）。
- v2 新增 6 个 test project 待 ship。
- **P1 修复**：TestDefaultHotkeys 加 vcxproj + sln + ci.yml test job（v0.18.6 release 前完成）。
- **R-008**（CI 不跑测试）风险待 P1 修复后 close。

### 2026-07-03: v0.18.7.0 已 ship (CI infra, R-008 close)

- ci.yml test job added (line 194); runs `scripts\run-tests.bat` (wrapper) -> `scripts\test-infra\run-test-suite.bat` (build + run loop).
- TestDefaultHotkeys vcxproj added; 4 test projects run in CI (per spec 015).
- **R-008** (CI 不执行单测) CLOSED: mitigation actually shipped in v0.18.7.0; verified 2026-07-04 (13/13 test projects PASS).

### 2026-07-03: v0.18.19.0 已 ship (5-version gap close)

- 5 versions (0.18.8-0.18.18) shipped as CHANGELOG-only tags (NO installer in git). 0.18.19.0 is the first to re-include the installer binary.
- **L40 lessons-learned**: AGENTS.md §2.5 smoke test + release/ directory hygiene must be enforced for every tag; release branches must not skip installer build.
- **L41 lessons-learned**: NSIS install.nsi byte-level discipline (UTF-8 BOM + LF-only newline in 0x3F test).
- release/fluxing-0.18.19.0-installer.exe (~40.6 MB).

### 2026-07-04: v0.18.20.0 已 ship (spec 033 ship + L42 post-mortem)

- spec 033 FluxingDarkModeBridge (F11 dark-mode cross-cut): TestDarkModeBridge 18/18 PASS, but L42 later discovered the production code was NOT linked into weasel.dll (false-positive test pass).
- **L42 lessons-learned**: Verify linked .dll/.exe content, NOT just .pdb / .lib symbols. Use byte-level search for `0x001E1E1E` in `weasel.dll` to confirm dark-mode palette bytes are in the actual binary.
- TestDarkModeBridge 18/18 PASS (re-classified as "spec implementation passes local link probe but does not survive end-to-end build").
- RimeWithWeasel/FluxingDarkModeBridge.{h,cpp} committed but did not survive `xbuild.bat installer`.

### 2026-07-04: v0.18.20.1 已 ship (spec 033 hotfix revert)

- spec 033 reverted (commit 46ee0b7). Build pipeline blocks the bridge link (L42 false-positive root cause).
- 0.18.20.1 binary is identical to 0.18.20.0 (revert was a docs + tests revert, not production code revert).

### 2026-07-04: v0.18.21.0 已 ship (spec 008 finalization + 4 test project ship)

- spec 008 finalization: TestUserDictUpdate 4/4 PASS (post-0.18.17.0 test ship + 0.18.21.0 spec-final commit).
- 4 new test projects ship increment: TestCandidateRButtonDown (spec 019) / TestCandidateIgnoreFilter (spec 020) / TestPanelDarkModeSubscribe (spec 032).
- 7/7 test projects PASS.
- release/fluxing-0.18.21.0-installer.exe (~42 MB).

### 2026-07-04: v0.18.22.0 已 ship (spec 033 retry success)

- spec 033 retry success: FluxingDarkModeBridge linked in weasel.dll (L42 byte-verify: 0x001E1E1E in weasel.dll = 1).
- **L43 lessons-learned**: /LTCG:OFF per-target cure (not global /LTCG removal). WeaselTSF target has its own add_shflags that re-enables LTCG; per-target /LTCG:OFF is the cure.
- 12/12 test projects PASS (107 assertions).
- release/fluxing-0.18.22.0-installer.exe (~40.6 MB).

### 2026-07-04: v0.18.23.0 已 ship (spec 034: F11 cross-cut integration test)

- spec 034 TestDarkModeBroadcast: new behavior-level test linking actual FluxingDarkModeBridge.cpp via L24 link-probe pattern. 14 behavior-level assertions (T1-T6, all PASS). WndProc filter is test-local L25 mock (3 lines wcscmp) to avoid WeaselPanel.cpp WTL/ATL/Gdiplus dependency cost.
- spec 022 placeholder now unblocked (was BLOCKED on spec 004 production code; block lifted by spec 033).
- **L44**: PowerShell Encoding.UTF8.GetString + IndexOf byte-vs-char miscalculation trap. Cure: byte-level pattern matching (L42 verification uses this).
- **Bug fix** (commit 33efa00): weasel.sln TestPanelDarkModeSubscribe missing EndProject (spec 032 leftover). Inline-test dead code in TestDarkModeBroadcast.cpp T3d else-branch removed.
- 13/13 test projects PASS, 14 new TestDarkModeBroadcast assertions PASS. release/fluxing-0.18.23.0-installer.exe (42,655,987 bytes).

---

**Test suite growth (post-v0.18.23.0):** 3 -> 13 test projects. The 10 new ones shipped incrementally across 0.18.7-0.18.23: TestShiftSelectBinding (0.18.8), TestBindingResolution (0.18.10), TestYamlRoundTripE2E (0.18.14), TestUserDictUpdate (0.18.17), TestCandidateRButtonDown (0.18.21), TestCandidateIgnoreFilter (0.18.21), TestPanelDarkModeSubscribe (0.18.21), TestTrayRestoreIgnored (0.18.22), TestDarkModeBridge (0.18.22), TestDarkModeBroadcast (0.18.23).

**Integration test coverage (TDD sec 3.1):**
- TestBindingResolution: SHIPPED (spec 017, 6/6 PASS).
- TestUserDictUpdate: SHIPPED (spec 020, 4/4 PASS).
- TestPhrasesRoundTrip: BLOCKED (spec 021 on spec 009 PhrasesStore - production code not yet written).
- TestDarkModeBroadcast: SHIPPED (spec 034, 14/14 PASS).
- TestYamlRoundTripE2E: SHIPPED (spec 023->024, multiple assertions PASS).
- TestBootstrapperStdio: BLOCKED (spec 025 on spec 011 Fluxing Bootstrapper - production code not yet written).

**R-008 (CI does not run tests):** CLOSED in v0.18.7.0; verified 2026-07-04 (13/13 test projects PASS in `scripts\test-infra\run-test-suite.bat`).

**Lessons-learned count:** L01-L44 (44 lessons) by 2026-07-04.

**Spec count:** 35 spec directories (000-035, with 013, 025-bookkeeping, 025-bootstrapper naming variants per L23 history).

**Code coverage target (sec 7):** deferred to v2.1+ (no measurement tool wired into ci.yml yet).

---

## 8. v0.18.25.0 测试基础设施更新 (2026-07-05)

### 8.1 L46 verification recipe 落地 (3 paths all PASS)

- **Path 1 (xmake)**: xbuild.bat weasel installer → exit 0, installer 42,850,220 bytes.
- **Path 2 (msbuild)**: msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32 /m:1 → 0 errors / 2 pre-existing warnings (C4267 + C4101).
- **Path 3 (test suite)**: scripts\test-infra\run-test-suite.bat → 13/13 test exe PASS, 115 assertions / 0 FAIL, "=== ALL TESTS PASSED ===".

L46 三路径全部成功是 release 0.18.25.0 的 hard gate. 任何一个路径 fail → 不能 tag.

### 8.2 L31 vcxproj OutDir 修复

3 个 test vcxproj (TestBindingResolution, TestResponseParser, TestYamlRoundTripE2E) 的 <IntDir>SolutionDir-msbuild-... 缺 \ 反斜杠, 触发 MSB3491 
imemsbuild 路径 (L31 root cause B). 修复: 加 \ → <IntDir>SolutionDir\-msbuild-....

0.18.25.0 ship 时已修. 验证: 
un-test-suite.bat exit=0 (之前是 exit=1 因为 build 步骤 fail).

### 8.3 MSB6001 workaround (L47 precursor)

PowerShell 5.1 启动 cmd 时把 PATH 转为 Path (小写), 而 VsDevCmd.bat 触发 .NET Hashtable "已添加项: 字典中的关键字 PATH 所添加的关键字 Path" 异常 (MSB6001). workaround: 用 cvars32.bat 不用 VsDevCmd.bat (cvars32.bat 是纯 cmd 脚本, 不触发此 .NET 冲突).

0.18.25.0 ship 时已统一用 cvars32.bat 路径. 0.18.25.0 之前几个 release 误以为 "xbuild 路径全过就 ship" 但 msbuild 路径被 VsDevCmd 阻塞, 是 L46 发现的盲点.

### 8.4 Test suite total (post-0.18.25.0)

- 13 test projects: TestDefaultHotkeys, TestShiftSelectBinding, TestBindingResolution, TestResponseParser, TestWeaselIPC, TestYamlRoundTripE2E, TestUserDictUpdate, TestCandidateRButtonDown, TestCandidateIgnoreFilter, TestPanelDarkModeSubscribe, TestTrayRestoreIgnored, TestDarkModeBridge, TestDarkModeBroadcast.
- 115+ assertions PASS / 0 FAIL across all 13 projects.
- TestDefaultHotkeys 35/35 (was 25/25 pre-0.18.5; spec 014/021 added Shift_L/R recovery + L16/L19 regressions).
- TestQuickPanelDialog 10/10 (spec 036 v0 ship).
- TestDarkModeBridge 18/18 (spec 033).
- TestDarkModeBroadcast 14/14 (spec 034 integration test).

---

## 9. spec 037 测试策略 (2026-07-05)

### 9.1 测试目标 (摘自 spec 037 plan.md sec 2.7)

- **TestFluxingButton** (4 assertions):
  - T1: Create(hwnd, rect, label, style) 返回非空 unique_ptr
  - T2: 模拟 WM_LBUTTONUP 触发 OnClick callback
  - T3: SetLabel 更新 HWND 文本 (GetWindowText 验证)
  - T4: 主题变化时 InvalidateRect 被调用 (mock WndProc 计数)
- **TestFluxingToggle** (4 assertions):
  - T1: Create 初始状态 IsOn() == initial
  - T2: SetOn(true) 改变状态 + 触发 OnChanged
  - T3: WM_LBUTTONUP 翻转状态
  - T4: 200ms 滑动动画完成后状态稳定
- **TestFluxingPanel** (3 assertions):
  - T1: Create 成功
  - T2: Card style 圆角半径 8px (内部状态)
  - T3: Plain style 无圆角
- **TestFluxingTheme** (3+ assertions):
  - T1: Instance() 返回同一引用
  - T2: Subscribe/Unsubscribe 正确管理订阅者
  - T3: CurrentPalette() 调 FluxingDarkModeBridge
  - T4: dark mode 切换触发订阅者 callback

### 9.2 字节级验证 (L42 + L44)

- **L42 byte-verify**:  x001E1E1E 仍在 weasel.dll (spec 033 dark-mode palette bytes, 新增 7 个 .cpp 不能 dead-strip 这 4 字节).
- **L44 byte-level search**: PowerShell Encoding.UTF8.GetString + IndexOf 会有 byte-vs-char miscalculation, 用 ReadAllBytes + 数组 IndexOf 替代.

### 9.3 测试金字塔 (post-spec-037)

`
        ┌─────────────────────┐
        │  E2E (manual 验证清单) │  ← spec 037/038 各列 manual 步骤 (DPI 100/150/200)
        │  集成 (mock librime)  │  ← spec 037 TestFluxingTheme (mock FluxingDarkModeBridge)
        │  单元 (现有 14 套 + 4 新套) │  ← spec 037 TestFluxingButton/Toggle/Panel + 现有 13 test
        └─────────────────────┘
`

### 9.4 v0.18.26.0 ship gate (2026-07-05, 实际 ship 完成)

- **15 test projects all PASS** (14 现有 + 1 new TestFluxingComponents) - 实测 ALL TESTS PASSED
- **199+35 = 234+ assertions** (TestFluxingComponents Button 8 + Toggle 12 + Panel 6 + Theme 9 = 35 new)
- L42 byte-verify: 0x001E1E1E 仍在 weasel.dll
- L14 arch-verify: 6 binary 全部 arch 一致 (WeaselServer / WeaselDeployer / WeaselSetup / weasel.dll / rime.dll = 0x14C, weaselx64.dll = 0x8664)
- L47 lessons-learned 正式追加 (6 byte/syntax bugs from spec 037 reactive fix cascade)
- spec 038 bootstrap: 26 tasks / 6 phase, QuickPanelDialog 重构使用 spec 037 4 个控件

### 9.5 v0.18.26.0 TestFluxingComponents 详情

| 测试 | assertions | 状态 |
|---|---|---|
| TestFluxingButton | 8/8 | PASS (Create + Hwnd + Label + Style + WM_LBUTTONUP + SetLabel x 2 + theme callback) |
| TestFluxingToggle | 12/12 | PASS (Create false/true x 2 + IsOn init x 2 + SetOn true + OnChanged + idempotence + WM_LBUTTONUP flip + 200ms anim + stable) |
| TestFluxingPanel | 6/6 | PASS (Create Card + Hwnd + GetStyle + kCardRadius==8 + Create Plain + GetStyle) |
| TestFluxingTheme | 9/9 | PASS (Instance 单例 + Subscribe A + B + unsubscribe A + 3-subscriber FIFO x 2) |
| **TestFluxingComponents 总计** | **35/35 + 4/4** | **ALL PASS** |

- 15 test projects all PASS (14 现有 + 1 new TestFluxingComponents)
- 199+35 = 234+ assertions (TestFluxingComponents Button 8 + Toggle 12 + Panel 6 + Theme 9 = 35 new)
- L42 byte-verify  x001E1E1E 仍在 weasel.dll
- L14 arch-verify 6 binary 全部 arch 一致
- AGENTS.md sec 2.5 smoke test 13+1/13+1 PASS
- "=== ALL TESTS PASSED ===" 出现

---

## 10. code coverage (deferred)

如 2026-07-04 现状, code coverage tool 未 wired into ci.yml. v2.0.0 推迟, v2.1+ 引入 OpenCppCoverage 或类似工具.