# 028 · Plan · 候选字编辑 stage 1 (核心 API 集成 + test)

## Technical context

- **Files touched (new)**: 2
  - `test\TestUserDictUpdate\TestUserDictUpdate.cpp` (new, ~80 lines, behavior-level test)
  - `test\TestUserDictUpdate\TestUserDictUpdate.vcxproj` (new, ~50 lines, L31 fix applied)
  - `.specify\specs\028-candidate-delete-core\{spec,plan,tasks}.md` (3 files)
- **Files touched (existing)**: 3
  - `include\RimeWithWeasel.h` (add 1 method declaration, ~3 lines)
  - `RimeWithWeasel\RimeWithWeasel.cpp` (add 1 method impl, ~10 lines)
  - `weasel.sln` (add new test project to solution, ~10 lines)
  - `.specify\memory\lessons-learned.md` (append L35, ~80 lines)
- **No installer change, no env.bat / weasel.props bump, no spec 008 UI changes.**

## Step-by-step

### T001 - Verify rime_api::delete_candidate_on_current_page symbol is linkable

Pre-flight: confirm that `rime.lib` exports the symbol
`delete_candidate_on_current_page` (it should, per librime 1.13). If
not, this spec's production code change cannot link.

Action: dump `librime\dist_Win32\rime.lib` exports (or use
`dumpbin /EXPORTS`) and grep for `delete_candidate`.

### T002 - Add `DeleteCandidateOnCurrentPage` declaration to `include\RimeWithWeasel.h`

Find `SelectCandidateOnCurrentPage` declaration (line ~1766) and add:

```cpp
void DeleteCandidateOnCurrentPage(size_t index, WeaselSessionId ipc_id);
```

immediately after.

### T003 - Add `DeleteCandidateOnCurrentPage` impl to `RimeWithWeasel\RimeWithWeasel.cpp`

Find `SelectCandidateOnCurrentPage` impl (line ~10748) and add:

```cpp
void RimeWithWeaselHandler::DeleteCandidateOnCurrentPage(
    size_t index, WeaselSessionId ipc_id) {
  DLOG(INFO) << "delete candidate on current page, ipc_id = " << ipc_id
             << ", index = " << index;
  if (m_disabled)
    return;
  rime_api->delete_candidate_on_current_page(to_session_id(ipc_id), index);
  _UpdateUI(ipc_id);
}
```

immediately after.

### T004 - Write `test\TestUserDictUpdate\TestUserDictUpdate.cpp`

Behavior-level test with 4 assertions (per spec 028 spec.md §2.2).
Does NOT link RimeWithWeasel.lib. Uses test-local RimeApi stub.

### T005 - Write `test\TestUserDictUpdate\TestUserDictUpdate.vcxproj`

Standard test project pattern, L31 OutDir fix applied
(`$(SolutionDir)\$(Configuration)\...` with explicit backslash).
Builds Release|Win32 only (matches the other 6 test projects).

### T006 - Add new test project to `weasel.sln`

Add `{GUID}.Release|Win32.ActiveCfg = Release|Win32` and
`{GUID}.Release|Win32.Build.0 = Release|Win32` (L31 AP-L31-D).
GUID: generate new, or use the same pattern as the other test projects
(check existing entries in `weasel.sln` for a TestResponseParser or
similar to copy the GUID format).

### T007 - Add `test\TestUserDictUpdate` to `scripts\test-infra\run-test-suite.bat`

Update the build loop (line ~57) and the run loop (line ~73) to
include `TestUserDictUpdate` in the respective enumerations.

### T008 - Write L35 lesson

Append L35 to `.specify\memory\lessons-learned.md` documenting:
- rime_api.h does NOT expose `is_user_dict` in `rime_candidate_t` (only
  `text`, `comment`, `reserved`).
- The spec 008 plan.md assumption "is_user_dict (1.13+ adding? needs
  verification)" was checked; the field is NOT there.
- Implication: stage 1 cannot do "is_user_dict==true -> delete" at the
  C API layer. The engine does the check inside `Context::DeleteCandidate`.
- The correct stage 1 design is: just call
  `delete_candidate_on_current_page(index)`. The engine removes
  user.db entry if it was a user-dict entry; otherwise the call is a
  no-op (or moves the cursor; the exact behavior is engine-internal).
- AP-L35-A: Planning around a C API field before verifying the C API
  actually exposes it. Always grep the .h file first.
- AP-L35-B: Assuming that because librime C++ has a class member, the
  C API exposes it. The C API surface is a deliberate subset, not
  a mirror.

### T009 - Verification

Run `cmd /c scripts\test-infra\run-test-suite.bat` (via Start-Process
to avoid L33/L34 path issues):
- Expect: RC 0, `=== ALL TESTS PASSED ===`, 7/7 test projects.
- TestUserDictUpdate output: 4 PASS, 0 FAIL.

Run `cmd /c scripts\test-infra\verify-test-binaries-fresh.bat`:
- Expect: RC 0, all 7 FRESH (the new TestUserDictUpdate.exe included).

### T010 - Pre-commit gate

AGENTS.md sec 5 five steps:
- byte health on new files (CRLF, no BOM)
- run-test-suite.bat OUTER_RC=0, 7/7 PASS
- build hygiene (no new .log/.token)
- format (no clang-format locally)
- scope check (single `test(fluxing):` or `feat(fluxing):` scope
  per P4 - this is a new feature, so `feat(fluxing):`)

### T011 - Commit, tag, push

Commit on `Fluxing` with `feat(fluxing):` scope. Tag v0.18.17.0
(lightweight, sub-release). Push to kizemo/Fluxing and
kizemo/v0.18.17.0.

Update CHANGELOG.md with the new sub-release section before
committing.

## Constitution check

| Rule | Status | Note |
|---|---|---|
| R1 intent+acceptance | OK | spec.md sections 0+1 |
| R2 spec separate from plan | OK | spec.md is text-only |
| R3 single priority | OK | All P1 (unblock TDD 3.1 + spec 008 stage 1) |
| R4 constitution gate | OK | This table |
| R5 task size < 4h, 1-3 files | OK | 2 new + 3 modified; ~2-3 hours total |
| R6 done = evidence | OK | T009 verification output is the evidence |
| R7 one source of truth | OK | L35 in lessons-learned.md only |
| R8 specs versioned in git | OK | spec/plan/tasks committed |
| R9 lookup beats memory | OK | This spec verified rime_api.h symbols before coding (L35) |