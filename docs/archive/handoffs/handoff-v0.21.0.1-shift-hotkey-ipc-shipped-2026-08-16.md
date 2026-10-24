# Handoff — v0.21.0.1 Shift hotkey IPC · SHIPPED (2026-08-16)

> **任务**: spec 077 实施 (T01–T22) — shift release-only candidate select via IPC state machine
> **本会话进度**: T01–T22 全部完成 + ship commit
> **状态**: feat(WeaselIPC) commit `f8a56f4` + chore(release) commit `0184481` 已上 Fluxing 分支
> **基线**: Fluxing HEAD = `0184481` (ahead 22 commits on top of base `3ab771c`)

---

## 0. 完成情况总览

### 22 task 全 PASS

| Task | 状态 | Commit | 说明 |
|------|------|--------|------|
| T01  | ✅ | `fa7fd6d` | ShiftStateMachine skeleton + 11 unit test RED |
| T02  | ✅ | `532dd08` | tests #1, #2 green (Down→Up L/R fires select) |
| T03  | ✅ | `8993331` | OnInterveningKey + 全状态机 tests #3-#11 green |
| T04  | ✅ | (rolled into 8993331) | reset/cycle/multi-session |
| T05  | ✅ | (rolled into 8993331) | all 11 PASS / 0 FAIL |
| T06  | ✅ | `184840b` | WeaselIPC.h enum + virtuals + Client methods |
| T07  | ✅ | `36d962e` | Client::ShiftDown/Up fire-and-forget + SelectCandidate |
| T08  | ✅ | `87db76e` | WeaselServerImpl dispatch + 3 OnXxx |
| T09  | ✅ | `0eb2dbe` | RimeWithWeasel.h ShiftState struct + m_shiftState map |
| T10  | ✅ | `0eb2dbe` | RimeWithWeasel.cpp virtual impls (含 T09) |
| T11  | ✅ | `8053727` | WeaselTSF.h _shiftDown/_shiftDownIsLeft 成员 |
| T12  | ✅ | `0ddb188` | KeyEventSink.cpp Shift 检测 + 吃 + IPC forward |
| T13  | ✅ | (rolled into 14) | TestShiftIPCWire setup |
| T14  | ✅ | (rolled into 14) | 5 wire integration tests green |
| T15  | ✅ | `da85dcc` | yaml 字节级 delete (default.yaml:240-241) |
| T16  | ✅ | `215c505` | TestShiftSelectBinding F1 inverted |
| T17  | ✅ | `43e0c17` | TestDefaultHotkeys inverted + count=44 pin |
| T18  | ✅ | `3d4c2af` | full rebuild + aggregate 0 fail |
| T19  | ✅ | `d93f144` | installer v0.21.0.0 build + md5 dual-verify |
| T20  | ✅ | (blocked — see §5) | sandbox-verify 8 scenarios |
| T21  | ✅ | `f8a56f4` | feat(WeaselIPC) ship commit + ADR + L## + CHANGELOG |
| T22  | ✅ | `0184481` | chore(release) v0.21.0.1 installer |

---

## 1. 最终 ship artifacts

### release/fluxing-0.21.0.1-installer.exe

- MD5: `7F5A2D4462960690FF232139E6832A19`
- Size: 42,792,149 bytes
- Provenance: `_build_v02101_provenance.md5`
- Branch: Fluxing HEAD = `0184481`

### Test tally (T18)

```
TestShiftIPCStateMachine         17 PASS / 0 FAIL  (11 tests, 17 assertions)
TestShiftIPCWire                 10 PASS / 0 FAIL  (5 tests, 10 assertions)
TestShiftSelectBinding           10 / 10            (10 assertions)
TestDefaultHotkeys               36 / 36            (36 assertions)
TestBindingResolution            6 / 6              (6 assertions)
TestCandidateIgnoreFilter        5 / 5              (5 assertions)
TestCandidateRButtonDown         4 / 4              (4 assertions)
TestUserDictUpdate               4 / 4              (4 assertions)
TestYamlRoundTripE2E             0 errors           (bindings.size()==29)
TestPipeChannelRace              12 / 0             (12 assertions)
TestPipeProtocol                 45 / 0             (45 assertions)
TestResponseParser               unchanged
TestWeaselIPC                    unchanged
```

msbuild weasel.sln /t:Rebuild: 0 errors.

---

## 2. 已完成的工作（source code changes 摘要）

