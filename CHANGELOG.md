
## [0.18.14.0-fluxing] - 2026-07-03

### spec 025: close 023 placeholder, create 025 placeholder, commit pre-staged atlas + 001 baseline (bookkeeping only)

- **Bookkeeping-only release**, no production code change. Same code
  version (0.18.14.0). Sub-bumped to 0.18.14.1 to record the spec
  close/create + atlas/baseline commit.

- **Closes spec 023 placeholder**: the TestYamlRoundTripE2E
  integration test placeholder (created in v0.18.13.0) was
  effectively closed by spec 024 (v0.18.14.0, commit 4dbcced),
  which shipped the YamlRoundTrip module + the test. The 023 spec
  file remains in git as a historical record with its `0. Status`
  section updated to `CLOSED (merged into spec 024)`.

- **Creates spec 025 placeholder** at
  `.specify/specs/025-bootstrapper-stdio-placeholder/` for the
  next TDD sec 3.1 integration test (#5, TestBootstrapperStdio).
  Blocked on spec 011 (Fluxing Bootstrapper).

- **Commits pre-staged materials** that have been in the working
  tree since before v0.18.10.0:
  - `docs/Fluxing-code-map/*` (7 files) - the project atlas for
    future agents / developers. LF-only line endings (docs do not
    need CRLF).
  - `.specify/specs/001-user-visible-strings/baseline/*` (8 files) -
    the pre-image snapshot for spec 001 T016 byte-level diff at end
    of spec 001. Encoding/line endings preserved as-is (some files
    are GBK-polluted from the upstream rime/weasel 0.17.4 base;
    the baseline must stay byte-faithful to support the diff).

- **No code, no test, no installer change.** `scripts/run-tests.bat`
  still 6/6 green. The new 025 directory is text-only.

### spec 024: YamlRoundTrip module + TestYamlRoundTripE2E (close 1 of 4 TDD sec 3.1 integration tests; L27 yaml-cpp comment limit)

- **Problem**: TDD.md sec 3.1 lists 4 integration tests for the
  Fluxing brand-fork. 3 of them (spec 020, 021, 022) are still
  blocked on their parent specs (008, 009, 004 sec 9). The 4th,
  TestYamlRoundTripE2E, was blocked on spec 007 (yaml-config-ui),
  whose full scope is a 6-10 week mac-style settings UI. spec 007
  sec 2 explicitly carves out YamlRoundTrip as a standalone module
  that the future settings UI can use; this spec ships that module
  alone, unblocking the test, and is independently useful for any
  code that needs to read and write rime/weasel YAML files without
  losing the on-disk key ordering of `key_binder.bindings`.

- **What this spec ships**:
  1. `FluxingConfigEditor/YamlRoundTrip.{h,cpp}` -- a small C++
     wrapper around yaml-cpp 0.5+ with 4 entry points:
     `Load(yaml_text, &doc)`, `Save(doc, &yaml_text)`,
     `ReadString(doc, dotted_key, &value)`,
     `WriteString(&doc, dotted_key, value)`. pImpl-style to avoid
     pulling yaml-cpp headers into every translation unit.
  2. `test/TestYamlRoundTripE2E/{TestYamlRoundTripE2E.cpp, .vcxproj,
     .vcxproj.filters, stdafx.h, stdafx.cpp, targetver.h}` -- the
     4th integration test, with 6 assertions: parser sanity, key
     order preservation on save, round-trip equivalence for the
     first 5 bindings, WriteString preserves other keys, dotted read
     for nested map keys, and the L27 comment-strip contract.
  3. weasel.sln registration: new `TestYamlRoundTripE2E` project
     entry with `Release|Win32` and `Debug|Win32`
     ProjectConfigurationPlatforms (L23 pattern).
  4. `scripts/run-tests.bat`: TestYamlRoundTripE2E added to both
     the build loop and the run loop (5 -> 6 entries each).
  5. **L27** in `lessons-learned.md`: yaml-cpp 0.5+ does not store
     comments in `YAML::Node`; round-trip tools must declare this
     in their API contract and compare parsed-original vs
     parsed-roundtripped (not text-original vs text-roundtripped).

- **What this spec does NOT ship (per the user's 'option 1' decision on spec 019)**:
  - The 3 remaining TDD sec 3.1 placeholder specs (020, 021, 022)
    remain BLOCKED on their parent specs (008, 009, 004 sec 9).
  - The full spec 007 mac-style settings UI (6-10 weeks of GUI work).
  - Comment preservation on round-trip (L27 -- requires a custom
    yaml tokenizer, deferred until a real user need surfaces).

- **Test results**:
  - TestYamlRoundTripE2E: 6 / 6 assertions passed (parser sanity,
    key order preservation, round-trip equivalence, write preserves
    other keys, dotted read, comment strip).
  - All 5 prior tests still pass (TestDefaultHotkeys 35/35,
    TestShiftSelectBinding 13/13, TestBindingResolution 6/6,
    TestResponseParser 4/4, TestWeaselIPC smoke).

�
## [0.18.12.0-fluxing] - 2026-07-02

### spec 018: fill TestBindingResolution with 4 real assertions (close L18 / L19 testing gap)

- **Problem**: spec 016 (v0.18.10.0) shipped TestBindingResolution
  as a SCAFFOLD stub. spec 017 (v0.18.11.0) upgraded it to LINKED
  rime.lib (link-probe pattern). Both stubs printed only a status
  message and returned 0 -- the test did not actually exercise any
  binding form. L18 / L19 had previously shipped 100% string-passing
  / 100% runtime-regressing fixes because the test layer had no
  parse-level coverage.

- **Fix (spec 018)**: 3 changes:
  1. **Mock namespace in TestBindingResolution.cpp**: a `mock`
     namespace with `KeyEvent`, `Modifier`, `ParseKeyEvent`, `Match`.
     The mock follows librime 1.13 key_event.h:64 exactly:
     `Match` is strict keycode + modifier equality. The modifier
     table has `Shift` (case-sensitive first letter per L16),
     `Control`, `Alt`, `Super`, `Release`. The keyname table covers
     Shift_L/R, Control_L/R, space, Tab, Left/Right, Page_Up/Down,
     comma, period, bracketleft/right, grave, exclam, at, dollar,
     Return, asterisk, plus, minus, slash, numbersign, KP_0-9,
     KP_Decimal/Multiply/Add/Subtract/Divide/Enter.
  2. **Hand-rolled yaml scanner**: ~40 lines of C++ that walks
     `output\data\default.yaml`, finds the `key_binder.bindings:`
     section, and extracts each `- { when: ..., accept: ..., send: ... }`
     entry into a `Binding` struct. The scanner skips the
     `bindings:` line itself, walks line by line, and stops at the
     next top-level yaml key.
  3. **4 real assertions** (replacing the SCAFFOLD / LINKED stub):
     - **Test 1** (parser sanity): every `accept:` parses to a valid
       KeyEvent. 31/31 parse OK on this machine.
     - **Test 2** (L18 invariant): a TSF release event
       `(keycode=Shift_L, modifier=Release)` does NOT match
       `accept: Shift+Shift_L`. PASS.
     - **Test 3** (spec 014 ordering): `Shift+Shift_L` at index 6
       appears before `Control+1` at index 8. PASS.
     - **Test 4** (existence + L19 guard): 4a has_menu + Shift+Shift_L
       exists. 4b has_menu + Control+1 exists. 4c NO bare `accept: Shift_L`.
       PASS.
- **First-run output**:
  ```
  TestBindingResolution: LINKED rime.lib (rime_get_api resolved at link time, sizeof(RimeApi)=396)
    spec 018 / 2026-07-02 - 4 assertions on output\data\default.yaml
    parsed 31 key_binder bindings from output\data\default.yaml
    PASS: Test 1: every binding accept: parses to a valid KeyEvent
    PASS: Test 2: TSF release event (Shift_L, Release) does NOT match Shift+Shift_L binding (L18 invariant)
    PASS: Test 3: Shift+Shift_L (idx=6) appears before Control+1 (idx=8) in key_binder.bindings (spec 014 ordering)
    PASS: Test 4a: has_menu + accept: Shift+Shift_L exists (spec 014 contract)
    PASS: Test 4b: has_menu + accept: Control+1 exists (spec 005 contract)
    PASS: Test 4c: NO bare accept: Shift_L or Shift_R (L19 guard)
    6 / 6 assertions passed
  ```
- **Why mock, not real rime::KeyEvent**: 3 independent discoveries
  ruled out the real API path (per spec 018 sec 1 / L25):
  (a) rime::KeyEvent lives in librime/src/rime/, NOT in dist/include.
  (b) key_table.h includes <X11/keysym.h> (Linux-only).
  (c) TDD.md sec 3.2 says integration tests are MOCK librime.
  The mock is the only viable path.

- **Test results (scripts\run-tests.bat)**:
  - TestDefaultHotkeys: 35/35 PASS (string-level guard)
  - TestShiftSelectBinding: 13/13 PASS (string-level guard, spec 014 contract)
  - **TestBindingResolution: 6/6 PASS (parse-level guard, L18 / L19 invariant)**
  - TestResponseParser: 3/4 (test_4 pre-existing WeaselIPC bug)
  - TestWeaselIPC: PASS

- **Spec**: .specify\specs\018-fill-binding-resolution\
  - spec.md 13556 bytes (intent + US + GWT acceptance)
  - plan.md 8106 bytes (approach + Constitution Check + verification matrix)
  - tasks.md 7066 bytes (T001..T005 implementation checklist)

- **L25** added to `.specify\memory\lessons-learned.md` documenting
  the mock pattern + 5 anti-patterns (AP-L25-A through AP-L25-E).

- **Installer**: `release\fluxing-0.18.12.0-installer.exe` (~42 MB).
  env.bat + weasel.props bumped 0.18.11 -> 0.18.12 (gitignored, NOT
  committed). Smoke test deferred to CI / clean machine (user's
  WeaselServer.exe is running, per spec 015 sec 5).
## [0.18.11.0-fluxing] - 2026-07-02

### spec 017: verify librime link (TestBindingResolution link-probe + L24)

- **Problem**: spec 016 (v0.18.10.0) shipped TestBindingResolution
  as a SCAFFOLD stub. The "real assertions deferred to spec 017+
  pending build.bat rime" language left the link path unbuilt.
  L18 / L19 both shipped 100% string-passing / 100% runtime-
  regressing fixes; the link-probe is the precondition for any
  behavior-level test that drives librime symbols.

- **Fix (spec 017)**: 3 changes:
  1. **Wire librime include + lib paths into
     TestBindingResolution.vcxproj**: added
     `$(SolutionDir)\librime\include` to
     AdditionalIncludeDirectories and
     `$(SolutionDir)\librime\dist_Win32\lib` to
     AdditionalLibraryDirectories, in both Debug|Win32 and
     Release|Win32 ItemDefinitionGroup. Byte count: 4888 -> 5026
     (+138). CRLF: 82 (unchanged).
  2. **Add link-probe branch in TestBindingResolution.cpp**:
     `#if __has_include(<rime_api.h>)` guard around the
     `#include <rime_api.h>`, then declare a function pointer
     `RimeApi* (*get_api_ptr)() = rime_get_api;` inside the
     guard. The declaration forces the linker to resolve
     `rime_get_api` from rime.lib (link-fails-loud if rime.lib
     is missing). The pointer is never dereferenced (would need
     rime.dll loaded; per TDD.md sec 3.2 integration tests are
     MOCK librime, not real rime.dll). The .cpp prints
     "LINKED rime.lib (rime_get_api resolved at link time,
     sizeof(RimeApi)=396)" on success or "SCAFFOLD MODE -
     rime_api.h not found" on a clean checkout. Byte count:
     4360 -> 5210 (+850). CRLF: 84 -> 106 (+22).
  3. **L24 entry** added to `.specify\memory\lessons-learned.md`
     documenting the link-probe pattern (declare function
     pointer, do not call it) and 4 anti-patterns (AP-L24-A
     through AP-L24-D).

- **First-run output** (scripts\run-tests.bat on this machine):
  ```
  TestBindingResolution: LINKED rime.lib (rime_get_api resolved at link time, sizeof(RimeApi)=396)
    spec 017 / 2026-07-02 - librime 1.13.1 link verified
    Real assertions deferred to spec 018+ (mock key_binder,
    per TDD.md sec 3.2).
  ```

- **Smoke test verification** (already executed 2026-07-02):
  ```
  cl /nologo /EHsc /I include _t.cpp /link /LIBPATH:librime\dist_Win32\lib rime.lib /OUT:_t.exe
  # exit 0, _t.exe 89,600 bytes
  ```

- **Test results (scripts\run-tests.bat)**:
  - TestDefaultHotkeys: 35/35 PASS
  - TestShiftSelectBinding: 13/13 PASS
  - TestBindingResolution: LINKED rime.lib (exit 0)  was SCAFFOLD
    in spec 016, now LINKED
  - TestResponseParser: 3/4 PASS (test_4 pre-existing WeaselIPC bug)
  - TestWeaselIPC: PASS (WeaselServer roundtrip)

- **Spec**: .specify\specs\017-verify-librime-link\
  - spec.md 11173 bytes (intent + US + GWT acceptance)
  - plan.md 7866 bytes (approach + Constitution Check + verification matrix)
  - tasks.md 9085 bytes (T001..T005 implementation checklist)

- **Installer**: `release\fluxing-0.18.11.0-installer.exe` (~42 MB).
  env.bat + weasel.props bumped 0.18.10 -> 0.18.11 (gitignored, NOT
  committed). Smoke test deferred to CI / clean machine (user's
  WeaselServer.exe is running, per spec 015 sec 5).

- **Next spec (018+)**: fill in real TestBindingResolution assertions
  using a MOCK key_binder (TDD.md sec 3.2), not real rime.dll. The
  link-probe is the bridge: it proves the build system can reach
  real librime symbols, but the test itself will never depend on
  rime.dll at runtime.
## [0.18.10.0-fluxing] - 2026-07-02

### spec 016: behavior-level test framework (TestBindingResolution scaffold + L23)

- **Problem (L23)**: Adding a new test project (TestBindingResolution)
  to close the L18 / L19 testing gap required registering the project
  in `weasel.sln` with the right GUID AND the right number of
  `ProjectConfigurationPlatforms` entries. The handoff build attempt
  used a PowerShell script that read the GUID from a regex match and
  substituted it into a templated `Project(...)` line; the regex match
  returned `$null` at the substitution call, so the sln ended up with
  an empty `{}` GUID and zero `ProjectConfigurationPlatforms` entries.
  VS would show `inconsistent project GUID` on open and the solution
  build would skip the project. The string-only test layer in spec 014
  / 015 cannot prove that librime's `key_binder` actually matches the
  binding on a real KeyEvent -- L18 / L19 both shipped 100%
  string-passing / 100% runtime-regressing fixes because the test layer
  had no real runtime coverage.

- **Fix (spec 016)**: 3 changes:
  1. **NEW test\TestBindingResolution\**: scaffold test project with
     vcxproj + .cpp + .filters + stdafx.h + stdafx.cpp + targetver.h
     (modeled on TestWeaselIPC, no ATL/MFC, Win32-only configs). The
     .cpp is a stub `main() { std::cout << "SCAFFOLD MODE"; return 0; }`
     that builds in ~2s and exits 0. Real assertions (parse default.yaml,
     build rime::KeyEvent, drive librime's key_binder) are stubbed in
     comments and deferred to spec 017+ which requires a one-time
     `build.bat rime` pre-step.
  2. **Byte-level fix weasel.sln**: replaced empty GUID with the real
     GUID `{99277F52-0973-411A-8171-E65FA3FF6D69}` read from
     `TestBindingResolution.vcxproj`, then inserted 4
     `ProjectConfigurationPlatforms` lines (Debug|Win32 + Release|Win32,
     each with ActiveCfg + Build.0) after the E3A7B91D
     (TestShiftSelectBinding) block. Byte count: 15796 -> 16148 (+352);
     CRLF: 225 -> 229 (+4); no newlines introduced.
  3. **Update scripts\run-tests.bat**: added TestBindingResolution to
     both the BUILD loop and the RUN loop. The script now builds and
     runs 5 test projects (added TestBindingResolution to the existing
     list of TestDefaultHotkeys, TestShiftSelectBinding,
     TestResponseParser, TestWeaselIPC).

- **L23 entry** added to `.specify\memory\lessons-learned.md` documenting
  the vcxproj-GUID + sln-ProjectConfigurationPlatforms contract and the
  scaffold-by-default / assertions-when-librime-built pattern.

- **Test results (scripts\run-tests.bat)**:
  - TestDefaultHotkeys: 35/35 PASS
  - TestShiftSelectBinding: 13/13 PASS
  - TestBindingResolution: SCAFFOLD MODE - no assertions yet (exit 0)
  - TestResponseParser: 3/4 PASS (test_4 pre-existing WeaselIPC bug)
  - TestWeaselIPC: PASS (WeaselServer roundtrip)

- **Spec**: .specify\specs\016-behavior-level-test-framework\
  - spec.md 8033 bytes (intent + US + GWT acceptance)
  - plan.md 7070 bytes (approach + Constitution Check + verification matrix)
  - tasks.md 9729 bytes (T001..T008 implementation checklist)

- **Installer**: `release\fluxing-0.18.10.0-installer.exe` (~42 MB).
  env.bat + weasel.props bumped 0.18.9 -> 0.18.10 (gitignored, NOT
  committed). Smoke test deferred to CI / clean machine (user's
  WeaselServer.exe is running, per spec 015 sec 5).
## [0.18.9.0-fluxing] - 2026-07-02

### spec 015: fix test infrastructure (TestResponseParser + TestWeaselIPC + system(pause) anti-pattern)

- **Problem (L22)**: TestResponseParser.exe + TestWeaselIPC.exe had been
  unable to build since the vcxproj files were added (TDD.md sec 2.1).
  The test projects use $(SolutionDir)\include for headers, but when
  built standalone, $(SolutionDir) resolves to the vcxproj's own
  directory. Even with that fix, the test .exe files contained
  system("pause"); as the last line before return (a Windows-console
  interactive UX anti-pattern from the 2024-02 clang-format pass,
  commit 21d2bf9), which crashes with 0xC0000005 in non-interactive
  environments (CI, scripts, redirected stdin). The crash was masked in
  double-click Explorer launches but blocks CI / scripted runs.

- **Fix (spec 015)**: 3 changes:
  1. **Byte-level remove system("pause");** from
     test\TestResponseParser\TestResponseParser.cpp and
     test\TestWeaselIPC\TestWeaselIPC.cpp (UTF-8 no BOM, CRLF, byte-level
     edit to preserve existing file state; L01 / A1 anti-pattern)
  2. **NEW scripts\run-tests.bat**: single-command wrapper that builds
     + runs all 4 test projects (TestDefaultHotkeys, TestShiftSelectBinding,
     TestResponseParser, TestWeaselIPC). The script:
     - Calls vcvars32.bat via 8.3 short path
       (C:\PROGRA~2\...) to avoid the backslash-paren CMD-parse trap
     - For each test project, calls
       msbuild <project>.vcxproj /t:Build /p:Configuration=Release
       /p:Platform=Win32 /p:SolutionDir=<absolute-path> (no trailing
       backslash) to work around the standalone-build SolutionDir issue
     - Runs each test exe with stdin redirected to nul (< nul)
     - Uses if !errorlevel! NEQ 0 set "FAIL=1" (delayed expansion) to
       correctly detect negative Windows STATUS codes (e.g. 0xC0000005
       = -1073741819) that if errorlevel 1 misinterprets as success
     - Reports === ALL TESTS PASSED === or === TESTS FAILED === and
       returns the correct exit code via the
       endlocal & set "OUTER_RC=%FINAL_RC%" propagation pattern
  3. **ci.yml test job (TDD.md sec 6.2)**: replaced the
     TestDefaultHotkeys-only job (commit 10b72e2) with a single call to
     scripts\run-tests.bat

- **L22 lesson (recorded in lessons-learned.md)**:
  1. system("pause") in test code is a Windows-console interactive UX
     anti-pattern that crashes in non-interactive environments. Right
     pattern: tests should return 0; directly.
  2. if errorlevel 1 in batch scripts misinterprets negative Windows
     STATUS codes (e.g. 0xC0000005 = -1073741819) as "no error" because
     -1073741819 < 1 numerically. Right pattern:
     if !errorlevel! NEQ 0 with delayed expansion.

- **Test status as of 2026-07-02**:
  - TestDefaultHotkeys: 35/35 PASS
  - TestShiftSelectBinding: 13/13 PASS
  - TestWeaselIPC: PASS
  - TestResponseParser: test_4 FAILS (BOOST_ASSERT(2 == c.candies.size())
    because WeaselIPC's ContextUpdater does not implement ctx.cand.0 /
    ctx.cand.1 array-style deserialization)
  - **The TestResponseParser test_4 failure is a pre-existing WeaselIPC
    bug**, NOT a test infrastructure issue. Out of scope for spec 015;
    filed for spec 016+. The scripts\run-tests.bat correctly reports
    === TESTS FAILED === + exit 1 for it, which is the right CI gate
    behavior.

- **Build**:
  - xbuild.bat installer -> output/archives/fluxing-0.18.9.0-installer.exe
    (~42 MB, NSIS 3.x Unicode)
  - 7z comparison with 0.18.8.0: only data\default.yaml is identical;
    no new binary changes (this is a test-infra-only release; the
    behavior fix in 0.18.8.0 is preserved)

- **Out of scope (deferred to spec 016+)**:
  - Fixing the WeaselIPC ContextUpdater cand.0/cand.1 deserialization
  - Adding the TestBindingResolution integration test (TDD.md sec 3)
  - Fixing the pre-existing RC errors in WeaselTSF.rc / WeaselServer.rc /
    WeaselDeployer.rc / WeaselSetup.rc (STRZ2 macro)
## [0.18.8.0-fluxing] - 2026-07-02

### spec 014: restore Shift_L/R select 2nd/3rd candidate (L21)

- **Problem**: (7� 0.18.7.0 �K͈ "(	W͗�, ��	�,�/
  ,		W�"spec 005 v1.1 US1-B �� "	 Shift_L/R 	, 2/3 	" (
  0.18.6.0 K� L19 � , 0.18.7.0 � CI �@��F*b���

- **Fix (spec 014)**: ( key_binder/bindings has_menu �, ( Control+1/2
  KM�� 2 L binding, ( spec 012 plan.md �2.2 ��� ccept: Shift+Shift_L/R
  b (modifier=Shift, � TSF release event �9M):
  `yaml
  - { when: has_menu, accept: Shift+Shift_L, send: 2 }
  - { when: has_menu, accept: Shift+Shift_R, send: 3 }
  `

- **L21 Y�**: L19 / over-correction - �d�@	 keycode=Shift_L/R binding,
  � spec 005 v1.1 �� has_menu bindingL19 ��� "W&2� ���
   yaml �,��a binding X(/X(", F L19 ��t�V TestDefaultHotkeys
  31/31 PASS e"��"�, ͽ0��W&2KՄ@P

- **KՆ�**:
  - TestDefaultHotkeys.exe output\data\default.yaml -> Passed: 35 / 35
    (4 * L19 �c + 4 *�c)
  - TestShiftSelectBinding.exe output\data\default.yaml -> Passed: 13 / 13 (�)
  - $W�� runtime KդɌ� spec 014 �, M L18/L19 � "passing
    test, regressed behavior" w1

- **Build**:
  - xbuild.bat installer -> output/archives/fluxing-0.18.8.0-installer.exe
    (42631276 W�, ~40.7 MB)
  - 7z ��� 0.18.7.0 vs 0.18.8.0: / � data\default.yaml 16607
    -> 17200 W� (+593 W�), 23 * binary 100% � (rime.dll / WeaselServer.exe
    I SHA256 hI)

- **Verified by**: silent install 0.18.8.0 -> exit 0, HKLM InstallDir =
  C:\Program Files\fluxing, HKCU RimeUserDir = C:\Program Files\fluxing
  \user1\fluxing, default.yaml + spec 014 �

- **Refs**: L19 (� L21 ��), L20 (silent-install cmd /c wrapper �(), spec 012
  (L16 �F* ship), spec 014 (L21 �), spec 005 v1.1 US1-B (b�)

�## [0.18.6.0-fluxing] - 2026-07-01

### L19: defensive remove of all keycode=Shift_L/R bindings (spec 005 v1.1)

- **Problem**: 0.18.5.0 (7�K͈ `shift+Enter` / `shift+<letter>` release
  event ��� ascii_mode bL18 �commit `e4095f2`	��d�
  `always: Shift+Shift_L/R toggle ascii_mode`F�Y�
  `has_menu: Shift+Shift_L/R send 2/3` � bindingL18 W&2� 
  25/25 PASS ���L� binding hL:0.18.6 ��* buildL18
  �*��Ō�	

- **Fix (L19)**: 2�'�`output/data/default.yaml` -
  `keycode=Shift_L/R` �@	 binding **h�X(**+ `always` 
  `has_menu` $a�	

  - 		�9( RIME >:ؤ.M `Control+1/2/3..9`
    keycode=`1`/`2`/`3`..`9`  `Shift_L/R` release event ��	
  - -��Y `Shift+space`keycode=`space`	
  - ascii_composer `switch_key.Shift_L/R: noop` �Y� key_binder ��	

- **Test coverage**: `test/TestDefaultHotkeys/TestDefaultHotkeys.cpp` �
  25/25 G�0 **31/31 PASS**�� 6 * L19 � ��@	
  `Shift_L/R` b� binding	+ 1 * L19 c� active ascii_mode
  toggle �p == 1� `Shift+space`	

- **Unverified**: ���L:librime submodule a�0.18.6 build
  �� release �6	(7�� 0.18.6 �{K���
  - `shift+Enter` / `shift+<letter>` �-�
  - `Shift+space` ��-�
  - 	�S � `Control+1` / `Control+2` 	, 2/3 	

- **Refs**: L18� L19 ��	L16spec 005 v1.1 plan.md �2.2

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
  the registry. When a previous silent-install smoke test (AGENTS.md �2.5)
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
- Shift_L after candidate commit goes back to Chinese (verified with hao -> })
## [0.18.4.0] - 2026-06-30

### 主要更新

- **架构一致性：始终安装 Win32 二进制** (修复 0xC000007B STATUS_INVALID_IMAGE_FORMAT bug)
  - librime 是 Win32-only（`output\rime.dll` 是 x86，~3MB 含 lua 插件）。但 `output\WeaselDeployer.exe` / `WeaselServer.exe` 是 x64。把 x64 进程加载 x86 rime.dll 会触发 WoW64 进程级架构冲突：0xC000007B。
  - 之前 0.17.5-0.18.3 的 installer 有个 `${If} ${RunningX64}` 条件分支，在 x64 Windows 上装 x64 版本的 `Weasel*.exe`。这条路径产出的安装包在目标机器上会立刻 crash。
  - 修复：删掉 x64 / Win32 条件分支，**始终**装 `Win32\Weasel*.exe` + `Win32\rime.dll`（全部 x86）。x64 OS 通过 WoW64 加载 32-bit EXE，rime.dll 也是 32-bit，架构统一。
  - **L14** lessons-learned 记录了完整根因 + 修复 + 教训。

- **默认安装路径简化为 `C:\Program Files\fluxing`**
  - 之前 `.onInit` 有 `${If} ${AtLeastWin11}` 嵌套 `${If} ${IsNativeARM64}` 等 4 路分支（Win11+ARM64 / Win11+AMD64 / Win11+x86 / Win10+AMD64 / Win10+x86），逻辑复杂且全用了 `$PROGRAMFILES64`（x86 installer 在 x64 Windows 上会被 WOW64 重定向到 `C:\Program Files (x86)`，导致奇怪路径）。
  - 简化为单行 `StrCpy $INSTDIR "$PROGRAMFILES64\fluxing"`。
  - 同时修了 `ForceFluxingSuffix` 的 logic bug（0.18.3.0 版本里 `StrCmp` 链路 fall-through，导致所有路径都进 `not_fluxing` 分支，强制追加 `\fluxing`，出现 `fluxing\fluxing` 双重后缀）。
  - 同时修了 upgrade 路径逻辑：之前 `.onInit` 的 `ReadRegStr $R0 ...; StrCmp $R0 "" 0 skip` 读了注册表但没用它，$INSTDIR 始终是 /D= 值或默认值。现在加了 `StrCpy $INSTDIR $R0` 真正使用注册表值，实现"原地升级"（用户首次装在 D:\foo\fluxing，再升级时仍用 D:\foo\fluxing，除非显式传 /D= 覆盖）。

- **安装日志**
  - NSIS 原生支持 `/LOG=path` CLI flag，无需在脚本里 `LogSet`（`LogSet` 在标准 NSIS 不可用，需要自定义编译）。
  - 在 `.onInit` 加了文档说明，提示用户 / 部署脚本传 `/LOG=path\to\file.log` 来获得完整安装日志，作为 post-mortem 工具。
  - **注意**：使用 `/LOG=` 时**必须**用 cmd /c 调用，否则 PowerShell `Start-Process` 会把 `/LOG=` 合并到 `/D=` 里（NSIS 行为：unknown CLI args 串入 $INSTDIR）。

### 验收

- 全新 silent install（`/S /D=C:\TEMP\fluxing-0184-test`）得到 `fluxing-0184-test\ProgramFiles\fluxing\weasel\` 布局。
- 所有 `WeaselServer.exe` / `WeaselDeployer.exe` / `WeaselSetup.exe` / `uninstall.exe` / `rime.dll` / `weasel.dll` / `WinSparkle.dll` 都是 x86。
- `weaselx64.dll` 是 x64（TSF text input processor 必须是 x64）。
- `rime.dll` 约 3MB（lua plugin 已链接）。
- 注册表 `HKLM\SOFTWARE\Fluxing\Weasel\InstallDir` = 安装根（含 `\fluxing`）。
- 注册表 `HKCU\Software\Fluxing\Weasel\RimeUserDir` = `<install-root>\user1\fluxing`。
- AGENTS.md §2.5 强制 silent-install smoke test 通过。

## [0.18.3.0] - 2026-06-30

### 主要更新

- **安装路径强制以 `fluxing` 结尾** (修复 0.18.2.0 路径布局 bug)
  - `output/install.nsi` 的 `IsFluxingPath` 函数之前有 broken Exch/Pop 链，导致返回值恒为垃圾；`ForceFluxingSuffix` 实际从未生效。
  - 旧版本下，用户在 GUI 选 `D:\Program Files` 时，安装器把引擎二进制写到 `D:\Program Files\weasel\`、把用户数据写到 `D:\Program Files\fluxing\user1\fluxing\` —— 引擎与用户数据"分裂"在两处。
  - 修复后：
    - 重写 `ForceFluxingSuffix` 为基于 label + `StrCmp` 的简单逻辑（不再依赖 Exch stack juggling）。
    - 在 `.onInit` 的 `skip:` 标签之后显式 `Call ForceFluxingSuffix` —— 之前仅靠 `MUI_PAGE_CUSTOMFUNCTION_LEAVE` 触发，**silent install (`/S` + `/D=`) 下根本不会执行**。
    - `output/install.nsi` L534-535 的 user-data 路径由 `$R3\fluxing\user1\fluxing` 修正为 `$R3\user1\fluxing`（避免与 `$R3` 末尾的 `\fluxing` 重复成 `\fluxing\fluxing`）。
  - 验证：`xbuild.bat installer` + silent install `/D=C:\TEMP\fluxing-0183-test` → 产生
    ```
    fluxing-0183-test\
      └─ fluxing\              ← 安装根
           ├─ weasel\          ← 引擎二进制
           └─ user1\           ← 用户数据
                └─ fluxing\
    ```
    注册表 `HKLM\...\InstallDir` 与 `HKCU\...\RimeUserDir` 同步更新。
  - **L13** lessons-learned 记录了根因 + 修复全过程 + 教训。

### 验收

- 全新安装：GUI 与 silent (`/S` + `/D=`) 都得到 `<chosen>\fluxing\weasel\` 布局。
- 注册表 `HKLM\SOFTWARE\Fluxing\Weasel\InstallDir` = 安装根（含 `\fluxing`）。
- 注册表 `HKCU\Software\Fluxing\Weasel\RimeUserDir` = `<install-root>\user1\fluxing`。

## [Unreleased] - 2026-06-28

### 主要更新

- 品牌重命名为"火流猩输入法 / Fluxing"
  - 用户可见字符串、安装器名称、桌面/开始菜单快捷方式、"程序和功能"卸载项名称等已统一为新品牌名。
  - 上游 RIME / 中州韻输入法引擎与开发者归属信息保持不变。
  - 关联：内部常量与图标重命名见 9f2b217（Fluxing 分枝首提交）。
<a name="0.17.4"></a>

- 端到端 Windows 构建流水线（spec 003）
  - 在 x64 与 x86 (Win32) 两种架构上完成 librime 1.13.1 的构建（glog/gtest/leveldb/marisa/opencc/yaml-cpp 六个第三方依赖 + rime 引擎本身）。
  - 通过 msbuild 在 Release|x64 与 Release|Win32 下构建 weasel.sln（WeaselTSF/WeaselUI/WeaselIPC/WeaselServer/WeaselDeployer/WeaselSetup/RimeWithWeasel），产物落到 output\ 与 output\Win32\。
  - 通过 NSIS 3.x 生成 output\archives\fluxing-0.17.4.0-installer.exe（约 10.5 MB）并复制到 elease\。
  - 新增 	ools\win-shims\include\ 仓库级 shim 头文件（X11/keysym.h、utf8.h、darts.h），通过 	ools\win-shims\setup-shims.ps1 复制到 librime/include/。
  - 修正 output/install.nsi：
    - 5 处默认安装路径 $PROGRAMFILES*\Rime 改为 $PROGRAMFILES*\Fluxing（L135/137/139/144/146），使默认建议路径即以 \Fluxing 结尾。
    - 移除多余的 UTF-8 BOM（4× → 1×），NSIS 3.x 不再解析失败。
  - 说明：本次提交**不**包含 librime 子模块内的 librime/build.bat 调试 echo 与 cmake 引用修复（仍保留在工作区为 dirty 状态）。后续若建立 kizemo/librime 派生仓库，可将这些补丁合入子模块。

- 相关 spec/plan/tasks：.specify/specs/003-windows-build-pipeline/{spec,plan,tasks}.md。

## [0.17.4](https://github.com/rime/weasel/compare/0.17.3...0.17.4)(2025-06-04)

### 主要更新
* 修复#1585 未处理完整的用户目录路径打开处理问题

#### Code Refactor
refactor(WeaselTSF): add error handling when try open RimeUserDir ([fxliang](https://github.com/rime/weasel/commit/7a52fce2e6f5a991c58f2a19c93e82d3cfa191d3))

#### Bug Fixes
fix(WeaselSetup): RimeUserDir in registry is empty when default location ([fxliang](https://github.com/rime/weasel/commit/1ffd4412005962e0f9dd88558f23ec37e982ce59))

<a name="0.17.3"></a>
## [0.17.3](https://github.com/rime/weasel/compare/0.17.0...0.17.3)(2025-05-24)

### 主要更新
* 修复未自定义设定用户目录潜在可能无法在右键菜单打开用户目录的问题
* 修复配色方案中未定义色回退错误问题
* 回退#1499，修复由之产生的inline_preedit失效问题

#### Bug Fixes
fix(WeaselTSF): explore user dir failed if it's not customized ([fxliang](https://github.com/rime/weasel/commit/facecbf2d29cb45ee695e5a27e68f76dd796264a))
fix(RimeWithWeasel): color parsing for decimal number fix(RimeWithWeasel): highlight label color and highlight comment color not correct when it's not defined ([fxliang](https://github.com/rime/weasel/commit/c52f260c603aa5b19b084317c22c978e61cbbaab))

#### Commits
Revert "fix(tsf): ime status (#1499)" ([居戎氏](https://github.com/rime/weasel/commit/c72cbc8e002f88c2b24b6866e28260897e49d1fe))

<a name="0.17.0"></a>
## [0.17.0](https://github.com/rime/weasel/compare/0.16.3...0.17.0)(2025-05-17)

### 主要更新
* 更新 librime 至 1.13.1 版本
* 修復托盤圖標卡死問題
* 修復當熱鍵設置為空時 WeaselDeployer 崩潰的問題
* 修復更新安裝後可能導致重啟後程式檔案被刪除的問題
* 修復多線程導致的服務崩潰問題
* 修復部分應用程式中的異常崩潰問題
* 修復部分應用中無法顯示輸入法的問題
* 修復因顯示卡重置導致的文字繪製失敗問題
* 修復「天圓地方」狀態下編碼高亮未正確繪製的問題
* 修復 vim-mode 下按鍵響應異常問題
* 修復輸入法顯示狀態異常問題
* 修復全螢幕模式下高亮背景繪製錯誤問題
* 修正混色算法，解決部分情況下的混色異常問題
* `WeaselDeployer.exe` 和 `WeaselSetup.exe` 新增 `/h` 及 `/help` 參數，顯示使用說明
* `WeaselSetup.exe` 新增參數支援設定用戶資料目錄，例如：`WeaselSetup.exe /userdir:D:\rime_data_dir`

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
feat: WeaselSetup 默认启动不请求管理员权限，必要时使用管理员权限重启 (#1390) ([Wendy](https://github.com/rime/weasel/commit/ba768a6d65895837b052a1d366ffb872df5f0091))

#### Chores
chore: update bump version scripts ([fxliang](https://github.com/rime/weasel/commit/967674ff5295c4a389b35e9f8070b9fe43d0dcb1))
chore: update update/bump-version.ps1 [skip ci] ([fxliang](https://github.com/rime/weasel/commit/d13fd1250545d05df6573be7c0dee2529a4dc3fc))
chore: update update/bump_version.sh [skip ci] ([fxliang](https://github.com/rime/weasel/commit/8d12cafec0a84498c3f32d6821b7ccbd85fe1f21))
chore: follow #1379, update `update/bump-version.sh` to work without clog[skip ci] ([fxliang](https://github.com/rime/weasel/commit/d75b34bce20026f54ebbae73b6c591b3db473d11))
chore: make clang-format.ps1 worked in linux/Mac OS[skip ci] ([fxliang](https://github.com/rime/weasel/commit/d3e872c6671aaab5bfc0a6caa8227b416a5c5601))
chore: update update/bump_version.ps1 ([fxliang](https://github.com/rime/weasel/commit/18cb65206c494e5b3bc21380dc938d5553a7e83a))
chore: add powershell script for linting ([fxliang](https://github.com/rime/weasel/commit/a6d15cea4ad14c921bbde4c6b9c99a8a15c4dcde))
chore: update .gitignore ([fxliang](https://github.com/rime/weasel/commit/094e99de9e47b0aa9469b3d94a03e78501d8b1bb))
chore(install_boost): update boost download url ([居戎氏](https://github.com/rime/weasel/commit/235308dc7425529b49ffe3a5eb29947a4657f8cd))

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
fix(ipcserver): concurrent access to rime api ([居戎氏](https://github.com/rime/weasel/commit/2dc4e1923a95d8ad4c1eff19399614253507c0fe))
fix(RimeWithWeasel): blend_colors algorithm, fix issue like #1405 ([fxliang](https://github.com/rime/weasel/commit/5dbafbb893cd79c876dab8600833266ce12ecdbd))
fix(WeaselUI): highlight back is not drawn correctly when fullscreen layout set ([fxliang](https://github.com/rime/weasel/commit/8b95887f4e2375659ed228e921f481a462b97376))
fix(CandidateList): null pointer error ([居戎氏](https://github.com/rime/weasel/commit/588a31f8eedf6066e5b6a1cc7f010ed528154445))
fix: silent installation script repeated call ([居戎氏](https://github.com/rime/weasel/commit/150c5608ba5338cedf124a0c1adf2caf5948a6cf))
fix: silent installation script typo ([Yh793](https://github.com/rime/weasel/commit/c599f2e67ab26dac3f77066d4d57d9295092be96))
fix: fix unexpected crash in some applications (#1458) ([Alfred Lieu](https://github.com/rime/weasel/commit/3f1e05b255867b8d9d31212fe840bcfa8f23b50c))
fix: candidate ui can't be drawn correctly after GPU reset ([fxliang](https://github.com/rime/weasel/commit/37c8fa161a221a263bb439a1158ba401c0cf90a3))

#### Commits
remove duplicated branch ([Qijia Liu](https://github.com/rime/weasel/commit/78e20ab7893ffe07904ef10eebe55c27bb05cc2a))
refactorï(RimeWithWeasel) simplify color parsing function ([fxliang](https://github.com/rime/weasel/commit/836dc9e35c25564fb4b8ab95e575afa4454ee5f3))

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

#### 安裝須知

**⚠️如您由0.16.0之前的版本升級，由於參數變化，安裝小狼毫前請保存好文件資料，於安裝後重啓或註銷 Windows，否則正在使用小狼毫的應用可能會崩潰。**

**⚠如您由0.16.0之前的版本升級，請確認您的 `installation.yaml` 文件編碼爲 `UTF-8`, 否則如您在其中修改了非 ASCII 字符內容的路徑時，有可能會引起未明錯誤。**

#### 主要更新
* 新特性：支持自動檢查更新使用測試通道，使用`WeaselSetup.exe`參數可修改，`/testing`使用測試通道，`/release`使用發佈版本，默認後者；
* 新特性：`WeaselSetup.exe`參數設置界面語言，设置后覆盖区域设置的自动检测。`/lt`設置爲繁體中文界面，`/ls` 設置爲簡體中文界面，`/le`設置爲英文界面
* 新特性：`WeaselSetup.exe`參數設置是否使用自動檢查更新，`/du`禁用自動檢查更新，`/eu`使用自動檢查更新
* 新特性：安裝器彈窗提示設置是否自動檢查升級
* 新特性：開關IME消息響應狀態可配置，`WeaselSetup.exe`參數`/toggleime`設置關閉鍵盤（原版本狀態），`/toggleascii`切換`ascii_mode`,安裝默認後者 #1364
* 新特性：支持xmake 2.9.4以上版本構建，使用`xbuild.bat`開展，相關參數基本同`build.bat`, 使用`xbuild.bat commands`可生成`compile_commands.json`便於lsp使用，`xbuild.bat clean`可清空xmake構建 #1360
* 新特性：支持`Caps_Lock` 按鍵binding（如選重）,需将`key_binder`置于`ascii_composer`之前
* 使能TSF dll中的WER
* nightly 構建後自動更新rime/home頁面更新測試通道appcast
* 升級lint檢查使用的llvm最低版本至18.1.6, 更新ci脚本检查更新llvm

#### Bug 修復

* 修復安裝器在系統未滿足要求時未中斷的問題
* 修復重新安裝時舊的安於路徑未保持的問題
* 修復界面語言根據區域格式未正確設置的問題
* 修復IPC通信時因新舊版本變更引起的異常崩潰的問題
* 修正代碼編碼格式
* 修復清空舊log文件
* 修復控制面板卸載界面中的圖標顯示問題
* 修復`style/hover_type`爲`"semi_hilite"`在首候選時的顯示異常問題
* 修復新版librime產物未能直接替換使用問題
* 禁用IPC通信的異步機制，修復一些因異步機制引發的應用異常
* 修復構建腳本不能重生成正確的版本信息問題
* 修復一些vs工程配置設置，處理一些deprecated API警告


<a name="0.16.1"></a>
## [0.16.1](https://github.com/rime/weasel/compare/0.16.0...0.16.1) (2024-06-06)


#### 安裝須知

**⚠️如您由0.16.0之前的版本升級，由於參數變化，安裝小狼毫前請保存好文件資料，於安裝後重啓或註銷 Windows，否則正在使用小狼毫的應用可能會崩潰。**

**⚠如您由0.16.0之前的版本升級，請確認您的 `installation.yaml` 文件編碼爲 `UTF-8`, 否則如您在其中修改了非 ASCII 字符內容的路徑時，有可能會引起未明錯誤。**

#### 主要更新
* 爲`WeaselServer.exe`使能Windows Error Reporting, 提供對應的`WeaselServer.pdb`文件, 在`WeaselServer.exe`崩潰時可以生成dmp報告文件在日誌文件夾中
* 提供`WeaselServer.exe`守護，在服務崩潰後6個按鍵事件（三次擊鍵Down&Up)後拉起服務
* 新增英文界面語言
* 更新7z和curl到最新版本，修復一些因爲7z的bug引起的問題
* 優化預覽圖PNG文件大小
* 新增語言欄菜單，打開日誌文件夾，調整日誌文件夾路徑爲`%TEMP%\rime.weasel`,方便查閱管理
* 異步處理消息，避免服務崩潰時長時間未響應引起客戶端程序崩潰
* 不在服務中部署方案，避免在守護拉起服務進入長耗時部署引起的僵死問題

#### Bug 修復

* 修復自動折行未正確處理標點符號（標點在折行後最前）的問題
* 修復`vim-mode`下的typo引起的`<C-C>`無法生效問題
* 修復部署消息未更新問題
* 修復卸載小狼毫時意外安裝語言包問題
* 修復`semi_hilite`下的UI未正確響應問題, `semi_hilite`顏色調整爲高亮色的半透明度狀態，改善體驗
* 減少不必要的服務端UI更新，提高性能減少服務崩潰機率
* 修復在非`DPI=96`的副屏上響應慢的問題
* 修復在高分屏上layout參數未dpi aware問題
* 修復Windows 11下Chrome等瀏覽器中非激活光標狀態下的按鍵響應異常問題
* 修復64位系統下默認安裝路徑不準確的問題



<a name="0.16.0"></a>
## [0.16.0](https://github.com/rime/weasel/compare/0.15.0...0.16.0) (2024-05-14)


#### 安裝須知

**⚠️由於參數變化，安裝小狼毫前請保存好文件資料，於安裝後重啓或註銷 Windows，否則正在使用小狼毫的應用可能會崩潰。**

**⚠請確認您的 `installation.yaml` 文件編碼爲 `UTF-8`, 否則如您在其中修改了非 ASCII 字符內容的路徑時，有可能會引起未明錯誤。**

#### 主要更新

* 升級核心算法庫至 [librime 1.11.2](https://github.com/rime/librime/releases/tag/1.11.2)
* 改善輸入法病毒誤報問題
* 新增 64 位算法服務程序，支持 64 位 librime，支持大內存（可部署大規模詞庫方案）
* 支持 arm/arm64 架構
* 單安裝包支持 win32/x64/arm/arm64 架構系統的自動釋放文件
* 32 位算法服務增加 LARGE ADDRESS AWARE 支持
* 升級 boost 算法庫至 1.84.0
* IME改爲可選項，默認不安裝
* 棄用 `weaselt*.dll`，增加註冊香港、澳門、新加坡區域語言配置（默認未啓用，需在控制面板/設置中手工添加）；支持簡繁體小狼毫同時使能
* 棄用 `weaselt*.ime`
* 移除 `pyweasel`
* 候選窗口 UI 內存優化
* 改善候選窗口 UI 繪製性能
* 升級 WTL 庫至 10.0，gdi+ 至 1.1
* 每顯示器 dpi aware，自適應不同顯示器不同 dpi 設定變化
* 更新高清圖標
* 增大 IPC 數據長度限制至 64k，支持長候選
* 升級 plum
* 應用界面及菜單簡繁體自動適應
* `app_options` 中應用名大小寫不敏感
* 字體抗鋸齒設定參數 `style/antialias_mode: {force_dword|cleartype|grayscale|aliased|default}`
* ASCII狀態提示跟隨鼠標光標設定 `style/ascii_tip_follow_cursor: bool`
* 新增參數 `style/layout/hilite_padding_x: int`、`style/layout/hilite_padding_y: int`，支持分別設置xy向的 padding
* 新增參數 `schema/full_icon: string`, `schema/half_icon: string`，支持在方案中設定全半角圖標
* 新增參數 `style/text_orientation: "horizontal" | "vertical"`, 與 `style/vertical_text: bool` 冗餘，設定文字繪製方向，兼容 squirrel 參數
* 新增參數 `style/paging_on_scroll: bool`，可設定滾輪相應類型（翻頁或切換前後候選）
* 新增參數，Windows10 1809後版本的Windows，支持 `style/color_scheme_dark: string` 設定暗色模式配色
* 新增參數 `style/candidate_abbreviate_length: int`，支持候選字數超限時縮略顯示
* 新增參數 `style/click_to_capture: bool` 設定鼠標點擊是否截圖
* 新增參數 `show_notifications_time: int` 可設定提示顯示時間，單位 ms；設置 0 時不顯示提示
* 新增參數 `show_notifications: bool` 或 `show_notifications: 開關列表 | "schema"`，可定製是否顯示切換提示、顯示那些切換提示
* 新增參數 `style/layout/baseline: int` 和 `style/layout/linespacing: int`，可自行調整參數修復候選窗高度跳躍閃爍問題
* 棄用 `style/mouse_hover_ms`；新增 `style/hover_type: "none"|"semi_hilite"|"hilite"`，改善鼠標懸停相應體驗
* 新增參數 `global_ascii: bool`, 支持全局 ascii 模式
* 新增 `app_options`，支持應用專用 `vim_mode: bool`，支持常見 vim 切換 normal 模式按鍵時，切換到 `ascii_mode`
* 新增 `app_options`，支持應用專用 `inline_preedit: bool` 設定，優先級高於方案內設定，高於 `weasel.yaml` 中的設定
* 支持命令行設置小狼毫 `ascii_mode` 狀態，`WeaselServer.exe /ascii`，`WeaselServer.exe /nascii`
* 支持設置 `comment_text_color`、`hilited_comment_text_color` 透明來隱藏對應文字顯示
* `hilited_mark_color` 非透明，`mark_text` 爲空字符串時，類 windowns 11 的高亮標識
* 切換方案後，提示方案圖標和方案名字
* 支持全部 switch 提示使用方案內設定的 label
* WeaselSetup通過打開目錄窗口設置用戶目錄路徑
* 新增支持方案內定義方案專用配色
* 支持 imtip
* 增加類微軟拼音的高亮標識在鼠標點擊時的動態
* 支持在字體設定任一分組中設置字體整體的字重或字形
* 優化點擊選字邏輯
* 豎直佈局反轉時，互換上下方向鍵
* 候選窗超出下方邊界時，在當前合成結束前保持在輸入位置上方，減少候選窗口高度變小時潛在的窗口上下跳動
* 調整 TSF 光標位置（`inline_preedit: false` 時），減少光標閃爍
* WeaselSetup 修改用戶目錄路徑（已安裝時）
* 語言欄新增菜單，重啓服務
* IPC 報文轉義 `\n`、`\t`，不再因 `\n` 引發應用崩潰
* 使用 clang-format 格式化代碼，統一代碼風格
* 自動文件版本信息
* 測試項目 test 只在 debug 配置狀態下編譯構建

#### Bug 修復

* 修復 word 365 中候選窗閃爍無法正常顯示的問題
* 修復 word 行尾輸入時候選窗反覆跳動問題
* 修復 word 中無法點擊選詞問題
* 修復 excel 等應用中，第一鍵 keydown 時未及時彈出候選窗問題
* 修復導出詞典數據後引起的多個 explorer 進程的問題，優化對應對話框界面顯示
* 修復打開用戶目錄，程序目錄引起的多個 explorer.exe 進程問題，支持服務未啓動時打開這些目錄
* 修復系統托盤重啓後未及時顯示的問題
* 修復 `style/layout/min_width` 在部分佈局下未生效問題
* 修復 preedit 寬高計算錯誤問題
* 修復翻頁按鈕在豎直佈局反轉時位置錯誤
* 修復豎直佈局帶非空 mark_text 時的計算錯誤
* 修復 composing 中候選窗隨文字移動問題
* 修復 wezterm gpu 模式下無法使用問題
* 修復 `style/inline_preedit: true` 時第一鍵輸入時候選窗位置錯誤
* 修復算法服務單例運行
* 修復調用 WeaselServer.exe 未正常重啓服務問題
* 修復偶發的顯卡關聯文字空白問題
* 修復部署過程中如按鍵輸入引發的重複發出 tip 提示窗問題
* 修復部分方案中的圖標顯示（`english.schema.yaml`）
* 修復 `preedit_type: preview` 時的光標錯誤問題
* 修復 `shadow_color` 透明時截圖尺寸過大問題，減小截圖尺寸
* 修復天園地方時，高亮候選圓角半徑不正確問題
* 修復某些狀態下天園地方的 preedit 背景色圓角異常問題
* 修復候選尾部空白字符引起的佈局計算錯誤問題
* 修復 mark_text 繪製鋸齒問題
* 修復靜默安裝彈窗問題
* 修復 librime-preedit 引起的應用崩潰問題
* 修復 plum 用戶目錄識別錯誤問題
* 修復安裝後未在控制面板中添加輸入法、卸載後未刪除控制面板中的輸入法清單問題
* 修復一些其他已知的 bug

#### 已知問題

* 部分應用仍存在輸入法無法輸入文字或響應異常的問題
* WeaselServer 仍可能發生崩潰
* 仍有極少部分防病毒軟件可能誤報病毒



<a name="0.15.0"></a>
## [0.15.0](https://github.com/rime/weasel/compare/0.14.3...0.15.0) (2023-06-06)


#### 安裝須知

**⚠️安裝小狼毫前請保存好文件資料，於安裝後重啓 Windows ，否則正在使用小狼毫的應用將會崩潰。**
**⚠️此版本的小狼毫需要使用 Windows 8.1 或更高版本的操作系統。**

#### 主要更新

* 升級核心算法庫至 [librime 1.8.5](https://github.com/rime/librime/blob/master/CHANGELOG.md#185-2023-02-05)
* DPI 根據顯示器自動調整
* 支持候選窗口等圓角顯示
  * `style/layout/corner_radius: int`
* 兼容鼠鬚管中高亮圓角參數`style/layout/hilited_corner_radius: int`
* 支持主題顏色中含有透明通道代碼, 支持格式 0xaabbggrr，0xbbggrr, 0xabgr, 0xbgr
* 配色主題支持默認ABGR順序，或ARGB、RGBA順序
  * `preset_color_schemes/color_scheme/color_format: "argb" | "rgba" | ""`
* 支持編碼/高亮候選/普通候選/輸入窗口/候選邊框的陰影顏色繪製
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
* 支持自定義標籤、註解字體及字號
  * `style/label_font_face: string`
  * `style/comment_font_face: string`
  * `style/label_font_point: int`
  * `style/comment_font_point: int`
  * `style/layout/align_type: "top" | "center" | "bottom"`
* 支持指定字符 Unicode 區間字體設定
* 支持字重，字形風格設定
  * `style/font_face: font_name[:start_code_point:end_code_point][:weight_set][:style_set][,font2...]`
    * example: `"Segoe UI Emoji:20:39:bold:italic, Segoe UI Emoji:1f51f:1f51f, Noto Color Emoji SVG:80, Arial:600:6ff, Segoe UI Emoji:80, LXGW Wenkai Narrow"`
* 支持自定义字体回退範圍、順序定义
* 彩色字體支持
  * Windows 10 周年版前：需要使用 COLR 格式彩色字體
  * Windows 11 ：可以使用 SVG 字體
* 新增豎直文字佈局
  * `style/vertical_text: bool`
  * `style/vertical_text_left_to_right: bool`
  * `style/vertical_text_with_wrap: bool`
* 新增豎直佈局vertical窗口上移時自動倒序排列
  * `style/vertical_auto_reverse: bool`
* 新增「天圓地方」佈局：由 margin 與 hilite_padding 確定, 當margin <= hilite_padding時生效
* margin_x 或 margin_y 設置爲負值時，隱藏輸入窗口，不影響方案選單顯示
* 新增 preedit_type: preview_all ，在輸入時將候選項顯示於 composition 中
  * `style/preedit_type: "composition" | "preview" | "preview_all"`
* 新增輸入法高亮提示標記
  * `style/mark_text: string`
* 新增輸入方案圖標顯示，可在語言欄中顯示，文件格式爲ico
  * `schema/icon: string`
  * `schema/ascii_icon: string`
* 新增選項，允許在光標位置獲取失敗時於窗口左上角繪製候選框（而不是桌面左上角）
  * `style/layout/enhanced_position: bool`
* 新增鼠標點擊截圖到剪貼板功能
* 新增選項，支持越長自動折行/換列顯示
  * `style/layout/max_width: int`
  * `style/layout/max_height: int`
* 支持方案內設定配色
  * `style/color_scheme: string`
* 支持多行内容顯示，\r, \n, \r\n均支持
* 支持方案內設定配色
* 繪製性能提升
* composition 模式下新增下劃線顯示
* 隨二進制文件提供調試符號

#### Bug 修復

* 轉義日文鍵盤中特殊按鍵
* 候選文字過長時崩潰
* 修復用戶目錄下無 `default.custom.yaml` 或 `weasel.custom.yaml` 時，設定窗口無法彈出的問題
* 方案中設定inline_preedit爲true時，部署後編碼末端出現異常符號
* 部分應用無法輸入文字的問題
* 修復部署時無顯示提示的問題
* 修復中文路徑相關問題
* 修復右鍵菜單打開程序目錄/用戶目錄時，資源管理器無響應的問題
* 修復部分內存訪問問題
* 修復操作系統 / WinGet 無法識別小狼毫版本號的問題
* 修復 composition 模式下光標位置不正常的問題
* 修復 Word 中小狼毫工作不正常的問題
* 若干開發環境配置問題修復

#### 已知問題

* 部分應用仍存在輸入法無法輸入文字的問題
* WeaselServer 仍可能發生崩潰
* 部分防病毒軟件可能誤報病毒



<a name="0.14.3"></a>
## 0.14.3 (2019-06-22)


#### 主要更新

* 升級核心算法庫 [librime 1.5.3](https://github.com/rime/librime/blob/master/CHANGELOG.md#153-2019-06-22)
  * 修復 `single_char_filter` 組件
  * 完善上游項目 `librime` 的全自動發佈流程，免去手工上傳構建結果的步驟



<a name="0.14.2"></a>
## 0.14.2 (2019-06-17)


#### 主要更新

* 升級核心算法庫 [librime 1.5.2](https://github.com/rime/librime/blob/master/CHANGELOG.md#152-2019-06-17)
  * 修復用戶詞的權重，穩定造句質量、平衡翻譯器優先級 [librime#287](https://github.com/rime/librime/issues/287)
  * 建議 0.14.1 版本用家升級



<a name="0.14.1"></a>
## 0.14.1 (2019-06-16)


#### 主要更新

* 升級核心算法庫 [librime 1.5.1](https://github.com/rime/librime/blob/master/CHANGELOG.md#151-2019-06-16)
  * 修復未裝配語言模型時缺省的造句算法 ([weasel#383](https://github.com/rime/weasel/issues/383))



<a name="0.14.0"></a>
## 0.14.0 (2019-06-11)


#### 主要更新

* 升級核心算法庫 [librime 1.5.0](https://github.com/rime/librime/blob/master/CHANGELOG.md#150-2019-06-06)
  * 遷移到VS2017構建工具；建設安全可靠的全自動構建、發佈流程
  * 通過更新第三方庫，修復userdb文件夾大量佔用磁盤空間的問題
  * 將Rime插件納入自動化構建流程。本次發行包含兩款插件：
    - [librime-lua](https://github.com/hchunhui/librime-lua)
    - [librime-octagram](https://github.com/lotem/librime-octagram)
* 高清重製真彩輸入法狀態圖標


#### Features

* **ui:**  high-res status icons; display larger icons in WeaselPanel ([093fa806](https://github.com/rime/weasel/commit/093fa80678422f972e7a7285060553eeedb0e591))



<a name="0.13.0"></a>
## 0.13.0 (2019-01-28)


#### 主要更新

* 升級核心算法庫 [librime 1.4.0](https://github.com/rime/librime/blob/master/CHANGELOG.md#140-2019-01-16)
  * 新增 [拼寫糾錯](https://github.com/rime/librime/pull/228) 選項
    當前僅限 QWERTY 鍵盤佈局及使用 `script_translator` 的方案
  * 修復升級、部署數據時發生的若干錯誤
* 更換輸入法狀態圖標，適配高分辨率屏幕


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

#### 主要更新

* 合併小狼毫與小狼毫（TSF）兩種輸入法
* 合併32位與64位系統下的安裝程序
* 使用系統的關閉輸入法功能（默認快捷鍵 Ctrl + Space）後，輸入法圖標將顯示禁用狀態
* 修復一些情況下的崩潰問題
* 升級核心算法庫 [librime 1.3.2](https://github.com/rime/librime/blob/master/CHANGELOG.md#132-2018-11-12)
  * 允許多個翻譯器共用同一個詞典時的組詞，實現固定單字順序的形碼組詞([librime#184](https://github.com/rime/librime/issues/184))。
  * 新增 translator/always_show_comments 選項，允許始終顯示候選詞註解。

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

#### 主要更新

* 修復了在 Excel 中奇怪的輸入丟失問題（[#185](https://github.com/rime/weasel/issues/185)）
* 功能鍵不再會觸發輸入焦點（[#194](https://github.com/rime/weasel/issues/194)、[#195](https://github.com/rime/weasel/issues/195)、[#204](https://github.com/rime/weasel/issues/204)）
* 「獲取更多輸入方案」功能優化（[#180](https://github.com/rime/weasel/issues/180)）
* 修復了後臺可能同時出現多個算法服務的問題（[#199](https://github.com/rime/weasel/issues/199)）
* 恢復語言欄右鍵菜單中「用戶資料同步」一項

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

#### 主要更新

* 新增 [Rime 配置管理器](https://github.com/rime/plum)，通過「輸入法設定／獲取更多輸入方案」調用
* 在輸入法語言欄顯示狀態切換按鈕（TSF 模式）
* 修復多個前端兼容性問題
* 新增配色主題「現代藍」`metroblue`、「幽能」`psionics`
* 安裝程序支持繁體中文介面
* 修復 0.10 版升級安裝後，因用戶文件夾中保留舊文件、配置不生效的問題
* 升級 0.9 版 `.kct` 格式的用戶詞典
  **注意**：僅此一個版本支持格式升級，請務必由 0.9 升級到 0.11，再安裝後續版本

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
## 小狼毫 0.10.0 (2018-03-14)


#### 主要更新

* 兼容 Windows 8 ~ Windows 10
* 支持高分辨率顯示屏
* 介面風格選項
  * 在內嵌編碼行預覽結果文字
  * 可指定候選序號的樣式
* 升級核心算法庫 [librime 1.3.0](https://github.com/rime/librime/blob/master/CHANGELOG.md#130-2018-03-09)
  * 支持 YAML 節點引用，方便模塊化配置
  * 改進部署流程，在 `build` 子目錄集中存放生成的數據文件
* 精簡安裝包預裝的輸入方案，更多方案可由 [東風破](https://github.com/rime/plum) 取得

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
## 小狼毫 0.9.30 (2014-04-01)


#### Rime 算法庫變更集

* 新增：中西文切換方式 `clear`，切換時清除未完成的輸入
* 改進：長按 Shift（或 Control）鍵不觸發中西文切換
* 改進：並擊輸入，若按回車鍵則上屏按鍵對應的字符
* 改進：支持對用戶設定中的列表元素打補靪，例如 `switcher/@0/reset: 1`
* 改進：缺少詞典源文件 `*.dict.yaml` 時利用固態詞典 `*.table.bin` 完成部署
* 修復：自動組詞的詞典部署時未檢查【八股文】的變更，導致索引失效、候選字缺失
* 修復：`comment_format` 會對候選註釋重複使用多次的BUG

#### 【東風破】變更集

* 新增：快捷鍵 `Control+.` 切換中西文標點
* 更新：【八股文】【朙月拼音】【地球拼音】【五筆畫】
* 改進：【朙月拼音·語句流】`/0` ~ `/10` 輸入數字符號



<a name="0.9.29.1"></a>
## 小狼毫 0.9.29.1 (2013-12-22)


#### 【小狼毫】變更集

* 變更：不再支持 Windows XP SP2，因升級編譯器以支持 C++11
* 修復：輸入語言選爲中文（臺灣）在 Windows 8 系統上出現多餘的輸入法選項
* 修復：升級安裝後，外觀設定介面未及時顯示出新增的配色方案
* 修復：配色方案 Google+ 的預覽圖

#### Rime 算法庫變更集

* 更新：librime 升級到 1.1
* 新增：固定方案選單排列順序的選項 `default.yaml`: `switcher/fix_schema_list_order: true`
* 修復：正確匹配嵌套的“‘彎引號’”
* 改進：碼表輸入法自動上屏及頂字上屏（[示例](https://gist.github.com/lotem/f879a020d56ef9b3b792)）<br/>
    若有 `speller/auto_select: true`，則選項 `speller/max_code_length:` 限定第N碼無重碼自動上屏
* 優化：爲詞組自動編碼時，限制因多音字而產生的組合數目，避免窮舉消耗過量資源

#### 【東風破】變更集

* 更新：【粵拼】匯入衆多粵語詞彙
* 優化：調整部分異體字的字頻



<a name="0.9.28"></a>
## 小狼毫 0.9.28 <2013-12-01>


#### 【小狼毫】變更集

* 新增：一組配色方案，作者：P1461、Patricivs、skoj、五磅兔
* 修復：[Issue 528](https://code.google.com/p/rimeime/issues/detail?id=528) Windows 7 IE11 文字無法上屏
* 修復：[Issue 531](https://code.google.com/p/rimeime/issues/detail?id=531) Windows 8 卸載輸入法後在輸入法列表中有殘留項
* 變更：註冊輸入法時同時啓用 IME、TSF 模式

#### Rime 算法庫變更集

* 更新：librime 升級到 1.0
* 改進：`affix_segmentor` 支持向匹配到的代碼段添加標籤 `extra_tags`
* 修復：`table_translator` 按字符集過濾候選字，修正對 CJK-D 漢字的判斷

#### 【東風破】變更集

* 優化：【粵拼】兼容[教育學院拼音方案](http://zh.wikipedia.org/wiki/%E6%95%99%E8%82%B2%E5%AD%B8%E9%99%A2%E6%8B%BC%E9%9F%B3%E6%96%B9%E6%A1%88)
* 更新：`symbols.yaml` 由 Patricivs 重新整理符號表
* 更新：Emoji 提供更加豐富的繪文字（需要字體支持）
* 更新：【八股文】【朙月拼音】【地球拼音】【中古全拼】修正錯別字、註音錯誤



<a name="0.9.27"></a>
## 小狼毫 0.9.27 (2013-11-06)


#### 【小狼毫】變更集

* 變更：動態鏈接 `rime.dll`，減小程序文件的體積
* 修復：嘗試解決 Issue 487 避免服務進程以 SYSTEM 帳號執行
* 新增：開始菜單項「安裝選項」，Vista 以降提示以管理員權限啓動
* 優化：更換圖標，解決 Windows 8 TSF 圖標不清楚的問題

#### Rime 算法庫變更集

* 優化：同步用戶資料時自動備份自定義短語等 .txt 文件
* 修復：【地球拼音】反查拼音失效的問題
* 變更：編碼提示不再添加括弧（，）及逗號，可自行設定樣式

#### 輸入方案設計支持

* 新增：`affix_segmentor` 分隔編碼的前綴、後綴
* 改進：`translator` 支持匹配段落標籤
* 改進：`simplifier` 支持多個實例，匹配段落標籤
* 新增：`switches:` 輸入方案選項支持多選一
* 新增：`reverse_lookup_filter` 爲候選字標註指定種類的輸入碼

#### 【東風破】變更集

* 更新：【粵拼】補充大量單字的註音
* 更新：【朙月拼音】【地球拼音】導入 Unihan 讀音資料
* 改進：【地球拼音】【注音】啓用自定義短語
* 新增：【注音·臺灣正體】
* 修復：【朙月拼音·簡化字】通過快捷鍵 `Control+Shift+4` 簡繁切換
* 改進：【倉頡五代】開啓繁簡轉換時，提示簡化字對應的傳統漢字
* 變更：間隔號採用「·」`U+00B7`



<a name="0.9.26.1"></a>
## 小狼毫 0.9.26.1 (2013-10-09)

* 修復：從上一個版本升級【倉頡】輸入方案不會自動更新的問題



<a name="0.9.26"></a>
## 小狼毫 0.9.26 (2013-10-08)

* 新增：【倉頡】開啓自動造詞<br/>
  連續上屏的5字（依設定）以內的組合，或以連打方式上屏的短語，
  按構詞規則記憶爲新詞組；再次輸入該詞組的編碼時，顯示「☯」標記
* 變更：【五筆】開啓自動造詞；從碼表中刪除與一級簡碼重碼的鍵名字
* 變更：【地球拼音】當以簡拼輸入時，爲5字以內候選標註完整帶調拼音
* 新增：【五筆畫】輸入方案（`stroke`），取代 `stroke_simp`
* 新增：支持在輸入方案中設置介面樣式（`style:`）<br/>
  如字體、字號、橫排／直排等；配色方案除外
* 修復：多次按「.」鍵翻頁後繼續輸入，不應視爲網址而在編碼中插入「.」
* 修復：開啓候選字的字符集過濾，導致有時不出現連打候選詞的 BUG
* 修復：`table_translator` 連打組詞時產生的內存泄漏（0.9.25.2）
* 修復：爲所有用戶創建開始菜單項
* 更新：修訂【八股文】詞典、【朙月拼音】【地球拼音】【粵拼】【吳語】
* 更新：2013款 Rime 輸入法圖標



<a name="0.9.25.2"></a>
## 小狼毫 0.9.25.2 (2013-07-26)

* 改進：碼表輸入法連打，Shift+BackSpace 以字、詞爲單位回退
* 修復：演示模式下開啓內嵌編碼行、查無候選字時程序卡死



<a name="0.9.25.1"></a>
## 小狼毫 0.9.25.1 (2013-07-25)

* 新增：開始菜單項「檢查新版本」，手動升級到最新測試版
* 新增：【地球拼音】5 字內候選標註完整帶調拼音



<a name="0.9.25"></a>
## 小狼毫 0.9.25 (2013-07-24)

* 新增：演示模式（全屏的輸入窗口）`style/fullscreen: true`
* 新增：【倉頡】按快趣取碼規則生成常用詞組
* 修復：【地球拼音】「-」鍵輸入第一聲失效的BUG
* 更新：拼音、粵拼等輸入方案
* 更新：`symbols.yaml` 增加一批特殊字符



<a name="0.9.24"></a>
## 小狼毫 0.9.24 (2013-07-04)

* 新增：支持全角模式
* 更新：中古漢語【全拼】【三拼】輸入方案；三拼亦採用全拼詞典
* 修復：大陸與臺灣異讀的字「微」「檔」「蝸」「垃圾」等
* 修復：繁簡轉換錯詞「么么哒」
* 新增：（輸入方案設計用）可設定對特定類型的候選詞不做繁簡轉換<br/>
  如不轉換反查字使用選項 `simplifier/excluded_types: [ reverse_lookup ]`
* 新增：（輸入方案設計用）干預多個 translator 之間的結果排序<br/>
  選項 `translator/initial_quality: 0`
* 修復：用戶詞典未能完整支持 `derive` 拼寫運算產生的歧義切分



<a name="0.9.23"></a>
## 小狼毫 0.9.23 (2013-06-09)

* 改進：方案選單按選用輸入方案的時間排列
* 新增：快捷鍵 Control+Shift+1 切換至下一個輸入方案
* 新增：快捷鍵 Control+Shift+2~5 切換輸入模式
* 新增：初次安裝時由用戶指定輸入語言：中文（中國／臺灣）
* 新增：可屏蔽符合 fuzz 拼寫規則的單字候選，僅以其輸入詞組<br/>
  選項 `translator/strict_spelling: true`
* 改進：綜合候選詞的詞頻和詞條質量比較不同 translator 的結果
* 修復：自定義短語不應參與組詞
* 修復：八股文錯詞及「鏈」字無法以簡化字組詞的 BUG



<a name="0.9.22.1"></a>
## 小狼毫 0.9.22.1 (2013-04-24)

* 修復：禁止自定義短語參與造句
* 修復：GVim 裏進入命令模式或在插入模式換行錯使輸入法重置爲初始狀態



<a name="0.9.22"></a>
## 小狼毫 0.9.22 (2013-04-23)

* 新增：配色方案【曬經石】／Solarized Rock
* 新增：Control+BackSpace 或 Shift+BackSpace 回退一個音節
* 新增：固態詞典可引用多份碼表文件以實現分類詞庫
* 新增：在輸入方案中加載翻譯器的多個具名實例
* 新增：以選項 `translator/user_dict:` 指定用戶詞典的名稱
* 新增：支持從用戶文件夾加載文本碼表作爲自定義短語詞典<br/>
  【朙月拼音】系列自動加載名爲 `custom_phrase.txt` 的碼表
* 修復：繁簡轉換使無重碼自動上屏失效的 BUG
* 修復：若非以 Caps Lock 鍵進入西文模式，<br/>
  按 Caps Lock 只切換大小寫，不返回中文模式
* 變更：`r10n_translator` 更名爲 `script_translator`，舊名稱仍可使用
* 變更：用戶詞典快照改爲文本格式
* 改進：【八股文】導入《萌典》詞彙，並修正了不少錯詞
* 改進：【倉頡五代】打單字時，以拉丁字母和倉頡字母並列顯示輸入碼
* 改進：使自動生成的 YAML 文檔更合理地縮排、方便閱讀
* 改進：碼表中 `# no comments` 行之後不再識別註釋，以支持 `#` 作文字內容
* 改進：檢測到因斷電造成用戶詞典損壞時，自動在後臺線程恢復數據文件



<a name="0.9.20"></a>
## 小狼毫 0.9.20 (2013-02-01)

* 變更：Caps Lock 燈亮時默認輸出大寫字母 [Gist](https://gist.github.com/2981316)
  升級安裝後若 Caps Lock 的表現不正確，請註銷並重新登錄
* 新增：無重碼自動上屏 `speller/auto_select:`<br/>
  輸入方案【倉頡·快打模式】
* 改進：允許以空格做輸入碼，或作爲符號頂字上屏<br/>
  `speller/use_space:`, `punctuator/use_space:`
* 改進：【注音】輸入方案以空格輸入第一聲（陰平）
* 新增：特殊符號表 `symbols.yaml` 用法見↙
* 改進：【朙月拼音·簡化字】以 `/ts` 等形式輸入特殊符號
* 改進：標點符號註明〔全角〕〔半角〕
* 優化：同步用戶資料時更聰明地備份用戶自定義的 YAML 文件
* 修復：避免創建、使用不完整的詞典文件
* 修復：糾正用戶詞典中無法調頻的受損詞條
* 修復：用戶詞典管理／輸出詞典快照後定位文件出錯
* 修復：TSF 內嵌輸入碼沒有反選效果、候選窗位置頻繁變化



<a name="0.9.19.1"></a>
## 小狼毫 0.9.19.1 (2013-01-16)

* 新增：Caps Lock 點亮時，切換到西文模式，輸出小寫字母<br/>
  選項 `ascii_composer/switch_key/Caps_Lock:`
* 修復：Control + 字母编辑键在临时西文模式下无效
* 修復：用戶詞典有可能因讀取時 I/O 錯誤導致部份詞序無法調整
* 改進：用戶詞典同步／合入快照的字頻合併算法



<a name="0.9.18.6"></a>
## 小狼毫 0.9.18.6 (2013-01-09)

* 修復：從 0.9.16 及以下版本升級用戶詞典出錯



<a name="0.9.18.5"></a>
## 小狼毫 0.9.18.5 (2013-01-07)

* 修復：含簡化字的候選詞不能以音節爲單位移動光標
* 改進：同步用戶資料時也備份用戶修改的YAML文件



<a name="0.9.18"></a>
## 小狼毫 0.9.18 (2013-01-05)

* 新增：同步用戶詞典，詳見 [Wiki » UserGuide](https://code.google.com/p/rimeime/wiki/UserGuide)
* 新增：上屏錯誤的詞組後立即按回退鍵（BackSpace）撤銷組詞
* 改進：拼音輸入法中，按左方向鍵以音節爲單位移動光標
* 修復：【地球拼音】不能以 - 鍵輸入第一聲



<a name="0.9.17.1"></a>
## 小狼毫 0.9.17.1 (2012-12-25)

* 修復：設置爲默認輸入語言後再安裝，IME 註冊失敗
* 修復：啓用托盤圖標的選項無效
* 新增：從開始菜單訪問用戶文件夾的快捷方式
* 修復：【小鶴雙拼】拼音 an 顯示錯誤



<a name="0.9.17"></a>
## 小狼毫 0.9.17 (2012-12-23)

* 新增：切換模式、輸入方案時，短暫顯示狀態圖標
* 新增：隱藏托盤圖標，設定、部署、詞典管理請用開始菜單。<br/>
  配置項 `style/display_tray_icon:`
* 修復BUG：TSF 前端在 MS Office 裏不能正常上屏中文
* 刪除：默認不啓用 TSF 前端，如有需要可在「文本服務與輸入語言」設置對話框添加。
* 新增：分別以 `` ` ' `` 標誌編碼反查的開始結束，例如 `` `wbb'yuepinyin ``
* 改進：形碼與拼音混打的設定下，降低簡拼候選的優先級，以降低對逐鍵提示的干擾
* 優化：控制用戶詞典文件大小，提高大容量（詞條數>100,000）時的查詢速度
* 刪除：因有用家向用戶詞典導入巨量詞條，故取消自動備份的功能，後續代之以用戶詞典同步
* 修復：【小鶴雙拼】diao, tiao 等拼音回顯錯誤
* 更新：【朙月拼音】【地球拼音】【粵拼】修正用戶反饋的註音錯誤



<a name="0.9.16"></a>
## 小狼毫 0.9.16 (2012-10-20)

* 新增：TSF 輸入法框架（測試階段）及嵌入式編碼行
* 新增：支持 IE 8 ~ 10 的「保護模式」
* 新增：識別 gVim 模式切換
* 新增：開關碼表輸入法連打功能的設定項 `translator/enable_sentence: `
* 修復：「語句流」模式直接回車上屏不能記憶用戶詞組的BUG
* 改進：部署時自動編譯輸入方案的自訂依賴項，如自選的反查碼
* 改進：更精細的排版，修正註釋文字寬度、調整間距
* 改進：未曾翻頁時按減號鍵，不上屏候選字及符號「-」以免誤操作
* 變更：《注音》以逗號或句號（<> 鍵）上屏句子，書名號改用 [] 鍵
* 更新：《朙月拼音》《地球拼音》《粵拼》，修正多音字
* 更新：《上海吳語》《上海新派》，修正註音
* 新增：寒寒豆作《蘇州吳語》輸入方案，方案標識爲 `soutzoe`
* 新增：配色方案【谷歌／Google】，skoj 作品



<a name="0.9.15"></a>
## 小狼毫 0.9.15 (2012-09-12)

* 新增：橫排候選欄——歡迎 wishstudio 同學加入開發！
* 新增：綠色安裝工具 WeaselSetup，註冊輸入語言、自訂用戶目錄
* 新增：碼表輸入法啓用用戶詞典、字頻調整
* 優化：自動編譯輸入方案依賴項，如五筆·拼音的反查詞典
* 修改：日誌系統改用glog，輸出到 `%TEMP%\rime.weasel.*`
* 修復：托盤圖標在重新登錄後不可見的BUG
* 更新：【明月拼音】【粵拼】【吳語】修正註音錯誤、缺字



<a name="0.9.14.2"></a>
## 小狼毫 0.9.14.2 (2012-07-13)

* 重新編譯了 `opencc.dll` 安全軟件不吭氣了



<a name="0.9.14.1"></a>
## 小狼毫 0.9.14.1 (2012-07-07)

* 解決【中古全拼】不可用的問題



<a name="0.9.14"></a>
## 小狼毫 0.9.14 (2012-07-05)

* 介面採用新的 Rime logo，狀態圖示用較柔和的顏色
* 新特性：碼表方案支持與反查碼混合輸入，無需切換或引導鍵
* 新特性：碼表方案可在選單中使用字符集過濾開關
* 新方案：【五筆86】衍生的【五筆·拼音】混合輸入
* 新方案：《廣韻》音系的中古漢語全拼、三拼輸入法
* 新方案：X-SAMPA 國際音標輸入法
* 更新：【吳語】碼表，審定一些字詞的讀音，統一字形
* 更新：【朙月拼音】碼表，修正多音字
* 改進：當前設定的字體缺字時，使用系統後備字體顯示文字
* 解決與MacType同時使用，Ext-B/C/D區文字排版不正確的問題



<a name="0.9.13"></a>
## 小狼毫 0.9.13 (2012-06-10)

* 編碼提示用淡墨來寫，亦可在配色方案中設定顏色
* 新增多鍵並擊組件及輸入方案【宮保拼音】
* 未經轉換的輸入如網址等不再顯示爲候選項
* `default.custom.yaml`: `menu/page_size:` 設定全局頁候選數
* 新增選項：導入【八股文】詞庫時限制詞語的長度、詞頻
* 【倉頡】支持連續輸入多個字的編碼（階段成果，不會記憶詞組）
* 【注音】改爲語句輸入風格，更接近臺灣用戶的習慣
* 較少用的【筆順五碼】、【速記打字法】不再隨鼠鬚管發行
* 修復「用戶詞典管理」導入文本碼表不生效的BUG；<br/>
  部署時檢查並修復已存在於用戶詞典中的無效條目
* 檢測到用戶詞典文件損壞時，重建詞典並從備份中恢復資料
* 修改BUG：簡拼 zhzh 因切分歧義使部分用戶詞失效



<a name="0.9.12"></a>
## 小狼毫 0.9.12 (2012-05-05)

* 用 Shift+Del 刪除已記入用戶詞典的詞條，詳見 Issue 117
* 可選用Shift或Control爲中西文切換鍵，詳見 Issue 133
* 數字後的句號鍵識別爲小數點、冒號鍵識別爲時分秒分隔符
* 解決在QQ等應用程序中的定位問題
* 支持設置爲系統默認輸入法
* 支持多個Windows用戶（新用戶執行一次佈署後方可使用）



<a name="0.9.11"></a>
## 小狼毫 0.9.11 (2012-04-14)

* 使用 `express_editor` 的輸入方案中，數字、符號鍵直接上屏
* 優化「方案選單」快捷鍵操作，連續按鍵選中下一個輸入方案
* 輸入簡拼、模糊音時提示正音，【粵拼】【吳語】中默認開啓
* 拼音反查支持預設的多音節詞、形碼反查可開啓編碼補全
* 修復整句模式運用定長編碼頂字功能導致崩潰的問題
* 修復碼表輸入法候選排序問題
* 修復【朙月拼音】lo、yo 等音節的候選錯誤
* 修復【地球拼音】聲調顯示不正確、部分字的註音缺失問題
* 【五笔86】反查引導鍵改爲 z、反查詞典換用簡化字拼音
* 更新【粵拼】詞典，調整常用粵字的排序、增補粵語常用詞
* 新增輸入方案【小鶴雙拼】、【筆順五碼】



<a name="0.9.10"></a>
## 小狼毫 0.9.10 (2012-03-26)

* 記憶繁簡轉換、全／半角符號開關狀態
* 支持定長編碼頂字上屏
* 新增「用戶詞典管理」介面
* 延遲加載繁簡轉換、編碼反查詞典，降低資源佔用
* 純單字構詞時不調頻
* 新增輸入方案【速成】，速成、倉頡詞句連打
* 新增【智能ABC雙拼】、【速記打字法】



<a name="0.9.9"></a>
## 小狼毫 0.9.9

* 新增「介面風格設定」，快速選擇預設的六款配色方案
* 優化長句中字詞的動態調頻
* 新增【注音】與【地球拼音】輸入方案
* 支持自訂選詞按鍵
* 修復編碼反查失效的BUG
* 修改標點符號「間隔號」及「浪紋」



<a name="0.9.8"></a>
## 小狼毫 0.9.8

* 新增「輸入方案選單」設定介面
* 優化包含簡拼的音節切分
* 修復部分用戶組詞無效的BUG
* 新增預設輸入方案「MSPY雙拼」



<a name="0.9.7"></a>
## 小狼毫 0.9.7

* 逐鍵提示、反查提示碼支持拼寫運算（如顯示倉頡字母等）
* 重構部署工具；以 `*.custom.yaml` 文件持久保存自定義設置
* 製作【粵拼】、【吳語】輸入方案「預發行版」



<a name="0.9.6"></a>
## 小狼毫 0.9.6

* 關機時妥善保存數據，降低用戶詞庫損壞機率；執行定期備份
* 新增基於【朙月拼音】的衍生方案：
  * 【語句流】，整句輸入，空格分詞，回車上屏
  * 【雙拼】，兼容自然碼雙拼方案，演示拼寫運算常用技巧
* 修復BUG：簡拼「z h, c h, s h」的詞候選先於單字簡拼
* 修復BUG：「拼寫運算」無法替換爲空串
* 完善拼寫運算的錯誤日誌；清理調試日誌



<a name="0.9.5"></a>
## 小狼毫 0.9.5

* Rime 獨門絕活之「拼寫運算」
* 升級【朙月拼音】，支持簡拼、糾錯；增設【簡化字】方案
* 升級【倉頡五代】，以倉頡字母顯示編碼
* 重修配色方案【碧水／Aqua】、【青天／Azure】



<a name="0.9.4"></a>
## 小狼毫 0.9.4

* 增設編碼反查功能，預設方案以「`」爲反查的引導鍵
* 修復Windows XP中西文狀態變更時的通知氣球



<a name="0.9.3"></a>
## 小狼毫 0.9.3

* 新增預設輸入方案【五笔86】、【臺灣正體】拼音
* 以托盤圖標表現輸入法狀態變更
* 新增輸入法維護模式，更安全地進行部署作業
* 優化中西文切換、自動識別小數、百分數、網址、郵箱



<a name="0.9.2"></a>
## 小狼毫 0.9.2

* 增設半角標點符號
* 增設Shift鍵切換中／西文模式
* 繁簡轉換、左Shift切換中西文對當前輸入即時生效
* 可自定義OpenCC異體字轉換字典
* 提升碼表查詢效率，更新倉頡七萬字碼表
* 增設托盤圖標，快速訪問配置管理工具
* 改進安裝程序



<a name="0.9"></a>
## 小狼毫 0.9

* 用C++重寫核心算法（階段成果）
* 將輸入法介面從前端遷移到後臺服務進程
* 兼容64位系統



## 小狼毫 0.1 ~ 0.3

* 以Python開發的實驗版本
* 獨創「拼寫運算」技術
* 預裝標調拼音、註音、粵拼、吳語等多種輸入方案



## [0.18.13.0-fluxing] - 2026-07-03

### spec 019: fix TestResponseParser test_4 (close TDD.md sec 8 known gap)

- **Problem**: TestResponseParser test_4 has been failing since the
  file was created. spec 015 L22 diagnosed it as "WeaselIPC
  ContextUpdater is missing `ctx.cand.0/1` array-style
  deserialization" but did not verify the writer side. spec 019
  (2026-07-03) re-investigated and found the test_4 input used a
  fabricated protocol (`ctx.cand.0=...`, `ctx.cand.1=...`,
  `ctx.cand.length=...`, `ctx.cand.cursor=...`, `ctx.cand.page=...`)
  that Weasel never emitted. The actual wire format is a single
  `ctx.cand=<boost::archive::text_woarchive serialized CandidateInfo>`
  line, written by RimeWithWeasel.cpp:881-893 and consumed by
  ContextUpdater.cpp:_StoreCand. The original test_4 input was
  silently ignored because `c.candies` remained empty; the
  `BOOST_ASSERT(2 == c.candies.size())` assertion then failed.

- **Fix (spec 019)**: 1 change to test code (no production code
  change):
  1. **test_4 rewrite**: TestResponseParser.cpp test_4 now
     constructs a known `CandidateInfo` (2 candidates, highlighted=1,
     currentPage=0, totalPages=1), serializes it via
     `boost::archive::text_woarchive` (mirroring
     RimeWithWeasel.cpp:884-885), then deserializes via a fresh
     `boost::archive::text_wiarchive` (mirroring ContextUpdater
     path) and asserts the round-trip preserves the 2 candidates,
     labels, and meta fields. This verifies the wire format itself.
  2. **Why bypass ResponseParser::operator()**: routing through
     `ResponseParser::operator()` -> `wbufferstream` ->
     `text_wiarchive` triggers an access violation (0xC0000005) under
     MSVC Release | NDEBUG | MaxSpeed optimization when the input
     is built by `std::wstring + wstringstream::str()`. The same
     _StoreCand path works correctly in production because
     production's input is the `text_woarchive` of a `RimeContext`
     (not a wstringstream chain). The direct round-trip bypasses
     this test-harness-only optimization interaction. Documented in
     the test source comments and in L26.

- **Test results**:
  - TestResponseParser: 4/4 PASS (was 3/4 before spec 019)
  - TestDefaultHotkeys: 35/35 PASS (unchanged)
  - TestShiftSelectBinding: 13/13 PASS (unchanged)
  - TestBindingResolution: 6/6 PASS (unchanged)
  - TestWeaselIPC: PASS (unchanged)
  - `scripts\run-tests.bat`: === ALL TESTS PASSED ===, exit 0

- **L26 lesson recorded**: "Test assumptions must match the code
  that GENERATES the wire format, not just the code that consumes
  it." Three anti-patterns documented: AP-L26-A (diagnose from
  consumer), AP-L26-B (trust the user's failure description),
  AP-L26-C (skip reading the writer).

- **TDD.md sec 8 known gap closed**: the "TestResponseParser test_4
  fails" line is removed. The full TDD.md sec 8 known gaps list is
  now: L16/L18 test gap (closed by spec 018), user_dict_update
  integration (spec 020 placeholder), WM_SETTINGCHANGE broadcast
  (spec 022 placeholder), rime_deployer --debug (spec 004 �7 SC-005
  no test project). Three of the four are now blocked-on-parent-spec
  placeholders.

- **Spec 020-023 placeholders created**: TDD.md sec 3.1 lists 4
  integration tests (TestUserDictUpdate, TestPhrasesRoundTrip,
  TestDarkModeBroadcast, TestYamlRoundTripE2E). Each is now a
  spec-stage placeholder at `.specify\specs\020-023-*/` with
  spec/plan/tasks 3-piece sets pointing to the parent spec (008 /
  009 / 004 �9 / 007). The production code for those tests does
  not exist yet (CandidateEdit, PhrasesStore, DarkModeBridge,
  YamlRoundTrip), so the test code is blocked on the parent spec
  shipping. This is a tracking release, not an implementation
  release.

### Files changed

- `test/TestResponseParser/TestResponseParser.cpp` (test_4 rewrite only)
- `.specify/memory/lessons-learned.md` (+L26)
- `.specify/specs/019-fix-test-response-parser-test4/{spec,plan,tasks}.md`
  (NEW 3-piece set)
- `.specify/specs/020-023-*/{spec,plan,tasks}.md` (NEW placeholder
  3-piece sets, 12 files total)
- `release/fluxing-0.18.13.0-installer.exe` (NSIS repack; binares
  unchanged from 0.18.12.0)
- `env.bat` (0.18.12 -> 0.18.13, gitignored)
- `weasel.props` (VERSION_PATCH 12 -> 13, gitignored)

### Refs

- spec 015 (L22 system(pause) + !errorlevel!)
- spec 018 (L25 mock pattern, test scaffolding)
- TDD.md sec 8 (known gaps; spec 019 closes the test_4 line)
- L26 (this spec)
- AGENTS.md sec 3.3 (version bump procedure)