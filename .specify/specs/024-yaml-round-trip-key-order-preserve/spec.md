# 024 - YamlRoundTrip key-order-preserving module + TestYamlRoundTripE2E

> Scope: ship a small C++ module `FluxingConfigEditor::YamlRoundTrip`
> that wraps yaml-cpp to read and write a YAML document while preserving
> the **key order** of the source file. Plus a behavior-level test
> `TestYamlRoundTripE2E` that reads `output/data/default.yaml` and
> asserts the round-trip preserves the `key_binder` binding order
> (L18 / L19 invariant closure).
>
> This is the **subset of spec 007** (yaml-config-ui) that the
> TDD.md sec 3.1 / spec 023 placeholder was waiting on. The full
> spec 007 mac-style settings UI is **out of scope** (multiple
> sprints of GUI work; deferred per the user's "选项 1" decision
> on spec 019). The YamlRoundTrip module alone is sufficient to
> unblock the test, and the module is independently useful for
> any future settings editor.

## 0. Context

- TDD.md sec 3.1: `TestYamlRoundTripE2E` (integration test #4) is
  blocked on spec 007. spec 007's full scope is a mac-style settings
  UI (spec 007 §1.2 US3-A through US3-D); that is 6-10 weeks of GUI
  work.
- spec 007 §2 explicitly carves out: "不实现 YamlRoundTrip 之外的
  编辑器（保留注释 + key 顺序由 YamlRoundTrip 单独 module 负责）"
  — i.e. YamlRoundTrip is intended as a standalone module that the
  future settings UI can use.
- librime 1.13.1 exposes `rime_api->config_load_string` (yaml-cpp
  deserialize) but **no** `config_save_string` / `config_to_string`.
  And the vendored yaml-cpp 0.5+ **does not store comments in
  YAML::Node** (no comment field). So a full "preserve comments
  and key order" round-trip requires either (a) a custom yaml
  tokenizer or (b) reducing scope to "key order only".
- This spec reduces scope to **(b) key order only**. Comments are
  stripped on round-trip; this is acceptable for spec 005/014/018
  use cases where the spec-edited yaml values are what matter, not
  the inline human comments. The YamlRoundTrip module's API
  contract makes this explicit so future code does not depend on
  comment preservation.

## 1. Goals

1. Ship `FluxingConfigEditor/YamlRoundTrip.{h,cpp}` — a small C++
   wrapper around yaml-cpp that exposes:
   - `bool Load(const std::string& yaml_text, YamlDocument* out)`
   - `bool Save(const YamlDocument& doc, std::string* out_yaml)`
   - `bool ReadString(const YamlDocument&, const std::string& key,
                      std::string* out)`
   - `bool WriteString(YamlDocument*, const std::string& key,
                       const std::string& value)`
   - `void SetIndentation(YamlDocument*, int spaces)` (default 2)
2. Ship `test/TestYamlRoundTripE2E/TestYamlRoundTripE2E.cpp` with 6
   real assertions (mirror spec 018 L25 mock pattern).
3. Wire the test into `scripts/run-tests.bat` for loop.
4. The test reads `output/data/default.yaml` and asserts the
   `key_binder.bindings` order survives a Load -> Save round-trip
   (L18 / L19 closure: the spec 014 binding form contract is
   parseable by an external consumer without losing information).

## 2. Acceptance (GWT)

- Given a clean checkout of Fluxing at v0.18.14.0+
- When the developer runs `scripts/run-tests.bat`
- Then within 60s, all 6 test exes are built and run
- And `TestYamlRoundTripE2E` prints `6 / 6 assertions passed`
- And the existing 5 tests (TestDefaultHotkeys 35/35,
  TestShiftSelectBinding 13/13, TestBindingResolution 6/6,
  TestResponseParser 4/4, TestWeaselIPC PASS) all still pass
- And `scripts/run-tests.bat` exits 0 (`=== ALL TESTS PASSED ===`)

- Same command in GitHub Actions windows-2022 image: same 6 test
  exes, same overall exit 0

- Given the installer's silent smoke test
- When the new installer is run in `C:\TEMP\fluxing-test`
- Then the layout invariants from AGENTS.md sec 2.5 still hold
  (path-force, arch, registry, rime.dll size)
- And `TestYamlRoundTripE2E.exe` is built and runs in CI
- And exit 0

## 3. Non-goals

- **No comment preservation.** yaml-cpp 0.5+ does not store
  comments in `YAML::Node`. Re-implementing a yaml tokenizer that
  preserves comments is a multi-day research project; out of scope
  for v0.18.14. The YamlRoundTrip module's docstring says
  "round-trips key order; comments are stripped".
- **No full spec 007 mac-style settings UI.** That is 6-10 weeks
  of GUI work. Deferred to a future spec (025+).
- **No new CI matrix.** Use the existing `windows-2022` build
  matrix in `ci.yml`. The new test joins the existing `test`
  job.
- **No yaml-cpp version bump.** We use the vendored
  `librime/deps/yaml-cpp/include/` and link against
  `librime/deps/yaml-cpp/build_Win32/Release/yaml-cpp.lib`.
  These are already in the repo; no new dependencies.

## 4. Out of scope / not affected

- The 3 remaining TDD sec 3.1 placeholder specs (020, 021, 022)
  remain BLOCKED on their parent specs (008, 009, 004 sec 9).
- L22 (system(pause) anti-pattern) and L25 (mock pattern) remain
  as recorded lessons; spec 024 reuses the L25 pattern but
  applies it to yaml instead of librime KeyEvent.
- L26 (wire-format diagnosis) does not apply; we are building
  fresh code, not diagnosing an existing test.
- AGENTS.md sec 3.3 version bump procedure is followed (env.bat +
  weasel.props + CHANGELOG.md, all bumped 0.18.13 -> 0.18.14).

## 5. Done

- `FluxingConfigEditor/YamlRoundTrip.h` + `YamlRoundTrip.cpp`
  ship with the API in spec 024 sec 1.
- `test/TestYamlRoundTripE2E/{TestYamlRoundTripE2E.cpp, .vcxproj,
  .vcxproj.filters, stdafx.h, stdafx.cpp, targetver.h}` ship.
- `weasel.sln` registers the new test project (per L23 GUID +
  ProjectConfigurationPlatforms pattern).
- `scripts/run-tests.bat` adds `TestYamlRoundTripE2E` to the
  build and run loops.
- `test/TestYamlRoundTripE2E/Release/TestYamlRoundTripE2E.exe`
  reports 6/6 PASS.
- `release/fluxing-0.18.14.0-installer.exe` ships.
- v0.18.14.0 tag pushed to `kizemo/Fluxing`.
- L27 lesson recorded: "yaml-cpp 0.5+ does NOT store comments in
  YAML::Node; round-trip tools built on it must explicitly say
  'comments are stripped' or implement a custom tokenizer".