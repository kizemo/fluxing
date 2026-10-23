# 079 damage inventory

> Pre-repair state: revision `HEAD` (before this spec landed).
> Damage class: a C escape sequence (`\a` `\b` `\t` `\v` `\f` `\r`)
> was un-escaped into the raw control byte, and the control byte
> swallowed the letter that had formed the escape.

| target | damaged lines | rewrite map |
|---|---|---|
| `.specify/PRD.md` | 5 | `rewrite-map-prd.json` |
| `.specify/TDD.md` | 3 | `rewrite-map-tdd.json` |
| `.specify/specs/001-user-visible-strings/tasks.md` | 9 | `rewrite-map-spec001-tasks.json` |
| `.specify/specs/014-restore-shift-select-2nd-3rd-candidate/plan.md` | 11 | `rewrite-map-spec014-plan.json` |
| `.specify/specs/014-restore-shift-select-2nd-3rd-candidate/spec.md` | 18 | `rewrite-map-spec014-spec.json` |
| `.specify/specs/014-restore-shift-select-2nd-3rd-candidate/tasks.md` | 15 | `rewrite-map-spec014-tasks.json` |
| `.specify/specs/016-behavior-level-test-framework/spec.md` | 5 | `rewrite-map-spec016-spec.json` |
| `.specify/specs/037-fluxing-components-v0/plan.md` | 3 | `rewrite-map-spec037-plan.json` |
| `.specify/specs/037-fluxing-components-v0/spec.md` | 3 | `rewrite-map-spec037-spec.json` |
| `test/TestShiftSelectBinding/TestShiftSelectBinding.cpp` | 1 | `rewrite-map-test-shift-select-binding.json` |
| `.specify/memory/lessons-learned.md` | 5 | `rewrite-map-lessons-learned.json` |
| `.specify/specs/015-fix-test-infrastructure/tasks.md` | 1 | `rewrite-map-spec015-tasks.json` |
| `.specify/specs/037-fluxing-components-v0/tasks.md` | 4 | `rewrite-map-spec037-tasks.json` |
| `CHANGELOG.md` | 4 | `rewrite-map-changelog.json` |

## per-file detail

### `.specify/PRD.md` (5 line(s))

| line | damaged | restored |
|---|---|---|
| 386 | `L47 lessons-learned 同步追加 (PowerShell 5.1 + VsDevCmd 触发 MSB6001 PATH/Path 冲突 + vcxproj $(SolutionDir)msbuild 缺 \ 引发 MSB3491; workaround: 用 <0B>cvars32.bat 不用 VsDevCmd.bat).` | `L47 lessons-learned 同步追加 (PowerShell 5.1 + VsDevCmd 触发 MSB6001 PATH/Path 冲突 + vcxproj $(SolutionDir)msbuild 缺 \ 引发 MSB3491; workaround: 用 vcvars32.bat 不用 VsDevCmd.bat).` |
| 441 | `- **L47 (待追加)**: PowerShell 5.1 启动 cmd 时把 PATH 转为 Path (小写), 而 VsDevCmd.bat 是 PowerShell module 触发 .NET Hashtable "已添加项: 字典中的关键字 PATH 所添加的关键字 Path" 异常 (MSB6001). workaround: 用 <0B>cvars32.bat (纯 cmd 脚本) 不用 VsDevCmd.bat. 0.18.25.0 ship 时已用此 workaround.` | `- **L47 (待追加)**: PowerShell 5.1 启动 cmd 时把 PATH 转为 Path (小写), 而 VsDevCmd.bat 是 PowerShell module 触发 .NET Hashtable "已添加项: 字典中的关键字 PATH 所添加的关键字 Path" 异常 (MSB6001). workaround: 用 vcvars32.bat (纯 cmd 脚本) 不用 VsDevCmd.bat. 0.18.25.0 ship 时已用此 workaround.` |
| 484 | `L47 lessons-learned 同步追加 (PowerShell 5.1 + VsDevCmd 触发 MSB6001 PATH/Path 冲突 + vcxproj SolutionDir-msbuild 缺 \ 引发 MSB3491; workaround: 用 <0B>cvars32.bat 不用 VsDevCmd.bat).` | `L47 lessons-learned 同步追加 (PowerShell 5.1 + VsDevCmd 触发 MSB6001 PATH/Path 冲突 + vcxproj SolutionDir-msbuild 缺 \ 引发 MSB3491; workaround: 用 vcvars32.bat 不用 VsDevCmd.bat).` |
| 527 | `- **L47 (待追加)**: PowerShell 5.1 启动 cmd 时把 PATH 转为 Path (小写), 而 VsDevCmd.bat 是 PowerShell module 触发 .NET Hashtable "已添加项: 字典中的关键字 PATH 所添加的关键字 Path" 异常 (MSB6001). workaround: 用 <0B>cvars32.bat (纯 cmd 脚本) 不用 VsDevCmd.bat. 0.18.25.0 ship 时已用此 workaround.` | `- **L47 (待追加)**: PowerShell 5.1 启动 cmd 时把 PATH 转为 Path (小写), 而 VsDevCmd.bat 是 PowerShell module 触发 .NET Hashtable "已添加项: 字典中的关键字 PATH 所添加的关键字 Path" 异常 (MSB6001). workaround: 用 vcvars32.bat (纯 cmd 脚本) 不用 VsDevCmd.bat. 0.18.25.0 ship 时已用此 workaround.` |
| 605 | `- **L43 /LTCG:OFF 已被 L43 cure 解决**: WeaselTSF/xmake.lua 已加 <07>dd_shflags('/DEBUG /LTCG:OFF /OPT:NOREF /OPT:NOICF', {force = true})，global xmake.lua line 64 也加 <07>dd_ldflags('/LTCG:OFF /INCREMENTAL:NO', {force = true})。WeaselServer.exe 不存在 dead-strip 问题。` | `- **L43 /LTCG:OFF 已被 L43 cure 解决**: WeaselTSF/xmake.lua 已加 add_shflags('/DEBUG /LTCG:OFF /OPT:NOREF /OPT:NOICF', {force = true})，global xmake.lua line 64 也加 add_ldflags('/LTCG:OFF /INCREMENTAL:NO', {force = true})。WeaselServer.exe 不存在 dead-strip 问题。` |

