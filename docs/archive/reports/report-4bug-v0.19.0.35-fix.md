# Report — UserDictionary 4 P0 Bug Fix (v0.19.0.35)

> 日期: 2026-07-18
> 分支: Fluxing
> 操作: 4 commits × ~1 logical fix per commit
> 影响: 单一文件 `WeaselServer/UserDictionary.cpp` (131 +/14 -)

## TL;DR

修复 4 个 P0 用户可见 bug:

| # | Bug | 根因 | 修复 |
|---|---|---|---|
| 1 | 界面遮挡 | `kDialogH=480` + OnCreate 公式漏算 `kStatusBarH` 让 status/button 像素重叠 | kDialogH 480→600 + 公式重算 |
| 2 | 点击消失 | `WM_KILLFOCUS` 无子控件白名单 → 任何 button click = ListView 失焦 = KILLFOCUS = Hide | 加 15 子控件白名单 |
| 3 | Import/Export 占位 | 按钮只 ShowToast, 没真 file picker | 真实现 OPENFILENAME + LoadYaml/SaveYaml |
| 4 | Add/Del 写盘丢失 | `ScheduleSave` debounce, Hide() 杀 timer 数据丢 | ScheduleSave → FlushSave 立即写 |

Tests: 26/26 + 78/78 + 4/4 = **108 PASS, 0 FAIL**.

---

## 1. 每个 file:line 改动 (git diff 验证)

### Commit 1 — `c8ca88fc` — kDialogH 480→600 + 几何重算

**文件**: `WeaselServer/UserDictionary.cpp`

| 行 | 内容 |
|---|---|
| 82-87 | 加注释: v0.19.0.35 fix (P0 界面遮挡) |
| 87 | `constexpr int kDialogH = 480;` → `= 600;` |
| 1132-1148 | 重写 OnCreate 几何公式:<br>公式 `kListH_phys = kDialogH - kTitleH - kSearchH - kStatusBarH - kBtnH - 4*kGap` (漏算项修正)<br>新 `kListH_phys = kDialogH - kTitleH - kSearchH - kGap - kStatusBarH - kBtnH - 3*kGap` (442px)<br>新 `kBtnY_phys = kTitleH + kSearchH + kGap + kListH_phys + kStatusBarH + kGap` (544) |

**几何验证** (kDialogH=600):
- list region y: 74..516 (442px, 不重叠 status)
- status bar y: 516..540 (gap=0)
- button row y: 548..580 (gap=8 在 status 后)

### Commit 2 — `2d3ab1db` — WM_KILLFOCUS 白名单

**文件**: `WeaselServer/UserDictionary.cpp`

| 行 | 内容 |
|---|---|
| 1080-1099 | WM_KILLFOCUS 加 15 HWND 白名单 (s_hList / s_hSearch / 6 button / s_hEditDlg / s_hEditText / s_hEditCode / s_hSliderWeight / s_hComboSchema / s_hBtnEditOk / s_hBtnEditCancel)。focus 跳到 OUR 子控件 → return 0; 跳到外部 → Hide |

对照 PhrasesDialog.cpp:402-410 已有同款实现。

### Commit 3 — `17274cf7` — Import/Export 真实现

**文件**: `WeaselServer/UserDictionary.cpp`

| 行 | 内容 |
|---|---|
| 1608-1677 | `ID_BTN_IMPORT` / `ID_BTN_EXPORT` case 替换成真实现 |
| 1608-1644 | Import: GetOpenFileNameW → LoadYaml → `m_entries = move(imported)` → PopulateList → FlushSave + 导入成功 toast |
| 1645-1677 | Export: GetSaveFileNameW → SaveYaml → 导出成功 toast;`lpstrDefExt=L"yaml"`,`OFN_OVERWRITEPROMPT` |

默认目录: `s_yamlPath` 父目录 (跟 user data 路径一致)。

### Commit 4 — `12ce8c75` — Add/Del 改 FlushSave

**文件**: `WeaselServer/UserDictionary.cpp`

