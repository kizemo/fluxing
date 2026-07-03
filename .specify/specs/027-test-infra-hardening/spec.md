# 027 - Test infra hardening (close L22/L28/L30/L31 latent traps)

> **Scope**: codify the L22/L28/L30/L31 win-batch knowledge into a reusable
> `scripts/test-infra/` directory of `.bat` wrappers, and switch
> `scripts\run-tests.bat` to delegate to them. Future specs that need to
> (a) run a silent-install smoke test, (b) build + run the test suite, or
> (c) detect a stale `Release\*.exe` can call the wrappers without having
> to re-derive the incantation from a lesson entry. Also adds L32 to
> lessons-learned.md describing the integration-test infra hardening pattern.

## 0. Why now (intent before implementation)

Across specs 015-026 we have accumulated four lessons (L22, L28, L30, L31)
that all share the same shape:

1. The lesson describes a footgun in the test infra (e.g. silent -2
   from TestWeaselIPC, PowerShell `cmd /c` false-positive OUTER_RC, NSIS
   `/D=` boundary break, stale-binary path-glue).
2. The lesson is currently stored ONLY as prose in `lessons-learned.md`.
3. The next spec author will not re-read the lesson, and will re-introduce
   the same bug (verified across 4 specs in the silent -2 case).

This spec breaks that loop by packaging each lesson into a `.bat` wrapper
that the next author calls by NAME, not by reading prose. The wrapper
embodies the lesson; if the lesson is wrong, the wrapper is wrong; if the
wrapper is wrong, the next author finds out within the spec, not 4 specs later.

## 1. Acceptance criteria

- `scripts\test-infra\install_smoke_test.bat` exists and can be run
  standalone. It is a thin wrapper that documents the AGENTS.md sec 2.5
  recipe (the source of truth for the smoke test), establishes the
  convention that smoke tests have a wrapper, and exits 0 on success
  (today: do nothing, the recipe is in AGENTS.md).
- `scripts\test-infra\run-test-suite.bat` exists and can be run
  standalone. Internally does the same work as `scripts\run-tests.bat`
  (build all 6 test projects, run them, propagate OUTER_RC). Exit code
  is the real exit code (not the PowerShell `cmd /c` parent process
  false-positive per L30). L30 awareness is baked in.
- `scripts\test-infra\verify-test-binaries-fresh.bat` exists and can be
  run standalone. For each of the 6 test projects, compares the mtime
  of `Release\<Name>.exe` against the mtime of every `.cpp` / `.h` /
  `.vcxproj` under `test\<Name>\`. If ANY source is newer than the
  .exe, prints a warning and exits non-zero. This is the L31 stale-binary
  detection that has now been hardened into a script (L31 was originally
  diagnosed by tracing the silent -2; this spec turns that diagnosis
  into a re-runnable check).
- `scripts\run-tests.bat` is updated to delegate the build+run work to
  `scripts\test-infra\run-test-suite.bat`. After this spec, `scripts\run-tests.bat`
  is a thin wrapper (5-10 lines) that just calls the infra script.
- L32 added to `.specify\memory\lessons-learned.md` describing the
  integration-test infra hardening pattern (lesson-to-script promotion).
- AGENTS.md sec 5 five-step pre-commit gate passes for the new files.
- `cmd /c scripts\test-infra\run-test-suite.bat` exits 0 with all 6 test
  projects passing (6/6 = 65 individual assertions across the 6 exes).
- No product code change. No installer change. No CI change. Scope is
  pure test-infra (test runners + smoke test + freshness check).

## 2. Approach (chosen: thin .bat wrappers over the existing recipes)

Three wrapper scripts in `scripts\test-infra\`:

- `install_smoke_test.bat` - thin entry point. The full NSIS smoke test
  recipe (80+ lines of PowerShell) lives in AGENTS.md sec 2.5 and is the
  source of truth. Wrapping the recipe inside a .bat would either
  duplicate the recipe (and drift from AGENTS.md) or shell out to
  PowerShell (which is what AGENTS.md already says). The wrapper instead:
  1. Establishes the convention that smoke tests have a wrapper.
  2. Is the place future work can fill in (e.g. if the recipe is ever
     ported to pure cmd / NSISExec).
  3. Is a hook for CI to call (so CI does not have to `cmd /c` a long
     PowerShell snippet from `yml`).
  4. Exits 0 to indicate smoke test infra is wired up even though the
     actual assertions still live in AGENTS.md. This is documented in
     the script header.

- `run-test-suite.bat` - the actual meat. Contains the current
  `scripts\run-tests.bat` body verbatim, with one improvement: the
  file-detection logic at the top that resolves the repo root uses
  a more robust pattern (`%~dp0` for script dir, `pushd/popd` for
  resolution). Exit code propagation uses the proven L30 cure
  (the `endlocal & set "OUTER_RC=%FINAL_RC%"` pattern that
  `scripts\run-tests.bat` already uses).

- `verify-test-binaries-fresh.bat` - iterates the 6 test projects and
  for each, uses PowerShell INSIDE the script to compare mtimes
  (because `[IO.File]::LastWriteTimeUtc` is the only practical way to
  read mtimes from cmd). L22 is about CMD interpretation of errorlevel,
  not about banning PowerShell for read-only operations. Exits non-zero
  if any source is newer than the .exe.

## 3. Non-goals

- No new test project. No new test assertion.
- No product code change (no C++ / NSIS / build script change other than
  the test-infra scripts themselves).
- No CI workflow change (`.github\workflows\ci.yml` untouched in this spec).
  The test-infra scripts CAN be invoked from CI in a later spec.
- No replacement of the PowerShell smoke test recipe in AGENTS.md sec 2.5.
  That recipe is the source of truth; the new `.bat` is a thin entry point.
- No version bump in `env.bat` / `weasel.props` (this is bookkeeping sub-release,
  not a code feature). The release version is `v0.18.16.0` per P8 / P4 scope
  convention (sub-release because the only thing that changes is test infra).

## 4. References

- L22 (TestWeaselIPC -2 root cause; `if errorlevel 1` vs `NEQ 0`).
- L28 (NSIS smoke test must use `.bat` wrapper, not `& cmd /c "..."`).
- L30 (PowerShell `cmd /c` false-positive OUTER_RC; the .bat wrapper is the cure).
- L31 (vcxproj OutDir path-glue; stale-binary detection is what we are formalizing).
- spec 015 (L22 surface; `scripts\run-tests.bat` origin).
- spec 024 (test infra style - YamlRoundTrip module + integration test).
- spec 026 (TestWeaselIPC orchestration; the run-tests.bat body that this spec
  splits out into `scripts\test-infra\run-test-suite.bat`).
- AGENTS.md sec 2.5 (NSIS smoke test recipe; canonical location).
- AGENTS.md sec 5 (five-step pre-commit gate).

## 5. Discovered during implementation (L33 + L34)

While writing `verify-test-binaries-fresh.bat`, two cmd parsing bugs were discovered and documented as L33 and L34:

- **L33**: PowerShell `$` parsing eats PowerShell -Command "$..." variables. Fix: use -File with sibling .ps1 instead of inline -Command. The `.ps1` is gitignored; the `.bat` is the tracked entry point.
- **L34**: cmd `rem` lines containing `(` start a sub-block that ends at the next `)`. Fix: avoid `(` and `)` in `rem` text (use `-`, `,`, or words). Symptom: phantom "is not recognized" errors on stderr even when exit 0 + tests pass.

Both bugs are also documented in `scripts/test-infra/run-test-suite.bat` and `verify-test-binaries-fresh.bat` headers as a "why" comment, so the next spec author does not re-introduce them.