### `.specify/TDD.md` (3 line(s))

| line | damaged | restored |
|---|---|---|
| 342 | `PowerShell 5.1 启动 cmd 时把 PATH 转为 Path (小写), 而 VsDevCmd.bat 触发 .NET Hashtable "已添加项: 字典中的关键字 PATH 所添加的关键字 Path" 异常 (MSB6001). workaround: 用 <0B>cvars32.bat 不用 VsDevCmd.bat (<0B>cvars32.bat 是纯 cmd 脚本, 不触发此 .NET 冲突).` | `PowerShell 5.1 启动 cmd 时把 PATH 转为 Path (小写), 而 VsDevCmd.bat 触发 .NET Hashtable "已添加项: 字典中的关键字 PATH 所添加的关键字 Path" 异常 (MSB6001). workaround: 用 vcvars32.bat 不用 VsDevCmd.bat (vcvars32.bat 是纯 cmd 脚本, 不触发此 .NET 冲突).` |
| 344 | `0.18.25.0 ship 时已统一用 <0B>cvars32.bat 路径. 0.18.25.0 之前几个 release 误以为 "xbuild 路径全过就 ship" 但 msbuild 路径被 VsDevCmd 阻塞, 是 L46 发现的盲点.` | `0.18.25.0 ship 时已统一用 vcvars32.bat 路径. 0.18.25.0 之前几个 release 误以为 "xbuild 路径全过就 ship" 但 msbuild 路径被 VsDevCmd 阻塞, 是 L46 发现的盲点.` |
| 552 | `复盘确认 L43 cure (WeaselTSF/xmake.lua <07>dd_shflags("/LTCG:OFF ...")) 已应用 + global xmake.lua line 64 <07>dd_ldflags("/LTCG:OFF /INCREMENTAL:NO", {force=true}) 也加。spec 041 ship 的 WeaselServer.exe 1981952 bytes 实际含所有 spec 041 DPI fix 函数 (?GetPhysicalClientRect@FluxingD2DRenderer@ui@fluxing@@..., ?HandleDpiChanged@FluxingPanel@ui@fluxing@@..., ?HandleDpiChanged@FluxingToggle@ui@fluxing@@...) + 4 Fluxing 控件 + QuickPanelDialog (dumpbin /DISASM 验证)。` | `复盘确认 L43 cure (WeaselTSF/xmake.lua add_shflags("/LTCG:OFF ...")) 已应用 + global xmake.lua line 64 add_ldflags("/LTCG:OFF /INCREMENTAL:NO", {force=true}) 也加。spec 041 ship 的 WeaselServer.exe 1981952 bytes 实际含所有 spec 041 DPI fix 函数 (?GetPhysicalClientRect@FluxingD2DRenderer@ui@fluxing@@..., ?HandleDpiChanged@FluxingPanel@ui@fluxing@@..., ?HandleDpiChanged@FluxingToggle@ui@fluxing@@...) + 4 Fluxing 控件 + QuickPanelDialog (dumpbin /DISASM 验证)。` |