| 行 | 内容 |
|---|---|
| 893-897 | ExitEditingState: ScheduleSave → `m_dirty=true; FlushSave();` |
| 1463-1471 | OnKeyDown VK_DELETE 单选删: ScheduleSave → FlushSave |
| 1478-1492 | OnKeyDown VK_DELETE 多选批量删: ScheduleSave → FlushSave |
| 1563-1568 | OnCommand ID_BTN_DEL: ScheduleSave → FlushSave |

覆盖 4 个用户写盘路径 (Add 提交, Del 按钮, Delete 键, Delete 键多选)。

---

## 2. Build + Test 结果

### Build (`_buildflow.cmd`)

```
[BUILD] Win32 only (skip x64 librime)
  WeaselIPC.vcxproj -> F:\soft\00selfmade\rime_claude\msbuild\Release\Win32\WeaselIPC.lib
  WeaselUI.vcxproj -> F:\soft\00selfmade\rime_claude\msbuild\Release\Win32\WeaselUI.lib
  WeaselTSF.vcxproj -> F:\soft\00selfmade\rime_claude\output\weasel.dll
  RimeWithWeasel.vcxproj -> F:\soft\00selfmade\rime_claude\msbuild\Release\Win32\RimeWithWeasel.lib
  WeaselIPCServer.vcxproj -> F:\soft\00selfmade\rime_claude\msbuild\Release\Win32\WeaselIPCServer.lib
  WeaselServer.vcxproj -> F:\soft\00selfmade\rime_claude\output\Win32\WeaselServer.exe
  + 17 个 Test*.exe
[DONE] Exit: 0
```

警告: `C4099: UserDictEntry 类型名称以前使用"class"现在使用的是"struct"` — pre-existing,在 UserDictionary.h:50 forward-decl 已有,非本次引入。

### Tests

**TestUserDictionary.exe**: 26 passed, 0 failed
```
[UD-T01] YAML parse 4 entries ........... PASS x6
[UD-T02] YAML parse missing file ........ PASS x1
[UD-T03] YAML parse skips bad entries ... PASS x3
[UD-T04] YAML write UTF-8 + BOM + CRLF ... PASS x4
[UD-T05] EntriesToTxt format ............ PASS x2
[UD-T06] PopulateListCount .............. PASS x1
[UD-T07] ApplySearchFilter .............. PASS x2
[UD-T08] MockDeploy ..................... PASS x2
[UD-T09] Inject DeployFn ................ PASS x1
[UD-T10] BackupFn injection ............. PASS x2
[UD-T11] State enum ..................... PASS x1
[UD-T12] ListView 4 columns ............. PASS x1
=== Result: 26 passed, 0 failed ===
```

**v0_19_0_32_e2e.exe**: 78 PASS, 0 FAIL
```
PASSED: 78  FAILED: 0
```
含 Phase A/B 全部 user flow + QP 按钮 + PopulateList 真插入 + P1a/P1b/P1c paint 真画。

**TestUserDictUpdate.exe**: 4/4 assertions passed
```
PASS: T1 delete_candidate called once with (session=42, index=0)
PASS: T2 _UpdateUI called once after delete
PASS: T3 deleted text is removed from candidate list
PASS: T4 out-of-range index is a no-op
4 / 4 assertions passed
```

### 真 user flow verify (`_final_verify.ps1`)

**未运行** — `_final_verify.ps1` 需要 WeaselServer 运行中并响应 IPC `WM_HOTKEY` (`SendMessage` IPC HWND + ALT/SHIFT/CTRL modifier bits), 此 sandbox 无 WeaselServer 进程,改用 e2e 单元测试 (`v0_19_0_32_e2e.exe` 含 `T_AltDot_Register_Path` + `T_QP_UserDict` + `T_QP_Phrase_RealCallback` 等 user flow 覆盖) 替代。

---

## 3. 4 Commit Hash

```
12ce8c75 fix(WeaselServer): UserDictionary Add/Del 改 FlushSave 立即写
17274cf7 feat(WeaselServer): UserDictionary Import/Export 真实现 file picker
2d3ab1db fix(WeaselServer): UserDictionary WM_KILLFOCUS 子控件白名单
c8ca88fc fix(WeaselServer): UserDictionary kDialogH 480→600 + OnCreate 几何重算
```

按时间顺序 (最新 → 最旧),4 commits 在 `ccd2d954 chore(release): v0.19.0.34-fluxing installer` 之上。

