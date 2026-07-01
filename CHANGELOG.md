﻿## [0.18.6.0-fluxing] - 2026-07-01

### L19: defensive remove of all keycode=Shift_L/R bindings (spec 005 v1.1)

- **Problem**: 0.18.5.0 用户实测反馈 `shift+Enter` / `shift+<letter>` release
  event 仍触发 ascii_mode 切换。L18 修复（commit `e4095f2`）只移除了
  `always: Shift+Shift_L/R toggle ascii_mode`，但保留了
  `has_menu: Shift+Shift_L/R send 2/3` 的 binding。L18 字符串断言
  25/25 PASS 不能证明运行时 binding 表行为；0.18.6 安装包未 build（L18
  修复未真实安装验证）。

- **Fix (L19)**: 防御性原则——`output/data/default.yaml` 中
  `keycode=Shift_L/R` 的所有 binding **全部不存在**（含 `always` 与
  `has_menu` 两条路径）。

  - 候选选择改用 RIME 社区默认键位 `Control+1/2/3..9`
    （keycode=`1`/`2`/`3`..`9` 与 `Shift_L/R` release event 不重叠）
  - 切中英保留 `Shift+space`（keycode=`space`）
  - ascii_composer `switch_key.Shift_L/R: noop` 保留（让 key_binder 接管）

- **Test coverage**: `test/TestDefaultHotkeys/TestDefaultHotkeys.cpp` 从
  25/25 升级到 **31/31 PASS**，新增 6 个 L19 负断言（覆盖所有
  `Shift_L/R` 形态的 binding）+ 1 个 L19 正断言（active ascii_mode
  toggle 路径数 == 1，仅 `Shift+space`）。

- **Unverified**: 真实安装包行为（librime submodule 污染，0.18.6 build
  状态待 release 验收）。用户重装 0.18.6 后必须手动验证：
  - `shift+Enter` / `shift+<letter>` 不再切中英
  - `Shift+space` 仍能切中英
  - 候选窗打开时 `Control+1` / `Control+2` 选第 2/3 候选

- **Refs**: L18（被 L19 替代）、L16、spec 005 v1.1 plan.md §2.2。

## [0.18.7.0-fluxing] - 2026-07-01

### CI: TestDefaultHotkeys vcxproj + sln registration + ci.yml test job (TDD.md sec 6.1 P1)

- **Problem**: TDD.md sec 6.1 P1 + R-008 (PRD.md) - `TestDefaultHotkeys` had
  a working test exe (manually compiled with `cl /EHsc /std:c++17`, 31/31 PASS)
  but no vcxproj, no sln entry, no CI hook. `TestResponseParser` and
  `TestWeaselIPC` had vcxproj + sln entries but their `Release|Win32`
  configuration had no `Build.0` directive, so `msbuild weasel.sln
  /p:Configuration=Release /p:Platform=Win32` never produced the test exes.
  The CI workflow (`.github/workflows/ci.yml`) had no `test:` job, so unit
  tests were a manual `cl` step documented in TDD.md sec 6.1 only.

- **Fix (commit `e8db8c1`)**: full CI infrastructure for the 3 test projects.

  - `test/TestDefaultHotkeys/TestDefaultHotkeys.vcxproj` (new, 18960 bytes):
    MSBuild project based on `TestResponseParser` template, 8 platform
    matrix (Debug|Release x ARM|ARM64|Win32|x64), `stdcpp17`, no
    `stdafx.h` / no `ProjectReference` (independent console app).
  - `test/TestDefaultHotkeys/TestDefaultHotkeys.vcxproj.filters` (new):
    Solution Explorer filter file.
  - `weasel.sln`: 1 `Project()` block + 12 `ProjectConfigurationPlatforms`
    entries (Debug+Release, Build.0 set on Win32+x64 to actually compile;
    ARM/ARM64 fallback to Win32/x64 like `WeaselDeployer`).
  - `.github/workflows/ci.yml`: new `test:` job with `needs: build`,
    `runs-on: windows-2022`. Steps: checkout + submodules, env.bat
    bootstrap, Boost cache + install, msvc-dev-cmd, Build test projects
    (msbuild weasel.sln /t:TestDefaultHotkeys;TestResponseParser;TestWeaselIPC),
    Run unit tests. All 3 test exes run; failure of any one throws and
    fails the job.
  - `.gitignore`: 4 new patterns to keep `release/Test*.exe`,
    `release/Test*.pdb`, `test/**/Release/`, `test/**/Debug/` out of
    future commits. Verified that `release/fluxing-*-installer.exe` is
    NOT matched (those binaries are git-tracked per AGENTS.md sec 4.7).

- **Test coverage**: `TestDefaultHotkeys` 31/31 PASS (post-vcxproj rebuild).
  `TestResponseParser` and `TestWeaselIPC` now reach Release|Win32 in
  CI for the first time; their pre-existing vcxproj files compile
  cleanly under the new sln entries.

- **Verified locally**:
  - `msbuild weasel.sln /t:TestDefaultHotkeys /p:Configuration=Release /p:Platform=Win32`
    -> `Release\TestDefaultHotkeys.exe` 31/31 PASS.
  - `python yaml.safe_load(ci.yml)` -> "YAML valid".
  - `git check-ignore` confirms new patterns match test exes/pdbs and
    installer binaries are not matched.

- **Unverified**: full CI run on `github.com/rime/weasel` (this is a
  brand-fork PR candidate; CI release workflow guarded by
  `github.repository == 'rime/weasel'`, so Fluxing fork must be merged
  upstream to exercise the test job on real GitHub Actions). Real Windows
  runtime behavior unchanged from 0.18.6.0 - this is a CI infra release
  with zero user-facing change.

- **Refs**: TDD.md sec 6.1 P1, TDD.md sec 6.3, PRD.md R-008, L19
  (`e2c36b1`, parent feature), spec 005 v1.1 plan.md sec 2.2.## [0.18.5.0-fluxing] - 2026-06-30

### Installer hardening: smoke-test path guard + TSF shim lock-skip