### `.specify/specs/001-user-visible-strings/tasks.md` (9 line(s))

| line | damaged | restored |
|---|---|---|
| 297 | `### A1. <TAB>asks.md internal duplication` | `### A1. tasks.md internal duplication` |
| 310 | `- Current HEAD~ = <08>543d5e (chore: rename WEASEL_VERSION identifier).` | `- Current HEAD~ = b543d5e (chore: rename WEASEL_VERSION identifier).` |
| 312 | `  "<0C>eat(fluxing): rename distribution code to Fluxing; swap` | `  "feat(fluxing): rename distribution code to Fluxing; swap` |
| 314 | `  <08>543d5e.` | `  b543d5e.` |
| 317 | `  matches T010's step-2 anchor text. <08>543d5e was not yet` | `  matches T010's step-2 anchor text. b543d5e was not yet` |
| 333 | `  phrase — but <08>543d5e rewrote the line to` | `  phrase — but b543d5e rewrote the line to` |
| 360 | `  <08>543d5e (FLUXING_VERSION identifier rename + NSIS` | `  b543d5e (FLUXING_VERSION identifier rename + NSIS` |
| 369 | `  comment with 中州韻, replaced 式恕堂 → <07>iec.fun at` | `  comment with 中州韻, replaced 式恕堂 → aiec.fun at` |
| 382 | `  changed to the project's own <07>iec.fun equivalents.` | `  changed to the project's own aiec.fun equivalents.` |

### `.specify/specs/014-restore-shift-select-2nd-3rd-candidate/plan.md` (10 line(s))

| line | damaged | restored |
|---|---|---|
| 8 | `spec 012 form (<07>ccept: Shift+Shift_L, not bare Shift_L) so that the binding` | `spec 012 form (accept: Shift+Shift_L, not bare Shift_L) so that the binding` |
| 31 | `  - <TAB>est/TestDefaultHotkeys/TestDefaultHotkeys.cpp - flip 2 L19 negatives` | `  - test/TestDefaultHotkeys/TestDefaultHotkeys.cpp - flip 2 L19 negatives` |
| 33 | `  - <TAB>est/TestShiftSelectBinding/TestShiftSelectBinding.cpp (new) - runtime` | `  - test/TestShiftSelectBinding/TestShiftSelectBinding.cpp (new) - runtime` |
| 35 | `  - <TAB>est/TestShiftSelectBinding/TestShiftSelectBinding.vcxproj (new) -` | `  - test/TestShiftSelectBinding/TestShiftSelectBinding.vcxproj (new) -` |
| 37 | `  - <TAB>est/TestShiftSelectBinding/TestShiftSelectBinding.vcxproj.filters (new)` | `  - test/TestShiftSelectBinding/TestShiftSelectBinding.vcxproj.filters (new)` |
| 43 | `  - <0D>elease/fluxing-0.18.8.0-installer.exe (new) - copy from` | `  - release/fluxing-0.18.8.0-installer.exe (new) - copy from` |
| 52 | `  - Conventional Commits with <0C>luxing scope (per P4 amendment 1)` | `  - Conventional Commits with fluxing scope (per P4 amendment 1)` |
| 98 | `\| R3 \| new Shift+Shift_L/R binding collides with Shift+space toggle \| different <07>ccept -> different KeyEvent -> different map bucket; no collision \|` | `\| R3 \| new Shift+Shift_L/R binding collides with Shift+space toggle \| different accept -> different KeyEvent -> different map bucket; no collision \|` |
| 109 | `- Commit message uses <0C>ix(fluxing): scope per P4` | `- Commit message uses fix(fluxing): scope per P4` |
| 111 | `- Optional lightweight tag <0B>0.18.8.0 (Fluxing-side, not pushed to kizemo` | `- Optional lightweight tag v0.18.8.0 (Fluxing-side, not pushed to kizemo` |

