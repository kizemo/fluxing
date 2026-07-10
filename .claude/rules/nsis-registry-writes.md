# NSIS registry writes — safety-net pattern (L66)

Any `WriteRegStr` / `WriteRegDWORD` / `WriteRegBin` that is meant as a
**safety-net fallback** (e.g. for COM calls that may fail in elevated
context, or for keys regsvr32 may not write) MUST run **unconditionally**,
never inside an `${If} regsvr32-failed` block.

## Why

The spec 066 v0.18.41.0 ship (commit ef6eb024) nested the KnownClasses
+ HKCU\0x00000804 writes inside the `If regsvr32 weasel.dll failed`
block. Intent: "fallback if regsvr32 fails". Reality: on success path,
**none of the writes ran**. Fluxing did not appear in the language
switcher. QuickPanel never showed. (See `.specify/memory/lessons-learned.md`
L66 for full post-mortem.)

## How to apply

When adding registry writes inside `.nsi`, ASK for each one:

> Does the success path of any preceding `ExecWait` (regsvr32, sc.exe,
> net.exe, etc.) also need this write?

- **YES** → write goes **outside** any `${If failed}` block.
- **NO** (write is genuinely only relevant on the failure case) →
  write goes inside `${If failed}` AND must be paired with a
  `DetailPrint` explaining why it's a failure-path-only operation.

## Anti-patterns to flag

- **AP-L66-A**: Safety-net writes nested in `If failed`. ❌
- **AP-L66-B**: Single error block for both diagnostic print AND
  corrective write. Split them.
- **AP-L66-C**: Ship without a post-install `reg query` verification.

## Verification (mandatory after any install.nsi WriteReg* change)

After building, run:

```powershell
reg query "HKLM\SOFTWARE\Microsoft\CTF\KnownClasses"
reg query "HKCU\Software\Microsoft\CTF\Assemblies\0x00000804"
reg query "HKLM\SOFTWARE\Classes\CLSID\{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}\InprocServer32"
```

If any expected key is missing, do NOT ship — fix the NSIS nesting first.

## Triggers

This rule applies to any edit that touches:
- `output/install.nsi`
- `output/uninstall.nsi` (mirror install-side fixes on uninstall)
- Any NSIS file referenced by `!include` directives in the above