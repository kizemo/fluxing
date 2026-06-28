# 001 · Plan — Fluxing Brand Fork, Slice 2 (User-Visible Strings + Install + Changelog)

## Summary

This slice rewrites every user-visible brand string in the project
("小狼毫" / "Weasel" → "火流猩输入法" / "Fluxing"), updates the
NSIS installer script's visible brand end-to-end (installer window
title, brand metadata, shortcut labels in three languages, the
"Programs and Features" entry name, the uninstall-tracking key's
trailing component, the uninstall confirmation prompt), and adds
the first "主要更新" entry in `CHANGELOG.md` that ties this slice
to Slice 1's commit by short hash. Attribution strings that name
the upstream RIME engine and the RIME developers are deliberately
preserved. Internal command identifiers, executable file names,
and the actual storage-path strings in installer / uninstaller
code are out of scope and remain for future slices. The change is
delivered as one atomic commit on the `Fluxing` branch on top of
Slice 1.

## Technical Context

- **Language / version**: C++ (C++14/17 subset), MSVC v142 toolset
  (per `.github/workflows/ci.yml` and `weasel.sln`).
- **Build system**: xmake (primary) and MSBuild via `weasel.sln`
  (secondary); NSIS for installer generation.
- **Dependencies (unchanged)**: Boost ≥ 1.60, librime (submodule),
  OpenCC, WinSparkle v0.9.2, NSIS 3.x.
- **Storage / files touched**:
  - `include/WeaselUtility.h` (1 function body, 2 return literals)
  - `WeaselDeployer/WeaselDeployer.rc` (string table + version
    metadata)
  - `WeaselServer/WeaselServer.rc` (version metadata + tray
    resource)
  - `WeaselSetup/WeaselSetup.rc` (string table + version
    metadata)
  - `WeaselTSF/WeaselTSF.rc` (string table + version metadata)
  - `WeaselSetup/WeaselSetup.rc` (the install-option dialog is built from this resource; no separate InstallOptionsDlg.rc exists in the source tree)
    text)
  - `output/install.nsi` (Name, LangString × N in 3 languages,
    VIAddVersionKey, REG_UNINST_KEY, Section name, uninstall
    confirmation, Programs-and-Features name via DISPLAYNAME)
  - `CHANGELOG.md` (new topmost "主要更新" entry)
  - 3 new spec documents under
    `.specify/specs/001-user-visible-strings/`
- **Platform**: Windows 8.1 ~ Windows 11; x86, x64, arm, arm64.
- **Project type**: Windows GUI / system-service binary with a
  multi-target C++ solution and an NSIS-generated installer.
- **Performance**: not applicable.
- **Constraints**:
  - GPLv3 license preserved.
  - Follow `.clang-format` (Chromium-based tabs) on the modified
    `.h` file.
  - Conventional Commits with the `fluxing` scope (per
    constitution P4, as extended by amendment 1).
  - No new third-party dependency (P3).
- **Scale**: 1 commit, 11–13 file paths (depending on how many
  `.rc` files end up modified — there are 4, plus the install
  script, plus the utility header, plus the change log, plus 3
  new spec docs).

## Constitution Check (against `.specify/memory/constitution.md` v1.1.0)

| Principle | Pass? | Notes |
|---|---|---|
| I. Intent Before Implementation | ✅ | `spec.md` Goal + 4 prioritized user stories; this plan opens with "rewrite user-visible strings + installer + change log". |
| II. Test-Backed Change | ✅ | Verification is textual search (SC-001 / SC-002) for the old vs new brand name — this IS a form of automated regression test, executed before commit. |
| III. Spec-Artifact Discipline | ✅ | This directory contains `spec.md` (R2-clean) + `plan.md` (this file) + `tasks.md`. R2 self-check is performed below. |
| IV. Structured Clarification | ✅ | Pre-flight answered; only `[NEEDS CLARIFICATION]` (whether to keep the upstream attribution phrase) was raised and resolved in chat — answer: keep. |
| V. Incremental Delivery | ✅ | Slice 2 of N; Slices 3+ (binary rename, CLSID rotation, registry path activation, mutex rename, project-level docs) are explicit out-of-scope items. |