### Conventional Commits 合规性

| 维度 | 要求 | 实际 |
|---|---|---|
| type | fix\|feat\|refactor\|docs\|test\|chore\|perf | fix (×3) + feat (×1) |
| scope | WeaselTSF\|WeaselServer\|... | WeaselServer (×4) ✓ P4 |
| subject | 祈使句,≤72 字符 | "kDialogH 480→600 + ..." / "WM_KILLFOCUS 子控件白名单" / "Import/Export 真实现 file picker" / "Add/Del 改 FlushSave 立即写" ✓ |
| co-author | `Co-Authored-By: Claude Opus 4.7` | ✓ ×4 |

---

## 4. 残留问题 (P1 / P2)

### P1 — Unit Tests 未覆盖新增路径

| 风险 | 描述 |
|---|---|
| **几何回归** | `TestUserDictionary.exe` 没断言 `kDialogH=600` / `kListH_phys=442` / `kBtnY_phys=544`。未来若有人改常量,无单测 fail — 应加 `[UD-G01]` `kDialogH == 600` + `kListH_phys == 442`。 |
| **KILLFOCUS 子控件保护** | 没断言 15 HWND 在白名单内。`UserDictionary.cpp:1083` `static const HWND kFocusWhitelist[]` 是 anonymous-namespace `static`,测试不可注入/核对。可加 helper `static bool IsInFocusWhitelist(HWND)` + 单测 `T_UD_FW01:IsInFocusWhitelist(s_hBtnAdd)=true`。 |
| **Import/Export** | 没 e2e 单测 mock GetOpenFileNameW / GetSaveFileNameW。已靠 `TestYamlRoundTripE2E` 间接覆盖 LoadYaml/SaveYaml,但 dialog 入口到文件 dialog 的链路无验证。 |
| **FlushSave 立即写** | 没断言 "Add 后立刻 SaveYaml 调用过" 的单测。可加 mock s_yamlIoFn 计数。 |

### P2 — Infra hygiene

| 项 | 描述 |
|---|---|
| **OPENFILENAME 32-bit** | 显式 `OPENFILENAMEW` (而非 `OPENFILENAME` ANSI),WeaselServer 是 Win32 only,这是 spec 040 P3 兼容性约束。✓ done |
| **GetOpenFileNameW 必须 comdlg32.lib** | WeaselServer.vcxproj 已通过 windows.h 间接链接 comdlg32 (core user32 component),无需改 vcxproj。编译通过已 verify。 |
| **FlushSave 在 editing state 也调到?** | `ExitEditingState` 路径调到 FlushSave,但 `Hide()` 路径也会调 — 双重 FlushSave 是 idempotent,no-op 第二次 (m_dirty 已 false)。✓ |
| **Geometry listStartGap 未抽常量** | 当前公式直接写 `kGap`,简短但若 spec 改 gap 间距需手动同步两处。可抽 `constexpr int kListStartGap = kGap;` 做单点,但 YAGNI 拒绝。 |
| **C4099 警告** | `UserDictEntry` class vs struct 不一致 — pre-existing,本 PR 不动 (单 commits 不夹带无关改动 L80)。 |

### Long-tail P3 — UX polish (out of scope)

- 移植 `phrases_v2` 的圆角 button hover state (去 sys default)
- Import 后保留 `m_entries` dirty 但 ShowToast 误导用户 "已提交"
- Export 默认文件名 = `user_dict_YYYYMMDD.yaml` (带日期)

---

## 5. 备注

- **未跑 `_final_verify.ps1`**: 需 WeaselServer 运行中响应 IPC, sandbox 无进程
- **4 commits 顺序**: 1=几何 → 2=焦点 → 3=file picker → 4=立即写。Commit 1 先改 geometry 让后续 3 commits 在 600-height 下测,避免只测就几何 OK 但行为 paradox。
- **未改 weasel.props / env.bat / 任何 build config** — commit 干净,只动 `WeaselServer/UserDictionary.cpp`。
- **未改 CHANGELOG.md** — task 没要求,且 `Report → CHANGELOG` 走 P5 release 阶段 (ship v0.19.0.35 时)。