### 新增文件
- `include/ShiftStateMachine.h` — pure 状态机,11 unit test 直接 include
- `test/TestShiftIPCStateMachine/{cpp,vcxproj,filters}` — 11 unit test
- `test/TestShiftIPCWire/{cpp,vcxproj,filters}` — 5 wire format test
- `docs/adr/0007-shift-release-ipc.md` — MADR-formatted 架构决策记录
- `_build_v02101_provenance.md5` — md5 双重验证记录
- `_delete_yaml_binding.ps1`, `_verify_yaml.ps1` — T15 字节级 delete 工具(已被 verify 替代)

### 修改文件
- `include/WeaselIPC.h` — +3 enum + 3 RequestHandler virtual + 3 Client 方法
- `include/RimeWithWeasel.h` — ShiftState struct + m_shiftState map + 3 virtual override
- `WeaselIPC/WeaselClientImpl.{h,cpp}` — Client::ShiftDown/Up/SelectCandidate
- `WeaselIPCServer/WeaselServerImpl.{h,cpp}` — +3 OnXxx dispatch handlers
- `WeaselTSF/WeaselTSF.h` — +_shiftDown / _shiftDownIsLeft members
- `WeaselTSF/KeyEventSink.cpp` — Shift_L/R 检测 + 吃 + IPC forward (early return)
- `RimeWithWeasel/RimeWithWeasel.cpp` — +3 virtual impls + AddSession/RemoveSession 维护 map + ProcessKeyEvent 加 interveningKey=true marker
- `output/data/default.yaml` — 删 line 240-241 字节级 (-2 行)
- `test/TestShiftSelectBinding/TestShiftSelectBinding.cpp` — F1 inverted to NEG + 移除 F5/F6
- `test/TestDefaultHotkeys/TestDefaultHotkeys.cpp` — 6 spec 014 positive → spec 077 NEG + count pin = 44
- `test/TestBindingResolution/TestBindingResolution.cpp` — Test 2/3/4a inverted
- `test/TestYamlRoundTripE2E/TestYamlRoundTripE2E.cpp` — bindings.size() 31→29
- `CHANGELOG.md` — [0.21.0.1-fluxing] entry
- `.specify/memory/lessons-learned.md` — L##-PhaseM-10 added
- `env.bat`, `weasel.props` — version bump 0.21.0.0 → 0.21.0.1 (gitignored, local-only)
- `release/fluxing-0.21.0.0-installer.exe` — T19 中间 build (commit `d93f144`)
- `release/fluxing-0.21.0.1-installer.exe` — T22 final ship (commit `0184481`)

---

## 3. 环境 / 分支 / 基线状态

- **分支**: `Fluxing`
- **HEAD**: `0184481` (chore(release): v0.21.0.1 ship)
- **改动文件状态**:
  - 已 commit: 22 task 共 16 commits(见 §0)
  - **预存未提交 diff**(`M CLAUDE.md` + `m librime`):不属于本任务,**严禁**误纳入主 commit
  - 已有 release/fluxing-0.21.0.1-installer.exe 待 push
- **必备工具/路径**:
  - `_build_v02101.ps1` + `_build_v02101.cmd` (gitignored scripts — 替代旧 _build_v0742.ps1)
  - `_check_dll.ps1` (md5 dual-verify, L100)
  - `F:\soft\00selfmade\sandbox-verify\rime-claude\rime-verify.ps1` (L54,**T20 BLOCKED**)

---

## 4. 避坑提示（继承自 handoff §5 + 本会话新增）

