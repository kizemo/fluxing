# 017 - Tasks

## T001 - P1 - smoke-test rime.lib link capability

- **Action**: verify that the existing `librime/dist_Win32/lib/rime.lib`
  (293,342 bytes, built 2026-07-01) can be linked from a tiny C++
  file that uses `rime_get_api`.
- **Steps** (already executed 2026-07-02, results recorded in spec.md
  sec 5):
  ```powershell
  $test = @"
  #include <rime_api.h>
  int main() { RimeApi* a = rime_get_api(); (void)a; return 0; }
  "@
  [System.IO.File]::WriteAllBytes("_test_link.cpp", $utf8NoBom.GetBytes($test))
  cmd /c "vcvars32.bat && cl /nologo /EHsc /I include _test_link.cpp /link /LIBPATH:librime\dist_Win32\lib rime.lib /OUT:_test_link.exe"
  # Expect: _test_link.exe ~89,600 bytes, exit 0
  ```
- **Acceptance**:
  - `_test_link.exe` builds (~89 KB)
  - `_test_link.exe` runs and exits 0
  - No LNK2001 (unresolved external `rime_get_api`)
  - Cleanup: remove `_test_link.cpp`, `_test_link.exe`, `_test_link.obj`

## T002 - P1 - update TestBindingResolution.vcxproj with librime include + lib paths

- **File**: test/TestBindingResolution/TestBindingResolution.vcxproj
- **Action**: byte-level patch. The current vcxproj has:
  ```xml
  <AdditionalIncludeDirectories>$(SolutionDir)\include;$(BOOST_ROOT);%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
  ```
  For both Debug|Win32 and Release|Win32 ItemDefinitionGroup. Add
  `$(SolutionDir)\librime\include` after `$(SolutionDir)\include`.
  The current AdditionalLibraryDirectories has:
  ```xml
  <AdditionalLibraryDirectories>$(SolutionDir)\lib;$(BOOST_ROOT)\stage\lib;%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>
  ```
  Add `$(SolutionDir)\librime\dist_Win32\lib` after `$(SolutionDir)\lib`.
- **Byte-level**: UTF-8 (no BOM), CRLF. byte-level replace; verify
  byte count delta is exactly +184 bytes (2 paths × 2 configs × 46 bytes
  per insertion: 22 char path + 1 semicolon = 23 chars × 2 = 46 per
  config, 2 configs = 184 bytes). Actually: 23 chars × 2 = 46 chars
  per config, 2 configs = 92 chars. The path string
  `$(SolutionDir)\librime\include;` is 33 chars; inserting this
  into the existing line changes the line by 33 chars. So 2 lines
  per config × 2 configs = 4 lines × 33 chars = 132 chars + CRLF
  (2 bytes per line × 4 = 8) = 140 bytes delta expected for include
  path. Similar for lib path. Total ~280 bytes delta. Exact value
  to be verified post-write.
- **Acceptance**:
  - `TestBindingResolution.vcxproj` builds (no new errors)
  - byte count grows by expected delta (track exact number)
  - `Select-String -Path test\TestBindingResolution\TestBindingResolution.vcxproj -Pattern "librime\\\\include"` returns 2 matches (Debug + Release)
  - `Select-String -Path test\TestBindingResolution\TestBindingResolution.vcxproj -Pattern "librime\\\\dist_Win32\\\\lib"` returns 2 matches

## T003 - P1 - update TestBindingResolution.cpp with link-probe branch

- **File**: test/TestBindingResolution/TestBindingResolution.cpp
- **Action**: byte-level patch. Add an `#if __has_include(<rime_api.h>)`
  block at the top of the file (after `#include "stdafx.h"`) and
  inside main(), branch on the include's availability.
- **New content (insert after the existing `#include "stdafx.h"` line)**:
  ```cpp
  #if __has_include(<rime_api.h>)
  #include <rime_api.h>
  #define RIME_API_H_PRESENT 1
  #else
  #define RIME_API_H_PRESENT 0
  #endif
  ```
- **New main() body** (replace existing main()):
  ```cpp
  int main(int argc, char* argv[]) {
      (void)argc;
      (void)argv;
  #if RIME_API_H_PRESENT
      // Link-probe: declare rime_get_api as a function pointer to force
      // the linker to resolve the symbol from rime.lib. The pointer
      // is never dereferenced (would need rime.dll at runtime; we
      // respect TDD.md sec 3.2 mock-librime principle).
      RimeApi* (*get_api_ptr)() = rime_get_api;
      (void)get_api_ptr;
      std::cout << "TestBindingResolution: LINKED rime.lib"
                << " (header version: " << RIME_VERSION << ")"
                << " (sizeof(RimeApi)=" << sizeof(RimeApi) << ")"
                << std::endl;
      std::cout << "  spec 017 / 2026-07-02 - librime 1.13.1 link verified"
                << std::endl;
      std::cout << "  Real assertions deferred to spec 018+ (mock key_binder,"
                << std::endl;
      std::cout << "  per TDD.md sec 3.2)." << std::endl;
  #else
      std::cout << "TestBindingResolution: SCAFFOLD MODE - rime_api.h not found"
                << std::endl;
      std::cout << "  spec 016 / 2026-07-02 - see .specify\\specs\\017-verify-librime-link\\spec.md"
                << std::endl;
      std::cout << "  Run build.bat rime to produce rime.lib, then re-run."
                << std::endl;
  #endif
      return 0;
  }
  ```