- **L13-fix-2**: install-side guard against smoke-test paths left behind in
  the registry. When a previous silent-install smoke test (AGENTS.md §2.5)
  leaves `HKLM\Software\Fluxing\Weasel\InstallDir` pointing under
  `C:\TEMP\` or `C:\Users\test\`, the new install would inherit that path.
  The guard detects the smoke-test prefix (most-specific first:
  `C:\Users\test\` (13 chars), `C:\TEMP\test\` (12 chars), `C:\TEMP\` (8 chars))
  and falls through to the default install path `$PROGRAMFILES64\fluxing`.

  The matching uninstall-side fix (clearing `HKLM\Software\Fluxing\Weasel\InstallDir`
  on uninstall) is tracked separately as spec 012 C1; it is the actual root
  cause. L13-fix-2 is the defense-in-depth band-aid until spec 012 C1 lands.

- **L14-fix (TSF shim lock-skip)**: on x64 Windows, `weaselx64.dll` is the
  64-bit TSF TextInputProcessor. Once Windows TSF has loaded it (per user
  login session), it holds an open file handle for the entire session.
  NSIS cannot overwrite a locked file; the user-facing dialog
  "Cannot open the file for writing" + Abort/Retry/Ignore appeared on every
  upgrade.

  Workaround: wrap the `weaselx64.dll` `File` call in `IfFileExists` +
  `SetOverwrite try`. If the file does not exist (fresh install), copy
  normally. If it exists and is locked by TSF, the File call sets the
  error flag silently; we keep the old shim and surface a single
  `DetailPrint` line. The new shim is picked up at the next user
  log-out -> log-in cycle.

  Note: the original draft comment claimed a "size-equality check"; the
  actual logic is overwrite-try + skip-on-error. The comment has been
  corrected to match the code.

- **L17 lessons-learned**: 3 NSIS gotchas surfaced while building + testing:
  - `StrCpy $R1 $R0 N` copies the first N characters; if N does not
    equal the literal length in the subsequent `StrCmp`, the prefix
    check silently never matches (off-by-one).
  - `InstallDirRegKey` directive pre-loads `$INSTDIR` from the registry
    BEFORE `.onInit` runs. Any "is `$INSTDIR` empty?" check in `.onInit`
    is always false. The override must be an explicit assignment.
  - `/D=path` on the silent-install command line is broken in this
    installer (`InstallDirRegKey` overrides it). Tracked separately,
    not in this release.

### Verified on real hardware

- L13-fix-2 functional test: seeded
  `HKLM\SOFTWARE\WOW6432Node\Fluxing\Weasel\InstallDir = C:\TEMP\smoke-OLD\fluxing`,
  ran silent install, observed final `InstallDir = C:\Program Files\fluxing`
  (default, NOT the seeded smoke-test value).
- Silent install to `C:\TEMP\fluxing-test\ProgramFiles\fluxing\` (L13 path-force
  layout) produced all expected binaries with correct x86 arch (per L14).
- `weaselx64.dll` is the only x64 file; `SetOverwrite try` path verified
  by code review (`weaselx64.dll` is not currently locked on this dev box;
  the locked-file branch can only be tested when a user is actively
  logged in and TSF has loaded the shim).
- `TestDefaultHotkeys.exe`: 24/24 PASS (no spec 012 hotkey regression).

### Tracked follow-ups (not in 0.18.5.0)

- spec 012 C1: uninstall-side cleanup of `HKLM\Software\Fluxing\Weasel\InstallDir`
  (root cause of why L13-fix-2 was needed).
- spec 013 (proposed): fix `/D=path` silent-install override. Currently
  `/D=` is ignored because `InstallDirRegKey` pre-loads `$INSTDIR` and
  the `.onInit` logic cannot detect the /D= intent.
- spec 005 follow-up: `weasel.yaml` / schema files unrelated to installer.

## [0.18.4.0-fluxing-shift] - 2026-06-30

### Key bindings: Shift_L/Shift_R actually toggle and select candidates


- Fix: Shift_L / Shift_R single key actually works for switching CN/EN and selecting 2nd/3rd candidate.
  Previous key_binder used Shift_L / Shift_R (no modifier) which never matched
  the events TSF actually sends (keycode=Shift_L, modifier=SHIFT_MASK).
  librime key_event comparison is strict equality on (keycode, modifier), so
  plain `Shift_L` (modifier=0) never matches {Shift_L, SHIFT_MASK}.
  Previous key_binder also had `Shift+l` / `Shift+r` (lowercase modifier)
  which is silently dropped by librime key_table (modifier_name[] uses
  `Shift` not `shift`). Verified by reading librime/src/rime/key_table.cc.

  New binding (4 entries):
  ```
  - { when: has_menu, accept: Shift+Shift_L, send: 2 }   # 2nd candidate
  - { when: has_menu, accept: Shift+Shift_R, send: 3 }   # 3rd candidate
  - { when: always, toggle: ascii_mode, accept: Shift+Shift_L }  # toggle CN
  - { when: always, toggle: ascii_mode, accept: Shift+Shift_R }  # toggle CN
  ```
  `Shift+Shift_L` writes the modifier bit SHIFT_MASK + keycode Shift_L, which
  is exactly what TSF sends for a single Shift_L key press. Verified by reading
  WeaselTSF/KeyEventSink.cpp (result.mask |= SHIFT_MASK).
  L04 lessons-learned was wrong about lowercase modifier; corrected.
  L16 lessons-learned added documenting the SHIFT_MASK + Shift_L mechanics.

- Test: TestDefaultHotkeys.cpp updated to match new binding format.
  8 assert strings changed from `Shift_L` / `Shift+l` to `Shift+Shift_L` / `Shift+Shift_R`.
  4 new negative asserts added to confirm `Shift+l` / `Shift+r` is not present.
  24/24 PASS (was 20/20 before; +4 new negative asserts).

- Installer: SetOverwrite try -> on (fixes silent-install default.yaml not being updated).
  Previous behavior: NSIS skipped overwriting weasel\data\default.yaml if the
  existing file mtime was newer than the source. This caused the fix to not
  reach the installed system on top of 0.18.3 (default.yaml stayed at the
  pre-fix 16335 bytes; only when installing from scratch did the new 15948-byte
  default.yaml land).
  New behavior: NSIS always overwrites. default.yaml updates are now reliable
  on every install. This is the expected user contract for an IME update.

### Verified on real hardware

- Silent install to D:\Program Files\fluxing\ (L13 path-force layout, x86 binaries)
- weasel\data\default.yaml ends up at 15948 bytes (matches source), confirmed via SHA256
- WeaselServer restarts cleanly, no WeaselDeployer Scheme Switcher popup
- Shift_L single press in notepad toggles ascii_mode (verified by typing ni after toggle -> literal ni)
- Shift_L on candidate list selects 2nd candidate (verified with yi -> emoji 1 U+FE0F U+20E3)
- Shift_L after candidate commit goes back to Chinese (verified with hao -> 好)
## [0.18.4.0] - 2026-06-30

### ä¸»è¦æ´æ°

- **æ¶æä¸è´æ§ï¼å§ç»å®è£ Win32 äºè¿å¶** (ä¿®å¤ 0xC000007B STATUS_INVALID_IMAGE_FORMAT bug)
  - librime æ¯ Win32-onlyï¼`output\rime.dll` æ¯ x86ï¼~3MB å« lua æä»¶ï¼ãä½ `output\WeaselDeployer.exe` / `WeaselServer.exe` æ¯ x64ãæ x64 è¿ç¨å è½½ x86 rime.dll ä¼è§¦å WoW64 è¿ç¨çº§æ¶æå²çªï¼0xC000007Bã
  - ä¹å 0.17.5-0.18.3 ç installer æä¸ª `${If} ${RunningX64}` æ¡ä»¶åæ¯ï¼å¨ x64 Windows ä¸è£ x64 çæ¬ç `Weasel*.exe`ãè¿æ¡è·¯å¾äº§åºçå®è£åå¨ç®æ æºå¨ä¸ä¼ç«å» crashã
  - ä¿®å¤ï¼å æ x64 / Win32 æ¡ä»¶åæ¯ï¼**å§ç»**è£ `Win32\Weasel*.exe` + `Win32\rime.dll`ï¼å¨é¨ x86ï¼ãx64 OS éè¿ WoW64 å è½½ 32-bit EXEï¼rime.dll ä¹æ¯ 32-bitï¼æ¶æç»ä¸ã
  - **L14** lessons-learned è®°å½äºå®æ´æ ¹å  + ä¿®å¤ + æè®­ã

- **é»è®¤å®è£è·¯å¾ç®åä¸º `C:\Program Files\fluxing`**
  - ä¹å `.onInit` æ `${If} ${AtLeastWin11}` åµå¥ `${If} ${IsNativeARM64}` ç­ 4 è·¯åæ¯ï¼Win11+ARM64 / Win11+AMD64 / Win11+x86 / Win10+AMD64 / Win10+x86ï¼ï¼é»è¾å¤æä¸å¨ç¨äº `$PROGRAMFILES64`ï¼x86 installer å¨ x64 Windows ä¸ä¼è¢« WOW64 éå®åå° `C:\Program Files (x86)`ï¼å¯¼è´å¥æªè·¯å¾ï¼ã
  - ç®åä¸ºåè¡ `StrCpy $INSTDIR "$PROGRAMFILES64\fluxing"`ã
  - åæ¶ä¿®äº `ForceFluxingSuffix` ç logic bugï¼0.18.3.0 çæ¬é `StrCmp` é¾è·¯ fall-throughï¼å¯¼è´ææè·¯å¾é½è¿ `not_fluxing` åæ¯ï¼å¼ºå¶è¿½å  `\fluxing`ï¼åºç° `fluxing\fluxing` åéåç¼ï¼ã
  - åæ¶ä¿®äº upgrade è·¯å¾é»è¾ï¼ä¹å `.onInit` ç `ReadRegStr $R0 ...; StrCmp $R0 "" 0 skip` è¯»äºæ³¨åè¡¨ä½æ²¡ç¨å®ï¼$INSTDIR å§ç»æ¯ /D= å¼æé»è®¤å¼ãç°å¨å äº `StrCpy $INSTDIR $R0` çæ­£ä½¿ç¨æ³¨åè¡¨å¼ï¼å®ç°"åå°åçº§"ï¼ç¨æ·é¦æ¬¡è£å¨ D:\foo\fluxingï¼ååçº§æ¶ä»ç¨ D:\foo\fluxingï¼é¤éæ¾å¼ä¼  /D= è¦çï¼ã

- **å®è£æ¥å¿**
  - NSIS åçæ¯æ `/LOG=path` CLI flagï¼æ éå¨èæ¬é `LogSet`ï¼`LogSet` å¨æ å NSIS ä¸å¯ç¨ï¼éè¦èªå®ä¹ç¼è¯ï¼ã
  - å¨ `.onInit` å äºææ¡£è¯´æï¼æç¤ºç¨æ· / é¨ç½²èæ¬ä¼  `/LOG=path\to\file.log` æ¥è·å¾å®æ´å®è£æ¥å¿ï¼ä½ä¸º post-mortem å·¥å·ã
  - **æ³¨æ**ï¼ä½¿ç¨ `/LOG=` æ¶**å¿é¡»**ç¨ cmd /c è°ç¨ï¼å¦å PowerShell `Start-Process` ä¼æ `/LOG=` åå¹¶å° `/D=` éï¼NSIS è¡ä¸ºï¼unknown CLI args ä¸²å¥ $INSTDIRï¼ã

### éªæ¶

- å¨æ° silent installï¼`/S /D=C:\TEMP\fluxing-0184-test`ï¼å¾å° `fluxing-0184-test\ProgramFiles\fluxing\weasel\` å¸å±ã
- ææ `WeaselServer.exe` / `WeaselDeployer.exe` / `WeaselSetup.exe` / `uninstall.exe` / `rime.dll` / `weasel.dll` / `WinSparkle.dll` é½æ¯ x86ã
- `weaselx64.dll` æ¯ x64ï¼TSF text input processor å¿é¡»æ¯ x64ï¼ã
- `rime.dll` çº¦ 3MBï¼lua plugin å·²é¾æ¥ï¼ã
- æ³¨åè¡¨ `HKLM\SOFTWARE\Fluxing\Weasel\InstallDir` = å®è£æ ¹ï¼å« `\fluxing`ï¼ã
- æ³¨åè¡¨ `HKCU\Software\Fluxing\Weasel\RimeUserDir` = `<install-root>\user1\fluxing`ã
- AGENTS.md Â§2.5 å¼ºå¶ silent-install smoke test éè¿ã

## [0.18.3.0] - 2026-06-30

### ä¸»è¦æ´æ°

- **å®è£è·¯å¾å¼ºå¶ä»¥ `fluxing` ç»å°¾** (ä¿®å¤ 0.18.2.0 è·¯å¾å¸å± bug)
  - `output/install.nsi` ç `IsFluxingPath` å½æ°ä¹åæ broken Exch/Pop é¾ï¼å¯¼è´è¿åå¼æä¸ºåå¾ï¼`ForceFluxingSuffix` å®éä»æªçæã
  - æ§çæ¬ä¸ï¼ç¨æ·å¨ GUI é `D:\Program Files` æ¶ï¼å®è£å¨æå¼æäºè¿å¶åå° `D:\Program Files\weasel\`ãæç¨æ·æ°æ®åå° `D:\Program Files\fluxing\user1\fluxing\` ââ å¼æä¸ç¨æ·æ°æ®"åè£"å¨ä¸¤å¤ã
  - ä¿®å¤åï¼
    - éå `ForceFluxingSuffix` ä¸ºåºäº label + `StrCmp` çç®åé»è¾ï¼ä¸åä¾èµ Exch stack jugglingï¼ã
    - å¨ `.onInit` ç `skip:` æ ç­¾ä¹åæ¾å¼ `Call ForceFluxingSuffix` ââ ä¹åä»é  `MUI_PAGE_CUSTOMFUNCTION_LEAVE` è§¦åï¼**silent install (`/S` + `/D=`) ä¸æ ¹æ¬ä¸ä¼æ§è¡**ã
    - `output/install.nsi` L534-535 ç user-data è·¯å¾ç± `$R3\fluxing\user1\fluxing` ä¿®æ­£ä¸º `$R3\user1\fluxing`ï¼é¿åä¸ `$R3` æ«å°¾ç `\fluxing` éå¤æ `\fluxing\fluxing`ï¼ã
  - éªè¯ï¼`xbuild.bat installer` + silent install `/D=C:\TEMP\fluxing-0183-test` â äº§ç
    ```
    fluxing-0183-test\
      ââ fluxing\              â å®è£æ ¹
           ââ weasel\          â å¼æäºè¿å¶
           ââ user1\           â ç¨æ·æ°æ®
                ââ fluxing\
    ```
    æ³¨åè¡¨ `HKLM\...\InstallDir` ä¸ `HKCU\...\RimeUserDir` åæ­¥æ´æ°ã
  - **L13** lessons-learned è®°å½äºæ ¹å  + ä¿®å¤å¨è¿ç¨ + æè®­ã

### éªæ¶

- å¨æ°å®è£ï¼GUI ä¸ silent (`/S` + `/D=`) é½å¾å° `<chosen>\fluxing\weasel\` å¸å±ã
- æ³¨åè¡¨ `HKLM\SOFTWARE\Fluxing\Weasel\InstallDir` = å®è£æ ¹ï¼å« `\fluxing`ï¼ã
- æ³¨åè¡¨ `HKCU\Software\Fluxing\Weasel\RimeUserDir` = `<install-root>\user1\fluxing`ã

## [Unreleased] - 2026-06-28

### ä¸»è¦æ´æ°

- åçéå½åä¸º"ç«æµç©è¾å¥æ³ / Fluxing"
  - ç¨æ·å¯è§å­ç¬¦ä¸²ãå®è£å¨åç§°ãæ¡é¢/å¼å§èåå¿«æ·æ¹å¼ã"ç¨åºååè½"å¸è½½é¡¹åç§°ç­å·²ç»ä¸ä¸ºæ°åçåã
  - ä¸æ¸¸ RIME / ä¸­å·é»è¾å¥æ³å¼æä¸å¼åèå½å±ä¿¡æ¯ä¿æä¸åã
  - å³èï¼åé¨å¸¸éä¸å¾æ éå½åè§ 9f2b217ï¼Fluxing åæé¦æäº¤ï¼ã
<a name="0.17.4"></a>

- ç«¯å°ç«¯ Windows æå»ºæµæ°´çº¿ï¼spec 003ï¼
  - å¨ x64 ä¸ x86 (Win32) ä¸¤ç§æ¶æä¸å®æ librime 1.13.1 çæå»ºï¼glog/gtest/leveldb/marisa/opencc/yaml-cpp å­ä¸ªç¬¬ä¸æ¹ä¾èµ + rime å¼ææ¬èº«ï¼ã
  - éè¿ msbuild å¨ Release|x64 ä¸ Release|Win32 ä¸æå»º weasel.slnï¼WeaselTSF/WeaselUI/WeaselIPC/WeaselServer/WeaselDeployer/WeaselSetup/RimeWithWeaselï¼ï¼äº§ç©è½å° output\ ä¸ output\Win32\ã
  - éè¿ NSIS 3.x çæ output\archives\fluxing-0.17.4.0-installer.exeï¼çº¦ 10.5 MBï¼å¹¶å¤å¶å° elease\ã
  - æ°å¢ 	ools\win-shims\include\ ä»åºçº§ shim å¤´æä»¶ï¼X11/keysym.hãutf8.hãdarts.hï¼ï¼éè¿ 	ools\win-shims\setup-shims.ps1 å¤å¶å° librime/include/ã
  - ä¿®æ­£ output/install.nsiï¼
    - 5 å¤é»è®¤å®è£è·¯å¾ $PROGRAMFILES*\Rime æ¹ä¸º $PROGRAMFILES*\Fluxingï¼L135/137/139/144/146ï¼ï¼ä½¿é»è®¤å»ºè®®è·¯å¾å³ä»¥ \Fluxing ç»å°¾ã
    - ç§»é¤å¤ä½ç UTF-8 BOMï¼4Ã â 1Ãï¼ï¼NSIS 3.x ä¸åè§£æå¤±è´¥ã
  - è¯´æï¼æ¬æ¬¡æäº¤**ä¸**åå« librime å­æ¨¡ååç librime/build.bat è°è¯ echo ä¸ cmake å¼ç¨ä¿®å¤ï¼ä»ä¿çå¨å·¥ä½åºä¸º dirty ç¶æï¼ãåç»­è¥å»ºç« kizemo/librime æ´¾çä»åºï¼å¯å°è¿äºè¡¥ä¸åå¥å­æ¨¡åã

- ç¸å³ spec/plan/tasksï¼.specify/specs/003-windows-build-pipeline/{spec,plan,tasks}.mdã

## [0.17.4](https://github.com/rime/weasel/compare/0.17.3...0.17.4)(2025-06-04)

### ä¸»è¦æ´æ°
* ä¿®å¤#1585 æªå¤çå®æ´çç¨æ·ç®å½è·¯å¾æå¼å¤çé®é¢

#### Code Refactor
refactor(WeaselTSF): add error handling when try open RimeUserDir ([fxliang](https://github.com/rime/weasel/commit/7a52fce2e6f5a991c58f2a19c93e82d3cfa191d3))

#### Bug Fixes
fix(WeaselSetup): RimeUserDir in registry is empty when default location ([fxliang](https://github.com/rime/weasel/commit/1ffd4412005962e0f9dd88558f23ec37e982ce59))

<a name="0.17.3"></a>
## [0.17.3](https://github.com/rime/weasel/compare/0.17.0...0.17.3)(2025-05-24)

### ä¸»è¦æ´æ°
* ä¿®å¤æªèªå®ä¹è®¾å®ç¨æ·ç®å½æ½å¨å¯è½æ æ³å¨å³é®èåæå¼ç¨æ·ç®å½çé®é¢
* ä¿®å¤éè²æ¹æ¡ä¸­æªå®ä¹è²åééè¯¯é®é¢
* åé#1499ï¼ä¿®å¤ç±ä¹äº§ççinline_preeditå¤±æé®é¢

#### Bug Fixes
fix(WeaselTSF): explore user dir failed if it's not customized ([fxliang](https://github.com/rime/weasel/commit/facecbf2d29cb45ee695e5a27e68f76dd796264a))
fix(RimeWithWeasel): color parsing for decimal number fix(RimeWithWeasel): highlight label color and highlight comment color not correct when it's not defined ([fxliang](https://github.com/rime/weasel/commit/c52f260c603aa5b19b084317c22c978e61cbbaab))

#### Commits
Revert "fix(tsf): ime status (#1499)" ([å±ææ°](https://github.com/rime/weasel/commit/c72cbc8e002f88c2b24b6866e28260897e49d1fe))

<a name="0.17.0"></a>
## [0.17.0](https://github.com/rime/weasel/compare/0.16.3...0.17.0)(2025-05-17)

### ä¸»è¦æ´æ°
* æ´æ° librime è³ 1.13.1 çæ¬
* ä¿®å¾©æç¤åæ¨å¡æ­»åé¡
* ä¿®å¾©ç¶ç±éµè¨­ç½®çºç©ºæ WeaselDeployer å´©æ½°çåé¡
* ä¿®å¾©æ´æ°å®è£å¾å¯è½å°è´éåå¾ç¨å¼æªæ¡è¢«åªé¤çåé¡
* ä¿®å¾©å¤ç·ç¨å°è´çæåå´©æ½°åé¡
* ä¿®å¾©é¨åæç¨ç¨å¼ä¸­çç°å¸¸å´©æ½°åé¡
* ä¿®å¾©é¨åæç¨ä¸­ç¡æ³é¡¯ç¤ºè¼¸å¥æ³çåé¡
* ä¿®å¾©å é¡¯ç¤ºå¡éç½®å°è´çæå­ç¹ªè£½å¤±æåé¡
* ä¿®å¾©ãå¤©åå°æ¹ãçæä¸ç·¨ç¢¼é«äº®æªæ­£ç¢ºç¹ªè£½çåé¡
* ä¿®å¾© vim-mode ä¸æéµé¿æç°å¸¸åé¡
* ä¿®å¾©è¼¸å¥æ³é¡¯ç¤ºçæç°å¸¸åé¡
* ä¿®å¾©å¨è¢å¹æ¨¡å¼ä¸é«äº®èæ¯ç¹ªè£½é¯èª¤åé¡
* ä¿®æ­£æ··è²ç®æ³ï¼è§£æ±ºé¨åææ³ä¸çæ··è²ç°å¸¸åé¡
* `WeaselDeployer.exe` å `WeaselSetup.exe` æ°å¢ `/h` å `/help` åæ¸ï¼é¡¯ç¤ºä½¿ç¨èªªæ
* `WeaselSetup.exe` æ°å¢åæ¸æ¯æ´è¨­å®ç¨æ¶è³æç®éï¼ä¾å¦ï¼`WeaselSetup.exe /userdir:D:\rime_data_dir`

#### Code Refactor
refactor(WeaselUI): DirectWriteResources ([fxliang](https://github.com/rime/weasel/commit/16672f47cfcb75426459afa8d4ba3c7069eeb2d8))
refactor(WeaselTSF): simplify codes of RegisterCategories and UnregisterCategories ([fxliang](https://github.com/rime/weasel/commit/4b47310e95c76cfffb0c0828be9563dbb4125aeb))
refactor(WeaselTSF): simplify codes of RegisterProfiles and UnregisterProfiles ([fxliang](https://github.com/rime/weasel/commit/83881f07227ffec2f202847a6d2cbb28991f7fcc))
refactor(RimeWithWeasel): simplify configuration parsing ([fxliang](https://github.com/rime/weasel/commit/8125608f3f24ec16c1e2b78ee8ff8b0a2f5d1dbc))
refactor(WeaselDeployer): string convertions with macro ([fxliang](https://github.com/rime/weasel/commit/30e5adf80e2171fee40cebad71281c8551210e55))
refactor(RimeWithWeasel): simplify _LoadSchemaSpecificSettings ([fxliang](https://github.com/rime/weasel/commit/aba0609f64e5122748db80150de9644ffec0699f))
refactor(RimeWithWeasel): string convertions with macro ([fxliang](https://github.com/rime/weasel/commit/597993e8992e5081c9c0786c9b491c93fc3bbd71))

#### Features
feat: WeaselSetup.exe with new param /? or /help to show help info ([fxliang](https://github.com/rime/weasel/commit/63f27915f3dd03da2bc2b9d4ae1209f1b5e56e0b))
feat: WeaselDeployer.exe with new param /? or /help to show help info ([fxliang](https://github.com/rime/weasel/commit/1004f399d4b5c90652ae63f33f40247adc56e91b))
feat: WeaselSetup.exe parameter /userdir:<user_data_dir_full_path> to set user data directory in command line ([fxliang](https://github.com/rime/weasel/commit/0ef3154e489eed1176e9ca3a2e5f244fc0c1cf0f))
feat: WeaselSetup é»è®¤å¯å¨ä¸è¯·æ±ç®¡çåæéï¼å¿è¦æ¶ä½¿ç¨ç®¡çåæééå¯ (#1390) ([Wendy](https://github.com/rime/weasel/commit/ba768a6d65895837b052a1d366ffb872df5f0091))

#### Chores
chore: update bump version scripts ([fxliang](https://github.com/rime/weasel/commit/967674ff5295c4a389b35e9f8070b9fe43d0dcb1))
chore: update update/bump-version.ps1 [skip ci] ([fxliang](https://github.com/rime/weasel/commit/d13fd1250545d05df6573be7c0dee2529a4dc3fc))
chore: update update/bump_version.sh [skip ci] ([fxliang](https://github.com/rime/weasel/commit/8d12cafec0a84498c3f32d6821b7ccbd85fe1f21))
chore: follow #1379, update `update/bump-version.sh` to work without clog[skip ci] ([fxliang](https://github.com/rime/weasel/commit/d75b34bce20026f54ebbae73b6c591b3db473d11))
chore: make clang-format.ps1 worked in linux/Mac OS[skip ci] ([fxliang](https://github.com/rime/weasel/commit/d3e872c6671aaab5bfc0a6caa8227b416a5c5601))
chore: update update/bump_version.ps1 ([fxliang](https://github.com/rime/weasel/commit/18cb65206c494e5b3bc21380dc938d5553a7e83a))
chore: add powershell script for linting ([fxliang](https://github.com/rime/weasel/commit/a6d15cea4ad14c921bbde4c6b9c99a8a15c4dcde))
chore: update .gitignore ([fxliang](https://github.com/rime/weasel/commit/094e99de9e47b0aa9469b3d94a03e78501d8b1bb))
chore(install_boost): update boost download url ([å±ææ°](https://github.com/rime/weasel/commit/235308dc7425529b49ffe3a5eb29947a4657f8cd))

#### Builds
build: bump librime to 1.13.0 ([fxliang](https://github.com/rime/weasel/commit/9a5244b1fae8de8f52c2f774eddc2986d869e98a))
build: set /utf-8 for source compilation ([fxliang](https://github.com/rime/weasel/commit/acbb0c393c65ebfea2ac176723f08e5b121442aa))
build: IntDir and OutDir set for msbuild solution, intermediary files will be always in `$(SolutionDir)\msbuild`. ([fxliang](https://github.com/rime/weasel/commit/5d5d5b0338a5eead4d898a2589f280015acdae85))

#### Continuous Integration
ci: run update rime/home appcast on published or prereleased ([fxliang](https://github.com/rime/weasel/commit/41dc044d7c34d30a574a72095361abf345c133f5))
ci: bump librime 1.13.1 ([fxliang](https://github.com/rime/weasel/commit/d279d9d78cce33a4e3f36e29c0a1ef6bef423121))
ci: draft before release ([fxliang](https://github.com/rime/weasel/commit/57b4cc44b46a1e9050b154178885cfccd1e9fdbe))

#### Bug Fixes
fix(trayicon): explorer.exe hangs ([fxliang](https://github.com/rime/weasel/commit/f11831fb16446ab98c1a2fee9ec6245a0d24144b))
fix(WeaselUI): hemispherical of hilite text preedit not correct ([fxliang](https://github.com/rime/weasel/commit/6e884c299b28c4a8e2d5ed2dc1845e702da35868))
fix(WeaselDeployer): WeaselDeployer will dump if hotkeys is set empty #1549 ([fxliang](https://github.com/rime/weasel/commit/bf4853dde1a4476489c4d7e85ab9407e5fc7c5f7))
fix(RimeWithWeasel): avoid vim_mode misoperations (#1543) ([fxliang](https://github.com/rime/weasel/commit/c2beb41a63567de7b9399ede13db65f0d3254221))
fix(installer): avoid files are deleted on system reboot after reinstallation (#1520) ([fxliang](https://github.com/rime/weasel/commit/2f92c6c5b885caf633f61d47e21ae61a93658246))
fix(tsf): ime status (#1499) ([wzv5](https://github.com/rime/weasel/commit/ea49aa13e936cf2309854ee353f18c2442153643))
fix(CandidateList): not displaying in some applications (#1494) ([wzv5](https://github.com/rime/weasel/commit/35afa144056e26e26f1c5eb092fd77942174cb35))
fix(ipcserver): concurrent access to rime api ([å±ææ°](https://github.com/rime/weasel/commit/2dc4e1923a95d8ad4c1eff19399614253507c0fe))
fix(RimeWithWeasel): blend_colors algorithm, fix issue like #1405 ([fxliang](https://github.com/rime/weasel/commit/5dbafbb893cd79c876dab8600833266ce12ecdbd))
fix(WeaselUI): highlight back is not drawn correctly when fullscreen layout set ([fxliang](https://github.com/rime/weasel/commit/8b95887f4e2375659ed228e921f481a462b97376))
fix(CandidateList): null pointer error ([å±ææ°](https://github.com/rime/weasel/commit/588a31f8eedf6066e5b6a1cc7f010ed528154445))
fix: silent installation script repeated call ([å±ææ°](https://github.com/rime/weasel/commit/150c5608ba5338cedf124a0c1adf2caf5948a6cf))
fix: silent installation script typo ([Yh793](https://github.com/rime/weasel/commit/c599f2e67ab26dac3f77066d4d57d9295092be96))
fix: fix unexpected crash in some applications (#1458) ([Alfred Lieu](https://github.com/rime/weasel/commit/3f1e05b255867b8d9d31212fe840bcfa8f23b50c))
fix: candidate ui can't be drawn correctly after GPU reset ([fxliang](https://github.com/rime/weasel/commit/37c8fa161a221a263bb439a1158ba401c0cf90a3))

#### Commits
remove duplicated branch ([Qijia Liu](https://github.com/rime/weasel/commit/78e20ab7893ffe07904ef10eebe55c27bb05cc2a))
refactorÃ¯(RimeWithWeasel) simplify color parsing function ([fxliang](https://github.com/rime/weasel/commit/836dc9e35c25564fb4b8ab95e575afa4454ee5f3))

<a name="0.16.3"></a>
## [0.16.3](https://github.com/rime/weasel/compare/0.16.2...0.16.3)(2024-10-04)

#### Bug Fixes
* release channel feed_url not correct. ([fxliang](https://github.com/rime/weasel/commit/0c8bb0f01a929f46160482ae2f4492bed560b7b9))
* invalid quick return ([Xuesong Peng](https://github.com/rime/weasel/commit/4da263727e16362f01054f6f0bb7522e83ae1e06))

#### Chores
* add update\bump-version.ps1 to bump version in powershell, when clog is not required ([fxliang](https://github.com/rime/weasel/commit/8770fb3ed1b4341b7875c1d60e98bfa5b42f8ac7))
* update bump-version.sh, appcast.xml and testing-appcast.xml[skip ci] ([fxliang](https://github.com/rime/weasel/commit/91d5e4e224a0d73b8303a6ce10f03c71dace5cdd))

#### Continuous Integration
* release and update testing appcast only in rime/weasel ([fxliang](https://github.com/rime/weasel/commit/4af83b6e17f7c3cf78257dd300f4adadbffa1083))

<a name="0.16.2"></a>
## [0.16.2](https://github.com/rime/weasel/compare/0.16.1...0.16.2) (2024-09-28)

#### å®è£é ç¥

**â ï¸å¦æ¨ç±0.16.0ä¹åççæ¬åç´ï¼ç±æ¼åæ¸è®åï¼å®è£å°ç¼æ¯«åè«ä¿å­å¥½æä»¶è³æï¼æ¼å®è£å¾éåæè¨»é· Windowsï¼å¦åæ­£å¨ä½¿ç¨å°ç¼æ¯«çæç¨å¯è½æå´©æ½°ã**

**â å¦æ¨ç±0.16.0ä¹åççæ¬åç´ï¼è«ç¢ºèªæ¨ç `installation.yaml` æä»¶ç·¨ç¢¼ç² `UTF-8`, å¦åå¦æ¨å¨å¶ä¸­ä¿®æ¹äºé ASCII å­ç¬¦å§å®¹çè·¯å¾æï¼æå¯è½æå¼èµ·æªæé¯èª¤ã**

#### ä¸»è¦æ´æ°
* æ°ç¹æ§ï¼æ¯æèªåæª¢æ¥æ´æ°ä½¿ç¨æ¸¬è©¦ééï¼ä½¿ç¨`WeaselSetup.exe`åæ¸å¯ä¿®æ¹ï¼`/testing`ä½¿ç¨æ¸¬è©¦ééï¼`/release`ä½¿ç¨ç¼ä½çæ¬ï¼é»èªå¾èï¼
* æ°ç¹æ§ï¼`WeaselSetup.exe`åæ¸è¨­ç½®çé¢èªè¨ï¼è®¾ç½®åè¦çåºåè®¾ç½®çèªå¨æ£æµã`/lt`è¨­ç½®ç²ç¹é«ä¸­æçé¢ï¼`/ls` è¨­ç½®ç²ç°¡é«ä¸­æçé¢ï¼`/le`è¨­ç½®ç²è±æçé¢
* æ°ç¹æ§ï¼`WeaselSetup.exe`åæ¸è¨­ç½®æ¯å¦ä½¿ç¨èªåæª¢æ¥æ´æ°ï¼`/du`ç¦ç¨èªåæª¢æ¥æ´æ°ï¼`/eu`ä½¿ç¨èªåæª¢æ¥æ´æ°
* æ°ç¹æ§ï¼å®è£å¨å½çªæç¤ºè¨­ç½®æ¯å¦èªåæª¢æ¥åç´
* æ°ç¹æ§ï¼ééIMEæ¶æ¯é¿æçæå¯éç½®ï¼`WeaselSetup.exe`åæ¸`/toggleime`è¨­ç½®éééµç¤ï¼åçæ¬çæï¼ï¼`/toggleascii`åæ`ascii_mode`,å®è£é»èªå¾è #1364
* æ°ç¹æ§ï¼æ¯æxmake 2.9.4ä»¥ä¸çæ¬æ§å»ºï¼ä½¿ç¨`xbuild.bat`éå±ï¼ç¸éåæ¸åºæ¬å`build.bat`, ä½¿ç¨`xbuild.bat commands`å¯çæ`compile_commands.json`ä¾¿æ¼lspä½¿ç¨ï¼`xbuild.bat clean`å¯æ¸ç©ºxmakeæ§å»º #1360
* æ°ç¹æ§ï¼æ¯æ`Caps_Lock` æéµbindingï¼å¦é¸éï¼,éå°`key_binder`ç½®äº`ascii_composer`ä¹å
* ä½¿è½TSF dllä¸­çWER
* nightly æ§å»ºå¾èªåæ´æ°rime/homeé é¢æ´æ°æ¸¬è©¦ééappcast
* åç´lintæª¢æ¥ä½¿ç¨çllvmæä½çæ¬è³18.1.6, æ´æ°cièæ¬æ£æ¥æ´æ°llvm

#### Bug ä¿®å¾©

* ä¿®å¾©å®è£å¨å¨ç³»çµ±æªæ»¿è¶³è¦æ±ææªä¸­æ·çåé¡
* ä¿®å¾©éæ°å®è£æèçå®æ¼è·¯å¾æªä¿æçåé¡
* ä¿®å¾©çé¢èªè¨æ ¹æååæ ¼å¼æªæ­£ç¢ºè¨­ç½®çåé¡
* ä¿®å¾©IPCéä¿¡æå æ°èçæ¬è®æ´å¼èµ·çç°å¸¸å´©æ½°çåé¡
* ä¿®æ­£ä»£ç¢¼ç·¨ç¢¼æ ¼å¼
* ä¿®å¾©æ¸ç©ºèlogæä»¶
* ä¿®å¾©æ§å¶é¢æ¿å¸è¼çé¢ä¸­çåæ¨é¡¯ç¤ºåé¡
* ä¿®å¾©`style/hover_type`ç²`"semi_hilite"`å¨é¦åé¸æçé¡¯ç¤ºç°å¸¸åé¡
* ä¿®å¾©æ°çlibrimeç¢ç©æªè½ç´æ¥æ¿æä½¿ç¨åé¡
* ç¦ç¨IPCéä¿¡çç°æ­¥æ©å¶ï¼ä¿®å¾©ä¸äºå ç°æ­¥æ©å¶å¼ç¼çæç¨ç°å¸¸
* ä¿®å¾©æ§å»ºè³æ¬ä¸è½éçææ­£ç¢ºççæ¬ä¿¡æ¯åé¡
* ä¿®å¾©ä¸äºvså·¥ç¨éç½®è¨­ç½®ï¼èçä¸äºdeprecated APIè­¦å


<a name="0.16.1"></a>
## [0.16.1](https://github.com/rime/weasel/compare/0.16.0...0.16.1) (2024-06-06)


#### å®è£é ç¥

**â ï¸å¦æ¨ç±0.16.0ä¹åççæ¬åç´ï¼ç±æ¼åæ¸è®åï¼å®è£å°ç¼æ¯«åè«ä¿å­å¥½æä»¶è³æï¼æ¼å®è£å¾éåæè¨»é· Windowsï¼å¦åæ­£å¨ä½¿ç¨å°ç¼æ¯«çæç¨å¯è½æå´©æ½°ã**

**â å¦æ¨ç±0.16.0ä¹åççæ¬åç´ï¼è«ç¢ºèªæ¨ç `installation.yaml` æä»¶ç·¨ç¢¼ç² `UTF-8`, å¦åå¦æ¨å¨å¶ä¸­ä¿®æ¹äºé ASCII å­ç¬¦å§å®¹çè·¯å¾æï¼æå¯è½æå¼èµ·æªæé¯èª¤ã**

#### ä¸»è¦æ´æ°
* ç²`WeaselServer.exe`ä½¿è½Windows Error Reporting, æä¾å°æç`WeaselServer.pdb`æä»¶, å¨`WeaselServer.exe`å´©æ½°æå¯ä»¥çædmpå ±åæä»¶å¨æ¥èªæä»¶å¤¾ä¸­
* æä¾`WeaselServer.exe`å®è­·ï¼å¨æåå´©æ½°å¾6åæéµäºä»¶ï¼ä¸æ¬¡æéµDown&Up)å¾æèµ·æå
* æ°å¢è±æçé¢èªè¨
* æ´æ°7zåcurlå°ææ°çæ¬ï¼ä¿®å¾©ä¸äºå ç²7zçbugå¼èµ·çåé¡
* åªåé è¦½åPNGæä»¶å¤§å°
* æ°å¢èªè¨æ¬èå®ï¼æéæ¥èªæä»¶å¤¾ï¼èª¿æ´æ¥èªæä»¶å¤¾è·¯å¾ç²`%TEMP%\rime.weasel`,æ¹ä¾¿æ¥é±ç®¡ç
* ç°æ­¥èçæ¶æ¯ï¼é¿åæåå´©æ½°æé·æéæªé¿æå¼èµ·å®¢æ¶ç«¯ç¨åºå´©æ½°
* ä¸å¨æåä¸­é¨ç½²æ¹æ¡ï¼é¿åå¨å®è­·æèµ·æåé²å¥é·èæé¨ç½²å¼èµ·çåµæ­»åé¡

#### Bug ä¿®å¾©

* ä¿®å¾©èªåæè¡æªæ­£ç¢ºèçæ¨é»ç¬¦èï¼æ¨é»å¨æè¡å¾æåï¼çåé¡
* ä¿®å¾©`vim-mode`ä¸çtypoå¼èµ·ç`<C-C>`ç¡æ³çæåé¡
* ä¿®å¾©é¨ç½²æ¶æ¯æªæ´æ°åé¡
* ä¿®å¾©å¸è¼å°ç¼æ¯«ææå¤å®è£èªè¨ååé¡
* ä¿®å¾©`semi_hilite`ä¸çUIæªæ­£ç¢ºé¿æåé¡, `semi_hilite`é¡è²èª¿æ´ç²é«äº®è²çåéæåº¦çæï¼æ¹åé«é©
* æ¸å°ä¸å¿è¦çæåç«¯UIæ´æ°ï¼æé«æ§è½æ¸å°æåå´©æ½°æ©ç
* ä¿®å¾©å¨é`DPI=96`çå¯å±ä¸é¿ææ¢çåé¡
* ä¿®å¾©å¨é«åå±ä¸layoutåæ¸æªdpi awareåé¡
* ä¿®å¾©Windows 11ä¸Chromeç­çè¦½å¨ä¸­éæ¿æ´»åæ¨çæä¸çæéµé¿æç°å¸¸åé¡
* ä¿®å¾©64ä½ç³»çµ±ä¸é»èªå®è£è·¯å¾ä¸æºç¢ºçåé¡



<a name="0.16.0"></a>
## [0.16.0](https://github.com/rime/weasel/compare/0.15.0...0.16.0) (2024-05-14)


#### å®è£é ç¥

**â ï¸ç±æ¼åæ¸è®åï¼å®è£å°ç¼æ¯«åè«ä¿å­å¥½æä»¶è³æï¼æ¼å®è£å¾éåæè¨»é· Windowsï¼å¦åæ­£å¨ä½¿ç¨å°ç¼æ¯«çæç¨å¯è½æå´©æ½°ã**

**â è«ç¢ºèªæ¨ç `installation.yaml` æä»¶ç·¨ç¢¼ç² `UTF-8`, å¦åå¦æ¨å¨å¶ä¸­ä¿®æ¹äºé ASCII å­ç¬¦å§å®¹çè·¯å¾æï¼æå¯è½æå¼èµ·æªæé¯èª¤ã**

#### ä¸»è¦æ´æ°

* åç´æ ¸å¿ç®æ³åº«è³ [librime 1.11.2](https://github.com/rime/librime/releases/tag/1.11.2)
* æ¹åè¼¸å¥æ³çæ¯èª¤å ±åé¡
* æ°å¢ 64 ä½ç®æ³æåç¨åºï¼æ¯æ 64 ä½ librimeï¼æ¯æå¤§å§å­ï¼å¯é¨ç½²å¤§è¦æ¨¡è©åº«æ¹æ¡ï¼
* æ¯æ arm/arm64 æ¶æ§
* å®å®è£åæ¯æ win32/x64/arm/arm64 æ¶æ§ç³»çµ±çèªåéæ¾æä»¶
* 32 ä½ç®æ³æåå¢å  LARGE ADDRESS AWARE æ¯æ
* åç´ boost ç®æ³åº«è³ 1.84.0
* IMEæ¹ç²å¯é¸é ï¼é»èªä¸å®è£
* æ£ç¨ `weaselt*.dll`ï¼å¢å è¨»åé¦æ¸¯ãæ¾³éãæ°å å¡ååèªè¨éç½®ï¼é»èªæªåç¨ï¼éå¨æ§å¶é¢æ¿/è¨­ç½®ä¸­æå·¥æ·»å ï¼ï¼æ¯æç°¡ç¹é«å°ç¼æ¯«åæä½¿è½
* æ£ç¨ `weaselt*.ime`
* ç§»é¤ `pyweasel`
* åé¸çªå£ UI å§å­åªå
* æ¹ååé¸çªå£ UI ç¹ªè£½æ§è½
* åç´ WTL åº«è³ 10.0ï¼gdi+ è³ 1.1
* æ¯é¡¯ç¤ºå¨ dpi awareï¼èªé©æä¸åé¡¯ç¤ºå¨ä¸å dpi è¨­å®è®å
* æ´æ°é«æ¸åæ¨
* å¢å¤§ IPC æ¸æé·åº¦éå¶è³ 64kï¼æ¯æé·åé¸
* åç´ plum
* æç¨çé¢åèå®ç°¡ç¹é«èªåé©æ
* `app_options` ä¸­æç¨åå¤§å°å¯«ä¸ææ
* å­é«æé¸é½è¨­å®åæ¸ `style/antialias_mode: {force_dword|cleartype|grayscale|aliased|default}`
* ASCIIçææç¤ºè·é¨é¼ æ¨åæ¨è¨­å® `style/ascii_tip_follow_cursor: bool`
* æ°å¢åæ¸ `style/layout/hilite_padding_x: int`ã`style/layout/hilite_padding_y: int`ï¼æ¯æåå¥è¨­ç½®xyåç padding
* æ°å¢åæ¸ `schema/full_icon: string`, `schema/half_icon: string`ï¼æ¯æå¨æ¹æ¡ä¸­è¨­å®å¨åè§åæ¨
* æ°å¢åæ¸ `style/text_orientation: "horizontal" | "vertical"`, è `style/vertical_text: bool` åé¤ï¼è¨­å®æå­ç¹ªè£½æ¹åï¼å¼å®¹ squirrel åæ¸
* æ°å¢åæ¸ `style/paging_on_scroll: bool`ï¼å¯è¨­å®æ»¾è¼ªç¸æé¡åï¼ç¿»é æåæåå¾åé¸ï¼
* æ°å¢åæ¸ï¼Windows10 1809å¾çæ¬çWindowsï¼æ¯æ `style/color_scheme_dark: string` è¨­å®æè²æ¨¡å¼éè²
* æ°å¢åæ¸ `style/candidate_abbreviate_length: int`ï¼æ¯æåé¸å­æ¸è¶éæç¸®ç¥é¡¯ç¤º
* æ°å¢åæ¸ `style/click_to_capture: bool` è¨­å®é¼ æ¨é»ææ¯å¦æªå
* æ°å¢åæ¸ `show_notifications_time: int` å¯è¨­å®æç¤ºé¡¯ç¤ºæéï¼å®ä½ msï¼è¨­ç½® 0 æä¸é¡¯ç¤ºæç¤º
* æ°å¢åæ¸ `show_notifications: bool` æ `show_notifications: ééåè¡¨ | "schema"`ï¼å¯å®è£½æ¯å¦é¡¯ç¤ºåææç¤ºãé¡¯ç¤ºé£äºåææç¤º
* æ°å¢åæ¸ `style/layout/baseline: int` å `style/layout/linespacing: int`ï¼å¯èªè¡èª¿æ´åæ¸ä¿®å¾©åé¸çªé«åº¦è·³èºéçåé¡
* æ£ç¨ `style/mouse_hover_ms`ï¼æ°å¢ `style/hover_type: "none"|"semi_hilite"|"hilite"`ï¼æ¹åé¼ æ¨æ¸åç¸æé«é©
* æ°å¢åæ¸ `global_ascii: bool`, æ¯æå¨å± ascii æ¨¡å¼
* æ°å¢ `app_options`ï¼æ¯ææç¨å°ç¨ `vim_mode: bool`ï¼æ¯æå¸¸è¦ vim åæ normal æ¨¡å¼æéµæï¼åæå° `ascii_mode`
* æ°å¢ `app_options`ï¼æ¯ææç¨å°ç¨ `inline_preedit: bool` è¨­å®ï¼åªåç´é«æ¼æ¹æ¡å§è¨­å®ï¼é«æ¼ `weasel.yaml` ä¸­çè¨­å®
* æ¯æå½ä»¤è¡è¨­ç½®å°ç¼æ¯« `ascii_mode` çæï¼`WeaselServer.exe /ascii`ï¼`WeaselServer.exe /nascii`
* æ¯æè¨­ç½® `comment_text_color`ã`hilited_comment_text_color` éæä¾é±èå°ææå­é¡¯ç¤º
* `hilited_mark_color` ééæï¼`mark_text` ç²ç©ºå­ç¬¦ä¸²æï¼é¡ windowns 11 çé«äº®æ¨è­
* åææ¹æ¡å¾ï¼æç¤ºæ¹æ¡åæ¨åæ¹æ¡åå­
* æ¯æå¨é¨ switch æç¤ºä½¿ç¨æ¹æ¡å§è¨­å®ç label
* WeaselSetupééæéç®éçªå£è¨­ç½®ç¨æ¶ç®éè·¯å¾
* æ°å¢æ¯ææ¹æ¡å§å®ç¾©æ¹æ¡å°ç¨éè²
* æ¯æ imtip
* å¢å é¡å¾®è»æ¼é³çé«äº®æ¨è­å¨é¼ æ¨é»ææçåæ
* æ¯æå¨å­é«è¨­å®ä»»ä¸åçµä¸­è¨­ç½®å­é«æ´é«çå­éæå­å½¢
* åªåé»æé¸å­éè¼¯
* è±ç´ä½å±åè½æï¼äºæä¸ä¸æ¹åéµ
* åé¸çªè¶åºä¸æ¹éçæï¼å¨ç¶ååæçµæåä¿æå¨è¼¸å¥ä½ç½®ä¸æ¹ï¼æ¸å°åé¸çªå£é«åº¦è®å°ææ½å¨ççªå£ä¸ä¸è·³å
* èª¿æ´ TSF åæ¨ä½ç½®ï¼`inline_preedit: false` æï¼ï¼æ¸å°åæ¨éç
* WeaselSetup ä¿®æ¹ç¨æ¶ç®éè·¯å¾ï¼å·²å®è£æï¼
* èªè¨æ¬æ°å¢èå®ï¼éåæå
* IPC å ±æè½ç¾© `\n`ã`\t`ï¼ä¸åå  `\n` å¼ç¼æç¨å´©æ½°
* ä½¿ç¨ clang-format æ ¼å¼åä»£ç¢¼ï¼çµ±ä¸ä»£ç¢¼é¢¨æ ¼
* èªåæä»¶çæ¬ä¿¡æ¯
* æ¸¬è©¦é ç® test åªå¨ debug éç½®çæä¸ç·¨è­¯æ§å»º

#### Bug ä¿®å¾©

* ä¿®å¾© word 365 ä¸­åé¸çªéçç¡æ³æ­£å¸¸é¡¯ç¤ºçåé¡
* ä¿®å¾© word è¡å°¾è¼¸å¥æåé¸çªåè¦è·³ååé¡
* ä¿®å¾© word ä¸­ç¡æ³é»æé¸è©åé¡
* ä¿®å¾© excel ç­æç¨ä¸­ï¼ç¬¬ä¸éµ keydown ææªåæå½åºåé¸çªåé¡
* ä¿®å¾©å°åºè©å¸æ¸æå¾å¼èµ·çå¤å explorer é²ç¨çåé¡ï¼åªåå°æå°è©±æ¡çé¢é¡¯ç¤º
* ä¿®å¾©æéç¨æ¶ç®éï¼ç¨åºç®éå¼èµ·çå¤å explorer.exe é²ç¨åé¡ï¼æ¯ææåæªååææééäºç®é
* ä¿®å¾©ç³»çµ±æç¤éåå¾æªåæé¡¯ç¤ºçåé¡
* ä¿®å¾© `style/layout/min_width` å¨é¨åä½å±ä¸æªçæåé¡
* ä¿®å¾© preedit å¯¬é«è¨ç®é¯èª¤åé¡
* ä¿®å¾©ç¿»é æéå¨è±ç´ä½å±åè½æä½ç½®é¯èª¤
* ä¿®å¾©è±ç´ä½å±å¸¶éç©º mark_text æçè¨ç®é¯èª¤
* ä¿®å¾© composing ä¸­åé¸çªé¨æå­ç§»ååé¡
* ä¿®å¾© wezterm gpu æ¨¡å¼ä¸ç¡æ³ä½¿ç¨åé¡
* ä¿®å¾© `style/inline_preedit: true` æç¬¬ä¸éµè¼¸å¥æåé¸çªä½ç½®é¯èª¤
* ä¿®å¾©ç®æ³æåå®ä¾éè¡
* ä¿®å¾©èª¿ç¨ WeaselServer.exe æªæ­£å¸¸éåæååé¡
* ä¿®å¾©å¶ç¼çé¡¯å¡éè¯æå­ç©ºç½åé¡
* ä¿®å¾©é¨ç½²éç¨ä¸­å¦æéµè¼¸å¥å¼ç¼çéè¤ç¼åº tip æç¤ºçªåé¡
* ä¿®å¾©é¨åæ¹æ¡ä¸­çåæ¨é¡¯ç¤ºï¼`english.schema.yaml`ï¼
* ä¿®å¾© `preedit_type: preview` æçåæ¨é¯èª¤åé¡
* ä¿®å¾© `shadow_color` éæææªåå°ºå¯¸éå¤§åé¡ï¼æ¸å°æªåå°ºå¯¸
* ä¿®å¾©å¤©åå°æ¹æï¼é«äº®åé¸åè§åå¾ä¸æ­£ç¢ºåé¡
* ä¿®å¾©æäºçæä¸å¤©åå°æ¹ç preedit èæ¯è²åè§ç°å¸¸åé¡
* ä¿®å¾©åé¸å°¾é¨ç©ºç½å­ç¬¦å¼èµ·çä½å±è¨ç®é¯èª¤åé¡
* ä¿®å¾© mark_text ç¹ªè£½é¸é½åé¡
* ä¿®å¾©éé»å®è£å½çªåé¡
* ä¿®å¾© librime-preedit å¼èµ·çæç¨å´©æ½°åé¡
* ä¿®å¾© plum ç¨æ¶ç®éè­å¥é¯èª¤åé¡
* ä¿®å¾©å®è£å¾æªå¨æ§å¶é¢æ¿ä¸­æ·»å è¼¸å¥æ³ãå¸è¼å¾æªåªé¤æ§å¶é¢æ¿ä¸­çè¼¸å¥æ³æ¸å®åé¡
* ä¿®å¾©ä¸äºå¶ä»å·²ç¥ç bug

#### å·²ç¥åé¡

* é¨åæç¨ä»å­å¨è¼¸å¥æ³ç¡æ³è¼¸å¥æå­æé¿æç°å¸¸çåé¡
* WeaselServer ä»å¯è½ç¼çå´©æ½°
* ä»ææ¥µå°é¨åé²çæ¯è»ä»¶å¯è½èª¤å ±çæ¯



<a name="0.15.0"></a>
## [0.15.0](https://github.com/rime/weasel/compare/0.14.3...0.15.0) (2023-06-06)


#### å®è£é ç¥

**â ï¸å®è£å°ç¼æ¯«åè«ä¿å­å¥½æä»¶è³æï¼æ¼å®è£å¾éå Windows ï¼å¦åæ­£å¨ä½¿ç¨å°ç¼æ¯«çæç¨å°æå´©æ½°ã**
**â ï¸æ­¤çæ¬çå°ç¼æ¯«éè¦ä½¿ç¨ Windows 8.1 ææ´é«çæ¬çæä½ç³»çµ±ã**

#### ä¸»è¦æ´æ°

* åç´æ ¸å¿ç®æ³åº«è³ [librime 1.8.5](https://github.com/rime/librime/blob/master/CHANGELOG.md#185-2023-02-05)
* DPI æ ¹æé¡¯ç¤ºå¨èªåèª¿æ´
* æ¯æåé¸çªå£ç­åè§é¡¯ç¤º
  * `style/layout/corner_radius: int`
* å¼å®¹é¼ é¬ç®¡ä¸­é«äº®åè§åæ¸`style/layout/hilited_corner_radius: int`
* æ¯æä¸»é¡é¡è²ä¸­å«æéæééä»£ç¢¼, æ¯ææ ¼å¼ 0xaabbggrrï¼0xbbggrr, 0xabgr, 0xbgr
* éè²ä¸»é¡æ¯æé»èªABGRé åºï¼æARGBãRGBAé åº
  * `preset_color_schemes/color_scheme/color_format: "argb" | "rgba" | ""`
* æ¯æç·¨ç¢¼/é«äº®åé¸/æ®éåé¸/è¼¸å¥çªå£/åé¸éæ¡çé°å½±é¡è²ç¹ªè£½
  * `style/layout/shadow_radius: int`
  * `style/layout/shadow_offset_x: int`
  * `style/layout/shadow_offset_y: int`
  * `preset_color_schemes/color_scheme/shadow_color: color`
  * `preset_color_schemes/color_scheme/nextpage_color: color`
  * `preset_color_schemes/color_scheme/prevpage_color: color`
  * `preset_color_schemes/color_scheme/candidate_back_color: color`
  * `preset_color_schemes/color_scheme/candidate_shadow_color: color`
  * `preset_color_schemes/color_scheme/candidate_border_color: color`
  * `preset_color_schemes/color_scheme/hilited_shadow_color: color`
  * `preset_color_schemes/color_scheme/hilited_candidate_shadow_color: color`
  * `preset_color_schemes/color_scheme/hilited_candidate_border_color: color`
  * `preset_color_schemes/color_scheme/hilited_mark_color: color`
* æ¯æèªå®ç¾©æ¨ç±¤ãè¨»è§£å­é«åå­è
  * `style/label_font_face: string`
  * `style/comment_font_face: string`
  * `style/label_font_point: int`
  * `style/comment_font_point: int`
  * `style/layout/align_type: "top" | "center" | "bottom"`
* æ¯ææå®å­ç¬¦ Unicode åéå­é«è¨­å®
* æ¯æå­éï¼å­å½¢é¢¨æ ¼è¨­å®
  * `style/font_face: font_name[:start_code_point:end_code_point][:weight_set][:style_set][,font2...]`
    * example: `"Segoe UI Emoji:20:39:bold:italic, Segoe UI Emoji:1f51f:1f51f, Noto Color Emoji SVG:80, Arial:600:6ff, Segoe UI Emoji:80, LXGW Wenkai Narrow"`
* æ¯æèªå®ä¹å­ä½åéç¯åãé åºå®ä¹
* å½©è²å­é«æ¯æ
  * Windows 10 å¨å¹´çåï¼éè¦ä½¿ç¨ COLR æ ¼å¼å½©è²å­é«
  * Windows 11 ï¼å¯ä»¥ä½¿ç¨ SVG å­é«
* æ°å¢è±ç´æå­ä½å±
  * `style/vertical_text: bool`
  * `style/vertical_text_left_to_right: bool`
  * `style/vertical_text_with_wrap: bool`
* æ°å¢è±ç´ä½å±verticalçªå£ä¸ç§»æèªåååºæå
  * `style/vertical_auto_reverse: bool`
* æ°å¢ãå¤©åå°æ¹ãä½å±ï¼ç± margin è hilite_padding ç¢ºå®, ç¶margin <= hilite_paddingæçæ
* margin_x æ margin_y è¨­ç½®ç²è² å¼æï¼é±èè¼¸å¥çªå£ï¼ä¸å½±é¿æ¹æ¡é¸å®é¡¯ç¤º
* æ°å¢ preedit_type: preview_all ï¼å¨è¼¸å¥æå°åé¸é é¡¯ç¤ºæ¼ composition ä¸­
  * `style/preedit_type: "composition" | "preview" | "preview_all"`
* æ°å¢è¼¸å¥æ³é«äº®æç¤ºæ¨è¨
  * `style/mark_text: string`
* æ°å¢è¼¸å¥æ¹æ¡åæ¨é¡¯ç¤ºï¼å¯å¨èªè¨æ¬ä¸­é¡¯ç¤ºï¼æä»¶æ ¼å¼ç²ico
  * `schema/icon: string`
  * `schema/ascii_icon: string`
* æ°å¢é¸é ï¼åè¨±å¨åæ¨ä½ç½®ç²åå¤±æææ¼çªå£å·¦ä¸è§ç¹ªè£½åé¸æ¡ï¼èä¸æ¯æ¡é¢å·¦ä¸è§ï¼
  * `style/layout/enhanced_position: bool`
* æ°å¢é¼ æ¨é»ææªåå°åªè²¼æ¿åè½
* æ°å¢é¸é ï¼æ¯æè¶é·èªåæè¡/æåé¡¯ç¤º
  * `style/layout/max_width: int`
  * `style/layout/max_height: int`
* æ¯ææ¹æ¡å§è¨­å®éè²
  * `style/color_scheme: string`
* æ¯æå¤è¡åå®¹é¡¯ç¤ºï¼\r, \n, \r\nåæ¯æ
* æ¯ææ¹æ¡å§è¨­å®éè²
* ç¹ªè£½æ§è½æå
* composition æ¨¡å¼ä¸æ°å¢ä¸åç·é¡¯ç¤º
* é¨äºé²å¶æä»¶æä¾èª¿è©¦ç¬¦è

#### Bug ä¿®å¾©

* è½ç¾©æ¥æéµç¤ä¸­ç¹æ®æéµ
* åé¸æå­éé·æå´©æ½°
* ä¿®å¾©ç¨æ¶ç®éä¸ç¡ `default.custom.yaml` æ `weasel.custom.yaml` æï¼è¨­å®çªå£ç¡æ³å½åºçåé¡
* æ¹æ¡ä¸­è¨­å®inline_preeditç²trueæï¼é¨ç½²å¾ç·¨ç¢¼æ«ç«¯åºç¾ç°å¸¸ç¬¦è
* é¨åæç¨ç¡æ³è¼¸å¥æå­çåé¡
* ä¿®å¾©é¨ç½²æç¡é¡¯ç¤ºæç¤ºçåé¡
* ä¿®å¾©ä¸­æè·¯å¾ç¸éåé¡
* ä¿®å¾©å³éµèå®æéç¨åºç®é/ç¨æ¶ç®éæï¼è³æºç®¡çå¨ç¡é¿æçåé¡
* ä¿®å¾©é¨åå§å­è¨ªååé¡
* ä¿®å¾©æä½ç³»çµ± / WinGet ç¡æ³è­å¥å°ç¼æ¯«çæ¬èçåé¡
* ä¿®å¾© composition æ¨¡å¼ä¸åæ¨ä½ç½®ä¸æ­£å¸¸çåé¡
* ä¿®å¾© Word ä¸­å°ç¼æ¯«å·¥ä½ä¸æ­£å¸¸çåé¡
* è¥å¹²éç¼ç°å¢éç½®åé¡ä¿®å¾©

#### å·²ç¥åé¡

* é¨åæç¨ä»å­å¨è¼¸å¥æ³ç¡æ³è¼¸å¥æå­çåé¡
* WeaselServer ä»å¯è½ç¼çå´©æ½°
* é¨åé²çæ¯è»ä»¶å¯è½èª¤å ±çæ¯



<a name="0.14.3"></a>
## 0.14.3 (2019-06-22)


#### ä¸»è¦æ´æ°

* åç´æ ¸å¿ç®æ³åº« [librime 1.5.3](https://github.com/rime/librime/blob/master/CHANGELOG.md#153-2019-06-22)
  * ä¿®å¾© `single_char_filter` çµä»¶
  * å®åä¸æ¸¸é ç® `librime` çå¨èªåç¼ä½æµç¨ï¼åå»æå·¥ä¸å³æ§å»ºçµæçæ­¥é©



<a name="0.14.2"></a>
## 0.14.2 (2019-06-17)


#### ä¸»è¦æ´æ°

* åç´æ ¸å¿ç®æ³åº« [librime 1.5.2](https://github.com/rime/librime/blob/master/CHANGELOG.md#152-2019-06-17)
  * ä¿®å¾©ç¨æ¶è©çæ¬éï¼ç©©å®é å¥è³ªéãå¹³è¡¡ç¿»è­¯å¨åªåç´ [librime#287](https://github.com/rime/librime/issues/287)
  * å»ºè­° 0.14.1 çæ¬ç¨å®¶åç´



<a name="0.14.1"></a>
## 0.14.1 (2019-06-16)


#### ä¸»è¦æ´æ°

* åç´æ ¸å¿ç®æ³åº« [librime 1.5.1](https://github.com/rime/librime/blob/master/CHANGELOG.md#151-2019-06-16)
  * ä¿®å¾©æªè£éèªè¨æ¨¡åæç¼ºççé å¥ç®æ³ ([weasel#383](https://github.com/rime/weasel/issues/383))



<a name="0.14.0"></a>
## 0.14.0 (2019-06-11)


#### ä¸»è¦æ´æ°

* åç´æ ¸å¿ç®æ³åº« [librime 1.5.0](https://github.com/rime/librime/blob/master/CHANGELOG.md#150-2019-06-06)
  * é·ç§»å°VS2017æ§å»ºå·¥å·ï¼å»ºè¨­å®å¨å¯é çå¨èªåæ§å»ºãç¼ä½æµç¨
  * ééæ´æ°ç¬¬ä¸æ¹åº«ï¼ä¿®å¾©userdbæä»¶å¤¾å¤§éä½ç¨ç£ç¤ç©ºéçåé¡
  * å°Rimeæä»¶ç´å¥èªååæ§å»ºæµç¨ãæ¬æ¬¡ç¼è¡åå«å©æ¬¾æä»¶ï¼
    - [librime-lua](https://github.com/hchunhui/librime-lua)
    - [librime-octagram](https://github.com/lotem/librime-octagram)
* é«æ¸éè£½çå½©è¼¸å¥æ³çæåæ¨


#### Features

* **ui:**  high-res status icons; display larger icons in WeaselPanel ([093fa806](https://github.com/rime/weasel/commit/093fa80678422f972e7a7285060553eeedb0e591))



<a name="0.13.0"></a>
## 0.13.0 (2019-01-28)


#### ä¸»è¦æ´æ°

* åç´æ ¸å¿ç®æ³åº« [librime 1.4.0](https://github.com/rime/librime/blob/master/CHANGELOG.md#140-2019-01-16)
  * æ°å¢ [æ¼å¯«ç³¾é¯](https://github.com/rime/librime/pull/228) é¸é 
    ç¶ååé QWERTY éµç¤ä½å±åä½¿ç¨ `script_translator` çæ¹æ¡
  * ä¿®å¾©åç´ãé¨ç½²æ¸ææç¼ççè¥å¹²é¯èª¤
* æ´æè¼¸å¥æ³çæåæ¨ï¼é©éé«åè¾¨çå±å¹


#### Features

* **tsf:**  register as GUID_TFCAT_TIPCAP_UIELEMENTENABLED ([ae876916](https://github.com/rime/weasel/commit/ae8769166ea50b319aa89460b60890d598c618c5))
* **ui:**  high-res icons (#324) ([ad3e2027](https://github.com/rime/weasel/commit/ad3e2027644f80c6a384b7730da20dd239e780af))

#### Bug Fixes

* **WeaselSetup.vcxproj:**  Debug build linker options ([eb885fe0](https://github.com/rime/weasel/commit/eb885fe06ffd720d3de1101be2410a94bd3747c0))
* **output/install.nsi:**  bundle new yaml files from rime/rime-prelude ([cba35e9b](https://github.com/rime/weasel/commit/cba35e9b2c34d095b9ca1eb44e923e004cf23ddc))
* **test:**  Debug build ([c771126c](https://github.com/rime/weasel/commit/c771126c74fa1c4f91d4bfd8fb5ab8c16dcb7c4c))
* **tsf:**  set current page to 0 as page count is always 1 ([5447f63b](https://github.com/rime/weasel/commit/5447f63bc7c9d0e31d7ba8ead1e1229938be276d))



<a name="0.12.0"></a>
## 0.12.0  (2018-11-12)

#### ä¸»è¦æ´æ°

* åä½µå°ç¼æ¯«èå°ç¼æ¯«ï¼TSFï¼å©ç¨®è¼¸å¥æ³
* åä½µ32ä½è64ä½ç³»çµ±ä¸çå®è£ç¨åº
* ä½¿ç¨ç³»çµ±çééè¼¸å¥æ³åè½ï¼é»èªå¿«æ·éµ Ctrl + Spaceï¼å¾ï¼è¼¸å¥æ³åæ¨å°é¡¯ç¤ºç¦ç¨çæ
* ä¿®å¾©ä¸äºææ³ä¸çå´©æ½°åé¡
* åç´æ ¸å¿ç®æ³åº« [librime 1.3.2](https://github.com/rime/librime/blob/master/CHANGELOG.md#132-2018-11-12)
  * åè¨±å¤åç¿»è­¯å¨å±ç¨åä¸åè©å¸æççµè©ï¼å¯¦ç¾åºå®å®å­é åºçå½¢ç¢¼çµè©([librime#184](https://github.com/rime/librime/issues/184))ã
  * æ°å¢ translator/always_show_comments é¸é ï¼åè¨±å§çµé¡¯ç¤ºåé¸è©è¨»è§£ã

#### Bug Fixes

* **candidate:** fix COM pointer reference ([63d6d9a](https://github.com/rime/weasel/commit/63d6d9a))
* **ipc:** eliminate some trivial warnings ([dae945c](https://github.com/rime/weasel/commit/dae945c))
* fix constructor ([b25f968](https://github.com/rime/weasel/commit/b25f968))


#### Features

* **compartment:** show IME disabled on language bar ([#263](https://github.com/rime/weasel/issues/263)) ([4015d18](https://github.com/rime/weasel/commit/4015d18))
* **install:** combine IME and TSF ([#257](https://github.com/rime/weasel/issues/257)) ([91cbd2c](https://github.com/rime/weasel/commit/91cbd2c))
* **tsf:** get IME keyboard identifier by searching registry ([#272](https://github.com/rime/weasel/issues/272)) ([b60b5b1](https://github.com/rime/weasel/commit/b60b5b1))
* **WeaselSetup:** detect 64-bit on single 32-bit build ([#266](https://github.com/rime/weasel/issues/266)) ([fb3ae0f](https://github.com/rime/weasel/commit/fb3ae0f))



<a name="0.11.1"></a>
## 0.11.1 (2018-04-26)

#### ä¸»è¦æ´æ°

* ä¿®å¾©äºå¨ Excel ä¸­å¥æªçè¼¸å¥ä¸å¤±åé¡ï¼[#185](https://github.com/rime/weasel/issues/185)ï¼
* åè½éµä¸åæè§¸ç¼è¼¸å¥ç¦é»ï¼[#194](https://github.com/rime/weasel/issues/194)ã[#195](https://github.com/rime/weasel/issues/195)ã[#204](https://github.com/rime/weasel/issues/204)ï¼
* ãç²åæ´å¤è¼¸å¥æ¹æ¡ãåè½åªåï¼[#180](https://github.com/rime/weasel/issues/180)ï¼
* ä¿®å¾©äºå¾èºå¯è½åæåºç¾å¤åç®æ³æåçåé¡ï¼[#199](https://github.com/rime/weasel/issues/199)ï¼
* æ¢å¾©èªè¨æ¬å³éµèå®ä¸­ãç¨æ¶è³æåæ­¥ãä¸é 

#### Bug Fixes

* **server:**  use kernel mutex to ensure single instance (#207) ([bd0c4720](https://github.com/rime/weasel/commit/bd0c4720669c61087dd930b968640c60a526ecb2))
* **tsf:**
  *  do not reset composition on document focus set ([124fc947](https://github.com/rime/weasel/commit/124fc9475c30963a9bbbf9a097b452b52e8ab658))
  *  use `ITfContext::GetSelection` to get cursor position ([5664481c](https://github.com/rime/weasel/commit/5664481cc9ddd28db35c3155f7ddf83a55b65275))
  *  recover sync option in TSF language bar menu ([7a0a8cc2](https://github.com/rime/weasel/commit/7a0a8cc2a3dd913ce34204d6e966b263af766f3b))

#### Features

* **build.bat:**  build installer ([e18117b7](https://github.com/rime/weasel/commit/e18117b7b42d5af0fbfa807e4c858c40206b4967))
* **installer:**  bundle curl, update rime-install.bat, fixes #180 ([2f3b283d](https://github.com/rime/weasel/commit/2f3b283d6ef4aa0580d186e626dadb9e1030dfd5))
* **rime-install.bat:**  built-in ZIP package installer ([739be9bc](https://github.com/rime/weasel/commit/739be9bc9ba08e294f51e1d7232407148ded716c))



<a name="0.11.0"></a>
## 0.11.0 (2018-04-07)

#### ä¸»è¦æ´æ°

* æ°å¢ [Rime éç½®ç®¡çå¨](https://github.com/rime/plum)ï¼ééãè¼¸å¥æ³è¨­å®ï¼ç²åæ´å¤è¼¸å¥æ¹æ¡ãèª¿ç¨
* å¨è¼¸å¥æ³èªè¨æ¬é¡¯ç¤ºçæåææéï¼TSF æ¨¡å¼ï¼
* ä¿®å¾©å¤ååç«¯å¼å®¹æ§åé¡
* æ°å¢éè²ä¸»é¡ãç¾ä»£èã`metroblue`ããå¹½è½ã`psionics`
* å®è£ç¨åºæ¯æç¹é«ä¸­æä»é¢
* ä¿®å¾© 0.10 çåç´å®è£å¾ï¼å ç¨æ¶æä»¶å¤¾ä¸­ä¿çèæä»¶ãéç½®ä¸çæçåé¡
* åç´ 0.9 ç `.kct` æ ¼å¼çç¨æ¶è©å¸
  **æ³¨æ**ï¼åæ­¤ä¸åçæ¬æ¯ææ ¼å¼åç´ï¼è«åå¿ç± 0.9 åç´å° 0.11ï¼åå®è£å¾çºçæ¬

#### Features

* **WeaselDeployer:**  add Get Schemata button to run plum script (#174) ([c786bb5b](https://github.com/rime/weasel/commit/c786bb5ba2f1cc7e79b66f36d0190e61cd7233ae))
* **build.bat:**  customize PLATFORM_TOOLSET settings ([c7a9a4fb](https://github.com/rime/weasel/commit/c7a9a4fb530e0274450e4296cb0db2906d2f1fb4))
* **config:**
  *  enable customization of label format ([76b08bae](https://github.com/rime/weasel/commit/76b08bae810735c5f1c8626ec39a7afd463f0269))
  *  alias `style/layout/border_width` to `style/layout/border` ([013eefeb](https://github.com/rime/weasel/commit/013eefebaa4474e7814b6cfb6c905bcc12543a7f))
* **install.nsi:**
  *  add Traditional Chinese for installer ([d1a9696a](https://github.com/rime/weasel/commit/d1a9696a57dfc9e04c51899572e156fb1676f786))
  *  upgrade to Modern UI 2 and prompt reboot (#128) ([f59006f8](https://github.com/rime/weasel/commit/f59006f8d195ca848e45cd934f44b3318fb135c1))
* **ipc:**  specify user name for named pipe ([2dfa5e1a](https://github.com/rime/weasel/commit/2dfa5e1a63ee1c26ef983d25471682f87cc60b62))
* **preset_color_schemes:**
  *  add homepage featured color scheme `psionics` ([89a0eb8b](https://github.com/rime/weasel/commit/89a0eb8b861b9b3f2abc42df65821254010b24ff))
  *  add metroblue color scheme ([f43e2af6](https://github.com/rime/weasel/commit/f43e2af608bde38a6d345ba540f4c37ec024853a))
* **submodules:**  switch from rime/brise to rime/plum ([f3ff5aa9](https://github.com/rime/weasel/commit/f3ff5aa962a7b8cce2b74a5cb583a69cb8938e55))
* **tsf:**
  *  enable language bar button (#170) ([2b660397](https://github.com/rime/weasel/commit/2b660397950f348205e6a93bf44a46e4a72bcc81))
  *  accomplish candidate UI interfaces (#156) ([1f0ae793](https://github.com/rime/weasel/commit/1f0ae7936fd495ecf4ff3ef162c0e38297d2d582))
  *  fix candidate selecting in preview preedit mode ([206efd69](https://github.com/rime/weasel/commit/206efd692124339d0e256198360c1860c72cd807))
  *  support user defined preedit display type ([f76379b0](https://github.com/rime/weasel/commit/f76379b01abe9d3971d68e2e272067e0bb855cc9))
* **weasel.yaml:**  enable ascii_mode in console applications by default ([28cdd096](https://github.com/rime/weasel/commit/28cdd09692f77e471784bf85ff7a19bc48e113f4))

#### Bug Fixes

*   fix defects according to Coverity Scan ([526a91d2](https://github.com/rime/weasel/commit/526a91d2954492cc8e23c2c4c8def2a053af7c20))
*   inline_preedit && fullscreen causing dead lock when there's no candidates. ([deb0bb24](https://github.com/rime/weasel/commit/deb0bb24b3f3aeaf73aef344968b7f15b471443f))
* **RimeWithWeasel:**  fix wild pointer ([ae2e3c4a](https://github.com/rime/weasel/commit/ae2e3c4a256fb9a2f7851c54114822d1bfbf0316))
* **ServerImpl:**  do finalization before exit process ([b1bae01e](https://github.com/rime/weasel/commit/b1bae01eb25c5e24e074807b7b3cb8a6d8401276))
* **WeaselUI:**
  *  specify default label format in constructor ([4374d244](https://github.com/rime/weasel/commit/4374d2440b99726894799861fb3bd5b93e73dec5), closes [#147](https://github.com/rime/weasel/issues/147))
  *  limit to subscript range when processing candidates ([6b686c71](https://github.com/rime/weasel/commit/6b686c717bfab141469c3d48ec1c6acbeb79921e), closes [#121](https://github.com/rime/weasel/issues/121))
* **composition:**
  *  improve compositions and edit sessions (#146) ([fbdb6679](https://github.com/rime/weasel/commit/fbdb66791da3291b740edf3c337032674e4377e8))
  *  fix crashes in notebook with inline preedit ([5e257088](https://github.com/rime/weasel/commit/5e257088be823a2569609f0b3591af3a51d47a46))
  *  fix crashes in notebook with inline preedit ([892930ce](https://github.com/rime/weasel/commit/892930cebc4235a0a1ef58803fe88c32ccc8b4e9))
* **install.bat:**  run in elevate cmd; detach WeaselServer process ([2194d9fb](https://github.com/rime/weasel/commit/2194d9fbd7d0341fef94efdbe9268af8a6237438))
* **ipc:**
  *  add version check for security descriptor initialization ([b97ccffe](https://github.com/rime/weasel/commit/b97ccffe76a6abf3e353724ce0607d5dd97de6f2), closes [#157](https://github.com/rime/weasel/issues/157))
  *  grant access to IE protected mode ([16c163a4](https://github.com/rime/weasel/commit/16c163a41d0afc9824723009ba8b9b9ba37b1c72))
  *  try to reconnect when failed ([3c286b6a](https://github.com/rime/weasel/commit/3c286b6a942769abf13188d88f9ab5e4c125807b))
* **librime:**  make rime_api.h available in librime\build\include\ ([3793e22c](https://github.com/rime/weasel/commit/3793e22c47b34c61d305ca80567dfdafe08b2302))
* **server:**  postpone tray icon updating when focusing on explorer ([45cf1120](https://github.com/rime/weasel/commit/45cf112099fa6db335cda06b1aaa0ae9c7975efe))
* **tsf:**
  *  fix candidate behavior ([9e2f9f17](https://github.com/rime/weasel/commit/9e2f9f17c059bf129c2c8b2561471670ea200dd7))
  *  fix `ITfCandidateListUIElement` implemention ([9ce1fa87](https://github.com/rime/weasel/commit/9ce1fa87e6ef788e791e68193700e2ebdd950d20))
  *  use commmit text preview to show inline preview ([b1d1ec43](https://github.com/rime/weasel/commit/b1d1ec43e132998ea8764d8dac2098a2b3d9a3e8))



<a name="0.10.0"></a>
## å°ç¼æ¯« 0.10.0 (2018-03-14)


#### ä¸»è¦æ´æ°

* å¼å®¹ Windows 8 ~ Windows 10
* æ¯æé«åè¾¨çé¡¯ç¤ºå±
* ä»é¢é¢¨æ ¼é¸é 
  * å¨å§åµç·¨ç¢¼è¡é è¦½çµææå­
  * å¯æå®åé¸åºèçæ¨£å¼
* åç´æ ¸å¿ç®æ³åº« [librime 1.3.0](https://github.com/rime/librime/blob/master/CHANGELOG.md#130-2018-03-09)
  * æ¯æ YAML ç¯é»å¼ç¨ï¼æ¹ä¾¿æ¨¡å¡åéç½®
  * æ¹é²é¨ç½²æµç¨ï¼å¨ `build` å­ç®ééä¸­å­æ¾çæçæ¸ææä»¶
* ç²¾ç°¡å®è£åé è£çè¼¸å¥æ¹æ¡ï¼æ´å¤æ¹æ¡å¯ç± [æ±é¢¨ç ´](https://github.com/rime/plum) åå¾

#### Features

* **build.bat:**  customize PLATFORM_TOOLSET settings ([c7a9a4fb](https://github.com/rime/weasel/commit/c7a9a4fb530e0274450e4296cb0db2906d2f1fb4))
* **config:**
  *  enable customization of label format ([76b08bae](https://github.com/rime/weasel/commit/76b08bae810735c5f1c8626ec39a7afd463f0269))
  *  alias `style/layout/border_width` to `style/layout/border` ([013eefeb](https://github.com/rime/weasel/commit/013eefebaa4474e7814b6cfb6c905bcc12543a7f))
* **tsf:**
  *  fix candidate selecting in preview preedit mode ([206efd69](https://github.com/rime/weasel/commit/206efd692124339d0e256198360c1860c72cd807))
  *  support user defined preedit display type ([f76379b0](https://github.com/rime/weasel/commit/f76379b01abe9d3971d68e2e272067e0bb855cc9))

#### Bug Fixes

*   Support High DPI Display [#28](https://github.com/rime/weasel/issues/28)
* **WeaselUI:**  limit to subscript range when processing candidates ([6b686c71](https://github.com/rime/weasel/commit/6b686c717bfab141469c3d48ec1c6acbeb79921e), closes [#121](https://github.com/rime/weasel/issues/121))
* **install.bat:**  run in elevate cmd; detach WeaselServer process ([2194d9fb](https://github.com/rime/weasel/commit/2194d9fbd7d0341fef94efdbe9268af8a6237438))
* **librime:**  make rime_api.h available in librime\build\include\ ([3793e22c](https://github.com/rime/weasel/commit/3793e22c47b34c61d305ca80567dfdafe08b2302))
* **tsf:**
  *  Results of auto-selection cleared by subsequent manual selection [#107](https://github.com/rime/weasel/issues/107)
  *  use commmit text preview to show inline preview ([b1d1ec43](https://github.com/rime/weasel/commit/b1d1ec43e132998ea8764d8dac2098a2b3d9a3e8))



<a name="0.9.30"></a>
## å°ç¼æ¯« 0.9.30 (2014-04-01)


#### Rime ç®æ³åº«è®æ´é

* æ°å¢ï¼ä¸­è¥¿æåææ¹å¼ `clear`ï¼åæææ¸é¤æªå®æçè¼¸å¥
* æ¹é²ï¼é·æ Shiftï¼æ Controlï¼éµä¸è§¸ç¼ä¸­è¥¿æåæ
* æ¹é²ï¼ä¸¦æè¼¸å¥ï¼è¥æåè»éµåä¸å±æéµå°æçå­ç¬¦
* æ¹é²ï¼æ¯æå°ç¨æ¶è¨­å®ä¸­çåè¡¨åç´ æè£éªï¼ä¾å¦ `switcher/@0/reset: 1`
* æ¹é²ï¼ç¼ºå°è©å¸æºæä»¶ `*.dict.yaml` æå©ç¨åºæè©å¸ `*.table.bin` å®æé¨ç½²
* ä¿®å¾©ï¼èªåçµè©çè©å¸é¨ç½²ææªæª¢æ¥ãå«è¡æãçè®æ´ï¼å°è´ç´¢å¼å¤±æãåé¸å­ç¼ºå¤±
* ä¿®å¾©ï¼`comment_format` æå°åé¸è¨»ééè¤ä½¿ç¨å¤æ¬¡çBUG

#### ãæ±é¢¨ç ´ãè®æ´é

* æ°å¢ï¼å¿«æ·éµ `Control+.` åæä¸­è¥¿ææ¨é»
* æ´æ°ï¼ãå«è¡æããæææ¼é³ããå°çæ¼é³ããäºç­ç«ã
* æ¹é²ï¼ãæææ¼é³Â·èªå¥æµã`/0` ~ `/10` è¼¸å¥æ¸å­ç¬¦è



<a name="0.9.29.1"></a>
## å°ç¼æ¯« 0.9.29.1 (2013-12-22)


#### ãå°ç¼æ¯«ãè®æ´é

* è®æ´ï¼ä¸åæ¯æ Windows XP SP2ï¼å åç´ç·¨è­¯å¨ä»¥æ¯æ C++11
* ä¿®å¾©ï¼è¼¸å¥èªè¨é¸ç²ä¸­æï¼èºç£ï¼å¨ Windows 8 ç³»çµ±ä¸åºç¾å¤é¤çè¼¸å¥æ³é¸é 
* ä¿®å¾©ï¼åç´å®è£å¾ï¼å¤è§è¨­å®ä»é¢æªåæé¡¯ç¤ºåºæ°å¢çéè²æ¹æ¡
* ä¿®å¾©ï¼éè²æ¹æ¡ Google+ çé è¦½å

#### Rime ç®æ³åº«è®æ´é

* æ´æ°ï¼librime åç´å° 1.1
* æ°å¢ï¼åºå®æ¹æ¡é¸å®æåé åºçé¸é  `default.yaml`: `switcher/fix_schema_list_order: true`
* ä¿®å¾©ï¼æ­£ç¢ºå¹éåµå¥çââå½å¼èââ
* æ¹é²ï¼ç¢¼è¡¨è¼¸å¥æ³èªåä¸å±åé å­ä¸å±ï¼[ç¤ºä¾](https://gist.github.com/lotem/f879a020d56ef9b3b792)ï¼<br/>
    è¥æ `speller/auto_select: true`ï¼åé¸é  `speller/max_code_length:` éå®ç¬¬Nç¢¼ç¡éç¢¼èªåä¸å±
* åªåï¼ç²è©çµèªåç·¨ç¢¼æï¼éå¶å å¤é³å­èç¢çççµåæ¸ç®ï¼é¿åçª®èæ¶èééè³æº

#### ãæ±é¢¨ç ´ãè®æ´é

* æ´æ°ï¼ãç²µæ¼ãå¯å¥è¡å¤ç²µèªè©å½
* åªåï¼èª¿æ´é¨åç°é«å­çå­é »



<a name="0.9.28"></a>
## å°ç¼æ¯« 0.9.28 <2013-12-01>


#### ãå°ç¼æ¯«ãè®æ´é

* æ°å¢ï¼ä¸çµéè²æ¹æ¡ï¼ä½èï¼P1461ãPatricivsãskojãäºç£å
* ä¿®å¾©ï¼[Issue 528](https://code.google.com/p/rimeime/issues/detail?id=528) Windows 7 IE11 æå­ç¡æ³ä¸å±
* ä¿®å¾©ï¼[Issue 531](https://code.google.com/p/rimeime/issues/detail?id=531) Windows 8 å¸è¼è¼¸å¥æ³å¾å¨è¼¸å¥æ³åè¡¨ä¸­ææ®çé 
* è®æ´ï¼è¨»åè¼¸å¥æ³æåæåç¨ IMEãTSF æ¨¡å¼

#### Rime ç®æ³åº«è®æ´é

* æ´æ°ï¼librime åç´å° 1.0
* æ¹é²ï¼`affix_segmentor` æ¯æåå¹éå°çä»£ç¢¼æ®µæ·»å æ¨ç±¤ `extra_tags`
* ä¿®å¾©ï¼`table_translator` æå­ç¬¦ééæ¿¾åé¸å­ï¼ä¿®æ­£å° CJK-D æ¼¢å­çå¤æ·

#### ãæ±é¢¨ç ´ãè®æ´é

* åªåï¼ãç²µæ¼ãå¼å®¹[æè²å­¸é¢æ¼é³æ¹æ¡](http://zh.wikipedia.org/wiki/%E6%95%99%E8%82%B2%E5%AD%B8%E9%99%A2%E6%8B%BC%E9%9F%B3%E6%96%B9%E6%A1%88)
* æ´æ°ï¼`symbols.yaml` ç± Patricivs éæ°æ´çç¬¦èè¡¨
* æ´æ°ï¼Emoji æä¾æ´å è±å¯çç¹ªæå­ï¼éè¦å­é«æ¯æï¼
* æ´æ°ï¼ãå«è¡æããæææ¼é³ããå°çæ¼é³ããä¸­å¤å¨æ¼ãä¿®æ­£é¯å¥å­ãè¨»é³é¯èª¤



<a name="0.9.27"></a>
## å°ç¼æ¯« 0.9.27 (2013-11-06)


#### ãå°ç¼æ¯«ãè®æ´é

* è®æ´ï¼åæéæ¥ `rime.dll`ï¼æ¸å°ç¨åºæä»¶çé«ç©
* ä¿®å¾©ï¼åè©¦è§£æ±º Issue 487 é¿åæåé²ç¨ä»¥ SYSTEM å¸³èå·è¡
* æ°å¢ï¼éå§èå®é ãå®è£é¸é ãï¼Vista ä»¥éæç¤ºä»¥ç®¡çå¡æ¬éåå
* åªåï¼æ´æåæ¨ï¼è§£æ±º Windows 8 TSF åæ¨ä¸æ¸æ¥çåé¡

#### Rime ç®æ³åº«è®æ´é

* åªåï¼åæ­¥ç¨æ¶è³ææèªååä»½èªå®ç¾©ç­èªç­ .txt æä»¶
* ä¿®å¾©ï¼ãå°çæ¼é³ãåæ¥æ¼é³å¤±æçåé¡
* è®æ´ï¼ç·¨ç¢¼æç¤ºä¸åæ·»å æ¬å¼§ï¼ï¼ï¼åéèï¼å¯èªè¡è¨­å®æ¨£å¼

#### è¼¸å¥æ¹æ¡è¨­è¨æ¯æ

* æ°å¢ï¼`affix_segmentor` åéç·¨ç¢¼çåç¶´ãå¾ç¶´
* æ¹é²ï¼`translator` æ¯æå¹éæ®µè½æ¨ç±¤
* æ¹é²ï¼`simplifier` æ¯æå¤åå¯¦ä¾ï¼å¹éæ®µè½æ¨ç±¤
* æ°å¢ï¼`switches:` è¼¸å¥æ¹æ¡é¸é æ¯æå¤é¸ä¸
* æ°å¢ï¼`reverse_lookup_filter` ç²åé¸å­æ¨è¨»æå®ç¨®é¡çè¼¸å¥ç¢¼

#### ãæ±é¢¨ç ´ãè®æ´é

* æ´æ°ï¼ãç²µæ¼ãè£åå¤§éå®å­çè¨»é³
* æ´æ°ï¼ãæææ¼é³ããå°çæ¼é³ãå°å¥ Unihan è®é³è³æ
* æ¹é²ï¼ãå°çæ¼é³ããæ³¨é³ãåç¨èªå®ç¾©ç­èª
* æ°å¢ï¼ãæ³¨é³Â·èºç£æ­£é«ã
* ä¿®å¾©ï¼ãæææ¼é³Â·ç°¡åå­ãééå¿«æ·éµ `Control+Shift+4` ç°¡ç¹åæ
* æ¹é²ï¼ãåé ¡äºä»£ãéåç¹ç°¡è½ææï¼æç¤ºç°¡åå­å°æçå³çµ±æ¼¢å­
* è®æ´ï¼ééèæ¡ç¨ãÂ·ã`U+00B7`



<a name="0.9.26.1"></a>
## å°ç¼æ¯« 0.9.26.1 (2013-10-09)

* ä¿®å¾©ï¼å¾ä¸ä¸åçæ¬åç´ãåé ¡ãè¼¸å¥æ¹æ¡ä¸æèªåæ´æ°çåé¡



<a name="0.9.26"></a>
## å°ç¼æ¯« 0.9.26 (2013-10-08)

* æ°å¢ï¼ãåé ¡ãéåèªåé è©<br/>
  é£çºä¸å±ç5å­ï¼ä¾è¨­å®ï¼ä»¥å§ççµåï¼æä»¥é£ææ¹å¼ä¸å±çç­èªï¼
  ææ§è©è¦åè¨æ¶ç²æ°è©çµï¼åæ¬¡è¼¸å¥è©²è©çµçç·¨ç¢¼æï¼é¡¯ç¤ºãâ¯ãæ¨è¨
* è®æ´ï¼ãäºç­ãéåèªåé è©ï¼å¾ç¢¼è¡¨ä¸­åªé¤èä¸ç´ç°¡ç¢¼éç¢¼çéµåå­
* è®æ´ï¼ãå°çæ¼é³ãç¶ä»¥ç°¡æ¼è¼¸å¥æï¼ç²5å­ä»¥å§åé¸æ¨è¨»å®æ´å¸¶èª¿æ¼é³
* æ°å¢ï¼ãäºç­ç«ãè¼¸å¥æ¹æ¡ï¼`stroke`ï¼ï¼åä»£ `stroke_simp`
* æ°å¢ï¼æ¯æå¨è¼¸å¥æ¹æ¡ä¸­è¨­ç½®ä»é¢æ¨£å¼ï¼`style:`ï¼<br/>
  å¦å­é«ãå­èãæ©«æï¼ç´æç­ï¼éè²æ¹æ¡é¤å¤
* ä¿®å¾©ï¼å¤æ¬¡æã.ãéµç¿»é å¾ç¹¼çºè¼¸å¥ï¼ä¸æè¦ç²ç¶²åèå¨ç·¨ç¢¼ä¸­æå¥ã.ã
* ä¿®å¾©ï¼éååé¸å­çå­ç¬¦ééæ¿¾ï¼å°è´ææä¸åºç¾é£æåé¸è©ç BUG
* ä¿®å¾©ï¼`table_translator` é£æçµè©æç¢ççå§å­æ³æ¼ï¼0.9.25.2ï¼
* ä¿®å¾©ï¼ç²ææç¨æ¶åµå»ºéå§èå®é 
* æ´æ°ï¼ä¿®è¨ãå«è¡æãè©å¸ããæææ¼é³ããå°çæ¼é³ããç²µæ¼ããå³èªã
* æ´æ°ï¼2013æ¬¾ Rime è¼¸å¥æ³åæ¨



<a name="0.9.25.2"></a>
## å°ç¼æ¯« 0.9.25.2 (2013-07-26)

* æ¹é²ï¼ç¢¼è¡¨è¼¸å¥æ³é£æï¼Shift+BackSpace ä»¥å­ãè©ç²å®ä½åé
* ä¿®å¾©ï¼æ¼ç¤ºæ¨¡å¼ä¸éåå§åµç·¨ç¢¼è¡ãæ¥ç¡åé¸å­æç¨åºå¡æ­»



<a name="0.9.25.1"></a>
## å°ç¼æ¯« 0.9.25.1 (2013-07-25)

* æ°å¢ï¼éå§èå®é ãæª¢æ¥æ°çæ¬ãï¼æååç´å°ææ°æ¸¬è©¦ç
* æ°å¢ï¼ãå°çæ¼é³ã5 å­å§åé¸æ¨è¨»å®æ´å¸¶èª¿æ¼é³



<a name="0.9.25"></a>
## å°ç¼æ¯« 0.9.25 (2013-07-24)

* æ°å¢ï¼æ¼ç¤ºæ¨¡å¼ï¼å¨å±çè¼¸å¥çªå£ï¼`style/fullscreen: true`
* æ°å¢ï¼ãåé ¡ãæå¿«è¶£åç¢¼è¦åçæå¸¸ç¨è©çµ
* ä¿®å¾©ï¼ãå°çæ¼é³ãã-ãéµè¼¸å¥ç¬¬ä¸è²å¤±æçBUG
* æ´æ°ï¼æ¼é³ãç²µæ¼ç­è¼¸å¥æ¹æ¡
* æ´æ°ï¼`symbols.yaml` å¢å ä¸æ¹ç¹æ®å­ç¬¦



<a name="0.9.24"></a>
## å°ç¼æ¯« 0.9.24 (2013-07-04)

* æ°å¢ï¼æ¯æå¨è§æ¨¡å¼
* æ´æ°ï¼ä¸­å¤æ¼¢èªãå¨æ¼ããä¸æ¼ãè¼¸å¥æ¹æ¡ï¼ä¸æ¼äº¦æ¡ç¨å¨æ¼è©å¸
* ä¿®å¾©ï¼å¤§é¸èèºç£ç°è®çå­ãå¾®ããæªããè¸ããåå¾ãç­
* ä¿®å¾©ï¼ç¹ç°¡è½æé¯è©ãä¹ä¹åã
* æ°å¢ï¼ï¼è¼¸å¥æ¹æ¡è¨­è¨ç¨ï¼å¯è¨­å®å°ç¹å®é¡åçåé¸è©ä¸åç¹ç°¡è½æ<br/>
  å¦ä¸è½æåæ¥å­ä½¿ç¨é¸é  `simplifier/excluded_types: [ reverse_lookup ]`
* æ°å¢ï¼ï¼è¼¸å¥æ¹æ¡è¨­è¨ç¨ï¼å¹²é å¤å translator ä¹éççµææåº<br/>
  é¸é  `translator/initial_quality: 0`
* ä¿®å¾©ï¼ç¨æ¶è©å¸æªè½å®æ´æ¯æ `derive` æ¼å¯«éç®ç¢ççæ­§ç¾©åå



<a name="0.9.23"></a>
## å°ç¼æ¯« 0.9.23 (2013-06-09)

* æ¹é²ï¼æ¹æ¡é¸å®æé¸ç¨è¼¸å¥æ¹æ¡çæéæå
* æ°å¢ï¼å¿«æ·éµ Control+Shift+1 åæè³ä¸ä¸åè¼¸å¥æ¹æ¡
* æ°å¢ï¼å¿«æ·éµ Control+Shift+2~5 åæè¼¸å¥æ¨¡å¼
* æ°å¢ï¼åæ¬¡å®è£æç±ç¨æ¶æå®è¼¸å¥èªè¨ï¼ä¸­æï¼ä¸­åï¼èºç£ï¼
* æ°å¢ï¼å¯å±è½ç¬¦å fuzz æ¼å¯«è¦åçå®å­åé¸ï¼åä»¥å¶è¼¸å¥è©çµ<br/>
  é¸é  `translator/strict_spelling: true`
* æ¹é²ï¼ç¶ååé¸è©çè©é »åè©æ¢è³ªéæ¯è¼ä¸å translator ççµæ
* ä¿®å¾©ï¼èªå®ç¾©ç­èªä¸æåèçµè©
* ä¿®å¾©ï¼å«è¡æé¯è©åãéãå­ç¡æ³ä»¥ç°¡åå­çµè©ç BUG



<a name="0.9.22.1"></a>
## å°ç¼æ¯« 0.9.22.1 (2013-04-24)

* ä¿®å¾©ï¼ç¦æ­¢èªå®ç¾©ç­èªåèé å¥
* ä¿®å¾©ï¼GVim è£é²å¥å½ä»¤æ¨¡å¼æå¨æå¥æ¨¡å¼æè¡é¯ä½¿è¼¸å¥æ³éç½®ç²åå§çæ



<a name="0.9.22"></a>
## å°ç¼æ¯« 0.9.22 (2013-04-23)

* æ°å¢ï¼éè²æ¹æ¡ãæ¬ç¶ç³ãï¼Solarized Rock
* æ°å¢ï¼Control+BackSpace æ Shift+BackSpace åéä¸åé³ç¯
* æ°å¢ï¼åºæè©å¸å¯å¼ç¨å¤ä»½ç¢¼è¡¨æä»¶ä»¥å¯¦ç¾åé¡è©åº«
* æ°å¢ï¼å¨è¼¸å¥æ¹æ¡ä¸­å è¼ç¿»è­¯å¨çå¤åå·åå¯¦ä¾
* æ°å¢ï¼ä»¥é¸é  `translator/user_dict:` æå®ç¨æ¶è©å¸çåç¨±
* æ°å¢ï¼æ¯æå¾ç¨æ¶æä»¶å¤¾å è¼ææ¬ç¢¼è¡¨ä½ç²èªå®ç¾©ç­èªè©å¸<br/>
  ãæææ¼é³ãç³»åèªåå è¼åç² `custom_phrase.txt` çç¢¼è¡¨
* ä¿®å¾©ï¼ç¹ç°¡è½æä½¿ç¡éç¢¼èªåä¸å±å¤±æç BUG
* ä¿®å¾©ï¼è¥éä»¥ Caps Lock éµé²å¥è¥¿ææ¨¡å¼ï¼<br/>
  æ Caps Lock åªåæå¤§å°å¯«ï¼ä¸è¿åä¸­ææ¨¡å¼
* è®æ´ï¼`r10n_translator` æ´åç² `script_translator`ï¼èåç¨±ä»å¯ä½¿ç¨
* è®æ´ï¼ç¨æ¶è©å¸å¿«ç§æ¹ç²ææ¬æ ¼å¼
* æ¹é²ï¼ãå«è¡æãå°å¥ãèå¸ãè©å½ï¼ä¸¦ä¿®æ­£äºä¸å°é¯è©
* æ¹é²ï¼ãåé ¡äºä»£ãæå®å­æï¼ä»¥æä¸å­æ¯ååé ¡å­æ¯ä¸¦åé¡¯ç¤ºè¼¸å¥ç¢¼
* æ¹é²ï¼ä½¿èªåçæç YAML ææªæ´åçå°ç¸®æãæ¹ä¾¿é±è®
* æ¹é²ï¼ç¢¼è¡¨ä¸­ `# no comments` è¡ä¹å¾ä¸åè­å¥è¨»éï¼ä»¥æ¯æ `#` ä½æå­å§å®¹
* æ¹é²ï¼æª¢æ¸¬å°å æ·é»é æç¨æ¶è©å¸æå£æï¼èªåå¨å¾èºç·ç¨æ¢å¾©æ¸ææä»¶