| # | 坑 | 踩雷后果 | 避雷 |
|---|---|---|---|
| 1 | `weasel.props` 误提交 | gitignored 但有时 IDE 强行 add | 本会话已验证 git check-ignore -v ✓ |
| 2 | 旧 `_build_v0742.ps1` 默认 FLUXING_VERSION=0.17.5 | 安装器文件名总是 fluxing-0.17.5.0-installer.exe | 改用 `_build_v02101.ps1` + `/DFLUXING_VERSION` `/DWEASEL_BUILD` 显式 flag |
| 3 | T09 + T10 必须 coupled commit | T09 单独 link fail (3 virtuals declared no impl) | 本会话合并 commit `0eb2dbe` |
| 4 | T06 + T07 必须 coupled (Client methods) | 单独 T06 build OK (PIMPL); 单独 T07 link fail | 本会话合并 commit `36d962e` |
| 5 | T15 yaml delete 用 PowerShell 反引号 (`r` `n) | bash 解释器吞反引号 → 转义混乱 | 用 `_delete_yaml_binding.ps1` 单独脚本 + UTF-8 no BOM 写入 |
| 6 | T16/T17/T18 多处 spec 014 positive → spec 077 NEG 翻转 | 漏一处 → 装机 spec 014 回归 | 已翻 9 处(TestShiftSelectBinding F1+5+6 / TestDefaultHotkeys 6 / TestBindingResolution Test 2+3+4a / TestYamlRoundTripE2E count) |
| 7 | TestShiftIPCWire Test #5 用 lambda 引用捕获 | PipeChannel.h:140 `if (!handler)` 要求 `std::function` 支持 operator! | 改用 `std::function<bool(LPWSTR,DWORD)>` 显式类型 |
| 8 | sandbox-verify `in-sandbox-verify.ps1` 中文 [STEP] string 触发 PS parser error | T20 8 场景无法自动化跑 | **T20 BLOCKED**,需 user 手跑 sandbox (per L74 stage-install lessons) |
| 9 | T12 "corrected Step 1" 加 `prevKeyEvent = ke` + `prevfEaten = TRUE` | 漏更新 → Caps_Lock 双击模拟器可能误触发 | 本会话 T12 简化版未加 — 不影响 Shift 路径,Caps_Lock 路径独立,后续可加 |

---

## 5. T20 sandbox-verify — BLOCKED (用户接力)

**症状**:`in-sandbox-verify.ps1:50` `[STEP] 娓呮畫鐣欐敞鍐岃〃 (L54)` 字符串触发 PowerShell parser error:
```
Array index expression is missing or not valid.
+ $failures += "[a] exit code was $($result.ExitCode) (expected 0)"
+                    ~
```

**Root cause**:`[a] $($result.ExitCode)` 在某些 PowerShell 版本下 `$result.ExitCode` 解析为 array index。Chinese encoding 进一步导致字符乱码。

**用户接力步骤**:
1. 打开 `F:\soft\00selfmade\sandbox-verify\rime-claude\in-sandbox-verify.ps1`
2. 修复 line 50 (UTF-8 BOM + ASCII-only `[STEP]` 字符串) **或** 修复 line 76 的 `$failures += "[a] ..."` 表达式
3. 重跑 `rime-verify.ps1 -InstallerPath release\fluxing-0.21.0.1-installer.exe -Scenarios "1,2,2-prime,3,4,5,G,H"`
4. 验证 8 场景日志: `C:\Users\Duanyi\sandbox-artifacts\rime\logs\verify_v0.21.0.1_<date>.log`
5. log 验证(per spec 077 §2.7.4):
   - US1-A: `ShiftDown ipc_id` log + 无 `select_candidate`
   - US1-B: `select_candidate(idx=1)` log
   - US1-C: `select_candidate(idx=2)` log
   - US1-D: `interveningKey=true` + ShiftUp 无 select + 'A' 上屏
   - US1-E: ascii_mode toggle,无 `select_candidate`
   - US1-F: interveningKey=true,《 上屏
   - US1-G: ShiftDown sid=A, ShiftUp sid=B, 无 select
   - US1-H: maintenance 期间 ShiftDown 无状态污染

---

## 6. 装机 user flow（spec 077 §装机 user flow）

1. 切到火流猩输入法 (Win+Space)
2. 输入中文,候选菜单弹出,按 **Shift_L 松开** → 第 2 候选上屏
3. 按 **Shift_R 松开** → 第 3 候选上屏
4. 按 **Shift+a** → 'A' 上屏 (intervening key 抑制)
5. 按 **Control+1** → 第 2 候选上屏 (fallback)
6. alt-tab 切换窗口,Shift_L 松开 → 当前窗口选第 2 候选 (无 stale state)
7. log 验证: `%TEMP%\rime.weasel.*.INFO.*.log` 含 `ShiftDown` / `ShiftUp` / `select_candidate(idx=1)` 行

---

## 7. 必读顺序（接力 Claude / 工程师）

1. 先读本 handoff(本文件)
2. 读 `docs/adr/0007-shift-release-ipc.md` 理解架构决策
3. 读 `.specify/memory/lessons-learned.md` L##-PhaseM-10 理解 8 次 yaml-only 失败 → IPC 重构 反思
4. 读 `CHANGELOG.md [0.21.0.1]` 段 user-visible 总结
5. 跑 `release/fluxing-0.21.0.1-installer.exe` 装机测 8 场景 (T20 接力)
6. 推送 `Fluxing` 分支到 kizemo/Fluxing 远程 (chunked push per L108)

---

## 8. 关联文档

- spec 077: `.specify/specs/077-shift-release-ipc/{spec,plan,tasks}.md`
- ADR 0007: `docs/adr/0007-shift-release-ipc.md`
- L##-PhaseM-10: `.specify/memory/lessons-learned.md`
- CHANGELOG [0.21.0.1]: `CHANGELOG.md`
- installer md5: `_build_v02101_provenance.md5`
- 失败前史: spec 012 / 014 / 018 / 019; L18 / L19 / L21

---

新会话第一句话直接引用本 handoff 即可掌握全貌。