# AGENTS.md — Operating Manual for AI Agents and New Contributors

> **Companion to `.specify/memory/constitution.md`.** That file states the
> "why" (5 principles, 9 hard rules, 8 project rules). This file states the
> "how" — the concrete commands, paths, and traps you need to actually
> deliver a change without breaking the build.

> **All pitfalls below are sourced from real incidents.** Each is annotated
> with the matching `L##` entry in `.specify/memory/lessons-learned.md` so
> you can read the full root-cause writeup. Do not just trust this summary;
> read the L## entry before applying a non-trivial fix.

---

## 1. Project Map (one screen)

| Concern | Location | Notes |
|---|---|---|
| Source root | `F:\soft\00selfmade\rime` (Windows) | Project lives on `F:`, Boost on `F:\b183`. |
| Active branch | `Fluxing` | Brand-fork worktree (P8 in constitution). `master` tracks upstream rime/weasel. |
| Remotes | `kizemo` (this fork), `origin` (rime/weasel) | Push to `kizemo`. Do not push to `origin` directly. |
| Build system | Visual Studio (`weasel.sln`) OR `xmake` (`xbuild.bat`) OR `build.bat` (msbuild wrapper) | CI matrix runs both. Local devs pick one; see §3. |
| Submodule | `librime/` (git submodule) | **Never** edit inside `librime/` without a separate PR. |
| Vendored ext | `thirdparty/librime-lua/` | Tracked in git. Hooked into librime build via `scripts/prepare-librime-lua.bat`. See L10. |
| Constitution | `.specify/memory/constitution.md` | Binding principles. Read first, follow always. |
| Specs | `.specify/specs/NNN-*/` (11 active specs as of 0.18.2.0) | spec/plan/tasks per spec-init. |
| Lessons | `.specify/memory/lessons-learned.md` (L01–L10) | Each L## is a post-mortem. Read before any non-trivial fix. |
| Installer script | `output/install.nsi` | Tracked. **NSIS BOM required** (L09). |
| User-visible strings | `output/data/*.yaml`, `*.rc` | All user-facing strings stay in Simplified Chinese (P2/P5). |
| Test code | `test/TestDefaultHotkeys/`, `test/TestResponseParser/`, `test/TestWeaselIPC/` | Built via `weasel.sln` (msbuild) only. xmake path is build-only. |
| Local-only | `env.bat`, `weasel.props`, `msbuild*.log`, `*.user` | Listed in `.gitignore`. Never commit. |

---

## 2. Build & Test Commands (copy-paste safe)

### 2.1 Prerequisite environment

Required on every Windows dev machine:
- **Visual Studio 2022 Build Tools** (C++ ATL + MFC, `v143` toolset). CI uses
  `windows-2022` image; local devs use whatever matches.
- **Boost 1.84+** at `F:\b183` (CI uses `${{ github.workspace }}\deps\boost_1_84_0`).
  The `BOOST_ROOT` env var in `env.bat` points to this.
