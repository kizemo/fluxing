# 002 · Tasks — Fluxing Install Paths and Registry

> Each task: `- [ ] TNNN [P?] [Story] Description`.
> `[P]` = parallelizable. `[Story]` = US1 / US2 / US3 tag from `spec.md`.
> MVP = Phase 1 + Phase 2 (US1 + US2 + US3) — all of T001–T008.

## Phase 1 — Documentation (no code change)

- [ ] T001 [P] [US1/US2/US3] Author spec/plan/tasks three-file triplet
  - Create `.specify/specs/002-install-paths-and-registry/{spec.md,plan.md,tasks.md}`.
  - spec.md has 3 user stories (US1 first-install, US2 upgrade, US3 registry).
  - plan.md has the Constitution Check table and the Substitution Map.
  - tasks.md is this file.
  - **SC-001..SC-004 / FR-001..FR-008 acceptance** by construction.

## Phase 2 — Source edits

- [ ] T002 [P] [US1/US3] Edit `output/install.nsi`
  - L22: `!define WEASEL_ROOT $INSTDIR\weasel-${FLUXING_VERSION}` -> `!define WEASEL_ROOT $INSTDIR\weasel` (no version stamp)
  - Add L23: `!define FLUXING_ROOT $INSTDIR\fluxing`
  - L25: keep `Name "Fluxing ${FLUXING_VERSION}"` (per decision 2B)
  - L52: add `!define MUI_PAGE_CUSTOMFUNCTION_LEAVE "ForceFluxingSuffix"`
  - Append `Function ForceFluxingSuffix` and `Function IsFluxingPath` at end of file (suffix enforcement, idempotent)
  - L127: change to read `Software\Fluxing\Weasel` first, fall back to `Software\Rime\Weasel` on empty
  - L202: `InstallDirRegKey HKLM "Software\Fluxing\Weasel" "InstallDir"`
  - L214: `WriteRegStr HKLM SOFTWARE\Fluxing\Weasel "InstallDir" "$INSTDIR"`
  - **FR-001 / FR-003 / FR-004 / FR-006 acceptance**.

- [ ] T003 [P] [US3] Edit `RimeWithWeasel/WeaselUtility.cpp`
  - L8: `const WCHAR KEY[] = L"Software\\Rime\\Weasel";` -> `const WCHAR KEY[] = L"Software\\Fluxing\\Weasel";`
  - L27: `ExpandEnvironmentStringsW(L"%AppData%\\Rime", _path, _countof(_path));` -> `ExpandEnvironmentStringsW(L"%AppData%\\fluxing", _path, _countof(_path));`
  - **FR-006 / FR-007 acceptance**.

- [ ] T004 [P] [US3] Edit `WeaselSetup/WeaselSetup.cpp` (multiple sites)
  - L62: `const WCHAR KEY[] = L"Software\\Rime\\Weasel";` -> `const WCHAR KEY[] = L"Software\\Fluxing\\Weasel";`
  - L166: `ExpandEnvironmentStringsW(L"%APPDATA%\\Rime", _path, _countof(_path));` -> `ExpandEnvironmentStringsW(L"%APPDATA%\\fluxing", _path, _countof(_path));`
  - L189, L194, L197, L200, L214, L218, L222, L226: each `L"Software\\Rime\\weasel"` (lowercase w) -> `L"Software\\Fluxing\\Weasel"`. Total 8 sites.
  - L205, L209: `L"Software\\Rime\\weasel\\Updates"` is INTENTIONALLY left as-is per spec 001 Appendix A.3 deferral (auto-update config; not in this slice).
  - **FR-006 / FR-007 acceptance**.

- [ ] T005 [P] [US3] Edit `WeaselTSF/LanguageBar.cpp`
  - L315 (approx): `Software\\Rime\\Weasel` -> `Software\\Fluxing\\Weasel`
  - **FR-006 acceptance**.

- [ ] T006 [P] [US1] No edit required (this slice)
  - **Finding during execution**: `WeaselSetup/InstallOptionsDlg.rc` does NOT exist in the repo (the dialog is constructed in C++ via `WeaselSetup/InstallOptionsDlg.cpp/.h`; the `user_dir` field is initialised as an empty `std::wstring` and the `%APPDATA%\\Rime` fallback lives in `WeaselSetup.cpp` L163-168, which T004 already covers).
  - **No additional edits** are required in this slice.
  - **FR-002 / FR-007 acceptance** is delivered by T003 fallback and T004 master-key migration.

## Phase 3 — Build environment (host-side)

- [ ] T007 [US1/US2] Build C++ binaries via `xbuild.bat`
  - Pre-conditions (run in order):
    - Initialise VS 2022 env: `env.vs2022.bat` (already present)
    - Set `HTTP_PROXY=http://127.0.0.1:7897` and `HTTPS_PROXY=http://127.0.0.1:7897` for any tool that hits the network
    - `git submodule update --init --recursive` (librime)
    - `install_boost.bat` (downloads + builds Boost 1.78.0)
    - `cmake -S librime -B librime/build -G "Visual Studio 17 2022" -A x64`
    - `cmake --build librime/build --config Release`
  - Then run `xbuild.bat` from the project root; expect all `output\Weasel*.exe` and `weasel*.dll` to be re-stamped.
  - **Verification**: `git status output/WeaselServer.exe` shows `M` (modification) — binary was rebuilt.
  - **SC-001 / SC-002 acceptance** (binary carries new key strings).

## Phase 4 — Package installer

- [ ] T008 [US1/US2] Build NSIS installer via `makensis`
  - `cd output`
  - `& "C:\Program Files (x86)\NSIS\makensis.exe" install.nsi`
  - **Expected output**: `output\fluxing-0.1.0-installer.exe`
  - **Copy to `release/`**: `Copy-Item output\fluxing-*-installer.exe release\`
  - **Verification**: `Test-Path release\fluxing-0.1.0-installer.exe` is `True`
  - **SC-001 / SC-002 / SC-003 / SC-004 acceptance** by construction
    (installer carries the suffix-enforcement callback and the new registry writes).

## Final Phase — Commit & push

- [ ] T009 [US1/US2/US3] Stage, commit, and push to `kizemo/Fluxing`
  - `git add output/install.nsi RimeWithWeasel/WeaselUtility.cpp WeaselSetup/WeaselSetup.cpp WeaselTSF/LanguageBar.cpp .specify/specs/002-install-paths-and-registry/`
  - **Do NOT add** `release/`, `output/Weasel*.exe`, `output/weasel*.dll` (those go to `.gitignore` or stay untracked)
  - `git add .gitignore` if release/ rule was added in T008
  - `git commit -m "feat(fluxing): install paths & registry (spec 002)" -m "..."`
  - `git push kizemo Fluxing`
  - **FR-001..FR-008 / R6 / R8 acceptance**.

- [ ] T010 [US1/US2] Append cross-document audit findings to `tasks.md`
  - If any spec-internal contradictions surface during T002–T008, record them in an Appendix similar to spec 001's Appendix A.
  - **R7 acceptance**.