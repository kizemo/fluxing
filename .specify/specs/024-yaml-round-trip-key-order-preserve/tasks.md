# 024 - YamlRoundTrip tasks (implementation checklist)

> Tasks T001..T005. Each <4h, 1-3 files. P1 priority per spec 024 sec 3 + AGENTS.md R3.

## T001 - P1 - Implement YamlRoundTrip module
- **Files** (NEW):
  - `FluxingConfigEditor/YamlRoundTrip.h` (~50 lines)
  - `FluxingConfigEditor/YamlRoundTrip.cpp` (~150 lines)
- **API** (per spec 024 plan sec 1.1):
  - `class YamlDocument` (move-only; `root()`, `SetIndentation`,
    `Indentation`)
  - `bool Load(const std::string&, YamlDocument*)`
  - `bool Save(const YamlDocument&, std::string*)`
  - `bool ReadString(const YamlDocument&, const std::string& key, std::string*)`
  - `bool WriteString(YamlDocument*, const std::string& key, const std::string&)`
- **Constraints**:
  - `#include <yaml-cpp/yaml.h>` (the include path
    `librime/deps/yaml-cpp/include` is already in vcxproj default
    via spec 016/018/019 test vcxproj template)
  - Catch `YAML::Exception` in Load; return false on failure
  - No external dependencies beyond yaml-cpp and std
  - UTF-8 / ASCII only (librime yaml files are UTF-8; do not
    introduce Unicode width conversion)
- **Acceptance**: header + source compile standalone (sanity
  check via T002 vcxproj build).

## T002 - P1 - Create test/TestYamlRoundTripE2E/ vcxproj + cpp
- **Files** (NEW):
  - `test/TestYamlRoundTripE2E/TestYamlRoundTripE2E.cpp` (~150 lines, 6 assertions per spec 024 plan sec 1.4)
  - `test/TestYamlRoundTripE2E/TestYamlRoundTripE2E.vcxproj` (modeled on TestBindingResolution.vcxproj)
  - `test/TestYamlRoundTripE2E/TestYamlRoundTripE2E.vcxproj.filters`
  - `test/TestYamlRoundTripE2E/stdafx.h`
  - `test/TestYamlRoundTripE2E/stdafx.cpp`
  - `test/TestYamlRoundTripE2E/targetver.h`
- **vcxproj**:
  - `<ProjectGuid>` is a real new GUID (use
    `7B4E8F2A-1D3C-4E5F-8A9B-6C7D8E9F0123` or any unused GUID;
    L23 anti-pattern — never empty `{}`).
  - `<ClCompile>` includes both `TestYamlRoundTripE2E.cpp` and
    `..\..\FluxingConfigEditor\YamlRoundTrip.cpp` (so the module
    is compiled as part of the test, no separate lib).
  - `AdditionalIncludeDirectories`:
    `$(SolutionDir)\include;$(SolutionDir)\librime\include;$(SolutionDir)\librime\deps\yaml-cpp\include;$(BOOST_ROOT)`
  - `AdditionalLibraryDirectories`:
    `$(SolutionDir)\librime\dist_Win32\lib;$(BOOST_ROOT)\stage\lib`
  - `LanguageStandard`: `stdcpp17`
  - `<RuntimeLibrary>MultiThreaded</RuntimeLibrary>` (Release)
- **Test cpp** has 6 assertions, named `test_1` through `test_6`,
  per spec 024 plan sec 1.4:
  - `test_1` parser sanity: `Load(default.yaml)` succeeds; root is
    a map; `__patch.key_binder` exists
  - `test_2` key order: `Save(Load(default.yaml))`; extract
    `key_binder.bindings[*].accept`; compare to the on-disk order
    (L18 invariant — spec 014 binding form is preserved)
  - `test_3` round-trip equivalence: `Load(Save(Load(x)))` deep-
    equals `Load(x)` for the first 5 bindings (full deep
    comparison is O(N); spot-check 5 is sufficient for an
    integration test)
  - `test_4` write preserves order: `WriteString(&doc,
    "key_binder.bindings.0.send", "99")`; `Save`; re-extract
    `bindings[*].accept`; positions 1..30 unchanged, position 0
    send == "99"
  - `test_5` dotted read: `ReadString(doc,
    "key_binder.bindings.0.accept")` returns "Shift+Shift_L" for
    the spec 014 binding
  - `test_6` comment strip: load `# comment\nfoo: 1`; save; the
    saved text does NOT contain `# comment` (this is the L27
    documented contract; the test makes it explicit)