### `.specify/specs/014-restore-shift-select-2nd-3rd-candidate/spec.md` (18 line(s))

| line | damaged | restored |
|---|---|---|
| 18 | `- spec 012 plan.md sec 2.2 already designed the fix: use <07>ccept: Shift+Shift_L` | `- spec 012 plan.md sec 2.2 already designed the fix: use accept: Shift+Shift_L` |
| 19 | `  and <07>ccept: Shift+Shift_R form (not bare Shift_L) so that TSF release events` | `  and accept: Shift+Shift_R form (not bare Shift_L) so that TSF release events` |
| 21 | `- L18 fix: removed the <07>lways: Shift+Shift_L/R toggle ascii_mode binding (was` | `- L18 fix: removed the always: Shift+Shift_L/R toggle ascii_mode binding (was` |
| 79 | `- And the <07>scii_composer.switch_key.Shift_L/R remain ` | `- And the ascii_composer.switch_key.Shift_L/R remain ` |
| 91 | `\| <TAB>est/TestDefaultHotkeys/TestDefaultHotkeys.cpp \| Update L19 negative assertions to expect Shift+Shift_L/R bindings to EXIST (positive assertions). Add new runtime behavior test for Shift+Shift_L -> keycode=Shift_L, modifier=Shift event matches has_menu binding \|` | `\| test/TestDefaultHotkeys/TestDefaultHotkeys.cpp \| Update L19 negative assertions to expect Shift+Shift_L/R bindings to EXIST (positive assertions). Add new runtime behavior test for Shift+Shift_L -> keycode=Shift_L, modifier=Shift event matches has_menu binding \|` |
| 95 | `\| <0D>elease/fluxing-0.18.8.0-installer.exe \| New artifact (output of xbuild.bat installer) \|` | `\| release/fluxing-0.18.8.0-installer.exe \| New artifact (output of xbuild.bat installer) \|` |
| 137 | `- <07>ccept: Shift_L (no modifier in the binding) parses to` | `- accept: Shift_L (no modifier in the binding) parses to` |
| 140 | `- <07>ccept: Shift+Shift_L parses to {keycode=Shift_L, modifier=Shift}. Matches` | `- accept: Shift+Shift_L parses to {keycode=Shift_L, modifier=Shift}. Matches` |
| 148 | `  held). Our new binding <07>ccept: Shift+Shift_L requires modifier=Shift, so` | `  held). Our new binding accept: Shift+Shift_L requires modifier=Shift, so` |
| 236 | `- **R2**: <07>scii_composer.cc:75-98 still has a hardcoded "short-press Shift` | `- **R2**: ascii_composer.cc:75-98 still has a hardcoded "short-press Shift` |
| 239 | `  <08>indings_.find(key_code) == end() returning false. So single-press Shift_L` | `  bindings_.find(key_code) == end() returning false. So single-press Shift_L` |
| 243 | `  Shift+space toggle binding. Different <07>ccept field means different` | `  Shift+space toggle binding. Different accept field means different` |
| 254 | `  if they are on different <07>ccept events. They ARE different events here` | `  if they are on different accept events. They ARE different events here` |
| 270 | `   assertions for <07>ccept: Shift+Shift_L/R are flipped to positive).` | `   assertions for accept: Shift+Shift_L/R are flipped to positive).` |
| 304 | `- No Shift+Shift_L/R for <07>scii_mode toggle (already handled by` | `- No Shift+Shift_L/R for ascii_mode toggle (already handled by` |
| 305 | `  Shift+space + <07>scii_composer.switch_key.Shift_L/R: noop)` | `  Shift+space + ascii_composer.switch_key.Shift_L/R: noop)` |
| 310 | `- No new CI workflow changes (the existing <TAB>est: job in ci.yml will pick up` | `- No new CI workflow changes (the existing test: job in ci.yml will pick up` |
| 317 | `- <0D>elease/fluxing-0.18.8.0-installer.exe is a valid NSIS installer with` | `- release/fluxing-0.18.8.0-installer.exe is a valid NSIS installer with` |

### `.specify/specs/014-restore-shift-select-2nd-3rd-candidate/tasks.md` (15 line(s))