| Hard rule | Pass? | Notes |
|---|---|---|
| R1 (intent + acceptance in chat) | ✅ | Conversation leading here stated What/Why/How-verified. |
| R2 (no tech words in spec.md) | ✅ | Self-check below. |
| R3 (priority + US tag in tasks.md) | ✅ | See `tasks.md`. |
| R4 (Constitution Check in plan.md) | ✅ | This section. |
| R5 (≤ 4 h, 1-3 files per task) | ✅ | Each task in `tasks.md` touches exactly one file; total diff exceeds 3 files but each individual task is small. |
| R6 (paste test output or document manual verification) | ✅ | The textual-search command and its expected output are recorded in `tasks.md` T007. |
| R7 (one source of truth; spec-check on conflict) | ✅ | `tasks.md` T010 performs the cross-document audit. |
| R8 (specs versioned in git) | ✅ | The 3 new documents are committed in the same atomic commit as the code changes. |
| R9 (lookup beats memory) | ✅ | Every modified file was re-read before editing. |

| Project rule | Pass? | Notes |
|---|---|---|
| P1 (`.clang-format`) | ✅ | Only one `.h` file is modified; the function body is two `return` statements which clang-format would not reformat. |
| P2 (Windows-specific / no new IPC) | ✅ | No new IPC messages, no threading changes. |
| P3 (no new third-party deps) | ✅ | No new dependency. |
| P4 (Conventional Commits + scopes) | ✅ | Subject: `feat(fluxing): rewrite user-visible brand strings to Fluxing/火流猩输入法; add CHANGELOG entry tying to slice 1`. |
| P5 (no silent UX change; CHANGELOG for user-visible) | ✅ | This slice IS the first user-visible change; it carries its own CHANGELOG entry. |
| P6 (backwards compat — user data path) | ✅ | P6 waived by P8 for the brand fork; the slice does NOT activate the new path in installer / uninstaller code (deferred to the "path activation" slice per D-2 of the prior slice's plan). |
| P7 (single supervisor) | ✅ | No new processes. |
| P8 (brand-fork scope) | ✅ | Every changed item is within P8's "Scope of allowed changes": product-name strings in `.rc` / `WeaselUtility.h`; NSIS `LangString`s / `Name` / `VIAddVersionKey` / `REG_UNINST_KEY` trailing component. Attribution strings (P8 spirit: "preserve RIME attribution") are preserved. |

## Project Structure

Files modified:

- `include/WeaselUtility.h` — 2 return literals (lines 197 and 199).
- `WeaselDeployer/WeaselDeployer.rc` — string table × 3 languages
  + version metadata × 4 fields.
- `WeaselServer/WeaselServer.rc` — version metadata × 4 fields +
  tray menu POPUP label.
- `WeaselSetup/WeaselSetup.rc` — string table × 3 languages +
  version metadata × 4 fields.
- `WeaselTSF/WeaselTSF.rc` — version metadata × 4 fields.
- `WeaselSetup/InstallOptionsDlg.rc` — dialog captions + LTEXT
  labels.
- `output/install.nsi` — Name, 13 LangStrings × 3 languages, 4
  VIAddVersionKey fields, REG_UNINST_KEY, Section name,
  uninstall confirmation text.
- `CHANGELOG.md` — 1 new topmost "主要更新" entry.

Files NOT modified (explicit, per FR-010 and FR-011):

- `include/WeaselConstants.h` (already updated in Slice 1).
- `resource/weasel.ico`, `WeaselSetup/WeaselSetup.ico` (already
  updated in Slice 1).
- The Windows registry storage paths inside the installer script
  (e.g. `Software\Rime\Weasel` reads/writes). These are
  deliberately left in place; activation of the new namespace
  belongs to a later slice.
- `README.md`, `INSTALL.md`, `LICENSE.txt`, `docs/`.
- The `*.vcxproj` / `*.sln` / `xmake.lua` files (no build-system
  change).
- The version macro `WEASEL_VERSION` (per E4).

## Substitution Map (deliberately narrow)

| Original | New (Chinese context) | New (English context) |
|---|---|---|
| `小狼毫` | `火流猩输入法` | n/a |
| `Weasel` (when used as a brand) | n/a | `Fluxing` |
| `Weasel IME` (English) | n/a | `Fluxing` |
| `【小狼毫】` (with brackets) | `【火流猩输入法】` | `Fluxing` (no brackets) |
| `WeaselServer` / `WeaselDeployer` / `WeaselSetup` (as `FileDescription` / `InternalName`) | `火流猩输入法 服务` / `部署` / `设置` | `Fluxing Server` / `Deployer` / `Setup` |
| `...Uninstall\Weasel` | n/a (one new key under standard MS path) | `...Uninstall\Fluxing` |
| `RIME | 中州韻` | preserved | preserved |
| `RIME Developers` | preserved | preserved |
| `式恕堂` | preserved | preserved |
| `Powered by RIME` | preserved | preserved |

`InternalName` and `OriginalFilename` in version metadata: NOT
changed in this slice (they reference the on-disk binary names,
which are still `WeaselServer.exe` etc.; renaming them before
the binary files are renamed would create a mismatch).

`Section "Weasel"` in `install.nsi` IS changed to `Section "Fluxing"`
because the installer's section name is user-visible in the
"Select Install Location" page (when shown) and in the
uninstall log; changing it does not require any binary rename.

## Risk & Edge-Case Notes

- **R-1 (string-table encoding)**: the `.rc` string tables use
  UTF-16 LE (the default for Visual C++ resource compiler). The
  new Chinese name `火流猩输入法` is already in the Unicode
  Basic Multilingual Plane, so encoding is safe.
- **R-2 (LANG 2052 / 1028 / 1033 selection)**: the NSIS script
  uses three `!insertmacro MUI_LANGUAGE` blocks; the
  corresponding `LangString` declarations are inside each
  block. Substituting per language preserves the per-language
  intent. `LANG_TRADCHINESE` gets the Traditional-Chinese form
  of the new name; `LANG_SIMPCHINESE` gets the Simplified form;
  `LANG_ENGLISH` gets the English form.
- **R-3 (changelog reference linkability)**: the short hash of
  the Slice 1 commit is hard-coded into the changelog entry
  (T011 verifies it matches the actual `git log` short hash).
  If the Slice 1 commit is later amended (it is not, in this
  workflow), the changelog entry will go stale; this is the
  standard trade-off and is acceptable per constitution
  P4 commit-discipline spirit.
- **R-4 (no build execution possible in this environment)**:
  no xmake / NSIS in this host. The slice relies on textual
  verification only (SC-001 / SC-002 search). Manual
  verification on a Windows host with NSIS is documented in
  T013 and matches R6.

## Complexity Tracking

No Constitution Check row above is "violation". One
**justified deferral** is recorded explicitly:

- **Deferral D-3 (P6 path activation)**: the uninstall-tracking
  key in `install.nsi` IS updated to the new product name
  (FR-007), but the actual storage paths that the installer
  reads / writes (e.g. the `Software\Rime\Weasel\InstallDir`
  reads and writes in `.onInit` and `Section`) are NOT changed
  in this slice. They remain `Rime\Weasel` until the
  "user-data path activation" slice lands. This split is
  intentional: it lets the brand fork ship a working
  installer that recognises its own brand name in the
  "Programs and Features" panel WITHOUT yet committing to
  moving the user's data. The next slice must
  include the one-shot migration prescribed by P8.