- **xmake 2.9.4+** at `F:\soft\08tools\xmake\xmake.exe`. Already on `DEVTOOLS_PATH`.
- **NSIS 3.x** at `C:\Program Files (x86)\NSIS\`. Used by `xbuild.bat installer`.
- **Git for Windows** (for `bash`, used by `clang-format.sh`).
- **Python 3.10+** (for `get-rime.ps1` / `plum/`-style data tasks).
- Optional: `clang-format 18+` (CI uses 18; local devs may use 17+).

### 2.2 Build variants (which to run when)

Three equivalent entry points; pick **one** for the iterative inner loop and
keep the other two for CI parity:

| Command | Tool | Use it for | Time cost |
|---|---|---|---|
| `xbuild.bat weasel installer` | xmake | Inner loop on Windows; fastest incremental | 30-90 s |
| `build.bat weasel installer` | msbuild + cmake | When you must touch `librime/` or vendored rime API | 5-15 min |
| `build.bat all` | msbuild end-to-end | First-time setup, monthly hygiene, before tagging | 30-60 min |

The `installer` suffix triggers `makensis` against `output/install.nsi`.
Without it, you get just the .exe and .dll under `output/` and `output\Win32\`.

### 2.3 Tests

```batch
:: Build + run unit tests (20/20 PASS is the bar, see commit 6277b59).
msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32
test\TestDefaultHotkeys\Release\TestDefaultHotkeys.exe
test\TestResponseParser\Release\TestResponseParser.exe
test\TestWeaselIPC\Release\TestWeaselIPC.exe
```

Test executables land in `test\<Name>\Release\`. The three test projects
**build only under `Release|Win32`** (x86). x64 build matrix is intentionally
not exercised by tests; installable binary is 32-bit and works on x64 OS via
WoW64. (See L10 §3 for the librime Win32-only constraint.)

### 2.4 Lint / format

```bash
# Run clang-format in-place on changed files (matches CI):
./clang-format.sh -i
```

CI also runs this on every PR; the build job will not start if lint fails.
If you are on Windows without bash, use the PowerShell equivalent:
```powershell
.\clang-format.ps1 -i
```

### 2.5 End-to-end silent-install smoke test

```powershell
# Always run this after a release build, before tagging.
$dst = "C:\TEMP\fluxing-test"
Remove-Item -Recurse -Force $dst -ErrorAction SilentlyContinue
$proc = Start-Process -FilePath ".\release\fluxing-<ver>-installer.exe" `
    -ArgumentList @("/S","/D=$dst\Fluxing") -Wait -PassThru
# Required invariants (each verified in 0.18.1.0 / 0.18.2.0):
#   exit code == 0
#   HKLM\SOFTWARE\WOW6432Node\Fluxing\Weasel\InstallDir = "$dst\Fluxing"
#   HKCU\Software\Fluxing\Weasel\RimeUserDir   = "$dst\Fluxing\fluxing\user1\fluxing"
#   "$dst\Fluxing\weasel\rime.dll"             exists, ~3.0 MB (lua-linked)
#   "$dst\Fluxing\weasel\data\build\rime_ice.table.bin" exists, ~60 MB
#   Do NOT pass /userdir=<x> in the args. NSIS concatenates unknown CLI
#   flags into $INSTDIR and corrupts the user-data registry key. (L10 §5)
```

---

## 3. Branch & Release Workflow

### 3.1 Branches

| Branch | Purpose | Push target |
|---|---|---|
| `master` | Track upstream `rime/weasel` | Do not push directly. |
| `Fluxing` | Brand-fork work (P8 waiver scope) | `kizemo/Fluxing` |

When a change is **upstream-relevant** (P1–P7 scopes), rebase onto `master`
and PR back. When it is **Fluxing-specific** (P8 scope `fluxing`), commit
on `Fluxing` and push.

### 3.2 Conventional Commits (P4)

Format: `type(scope): subject` — scopes:
`WeaselTSF`, `WeaselServer`, `WeaselDeployer`, `WeaselSetup`, `WeaselUI`,
`WeaselIPC`, `RimeWithWeasel`, `librime`, `installer`, `docs`, `ci`,
`fluxing` (last one is brand-fork only, see P8).

Examples (from current history):
- `fix(WeaselTSF): handle empty RimeUserDir`
- `fix(installer): v0.18.2.0 - integrate librime-lua for rime_ice lua_processor`
- `docs(memory): lessons-learned L10 - librime-lua integration`

### 3.3 Version bump procedure

A release is the only time the following files are touched **together**:
1. `weasel.props` — `VERSION_PATCH` + `PRODUCT_VERSION` + `FILE_VERSION`.
2. `env.bat` — `FLUXING_VERSION` + `WEASEL_BUILD` + `RELEASE_BUILD=1`.
   Without `RELEASE_BUILD=1`, `build.bat` falls into the git-hash-suffix
   branch and produces a wrong-named installer. (L10 §6)