| line | damaged | restored |
|---|---|---|
| 53 | `- **File**: <TAB>est/TestDefaultHotkeys/TestDefaultHotkeys.cpp` | `- **File**: test/TestDefaultHotkeys/TestDefaultHotkeys.cpp` |
| 82 | `    adding <07>ccept: Shift_L (no modifier). The negative assertion` | `    adding accept: Shift_L (no modifier). The negative assertion` |
| 120 | `  - <TAB>est/TestShiftSelectBinding/TestShiftSelectBinding.cpp (new) - the` | `  - test/TestShiftSelectBinding/TestShiftSelectBinding.cpp (new) - the` |
| 122 | `  - <TAB>est/TestShiftSelectBinding/TestShiftSelectBinding.vcxproj (new) -` | `  - test/TestShiftSelectBinding/TestShiftSelectBinding.vcxproj (new) -` |
| 123 | `    based on <TAB>est/TestDefaultHotkeys/TestDefaultHotkeys.vcxproj, with` | `    based on test/TestDefaultHotkeys/TestDefaultHotkeys.vcxproj, with` |
| 125 | `  - <TAB>est/TestShiftSelectBinding/TestShiftSelectBinding.vcxproj.filters (new)` | `  - test/TestShiftSelectBinding/TestShiftSelectBinding.vcxproj.filters (new)` |
| 128 | `  - .gitignore - no change (existing patterns cover <TAB>est/**/Release/)` | `  - .gitignore - no change (existing patterns cover test/**/Release/)` |
| 133 | `    - Construct <0D>ime::KeyEvent from {keycode, modifier, release_mask}` | `    - Construct rime::KeyEvent from {keycode, modifier, release_mask}` |
| 134 | `    - Use <0D>ime::KeyEvent::Parse("Shift+Shift_L") and check` | `    - Use rime::KeyEvent::Parse("Shift+Shift_L") and check` |
| 137 | `    - Assert <TAB>sf_press == binding_event and !(tsf_release == binding_event)` | `    - Assert tsf_press == binding_event and !(tsf_release == binding_event)` |
| 175 | `  - <0D>elease/fluxing-0.18.8.0-installer.exe is the new file` | `  - release/fluxing-0.18.8.0-installer.exe is the new file` |
| 179 | `- **Action**: copy <0D>elease/fluxing-0.18.8.0-installer.exe to a temp` | `- **Action**: copy release/fluxing-0.18.8.0-installer.exe to a temp` |
| 221 | `  - The spec 012 form <07>ccept: Shift+Shift_L (not bare Shift_L) is the` | `  - The spec 012 form accept: Shift+Shift_L (not bare Shift_L) is the` |
| 250 | `  - git tag shows <0B>0.18.8.0` | `  - git tag shows v0.18.8.0` |
| 255 | `- spec 005 v1.1 design.md sec 2.2 example still uses <07>ccept: shift+l` | `- spec 005 v1.1 design.md sec 2.2 example still uses accept: shift+l` |

### `.specify/specs/016-behavior-level-test-framework/spec.md` (5 line(s))

| line | damaged | restored |
|---|---|---|
| 56 | `  - <07>ccept: Shift+Shift_L (spec 014) matches a real KeyEvent with` | `  - accept: Shift+Shift_L (spec 014) matches a real KeyEvent with` |
| 58 | `  - <07>ccept: Control+1 matches a real KeyEvent with` | `  - accept: Control+1 matches a real KeyEvent with` |
| 60 | `  - <07>ccept: Shift+space matches a real KeyEvent with` | `  - accept: Shift+space matches a real KeyEvent with` |
| 63 | `    does NOT match the <07>ccept: Shift+Shift_L binding (the L18` | `    does NOT match the accept: Shift+Shift_L binding (the L18` |
| 95 | `2. For each binding, parse <07>ccept: and send: strings using` | `2. For each binding, parse accept: and send: strings using` |

### `.specify/specs/037-fluxing-components-v0/plan.md` (3 line(s))

