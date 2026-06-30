# 013 - Spec - /D= silent-install path override is broken

> Scope: fix the silent-install path override behavior that has been
> broken since at least 0.18.4.0 (commit b98c7d5). Discovered while
> implementing spec 012 cleanup (L13-fix-2 / L14-fix, commit 053515b)
> and verified against the released 0.18.4.0 installer.

## 0. Context

When a user runs the Fluxing installer silently with a custom install path:

```
cmd /c "fluxing-0.18.5.0-installer.exe /S /D=C:\TEMP\foo\ProgramFiles"
```

The installer should install under
`C:\TEMP\foo\ProgramFiles\fluxing\weasel\` (with
`ForceFluxingSuffix` appending the final `\fluxing\`).
Instead, the installer ignores the `/D=` flag and installs to
`C:\Program Files\fluxing\weasel\` (the default).

The L13 / L13-fix-2 / L14-fix work in 0.18.3.0 - 0.18.5.0 preserved this bug.
The AGENTS.md \u00a72.5 smoke test recipe (which assumes `/D=` works) also still
uses the now-broken behavior as its baseline.

## 1. Product perspective

### 1.1 Goal

Restore silent-install `/D=` override behavior so that:

```
installer.exe /S /D=<custom-path>   -> installs under <custom-path>\fluxing\weasel\
installer.exe /S                    -> installs under $PROGRAMFILES64\fluxing\weasel\
installer.exe                       -> (GUI: directory page; user chooses)
```

### 1.2 User stories

**US1-A** [P1]: as a power user / scripted deployer, I want to install
Fluxing silently to a non-default path (e.g. `D:\Apps\Fluxing\`)
so I can co-locate it with my other tools without polluting `C:\Program Files\`.

**US1-B** [P1]: as a CI / smoke-test runner, I want AGENTS.md \u00a72.5 to work as
written, so my smoke test runs in a clean `C:\TEMP\fluxing-test\` tree
without leaving real install artifacts on the test box.

**US1-C** [P2]: as a user upgrading from 0.18.3.x with a non-default install
path, I want upgrade to find my existing install (the L13 logic handles this
via the registry, but `/D=` is the more reliable signal for scripted upgrades).

### 1.3 Acceptance

Given a clean registry (no `HKLM\Software\Fluxing\Weasel\InstallDir`)
And a clean `C:\Program Files\fluxing\` (no prior install)
When `installer.exe /S /D=C:\TEMP\foo\ProgramFiles` runs
Then:
- Final `HKLM\Software\Fluxing\Weasel\InstallDir`
  = `C:\TEMP\foo\ProgramFiles\fluxing\`
- `C:\TEMP\foo\ProgramFiles\fluxing\weasel\WeaselServer.exe` exists
- `C:\TEMP\foo\ProgramFiles\fluxing\user1\fluxing\` exists
- `C:\Program Files\fluxing\` is unchanged (not touched)

## 2. Technical perspective (preliminary)

### 2.1 Root cause (per L17 gotcha 3)

`install.nsi` line 236:

```nsi
InstallDirRegKey HKLM "Software\Fluxing\Weasel" "InstallDir"
```

This directive tells NSIS to read the registry at startup and set
`$INSTDIR` to that value BEFORE `.onInit` runs. The `.onInit` function
runs AFTER this, and any code that checks `$INSTDIR` will see the
registry-loaded value, not the `/D=` value (if any).

Per NSIS docs, `/D=` is supposed to override the InstallDirRegKey value
if non-empty. The override is implemented by NSIS itself BEFORE
`InstallDirRegKey` reads the registry, so the order should be:

  1. NSIS parses CLI args. If `/D=path` is present and non-empty,
     NSIS sets `$INSTDIR = path`.
  2. `InstallDirRegKey` runs. If the registry has a value, it OVERWRITES
     `$INSTDIR` with the registry value.
  3. `.onInit` runs. `$INSTDIR` is now the registry value, not the `/D=` value.

The bug is in step 2: `InstallDirRegKey` overrides `/D=` even when
`/D=` is non-empty. The clean fix is to remove `InstallDirRegKey` and
replicate its behavior manually in `.onInit`:

```nsi
Function .onInit
  $INSTDIR = ""   ; explicit reset - clear any /D= leak from previous runs
  $R0 = ""
  ReadRegStr $R0 HKLM "Software\Fluxing\Weasel" "InstallDir"
  StrCmp $R0 "" 0 use_reg_check
  ReadRegStr $R0 HKLM "Software\Rime\Weasel" "InstallDir"
  StrCmp $R0 "" 0 use_reg_check
  ; No prior install + no /D=: fall through to default.
  Goto set_default
use_reg_check:
  ; [L13-fix-2 check_reg* block from commit 053515b goes here]
  ...
  Goto use_default
use_default:
  StrCpy $INSTDIR "$PROGRAMFILES64\fluxing"
  StrCpy $R0 ""
  Goto skip
set_default:
  ; Honor /D= if user provided it (NSIS sets $INSTDIR from /D= BEFORE
  ; .onInit runs; we just need to NOT overwrite it here).
  StrCmp $INSTDIR "" 0 skip_default
  StrCpy $INSTDIR "$PROGRAMFILES64\fluxing"
skip_default:
skip:
  Call ForceFluxingSuffix
  ; ... rest of .onInit
FunctionEnd
```

Key change: `InstallDirRegKey` directive REMOVED from script. The
registry-to-`$INSTDIR` logic is now under our explicit control in
.onInit, with the smoke-test guard (L13-fix-2) and the /D= passthrough
both working.

### 2.2 Risk register

- **R1**: removing `InstallDirRegKey` changes the upgrade flow for users
  with non-default install paths. Mitigation: the manual `ReadRegStr`
  logic in .onInit replicates the exact same behavior, so upgrades work the same.
- **R2**: `/D=` may still be ignored if NSIS version is < 2.46 (unlikely
  - we use 3.x per L09). Mitigation: NSIS 3.x supports `/D=` override.
- **R3**: regression in the GUI flow. Mitigation: the GUI flow uses
  `MUI_PAGE_DIRECTORY` which sets `$INSTDIR` from the user's selection,
  not from the registry. Removing `InstallDirRegKey` does not affect the GUI flow.

## 3. Out of scope

- The uninstall-side `HKLM\Software\Fluxing\Weasel\InstallDir` cleanup
  is spec 012 C1; not addressed here.
- The L13-fix-2 smoke-test path guard (already shipped in 0.18.5.0)
  remains as defense-in-depth even if /D= is fixed.

## 4. Acceptance for this spec

- AGENTS.md \u00a72.5 smoke test recipe works as written (no
  "workaround: delete registry key first" step required).
- US1-A / US1-B / US1-C pass on a clean Windows install.
- 24/24 still PASS in TestDefaultHotkeys (no regression).

## 5. Tracked

- L17 gotcha 3 (lessons-learned.md).
- Commit b98c7d5 introduced the `InstallDirRegKey` directive without
  preserving /D= override.
- Originally discovered while testing commit 053515b (spec 012 cleanup).