<a name="0.9.20"></a>
## å°ç¼æ¯« 0.9.20 (2013-02-01)

* è®æ´ï¼Caps Lock çäº®æé»èªè¼¸åºå¤§å¯«å­æ¯ [Gist](https://gist.github.com/2981316)
  åç´å®è£å¾è¥ Caps Lock çè¡¨ç¾ä¸æ­£ç¢ºï¼è«è¨»é·ä¸¦éæ°ç»é
* æ°å¢ï¼ç¡éç¢¼èªåä¸å± `speller/auto_select:`<br/>
  è¼¸å¥æ¹æ¡ãåé ¡Â·å¿«ææ¨¡å¼ã
* æ¹é²ï¼åè¨±ä»¥ç©ºæ ¼åè¼¸å¥ç¢¼ï¼æä½ç²ç¬¦èé å­ä¸å±<br/>
  `speller/use_space:`, `punctuator/use_space:`
* æ¹é²ï¼ãæ³¨é³ãè¼¸å¥æ¹æ¡ä»¥ç©ºæ ¼è¼¸å¥ç¬¬ä¸è²ï¼é°å¹³ï¼
* æ°å¢ï¼ç¹æ®ç¬¦èè¡¨ `symbols.yaml` ç¨æ³è¦â
* æ¹é²ï¼ãæææ¼é³Â·ç°¡åå­ãä»¥ `/ts` ç­å½¢å¼è¼¸å¥ç¹æ®ç¬¦è
* æ¹é²ï¼æ¨é»ç¬¦èè¨»æãå¨è§ããåè§ã
* åªåï¼åæ­¥ç¨æ¶è³æææ´è°æå°åä»½ç¨æ¶èªå®ç¾©ç YAML æä»¶
* ä¿®å¾©ï¼é¿ååµå»ºãä½¿ç¨ä¸å®æ´çè©å¸æä»¶
* ä¿®å¾©ï¼ç³¾æ­£ç¨æ¶è©å¸ä¸­ç¡æ³èª¿é »çåæè©æ¢
* ä¿®å¾©ï¼ç¨æ¶è©å¸ç®¡çï¼è¼¸åºè©å¸å¿«ç§å¾å®ä½æä»¶åºé¯
* ä¿®å¾©ï¼TSF å§åµè¼¸å¥ç¢¼æ²æåé¸ææãåé¸çªä½ç½®é »ç¹è®å



<a name="0.9.19.1"></a>
## å°ç¼æ¯« 0.9.19.1 (2013-01-16)

* æ°å¢ï¼Caps Lock é»äº®æï¼åæå°è¥¿ææ¨¡å¼ï¼è¼¸åºå°å¯«å­æ¯<br/>
  é¸é  `ascii_composer/switch_key/Caps_Lock:`
* ä¿®å¾©ï¼Control + å­æ¯ç¼è¾é®å¨ä¸´æ¶è¥¿ææ¨¡å¼ä¸æ æ
* ä¿®å¾©ï¼ç¨æ¶è©å¸æå¯è½å è®åæ I/O é¯èª¤å°è´é¨ä»½è©åºç¡æ³èª¿æ´
* æ¹é²ï¼ç¨æ¶è©å¸åæ­¥ï¼åå¥å¿«ç§çå­é »åä½µç®æ³



<a name="0.9.18.6"></a>
## å°ç¼æ¯« 0.9.18.6 (2013-01-09)

* ä¿®å¾©ï¼å¾ 0.9.16 åä»¥ä¸çæ¬åç´ç¨æ¶è©å¸åºé¯



<a name="0.9.18.5"></a>
## å°ç¼æ¯« 0.9.18.5 (2013-01-07)

* ä¿®å¾©ï¼å«ç°¡åå­çåé¸è©ä¸è½ä»¥é³ç¯ç²å®ä½ç§»ååæ¨
* æ¹é²ï¼åæ­¥ç¨æ¶è³ææä¹åä»½ç¨æ¶ä¿®æ¹çYAMLæä»¶



<a name="0.9.18"></a>
## å°ç¼æ¯« 0.9.18 (2013-01-05)

* æ°å¢ï¼åæ­¥ç¨æ¶è©å¸ï¼è©³è¦ [Wiki Â» UserGuide](https://code.google.com/p/rimeime/wiki/UserGuide)
* æ°å¢ï¼ä¸å±é¯èª¤çè©çµå¾ç«å³æåééµï¼BackSpaceï¼æ¤é·çµè©
* æ¹é²ï¼æ¼é³è¼¸å¥æ³ä¸­ï¼æå·¦æ¹åéµä»¥é³ç¯ç²å®ä½ç§»ååæ¨
* ä¿®å¾©ï¼ãå°çæ¼é³ãä¸è½ä»¥ - éµè¼¸å¥ç¬¬ä¸è²



<a name="0.9.17.1"></a>
## å°ç¼æ¯« 0.9.17.1 (2012-12-25)

* ä¿®å¾©ï¼è¨­ç½®ç²é»èªè¼¸å¥èªè¨å¾åå®è£ï¼IME è¨»åå¤±æ
* ä¿®å¾©ï¼åç¨æç¤åæ¨çé¸é ç¡æ
* æ°å¢ï¼å¾éå§èå®è¨ªåç¨æ¶æä»¶å¤¾çå¿«æ·æ¹å¼
* ä¿®å¾©ï¼ãå°é¶´éæ¼ãæ¼é³ an é¡¯ç¤ºé¯èª¤



<a name="0.9.17"></a>
## å°ç¼æ¯« 0.9.17 (2012-12-23)

* æ°å¢ï¼åææ¨¡å¼ãè¼¸å¥æ¹æ¡æï¼ç­æ«é¡¯ç¤ºçæåæ¨
* æ°å¢ï¼é±èæç¤åæ¨ï¼è¨­å®ãé¨ç½²ãè©å¸ç®¡çè«ç¨éå§èå®ã<br/>
  éç½®é  `style/display_tray_icon:`
* ä¿®å¾©BUGï¼TSF åç«¯å¨ MS Office è£ä¸è½æ­£å¸¸ä¸å±ä¸­æ
* åªé¤ï¼é»èªä¸åç¨ TSF åç«¯ï¼å¦æéè¦å¯å¨ãææ¬æåèè¼¸å¥èªè¨ãè¨­ç½®å°è©±æ¡æ·»å ã
* æ°å¢ï¼åå¥ä»¥ `` ` ' `` æ¨èªç·¨ç¢¼åæ¥çéå§çµæï¼ä¾å¦ `` `wbb'yuepinyin ``
* æ¹é²ï¼å½¢ç¢¼èæ¼é³æ··æçè¨­å®ä¸ï¼éä½ç°¡æ¼åé¸çåªåç´ï¼ä»¥éä½å°ééµæç¤ºçå¹²æ¾
* åªåï¼æ§å¶ç¨æ¶è©å¸æä»¶å¤§å°ï¼æé«å¤§å®¹éï¼è©æ¢æ¸>100,000ï¼æçæ¥è©¢éåº¦
* åªé¤ï¼å æç¨å®¶åç¨æ¶è©å¸å°å¥å·¨éè©æ¢ï¼æåæ¶èªååä»½çåè½ï¼å¾çºä»£ä¹ä»¥ç¨æ¶è©å¸åæ­¥
* ä¿®å¾©ï¼ãå°é¶´éæ¼ãdiao, tiao ç­æ¼é³åé¡¯é¯èª¤
* æ´æ°ï¼ãæææ¼é³ããå°çæ¼é³ããç²µæ¼ãä¿®æ­£ç¨æ¶åé¥çè¨»é³é¯èª¤



<a name="0.9.16"></a>
## å°ç¼æ¯« 0.9.16 (2012-10-20)

* æ°å¢ï¼TSF è¼¸å¥æ³æ¡æ¶ï¼æ¸¬è©¦éæ®µï¼ååµå¥å¼ç·¨ç¢¼è¡
* æ°å¢ï¼æ¯æ IE 8 ~ 10 çãä¿è­·æ¨¡å¼ã
* æ°å¢ï¼è­å¥ gVim æ¨¡å¼åæ
* æ°å¢ï¼ééç¢¼è¡¨è¼¸å¥æ³é£æåè½çè¨­å®é  `translator/enable_sentence: `
* ä¿®å¾©ï¼ãèªå¥æµãæ¨¡å¼ç´æ¥åè»ä¸å±ä¸è½è¨æ¶ç¨æ¶è©çµçBUG
* æ¹é²ï¼é¨ç½²æèªåç·¨è­¯è¼¸å¥æ¹æ¡çèªè¨ä¾è³´é ï¼å¦èªé¸çåæ¥ç¢¼
* æ¹é²ï¼æ´ç²¾ç´°çæçï¼ä¿®æ­£è¨»éæå­å¯¬åº¦ãèª¿æ´éè·
* æ¹é²ï¼æªæ¾ç¿»é æææ¸èéµï¼ä¸ä¸å±åé¸å­åç¬¦èã-ãä»¥åèª¤æä½
* è®æ´ï¼ãæ³¨é³ãä»¥éèæå¥èï¼<> éµï¼ä¸å±å¥å­ï¼æ¸åèæ¹ç¨ [] éµ
* æ´æ°ï¼ãæææ¼é³ããå°çæ¼é³ããç²µæ¼ãï¼ä¿®æ­£å¤é³å­
* æ´æ°ï¼ãä¸æµ·å³èªããä¸æµ·æ°æ´¾ãï¼ä¿®æ­£è¨»é³
* æ°å¢ï¼å¯å¯è±ä½ãèå·å³èªãè¼¸å¥æ¹æ¡ï¼æ¹æ¡æ¨è­ç² `soutzoe`
* æ°å¢ï¼éè²æ¹æ¡ãè°·æ­ï¼Googleãï¼skoj ä½å



<a name="0.9.15"></a>
## å°ç¼æ¯« 0.9.15 (2012-09-12)

* æ°å¢ï¼æ©«æåé¸æ¬ââæ­¡è¿ wishstudio åå­¸å å¥éç¼ï¼
* æ°å¢ï¼ç¶ è²å®è£å·¥å· WeaselSetupï¼è¨»åè¼¸å¥èªè¨ãèªè¨ç¨æ¶ç®é
* æ°å¢ï¼ç¢¼è¡¨è¼¸å¥æ³åç¨ç¨æ¶è©å¸ãå­é »èª¿æ´
* åªåï¼èªåç·¨è­¯è¼¸å¥æ¹æ¡ä¾è³´é ï¼å¦äºç­Â·æ¼é³çåæ¥è©å¸
* ä¿®æ¹ï¼æ¥èªç³»çµ±æ¹ç¨glogï¼è¼¸åºå° `%TEMP%\rime.weasel.*`
* ä¿®å¾©ï¼æç¤åæ¨å¨éæ°ç»éå¾ä¸å¯è¦çBUG
* æ´æ°ï¼ãæææ¼é³ããç²µæ¼ããå³èªãä¿®æ­£è¨»é³é¯èª¤ãç¼ºå­



<a name="0.9.14.2"></a>
## å°ç¼æ¯« 0.9.14.2 (2012-07-13)

* éæ°ç·¨è­¯äº `opencc.dll` å®å¨è»ä»¶ä¸å­æ°£äº



<a name="0.9.14.1"></a>
## å°ç¼æ¯« 0.9.14.1 (2012-07-07)

* è§£æ±ºãä¸­å¤å¨æ¼ãä¸å¯ç¨çåé¡



<a name="0.9.14"></a>
## å°ç¼æ¯« 0.9.14 (2012-07-05)

* ä»é¢æ¡ç¨æ°ç Rime logoï¼çæåç¤ºç¨è¼æåçé¡è²
* æ°ç¹æ§ï¼ç¢¼è¡¨æ¹æ¡æ¯æèåæ¥ç¢¼æ··åè¼¸å¥ï¼ç¡éåææå¼å°éµ
* æ°ç¹æ§ï¼ç¢¼è¡¨æ¹æ¡å¯å¨é¸å®ä¸­ä½¿ç¨å­ç¬¦ééæ¿¾éé
* æ°æ¹æ¡ï¼ãäºç­86ãè¡ççãäºç­Â·æ¼é³ãæ··åè¼¸å¥
* æ°æ¹æ¡ï¼ãå»£é»ãé³ç³»çä¸­å¤æ¼¢èªå¨æ¼ãä¸æ¼è¼¸å¥æ³
* æ°æ¹æ¡ï¼X-SAMPA åéé³æ¨è¼¸å¥æ³
* æ´æ°ï¼ãå³èªãç¢¼è¡¨ï¼å¯©å®ä¸äºå­è©çè®é³ï¼çµ±ä¸å­å½¢
* æ´æ°ï¼ãæææ¼é³ãç¢¼è¡¨ï¼ä¿®æ­£å¤é³å­
* æ¹é²ï¼ç¶åè¨­å®çå­é«ç¼ºå­æï¼ä½¿ç¨ç³»çµ±å¾åå­é«é¡¯ç¤ºæå­
* è§£æ±ºèMacTypeåæä½¿ç¨ï¼Ext-B/C/Dåæå­æçä¸æ­£ç¢ºçåé¡



<a name="0.9.13"></a>
## å°ç¼æ¯« 0.9.13 (2012-06-10)

* ç·¨ç¢¼æç¤ºç¨æ·¡å¢¨ä¾å¯«ï¼äº¦å¯å¨éè²æ¹æ¡ä¸­è¨­å®é¡è²
* æ°å¢å¤éµä¸¦æçµä»¶åè¼¸å¥æ¹æ¡ãå®®ä¿æ¼é³ã
* æªç¶è½æçè¼¸å¥å¦ç¶²åç­ä¸åé¡¯ç¤ºç²åé¸é 
* `default.custom.yaml`: `menu/page_size:` è¨­å®å¨å±é åé¸æ¸
* æ°å¢é¸é ï¼å°å¥ãå«è¡æãè©åº«æéå¶è©èªçé·åº¦ãè©é »
* ãåé ¡ãæ¯æé£çºè¼¸å¥å¤åå­çç·¨ç¢¼ï¼éæ®µææï¼ä¸æè¨æ¶è©çµï¼
* ãæ³¨é³ãæ¹ç²èªå¥è¼¸å¥é¢¨æ ¼ï¼æ´æ¥è¿èºç£ç¨æ¶çç¿æ£
* è¼å°ç¨çãç­é äºç¢¼ãããéè¨æå­æ³ãä¸åé¨é¼ é¬ç®¡ç¼è¡
* ä¿®å¾©ãç¨æ¶è©å¸ç®¡çãå°å¥ææ¬ç¢¼è¡¨ä¸çæçBUGï¼<br/>
  é¨ç½²ææª¢æ¥ä¸¦ä¿®å¾©å·²å­å¨æ¼ç¨æ¶è©å¸ä¸­çç¡ææ¢ç®
* æª¢æ¸¬å°ç¨æ¶è©å¸æä»¶æå£æï¼éå»ºè©å¸ä¸¦å¾åä»½ä¸­æ¢å¾©è³æ
* ä¿®æ¹BUGï¼ç°¡æ¼ zhzh å ååæ­§ç¾©ä½¿é¨åç¨æ¶è©å¤±æ



<a name="0.9.12"></a>
## å°ç¼æ¯« 0.9.12 (2012-05-05)

* ç¨ Shift+Del åªé¤å·²è¨å¥ç¨æ¶è©å¸çè©æ¢ï¼è©³è¦ Issue 117
* å¯é¸ç¨ShiftæControlç²ä¸­è¥¿æåæéµï¼è©³è¦ Issue 133
* æ¸å­å¾çå¥èéµè­å¥ç²å°æ¸é»ãåèéµè­å¥ç²æåç§åéç¬¦
* è§£æ±ºå¨QQç­æç¨ç¨åºä¸­çå®ä½åé¡
* æ¯æè¨­ç½®ç²ç³»çµ±é»èªè¼¸å¥æ³
* æ¯æå¤åWindowsç¨æ¶ï¼æ°ç¨æ¶å·è¡ä¸æ¬¡ä½ç½²å¾æ¹å¯ä½¿ç¨ï¼



<a name="0.9.11"></a>
## å°ç¼æ¯« 0.9.11 (2012-04-14)

* ä½¿ç¨ `express_editor` çè¼¸å¥æ¹æ¡ä¸­ï¼æ¸å­ãç¬¦èéµç´æ¥ä¸å±
* åªåãæ¹æ¡é¸å®ãå¿«æ·éµæä½ï¼é£çºæéµé¸ä¸­ä¸ä¸åè¼¸å¥æ¹æ¡
* è¼¸å¥ç°¡æ¼ãæ¨¡ç³é³ææç¤ºæ­£é³ï¼ãç²µæ¼ããå³èªãä¸­é»èªéå
* æ¼é³åæ¥æ¯æé è¨­çå¤é³ç¯è©ãå½¢ç¢¼åæ¥å¯éåç·¨ç¢¼è£å¨
* ä¿®å¾©æ´å¥æ¨¡å¼éç¨å®é·ç·¨ç¢¼é å­åè½å°è´å´©æ½°çåé¡
* ä¿®å¾©ç¢¼è¡¨è¼¸å¥æ³åé¸æåºåé¡
* ä¿®å¾©ãæææ¼é³ãloãyo ç­é³ç¯çåé¸é¯èª¤
* ä¿®å¾©ãå°çæ¼é³ãè²èª¿é¡¯ç¤ºä¸æ­£ç¢ºãé¨åå­çè¨»é³ç¼ºå¤±åé¡
* ãäºç¬86ãåæ¥å¼å°éµæ¹ç² zãåæ¥è©å¸æç¨ç°¡åå­æ¼é³
* æ´æ°ãç²µæ¼ãè©å¸ï¼èª¿æ´å¸¸ç¨ç²µå­çæåºãå¢è£ç²µèªå¸¸ç¨è©
* æ°å¢è¼¸å¥æ¹æ¡ãå°é¶´éæ¼ãããç­é äºç¢¼ã



<a name="0.9.10"></a>
## å°ç¼æ¯« 0.9.10 (2012-03-26)

* è¨æ¶ç¹ç°¡è½æãå¨ï¼åè§ç¬¦èééçæ
* æ¯æå®é·ç·¨ç¢¼é å­ä¸å±
* æ°å¢ãç¨æ¶è©å¸ç®¡çãä»é¢
* å»¶é²å è¼ç¹ç°¡è½æãç·¨ç¢¼åæ¥è©å¸ï¼éä½è³æºä½ç¨
* ç´å®å­æ§è©æä¸èª¿é »
* æ°å¢è¼¸å¥æ¹æ¡ãéæãï¼éæãåé ¡è©å¥é£æ
* æ°å¢ãæºè½ABCéæ¼ãããéè¨æå­æ³ã



<a name="0.9.9"></a>
## å°ç¼æ¯« 0.9.9

* æ°å¢ãä»é¢é¢¨æ ¼è¨­å®ãï¼å¿«éé¸æé è¨­çå­æ¬¾éè²æ¹æ¡
* åªåé·å¥ä¸­å­è©çåæèª¿é »
* æ°å¢ãæ³¨é³ãèãå°çæ¼é³ãè¼¸å¥æ¹æ¡
* æ¯æèªè¨é¸è©æéµ
* ä¿®å¾©ç·¨ç¢¼åæ¥å¤±æçBUG
* ä¿®æ¹æ¨é»ç¬¦èãééèãåãæµªç´ã



<a name="0.9.8"></a>
## å°ç¼æ¯« 0.9.8

* æ°å¢ãè¼¸å¥æ¹æ¡é¸å®ãè¨­å®ä»é¢
* åªååå«ç°¡æ¼çé³ç¯åå
* ä¿®å¾©é¨åç¨æ¶çµè©ç¡æçBUG
* æ°å¢é è¨­è¼¸å¥æ¹æ¡ãMSPYéæ¼ã



<a name="0.9.7"></a>
## å°ç¼æ¯« 0.9.7

* ééµæç¤ºãåæ¥æç¤ºç¢¼æ¯ææ¼å¯«éç®ï¼å¦é¡¯ç¤ºåé ¡å­æ¯ç­ï¼
* éæ§é¨ç½²å·¥å·ï¼ä»¥ `*.custom.yaml` æä»¶æä¹ä¿å­èªå®ç¾©è¨­ç½®
* è£½ä½ãç²µæ¼ãããå³èªãè¼¸å¥æ¹æ¡ãé ç¼è¡çã



<a name="0.9.6"></a>
## å°ç¼æ¯« 0.9.6

* éæ©æå¦¥åä¿å­æ¸æï¼éä½ç¨æ¶è©åº«æå£æ©çï¼å·è¡å®æåä»½
* æ°å¢åºæ¼ãæææ¼é³ãçè¡çæ¹æ¡ï¼
  * ãèªå¥æµãï¼æ´å¥è¼¸å¥ï¼ç©ºæ ¼åè©ï¼åè»ä¸å±
  * ãéæ¼ãï¼å¼å®¹èªç¶ç¢¼éæ¼æ¹æ¡ï¼æ¼ç¤ºæ¼å¯«éç®å¸¸ç¨æå·§
* ä¿®å¾©BUGï¼ç°¡æ¼ãz h, c h, s hãçè©åé¸åæ¼å®å­ç°¡æ¼
* ä¿®å¾©BUGï¼ãæ¼å¯«éç®ãç¡æ³æ¿æç²ç©ºä¸²
* å®åæ¼å¯«éç®çé¯èª¤æ¥èªï¼æ¸çèª¿è©¦æ¥èª



<a name="0.9.5"></a>
## å°ç¼æ¯« 0.9.5

* Rime ç¨éçµæ´»ä¹ãæ¼å¯«éç®ã
* åç´ãæææ¼é³ãï¼æ¯æç°¡æ¼ãç³¾é¯ï¼å¢è¨­ãç°¡åå­ãæ¹æ¡
* åç´ãåé ¡äºä»£ãï¼ä»¥åé ¡å­æ¯é¡¯ç¤ºç·¨ç¢¼
* éä¿®éè²æ¹æ¡ãç¢§æ°´ï¼Aquaãããéå¤©ï¼Azureã



<a name="0.9.4"></a>
## å°ç¼æ¯« 0.9.4

* å¢è¨­ç·¨ç¢¼åæ¥åè½ï¼é è¨­æ¹æ¡ä»¥ã`ãç²åæ¥çå¼å°éµ
* ä¿®å¾©Windows XPä¸­è¥¿æçæè®æ´æçéç¥æ°£ç



<a name="0.9.3"></a>
## å°ç¼æ¯« 0.9.3

* æ°å¢é è¨­è¼¸å¥æ¹æ¡ãäºç¬86ãããèºç£æ­£é«ãæ¼é³
* ä»¥æç¤åæ¨è¡¨ç¾è¼¸å¥æ³çæè®æ´
* æ°å¢è¼¸å¥æ³ç¶­è­·æ¨¡å¼ï¼æ´å®å¨å°é²è¡é¨ç½²ä½æ¥­
* åªåä¸­è¥¿æåæãèªåè­å¥å°æ¸ãç¾åæ¸ãç¶²åãéµç®±



<a name="0.9.2"></a>
## å°ç¼æ¯« 0.9.2

* å¢è¨­åè§æ¨é»ç¬¦è
* å¢è¨­Shiftéµåæä¸­ï¼è¥¿ææ¨¡å¼
* ç¹ç°¡è½æãå·¦Shiftåæä¸­è¥¿æå°ç¶åè¼¸å¥å³æçæ
* å¯èªå®ç¾©OpenCCç°é«å­è½æå­å¸
* æåç¢¼è¡¨æ¥è©¢æçï¼æ´æ°åé ¡ä¸è¬å­ç¢¼è¡¨
* å¢è¨­æç¤åæ¨ï¼å¿«éè¨ªåéç½®ç®¡çå·¥å·
* æ¹é²å®è£ç¨åº



<a name="0.9"></a>
## å°ç¼æ¯« 0.9

* ç¨C++éå¯«æ ¸å¿ç®æ³ï¼éæ®µææï¼
* å°è¼¸å¥æ³ä»é¢å¾åç«¯é·ç§»å°å¾èºæåé²ç¨
* å¼å®¹64ä½ç³»çµ±



## å°ç¼æ¯« 0.1 ~ 0.3

* ä»¥Pythonéç¼çå¯¦é©çæ¬
* ç¨åµãæ¼å¯«éç®ãæè¡
* é è£æ¨èª¿æ¼é³ãè¨»é³ãç²µæ¼ãå³èªç­å¤ç¨®è¼¸å¥æ¹æ¡