| line | damaged | restored |
|---|---|---|
| 17 | `- **测试**：<TAB>est/TestFluxingComponents/{TestFluxingButton.cpp, TestFluxingToggle.cpp, TestFluxingPanel.cpp, TestFluxingTheme.cpp, stdafx.h, stdafx.cpp, targetver.h, TestFluxingComponents.vcxproj}。` | `- **测试**：test/TestFluxingComponents/{TestFluxingButton.cpp, TestFluxingToggle.cpp, TestFluxingPanel.cpp, TestFluxingTheme.cpp, stdafx.h, stdafx.cpp, targetver.h, TestFluxingComponents.vcxproj}。` |
| 43 | `namespace <0C>luxing::ui，class 前缀 Fluxing。` | `namespace fluxing::ui，class 前缀 Fluxing。` |
| 88 | `- 颜色从 FluxingTheme::Instance().CurrentPalette() 拿（primary 用 hilited_back，secondary 用 <08>ack，destructive 用 0xD04545）。` | `- 颜色从 FluxingTheme::Instance().CurrentPalette() 拿（primary 用 hilited_back，secondary 用 back，destructive 用 0xD04545）。` |

### `.specify/specs/037-fluxing-components-v0/spec.md` (3 line(s))

| line | damaged | restored |
|---|---|---|
| 57 | `- [ ] 4.2 新建 WeaselUI/FluxingComponents/FluxingTheme.h（namespace <0C>luxing::ui，class FluxingTheme 单例 + Subscribe(Callback) / CurrentPalette()）。` | `- [ ] 4.2 新建 WeaselUI/FluxingComponents/FluxingTheme.h（namespace fluxing::ui，class FluxingTheme 单例 + Subscribe(Callback) / CurrentPalette()）。` |
| 59 | `- [ ] 4.4 新建 WeaselUI/FluxingComponents/Button.h（class FluxingButton 静态工厂 Create(hwnd, rect, label, style) + <0B>oid Destroy()）。` | `- [ ] 4.4 新建 WeaselUI/FluxingComponents/Button.h（class FluxingButton 静态工厂 Create(hwnd, rect, label, style) + void Destroy()）。` |
| 68 | `- [ ] 4.13 新建 <TAB>est/TestFluxingComponents/ 测试项目（4 个独立 .cpp + vcxproj + stdafx）。` | `- [ ] 4.13 新建 test/TestFluxingComponents/ 测试项目（4 个独立 .cpp + vcxproj + stdafx）。` |

### `test/TestShiftSelectBinding/TestShiftSelectBinding.cpp` (1 line(s))

| line | damaged | restored |
|---|---|---|
| 12 | `// The binding form <07>ccept: Shift+Shift_L (NOT bare Shift_L) is the spec` | `// The binding form accept: Shift+Shift_L (NOT bare Shift_L) is the spec` |

## TAB class damage (the second pass)

`0x09` is a C0 control character, but it is a legitimate
indentation character, so the first pass of the scanner excluded it
and a `\t` escape that was un-escaped into a real TAB left a line
that looked like ordinary text to a byte scan.  The rows below were
missed by the first pass; four of them are the same rows the first
pass already listed for `014/.../plan.md` (lines 31, 33, 35, 37),
written there with a real TAB instead of a `<TAB>` marker.

### `.specify/specs/014-restore-shift-select-2nd-3rd-candidate/plan.md` (1 line(s))

| line | damaged | restored |
|---|---|---|
| 15 | `<TAB>est/TestShiftSelectBinding/ that pins down the parse + match invariants.` | `test/TestShiftSelectBinding/ that pins down the parse + match invariants.` |

### `.specify/memory/lessons-learned.md` (5 line(s))

| line | damaged | restored |
|---|---|---|
| 1283 | `2. <TAB>est/TestDefaultHotkeys/TestDefaultHotkeys.cpp: L19 的 4 个负断言翻转为正断言 (从 "应不存在" 变 "应存在"); 新增 4 个正断言覆盖 spec 014 新增 binding 与 ascii_composer.switch_key 保持 noop。` | `2. test/TestDefaultHotkeys/TestDefaultHotkeys.cpp: L19 的 4 个负断言翻转为正断言 (从 "应不存在" 变 "应存在"); 新增 4 个正断言覆盖 spec 014 新增 binding 与 ascii_composer.switch_key 保持 noop。` |
| 1285 | `3. <TAB>est/TestShiftSelectBinding/ (新): 第二个 runtime yaml 契约测试, 13 个新正断言, 验证:` | `3. test/TestShiftSelectBinding/ (新): 第二个 runtime yaml 契约测试, 13 个新正断言, 验证:` |
| 9216 | `   强制走同一检查**。建议把检查抽到 <TAB>ools/build_verify.lua, 在 WeaselServer、` | `   强制走同一检查**。建议把检查抽到 tools/build_verify.lua, 在 WeaselServer、` |
| 9223 | `   脚本, 检测 WeaselServer.exe < 1 MB / 无 MZ / 全零时, 自动 <TAB>askkill + 卸装` | `   脚本, 检测 WeaselServer.exe < 1 MB / 无 MZ / 全零时, 自动 taskkill + 卸装` |
| 9269 | `- Դ issue: <TAB>ask.md Phase K3 T019 catastrophic regression` | `- Դ issue: task.md Phase K3 T019 catastrophic regression` |

