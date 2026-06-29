# 003 — Spec — Windows Build Pipeline (Fluxing 0.17.4)

## Intent

The Fluxing fork on Windows needs a reproducible end-to-end build that
produces a signed-ready NSIS installer in `release/fluxing-<ver>-installer.exe`.

What worked today (2026-06-29, automated by Codex):
- 30+ year-old project (`rime/weasel` → `kizemo/fluxing`) was successfully
  built from a clean `F:\b183\` Boost 1.83.0 source tree.
- 6 third-party deps (glog, gtest, leveldb, marisa, opencc, yaml-cpp) built
  for both x64 and x86 (Win32).
- librime 1.13.1 built as both x64 (8664) and x86 (14C).
- weasel.sln (WeaselTSF, WeaselUI, WeaselIPC, WeaselServer, WeaselDeployer,
  WeaselSetup, RimeWithWeasel) built for Release|x64 and Release|Win32 via
  msbuild.
- NSIS 3.x produced `fluxing-0.17.4.0-installer.exe` (10.5 MB).
- `weasel.props` rendered from `weasel.props.template` via `render.js`.

The product change this spec introduces:
- A documented, single-shot build flow for the Fluxing Windows installer.
- Three shim headers (`X11/keysym.h`, `utf8.h`, `darts.h`) under
  `librime/include/` that librime expects on Windows but the upstream
  `librime` project does not ship.
- A 1-BOM (not 4-BOM) `output/install.nsi` for NSIS 3.x compatibility.
- A `release/` directory holding the installable artifact.

## User Stories

### US1 — Reproducible build (P1, MVP)
- **Why P1**: Without a clean reproducer, future maintainers cannot ship
  Fluxing patches.
- **Independent test**: On a fresh Windows VM with VS 2022 BuildTools,
  Boost 1.83, NSIS 3.x, and CMake 3.20+, run the build flow and observe
  a `release/fluxing-0.17.4.0-installer.exe` artifact.
- **Acceptance**:
  - Given a clean Boost 1.83 in `F:\b183\`
  - When the build flow runs (x64 + Win32 rime, then weasel, then NSIS)
  - Then `release/fluxing-0.17.4.0-installer.exe` exists and is a valid
    Windows installer that defaults install to `C:\Program Files\Fluxing`.

### US2 — Install path brand consistency (P1, MVP)
- **Why P1**: Spec 002 forced `fluxing` suffix via `ForceFluxingSuffix`,
  but the default `$INSTDIR` still pointed to `…\Rime`. Spec 003
  corrects the default to `…\Fluxing` so the **default** install already
  shows the right brand (no rewriting needed).
- **Independent test**: Read `output/install.nsi` and confirm lines
  135/137/139/144/146 say `Fluxing` (not `Rime`).
- **Acceptance**:
  - Given the install runs with all defaults
  - When the directory page is shown
  - Then the suggested path ends in `\Fluxing` (becomes
    `…\Fluxing\fluxing\weasel` after `ForceFluxingSuffix`).

### US3 — Build pipeline documentation (P2)
- **Why P2**: Future agents / devs need to know the build order, the
  shim headers, the env.bat hardlink, and the BOM trimming.
- **Acceptance**:
  - `plan.md` enumerates the build steps.
  - `tasks.md` lists each task with single-file scope.
  - `release/` holds the installer binary.

## Substitution Map (code changes only)

| Layer | Upstream term | Fluxing term | Files |
|---|---|---|---|
| Install default | `$PROGRAMFILES*\Rime` | `$PROGRAMFILES*\Fluxing` | `output/install.nsi` L135/137/139/144/146 |
| NSIS BOM | 4× UTF-8 BOM | 1× UTF-8 BOM | `output/install.nsi` |

## Non-Goals (deferred)
- **ARM64 weasel rebuild**: the existing 2025-era `weaselARM64.dll/.ime`
  binaries are kept (`/nonfatal` in `install.nsi`).
- **.ime wrapper generation**: `weasel.ime` and `weaselx64.ime` are
  the 2025-era wrappers (kept as-is for this release).
- **NSIS plugin signing / Authenticode**: deferred to release-time.
- **Update URL rewriting (`URLInfoAbout` / `HelpLink`)**: deferred until
  first GitHub release is published. Spec keeps upstream
  `https://rime.im/` per attribution rule.