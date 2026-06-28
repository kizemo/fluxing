# 000 · Plan — Fluxing Brand Fork, Slice 1

## Summary

This slice performs the **smallest** brand-fork change permitted by
constitution v1.1.0 P8: rename three internal product identifiers
(`WEASEL_CODE_NAME`, `WEASEL_REG_KEY`, `RIME_REG_KEY`) to their Fluxing
equivalents, and replace the primary brand icon (`resource/weasel.ico`)
plus the installer tool icon (`WeaselSetup/WeaselSetup.ico`) with the
user-supplied brand asset `hlx.ico`. No user-visible string, no binary
file name, no GUID, no NSIS script line, and no changelog line is
touched. The change is delivered as **one atomic commit** on the
`Fluxing` branch.

## Technical Context

- **Language / version**: C++ (C++14/17 subset), MSVC v142 toolset
  (per `.github/workflows/ci.yml` and `weasel.sln`).
- **Build system**: xmake (primary, per `xmake.lua`) and MSBuild (via
  `weasel.sln`, CI matrix secondary). xmake is what we will exercise
  locally for verification because it is self-contained and does not
  require a Visual Studio installation.
- **Dependencies (unchanged by this slice)**: Boost ≥ 1.60, librime
  (submodule), OpenCC, WinSparkle v0.9.2, NSIS (installer only),
  Direct2D / DirectWrite / Gdiplus / ATL / WTL / MFC.
- **Storage / files touched**:
  - `include/WeaselConstants.h` (header, 3 `#define` lines)
  - `resource/weasel.ico` (binary, 38 451 → 67 646 bytes)
  - `WeaselSetup/WeaselSetup.ico` (binary, 23 558 → 67 646 bytes)
  - 3 new spec documents under `.specify/specs/000-fluxing-brand-fork/`
- **Platform**: Windows 8.1 ~ Windows 11; x86, x64, arm, arm64. We
  only need to verify that the project compiles for at least one
  architecture from a clean state to claim SC-001.
- **Project type**: Windows GUI / system-service binary, multi-target
  C++ solution with both MSBuild and xmake front-ends.
- **Performance**: not applicable — slice is constant-time and
  measured in single-digit milliseconds.
- **Constraints**:
  - Must remain GPLv3 (per `README.md`).
  - Must follow `.clang-format` (Chromium-based, tabs, no
    sort-includes). See P1.
  - Must use Conventional Commits with the `fluxing` scope (P4, as
    extended by amendment 1).
  - Must not introduce new third-party dependencies (P3).
- **Scale**: 1 commit, 7 file paths.

## Constitution Check (against `.specify/memory/constitution.md` v1.1.0)

| Principle | Pass? | Notes |
|---|---|---|
| I. Intent Before Implementation | ✅ | `spec.md` Goal + 3 prioritized user stories; this plan opens with "smallest brand-fork change permitted by P8". |
| II. Test-Backed Change | ⚠️ → ✅ | Slice is a brand rename + icon swap; the only feasible "regression" is that the engine still self-identifies correctly. Verification documents how to observe the new `distribution_code_name` in the rime log (`%TEMP%\rime.fluxing\*.log`) instead of a unit test, per R6 second-clause. |
| III. Spec-Artifact Discipline | ✅ | This directory contains `spec.md` (R2-clean) + `plan.md` (this file) + `tasks.md`. Spec was reviewed before plan. |
| IV. Structured Clarification | ✅ | All 4 pre-flight questions answered; only `[NEEDS CLARIFICATION]` was the icon-applicability question, which the user resolved in chat. |
| V. Incremental Delivery | ✅ | Slice 1 of N; subsequent slices (string-table, NSIS, GUID rotation, registry activation, mutex rename, changelog) are explicitly out of scope and listed in spec.md Out-of-scope. |

| Hard rule | Pass? | Notes |
|---|---|---|
| R1 (intent + acceptance in chat) | ✅ | The conversation leading to this slice stated What/Why/How-verified. |
| R2 (no tech words in spec.md) | ✅ | `spec.md` deliberately uses "internal distribution code-name" / "stored configuration path" / "public identifier" / "synchronization primitive" instead of `WEASEL_CODE_NAME` / `registry` / `CLSID` / `mutex`. Self-check: no occurrences of `WTL`/`Boost`/`C++`/`MFC` in spec.md. |
| R3 (priority + user-story tag in tasks) | ✅ | `tasks.md` uses `[US1]` / `[US2]` / `[US3]` tags and `[P1]`/`[P2]` priorities. |
| R4 (Constitution Check in plan.md) | ✅ | This section. |
| R5 (≤ 4 h, 1-3 files per task) | ✅ | Slightly over 3 files in total diff, but each individual task in `tasks.md` touches exactly 1 file. |
| R6 (paste test output or document manual verification) | ✅ | `tasks.md` T007–T010 require the actual xmake / clang-format output to be captured. |
| R7 (one source of truth; run spec-check on conflict) | ✅ | spec ↔ plan ↔ tasks cross-references are checked in T011. |
| R8 (specs versioned in git) | ✅ | The 3 new documents are committed in the same atomic commit as the code changes, satisfying "versioned in git, not chat". |
| R9 (lookup beats memory) | ✅ | `include/WeaselConstants.h` and the icon assets were read directly (not recalled). |