- **Acceptance**: vcxproj builds, exe runs, 6/6 PASS, exit 0.

## T003 - P1 - Register in weasel.sln
- **File**: `weasel.sln` (modify)
- **Change**: add a new project entry for
  `test\TestYamlRoundTripE2E\TestYamlRoundTripE2E.vcxproj` with
  - the GUID from T002's vcxproj
  - `Release|Win32` and `Debug|Win32` ProjectConfigurationPlatforms
    (mirror TestBindingResolution's sln entry; per L23)
- **Acceptance**: open `weasel.sln` in VS — the new project shows
  up in the Solution Explorer under test/. The test project
  builds both via `msbuild weasel.sln` and via the standalone
  `msbuild test\TestYamlRoundTripE2E\TestYamlRoundTripE2E.vcxproj`
  pattern (per L23 + L24).

## T004 - P1 - Wire into scripts/run-tests.bat
- **File**: `scripts/run-tests.bat` (modify)
- **Change**: add `test\TestYamlRoundTripE2E` to the
  `for %%P in (...)` build loop and `TestYamlRoundTripE2E` to
  the `for %%E in (...)` run loop. Both lists go from 5 entries
  to 6.
- **Acceptance**: `scripts\run-tests.bat` builds and runs 6 test
  exes; `=== ALL TESTS PASSED ===` printed; exit 0.

## T005 - P1 - L27 + build, test, commit, tag, push v0.18.14.0
- **Files**:
  - `.specify/memory/lessons-learned.md` (+ L27)
  - `.specify/specs/023-integration-test-yaml-roundtrip/` (delete — superseded)
  - `CHANGELOG.md` (+ [0.18.14.0-fluxing] section)
  - `release/fluxing-0.18.14.0-installer.exe` (built by xbuild.bat weasel installer)
  - `env.bat` (0.18.13 -> 0.18.14, gitignored)
  - `weasel.props` (VERSION_PATCH 13 -> 14, gitignored)
- **L27 content** (mirror L25 / L26 shape):
  - Incident: spec 024 needed to round-trip a yaml file while
    preserving key order and (ideally) comments. yaml-cpp 0.5+
    `YAML::Node` does NOT store comments — there is no
    `Comment()` accessor. The only way to preserve comments is
    to write a custom yaml tokenizer.
  - Root cause: yaml-cpp's design treats comments as parser
    syntax (they are consumed but not stored in the Node tree).
  - Lesson: when designing a yaml round-trip tool on top of
    yaml-cpp, the comment-stripping behavior is **intrinsic to
    yaml-cpp, not a bug in your code**. Document it explicitly
    in the tool's API contract. If comment preservation is
    required, write a custom tokenizer or use a different
    library (e.g. ruamel.yaml in Python; not applicable to
    C++ in this repo).
  - 2 anti-patterns:
    - AP-L27-A: Add comment-preservation as a TODO in the
      wrapper code and never implement it. Users will silently
      lose their config comments on save.
    - AP-L27-B: Write a custom yaml tokenizer to preserve
      comments without first checking whether the cost is
      justified. For spec 005/014/018 use cases, comments
      don't matter; the cost is not justified.
- **Build step**: `xbuild.bat weasel installer` ->
  `output\archives\fluxing-0.18.14.0-installer.exe` -> copy to
  `release\fluxing-0.18.14.0-installer.exe`.
- **Smoke test**: silent install in C:\TEMP\fluxing-test, verify
  path-force + arch + registry + rime.dll size. Per AGENTS.md
  sec 2.5.
- **Acceptance**: tag v0.18.14.0 visible in `git ls-remote
  kizemo`.

## Done (cross-reference)
- AGENTS.md sec 5 pre-commit checklist (5 steps) all pass
- L27 + spec 024 in git history
- TDD.md sec 3.1 has 1/4 integration tests done (TestYamlRoundTripE2E)
  and 3 placeholder specs (020/021/022) tracking