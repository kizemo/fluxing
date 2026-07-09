---
name: shipping-and-launch
description: Ship with confidence. Pre-launch checklist, rollback plan, monitoring. Use when tagging a Fluxing release or preparing the one-shot user-data migration.
---

# Shipping and Launch

> Goal is not just to deploy — it's to deploy safely, with monitoring in place, a rollback plan ready, and a clear understanding of what success looks like. Every launch should be reversible, observable, and incremental.

## When to use

- Deploying a feature for the first time
- Tagging a Fluxing release (e.g., v0.18.34.0)
- One-shot data migration
- Beta / alpha rollout
- Any deployment with risk (all of them)

## Pre-Launch Checklist

For Fluxing releases (per AGENTS.md §3.3 + §4.1 + L## cross-ref):

### Code
- [ ] L46 3-path gate: `xmake` + `msbuild` + `test suite` all PASS
- [ ] L14 byte-verify: all binary archs consistent (x86=0x14C, x64=0x8664)
- [ ] L42 byte-verify: dark-mode bytes present (if F11 affected)
- [ ] L47 byte-verify: source files byte-healthy (no 0xC0/0xC1, BOM only on .nsi/.rc)

### Installer
- [ ] AGENTS.md §2.5 silent-install smoke test (8 invariants)
- [ ] L13 verify: `ForceFluxingSuffix` works in silent mode
- [ ] L17 verify: `/D=` honored when `InstallDirRegKey` is stale
- [ ] L58 verify: default install path = `D:\Program Files\fluxing`
- [ ] NSIS BOM + 100% CRLF (L09)
- [ ] installer binary size < 25 MB (SC-001)

### Version
- [ ] `weasel.props` VERSION_PATCH bumped (gitignored)
- [ ] `env.bat` FLUXING_VERSION + RELEASE_BUILD=1 (gitignored)
- [ ] `CHANGELOG.md` entry under new version heading
- [ ] Tag name: `v0.18.X.0` (lightweight, per AGENTS.md §3.5)

### Release commit
- [ ] Conventional Commits format (P4)
- [ ] Single scope from allowed set
- [ ] Body explains L## / spec references

### Tag
- [ ] Lightweight tag: `git tag v0.18.X.0`
- [ ] Push: `git push kizemo v0.18.X.0`
- [ ] **NEVER** push to `origin` (upstream `rime/weasel`)

### Installer binary
- [ ] `release/fluxing-X.Y.Z-installer.exe` (committed to git)
- [ ] SHA256 logged
- [ ] Size logged (compare to previous version)

## Rollback plan

For every release, have:
1. **Previous installer**: keep `release/fluxing-0.18.(X-1).0-installer.exe` for 6+ months
2. **Previous tag**: `v0.18.(X-1).0` stays in git forever
3. **User-data migration**: if migration was one-shot, keep backup in `installation.yaml::ImportedFrom`
4. **Commit revert**: `git revert v0.18.X.0` is possible

## Monitoring (Fluxing: light)

Fluxing doesn't have a monitoring stack (no Sentry / PagerDuty). "Monitoring" =:
- L## entries for new issues
- User reports on GitHub Issues
- Test suite PASS on user machines (manual smoke test)

## One-shot data migration (P8 waiver)

For the `%AppData%\Rime` → `%LocalAppData%\Fluxing` migration:
- On first run of Fluxing, copy (NOT move) existing data
- Record original path in `HKCU\Software\Fluxing\ImportedFrom`
- Keep original data for 1 release cycle (allows rollback)
- L## entry documenting the migration design

## Post-launch

- [ ] Verify release on a fresh Windows VM
- [ ] Run manual smoke test on Win 10/11
- [ ] Test installer upgrade path (old → new)
- [ ] Test installer downgrade path (new → old)
- [ ] Document any issues in L##

## Anti-patterns

- "It compiled, ship it" (NSIS L09 / L13 / L17 territory)
- "Tag at the end of the day" (lose focus, ship broken)
- "Push and pray" (no monitoring = silent failure)
- "Delete the old installer to save space" (L07 lesson: keep N-1)
- "We can always fix it in the next release" (next release = 2 weeks away for users)