3. `CHANGELOG.md` — entry under the new version heading.
4. `release/fluxing-<FLUXING_VERSION>.<WEASEL_BUILD>-installer.exe` —
   copy from `output/archives/` after a successful `xbuild.bat installer`.

`weasel.props` and `env.bat` are in `.gitignore` — **the version bump
files do NOT get committed**. The release commit records the version
only in `CHANGELOG.md` and the new installer binary in `release/`.

### 3.4 Tagging & release (CI-driven)

CI only releases when **both** are true (see `.github/workflows/ci.yml`):
- `github.repository == 'rime/weasel'` (Fluxing brand-fork has no CI release)
- `startsWith(github.ref, 'refs/tags/')`

For local Fluxing releases, push `Fluxing` to `kizemo/Fluxing` and announce
manually; do not create upstream tags from this worktree.

---

## 4. Dangerous Zones (do not poke without reading L## first)

### 4.1 NSIS script (`output/install.nsi`)

⚠️ **L09 trap**: This file MUST start with the UTF-8 BOM (`EF BB BF`).
Without it, NSIS `Unicode true` decodes as ANSI and crashes on the first
non-ASCII byte (`Bad text encoding line 69`). Verified before every
release with:

```powershell
# First 3 bytes must be 0xEF 0xBB 0xBF.
$head = [System.IO.File]::ReadAllBytes("output\install.nsi")[0..2]
($head -join ",") -eq "239,187,191"
```

Also:
- 100% CRLF line endings. Lone `\r` (Mac classic) is tolerated but
  inconsistent; lone `\n` is wrong.
- `OutFile` must be `archives\fluxing-${FLUXING_VERSION}.${WEASEL_BUILD}-installer.exe`
  — never hard-code a version.
- `MUI_ICON ..\resource\weasel.ico` is **cwd-relative**, not
  install.nsi-relative. `xbuild.bat` cds to `output/` before invoking
  makensis to make this work. (L10 §4)

### 4.2 `env.bat` (local-only, gitignored)

Contains: `BOOST_ROOT`, `DEVTOOLS_PATH`, `ARCH`, and (for releases)
`FLUXING_VERSION` / `RELEASE_BUILD`. If you change it, run
`build.bat installer` once to confirm `OutFile` produces the right name.
A wrong `RELEASE_BUILD` value causes the installer to overwrite the
previous release with a different version string. (L10 §6)

### 4.3 `weasel.props` (local-only, gitignored)

Mirrors version fields for the `weasel.sln` msbuild path. Must agree
with `env.bat` or xmake and msbuild will produce inconsistent .exe
metadata. Verified with: `Get-Item output\Win32\WeaselServer.exe | ?{$_.VersionInfo.FileVersion}`.

### 4.4 `librime/` (git submodule)

⚠️ **L10 trap**: librime is built with cmake `-AWin32` (32-bit) by
default. The import `rime.lib` in `lib64/` is therefore 32-bit, and
xmake x64 build of WeaselServer.exe will fail with `LNK2001 rime_get_api`
(machine-type mismatch). `xbuild.bat` deliberately skips the x64 build
step — **do not "fix" it by re-enabling x64**, that requires
librime-to-64-bit which is a separate task.

Also:
- If `cmake install(TARGETS rime)` shows `-- Up-to-date: rime.lib` while
  `rime.dll` is fresh, the import .lib was NOT regenerated. `build.bat`
  has a manual `copy /Y build_%1\src\Release\rime.lib dist_%1\lib\rime.lib`
  after `stash_build push` to force the right .lib into place.
- The `librime-lua` plugin lives at `librime/plugins/lua/`; do not
  commit changes there. The plugin source is **vendored** at
  `thirdparty/librime-lua/` and copied in by
  `scripts/prepare-librime-lua.bat` before each librime build.

### 4.5 `thirdparty/` (vendored, tracked)

