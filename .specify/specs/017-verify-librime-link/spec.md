# 017 - Verify librime link path for TestBindingResolution (build-precondition)

> Scope: confirm that librime 1.13.1 import library (`rime.lib`) is
> buildable + linkable from the existing build pipeline, and wire the
> include / lib paths into TestBindingResolution.vcxproj so spec 018+
> can implement real assertions without re-deriving the link setup.
>
> This spec does NOT implement real TestBindingResolution assertions
> (that is spec 018+). It ships:
> 1. A verified rime.lib (already built on this machine; documented
>    build command for fresh checkouts)
> 2. Updated TestBindingResolution.vcxproj with librime include +
>    lib paths (conditional via `__has_include(<rime_api.h>)`)
> 3. Updated TestBindingResolution.cpp: at SCAFFOLD start, log whether
>    rime_api.h was found (so CI / developer sees link capability)
> 4. A short, fast test that loads rime_api symbols (without calling
>    rime_get_api() -- which would need a full RimeStartMaintenance
>    setup) so the test project is genuinely linked against rime.lib
>    and not silently a no-op
>
> Per TDD.md sec 3.2, integration tests are MOCK librime (C++ header
> + stub implementation), not real rime.dll. This spec respects that
> principle: the test links rime.lib (the import library, header-only)
> but does NOT load rime.dll at runtime, does NOT call
> rime_get_api()->start_maintenance, and does NOT drive the real
> key_binder. The "link-only" link ensures the include path + lib path
> + symbol resolution all work, so spec 018+ can layer real mock
> key_binder logic on top without debugging the build system.

## 0. Background

- spec 016 (v0.18.10.0) shipped TestBindingResolution as a SCAFFOLD
  that prints "SCAFFOLD MODE" and returns 0. The scaffold proves
  the build system works (vcxproj + sln + run-tests.bat) but does
  not prove the test can drive librime.
- L18 / L19 both shipped 100% string-passing / 100% runtime-regressing
  fixes because no test exercised librime's actual key_binder.
  spec 016 closed the "no real test" gap by scaffolding the project;
  spec 017 closes the "no real link" gap by proving rime.lib can be
  linked from TestBindingResolution.
- librime 1.13.1 is a git submodule at `librime/`. The pre-built
  import library on this machine is at `librime/dist_Win32/lib/rime.lib`
  (293,342 bytes, 2026-07-01 14:38), built via
  `build.bat rime` (per AGENTS.md sec 4.4).
- The header `include/rime_api.h` (20,632 bytes) is the C API surface
  the test will use. The vendored `librime/include/` is the source
  of the C++ API (rime::KeyEvent, rime::KeySequence) but spec 018+
  will mock these per TDD.md sec 3.2.

## 1. Product Angle (PRD section)

### 1.1 Goal

Prove that the existing `build.bat rime` pipeline produces a
linkable rime.lib AND that TestBindingResolution can link against it
on a fresh checkout. This is the precondition for spec 018+ to
implement real key_binder assertions.

### 1.2 User Stories

- **US1-A** [P1]: Developer fresh-clones the repo. Runs
  `build.bat rime` then `scripts\run-tests.bat`. The
  TestBindingResolution build line includes rime.lib in the link
  command (verifiable in the msbuild output). The .exe loads
  without LNK errors.
- **US1-B** [P1]: CI on a clean checkout (windows-2022) builds
  librime first, then runs the test job. TestBindingResolution
  links against rime.lib and runs the SCAFFOLD-MODE-with-link-check
  variant.
- **US1-C** [P2]: A developer without librime built (e.g. only
  running inner-loop unit tests) can still run scripts\run-tests.bat
  with TestBindingResolution in SCAFFOLD-MODE-no-link mode. The
  vcxproj compiles and runs without rime.lib (the include is
  optional via `__has_include`).

### 1.3 Acceptance (GWT)

- Given a fresh checkout OR the current working tree
  (with `librime/dist_Win32/lib/rime.lib` already present)
