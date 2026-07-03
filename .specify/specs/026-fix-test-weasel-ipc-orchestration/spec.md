# 026 - TestWeaselIPC integration test orchestration fix (close the silent -2 failure)

> **Scope**: fix `scripts\run-tests.bat` so that TestWeaselIPC.exe
> runs as a real integration test (server-spawn + client + shutdown),
> not as a failing smoke test. The current behavior - TestWeaselIPC
> prints `failed to connect to server.` and returns `-2` - has been
> mis-classified as "pre-existing exit-code issue, not in scope"
> by 4 consecutive sessions (spec 015 / 016 / 017 / 018 / 019 / 024 / 025).
> This spec closes that mis-classification and makes the run-tests gate
> actually green.

## 0. Why now (intent before implementation)

L22 (spec 015) fixed `if errorlevel 1` -> `if !errorlevel! NEQ 0` so
that `BOOST_ASSERT` failures (return 0xC0000005 = -1073741819) are
correctly detected. That fix made `TestWeaselIPC -2` (STATUS_INVALID_HANDLE)
also correctly detectable as failure. Prior to L22, `-2` was
arithmetic-comparable and might have been swallowed by `if errorlevel 1`
(since `-2 < 1`, it would have been mis-read as success - but in
practice it was caught). The current `if !errorlevel! NEQ 0` correctly
flags `-2` as failure, which is why `=== TESTS FAILED ===` prints.

User feedback so far has been "OUTER_RC=0 so it is fine" - that was
a false positive from PowerShell `cmd /c` argument forwarding
(verified in this spec's pre-flight: real exit code IS 1, not 0).

## 1. Acceptance criteria

- `scripts\run-tests.bat` exits with `OUTER_RC=0` from cmd /c.
- `run-tests.bat` prints `=== ALL TESTS PASSED ===` (not FAILED).
- TestWeaselIPC runs as a real integration test: TestWeaselIPC.exe /start
  in the background, then TestWeaselIPC.exe (client mode) connects,
  StartSession returns non-zero session_id, Echo returns true, and
  Final /stop cleans up.
- No WeaselServer.exe (the live product) is required to be running.
- No new failures introduced in the 5 other test projects (35 + 13 + 6 + 4 + 6 = 64 PASS).
- L30 documents the silent -2 / PowerShell OUTER_RC false-positive.
- L31 documents the two root causes (virtual hiding + vcxproj OutDir path glue).
- AGENTS.md sec 5 five-step pre-commit gate passes.

## 1.5 Extended scope (discovered during pre-flight)

The pre-flight trace of TestWeaselIPC revealed TWO additional root causes
beyond the missing batch orchestration:

1. **C++ virtual function hiding** in TestRequestHandler. The 1-arg
   `AddSession(LPWSTR)` HIDES the base class 2-arg `AddSession(LPWSTR, EatLine)`
   instead of overriding it. The server therefore always calls the base
   class default (returns 0), so client session_id stays 0 and Echo fails.
   **Fix**: change derived signature to match base + add `override` keyword.
2. **vcxproj OutDir path glue** in TestWeaselIPC.vcxproj. OutDir was
   `$(SolutionDir)msbuild\$(Configuration)\$(Platform)\` but
   `$(SolutionDir)` has no trailing backslash, so the linker wrote the .exe
   to a malformed path `F:\soft\00selfmade\rimemsbuild\...` (rime +
   msbuild glued). run-tests.bat then ran a STALE binary from a prior
   build, which is why the "broken -2" state persisted invisibly for 4+ specs.
   **Fix**: use `$(SolutionDir)\$(Configuration)\...` (explicit separator).
   Also add the missing `Build.0 = Release|Win32` line for the TestWeaselIPC
   project in weasel.sln (it had only ActiveCfg, never Build.0, so
   `msbuild weasel.sln` never built it).

Both root causes are documented in L31. The vcxproj fix is gated by
`xbuild.bat`-equivalent verification, but in practice run-tests.bat now
produces a real PASS for TestWeaselIPC for the first time since
the project was added to the test suite.

---

## 2. Approach (chosen: path A - batch orchestration)

Two paths were considered:
- **A (chosen)**: in `scripts\run-tests.bat`, before running TestWeaselIPC,
  `start "" "Release\TestWeaselIPC.exe" /start`, sleep 2s, then run
  `Release\TestWeaselIPC.exe` (client mode), then `Release\TestWeaselIPC.exe /stop`.
  No C++ change. Fastest to land, lowest risk.
- **B (deferred)**: add a `/test` flag to TestWeaselIPC.cpp that self-orchestrates
  (fork server thread in-process, run client, join, exit). Cleaner long-term
  but requires multi-threading the Weasel::Server (which currently runs in
  the calling thread - L25 mock pattern does not apply).

## 3. Non-goals

- No changes to TestWeaselIPC.cpp C++ source.
- No changes to run-tests.bat other than the TestWeaselIPC block.
- No new test project.
- No CI workflow change.
- No spec 011 bootstrapper work (separate spec, blocked on its own parent).

## 4. References

- L22 (if errorlevel 1 vs NEQ 0; TestWeaselIPC -2 root cause).
- L25 (mock pattern - NOT applied here, TestWeaselIPC exercises real Weasel::Server).
- spec 015 (L22 surface; test infra).
- spec 016 (L23 vcxproj pattern - not used here, no new project).
- `test\TestWeaselIPC\TestWeaselIPC.cpp` (read-only reference for /start / /stop / client_main).
