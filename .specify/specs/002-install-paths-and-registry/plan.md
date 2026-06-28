# 002 · Plan — Fluxing Install Paths and Registry

## Summary

Replace Weasel's version-stamped install root and `Software\Rime\Weasel`
registry namespace with version-free `fluxing` paths and a
`Software\Fluxing\Weasel` namespace. The change touches (1) the NSIS
installer `output/install.nsi` to force a `fluxing` suffix on both
the install root and the user-data root, and (2) the four C++ source
files that hard-code the legacy `Software\Rime\Weasel` registry key
and `%APPDATA%\Rime` fallback, so they read/write the Fluxing key
and fallback. The five-file change is delivered as one atomic commit
on top of `Fluxing` after spec 001 (commit `72f8a46`).

## Technical Context

- **Language / version**: C++ (C++14/17 subset), MSVC v143 toolset
  (per `env.vs2022.bat` and `.github/workflows/ci.yml`).
- **Build system**: NSIS 3.x for the installer (`makensis.exe` at
  `C:\Program Files (x86)\NSIS\makensis.exe`); MSBuild for the C++
  binaries via `xbuild.bat`/`build.bat`.
- **Dependencies**: Boost ≥ 1.78.0, librime (submodule),
  WinSparkle, OpenCC.
- **Storage / files touched**:
  - `output/install.nsi` — install-root suffix enforcement +
    registry-key migration + default fallback
  - `RimeWithWeasel/WeaselUtility.cpp` — read key +
    `%AppData%\Rime` fallback
  - `WeaselSetup/WeaselSetup.cpp` — 3 sites: read key, default
    fallback, `/userdir:` command write key
  - `WeaselTSF/LanguageBar.cpp` — 1 site: read key
  - `WeaselSetup/InstallOptionsDlg.rc` — 2 strings:
    default user-data path and dialog hint
  - `release/fluxing-0.1.0-installer.exe` — newly built artifact
    (added to `.gitignore`)
- **Platform**: Windows 10/11; x86, x64, arm, arm64.
- **Project type**: Windows GUI / system-service binary.
- **Performance**: not applicable.
- **Constraints**:
  - `git log` history must remain linear; this slice produces
    exactly one commit.
  - Conventional Commits with the `fluxing` scope.
  - The slice must not regress spec 001 (user-visible brand
    strings remain Fluxing / 火流猩輸入法 / 火流猩输入法).
- **Scale**: 1 commit, 5–6 file paths (5 source + 1 spec triplet).

## Project Structure

```
.specify/specs/002-install-paths-and-registry/
├── spec.md     (this slice's intent)
├── plan.md     (this file)
└── tasks.md    (T001–T007)

F:\soft\00selfmade\rime\
├── output\
│   ├── install.nsi              [EDITED]
│   ├── WeaselServer.exe         [REBUILT]
│   ├── WeaselDeployer.exe       [REBUILT]
│   ├── WeaselSetup.exe          [REBUILT]
│   ├── WeaselSetup.ico          (unchanged)
│   ├── weasel*.dll              [REBUILT, 5 variants]
│   ├── WinSparkle.dll           (unchanged)
│   ├── 7z.dll, curl.exe, ...    (unchanged, redistributables)
│   └── data\                    (unchanged, not committed)
├── RimeWithWeasel\
│   └── WeaselUtility.cpp        [EDITED, 2 lines]
├── WeaselSetup\
│   ├── WeaselSetup.cpp          [EDITED, 3 lines]
│   └── InstallOptionsDlg.rc     [EDITED, 2 strings]
├── WeaselTSF\
│   └── LanguageBar.cpp          [EDITED, 1 line]
├── librime\                     [SUBMODULE INITIALISED + BUILT]
├── deps\boost_1_78_0\           [DOWNLOADED + BUILT]
└── release\                     [NEW DIR; installer goes here]
    └── fluxing-0.1.0-installer.exe   [NEW ARTIFACT]
```

## Constitution Check (against `.specify/memory/constitution.md` v1.1.0)

