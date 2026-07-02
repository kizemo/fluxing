# 016 - Tasks

## T001 - P1 - byte-level create TestBindingResolution.cpp scaffold

- **File**: test/TestBindingResolution/TestBindingResolution.cpp (NEW)
- **Action**: write a stub main() that returns 0 and prints
  "TestBindingResolution: SCAFFOLD MODE - no assertions yet" plus
  a header line citing spec 016. The body must NOT call
  system("pause") (L22). Use std::cout / std::endl so the output
  flushes on exit. Include a comment block describing the 4 planned
  test cases (per spec.md sec 1.3 GWT) so future spec 017+ has
  the contract documented in source.
- **Byte-level**: UTF-8 (no BOM, matches existing test files), CRLF.
  byte-level write; do NOT use PowerShell string APIs (L01 / A1).
- **Acceptance**:
  - File exists, ~3-5 KB
  - `Select-String -Path test\TestBindingResolution\TestBindingResolution.cpp -Pattern "system.*pause"` returns 0 matches
  - `Release\TestBindingResolution.exe` builds standalone via
    `msbuild test\TestBindingResolution\TestBindingResolution.vcxproj /t:Build /p:Configuration=Release /p:Platform=Win32 /p:SolutionDir=<repo>\`
  - Running the .exe prints the scaffold message and returns 0

## T002 - P1 - create TestBindingResolution.vcxproj (modeled on TestWeaselIPC)

- **File**: test/TestBindingResolution/TestBindingResolution.vcxproj (NEW)
- **Action**: model on test\TestWeaselIPC\TestWeaselIPC.vcxproj.
  Differences:
  - ProjectGuid = `{99277F52-0973-411A-8171-E65FA3FF6D69}` (new, not reused)
  - RootNamespace = `TestBindingResolution`
  - AdditionalIncludeDirectories: $(SolutionDir)\include +
    $(SolutionDir)\librime\include (librime include; spec 017+ will
    need rime_api.h). For this spec the .cpp does NOT include rime_api.h,
    so the librime include path is forward-declared only.
  - AdditionalLibraryDirectories: $(SolutionDir)\librime\build\lib\Release
    (forward-declared; this spec does NOT link rime.lib)
  - AdditionalDependencies: $(NOINHERIT) (no extra lib this spec)
  - ItemGroup ProjectConfigurations: only Debug|Win32 + Release|Win32
    (NOT 4-8 configs; this test is Win32-only per AGENTS.md sec 2.3)
  - PrecompiledHeader: Use PrecompiledHeader (mirrors TestWeaselIPC)
- **Byte-level**: UTF-8 (no BOM, matches existing vcxproj files), CRLF.
  Use byte-level write; do NOT use PowerShell here-strings to author
  vcxproj content (L23 — see Risks).
- **Acceptance**:
  - File exists
  - GUID matches the one in weasel.sln (T004)
  - Builds via `msbuild ...` exit 0
  - No trailing backslash in any path
  - No `</SomeTag></SomeTag>` style malformed XML
  - First 100 bytes verified to be `<?xml version="1.0" encoding="utf-8"?>` etc. (no stray CR or BOM)

## T003 - P1 - create TestBindingResolution.vcxproj.filters

- **File**: test/TestBindingResolution/TestBindingResolution.vcxproj.filters (NEW)
- **Action**: copy the structure from TestWeaselIPC.vcxproj.filters,
  replacing the cpp/stdafx/targetver filenames.
- **Byte-level**: UTF-8 (no BOM, matches existing .filters files), CRLF.
- **Acceptance**:
  - File exists
  - Open in Visual Studio solution explorer shows the files in the
    expected hierarchy

## T003b - P1 - create stdafx.h / stdafx.cpp / targetver.h

- **File**: test/TestBindingResolution/stdafx.h, stdafx.cpp, targetver.h (NEW)
- **Action**: minimal stdafx modeled on TestWeaselIPC stdafx.h
  (includes windows.h + boost headers, no ATL/MFC since this test
  has no GUI dependency).
- **Byte-level**: UTF-8 (no BOM, matches existing test files), CRLF.
- **Acceptance**:
  - Files exist
  - Release\TestBindingResolution.exe builds (precompiled header
    path resolves correctly)

## T004 - P1 - fix weasel.sln (GUID + ProjectConfigurationPlatforms)

- **File**: weasel.sln (MODIFIED)
- **Action**: byte-level patch (per L23) of the handoff-broken state:
  1. Replace `... TestBindingResolution.vcxproj", "{}"` with the real GUID
     `... TestBindingResolution.vcxproj", "{99277F52-0973-411A-8171-E65FA3FF6D69}"`
  2. Insert 4 lines after the last E3A7B91D (TestShiftSelectBinding)
     line in the ProjectConfigurationPlatforms section:
     - `{99277F52-0973-411A-8171-E65FA3FF6D69}.Debug|Win32.ActiveCfg = Debug|Win32`
     - `{99277F52-0973-411A-8171-E65FA3FF6D69}.Debug|Win32.Build.0 = Debug|Win32`
     - `{99277F52-0973-411A-8171-E65FA3FF6D69}.Release|Win32.ActiveCfg = Release|Win32`
     - `{99277F52-0973-411A-8171-E65FA3FF6D69}.Release|Win32.Build.0 = Release|Win32`
  3. The sln is CRLF only (no BOM); byte count goes from 15796 to
     16148; CRLF count goes from 225 to 229.
- **Byte-level**: PowerShell `-replace` of literal strings, with
  byte-count + CRLF count verification AFTER write.
