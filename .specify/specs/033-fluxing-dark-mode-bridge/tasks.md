# 033 - Tasks - FluxingDarkModeBridge extraction

> Atomic task list. Each T### is 1-3 files and <4h. Run in order
> T001 -> T009. T005-T007 may be done in any order within the
> testing phase (the vcxproj + sln + test-infra edits are
> independent files).

## Phase 1 - production code (T001-T003)

- [ ] T001 [P1] [US-cross] Create `RimeWithWeasel/FluxingDarkModeBridge.h`
      with the singleton + Subscriber + Palette API. UTF-8, no BOM, CRLF.
      Verify: `git hash-object` matches a known-clean CRLF SHA; no
      C0/C1 overlong; only legitimate `?` (the comment in section 2.3
      of plan.md has none; this file has none).

- [ ] T002 [P1] [US-cross] Create `RimeWithWeasel/FluxingDarkModeBridge.cpp`
      with the implementation per plan.md section 2. UTF-8, no BOM, CRLF.
      Verify: `wc -l` < 150 (small file), all 6 API methods present.

- [ ] T003 [P1] Add `FluxingDarkModeBridge.cpp` to
      `RimeWithWeasel/xmake.lua`. Edit the `add_files` line. Do
      **not** add it to `RimeWithWeasel.vcxproj` yet (the test
      project at T005 links the .cpp directly, and the vcxproj
      rebuilds the entire RimeWithWeasel.lib which we do not need
      for behavior-level tests; the production binary picks up
      the new .obj via xmake on the next `xbuild.bat weasel`).

## Phase 2 - WeaselPanel refactor (T004)

- [ ] T004 [P1] Refactor `WeaselUI/WeaselPanel.cpp`:
      - Replace OnSettingChange body (3 lines now).
      - Replace _RefreshStylePalette body (use FluxingDarkModeBridge).
      - Keep IsUserDarkMode() defined for now (other callers may
        use it; grep before deletion).
      - Update WeaselUI/WeaselPanel.h with new include if needed.
      - Verify: TestPanelDarkModeSubscribe still passes (3/3).
        It tests a mirror, so this refactor must not change its
        result.

## Phase 3 - test (T005-T006)

- [ ] T005 [P1] Create `test/TestDarkModeBridge/`:
      - `TestDarkModeBridge.cpp` with 5+ behavior-level assertions
        (per plan.md section 2.4.3 / spec.md section 4.3):
        T1: IsDarkMode reads HKCU
        T2: Refresh fires subscribers exactly once
        T3: Subscribe + Unsubscribe ordering (LIFO)
        T4: CurrentPalette returns correct dark bytes
        T5: CurrentPalette returns correct light bytes
        T6: SetDarkForTest toggles IsDarkMode without HKCU
        T7: No-thread-leak on rapid Subscribe/Unsubscribe
        T8: Multiple subscribers all called in one Refresh
      - `TestDarkModeBridge.vcxproj` (L31 fix template: OutDir with
        backslash; 6 platform configurations per spec 029 L36).
      - `stdafx.h`, `stdafx.cpp`, `targetver.h` (boilerplate).
      - Link the production code by direct ClCompile entry of
        `RimeWithWeasel/FluxingDarkModeBridge.cpp` (L24 pattern).

- [ ] T006 [P1] Register the test:
      - Add `TestDarkModeBridge` Project block to `weasel.sln`
        with 6 platform configurations. Generate a new GUID
        (use `[guid]::NewGuid()` in PowerShell, format
        `XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX`).
      - Add `TestDarkModeBridge` to
        `scripts/test-infra/run-test-suite.bat` build + run loops.
      - Update `scripts/test-infra/verify-test-binaries-fresh.bat`
        list of test projects (it iterates the same 12).

## Phase 4 - verify (T007-T008)

- [ ] T007 [P1] Build + run all tests:
      - `cmd /c scripts\test-infra\run-test-suite.bat` -> RC 0,
        `=== ALL TESTS PASSED ===`, 12/12 test projects.
      - `cmd /c scripts\test-infra\verify-test-binaries-fresh.bat`
        -> RC 0, 12 FRESH.
      - TestDarkModeBridge reports 5+/5+ assertions PASS.

- [ ] T008 [P1] Smoke test on the new installer:
      - Build via `xbuild.bat weasel installer` (xmake path).
      - Run AGENTS.md section 2.5 recipe with L41 corrections
        (D: drive path, unquoted /D=).
      - All 8 invariants pass: layout, registry, rime.dll size,
        prebuilt dicts, PE arch (L14: Weasel*.exe x86,
        weaselx64.dll x64, rime.dll x86).
      - Cleanup via uninstall.exe /S.

## Phase 5 - commit + release (T009)

- [ ] T009 [P1] Commit + tag + push:
      - Bump env.bat + weasel.props to 0.18.20.0 (local-only,
        not committed per A3).
      - Add CHANGELOG.md entry for 0.18.20.0 (transliterated
        per spec 032 template, ASCII-only per L01).
      - Commit 1: `feat(fluxing): spec 033 - FluxingDarkModeBridge
        (F11 cross-cut foundation)` - production code + WeaselPanel
        refactor + test + sln + scripts changes.
      - Commit 2: `chore(release): 0.18.20.0 - spec 033 + L40/L41
        stable` - CHANGELOG + new release/fluxing-0.18.20.0-installer.exe.
      - Tag v0.18.20.0 lightweight, push to kizemo/Fluxing.

## Anti-patterns to avoid (per L36 audit pattern)

- [ ] AP-033-A: do not copy the inline palette constants into the
      test; the test links the production .cpp.
- [ ] AP-033-B: do not omit `KEY_WOW64_64KEY` on the registry read.
- [ ] AP-033-C: do not subscribe inside a callback (reentrancy).
- [ ] AP-033-D: do not use raw `static`; use function-local static
      with C++17 thread-safe init.
- [ ] AP-033-E: do not omit the `OutDir` backslash in the new
      vcxproj; L31 was a recurring bug.
- [ ] AP-033-F: do not `git add .`; stage by path per A10.
- [ ] AP-033-G: do not commit env.bat or weasel.props per A3.
- [ ] AP-033-H: do not push to `origin` (use `kizemo` per P8).