- **Byte-level**: UTF-8 (no BOM), CRLF. byte-level replace. Verify
  file builds standalone via msbuild, prints "LINKED rime.lib"
  (not "SCAFFOLD MODE - no assertions yet") when rime_api.h is on
  the include path.
- **Acceptance**:
  - `Release\TestBindingResolution.exe` builds
  - First stdout line is "TestBindingResolution: LINKED rime.lib" (since
    rime_api.h is on include path on this machine)
  - Exit code 0
  - If rime_api.h is removed from include path, the .exe prints
    "TestBindingResolution: SCAFFOLD MODE - rime_api.h not found"
    and still exits 0

## T004 - P1 - lessons-learned L24 entry

- **File**: .specify/memory/lessons-learned.md
- **Action**: append a new L24 section documenting the
  rime.lib link path + `__has_include` pattern.
- **Key points**:
  1. librime 1.13.1 import library is at
     `librime/dist_Win32/lib/rime.lib` after `build.bat rime`
     (per AGENTS.md sec 4.4). It is a Microsoft `ar`-format
     import library (NOT LLVM LTO bitcode -- the `!<arch>`
     magic is identical to GNU ar archives).
  2. librime C API header is at `include/rime_api.h` (copied
     from `librime/dist_Win32/include/rime_*.h` by build.bat
     line 366). librime C++ API is at `librime/include/`
     (NOT `librime/include/rime/` -- flat layout). For spec 018+
     mock key_binder, prefer the C API (`rime_api.h`) over the
     C++ API to avoid the flat-layout include path.
  3. The `__has_include(<rime_api.h>)` C++17 pattern lets the
     test project build with or without librime available. The
     link-probe is the second half: `RimeApi* (*get_api)() = rime_get_api;`
     forces the linker to resolve the symbol. Missing rime.lib
     -> LNK2001 (build fails loud, not silent).
  4. TDD.md sec 3.2 says integration tests are MOCK librime.
     The link-probe does NOT call rime_get_api (would need
     rime.dll loaded, requires rime_start_maintenance setup,
     is a runtime test, not a link test). The pattern is:
     link the symbol, do not call it. spec 018+ will write
     a mock key_binder that simulates the rime::KeyEvent API
     surface in pure C++.
  5. Smoke test pattern for any future rime.lib consumer: compile
     a 1-file C++ test with `#include <rime_api.h>` + `cl /link
     /LIBPATH:librime\dist_Win32\lib rime.lib` and verify the
     linker resolves `rime_get_api` without LNK2001. ~1.7s on
     this machine.
- **Acceptance**:
  - `Select-String -Path .specify\memory\lessons-learned.md -Pattern "^##\s+L24 "` returns 1 match
  - File total length grew by ~2500 bytes

## T005 - P1 - bump version 0.18.10 -> 0.18.11 + rebuild + commit + tag + push

- **Files** (tracked):
  - test/TestBindingResolution/TestBindingResolution.vcxproj (MODIFIED)
  - test/TestBindingResolution/TestBindingResolution.cpp (MODIFIED)
  - .specify/memory/lessons-learned.md (MODIFIED)
  - .specify/specs/017-verify-librime-link/* (3 files, NEW)
  - CHANGELOG.md (MODIFIED -- 0.18.11.0 section)
  - release/fluxing-0.18.11.0-installer.exe (NEW, ~42 MB)
- **Files** (gitignored, NOT in commit):
  - env.bat: 0.18.10 -> 0.18.11 (FLUXING_VERSION / VERSION_PATCH /
    PRODUCT_VERSION / FILE_VERSION)
  - weasel.props: 0.18.10 -> 0.18.11 (VERSION_PATCH / PRODUCT_VERSION
    / FILE_VERSION)
- **Action**:
  1. Bump env.bat + weasel.props
  2. `git add` only the explicit tracked paths above; never `git add .`
  3. `git commit -m "test(fluxing): spec 017 - verify librime link (TestBindingResolution link-probe + L24)"`
  4. `git tag v0.18.11.0`
  5. `git push kizemo Fluxing + git push kizemo v0.18.11.0`
  6. `xbuild.bat installer` to produce 0.18.11.0 installer
  7. Move installer to `release/fluxing-0.18.11.0-installer.exe`
  8. Amend the commit with the installer (or commit in 2 steps --
     amend is cleaner per AGENTS.md sec 3.3 version bump procedure)
- **Acceptance**:
  - `git status -s` after bump shows env.bat + weasel.props as `??` (untracked)
  - xbuild.bat installer exit code 0
  - release/fluxing-0.18.11.0-installer.exe exists, ~42 MB
  - git log --oneline -1 shows the new commit
  - git tag --list v0.18.11.0 returns 1 match
  - git push output shows the v0.18.11.0 tag on kizemo