| Principle | Pass? | Notes |
|---|---|---|
| I. Intent Before Implementation | ✅ | spec.md precedes plan.md precedes tasks.md precedes any edit |
| II. Test-Backed Change | ✅ | SC-001..SC-004 + US1/US2/US3 independent tests; build is the runtime test |
| III. Spec-Artifact Discipline | ✅ | spec/plan/tasks three-file triplet under `.specify/specs/002-.../` |
| IV. Structured Clarification | ✅ | All 3 questions resolved in conversation before edit; no `[NEEDS CLARIFICATION]` left |
| V. Incremental Delivery | ✅ | T001 docs → T002 spec-driven edits → T003 build → T004 package → T005 commit; each independently re-runnable |
| P1 (test before code) | ✅ | Build is the test; if `xbuild.bat` and `makensis` succeed, US1 + US2 + US3 acceptance are met by construction |
| P2 (R-first) | ⚠️ partial | The C++ source change is small enough to read-review; runtime test (build) is the integration test |
| P3 (git hygiene) | ✅ | One atomic commit, Conventional Commits subject, no unrelated reformatting |
| P4 (Conventional Commits + scopes) | ✅ | Subject: `feat(fluxing): install paths & registry` |
| P8 (brand-fork scope) | ✅ | In-scope: install-root suffix enforcement, registry-key migration, default fallback change. Out-of-scope (deferred to FR-010/FR-011): binary rename, `HKCU\...\Run` migration, `Updates` key migration |

## Substitution Map (deliberately narrow, slice 002)

| Original | New | Notes |
|---|---|---|
| `$INSTDIR\weasel-${FLUXING_VERSION}` | `$INSTDIR\fluxing` | Install root; version-free |
| `${FLUXING_VERSION}` in install root | (removed) | Fulfils FR-003 |
| `Software\Rime\Weasel` | `Software\Fluxing\Weasel` | All read/write sites |
| `%APPDATA%\Rime` | `%APPDATA%\fluxing` | User-data fallback |
| Last segment of user-chosen install root | forced to `fluxing` | Idempotent: typing `fluxing` produces no doubling |
| Last segment of user-chosen user-data root | forced to `fluxing` | Idempotent |

## Substitution Map (explicitly OUT)

The following are **not** touched in this slice and remain `Weasel` /
`Rime` as the prior slice decided:

- `Weasel*.exe` and `weasel*.dll` file names (FR-010 deferred).
- `weasel-backup` temp dir (FR-011 deferred).
- `InternalName` / `OriginalFilename` strings in `.rc` files
  (FR-010 deferred).
- `HKCU\Software\Rime\Weasel\Run\WeaselServer` (FR-011 deferred).
- `HKCU\Software\Rime\Weasel\Updates` (auto-update config;
  migration deferred to a later slice).
- `weasel.ico` / `WeaselSetup.ico` (binary asset; FR-010 deferred).
- `LANG=2052 Comments` attribution line ("Powered by Fluxing & RIME")
  (spec 001 final).

## Risk & Edge-Case Notes

- **R-1 (NSIS string handling)**: the suffix-enforcement callback uses
  `StrCpy` and `StrLen`; if the user types a trailing backslash, the
  callback must strip it before checking the last segment.
- **R-2 (legacy key fallback)**: the `.onInit` function reads
  `Software\Fluxing\Weasel\InstallDir` first; if absent, falls back
  to `Software\Rime\Weasel\InstallDir`. The fallback is read-only
  and never re-written — the legacy key is not migrated in place.
- **R-3 (idempotent suffix)**: if the user types
  `C:\Program Files\fluxing` explicitly, the chooser must NOT
  produce `C:\Program Files\fluxing\fluxing`. The callback checks
  the last `\`-delimited segment before appending.
- **R-4 (build environment variance)**: librime and Boost each take
  20–40 minutes to build from scratch. The Codex shell timeout per
  call is 30 minutes; we use background `Start-Process` with
  `WindowStyle Hidden` and poll the log file for completion.
- **R-5 (Clash proxy interaction)**: VS link.exe and cl.exe occasionally
  resolve DNS via WinHTTP; the proxy at `127.0.0.1:7897` is HTTP, not
  SOCKS. We pre-set `HTTP_PROXY` and `HTTPS_PROXY` environment
  variables and `git config --global http.proxy https://...` for
  submodule fetches.

## Complexity Tracking

No Constitution Check violations. No complexity tracking table needed.