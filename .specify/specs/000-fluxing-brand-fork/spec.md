# 000 · Fluxing Brand Fork — Slice 1 (Internal ID + Brand Icon)

> Brand-fork worktree on top of the upstream Rime/Weasel codebase, governed by
> project constitution v1.1.0 (P8 "Brand Fork: Fluxing / 火流猩输入法").
> This spec is the first of several planned slices (Slice 1 of N).

## Goal (user-experience level)

The end-user-facing "flavor" of the product is being renamed from the
upstream identity to a new identity, **"Fluxing" / "火流猩输入法"**. Slice 1
makes the **internal product code-name** and the **primary product icon**
reflect the new identity, without changing any other user-visible string,
file name, or system path yet. From the user's point of view after this
slice, nothing visible has changed — but every internal component that
reads the product's "self-identification" sees the new name, and the
on-disk icon resource is the new one. Subsequent slices will surface the
new name in the UI, the installer, the stored configuration locations,
and the inter-process message layer.

## User stories (prioritized)

### US1 — Internal product identity renamed [P1, MVP]

**Why P1**: This is the smallest change that establishes the fork at the
identity layer. Every other slice (visible strings, installer, stored
configuration, inter-process messaging) builds on top of this and would
otherwise have to be retargeted later.
**Independent test**: A diagnostic that prints the product's
self-identification (an internal log line, or the engine's
"distribution code-name" field) shows the new name, not the old one.
**Acceptance scenarios**:
- Given the product's startup log is captured, when the product starts,
  then the log line that records the product's self-identification
  contains the new name and does **not** contain the old name.
- Given a third-party component that asks the product "what is your
  distribution name?", when the product responds, then the response is
  the new name.

### US2 — Primary product icon swapped to the new brand asset [P1, MVP]

**Why P1**: A user looking at the installed product's files in Windows
Explorer, or the setup tool's title bar, sees the new brand icon. The
icon is a primary brand signifier; without it, the rename reads as
incomplete.
**Independent test**: A Windows resource viewer (or a standard image
preview) opened on the primary brand-asset file and the installer-icon
file shows the new brand glyph, not the old one.
**Acceptance scenarios**:
- Given the user opens the primary brand-icon file location in Windows
  Explorer, when they preview the icon, then the rendered image is the
  new brand glyph.
- Given the user runs the setup tool, when the setup tool displays its
  window/taskbar icon, then the displayed image is the new brand glyph.

### US3 — Changelog, public docs, and other user-visible artifacts intentionally untouched [P2]

**Why P2**: This slice is invisible to end users. Per project policy,
changelog entries are added only when a user-visible change ships. The
"visible-strings" slice (next) will carry the changelog entry that ties
together slices 1+2+3. For now, the changelog and the user-facing docs
deliberately still describe the upstream identity.
**Independent test**: A user reads the project's public change log and
the main readme — both still describe the upstream identity and contain
no mention of the new name.
**Acceptance scenarios**:
- Given the user opens the project's public change log, when they read
  the latest entry, then it does not mention the new name.
- Given the user opens the main readme, when they read the title, then
  it still shows the upstream title.

## Functional requirements

- **FR-001** The product's internal distribution code-name constant MUST
  evaluate to the new name (literal: `"Fluxing"`) and MUST NOT contain
  the upstream distribution code-name as a substring.
- **FR-002** The product's "shared configuration path" constant MUST
  evaluate to a path that begins with the new brand namespace
  (literal: `Software\Fluxing\Fluxing`) and MUST NOT contain the
  upstream configuration path as a substring.
- **FR-003** The product's "general configuration path" constant MUST
  evaluate to a path that begins with the new brand namespace
  (literal: `Software\Fluxing`) and MUST NOT contain the upstream
  configuration path as a substring.
- **FR-004** The primary brand icon file in the project's resources
  directory MUST be byte-identical to the supplied brand asset
  `hlx.ico` (single 128×128 32-bpp BMP-in-ICO, 67 646 bytes) and MUST
  not contain the old icon's bytes.
- **FR-005** The installer tool's icon file MUST be byte-identical to
  the same supplied brand asset, and MUST not contain the old icon's
  bytes.
- **FR-006** No other file in the repository is modified by this slice.
  In particular: no other icon file in the resources directory; no
  user-visible string in any string resource; no installer script
  string; no binary product file name; no public identifier (CLSID /
  GUID); no synchronization primitive name; no inter-process message;
  no changelog line; no main readme / install-guide line.
- **FR-007** The change MUST be expressible as a single atomic commit
  with a Conventional-Commits subject that names the brand-fork scope,
  per project policy.

## Success criteria

- **SC-001** All buildable targets in the project (one per supported
  architecture) compile successfully from a clean state.
- **SC-002** The version-controlled diff for this slice, relative to
  the previous commit, touches exactly 7 file paths: one public
  constant header, two icon files, and three new specification
  documents under the specifications directory. No other path appears
  in the diff.
- **SC-003** A code-style formatter run with the project's
  configuration produces no change to any file modified by this slice.
- **SC-004** The compiled product's resource directory contains the new
  brand icon as the primary brand-icon file.
- **SC-005** The compiled product's distribution code-name constant, as
  exposed through the public constants header, equals the new name and
  does not equal the upstream name.

## Edge cases

- **E1** If the supplied brand asset is missing or not a valid icon
  file of the expected dimensions / bit depth, the slice MUST abort
  before any modification, and the user MUST be informed which
  validation failed.
- **E2** If the build system cannot be exercised in the current host
  environment (e.g. no compiler toolchain, no third-party C++ library
  such as a pinned dependency of the upstream codebase), the slice is still permitted to land, but the manual
  verification steps in `plan.md` MUST be documented and the agent MUST
  NOT claim "builds successfully" without producing the actual compiler
  output.

## Assumptions

- **A1** The supplied brand asset is the canonical icon for the new
  product. (Confirmed: user-provided path resolves to a single 128×128
  32-bpp BMP-in-ICO of 67 646 bytes.)
- **A2** Future slices will handle user-visible string changes, file
  name changes, public identifier (CLSID / GUID) rotation, stored
  configuration path activation, synchronization primitive rename, and
  changelog entry. This slice deliberately does none of those.
- **A3** The project constitution v1.1.0 (P8) is in force; this slice
  is a permitted brand-fork change under P8.
- **A4** The brand-fork branch is `Fluxing`, based on the upstream
  `master` at the captured HEAD. The new commit will be authored on
  `Fluxing`.

## Out of scope (explicit non-goals; see `plan.md` Complexity Tracking)

- Renaming any user-visible string in any string resource /
  utility-function return value / installer script / tray menu /
  public readme / install guide.
- Rotating the TSF public service identifier or the TSF profile
  identifier.
- Renaming any binary product file.
- Updating the changelog or user-facing documentation.
- Changing the user-data directory or the install directory.
- Modifying the inter-process protocol or the response schema.
- Replacing the auxiliary icon files (Chinese / English / full-shape /
  half-shape / reload) in the resources directory. They will be
  addressed in a later icon slice if the user asks for them.