- When the developer runs `scripts\run-tests.bat`
- Then TestBindingResolution builds AND:
  - The build output contains `/LIBPATH:librime\dist_Win32\lib rime.lib`
    in the link command (verifiable via msbuild /v:detailed)
  - The .exe prints "TestBindingResolution: LINKED rime.lib" on its
    first line (not "SCAFFOLD MODE - no assertions yet")
  - The .exe loads successfully; the rime_api symbol `rime_get_api`
    is resolved at load time (or the LNK error would have been
    caught at build)
  - The .exe exits 0
- And the run-tests.bat result is unchanged for the other 4 tests
- And `librime/dist_Win32/lib/rime.lib` byte count is unchanged
  (this spec does not rebuild librime; it verifies the existing
  build is usable)

## 2. Technical Angle (TDD section)

### 2.1 Files in this spec

| File | Change |
|---|---|
| test/TestBindingResolution/TestBindingResolution.vcxproj | MODIFIED: add librime include + lib paths via `__has_include` condition |
| test/TestBindingResolution/TestBindingResolution.cpp | MODIFIED: add a "LINKED rime.lib" branch alongside the SCAFFOLD branch |
| .specify/specs/017-verify-librime-link/ | spec/plan/tasks three-piece set |
| .specify/memory/lessons-learned.md | New L24 entry: rime.lib link path + `__has_include` pattern |
| CHANGELOG.md | [0.18.11.0-fluxing] section |
| release/fluxing-0.18.11.0-installer.exe | rebuilt with bumped version |

### 2.2 vcxproj changes (librime include + lib paths)

For the Release|Win32 and Debug|Win32 ItemDefinitionGroup, add:

```xml
<AdditionalIncludeDirectories>$(SolutionDir)\include;$(SolutionDir)\librime\include;$(BOOST_ROOT);%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
<AdditionalLibraryDirectories>$(SolutionDir)\librime\dist_Win32\lib;$(SolutionDir)\lib;$(BOOST_ROOT)\stage\lib;%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>
```

The AdditionalDependencies remains the existing pattern (no extra
lib). The link succeeds because the existing tests do not call
any librime function -- they only need the include path to be
resolvable. If spec 018+ adds a real call, the spec will add
rime.lib to AdditionalDependencies and a Link.Input.

### 2.3 .cpp changes (link-probe branch)

The .cpp main() will:

1. Try `#include <rime_api.h>` inside an `#if __has_include(<rime_api.h>)` guard
2. Inside the guard, declare a function pointer:
   `RimeApi* (*get_api)() = rime_get_api;` -- this requires the
   linker to resolve `rime_get_api` from rime.lib at link time
3. In main(), print:
   - If guard is true: `"TestBindingResolution: LINKED rime.lib (header version: %s, sizeof(RimeApi)=%u)\n"`
   - If guard is false: `"TestBindingResolution: SCAFFOLD MODE - rime_api.h not found"`
4. Always return 0

This makes the link-probe visible at run time. If the build
succeeds but the .exe crashes on `rime_get_api` symbol resolution
at load time, that would surface in the next test (TestWeaselIPC
loads WeaselServer.dll, not rime.dll, so no cross-test pollution).

### 2.4 Risks and mitigations

- **R1**: `rime.lib` not present on fresh checkout. Mitigation: the
  vcxproj's `AdditionalLibraryDirectories` references
  `librime\dist_Win32\lib` which is empty / missing. msbuild will
  emit a warning "lib path does not exist" but will NOT fail the
  build (the lib is in AdditionalDependencies only when referenced).
  spec 017 documents the `build.bat rime` pre-step in
  scripts\run-tests.bat header.
- **R2**: `__has_include(<rime_api.h>)` returns true even when
  rime.lib is missing. Mitigation: the link-probe declares
  `rime_get_api` as an extern pointer -- this DOES require rime.lib
  in the link. If rime.lib is missing, link fails with LNK2001.
