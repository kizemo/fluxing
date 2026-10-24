# Verification Report — v0.19.0.36 (Phase D Phrase) — 5 Axis

**Date**: 2026-07-19 17:42
**Verifier**: Claude (Phase D 收尾 after fbd44a04 session-health incident)
**Subject**: Fluxing v0.19.0.36 **source-only** Phrase Phase D 收尾 (commit `5068922` + `7c06b89`)

---

## TL;DR

**Source verified clean** — v0.19.0.36 Phrase Phase D 收尾 commit `5068922` + infra fix `7c06b89` 解决了 v0.19.0.35 ship 时埋的 2 个未捕获 link error (MoveSelection definition 缺失 + TestPhrasesDialog 缺 imm32.lib)。Build Exit 0 + 173 unit/e2e tests PASS (69 + 78 + 26) 是新 hard evidence — 不再受 v0.19.0.35 的 stale-binary 问题影响。

**Installer rebuild + user 装机 = NOT IN SCOPE** (sandbox 无 NSIS 工具,装机需 user)。下游 ship gate 见 §5。

---

## 5-axis verification

| Axis | Status | Evidence |
|---|---|---|
| **1. build (incremental)** | **PASS** | `_buildflow.cmd` Exit 0; `output/Win32/WeaselServer.exe` mtime 2026-07-19 17:27 (≠ 之前 7-18 20:37 stale binary) |
| **1'. build (force rebuild)** | **PASS** | `msbuild test/TestPhrasesDialog/TestPhrasesDialog.vcxproj /t:Rebuild` Exit 0; `Release/TestPhrasesDialog.exe` mtime 2026-07-19 17:36, size 310272 bytes |
| **2. unit tests** | **PASS** | `TestPhrasesDialog` 69/69 + `TestUserDictionary` 26/26 + `v0_19_0_32_e2e` 78/0 — 合计 173 PASS / 0 FAIL |
| **3. 真 user flow (e2e)** | **PASS** | `v0_19_0_32_e2e` 78 case 含 T_QP_Phrase_RealCallback + T_AltDot_Register_Path + T_QP_Icon_Unique, 验证 QuickPanel button 1 → PhrasesDialog 真链, Alt+. hotkey wire, UserDict icon ≠ Account icon |
| **4. installer rebuild** | **NOT IN SCOPE** | Sandbox 无 NSIS makensis 工具链 (需 D:\soft\08tools\nsis) — installer rebuild 留 user-side |
| **5. 真 user 装机 feedback** | **PENDING** | 需要 user 装 v0.19.0.36 + 验证 Alt+. + 默认选第一条 + 中文 IME 4 项 |

---

## Axis 1 / 1' — Build evidence (NEW)

**v0.19.0.35 ship 时** (commit `fa196049`, 7-18 20:51) binary stale (md5 `389610fb` from 7-18 20:37) 因为 `SavePhrases` line 345 lambda 缺 `;` 终止符 → MSVC C2601 abort。Verify report `report-verify-v0.19.0.35-5axis-2026-07-19.md` 判定 HOLD — DO NOT SHIP。

**v0.19.0.36 Phase D commit `5068922` 后**:

| Binary | md5 / mtime | 说明 |
|---|---|---|
| `output/Win32/WeaselServer.exe` | 2026-07-19 17:27 (今天 rebuild) | 含 MoveSelection definition + SavePhrases lambda 重排 + ImmDestroyContext |
| `Release/TestPhrasesDialog.exe` | 2026-07-19 17:36, 310272 bytes | 修 v0.19.0.35 ship 时缺 imm32.lib 的 link error |

**Build 命令**:
```bash
powershell -File build_test_phrases.ps1    # 单 test, incremental
_buildflow.cmd                              # 完整 sln (含 librime skip x64)
```

`build_test_phrases.ps1` 内容:
```powershell
$env:BOOST_ROOT='F:\b183'
$env:PLATFORM_TOOLSET='v143'
$env:PATH='C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin;' + $env:PATH
Set-Location 'F:\soft\00selfmade\rime_claude'
msbuild 'test\TestPhrasesDialog\TestPhrasesDialog.vcxproj' /t:Rebuild `
  /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 `
  /p:BOOST_ROOT=F:\b183 /p:SolutionDir=F:\soft\00selfmade\rime_claude\ `
  /v:minimal /nologo
