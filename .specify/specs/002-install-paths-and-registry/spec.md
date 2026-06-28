# 002 · Spec — Fluxing Install Paths and Registry

## Intent

What the user does today and the gap:
- Today the Weasel installer drops files into `$INSTDIR\weasel-${FLUXING_VERSION}`
  (e.g. `C:\Program Files\weasel-0.1.0`) — version-stamped, so each new release
  produces a new directory, breaking upgrade-in-place.
- Today the user-data directory falls back to `%APPDATA%\Rime` and the registry
  key falls back to `Software\Rime\Weasel` — neither aligns with the Fluxing
  brand already introduced in spec 001.
- Today upgrading requires the new installer to read `Software\Rime\Weasel`
  to find the old path — fragile and brand-inconsistent.

The product change this spec introduces:
- A single, version-free Fluxing install root (e.g. `C:\Program Files\fluxing`)
  that all upgrades overwrite in place.
- A version-free Fluxing user-data root (e.g. `%APPDATA%\fluxing`) that
  follows the same in-place upgrade contract.
- All Fluxing registry keys migrated to the `Fluxing` namespace
  (`Software\Fluxing\Weasel`) and the user-data default
  (`%APPDATA%\fluxing`).
- During install the user is offered a directory chooser for both
  the install root and the user-data root; both default to the Fluxing
  paths and both are forced to end in a `fluxing` segment.

## User Stories

### US1 — First-time install (P1, MVP)

- **Why P1**: No Fluxing install can happen without a path contract.
- **Independent test**: On a clean Windows VM with no prior Weasel/Fluxing
  install, run the installer. Verify install root and user-data root are
  created in the chosen locations (or the Fluxing defaults), and that
  both end in `fluxing` even if the user typed a different terminal segment.
- **Acceptance**:
  - Given the user accepts all defaults
  - When the installer completes
  - Then the install root is `C:\Program Files\fluxing`
    and the user-data root is `%APPDATA%\fluxing`.
  - Given the user picks a custom path
  - When the installer completes
  - Then the install root is `<chosen>\fluxing`
    and the user-data root is `<chosen-parent>\fluxing`,
    regardless of what the user typed.
  - Given the install root is created
  - Then it contains the binary files (the existing pre-compiled set is
    acceptable for the first installer; later slices will re-compile).

### US2 — Upgrade install (P1, MVP)

- **Why P1**: Every Fluxing user who upgrades is a US2 user.
- **Independent test**: Install Fluxing 0.1.0, then run the new installer.
  Verify the new install overwrites in place; the user-data directory
  is not relocated; no versioned subdirectory is created.
- **Acceptance**:
  - Given a prior Fluxing install exists at registry-stored path
  - When the new installer runs
  - Then it pre-fills the install-root chooser with the prior path
    (and the prior user-data path).
  - Then it does NOT create a new version-stamped subdirectory.
  - Then it overwrites the binary files in the existing root.

### US3 — Brand-aligned registry (P1, MVP)

- **Why P1**: spec 001 deferred registry migration to this slice.
- **Independent test**: After install, inspect `HKLM` and `HKCU`. All
  Fluxing-related values live under `Software\Fluxing\Weasel`.
- **Acceptance**:
  - Given a fresh install
  - Then `HKLM\Software\Fluxing\Weasel\InstallDir` is set to the install root.
  - Then `HKCU\Software\Fluxing\Weasel\RimeUserDir` is set to the user-data root.
  - Then no `HK*\Software\Rime\Weasel\*` Fluxing-related values are written.
  - Given an upgrade over a prior Weasel/Fluxing install
  - Then the installer still finds the prior path
    (read either the new key or the legacy key as a fallback).

## Functional Requirements

- **FR-001** The install root MUST be forced to end with a `fluxing`
  directory segment, regardless of the user's chooser input.
- **FR-002** The user-data root MUST be forced to end with a `fluxing`
  directory segment, regardless of the user's chooser input.
- **FR-003** The install root MUST NOT contain a version-stamped segment.
- **FR-004** On upgrade, the install-root chooser MUST be pre-filled
  from the previously-stored registry value.
- **FR-005** On upgrade, the user-data root MUST be pre-filled
  from the previously-stored registry value.
- **FR-006** All Fluxing-related registry writes MUST go under
  `Software\Fluxing\Weasel`.
- **FR-007** The default user-data root MUST be `%APPDATA%\fluxing`.
- **FR-008** Legacy `Software\Rime\Weasel` reads MUST continue to work
  so existing users can be upgraded in place.

## Success Criteria

- **SC-001** A clean install on a Windows 10/11 VM with no prior install
  creates both roots at the Fluxing defaults and both end in `fluxing`.
- **SC-002** A clean install with custom paths results in
  `<chosen>\fluxing` and `<chosen-parent>\fluxing` regardless of the
  user-typed terminal segment.
- **SC-003** An upgrade install reads the prior `InstallDir` and
  `RimeUserDir` from either the new `Fluxing` key or the legacy
  `Rime` key, and overwrites in place.
- **SC-004** After install, `reg query HKLM\Software\Fluxing\Weasel`
  shows the install root; `reg query HKCU\Software\Fluxing\Weasel`
  shows the user-data root.

## Out of Scope (deferred to later slices)

- Renaming the on-disk binaries (`Weasel*.exe`, `weasel*.dll`,
  `weaselARM*.dll`, etc.) — spec 001 FR-010.
- Migrating `HKCU\...\Run\WeaselServer` and other non-install paths —
  spec 001 FR-011.
- Migrating `HKCU\Software\Rime\Weasel\Updates` (auto-update config).
- Auto-update channel rewriting — `WeaselSetup` currently uses
  `WinSparkle` whose keys live under `Software\Rime\Weasel`.
- Compiling a C++ change to `WeaselServer.cpp` itself — this spec
  only changes the C++ source-level registry-key *strings*; the
  binary rebuild follows separately.

## Edge Cases

- User picks a path whose terminal segment is already `fluxing` —
  chooser MUST NOT produce `fluxing\fluxing` (idempotency).
- User picks a path on a non-ASCII volume (e.g. `D:\程序\`) — install
  MUST still produce a `fluxing` subdir.
- User picks a path with trailing backslash — chooser MUST normalise.
- Installer is launched in non-admin context — pre-flight detects
  and re-launches with elevation; this slice does NOT change that
  behaviour.
- A prior Weasel install used a path that is not in the default
  Fluxing location — the upgrade must still find it via legacy
  registry fallback.

## Assumptions

- Windows 10/11 (administrator-required for `C:\Program Files\*`).
- NSIS 3.x is available on the build machine (verified
  `C:\Program Files (x86)\NSIS\makensis.exe`).
- Visual Studio 2022 with C++ workload is available on the build machine
  (verified `C:\Program Files\Microsoft Visual Studio\2022\BuildTools`).
- The `release/` directory is the canonical location for the produced
  installer; a `.gitignore` rule covers it.