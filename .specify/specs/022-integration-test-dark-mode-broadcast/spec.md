# 022 - TestDarkModeBroadcast (integration test for spec 004 sec 9) (placeholder)

> Scope: TDD.md sec 3.1 integration test #X for spec 004.
> This spec is a PLACEHOLDER. The production code that the test would
> exercise does not exist yet (spec 004 is spec-stage only), so the
> test cannot be implemented until that spec ships.
> This placeholder exists so the v0.18.13.0 release records the
> tracking item.

## 0. Status: BLOCKED on spec 004

- spec 004 production code: NOT YET SHIPPED.
- Integration test file: NOT YET CREATED.
- vcxproj + sln entry: NOT YET CREATED.
- run-tests.bat entry: NOT YET CREATED.

## 1. Plan (when unblocked)

When spec 004 ships its production code:
1. Create test/Integration/Testintegration-test-dark-mode-broadcast/integration-test-dark-mode-broadcast.cpp with the mock pattern
   from spec 018 L25 (mock librime objects + behavior assertions).
2. Create test/Integration/Testintegration-test-dark-mode-broadcast/integration-test-dark-mode-broadcast.vcxproj following
   TestBindingResolution.vcxproj template.
3. Add Testintegration-test-dark-mode-broadcast to scripts/run-tests.bat for loop.
4. Add 4-6 real assertions that exercise the production code path.
5. Add L## lesson for any new pattern discovered.

## 2. Acceptance (when unblocked)

- Testintegration-test-dark-mode-broadcast.exe reports N/N PASS, exit 0.
- run-tests.bat reports === ALL TESTS PASSED ===.
- New installer ships with the production code AND the test.

## 3. References

- TDD.md sec 3.1 (4 integration tests)
- spec 004
- spec 015 (test infrastructure)
- spec 018 (L25 mock pattern)
- spec 019 (test_4 fix + L26 test-vs-writer lesson)