- **Acceptance**:
  - `weasel.sln` byte count = 16148, CRLF count = 229, lone LF/CR = 0
  - `Select-String -Path weasel.sln -Pattern 'TestBindingResolution.vcxproj", "{}"'` returns 0 matches
  - `Select-String -Path weasel.sln -Pattern "99277F52"` returns 5 matches
    (1 Project line + 4 config lines)
  - Opening the sln in VS does not show "inconsistent GUID" warning

## T005 - P1 - update scripts\run-tests.bat to include TestBindingResolution

- **File**: scripts/run-tests.bat (MODIFIED)
- **Action**: add TestBindingResolution to the test loop. Match the
  pattern used for TestShiftSelectBinding (same author, same era,
  same scaffolding).
- **Byte-level**: ASCII + CRLF (no BOM). byte-level patch.
- **Acceptance**:
  - `scripts\run-tests.bat` builds TestBindingResolution and runs it
  - Script reports `TestBindingResolution: SCAFFOLD MODE - no assertions yet`
    in the output
  - Script exits 0 if all 5 tests pass; non-zero otherwise

## T006 - P1 - lessons-learned L23 entry

- **File**: .specify/memory/lessons-learned.md (MODIFIED)
- **Action**: append a new L23 section documenting the
  behavior-level test framework pattern. Key points:
  1. **vcxproj GUID is a contract**: when adding a new test project,
     the sln Project line and the sln ProjectConfigurationPlatforms
     lines MUST use the real GUID from the vcxproj, not a placeholder.
     Reading the GUID from `$matches[0].Groups[1].Value` of a regex
     on `ReadAllText` output is brittle: if the script variable was
     null at write time, an empty `{}` ended up in the sln. The fix
     is to read the vcxproj BYTE-LEVEL, parse the GUID, and inject
     it into the sln in the same operation.
  2. **ProjectConfigurationPlatforms is sln-side, not vcxproj-side**:
     even if the vcxproj only declares 2 configs (Debug|Win32,
     Release|Win32), the sln still needs 4 lines per project
     (ActiveCfg + Build.0 × 2 configs). VS will warn "Solution
     ... does not contain a configuration ... that builds ... in
     solution configuration" if the count is wrong.
  3. **vcxproj content authored via PowerShell here-strings is
     hostile to backslash + CR + LITERAL-LF in content**: every
     here-string line becomes LF (no CR), and any embedded CR in
     a string literal gets re-encoded. The robust path is to write
     the vcxproj with byte-level WriteAllBytes and then post-verify
     the first 100 bytes are `<?xml version="1.0" encoding="utf-8"?>`.
  4. **The pattern that makes this work**: scaffold-by-default
     (test builds and exits 0) + assertions-when-librime-built.
     The scaffold is the stable surface; the assertions are layered
     on top. CI on a clean checkout will pass (scaffold runs);
     CI with `build.bat rime` pre-step will run real assertions.
- **Byte-level**: UTF-8 (no BOM, matches existing file), CRLF.
  byte-level edit.
- **Acceptance**:
  - `Select-String -Path .specify\memory\lessons-learned.md -Pattern "^##\s+L23 "` returns 1 match
  - File total length grew by ~2000-3000 bytes

## T007 - P1 - bump version 0.18.9 -> 0.18.10 in env.bat + weasel.props

- **Files** (2 files, both gitignored per AGENTS.md sec 3.3):
  - env.bat: FLUXING_VERSION 0.18.9 -> 0.18.10; WEASEL_BUILD 0 -> 0;
    RELEASE_BUILD=1
  - weasel.props: VERSION_PATCH 9 -> 10; PRODUCT_VERSION + FILE_VERSION
    follow
- **Action**: byte-level bump. Both files MUST stay untracked after
  the bump (NOT staged in the commit).
- **Acceptance**:
  - `git status -s` shows env.bat + weasel.props as `??` (untracked),
    never ` M` (modified)
  - `Get-Content env.bat` + `Get-Content weasel.props` show 0.18.10
  - `Get-Item output\Win32\WeaselServer.exe | ?{$_.VersionInfo.FileVersion}`
    reports 0.18.10 after rebuild

## T008 - P1 - rebuild installer + commit + tag + push

- **Files** (tracked):
  - test/TestBindingResolution/* (4 files, NEW)
  - weasel.sln (MODIFIED — T004)
  - scripts/run-tests.bat (MODIFIED — T005)
  - .specify/memory/lessons-learned.md (MODIFIED — T006)
  - .specify/specs/016-behavior-level-test-framework/* (3 files, NEW)
  - CHANGELOG.md (MODIFIED — 0.18.10.0 entry)
  - release/fluxing-0.18.10.0-installer.exe (NEW, ~42 MB)
- **Action**:
  1. Add `[0.18.10.0-fluxing]` section to CHANGELOG.md (preserve BOM)
  2. Run `xbuild.bat installer` to produce 0.18.10.0 installer
  3. Move installer to `release/fluxing-0.18.10.0-installer.exe`
  4. `git add` only the explicit paths above; never `git add .`
  5. `git commit -m "test(scaffold): spec 016 - behavior-level test framework (TestBindingResolution scaffold + L23)"`
  6. `git tag v0.18.10.0`
  7. `git push kizemo Fluxing + git push kizemo v0.18.10.0`
- **Acceptance**:
  - xbuild.bat installer exit code 0
  - release/fluxing-0.18.10.0-installer.exe exists, ~42 MB
  - git log --oneline -1 shows the new commit
  - git tag --list v0.18.10.0 returns 1 match
  - git push output shows the v0.18.10.0 tag on kizemo