- **R3**: `rime_get_api` is a function, not a global. Mitigation:
  declare `RimeApi* (*get_api)() = rime_get_api;` -- the
  initializer is a constant pointer; the link resolves
  `rime_get_api` symbol from rime.lib.
- **R4**: changing the vcxproj breaks other test projects. Mitigation:
  the AdditionalLibraryDirectories change is per-test-project; it
  only affects TestBindingResolution.vcxproj.

### 2.5 `__has_include` vs runtime check

`__has_include` is a C++17 feature (we already set
`LanguageStandard=stdcpp17` per spec 015 / L23). The runtime check
(`access("librime/dist_Win32/lib/rime.lib", F_OK)`) is more robust
but requires `<sys/stat.h>` or `<io.h>` and is harder to read.
spec 017 picks `__has_include` for readability; the link step
catches the missing-lib case (R2).

## 3. Out of scope (deferred to spec 018+)

- Filling in the actual TestBindingResolution assertions that
  drive librime's key_binder (per TDD.md sec 3.2 these will be
  MOCK librime, not real rime.dll loading)
- Adding rime.lib to AdditionalDependencies of TestBindingResolution
  (only needed when real assertions call librime symbols)
- TestUserDictUpdate, TestPhrasesRoundTrip, TestDarkModeBroadcast,
  TestYamlRoundTripE2E (TDD.md sec 3.1 -- spec 019+)
- Rebuilding rime.lib (already done 2026-07-01; spec 017 only
  verifies it is linkable from the current tree)
- The pre-existing TestResponseParser test_4 bug (out of scope since
  spec 015; this spec does not touch TestResponseParser)

## 4. Implementation steps (T001-T005)

- T001 - P1 - verify rime.lib link capability (smoke test compile
  + link a tiny C++ file that references rime_get_api)
- T002 - P1 - byte-level update TestBindingResolution.vcxproj with
  librime include + lib paths
- T003 - P1 - byte-level update TestBindingResolution.cpp with
  the `__has_include` link-probe branch
- T004 - P1 - lessons-learned L24 entry (librime link path +
  `__has_include` pattern)
- T005 - P1 - bump env.bat + weasel.props to 0.18.11.0; rebuild
  installer; commit + tag v0.18.11.0 + push

## 5. Status as of 2026-07-02

- librime 1.13.1 already built (commit 1c233581) on this machine
- rime.lib at `librime/dist_Win32/lib/rime.lib` (293,342 bytes)
- rime_api.h at `include/rime_api.h` (20,632 bytes)
- The smoke-test link (cl + rime.lib + rime_api.h) succeeds
  (89,600-byte exe produced in 1.7s, exit 0)
- spec 017 ships: (a) verified rime.lib link, (b) vcxproj wired,
  (c) .cpp link-probe, (d) L24, (e) v0.18.11.0 installer
- spec 018+ will fill in real assertions (mock key_binder per
  TDD.md sec 3.2)

## 6. Done

- T001..T005 all completed
- TestBindingResolution.vcxproj has librime include + lib paths
- TestBindingResolution.cpp prints "LINKED rime.lib" at run time
  (when rime_api.h is on the include path)
- v0.18.11.0 installer ships (L24 lesson + link-probe added)
- run-tests.bat: 5 tests pass; TestBindingResolution now in
  LINKED mode, not SCAFFOLD mode

## 7. References

- TDD.md sec 3.1, 3.2 (integration test strategy + mock librime
  principle)
- AGENTS.md sec 4.4 (librime is Win32-only, build.bat rime flow)
- L18, L19 (testing gap that this test exists to close)
- L21 (spec 014 fix; same dual-test pattern)
- L22 (spec 015 system(pause) + if errorlevel 1 anti-patterns)
- L23 (spec 016 vcxproj + sln GUID + ProjectConfigurationPlatforms
  contract; scaffold-by-default pattern)
- spec 014, spec 015, spec 016 (this spec is the third in the chain)