L10 added `thirdparty/librime-lua/`. When you vendor a new RIME
extension, follow the same pattern: drop it in `thirdparty/<name>/`,
add a `scripts/prepare-<name>.bat` that copies into `librime/plugins/<name>/`,
and hook the call into `build.bat` in the rime section. Never edit
`librime/plugins/<name>/` directly — it gets blown away on the next
`build.bat rime` run.

### 4.6 Secrets and tokens

- `github_token.txt` (if it exists at repo root) — **NEVER commit**.
  `.gitignore` covers `release/*token*` and `release/*secret*`, but
  `github_token.txt` at root is **not** covered; double-check with
  `git status` before every push.
- `arm64x_wrapper/*.{dll,lib,exp,o,ime}` are not in `.gitignore` by
  filename pattern but are intentionally not tracked; verify with
  `git ls-files arm64x_wrapper/`.
- `~/.ssh/id_ed25519` (or any SSH key): never paste contents into
  chat, code, or commit messages. If GitHub auth is needed, document
  the SSH config setup in `INSTALL.md` instead of inlining credentials.

### 4.7 `release/` directory

- Committed: `release/fluxing-<X.Y.Z.W>-installer.exe` binaries.
- Never committed: `release/*token*`, `release/*secret*`,
  `release-notes.html`, `RELEASE_CHANGELOG.md` (all in `.gitignore`).
- The 4 historical installers (0.17.5, 0.18.0, 0.18.1, 0.18.2) total
  ~177 MB. Before adding another, confirm the previous one is actually
  promoted to a GitHub Release (the CI workflow does this on tag
  push); if not, remove it to keep `git clone` cheap.

---

## 5. Pre-Commit Checklist (5 steps, run in order)

Adapted from L05. **All five must pass**; paste the output in the chat
response (R6: evidence before assertion).

1. **Byte health** (if the change touches any text file with non-ASCII
   content — `output/install.nsi`, `*.rc`, `*.yaml`, `*.md`, `*.lua`):
   ```powershell
   $f = "path\to\file"
   $b = [System.IO.File]::ReadAllBytes($f)
   # No 0xC0 / 0xC1 overlong UTF-8 starters.
   ($b | Where-Object {$_ -in 0xC0,0xC1}) | Measure-Object | Select-Object -ExpandProperty Count  # must be 0
   # For install.nsi: BOM present.
   $b[0..2] -join "," -eq "239,187,191"  # must be True
   # CRLF count == total-line-endings (no lone CR or lone LF).
   (($b | Where-Object {$_ -eq 0x0D}).Count) -eq (($b | Where-Object {$_ -in 0x0A,0x0D}).Count)
   ```

2. **Unit tests**:
   ```
   test\TestDefaultHotkeys\Release\TestDefaultHotkeys.exe
   ```
   Must report `20/20 PASS` (or your new test count if you added
   tests). A regression in spec 005 hotkeys is the most common cause
   of 0/20.

3. **Build hygiene**:
   - No new `*.log` / `*.log.err` / `*.build.log` in `git status`.
   - No new `release/*token*` / `github_token.txt`.
   - `weasel.props` and `env.bat` show as untracked (not modified).

4. **Format**:
   ```
   ./clang-format.sh -i
   ```
   Or `clang-format.ps1 -i` on Windows. CI will reject unformatted code.

5. **Scope check** (P4): the commit message must use a single scope from
   the allowed set (see §3.2). Multiple scopes in one commit block
   merging.

---

## 6. How to Use This File (and the rest of the project memory)

When you start a session and the user gives a non-trivial task:

1. **Read `.specify/memory/constitution.md` first.** Understand which
   principles (I-V) and which P-rules (P1-P8) apply. The
   `Skills Mapping` appendix tells you which skill to invoke for each
   task shape.

2. **Check the relevant `L##` entry** in
   `.specify/memory/lessons-learned.md` if your task is even
   tangentially related to NSIS, PowerShell byte handling, GitHub
   MCP, xmake, librime, or installer naming. Skipping this step is
   what cost us L08, L09, and L10.