```

---

## Axis 2 / 3 — Test evidence (NEW, 173 PASS / 0 FAIL)

| Test | Result | 涵盖 |
|---|---|---|
| `Release/TestPhrasesDialog.exe` | **69/69 PASS** | YAML parse/write + UI 控件创建 + user flow (Add/Edit/Del/Cancel) + 默认选第一条 + IME (OnCreate ImmCreateContext+Associate+Destroy 调用成功 link) |
| `Release/TestUserDictionary.exe` | **26/26 PASS** | UserDict 4 P0 回归保护 (kDialogH=600 + WM_KILLFOCUS 白名单 + Import/Export 真 picker + FlushSave 立即写) |
| `release/v0_19_0_32_e2e.exe` | **78/78 PASS** | QP button chain 真 invoke + Alt+. Register path + QP UserDict icon ≠ Account + PopulateList 真插入 + P1a/P1b/P1c paint |

**Test gap** (P2 follow-up, NOT blocker):
- MoveSelection 单元 test 缺失。TestPhrasesDialog 11.1-11.9 测 UI 控件创建,**不** 测 OnKeyDown VK_UP/VK_DOWN → MoveSelection 行为路径。fix definition 后 link 通过, 但 unit 层断言键盘导航行为是后续 P2。
- 建议: 加 `[Test 19] OnKeyDown VK_UP/VK_DOWN` (m_phrases 5 个 → 按 ↓ 4 次 → 选中 index 4, 按 ↑ 1 次 → 选中 index 3)。

---

## Axis 4 — Installer rebuild (OUT OF SCOPE)

**Sandbox 状态**: `Release/fluxing-0.19.0.34-installer.exe` ~ v0.19.0.34 都在,但**无** `v0.19.0.36-installer.exe`。

**User-side 步骤** (装机前必跑, 见 handoff §6 风险点):
1. `_buildflow.cmd` Exit 0 (already verified)
2. 手动调 makensis: `makensis output/install.nsi` → expect `release/fluxing-0.19.0.36-installer.exe`
3. `certutil -hashfile release\fluxing-0.19.0.36-installer.exe MD5` → expect ≠ v0.19.0.35 (`df94e18f`)
4. 提取 installer → `certutil -hashfile _extract_v36\WeaselServer.exe MD5` → expect = 今天 build (commit `5068922` source 的 md5)
5. `_check_install_v2.ps1` 6 模块 verify (running md5 vs install md5 vs source md5)

**Installer hardening (v0.19.0.36)** 已经在 commit `af13cbff` ship:
- 5 加强 L72-fix + 老 binary force delete
- regsvr32 success-path unconditional WriteReg (L66-fix 不退化)
- /D=<path> silent install 必带 (L13/L17)

---

## Axis 5 — 真 user flow (装机后 PENDING)

**装机后 user 必测 5 项** (按 handoff §7 装机 verify checklist):

| # | 测试 | 期望 |
|---|---|---|
| 1 | Alt+. 调出 PhrasesDialog | 弹 modal + 9+ children visible |
| 2 | QuickPanel button 1 (Phrase) click → 弹 PhrasesDialog | 不是 explore(install_dir) |
| 3 | 默认选第一条 | m_selectedIndex=0 + LVIS_SELECTED on item 0 |
| 4 | 中文 IME 输入 | 候选窗显示 (之前 v0.19.0.34 报"只能英文") |
| 5 | 按 ↑↓ 切换选中 | 选中 index 跟着移 (本次新修的 MoveSelection 路径) |

**回退 plan**: 如果 Axis 5 任一项 FAIL, revert commit `5068922` + `7c06b89` + reinstall v0.19.0.35 binary (注意 v0.19.0.35 binary 是 stale, 实际是 v0.19.0.32 行为)。

---

## 与 v0.19.0.35 verify report 对比

**v0.19.0.35 verify** (`report-verify-v0.19.0.35-5axis-2026-07-19.md`) 判定 HOLD - DO NOT SHIP:
- Axis 1' force rebuild FAIL (C2601 SavePhrases lambda 缺 `;`)
- Axis 2 PASS 但 misleading (test 不测 binary)
- Axis 3 FAIL (binary RoundRectRgn / SetWindowRgn 仍 active → commit message 撒谎)
- Axis 4 FAIL (no ImmAssociateContext / SysTreeView32 in binary)
- Axis 5 conditional correctness (L72-fix protect 但 binary stale → silently succeed 写老 binary)

**v0.19.0.36 verify (本文)**: Axis 1/1'/2/3 PASS, Axis 4/5 out of scope / pending user。
- C2601 已通过 commit `5068922` SavePhrases lambda 重排彻底修 (escape 移到 for 顶, 标准 auto lambda + `;`)
- 新 hard evidence: Build Exit 0 + 173 test PASS + 4 commit (5068922 + 7c06b89 + af13cbff + fa196049) 链接成 ship-ready 链
- Axis 5 真 user 反馈仍需 user 装机 verify (L100-Z1 / L100-Z4 PE-import-scan 已写入 lessons learned)

---

## Commit hash (本 verify 涵盖)

```
5068922 fix(WeaselServer): v0.19.0.36 (Phase D) — MoveSelection definition + SavePhrases lambda 重排 + ImmDestroyContext
7c06b89 fix(ci): TestPhrasesDialog.vcxproj 加 imm32.lib (修 v0.19.0.35 ship 漏的 link error)
af13cbff fix(installer): v0.19.0.36 - 5 加强 L72-fix + 老 binary force delete (Phase D)
fa196049 feat(WeaselServer): v0.19.0.35 - 修 Phrase 4 P0 bug + 恢复 v0.19.0.25 UX
```

---

## Verdict

**Source verified PASS.** v0.19.0.36 Phrase Phase D 收尾 source + test + e2e 全部 clean。Installer rebuild + user 装机是 ship gate, 不在本 verify 范围。

— END OF REPORT —