### `.specify/specs/015-fix-test-infrastructure/tasks.md` (1 line(s))

| line | damaged | restored |
|---|---|---|
| 7 | `  located between <TAB>est_4(); and return boost::report_errors(); in` | `  located between test_4(); and return boost::report_errors(); in` |

### `.specify/specs/037-fluxing-components-v0/tasks.md` (4 line(s))

| line | damaged | restored |
|---|---|---|
| 34 | `- [ ] T016 [P1] 新建 <TAB>est/TestFluxingComponents/{TestFluxingButton.cpp, TestFluxingToggle.cpp, TestFluxingPanel.cpp, TestFluxingTheme.cpp, stdafx.h, stdafx.cpp, targetver.h}。` | `- [ ] T016 [P1] 新建 test/TestFluxingComponents/{TestFluxingButton.cpp, TestFluxingToggle.cpp, TestFluxingPanel.cpp, TestFluxingTheme.cpp, stdafx.h, stdafx.cpp, targetver.h}。` |
| 35 | `- [ ] T017 [P1] 新建 <TAB>est/TestFluxingComponents/TestFluxingComponents.vcxproj（mirror TestDarkModeBroadcast vcxproj 结构，4 个 ClCompile + 1 link + OutDir 用 L31 修复后的 SolutionDir 反斜杠模式）。` | `- [ ] T017 [P1] 新建 test/TestFluxingComponents/TestFluxingComponents.vcxproj（mirror TestDarkModeBroadcast vcxproj 结构，4 个 ClCompile + 1 link + OutDir 用 L31 修复后的 SolutionDir 反斜杠模式）。` |
| 42 | `- [ ] T021 [P1] <TAB>est\TestDefaultHotkeys\Release\TestDefaultHotkeys.exe → 35/35 PASS (回归)。` | `- [ ] T021 [P1] test\TestDefaultHotkeys\Release\TestDefaultHotkeys.exe → 35/35 PASS (回归)。` |
| 43 | `- [ ] T022 [P1] <TAB>est\TestDarkModeBroadcast\Release\TestDarkModeBroadcast.exe → 14/14 PASS (回归)。` | `- [ ] T022 [P1] test\TestDarkModeBroadcast\Release\TestDarkModeBroadcast.exe → 14/14 PASS (回归)。` |

### `CHANGELOG.md` (4 line(s))

| line | damaged | restored |
|---|---|---|
| 3141 | `  8. <TAB>est/{TestFluxingComponents,TestQuickPanelDialog,TestQuickPanelRefactor}/targetver.h - 同步升 WIN10.` | `  8. test/{TestFluxingComponents,TestQuickPanelDialog,TestQuickPanelRefactor}/targetver.h - 同步升 WIN10.` |
| 4234 | `  - 新增 <TAB>ools\win-shims\include\ 仓库级 shim 头文件（X11/keysym.h、utf8.h、darts.h），通过 <TAB>ools\win-shims\setup-shims.ps1 复制到 librime/include/。` | `  - 新增 tools\win-shims\include\ 仓库级 shim 头文件（X11/keysym.h、utf8.h、darts.h），通过 tools\win-shims\setup-shims.ps1 复制到 librime/include/。` |
| 5741 | `  - <TAB>est/TestFluxingComponents/{TestFluxingButton,TestFluxingToggle,TestFluxingPanel,TestFluxingTheme}.cpp + stdafx.h + stdafx.cpp + targetver.h` | `  - test/TestFluxingComponents/{TestFluxingButton,TestFluxingToggle,TestFluxingPanel,TestFluxingTheme}.cpp + stdafx.h + stdafx.cpp + targetver.h` |
| 5742 | `  - <TAB>est/TestFluxingComponents/TestFluxingComponents.vcxproj` | `  - test/TestFluxingComponents/TestFluxingComponents.vcxproj` |

