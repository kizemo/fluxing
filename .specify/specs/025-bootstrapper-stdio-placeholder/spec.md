# 025 - TestBootstrapperStdio (integration test for spec 011) (placeholder)

> Scope: TDD.md sec 3.1 integration test #5 for spec 011.
> This spec is a PLACEHOLDER. The production code that the test would
> exercise (NSIS silent pipe JSON protocol round-trip) does not exist
> yet (spec 011 Fluxing Bootstrapper is spec-stage only), so the test
> cannot be implemented until that spec ships.
> This placeholder exists so this release records the tracking item.

## 0. Status: BLOCKED on spec 011

- spec 011 production code: NOT YET SHIPPED.
- Integration test file: NOT YET CREATED.
- vcxproj + sln entry: NOT YET CREATED.
- run-tests.bat entry: NOT YET CREATED.

## 1. Plan (when unblocked)

When spec 011 ships its production code (the NSIS silent-mode
bootstrapper that consumes a JSON config from a named pipe and
applies the install):

1. Create `test/TestBootstrapperStdio/TestBootstrapperStdio.cpp`
   with a child-process-driven test harness: fork the bootstrapper
   exe, connect to its named pipe, send a JSON config blob, assert
   the install layout invariant from spec 002 (path-force,
   fluxing-suffix) matches the JSON-config-driven path.
2. Create `test/TestBootstrapperStdio/TestBootstrapperStdio.vcxproj`
   following the TestBindingResolution.vcxproj template.
3. Add `TestBootstrapperStdio` to `scripts/run-tests.bat` for loop.
4. Add 4-6 real assertions covering: pipe connect, JSON parse,
   install path = config["install_dir"] + "fluxing\\weasel\\",
   registry write, HKCU user-dir fallback.
5. Add L## lesson for any new pattern discovered (named-pipe
   stdin/stdout interop is rare in this repo; first use deserves a
   post-mortem).

## 2. Acceptance (when unblocked)

- `TestBootstrapperStdio.exe` reports N/N PASS, exit 0.
- `scripts/run-tests.bat` reports `=== ALL TESTS PASSED ===`.
- New installer ships with the production code AND the test.

## 3. References

- TDD.md sec 3.1 (5 integration tests)
- spec 011 (Fluxing Bootstrapper - parent spec)
- spec 015 (test infrastructure - run-tests.bat pattern)
- spec 016 (L23 vcxproj + sln registration pattern)
- spec 018 (L25 mock pattern for behavior-level tests)
- spec 019 (L26 test-vs-writer lesson)
- spec 024 (YamlRoundTrip pattern for the sibling test #4)

