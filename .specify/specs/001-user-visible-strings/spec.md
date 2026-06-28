# 001 · Fluxing Brand Fork — Slice 2 (User-Visible Strings + Install + Changelog)

> Brand-fork worktree on top of the upstream Rime/Weasel codebase, governed by
> project constitution v1.1.0 (P8 "Brand Fork: Fluxing / 火流猩输入法").
> This is **Slice 2 of N**, immediately following Slice 1 (which renamed the
> internal distribution code-name and swapped the primary brand icon).

## Goal (user-experience level)

After Slice 1, the product's internal self-identification is "Fluxing" but
the end user still sees "小狼毫" / "Weasel" everywhere in the UI and the
installer. Slice 2 surfaces the new identity to the user: every
user-visible brand string in the application windows, the system tray
menu, the dialog captions, the help messages, the installer's name and
metadata, the desktop / Start-menu shortcuts, the uninstall entry, and
the project's public change log are rewritten to the new brand identity.

The user must be able to install, run, and uninstall the product, and
throughout the entire flow see **only** the new brand name and never the
old one. Any attribution text that names the upstream open-source
project (the RIME engine) is **deliberately preserved** — the brand
fork does not detach the product from its upstream community.

## User stories (prioritized)

### US1 — In-product brand strings reflect Fluxing [P1, MVP]

**Why P1**: When a user launches the deployer tool, opens the
settings dialog, opens the dictionary manager, or right-clicks the
tray icon, every window title, status text, help message, and about
label they see must read "Fluxing" or "火流猩输入法" — never the old
name. This is the most visible part of the rename.
**Independent test**: Launch the deployer / settings tool, open each
dialog, and observe every title, caption, and message. None of them
should mention the old brand name.
**Acceptance scenarios**:
- Given the user runs the deployer tool, when the deployer window
  appears, then its title and every dialog caption it can open read
  the new brand name.
- Given the user right-clicks the system tray icon, when the context
  menu appears, then the menu's top-level label reads the new brand
  name.
- Given the user runs the setup tool with the "show help" command,
  when the help message box appears, then every sentence referring to
  the product uses the new brand name.

### US2 — Installer surfaces the new brand end-to-end [P1, MVP]

**Why P1**: The installer is the user's first contact with the
product. The installer window title, the brand metadata embedded in
the resulting files, the desktop and Start-menu shortcut names, the
"Programs and Features" entry, and the uninstall confirmation
dialog must all read the new brand. If any one of them still says the
old name, the user reasonably concludes that they have installed the
wrong product.
**Independent test**: Run the generated installer on a clean Windows
machine, observe every installer window, then check the resulting
shortcuts, the Programs and Features entry, and the uninstall prompt.
None of them should mention the old brand name.
**Acceptance scenarios**:
- Given the user starts the installer, when the installer window
  opens, then the title reads the new brand name (with the new
  version number).
- Given the installation completes, when the user opens the Start
  menu, then every shortcut under the new product folder reads the
  new brand name.
- Given the user opens "Programs and Features", when the new product
  entry is displayed, then the entry's name and description read the
  new brand name.
- Given the user starts uninstallation, when the confirmation prompt
  appears, then the prompt refers to the new brand name.

### US3 — Public change log carries an entry that ties this slice to Slice 1 [P1, MVP]

**Why P1**: Per project policy, every user-visible change must be
recorded in the public change log. Slice 1 was deliberately invisible
to the user and was therefore intentionally not logged; Slice 2 is
the first slice that flips user-visible strings, so the first
"主要更新" entry in the change log must reference Slice 1's commit
identifier (the deferred entry from the previous slice's plan) AND
describe the new user-visible brand.
**Independent test**: Open the project's public change log. The
topmost "主要更新" entry exists, names the new brand, and contains
the commit identifier of Slice 1.
**Acceptance scenarios**:
- Given the user opens the change log, when they read the topmost
  entry, then it names the new brand and the new product behavior.
- Given the user reads the topmost entry, when they look for the
  Slice 1 commit identifier, then it is present (as a 7-character
  short hash) so the two slices can be linked.

### US4 — Upstream attribution is preserved [P1, MVP]

**Why P1**: The product is built on top of an upstream open-source
project (RIME) and an upstream community (the RIME developers). The
brand fork only changes the new product's identity; it does not
re-license the product, does not republish under a new organization,
and does not erase the upstream attribution. Every existing
attribution string that names the upstream project or community must
remain untouched in this slice.
**Independent test**: Inspect every "Powered by" / "copyright" /
"developers" string. All such strings must still be present and must
still name the upstream project or community.
**Acceptance scenarios**:
- Given the user opens the installer's "Properties" dialog, when
  they look at the metadata, then the "Comments" field still names
  the upstream engine and the "LegalCopyright" field still names the
  upstream developers.
- Given the user opens the installer source file, when they search
  for the upstream engine's name, then every match is from an
  attribution string, never from a brand label.

## Functional requirements

- **FR-001** Every user-visible string in the deployer tool's
  resource string table that currently reads "小狼毫" or "Weasel"
  MUST read the new brand identity instead. The new strings are
  the new Chinese name "火流猩输入法" and the new English name
  "Fluxing", chosen per the user-facing language as the original
  strings did.
- **FR-002** Every user-visible string in the setup tool's resource
  string table that currently reads "小狼毫" or "Weasel" MUST read
  the new brand identity instead. The new strings are the new
  Chinese name and the new English name as described in FR-001.
- **FR-003** Every user-visible string in the server tool's resource
  string table and tray-menu resource that currently reads
  "小狼毫" or "Weasel" MUST read the new brand identity instead.
- **FR-004** Every user-visible string in the TSF text service's
  resource string table that currently reads "小狼毫" or "Weasel"
  MUST read the new brand identity instead.
- **FR-005** The helper function in the public utility header that
  returns the product's "self-identification" name string MUST
  return the new Chinese name when the user's preferred UI language
  is Chinese, and the new English name otherwise. The function name
  and its call sites are NOT changed in this slice.
- **FR-006** The installer script's visible brand name (its window
  title), all shortcut labels in all supported languages, the brand
  metadata embedded into the resulting files, and the "Programs and
  Features" entry name MUST all use the new brand identity. The
  uninstall confirmation prompt MUST refer to the new brand.