3. **Check `.specify/specs/NNN-*/`** for the matching spec if your
   task falls into an existing spec (000 through 011 as of 0.18.2.0).
   The `spec.md` is tech-agnostic; `plan.md` has the technical
   approach; `tasks.md` is the implementation checklist. The
   Constitution Check in `plan.md` is a hard gate before `implement`.

4. **Use the right skill** (see constitution `Skills Mapping`):
   - Bug repro / root cause: `systematic-debugging` then `verification-before-completion`
   - Spec/plan/tasks creation: `spec-init` → `spec-check`
   - Multi-file refactor: `incremental-implementation`
   - Public API change: `source-driven-development`
   - Any UX string change: `interview-me` first, then `brainstorming`

5. **Run §5 checklist before every commit.** If any step fails, do
   not commit. If you can't make it pass in 3 attempts, escalate
   to the user with a `doubt-driven-development` review.

---

## 7. Anti-Patterns Seen in This Project (do not repeat)

| # | Anti-pattern | Incident |
|---|---|---|
| A1 | Reading/writing Chinese text via PowerShell `Get-Content` / `Out-File` | L01 — GBK pollution, file truncation, GBK-encoded paths in NSIS errors. Use byte-level `[IO.File]::ReadAllBytes` / `WriteAllBytes` or set `[Console]::OutputEncoding = [System.Text.Encoding]::UTF8`. |
| A2 | Patching `output/install.nsi` via PowerShell string APIs (loses BOM, flips line endings) | L09. Always use byte-level replace and re-verify the 3-byte BOM after editing. |
| A3 | Pushing the current `weasel.props` / `env.bat` "because they have the version in them" | They're in `.gitignore` for a reason — they are per-machine. |
| A4 | Trusting `cmake install(TARGETS rime)` to also install the import `.lib` | L10 §2. On Windows MSVC, `.lib` install is not reliable; the post-install manual copy in `build.bat` is mandatory. |
| A5 | Re-enabling `xmake f -a x64` in `xbuild.bat` because "the project is supposed to be 64-bit" | L10 §3. librime is Win32-only; the project is Win32-only; the 32-bit rime.dll works on x64 OS via WoW64. |
| A6 | Passing `/userdir=<path>` in a silent install invocation | L10 §5. NSIS concatenates unknown CLI args into `$INSTDIR` and corrupts the user-data registry key. |
| A7 | Editing `librime/plugins/lua/` directly | L10 §4. It gets wiped on the next `build.bat rime`; edit the source at `thirdparty/librime-lua/` and let `prepare-librime-lua.bat` re-copy. |
| A8 | Trusting `git describe --tags` to give a clean version in `build.bat` | L10 §6. Set `RELEASE_BUILD=1` in `env.bat` for release builds. |
| A9 | Modifying the CI matrix to add an "x64 librime build" without first verifying the entire rime API surface compiles in 64-bit | L10 §3 deferral — this is a separate, multi-week task. |
| A10 | Calling `git add .` from the project root | Will sweep up `weasel.props`, `env.bat`, `*.log`, and the next session's `github_token.txt`. Stage files explicitly by path. |

---

## 8. When in Doubt

- **Build fails** → check `L10` first; if not librime, check `L09` (NSIS).
- **Silent install produces wrong registry keys** → check `L10` §5; the
  cause is almost always an unknown CLI flag being concatenated into
  `$INSTDIR`.
- **PowerShell corrupts a text file** → check `L01` / `L02`; switch to
  byte-level APIs.
- **Tests fail with 0/20** → check `L04` (librime key_binder action
  names) and `L03` (modifier case — `Shift+L` not `shift+l`).
- **GitHub API returns 422 on PATCH** → check `L08` (Chinese in body
  is mangled).

If none of these match, the issue is novel — open a new `L##` entry
in `.specify/memory/lessons-learned.md` once you resolve it, so the
next agent (or future you) doesn't have to rediscover the fix.