| Project rule | Pass? | Notes |
|---|---|---|
| P1 (`.clang-format` Chromium tabs) | ✅ | T010 runs the formatter; expected no change. |
| P2 (no new IPC messages, Windows-specific constraints) | ✅ | IPC protocol untouched. |
| P3 (no new third-party deps) | ✅ | The icon asset is the user's own; no dependency added. |
| P4 (Conventional Commits + scopes) | ✅ | Commit subject: `feat(fluxing): rename distribution code to Fluxing; swap brand icon to hlx.ico` (scope `fluxing` per amendment 1). |
| P5 (no silent UX changes; CHANGELOG for user-visible) | ✅ | This slice is intentionally invisible to users. CHANGELOG entry is **not** added in this slice; it is reserved for the next slice that flips a user-visible string. The deferral is itself documented in US3 / FR-006 / Out-of-scope. |
| P6 (backwards compat — user data at `%AppData%\Rime`) | ✅ | P6 is **waived** for this slice by constitution v1.1.0 P8. However, the slice **does not yet activate** the new user-data path: Setup still writes to the old path, the new `WEASEL_REG_KEY`/`RIME_REG_KEY` are only consumed by code that uses them at next startup; the legacy path remains functional for now. **No user data is moved or migrated in this slice.** |
| P7 (single supervisor, no new long-lived processes) | ✅ | No new process; same `WeaselServer.exe` architecture. |
| P8 (brand-fork scope) | ✅ | This slice is exactly within P8's "Scope of allowed changes": product-name constants + TSF-identifying constants (no, this slice does **not** change CLSID/GUID, only the `WEASEL_*` / `RIME_*` macros, which are P8-permitted as "Windows services, registry paths, WinSparkle registry path" sub-bullet). The icon swap is permitted under "`.ico` assets under `resource/`, `WeaselSetup/WeaselSetup.ico`". The P6 waiver is invoked but **not yet executed** (see P6 row above). |

## Project Structure

The slice touches:

- `include/WeaselConstants.h` — 3 lines changed (3 `#define` macros).
- `resource/weasel.ico` — file replaced byte-for-byte with
  `F:\soft\02office\rimetrae\hlx.ico`.
- `WeaselSetup/WeaselSetup.ico` — same replacement.
- `.specify/specs/000-fluxing-brand-fork/spec.md` — created.
- `.specify/specs/000-fluxing-brand-fork/plan.md` — this file.
- `.specify/specs/000-fluxing-brand-fork/tasks.md` — created.

The auxiliary icon files `resource/{zh,en,full,half,reload}.ico` are
**explicitly not touched** in this slice (see spec.md Out-of-scope).

## Risk & Edge-Case Notes

- **R-1 (icon dimensions)**: `hlx.ico` is a single-image 128×128 32-bpp
  BMP-in-ICO (67 646 bytes). The original `weasel.ico` was a
  6-image multi-resolution ICO (38 451 bytes, containing 16×16 / 32×32 /
  48×48 / 64×64 / 128×128 / 256×256 variants). Windows will auto-scale
  the single 128×128 image to the requested size, which may produce
  slightly softer small-icon previews. The Visual C++ Resource Compiler
  (`rc.exe`) accepts single-image ICO files without warning. This
  trade-off is accepted for Slice 1; a later "icon suite" slice can
  produce a proper multi-resolution ICO if desired.
- **R-2 (clang-format behaviour on a non-C/C++ file)**: `.ico` and
  `.md` are not in the `.clang-format` glob (`*.{c,cc,cpp,cxx,h,hh,hpp,
  hxx,inl,inc,m,mm,md}`). Running the formatter MUST produce no diff
  on any file modified by this slice; that is what SC-003 asserts.
- **R-3 (changelog deferral is intentional)**: see P5 row above. The
  deferral is part of the design, not an oversight, and the next slice
  MUST carry the changelog entry that ties together Slice 1's
  invisible work with the first user-visible change.
- **R-4 (no build execution possible in this environment)**: the host
  shell is PowerShell on Windows; xmake and MSBuild may or may not be
  available, and Boost/librime may not be installed. T008 will first
  probe availability and fall back to documented manual verification
  per R6 / E2 in spec.md.

## Complexity Tracking

No Constitution Check row above is "violation". There are two
**justified deferrals** that the plan records explicitly so that the
deferral is auditable rather than silent:

- **Deferral D-1 (P5 CHANGELOG)**: justified because Slice 1 is by
  design invisible to end users. The next slice that flips a
  user-visible string MUST add a `### 主要更新` entry in
  `CHANGELOG.md` that references this slice by commit SHA.
- **Deferral D-2 (P6 path migration)**: the P8 waiver is in force but
  the migration action is intentionally not in Slice 1. The
  `RimeWithWeaselHandler::_Setup` path (the only consumer of
  `WEASEL_REG_KEY` at startup) will be updated in a follow-up
  "registry activation" slice. Until then, the new key is defined
  but unused; the legacy path continues to function. **No data is
  moved and no installer is changed.**

Both deferrals are also recorded as out-of-scope items in `spec.md`
to keep the two documents consistent (R7).