- **FR-007** The uninstall-tracking key in the installer script
  MUST use the new brand identity as the trailing path component
  under the standard Microsoft uninstall key, so that "Programs and
  Features" identifies the new product by its new name.
- **FR-008** Attribution strings that name the upstream engine
  (RIME) or the upstream community (the RIME developers, 式恕堂)
  MUST remain untouched.
- **FR-009** The public change log's "主要更新" section MUST have
  a new topmost entry that names the new brand, describes the
  user-visible behavior change, and references the commit identifier
  of Slice 1 (a 7-character short hash) so the two slices are
  linked.
- **FR-010** Internal command identifiers, resource identifiers,
  win32 numeric identifiers, and executable file names are OUT OF
  SCOPE for this slice and MUST NOT be modified.
- **FR-011** Internal constant strings that drive configuration
storage locations (under the system configuration tree and under the
  user-data directory) are OUT OF SCOPE for this slice and MUST
  NOT be modified — they were already updated in Slice 1 to the
new product name, and activating the new paths in installer and
  uninstaller code is a deferred item tracked in the prior slice's
  plan.
- **FR-012** The change MUST be expressible as a single atomic
  commit with a Conventional-Commits subject that names the
  brand-fork scope, per project policy.

## Success criteria

- **SC-001** A repository-wide text search for the old brand name
  within the files modified by this slice returns **zero** matches
  inside any user-facing string (resource string tables, dialog
  captions, menu labels, installer visible name, shortcut labels,
  uninstall entry name, brand metadata fields). Attribution strings
  that mention the upstream project are excluded from this search
  and MUST continue to match.
- **SC-002** A repository-wide text search for the new brand name
  within the files modified by this slice returns matches in every
  location where the old brand name used to appear.
- **SC-003** The version-controlled diff for this slice, relative
  to the previous commit, touches the documented resource string
  tables, the public utility header, the installer script, and the
  public change log. It does NOT touch any internal command
  identifier, any executable file name, or any system-configuration storage
  location.
- **SC-004** The public change log's topmost "主要更新" entry
  contains the 7-character short hash of the Slice 1 commit and
  describes the user-visible change in terms the user can verify by
  observation (open the deployer / install the product / open the
  Start menu).

## Edge cases

- **E1** If a text appears to be a brand string but is actually a
  functional label (e.g. a status indicator like "中/英/全/半"
  that marks input-mode state rather than product identity), the
  label MUST NOT be rewritten. The slice MUST leave functional
  labels alone.
- **E2** If a brand string and an attribution string share a
  container (e.g. a multi-line value containing both), the slice
  MUST rewrite only the brand portion and leave the attribution
  portion untouched. (This case does not arise in the current
  source, but the slice MUST be safe if it did.)
- **E3** The default install directory used by the installer
  (`$PROGRAMFILES64\Rime`) is OUT OF SCOPE and MUST NOT be changed
  in this slice. The install directory belongs to the
  "user-data path activation" deferred item.
- **E4** The version macro in the installer script and in the
  build system is OUT OF SCOPE for this slice. The macro name is a
  build-system concern, not a brand concern, and renaming it
  belongs to a future "build macro" slice.

## Assumptions

- **A1** "火流猩输入法" (Chinese) and "Fluxing" (English) are the
  new brand strings, as confirmed by the user at the start of the
  brand-fork project.
- **A2** The user-facing language selection for the new strings
  follows the same scheme as the original: the Chinese name when
  the user's preferred UI language is Chinese, the English name
  otherwise. This is the same rule already encoded in the helper
  function modified by FR-005.
- **A3** The build system can produce a fresh installer from the
  modified installer script on a Windows host with the installer-generation tool available.
  The slice's verification does not require the installer to be
  built in the current host environment, only that the modified
  script is syntactically and semantically consistent.
- **A4** The brand-fork branch `Fluxing` already carries the
  Slice 1 commit. Slice 2 is authored on top of that commit.

## Out of scope (explicit non-goals; see `plan.md` Complexity Tracking)

- Renaming any internal command identifier (resource ID, win32
  numeric ID, command-line flag like `/deploy`).
- Renaming any executable file name.
- Rotating any public service identifier or profile identifier.
- Changing the default install directory used by the installer
  (the `$PROGRAMFILES64\Rime` literal).
- Changing the version macro in the build system and the
  installer.
- Modifying internal storage paths in installer / uninstaller
  code (the system-configuration keys still pointing at the upstream configuration area). Slice 1 already
  renamed the *constants*; this slice does not yet *activate* them.
- Touching any file under the project's `docs/` directory or
  the top-level `README.md` / `INSTALL.md` files. Those belong to
  a separate "project documentation" slice.
- Touching the project's `LICENSE.txt`. License attribution is
  preserved as-is.
- Rewriting the "Powered by RIME" / "RIME Developers" attribution
  strings.



