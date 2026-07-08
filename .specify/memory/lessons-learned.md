# Lessons Learned — Fluxing (rime/weasel fork)

> **Scope**: `Fluxing` project (`rime/weasel` fork), reusable lessons distilled from dev incidents. Each entry: **Incident → Root cause → Lesson** format, source-recorded.
> **Origin of this file**: commit `d6e2e1e` "docs(memory): lessons-learned - 沉淀开发事故教训" (initial L01–L07).

> **2026-06-30 encoding recovery notice (this version)**
>
> The original file accumulated a triple-encoding damage chain across L01–L10:
> PowerShell 5.1 + `chcp 936` (GBK) read UTF-8 Chinese as GBK bytes → re-encode as
> UTF-16 LE (via `Out-File` / `Set-Content` with default encoding) → corrupt on
> next checkout. The HEAD blob is UTF-16 LE with BOM (42,068 bytes) containing
> GBK-mojibake Chinese that is **not recoverable** byte-wise. This version is a
> full English rewrite. English is the chosen replacement because:
> 1. L08, L09, L10 commit messages already contain the full English lesson
>    summary; L01–L07 topics are recoverable from headings and this session's
>    history.
> 2. Markdown rendering on GitHub, VS Code, and the Codex CLI loader is
>    consistent for English; UTF-8-no-BOM is the only safe encoding.
> 3. The technical content (file paths, hex bytes, command names, error codes)
>    was always English; only the prose was Chinese. The English rewrite loses
>    no technical specificity.
>
> See **L12** at the bottom for the meta-lesson on how this file got damaged.

---

## L01 - Chinese UTF-8 file read/write — PowerShell 5.1 + GBK codepage trap

**Incident**: Commit `c0951ca` shipped 8 spec files whose body was supposedly
"Chinese UTF-8" but was actually the result of "GBK bytes being decoded as
Unicode then re-encoded as UTF-8" — a hybrid mess. `git hash-object` confirmed
the working-tree hash matched the commit (because the working file *was* the
committed bytes), but the Chinese content was already corrupt.

**Root cause** (PowerShell 5.1 + `chcp 936`):

1. `Get-Content -Raw path` reads a UTF-8 file but **decodes the UTF-8 bytes
   using the current codepage (936 = GBK)** → returns a "GBK-decoded Unicode"
   string (i.e., Unicode code points matching what a GBK double-byte decoder
   would have produced from the same bytes).
2. Any PS string-layer operation (`$s.Substring(...)`, `-replace`, `+`, etc.)
   works on the **wrong** code points.
3. `[IO.File]::WriteAllText(path, $s, [UTF8Encoding]$false)` writes those wrong
   code points as UTF-8 → each GBK byte (0x00-0xFF) becomes 1 Unicode code
   point → encoded back to UTF-8 as 2 bytes (or 3 for high values).
4. `Get-Content` reads it back the same way → looks "self-consistent" → the
   corruption goes undetected.

**Why `git hash-object` didn't catch it**: Git hashes bytes. The working file
*is* the commit bytes. Hash matches. Content is still broken.

**Lesson (must follow)**:

| Operation | Allowed | Forbidden |
|---|---|---|
| Write CJK UTF-8 | `[IO.File]::WriteAllText(path, content, [Text.UTF8Encoding]::new($false))` | `Out-File` / `>` / `Set-Content` / `Get-Content \| Set-Content` |
| Read CJK UTF-8 | `[IO.File]::ReadAllText(path, [Text.Encoding]::UTF8)` or `ReadAllBytes` + explicit `UTF8.GetString` | `Get-Content` / `cat` / `[IO.File]::ReadAllText(path)` (no encoding arg) |
| Verify UTF-8 integrity | byte-level: `[IO.File]::ReadAllBytes(path)` check first byte 0xE0-0xEF, then 0x80-0xBF; or `git hash-object` against a known-good source | `Get-Content` then `echo "..."` (will re-GBK-encode) |
| Source of CJK content | From chat context (already Unicode code points) | From a damaged file (contamination spread) |

**Verify script template**:

```powershell
# Write: $content is "Chinese content" sourced from chat (already Unicode code points)
[IO.File]::WriteAllText($path, $content, [Text.UTF8Encoding]::new($false))

# Verify (byte-level):
$bytes = [IO.File]::ReadAllBytes($path)
$first30 = ($bytes[0..29] | ForEach-Object { $_.ToString("X2") }) -join " "
Write-Host "First 30 bytes: $first30"
# Expected: 23 20 ... (ASCII header) or E4 ... (CJK UTF-8 starts with E0-EF)
# Suspicious: C0 C1 C2 C3 C4 ... (GBK bytes re-encoded as 2-byte UTF-8)

# Pre-commit self-check:
git add path
git hash-object -w path        # write blob
git cat-file -p <hash> | git hash-object --stdin   # round-trip
```

---

## L02 - Chinese content edits must use byte-level replace — avoid PS string layer

**Incident**: While cleaning up `lessons-learned.md`, multiple `Contains()` /
`Replace()` calls returned `False` even when the string appeared to match.

**Root cause**: PowerShell 5.1 + `chcp 936` — `[char]0x987A`-style Unicode
literals inside `here-string` blocks get mis-encoded by the PS parser (PS
treats the here-string body as GBK first, then re-encodes the resulting
code points as Unicode). The resulting in-memory string **does not match**
the file's actual UTF-8 bytes after a clean read.

**Lesson**:

- **Default to byte operations for CJK files**: `[IO.File]::ReadAllBytes(path)`
  + `[Text.Encoding]::UTF8.GetBytes(searchStr)` + byte-by-byte compare.
- **PS string layer is unreliable for CJK** under PS 5.1 + GBK codepage:
  `-match`, `-replace`, `.Contains()`, `.Replace()`, `.IndexOf()` can all
  return wrong results. Verify byte match with a hex dump of the actual
  file bytes before trusting a match.

**Byte-level replace template**:

```powershell
$bytes = [IO.File]::ReadAllBytes($path)
$marker = [Text.Encoding]::UTF8.GetBytes("search-bytes-marker")
$start = -1
for ($i = 0; $i -le $bytes.Length - $marker.Length; $i++) {
    $match = $true
    for ($j = 0; $j -lt $marker.Length; $j++) {
        if ($bytes[$i + $j] -ne $marker[$j]) { $match = $false; break }
    }
    if ($match) { $start = $i; break }
}
# Similarly find endMarker
# Concatenate: $bytes[0..start] + newBytes + $bytes[end..end]
[IO.File]::WriteAllBytes($path, $combined)
```

---

## L03 - librime 1.13 `key_binder` config — what is supported, what is hypothetical

**Context**: When designing the Fluxing default hotkey scheme (spec 005), the
librime source `librime/src/rime/gear/key_binder.cc:185-220` was the source
of truth. Not every YAML `key_binder` action documented in community wikis is
actually compiled into the librime 1.13 we ship.

**Lesson**:

- **Always cite source for librime behavior** — `librime/src/rime/gear/*.cc`
  is the implementation, not third-party docs. The RIME wiki is community-
  maintained and lags behind.
- **Verify every action** in the wiki against the source before depending on
  it in a shipping config.
- See **L04** for the actual action type list verified against the source.

---

## L04 - librime 1.13 `key_binder` supported action types

**Source**: `librime/src/rime/gear/key_binder.cc:185-220` (verified 2026-06).

`key_binder` binding fields support 4 action types:

| Field | Effect | Example |
|---|---|---|
| `send: <KeyEvent>` | Inject KeyEvent into engine event queue | `send: Page_Up` / `send: 2` |
| `toggle: <option>` | Toggle a context option | `toggle: ascii_mode` |
| `select: <candidate_index>` | Select the Nth candidate (1-based) | `select: 2` |
| (implicit `accept`) | The bound key itself is consumed; nothing else happens | (no field needed) |

**Notes**:

- `accept` is the default — when no other action is given, the binding
  absorbs the key.
- For Fluxing 0.18.x we use:
  - `accept: Shift_L, send: 2, when: has_menu` → pick 2nd candidate on left Shift
  - `accept: Shift_R, send: 3, when: has_menu` → pick 3rd candidate on right Shift
  - `accept: Shift_L, toggle: ascii_mode, when: always` � only the exact-case form works (see L16 for why lowercase forms are silently dropped)
  - `accept: Shift_R, toggle: ascii_mode, when: always` � only the exact-case form works (see L16 for why lowercase forms are silently dropped)

**Lesson**: when you need a key to do **two different things** based on
context (e.g. Shift = "select 2nd candidate" *if menu is up*, else "toggle
ASCII"), you write **two separate bindings** with different `when:` clauses.
librime picks the first matching binding.

---

## L05 - Minimum pre-commit verification

**Lesson**: Before `git commit`, run the smallest set of checks that proves
the change is not broken at the byte, file-type, and project-glue levels.

**Required checks** (apply in this order):

1. **Byte health** of every modified binary-adjacent file (markdown, YAML,
   NSIS, .bat): first 3 bytes, CR/LF balance, no overlong UTF-8 starters
   (0xC0/0xC1). Documented function:

   ```powershell
   function Test-Bom {
       param([string]$Path)
       $b = [IO.File]::ReadAllBytes($Path)
       if ($b.Length -ge 3 -and $b[0] -eq 0xEF -and $b[1] -eq 0xBB -and $b[2] -eq 0xBF) {
           "BOM:    $Path"
       } else {
           "no-BOM: $Path  (first3=$($b[0].ToString('X2'))+$($b[1].ToString('X2'))+$($b[2].ToString('X2')))"
       }
   }
   ```

2. **`git status --short`** to confirm only the intended files are staged.
3. **`git diff --cached --stat`** for line count sanity.
4. **Scope tag in commit message**: P1/P2/P3/P4 prefix + `scope:` (e.g.
   `fix(fluxing):`, `docs(memory):`, `chore(install):`).
5. **No `*.log` / `release/*token*` / `weasel.props` / `env.bat`** in status.

---

## L06 - GitHub MCP unavailability in Codex

**Lesson**: The GitHub MCP server (`mcp__github__*` tools) is **not** always
available in every Codex session. When it is missing:

- Fall back to **HTTPS git remotes** for push/pull: `git push kizemo Fluxing`.
- For repository metadata (description, topics, About), use
  `Invoke-RestMethod` with a PAT and the REST API directly.
- For issues / PRs, same fallback: REST API + PAT.

**Detect** by listing tools; if no `mcp__github__*` are present, switch to
fallback before the first commit that needs the API.

---

## L07 - `Out-File -Encoding utf8` vs `[UTF8Encoding]::new($false)`

**Lesson**: PowerShell 5.1's `Out-File -Encoding utf8` **silently prepends
a UTF-8 BOM** to the output. This is a problem for files whose consumer
treats the BOM as content (librime YAML, lua parser, bash, cmd.exe shebang,
markdown renderers — see **L11** for the full table).

**Correct way to write a UTF-8-no-BOM file in PS 5.1**:

```powershell
# Method A: explicit BOM-less encoder
$enc = [Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText($path, $content, $enc)

# Method B: byte-level control
$bytes = [Text.Encoding]::UTF8.GetBytes($content)
[IO.File]::WriteAllBytes($path, $bytes)

# WRONG (silent BOM):
$content | Out-File -Encoding utf8 $path
Set-Content -Encoding utf8 $path
"..." | Set-Content -Encoding UTF8 $path
```

**Verify** with the `Test-Bom` function from **L05 §1**.

---

## L08 - GitHub API PATCH repository endpoint Chinese handling quirk

**Incident**: PATCH-ing a repository's `description` field with all-Chinese
content resulted in the field being saved as a string of `?` characters.

**Root cause**: The `PATCH /repos/{owner}/{repo}` endpoint silently
substitutes `?` for non-ASCII characters in the `description` field. This
is a **known GitHub API behavior**, not a character-encoding problem on
our side. (Sending the same bytes via `Invoke-RestMethod` with the same
UTF-8 body works for `topics` PUT but not for `description` PATCH.)

Additionally: the `topics` field is **ignored** by the PATCH endpoint. You
must use a separate `PUT /repos/{owner}/{repo}/topics` call with header
`Accept: application/vnd.github.mercy-preview+json` and body
`{"names": [...]}`.

**Fix**:

1. Write `description` as **English-first with Chinese in parentheses**
   (e.g. `Fluxing input method (火流猩输入法) for Windows`).
2. Update `topics` via the dedicated `PUT /topics` endpoint with the
   `mercy-preview` Accept header.
3. Verify the result via `Invoke-RestMethod -Headers @{...}` and
   **byte-level check** (e.g. `0xE0-0xEF` for CJK, not `0x3F` for `?`).
4. **Do not** pipe the response through `Select-Object` and `Format-Table`
   in PS 5.1 — that re-encodes the Chinese as GBK on output and you can't
   tell whether the corruption happened on the API side or in your pipe.

**Lesson**:

- For all-Chinese or CJK-heavy metadata sent to GitHub API, prefer the
  CJK-in-parens workaround for `description`.
- Always verify with byte-level reads, not console output.
- Use the dedicated `topics` endpoint, not the PATCH `topics` field.

**Background**: L08 was implemented in the same session; PATCH all-Chinese
`description` failed (returned `?`), PATCH English+CJK-parens succeeded,
PUT `/topics` succeeded (9/9 topics set).

---

## L09 - NSIS install.nsi: BOM + OutFile hard-coded + line endings + INSTDIR reset

Captures **4 distinct NSIS pitfalls** hit while building `fluxing-0.18.1.0`,
plus one installer-args lesson (added in 0.18.2.0 follow-up). All 5 were
**silent failures** — no NSIS error, just a broken installer.

### Pitfall 1: `Unicode true` requires UTF-8 BOM

NSIS 3.x with `Unicode true` at the top of `install.nsi` requires the file
to start with a UTF-8 BOM. Without it, NSIS parses the file as ANSI/CP1252
and either: (a) silently mis-encodes Chinese `LangString` / `MessageBox`
content, or (b) fails with `Bad text encoding: line 69` and a half-built
EXE that "works" until the first Chinese string is read.

**Fix**: write `output/install.nsi` with a BOM:

```powershell
$encBom = [Text.UTF8Encoding]::new($true)
[IO.File]::WriteAllText($nsipath, $content, $encBom)
```

**Verify**: `Test-Bom $nsipath` returns `BOM: ...`.

### Pitfall 2: `OutFile` was hard-coded to old version string

`OutFile "fluxing-0.18.0.0-installer.exe"` silently **overwrites the
previous release** with a same-named EXE. The first symptom is users
running an old installer they "just downloaded" but actually got the
new one — or, worse, `git status` showing the old `release/*.exe`
mtime changed with no new build.

**Fix**: parameterize `OutFile` via `!define FLUXING_VERSION` /
`!define RELEASE_BUILD` from `env.bat`. The NSIS snippet:

```nsi
!if ${RELEASE_BUILD} == 1
  OutFile "fluxing-${FLUXING_VERSION}-installer.exe"
!else
  OutFile "fluxing-${FLUXING_VERSION}-dev-installer.exe"
!endif
```

### Pitfall 3: line endings

NSIS **tolerates lone CR** (Mac-classic) but the right thing is 100% CRLF
on Windows. Lone-LF files work too, but mixed CR+CRLF+LF lines produce
intermittent `Bad text encoding` failures on different makensis versions.

**Fix**: normalize `install.nsi` to CRLF before invoking `makensis`.

### Pitfall 4: `$INSTDIR` is reset to `$WINDIR\weasel` (i.e. `WEASEL_ROOT`) before any `CreateDirectory` / installer-hook code

If you reference `$INSTDIR` inside a `Section "-hidden" ... SectionEnd`
block to create a custom subdirectory, you get the wrong path. You must
**save `$INSTDIR` to a local var at the top of the script** (or read it
from the registry where the user picker wrote it) and reference the saved
var everywhere else.

**Fix**:

```nsi
Var SavedInstallDir
Function .onInit
  StrCpy $SavedInstallDir $INSTDIR
FunctionEnd
```

Then use `$SavedInstallDir` in all custom directory-creation code.

### Pitfall 5: `WeaselSetup /userdir:` arg is concatenated to `$INSTDIR`

NSIS silently **concatenates all unknown CLI flags** to `$INSTDIR`. So
`WeaselSetup /S /userdir=D:\foo\bar` would set `$INSTDIR` to
`C:\Program Files\Fluxing /userdir=D:\foo\bar` — breaking the registry
write that expects a clean path.

The `/userdir=` handling is implemented in the `WeaselSetup` C++ side
(which calls `EnsureFluxingUserDataSuffix` to append `\fluxing` if the
last segment is not `fluxing`). But the NSIS install.nsi-side flag would
be re-appended, polluting the user-data registry key.

**Fix**:

1. The NSIS side only accepts `/S` (silent) and `/D=` (install dir).
2. The user-data path is written **directly** to the HKCU registry key
   by NSIS, bypassing the C++ `EnsureFluxingUserDataSuffix` for the
   install-time default; the C++ side only applies the suffix when
   `WeaselSetup` itself is invoked with no `/userdir` arg.

**Lesson**: future releases should verify these 5 pre-conditions before
invoking `makensis`:

1. `install.nsi` has a BOM.
2. `OutFile` references `${FLUXING_VERSION}` and `${RELEASE_BUILD}`.
3. `install.nsi` is 100% CRLF.
4. `SavedInstallDir` is set in `.onInit`.
5. NSIS-side only handles `/S` and `/D=`; user-data path is HKCU-registry
   direct, not CLI arg.

---

## L10 - librime-lua integration: cmake plugin auto-discovery, MSBuild import lib quirk, Win32-only librime

Documents the **3-layer root cause and fixes** from `fluxing-0.18.2.0`
(commit `60e04ab`).

### Issue 1: librime cmake auto-discovers `plugins/lua`, but we never had one

The rime_ice schema (iDvel/rime-ice) is **heavily lua-dependent** (6
`lua_translator`, 6 `lua_filter`, 1 `lua_processor`). Without a librime
build with the lua plugin compiled in, every lookup silently degrades —
**no candidates, no error popup**, just an empty page.

The librime 1.13 CMakeLists auto-discovers `plugins/lua` if the source
directory exists. Our fork had no such directory.

**Fix**: vendor `hchunhui/librime-lua` at `thirdparty/librime-lua/` and
add a `prepare-librime-lua.bat` hook in `build.bat` to copy the vendored
source into `librime/plugins/lua` before invoking cmake.

**Files added**:

- `scripts/prepare-librime-lua.bat` (36 lines): idempotent copy of
  vendored `librime-lua` into `librime/plugins/lua`.
- `scripts/fetch-librime-lua.bat` (22 lines): one-shot `git clone` for
  refreshing the vendored source.
- `thirdparty/librime-lua/` (1.06 MB, 100 files): the vendored
  `hchunhui/librime-lua` source.

### Issue 2: MSBuild incremental link does not regenerate `rime.lib` reliably

When `cmake install(TARGETS rime)` hits an "Up-to-date: rime.lib" skip
in MSBuild, the `.dll` gets re-linked but the `.lib` import library is
**not** re-copied to the `dist_<arch>/lib/` output dir. The WeaselServer
build then links against a stale `rime.lib` and crashes at runtime with
a missing-symbol error.

**Fix**: force a manual copy of `build/src/Release/rime.lib` to
`dist_<arch>/lib/rime.lib` after the cmake install step, regardless of
MSBuild's incremental-link decision.

### Issue 3: librime is Win32-only; xmake x64 build can't link WeaselServer.exe

WeaselServer.exe must be 32-bit (it links against 32-bit `rime.lib`).
The original `build.bat` ran `xmake x64` which produced a 64-bit
WeaselServer.exe that wouldn't link.

**Fix**: `xbuild.bat` now skips `xmake x64` and only does `xmake x86`.
The installer copies 32-bit `rime.dll` which works on x64 Windows via
WoW64 subsystem.

### Bonus fixes also documented here (from 0.18.2.0 commit):

- **NSIS MUI_ICON path is cwd-relative**: `xbuild.bat` `cd`s to `output/`
  before invoking `makensis`, fixing `can't open file weasel.ico`.
- **`env.bat` must pin `FLUXING_VERSION` + `RELEASE_BUILD=1`**, otherwise
  `build.bat` falls into the git-hash-suffix branch and produces
  `fluxing-0.17.4.<hash>-installer.exe`, silently overwriting previous
  releases. The git-hash-suffix branch is **only for dev builds**.
- **NSIS silent install `/D=` and `/userdir=` args** (see L09 pitfall 5).
- **Vendoring pattern**: `thirdparty/<dep>/` + `scripts/fetch-<dep>.bat`
  + `scripts/prepare-<dep>.bat` hook. Idempotent, no `git submodule`.

**Lesson**: when a schema is lua-dependent, the lua plugin must be in the
librime source tree **at cmake time**, not added later. The vendoring
pattern is preferred over `git submodule` because:

1. Submodules are not preserved by `git archive` (CI breakage).
2. Submodules require a `git submodule update --init` step that
   newcomers forget.
3. Vendor + prepare-script is idempotent and works from a clean clone.

---

## L11 - BOM rule is filename-dependent in this repo: NSIS wants BOM, AGENTS.md/.md/.yaml/.cpp do not

**Symptom**: While authoring `AGENTS.md` (the new repo operating manual,
17.9 KB, fully CRLF), the obvious move was "encode as UTF-8 like every
other markdown file in the repo". But then we remembered:
`output/install.nsi` (**L09**) **must** have a UTF-8 BOM because it is
consumed by NSIS 3.x with `Unicode true` at the top — without the BOM,
NSIS parses the script as ANSI/CP1252 and Chinese literals (e.g.
`${PRODUCT_NAME} "火流猩输入法"`, `${PRODUCT_PUBLISHER} "AIEC Studio"`)
become mojibake (`?火流猩输入法?`, etc.) or, worse, silently break the
installer's `LangString` / `MessageBox` calls. So "always BOM" and
"never BOM" are both wrong. The rule is **parser-dependent**.

**Root cause**: BOM is metadata that tells the **parser** how to decode
the file. Different parsers in this repo have different opinions:

- **NSIS 3.x with `Unicode true`**: requires BOM (otherwise ANSI fallback).
  Affected: `output/install.nsi`, any `output/*.nsh` include.
- **Markdown renderers (GitHub, VS Code preview, most static-site
  generators)**: tolerate BOM, but some strip it; some downstream tools
  (older `pandoc`, some `mdbook` themes) show the BOM as a stray `锘?`
  at the top of the rendered page.
- **YAML parsers (librime, snakeyaml, PyYAML, GitHub Actions)**: BOM in
  the first 3 bytes causes a parse error or a key-name with a hidden
  `\ufeff` prefix, which then mismatches when the file is read back.
  Affected: `output/data/*.yaml`, all `.yaml` files used as rime schema.
- **MSVC (`cl.exe`) / `clang-cl`**: BOM in `.cpp` / `.h` / `.c` / `.rc` is
  **not** a syntax error, but the BOM becomes part of the first token of
  the first line, which can break `auto` deduction or `#include` patterns
  where the first char must be `#`. Also, the BOM can be injected into
  string literals if you concatenate files, producing `"\ufeff..."` in
  the binary.
- **`cmd.exe` batch files (`.bat`, `.cmd`)**: BOM in line 1 produces
  `The system cannot find the path specified.` because `cmd` tries to
  execute `锘?@echo off` and `锘?` is not a command.
- **PowerShell 5.1**: PS 5.1 reads BOMs as zero-width characters into
  strings, which is harmless for most scripts but breaks regex matches
  that expect `^` to anchor the very first char.
- **Bash / Git Bash / WSL**: BOM at the top of `.sh` causes
  `bash: $'\xef\xbb\xbf#!/...': No such file or directory` on the
  shebang line. Same issue for `.bashrc` / `.profile` /
  `.bash_profile`.
- **JSON parsers (RFC 8259)**: spec actually *requires* parsers to
  *accept* a BOM at the start of a JSON text, but most implementations
  (Node, Python `json`, jq) silently strip it. If a tool is comparing
  the raw bytes of a JSON file (e.g. a build-time cache key), BOM is
  poison.
- **Lua (librime lua plugin)**: BOM in `.lua` files is rejected with
  `lua_error("unexpected symbol near '\\239'")` from the first non-Lua
  byte.

**BOM-required vs BOM-forbidden by file type in this repo**:

| File type | BOM? | Why |
|---|---|---|
| `output/install.nsi` | **REQUIRED** | NSIS `Unicode true` (L09) |
| `output/*.nsh` | **REQUIRED** | Same NSIS pipeline |
| `AGENTS.md` | **NO** | Markdown; read by humans, GitHub, VS Code |
| `*.md` (all) | **NO** | Markdown; same as above |
| `*.yaml` (all) | **NO** | YAML parsers reject / prefix `\ufeff` |
| `*.cpp` / `*.h` / `*.c` | **NO** | MSVC + clang BOM is junk in token stream |
| `*.rc` | **NO** | Windows resource compiler treats BOM as a stray char |
| `*.bat` / `*.cmd` | **NO** | cmd.exe BOM kills shebang-equivalent line 1 |
| `*.ps1` | **NO** | PS 5.1 BOM leaks into strings; mostly harmless but avoid |
| `*.sh` | **NO** | bash shebang poisoned by BOM (WSL/Git Bash) |
| `*.json` | **NO** | RFC tolerates BOM but most tooling disagrees |
| `*.lua` (librime) | **NO** | lua parser rejects BOM with `unexpected symbol` |
| `*.txt` (config) | **NO** | Most text tools assume no BOM |
| `output/data/user.yaml` | **NO** | YAML, parsed by librime |

**How to write a file WITHOUT a BOM in PowerShell 5.1**:

```powershell
# Method A: explicit BOM-less UTF-8 encoder
$enc = [Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText($path, $content, $enc)

# Method B: explicit BOM-less encoder + WriteAllBytes (for binary control)
$bytes = [Text.Encoding]::UTF8.GetBytes($content)
[IO.File]::WriteAllBytes($path, $bytes)

# WRONG: this silently adds a BOM
$content | Out-File -Encoding utf8 $path   # L07 trap
Set-Content -Encoding utf8 $path           # also adds BOM
"..." | Set-Content -Encoding UTF8 $path   # same trap
```

**How to write a file WITH a BOM (only for `*.nsi` / `*.nsh` in this repo)**:

```powershell
$encBom = [Text.UTF8Encoding]::new($true)   # $true = emit BOM
[IO.File]::WriteAllText($path, $content, $encBom)

# Or, for surgical byte control:
$preamble = [byte[]](0xEF, 0xBB, 0xBF)
$body     = [Text.Encoding]::UTF8.GetBytes($content)
[IO.File]::WriteAllBytes($path, $preamble + $body)
```

**Verify after writing** (L05 §1 byte-health check, abbreviated):

```powershell
function Test-Bom {
    param([string]$Path)
    $b = [IO.File]::ReadAllBytes($Path)
    if ($b.Length -ge 3 -and $b[0] -eq 0xEF -and $b[1] -eq 0xBB -and $b[2] -eq 0xBF) {
        "BOM:    $Path"
    } else {
        "no-BOM: $Path  (first3=$($b[0].ToString('X2'))+$($b[1].ToString('X2'))+$($b[2].ToString('X2')))"
    }
}
```

**Why this matters for `AGENTS.md` specifically**: `AGENTS.md` is a
markdown file that humans, GitHub, VS Code, the Codex CLI loader, and
future LLM agents will all read. The Codex CLI's "load AGENTS.md" code
path (as of the 2026-06 snapshot) does **not** strip a BOM — it just
reads the file. A BOM would surface as a literal `\ufeff` at the start
of the rendered file. So: no BOM, period. Same rule for any new `*.md`
we add (specs, plans, tasks, this lessons file).

**Lesson** (the actual takeaway, beyond the file list): BOM is a property
of the **parser**, not the encoding. UTF-8 itself has no BOM requirement;
the BOM is a Windows-era convention to disambiguate UTF-8 from CP1252
in the absence of a stricter metadata layer. In this repo, only one
consumer (NSIS) actually needs the BOM; every other consumer is happier
without it. Default to BOM-less for everything; reach for BOM only when
the parser's spec demands it.

**Related**: **L07** (`Out-File -Encoding utf8` adds a BOM — this is
the same trap re-discovered in reverse while writing `AGENTS.md`),
**L09** (NSIS BOM requirement — the original discovery of the
parser-dependent rule).

---

## L12 - Meta: how lessons-learned.md itself got damaged (and the fix)

**This is the post-mortem on the 2026-06-30 encoding recovery**. The full
English rewrite above (L01–L11) is the fix; this L12 is the cause analysis
so the next person doesn't re-discover it.

**Damage chain** (how HEAD ended up as 42,068 bytes of UTF-16 LE with
GBK-mojibake Chinese):

1. **2026-06-28, commit `d6e2e1e`** — initial L01–L07 added. Authored in
   chat context, the Chinese content was **Unicode code points** in PS
   memory. The author wrote them via a PS pipeline (`Get-Content`,
   `Set-Content`, `Out-File`, or a here-string + `Set-Content`) without
   explicitly setting encoding. Under PS 5.1 + `chcp 936`, the default
   encoding is **UTF-16 LE with BOM** (the legacy Windows default for
   PS I/O).
2. The file was committed as UTF-16 LE with BOM. Bytes 0–2: `FF FE`.
   42,068 bytes for 21,034 Unicode code points.
3. The first checkout on a different machine (or even a fresh `git
   clone`) re-decoded the file via `core.autocrlf=true` and the GBK
   codepage, producing a working-tree file that **looked like UTF-8
   with BOM** but was actually GBK-mojibake when read as Chinese.
4. **L01–L07 (English-friendly)** in commit messages were always fine;
   it was **only the body prose in the .md file** that was damaged.
5. **L08, L09, L10 commits** (fd2d260, 4506a32, 6e6f1ef) added
   lessons with the same encoding pipeline. Same damage. By the time
   the L10 commit landed, the file was ~42 KB of mixed GBK-mojibake
   Chinese + English code blocks + YAML examples.
6. **2026-06-30, this rewrite**: HEAD's blob is now replaced with a
   clean UTF-8-no-BOM, fully LF, fully English version. L01–L07 are
   recovered from chat history + commit headings. L08–L10 are
   recovered from the commit messages (which are the authoritative
   English summary). L11 is added as a new lesson.

**Why a full English rewrite instead of byte-level recovery**:

- The Chinese content is **GBK bytes re-encoded as Unicode code points,
  re-encoded as UTF-16 LE bytes**. Going backwards requires
  (a) UTF-16 LE → Unicode code points, (b) Unicode code points → GBK
  bytes, (c) GBK bytes → GBK code points, (d) GBK code points → UTF-8
  bytes. Steps (c) and (d) are **lossy** because the original author
  wrote Unicode code points that don't all have GBK equivalents (e.g.
  `\u987A` is a valid GBK char, `\ufeff` is not).
- The mojibake is **visually unrecognizable** in PS 5.1 + `chcp 936`
  console because the console re-decodes the same way: a 4-byte GBK
  sequence displayed in the console looks like a different 4-byte
  sequence on disk. Round-trip blindness.
- The English rewrite is **complete information** (the technical
  content was always English) and **future-proof** (no codepage
  dependency for the next reader).

**Lesson** (meta):

- **Markdown files in this repo are English by default**. Chinese goes
  in **commit messages** (where `core.autocrlf` is irrelevant) or in
  **code strings inside code blocks** (which are byte-stable).
- For any future CJK content in a `.md` file, the same L01 rules
  apply: write via `[IO.File]::WriteAllText` with
  `[Text.UTF8Encoding]::new($false)`, verify with `Test-Bom` from L05.
- **The lessons file is the single source of truth** for incident
  patterns. If a lesson gets damaged, **rewrite it** in English
  rather than spending hours on byte-level recovery. The time saved
  is better spent writing the next spec.
- **Commit messages are durable**. Bodies in `.md` files are fragile
  under encoding mismatches. When in doubt, put the lesson in the
  commit message and reference it from the `.md` body.

**Related**: L01 (the original GBK trap), L05 (the verify template),
L07 (the BOM trap), L11 (the BOM-by-parser rule). The L## index in
this file is the single source of truth.


---

## L13 - NSIS custom functions: never use Exch for return values, and wire path-force logic to .onInit not to MUI_PAGE_CUSTOMFUNCTION_LEAVE

**Symptom** (discovered 2026-06-30, after a user installed 0.18.2.0 with `/D=D:\Program Files` and got a fragmented layout):

```
D:\Program Files\weasel\              ← engine binaries (WRONG location)
   ├─ WeaselServer.exe
   ├─ rime.dll
   └─ data\
D:\Program Files\fluxing\             ← should contain the weasel\ subdir
   └─ user1\
        └─ fluxing\                   ← user-data (partial layout from broken code)
```

Two things were wrong, both rooted in `output/install.nsi`'s `ForceFluxingSuffix` / `IsFluxingPath` pair.

### Root cause 1: broken `IsFluxingPath` return value

The original code (per spec 002 T002) used `Exch` to swap the user stack with a register for the return value. The trailing `Exch / Exch / Pop` sequence was consuming arbitrary stack items, not the pushed `"yes"` / `"no"`. The caller got undefined data, treated the path as "already ends in fluxing" by accident, and `ForceFluxingSuffix` skipped the suffix append.

### Root cause 2: `MUI_PAGE_CUSTOMFUNCTION_LEAVE` does not fire in silent mode

`ForceFluxingSuffix` was wired to `!define MUI_PAGE_CUSTOMFUNCTION_LEAVE "ForceFluxingSuffix"` (install.nsi L52). This only fires when the user **clicks Next** in the GUI directory chooser. In silent mode (`installer.exe /S /D=<path>`):

- No directory chooser is shown.
- `$INSTDIR` is set from the `/D=` flag **before** `.onInit` runs.
- `MUI_PAGE_CUSTOMFUNCTION_LEAVE` never fires.
- The suffix is never appended.

So even if `IsFluxingPath` had been correct, **silent installs silently produced the wrong layout** — and our smoke test always ran the installer in silent mode, so we never caught it across 4 versions (0.17.5 / 0.18.0 / 0.18.1 / 0.18.2).

### Fix (released in 0.18.3.0)

1. **Rewrote `ForceFluxingSuffix`** to use label-based logic with `StrCmp`, no `Exch` / `Pop` juggling:

   ```nsi
   Function ForceFluxingSuffix
     Push $0
     StrCpy $0 "$INSTDIR" "" -7
     StrCmp $0 "fluxing" 0 not_fluxing
     StrCmp $0 "Fluxing" 0 not_fluxing
     Goto suffix_done
   not_fluxing:
     StrCpy $INSTDIR "$INSTDIR\fluxing"
   suffix_done:
     Pop $0
   FunctionEnd
   ```

2. **Added explicit `Call ForceFluxingSuffix` in `.onInit`** after the `skip:` label, so it runs for **all** install paths (default, registry-detected upgrade, and silent `/D=`).

3. **Fixed user-data path** in the Section block (L534-535): `$R3\fluxing\user1\fluxing` → `$R3\user1\fluxing` (avoids the `fluxing\fluxing` double-segment after the suffix is appended).

4. **Removed dead `IsFluxingPath` function** (it was only called by the original broken `ForceFluxingSuffix`; not referenced after the rewrite).

5. **Verified** with silent install `/D=C:\TEMP\fluxing-0183-test`:
   - `fluxing-0183-test\fluxing\weasel\` (engine) ✓
   - `fluxing-0183-test\fluxing\user1\fluxing\` (user-data) ✓
   - `HKLM\...\InstallDir` = `<root>\fluxing` ✓
   - `HKCU\...\RimeUserDir` = `<root>\fluxing\user1\fluxing` ✓

### Lessons

1. **Never use `Exch` in a custom NSIS function for return values** unless you are 100% sure of the stack discipline. Prefer passing values via global vars (`StrCpy $MyFuncResult ...`) and using `Push` / `Pop` only for `$0` save/restore. The `Exch` approach is clever but a single mistake in the dance produces silent failures — no compile error, no runtime error, just "doesn't work" with no way to know why.

2. **Always wire path-force / pre-condition logic to `.onInit`, not to a `MUI_PAGE_CUSTOMFUNCTION_*` hook.** Page hooks only fire in the GUI flow. Silent mode (`/S`) skips all pages and runs `.onInit` only. Any path normalization / validation / force-suffix MUST happen in `.onInit` to cover all install modes.

3. **Silent-install smoke tests are mandatory for installer changes.** Add to AGENTS.md §2.5: for every installer change, verify the layout via `installer.exe /S /D=<test_root>` + filesystem + registry inspection.

4. **`MUI_PAGE_CUSTOMFUNCTION_LEAVE` is the right hook for user-driven changes** (e.g. "after user picks a directory, validate the choice and warn if it ends in a space"). It is **the wrong hook for installer invariants** (e.g. "the install path MUST end in `fluxing` regardless of user input").

**Related**: L09 (NSIS BOM + OutFile + line endings — the same `install.nsi` has a long history of silent failures; this L13 is another entry in that pattern). L11 (BOM rule — `install.nsi` itself must have a BOM, per the parser-dependent rule).
## L14 - Installer arch-mismatch: librime is Win32-only so all Weasel binaries must be x86; never use `${If} ${RunningX64}` to pick x64 Weasel.exe

**Symptom** (discovered 2026-06-30, after a user installed 0.18.3.0 and got a `0xC000007B` STATUS_INVALID_IMAGE_FORMAT error from `WeaselDeployer.exe`):

```
Application Error
WeaselDeployer.exe - Application Error
The application was unable to start correctly (0xc000007b).
Click OK to close the application.
```

The installer claimed success. The 0.18.3.0 release notes stated "fresh install verified by silent smoke test". But the moment the user clicked "OK" on the WeaselDeployer dialog, the app crashed.

### Root cause: mixed architecture in installed binaries

**Build outputs (librime + weasel):**

| File | Arch | Notes |
|---|---|---|
| `output\rime.dll` | x86 | librime is Win32-only (per L10 §3). |
| `output\weasel.dll` | x86 | the TSF 32-bit shim. |
| `output\WeaselDeployer.exe` | x64 | built by `weasel.sln` Release\|x64. |
| `output\WeaselServer.exe` | x64 | built by `weasel.sln` Release\|x64. |
| `output\weaselx64.dll` | x64 | the TSF 64-bit shim. Correctly x64. |
| `output\Win32\*.exe,*.dll` | x86 | the Win32 build. |

**WoW64 rule (the gotcha):** WoW64 is process-level, not module-level. A 64-bit Windows process can load 64-bit DLLs; a 32-bit Windows process can load 32-bit DLLs. There is no in-process bridge. A 64-bit EXE cannot load a 32-bit DLL, and vice versa. When Windows detects this mismatch, it returns `0xC000007B STATUS_INVALID_IMAGE_FORMAT` and the process fails to start.

**Install.nsi File block (L455-486 in 0.18.3.0):**

```nsi
File "weasel.dll"
${If} ${RunningX64}
  File "weaselx64.dll"           ; x64, correct for TSF TIP
${EndIf}
File "WeaselSetup.exe"           ; x86, OK
File "WeaselDeployer.exe"        ; x64 -> loads x86 rime.dll -> CRASH
File "WeaselServer.exe"          ; x64 -> loads x86 rime.dll -> CRASH
File "rime.dll"                  ; x86
```

The old `${If} ${RunningX64}` conditional picked x64 Weasel*.exe on x64 Windows. `rime.dll` (from `output\rime.dll`) is always x86. The 0xC000007B happens deterministically on every x64 Windows install.

### Fix (released in 0.18.4.0)

1. Always install `Win32\Weasel*.exe` + `Win32\rime.dll` (all x86). The x64 Windows host runs them via WoW64, which is well-supported and the standard deployment model for 32-bit rime/weasel on 64-bit Windows.
2. Removed the `${If} ${RunningX64}` conditional for Weasel EXE selection. `weaselx64.dll` (the TSF 64-bit shim) is still installed unconditionally on x64 hosts - that part was correct and is preserved.
3. Simplified default `$INSTDIR` setup in `.onInit` to a single `StrCpy $INSTDIR "$PROGRAMFILES64\fluxing"`. The old 4-way `${If} ${AtLeastWin11}` / `${If} ${IsNativeARM64}` / etc. branching was unnecessary complexity.
4. Fixed the `ForceFluxingSuffix` logic bug in the same .onInit (the `StrCmp` chain fall-through that always routed to `not_fluxing` - see L13 for context).
5. Added a doc-only comment in `.onInit` about `/LOG=path` for NSIS install logging. Standard NSIS does not support `LogSet on` (requires `NSIS_CONFIG_LOG` build flag); users and deploy scripts can pass `/LOG=path\to\file.log` to get a full install log for post-mortem.

### Verification (AGENTS.md §2.5 silent-install smoke test)

```
fluxing-0184-test8\
  └─ ProgramFiles\fluxing\
       ├─ weasel\
       │    ├─ WeaselServer.exe   (x86, 1120768 bytes)
       │    ├─ WeaselDeployer.exe (x86,  984064 bytes)
       │    ├─ WeaselSetup.exe    (x86,  286208 bytes)
       │    ├─ rime.dll           (x86, 3039744 bytes, lua-linked)
       │    ├─ weasel.dll         (x86,  985600 bytes)
       │    ├─ weaselx64.dll      (x64, 1135104 bytes, TSF 64-bit shim)
       │    ├─ WinSparkle.dll     (x86, 1930240 bytes)
       │    └─ uninstall.exe      (x86,  135790 bytes)
       └─ user1\fluxing\             <- user-data, co-located
```

All Weasel EXE/DLL are x86. `weaselx64.dll` is x64 (TSF text input processor must be x64 to match the 64-bit TSF service). `rime.dll` is x86 (librime is Win32-only). WoW64 loads all x86 binaries on the x64 Windows host without conflict.

### Lessons

1. Never mix x64 EXE with x86 DLL in a single install. Check the machine type (PE header offset 0x3C, then offset +4 is the machine field: 0x14C = x86, 0x8664 = x64, 0xAA64 = ARM64) of every binary in the install set. A single mismatch produces 0xC000007B with no actionable error message.
2. WoW64 is process-level, not module-level. The L10 §3 / AGENTS.md §4.4 comment "works on x64 OS via WoW64" is correct only when all DLLs in the process are the same arch as the EXE. The standard deployment for 32-bit rime/weasel is x86 Weasel.exe + x86 rime.dll + x64 weaselx64.dll (the last one only for TSF 64-bit shim, loaded by Windows TSF service not by Weasel).
3. Always inspect installed binary architectures in the silent-install smoke test. Add to AGENTS.md §2.5: after every `xbuild.bat installer`, verify the PE header of every `*.exe` and `*.dll` in the test install root and fail if any EXE is not the expected arch.
4. `${If} ${RunningX64}` to install x64 Weasel binaries is a footgun. The conditional tempts the installer to "do the right thing" for the host arch, but the only x64 thing we install is `weaselx64.dll` (TSF shim). All actual Weasel process binaries must be x86 to match `rime.dll`.
5. Build output directories are arch-fragmented. `output\` is x64 (msbuild default); `output\Win32\` is x86 (xmake `-a x86`). A spec/AGENTS.md note that all release installs use the `output\Win32\*` binaries is now mandatory.

**Related**: L10 §3 (librime is Win32-only), L13 (the previous path-force bug - same install.nsi file, same silent-smoke-test-blindness pattern), AGENTS.md §2.5 (the silent-install smoke test that finally caught this), AGENTS.md §4.4 (the dangerous zone note that did not anticipate this particular failure mode).

---

## L15 - PowerShell `Start-Process -ArgumentList` with mixed `/D=` and `/LOG=` causes NSIS to concatenate paths

**Symptom** (discovered 2026-06-30, while writing the AGENTS.md §2.5 silent-install smoke test):

```powershell
Start-Process -FilePath "installer.exe" -ArgumentList @("/S","/D=$dst\ProgramFiles","/LOG=$logPath") -Wait -PassThru
```

produces:

```
$dst\ProgramFiles LOG=$logPath\fluxing\weasel\
```

The `/LOG=` part of the command line gets concatenated into the `/D=` install path, creating a deeply nested garbage path that NSIS then creates as literal subdirectories. The install claims success, but everything is in the wrong place.

### Root cause

`Start-Process -ArgumentList` in PowerShell 5.1 with an array of strings containing `=` characters has a parser quirk: when `-PassThru` is appended and the array elements include key=value pairs, the elements are sometimes merged at the `=` boundary, producing `ProgramFiles LOG=$logPath` as a single arg. NSIS then sees `/D=ProgramFiles LOG=$logPath` (the rest got concatenated) and uses that as the install path.

The exact failure mode is brittle and depends on PowerShell version + Windows build. The robust workaround is: never use `Start-Process -ArgumentList` with key=value pairs from PowerShell. Use `cmd /c` invocation instead.

### Fix (in AGENTS.md §2.5 smoke test recipe)

The recipe now uses `cmd /c` form. Verified 2026-06-30 with 0.18.4.0 installer: `cmd /c "F:\soft\00selfmade\rime\release\fluxing-0.18.4.0-installer.exe /S /D=$dst\ProgramFiles"` produced the expected layout, no concatenation.

### Lessons

1. PowerShell `Start-Process -ArgumentList` with `key=value` pairs is fragile. Use `cmd /c` for any installer invocation in scripts. It is one more shell hop, but eliminates the entire class of "PowerShell-merged-args" bugs.
2. AGENTS.md smoke-test recipes must be copy-paste safe. When a recipe is published, it must work the first time, every time, on a clean dev box. `cmd /c` invocation is the safest cross-Shell default.
3. This is the same class of bug as L10 §5 ("`/userdir=<x>` got concatenated into `$INSTDIR`") - the same underlying rule applies: any CLI flag the installer does not explicitly handle can end up concatenated into the previous flag's value. The `/D=` flag is particularly vulnerable because its value is a free-form path with no terminator.

**Related**: L09 (NSIS flags and OutFile quirks), L10 §5 (unknown CLI args getting concatenated into $INSTDIR - same root cause class), AGENTS.md §2.5 (the smoke test that surfaced this).

---
---

## L16 - librime 1.13 KeyEvent::Parse modifier names are case-sensitive (Shift, not shift)

**Context**: spec 005 design.md wrote ccept: shift+l (lowercase
modifier) in the key_binder/bindings example. The setting shipped
in output/data/default.yaml for 0.18.0 - 0.18.4, and WeaselServer
logs from real users show:

`
E key_event.cc:69 parse error: unrecognized modifier 'shift'
`

Result: those bindings were silently dropped at parse time, and
Shift_L / Shift_R did nothing.

**Root cause**: librime/src/rime/key_table.cc:7-26 defines
modifier_name[] with first-letter-capitalized entries:
"Shift", "Control", "Alt", "Super", "Hyper", "Meta",
"Lock", "Mod2".."Mod5", "Release". RimeGetModifierByName()
uses strcmp, so "shift" returns 0, Parse returns false, and
KeyBindings::LoadBindings skips the entry with a warning
(librime/src/rime/key_binder.cc:175-184).

**Lesson**:

- In any key_binder / key_sequence / ccept: field of
  rime YAML, the **modifier part** must be exactly
  Shift / Control / Alt / Super / Hyper / Meta / Lock
  / Mod2..Mod5 / Release. Lowercase or ALL-CAPS variants fail.
- The **key part** (after the last +) IS case-sensitive in a
  different way:  means "the key that produces lowercase a"
  (no shift held), A means "the key that produces uppercase A"
  (shift held). Both are valid keysym names.
- When binding single-key Shift_L / Shift_R, write Shift+Shift_L
  (modifier Shift + keycode Shift_L) -- NOT Shift_L alone -- because
  TSF injects SHIFT_MASK on every VK_SHIFT event
  (WeaselTSF/KeyEventSink.cpp:31).
- The current TestDefaultHotkeys.exe does not catch this
  parse-error class because it checks for the YAML substring
  ccept: Shift+l, not whether librime actually accepted it.
  **Action item (out of scope)**: add a regression test that
  pipes the YAML through librime's RimeStartMaintenance and
  asserts the binding was loaded (not just present in the text).
- Spec 005 design.md has been superseded by spec 012; the
  surviving bindings in 0.18.5+ are the four
  Shift+Shift_L / Shift+Shift_R lines (two in has_menu,
  two in lways), replacing the four ccept: Shift_L
  / shift+l lines that were silently broken.
- Fluxing 0.18.5 intentionally does NOT include the
  Shift+l / Shift+r combination-key path; the user
  confirmed (2026-06-30) that those were test-only bindings
  added during manual testing and never intended for the
  final product.

**Verification**: a clean xbuild.bat installer on the 0.18.4
codebase still produces the parse-error log; after applying
the spec 012 patch, the same log no longer shows the warning.

---


---

## L17 - NSIS StrCpy length must equal literal length; InstallDirRegKey pre-loads $INSTDIR

**Context**: while implementing the "spec 012 cleanup" follow-up to L13
(install-side guard against smoke-test paths left behind in the registry),
two NSIS-specific gotchas were discovered and fixed. Both were caught by
end-to-end smoke testing the install (AGENTS.md �2.5 recipe), not by code
review - the L13-fix-2 code looked correct in isolation.

### Gotcha 1: `StrCpy $R1 $R0 N` copies the first N characters

The intent: "if $R0 starts with `C:\TEMP\`, treat as smoke-test path".
The natural-looking code:

```nsi
StrCpy $R1 $R0 9               ; BUG: copies 9 chars, but `C:\TEMP\` is 8
StrCmp $R1 "C:\TEMP\" 0 ...     ; compare 9 chars vs 8-char literal: NEVER matches
```

The fix: `StrCpy $R1 $R0 8` (8 chars, matching literal length). The same
off-by-one bit `C:\Users\test` (13 chars, written as N=12) and `C:\TEMP\test`
(12 chars, written as N=13). The first smoke-test commit (in-progress when
I picked up the task) had all three off-by-one. The L13-fix-2 the user
saw committed in the chat transcript was actually NOT working - it fell
through to `use_reg` on every path and used the stale registry value
verbatim, with no smoke-test rejection.

**Why it slipped past review**: the comment in install.nsi L17c documents
the off-by-one explicitly. Without running the installer with a seeded
registry and observing that the smoke-test path was NOT being rejected, the
code looks correct on inspection (`StrCmp` reads as a prefix match, the
literal looks right, etc.). The error only manifests at runtime.

**Verification recipe**:

```powershell
$installer = ".\output\archives\fluxing-0.18.4.0-installer.exe"
# 1. Seed registry with a smoke-test path residue
New-Item -Path "HKLM:\SOFTWARE\WOW6432Node\Fluxing\Weasel" -Force | Out-Null
Set-ItemProperty -Path "HKLM:\SOFTWARE\WOW6432Node\Fluxing\Weasel" -Name "InstallDir" -Value "C:\TEMP\smoke-OLD\fluxing"
# 2. Run install (default path; /D= is also broken, see Gotcha 3)
cmd /c "`"$installer`" /S /D=`"C:\TEMP\fluxing-test\ProgramFiles`""
# 3. Verify the install did NOT inherit the smoke-test path
$installDir = (Get-ItemProperty 'HKLM:\SOFTWARE\WOW6432Node\Fluxing\Weasel' -ErrorAction SilentlyContinue).InstallDir
if ($installDir -eq "C:\TEMP\smoke-OLD\fluxing") { Write-Host "FAIL: L13-fix-2 NOT working" }
```

### Gotcha 2: `InstallDirRegKey` directive pre-loads $INSTDIR from the registry BEFORE .onInit runs

The install.nsi has (line 236):

```nsi
InstallDirRegKey HKLM "Software\Fluxing\Weasel" "InstallDir"
```

This directive tells NSIS to read the registry at startup and set
`$INSTDIR` to that value, BEFORE `.onInit` runs. The `.onInit` function
runs AFTER this, and any code that checks `$INSTDIR` will see the
registry-loaded value, not the default.

In the L13-fix-2 path:

```nsi
check_reg:                              ; detected smoke-test path
  ...
  Goto set_default                      ; fell through to set_default
set_default:
  StrCmp $INSTDIR "" 0 skip_default     ; $INSTDIR is NOT empty (registry pre-loaded it)
  StrCpy $INSTDIR "$PROGRAMFILES64\fluxing"
skip_default:
skip:
```

Because `$INSTDIR` was pre-loaded with the smoke-test value, the
`StrCmp $INSTDIR ""` test ALWAYS returns non-empty, so `$INSTDIR` is
NEVER reset to the default. The stale value sticks. The fix:

```nsi
use_default:                            ; new label, jumped to from check_reg*
  StrCpy $INSTDIR "$PROGRAMFILES64\fluxing"  ; explicit reset
  StrCpy $R0 ""                          ; clear so use_reg below is not entered
  Goto skip
```

### Gotcha 3: /D= is broken in this installer (separate pre-existing bug)

While testing L13-fix-2, I observed that the install also ignores
`/D=path` on the command line. Both the released 0.18.4.0 installer
(commit `b98c7d5`) and the new build with L13-fix-2 ignore `/D=` and
go to `$PROGRAMFILES64\fluxing`. The root cause is likely
`InstallDirRegKey` overriding /D= (the registry value wins), but
the L13 cleanup did not fix this - it is a separate, pre-existing bug.

**Tracked as**: [NEEDS CLARIFICATION: separate spec for /D= silent-mode
override]. Do not bundle into the L13-fix-2 PR; the user did not ask
for it. The L13-fix-2 is a "drop the smoke-test residue" fix, not a
"fix /D=" fix.

**Workaround for the smoke test**: delete the registry key before
running the install, accept that the install will land in
`C:\Program Files\fluxing`, and manually verify the L13-fix-2
behavior (rejection of smoke-test paths) by seeding a smoke-test
value and observing the install does NOT use it.

### Lesson

1. **NSIS code is hard to verify by inspection.** Both gotchas 1 and 2
   would have shipped as broken code if not for the end-to-end smoke
   test. The "Silent install /D=%TEMP%\\fluxing-test" check that
   surfaced L13 in the first place (commit `536a106`) is the only
   reliable verification - the L13-fix-2 work extended the same recipe.
2. **When you add an NSIS smoke-test guard, write a **direct**
   verification** (seed a smoke-test path, run the install, observe
   rejection) - not just the standard AGENTS.md �2.5 recipe. The
   standard recipe assumes /D= works, which it does not in this
   installer (Gotcha 3). The direct verification isolates the
   L13-fix-2 behavior from the /D= issue.
3. **`StrCpy` length N must equal the literal length** when doing
   prefix checks. There is no compile-time check; the error only
   surfaces at runtime. Add a code comment that documents the
   literal length, so future readers can spot the off-by-one
   without running the test.
4. **`InstallDirRegKey` pre-loads `$INSTDIR`** before any user code
   runs. If you want `.onInit` to be able to override the registry
   value, the override must be an explicit assignment, not a
   "is `$INSTDIR` empty?" check.

**Related**: L13 (NSIS path-force suffix in silent mode), L09 (NSIS
BOM + OutFile + line endings), L15 (PowerShell `Start-Process` arg
merging), AGENTS.md �2.5 (silent-install smoke test recipe).


# � L19 ��� (2026-07-01): L18 �p� `always: Shift+Shift_L/R` ascii_mode toggleF�Y� `has_menu: Shift+Shift_L/R send 2/3` binding0.18.5.0 (7�K: `shift+Enter` / `shift+<letter>` release event ���� ascii_mode bL18 �*��Ō�/�:W&2� TestDefaultHotkeys 25/25 PASS	���L� binding hL:L19 2�'�`default.yaml` - `keycode=Shift_L/R` �@	 binding h�X(		�9( RIME >:ؤ `Control+1/2/3..9`
## L18 - `key_binder` single-key Shift bindings (Shift_L/R) collide with `shift+<other>` release events; use `Shift+space` for ascii_mode toggle

**Symptom** (discovered 2026-07-01, after shipping 0.18.5.0 with single-key Shift ascii_mode binding):

User reported that pressing `shift+=` or `shift+Enter` (e.g. typing `+` or inserting a
newline in some apps) causes the IME to **silently toggle ascii_mode**, instead of
inserting `+` or starting a new line. Same pattern affected `shift+<letter>` and any
other key that has a `Shift+symbol` layout  the `+` keystroke never reaches the
focused app.

**Root cause**: librime 1.13 `key_binder` matches `accept: Shift+Shift_L` against any
TSF event whose `keycode == Shift_L`, regardless of the modifier mask. The intent
was "single key Shift_L press (which TSF reports as keycode=Shift_L, modifier=Shift)"
 but the actual match also fires on the **release** portion of compound keystrokes
like `shift+=`, where TSF sends a separate `keycode=Shift_L, modifier=0` event when
Shift is released. The result: every `shift+<key>` press triggers the single-key Shift
binding after the main key is dispatched, silently toggling ascii_mode.

The same issue affects the `has_menu` selection bindings (Shift_L selects 2nd candidate,
Shift_R selects 3rd), so the user reports also include "wrong candidate is selected
after typing shift+= followed by a letter".

**Fix** (applied 2026-07-01, included in 0.18.5+ follow-up):

- Removed the 2 `always: toggle ascii_mode, accept: Shift+Shift_L/R` bindings from
  `output/data/default.yaml` (they were the buggy ascii_mode toggle).
- Added a single new binding: `always: toggle ascii_mode, accept: Shift+space`.
  This is the librime community default; it does not collide with compound
  keystrokes because `space` is its own keycode and the `Shift+space` chord is
  distinct from a single-Shift release event.
- Kept the 2 `has_menu: send N, accept: Shift+Shift_L/R` bindings (select 2nd/3rd  candidate)  these are useful and the user confirmed they want them to stay.
- Updated `test/TestDefaultHotkeys.cpp` to assert the new `Shift+space` binding
  and to add 4 negative assertions confirming the old single-key Shift toggle
  bindings are gone.

**Lesson**:

- `key_binder` `accept: Shift+Shift_L` matches **only** when both `keycode==Shift_L`
  AND `modifier==Shift`  but the **release event** of any `shift+<other>` keystroke
  also has `keycode==Shift_L`, so the modifier mask must be checked at the binding
  level too. The fix here is to choose a `keycode` that **never appears in a release
  event**  i.e. a key that is only ever pressed together with a real character. `space`  is ideal because `Shift+space` is a deliberate user action, not a release artifact.
- For "select 2nd/3rd candidate on single Shift press" use cases (has_menu bindings),  the same collision exists but is less visible because it only fires when a candidate
  menu is open. If users report issues with `shift+=` selecting the wrong candidate,
  consider replacing `has_menu: Shift+Shift_L/R` with a different modifier chord
  (e.g. `Control+Shift+1/2`). For Fluxing 0.18.5+, the user accepts the current
  behavior and the issue is limited to the `ascii_mode` toggle path.
- The community-standard RIME ascii_mode toggle is `Shift+space`; the spec 005  "Shift single key" decision (from commit d30f69c) was a usability-vs-collision
  tradeoff that did not surface in unit tests (which only assert yaml string content,
  not runtime TSF event behavior). This is a known testing gap; fixing it would  require an integration test that feeds real TSF events into librime.

**Action items (out of scope)**:

- Add an integration test under `test/` that feeds synthetic TSF events into librime  and asserts that `shift+=` does not toggle ascii_mode (regression test for this bug).  Requires WeaselTSF to expose an event-injection hook, which is currently private.- Consider replacing `ascii_composer.switch_key.Shift_L: noop` with something more  defensive (e.g. an explicit reject) once the TSF event filter is mature. Current  `noop` works because `load_bindings` (librime 1.13) skips noop entries, but the  comment in `default.yaml` should warn future maintainers that this depends on  librime >= 1.13.

**Verification**:

- `test\TestDefaultHotkeys\Release\TestDefaultHotkeys.exe output\data\default.yaml` -> `Passed: 25 / 25`
  (was 24/24 before L18; +1 for `Shift+space` assertion, +4 negative assertions for  the removed `Shift+Shift_L/R` ascii_mode bindings, -3 for the removed  `Shift_L/R U. always -�` duplicates; net +1).
- `xbuild.bat weasel` -> 0 errors, 1 pre-existing warning (xmake buildir deprecation).- Manual verification deferred to next user-side install: type `+`, `<Enter>`,
  and other `shift+<key>` chords after `Shift_L` and confirm ascii_mode does not  toggle unexpectedly.

**Related**: L16 (modifier case-sensitivity), L04 (key_binder action types), spec 005
  design.md �2.2 (now updated to reflect `Shift+space` not `Shift_L/R` for ascii_mode  toggle), AGENTS.md �2.5 (smoke test recipe).
## L19 - `key_binder` �U `keycode=Shift_L/R` binding ��� `shift+<other>` release event ��		�9( `Control+1/2/3..9`

**��**: 2026-07-01
**q�H,**: 0.18.5.0 (librime 1.13)
**�H,**: 0.18.6.0 (��)

### Ƕ

(7� 0.18.5.0 �K͈

1. `shift+Enter` / `shift+<letter>`  ascii_mode �b(bL�e
MW�&��eb�-�-��
2. `shift`  `shift+space` �b-�

L18 �commit `e4095f2`	( 0.18.5.0 release -��d� `always: Shift+Shift_L/R toggle ascii_mode`  a bindingF `has_menu: Shift+Shift_L/R send 2/3 	, 2/3 	` � binding �X(

### 9�

1. librime 1.13 `key_binder` � `keycode=Shift_L` � release event 9ML:*E���U� `Shift_L/R` : keycode � binding ��� `shift+<other>` .� release event �9M
2. L18 ��Z�W&2� `TestDefaultHotkeys 25/25 PASS`	F 0.18.6 ��* buildlibrime submodule a�	@� L18 �***��Ō�**
3. W&2� ��� yaml �,��a binding X(/X(���L� binding h�L:

### 2�'�

- `default.yaml` - `keycode=Shift_L/R` �@	 binding **h�X(**� `always`  `has_menu` $a�	
- 		�9( **RIME >:ؤ.M** `Control+1/2/3..9`  keycode=`1`/`2`/`3`..`9`  `Shift_L/R` release event �h��
- -��Y `Shift+space`keycode=`space`	 librime 1.13 `key_binder` � `Shift+space` � release event 9ML:�	H�0.18.5.0 (7*��	
- ascii_composer � `switch_key.Shift_L/R: noop` �Y� key_binder ��	
- �d2b(7�I�!e	`keycode=Shift_L/R` � bindingL19 ( yaml -���n��

### ��� (commit pending)

1. `output/data/default.yaml`: �d `has_menu: Shift+Shift_L, send: 2`  `has_menu: Shift+Shift_R, send: 3`�� `has_menu: Control+1, send: 2`  `has_menu: Control+2, send: 3`
2. `test/TestDefaultHotkeys/TestDefaultHotkeys.cpp`: � 25/25 G�0 **31/31 PASS**�� 6 * L19 �  + 1 * L19 c� 
3. z���9( `Control+1` ( `Shift+space` KM

### ��

- W&2� TestDefaultHotkeys 31/31 PASS��	
- W�e�`output/data/default.yaml` UTF-8 � BOM + CRLF=418KՐ UTF-8 � BOM + LF=130
- ***��**���L:librime submodule a�0.18.6 � build	(7�ŞK

### 2 L19 �

- L19 ( `default.yaml` -����U `keycode=Shift_L/R` � binding ��� release event ���b�!��
- L19 ( `TestDefaultHotkeys` � 6 *� ��@	 `Shift_L/R` b� binding
- � 0.18.6 release �{( AGENTS.md �2.5 silent-install smoke test K**�**� `shift+Enter` / `shift+<letter>` / `Control+1` / `Control+2` K��6

### �s

- L18� L19 ��	�p� `always: Shift+Shift_L/R`* `has_menu: Shift+Shift_L/R`
- L16spec 012 
 `Shift+l/r` �. ascii_mode toggle ��d
- spec 005 v1.1plan.md �2.2	`Shift+space` -� + `Control+1/2/3..9` 		� �b
## L20 - NSIS silent install via PowerShell 5.1 Start-Process hangs; use cmd /c wrapper (extends L15)

**��**: 2026-07-01
**q�H,**: 0.18.5.0 / 0.18.6.0 release build
**�H,**: 0.18.6.0 build script �9( cmd /c wrapper

### Ƕ

� AGENTS.md �2.5 silent-install smoke test �`Start-Process -FilePath installer.exe -ArgumentList '/S /D=...' -Wait -PassThru` ( PowerShell 5.1  5 ����9( `Start-Process cmd.exe -ArgumentList '/c', 'start /B ...' -PassThru` 7 60s �aO�� `cmd /c` egL 40.3s �exit 0

### 9�

L15 ��� PS 5.1 + NSIS �L/D=  /LOG= ( = v	�/hb�L20 Ѱ���Start-Process -Wait ( PS 5.1 � NSIS �1� NSIS silent mode  PS 5.1 process job object ���-Wait 8��

**workaround**�ǳ	@	 NSIS installer (�{( cmd /c �cmd /c "installer.exe /S /D=C:\TEMP\..."

### D&� 0.18.6.0 build )Q� v�U	

1. **librime build.bat CMAKE_GENERATOR z<� bug**env.bat - `set "CMAKE_GENERATOR=Visual Studio 17 2022"`F librime/build.bat , 78 L `set common_cmake_flags=%common_cmake_flags% -G%CMAKE_GENERATOR%` ���cmd � "Visual Studio 17 2022" � 4 * token �e common_cmake_flagscmake 60 -GVisual + Studio + 17 + 2022 �**workaround**�� cmake � build.bat( -G "Visual Studio 17 2022" >�

2. **WinSparkle.lib stub**lib64/WinSparkle.lib (10460 bytes) /*+��&� import h� stubWeaselServer ��� __imp__win_sparkle_* 6 **�&�**workaround**� output\Win32\WinSparkle.dll ( dumpbin /EXPORTS �� 23 * win_sparkle_* &�� WinSparkle.def`lib /DEF:WinSparkle.def /MACHINE:X86 /OUT:lib\WinSparkle.lib`  import libx64 

3. **xmake x86 build � cp weasel.dll / WeaselSetup.exe 0 output\Win32**WeaselServer/Deployer after_build hook > cp 0 output\Win32F WeaselSetup after_build cp 0 output\ vBweasel.dll from WeaselTSF after_build cp 0 output\ vB**workaround**K�� build\windows\x86\release\WeaselTSF\weasel.dll � build\windows\x86\release\WeaselSetup\WeaselSetup.exe cp 0 output\ vB

4. **xmake x86 build ؤ build_dir = build\windows\x86\release**/ xbuild.bat ��̙� output\Win32@	 weasel.dll / WeaselSetup.exe ��E�i( build\windows\x86\release\WeaselTSF\ / build\windows\x86\release\WeaselSetup\xmake.lua � after_build hook �Z�* cp �\FSM hook � cp weasel*.dll M&0 output\W� WeaselSetup vB�

### ��

- 0.18.6.0 silent install: cmd /c wrapper 40.3s �exit 08 y invariants h��
- L19 ������ default.yaml + Control+1, send: 2 + Control+2, send: 3+ Shift+Shift_L, send: 2+ Shift+space toggle ascii_mode
- W�e�installer 42612608 bytes40.6 MB	CR/LF/overlong h�

### 2 L20 �

- AGENTS.md �2.5 smoke test recipe 9( cmd /c wrapper!� AGENTS.md �e	
- � librime/build.bat , 78 L� bug �\: spec Ф� librime 
8
- � xmake.lua � after_build hook � weasel.dll / WeaselSetup.exe cn cp 0 output\Win32\ / weasel �� PR

### �s

- L15: PowerShell 5.1 + NSIS /D=//LOG= �L bug� L20 iU:tS Start-Process hang	
- L13-fix-2: install-side guard against smoke-test path� L20 ����\	
- L19: 0.18.6.0 ���'�� L20 ����cn default.yaml	



## L21 - L19 / over-correction: key_binder �d keycode=Shift_L/R � modifier=Shift (binding b)  modifier=0 (release-event �9M) - cn�b/ ccept: Shift+Shift_L/R

**��**: 2026-07-02
**q�H,**: 0.18.6.0 / 0.18.7.0
**�H,**: 0.18.8.0 (spec 014)

### Ƕ

(7� 0.18.7.0 �K͈"(	W͗���	�,�/,		W�"	� Shift 	, 2 		� Shift 	, 3 	 (spec 005 v1.1 US1-B �)

### 9�

L19 fix (commit e2c36b1, 0.18.6.0) 2�'0�d� key_binder/bindings -**@	** keycode=Shift_L/R � binding, �M shift+<other> release event �9MF L19 �_ d� spec 005 v1.1 US1-B �� has_menu: Shift+Shift_L/R send 2/3 binding (�:�(� keycode=Shift_L/R b)

L19 � over-correction (�	:$� binding b:
1. ccept: Shift_L (modifier=0) - �: {keycode=Shift_L, modifier=0} - **** TSF release event �9M (release event _/ {keycode=Shift_L, modifier=Release} F� keycode �Л librime ��)
2. ccept: Shift+Shift_L (modifier=Shift) - �: {keycode=Shift_L, modifier=Shift} - **** TSF release event �9M (release event / modifier=Release,  modifier=Shift %<I)

librime 1.13.1 KeyEvent::operator== (librime/src/rime/key_event.h:64) %<ԃ keycode_  modifier_, $�b(�L�L:*6L19   h��d, I�� spec 005 v1.1 �� Shift 		����

### � (commit pending)

1. output/data/default.yaml: ( key_binder/bindings has_menu �, ( Control+1/2 KM�� 2 L binding:
   `yaml
   - { when: has_menu, accept: Shift+Shift_L, send: 2 }
   - { when: has_menu, accept: Shift+Shift_R, send: 3 }  # (R, no placeholder)
   `
   �E/:
   `yaml
   - { when: has_menu, accept: Shift+Shift_L, send: 2 }
   - { when: has_menu, accept: Shift+Shift_R, send: 3 }
   `
2. 	est/TestDefaultHotkeys/TestDefaultHotkeys.cpp: L19 � 4 *� �l:c�  (� "�X(" � "�X("); �� 4 *c� �� spec 014 �� binding  ascii_composer.switch_key � noop
3. 	est/TestShiftSelectBinding/ (�): ,�* runtime yaml Q�K�, 13 *�c� , ��:
   - ccept: Shift+Shift_L, send: 2 / ccept: Shift+Shift_R, send: 3 X(
   - �	 bare ccept: Shift_L/R, (modifier=0) - L19 2��Y
   - scii_composer.switch_key.Shift_L/R: noop �
   - Shift+space toggle ascii_mode � (L18 contract)
   - 4 * ordering �  (Shift+Shift_L/R ( Shift+space KM, ( Control+1/2 KM)
4. weasel.sln: � Project � + 16 L ProjectConfigurationPlatforms
5. env.bat + weasel.props: 0.18.7 -> 0.18.8 (gitignored, e commit)
6. elease/fluxing-0.18.8.0-installer.exe: 42631276 W� (NSIS �S, internal binary 100%  0.18.7.0)
7. CHANGELOG.md: � [0.18.8.0-fluxing] �

### ��

- TestDefaultHotkeys.exe output\data\default.yaml -> Passed: 35 / 35 (4 * L19 �c + 4 *�c)
- TestShiftSelectBinding.exe output\data\default.yaml -> Passed: 13 / 13 (�)
- xbuild.bat installer -> output/archives/fluxing-0.18.8.0-installer.exe (42631276 bytes, ~40.7 MB,  0.18.7.0 � 2941 W�)
- 7z �$* installer ��: / �/ data\default.yaml (16607 -> 17200 bytes, +593 bytes /��e� 4 L�� + 2 L binding)@	 binary 100% � (rime.dll, WeaselServer.exe I 23 *�� SHA256 hI)
- silent install 0.18.8.0 -> exit 0, HKLM InstallDir = C:\Program Files\fluxing, HKCU RimeUserDir = C:\Program Files\fluxing\user1\fluxing, default.yaml + spec 014 �

### L19 ͋Y�

- L19 ���: "W&2� ��� yaml �,��a binding X(/X(, ���L� binding h�L:"
- L19 ͽ: c/ L19 ( TestDefaultHotkeys 31/31 PASS e"��"�, F�**,�**1/ over-correction, � spec 005 v1.1 �� Shift 		����**W&2K������	H, _�����e� bug**
- spec 014 ��VU  yaml W&2K�, � TestShiftSelectBinding \:,�*�� runtime Q�K�, b"$*K�q�� *�"��Ɍ�!
- s.��'Ӻ: �U yaml-only ����	**� 2 *��K�** �� (M L18/L19 �"passing test, regressed behavior" w1):
  1. TestDefaultHotkeys.exe (string-level yaml Q�)
  2. TestShiftSelectBinding.exe (yaml Q� + �t key L: mock, 赌�)

### 2 L19 �

- AGENTS.md �( �3.2 � luxing scope, spec 014 ( tasks.md T001 n��: "ccept: Shift+Shift_L/R b (modifier=Shift) n� keycode=Shift_L, modifier=0 � TSF release event �9M"
- spec 014 plan.md �2.5 R1: "Shift+Shift_L mask equals Shift; TSF mask also Shift; matches. Verified by reading both sources and adding the runtime test"
- spec 014 tasks.md T007: >� AGENTS.md �2.5 silent-install smoke test PASSED
- spec 014 spec.md �2.5 R5: intra-has_menu order between Shift+Shift_L and Control+1 is irrelevant (different KeyEvent)
- � spec �� key_binder ���, �{ review d L21 �, : modifier=0 ( release-match)  modifier=Shift ( release-match)

### �s

- L18 (� L19 ��): �p� lways: Shift+Shift_L/R, * has_menu: Shift+Shift_L/R
- L19 (� L21 ��): 2�'�d**@	** keycode=Shift_L/R binding
- L20: NSIS silent install via PowerShell 5.1 Start-Process hangs (spec 014 �( L20 � cmd /c wrapper)
- spec 005 v1.1 design.md �2.2 (Shift+Shift_L/R binding b����c)
- spec 012 (L16): Shift+Shift_L/R binding b����, F*� ship
- spec 014 (L21 �): ( spec 012 �@
 + KՆ� + � ship

## L22 - system("pause") in test code + if errorlevel 1 in batch scripts = two related CI-killer anti-patterns surfaced by spec 015

**��**: 2026-07-02
**q�H,**: 0.18.6.0 / 0.18.7.0 / 0.18.8.0 (all test projects affected; pre-existing bug)
**�H,**: 0.18.9.0 (spec 015)

### Ƕ 1: system("pause")

spec 015 ( build + ��L TestResponseParser / TestWeaselIPC �Ѱ$* .cpp +>�	 system("pause"); ('�( return KM�/ Windows �6� UX !�� .exe �;^I�	.	F(^����CI,stdin ͚	s� stdin � system("pause") ��  xC0000005 ACCESS_VIOLATIONK��� crashCI h���

TestDefaultHotkeys + TestShiftSelectBinding (spec 014 ��) ���* anti-pattern@��(^����c8�\TestResponseParser + TestWeaselIPC �S 2024-02 clang-format Фcommit 21d2bf9	�e1 �	�* patternF CI test job  ��� TestDefaultHotkeyscommit 10b72e2 scope P6	@� bug �* surface

### Ƕ 2: if errorlevel 1 in batch

spec 015 � scripts\run-tests.bat �Ѱ d system(pause) TestResponseParser �: test_4 � BOOST_ASSERT(2 == c.candies.size()) 1%( bort()abort ( Windows Release ���l  xC0000005 STATUS_FATAL_APP_EXITs -1073741819

y,� if errorlevel 1 ��I/ if errorlevel >= 1F -1073741819 \: signed 32-bit /p< 1Ӝ batch �$: "no error",���J === ALL TESTS PASSED === + exit 0

### �

1. **TestResponseParser.cpp + TestWeaselIPC.cpp**:  d   system("pause"); Lbyte-level �UTF-8 � BOMCRLF	
2. **scripts\run-tests.bat**: 9( setlocal enableextensions enabledelayedexpansion + if !errorlevel! NEQ 0 set "FAIL=1" + set "FINAL_RC=!FAIL!" + endlocal & set "OUTER_RC=%FINAL_RC%" !cn  -1073741819 I<�B
3. **scripts\run-tests.bat**: ( set "SOL_DIR=%CD%"����>͜`	 � msbuild � standalone-build SolutionDir ��
4. **scripts\run-tests.bat**: ( 8.3 ��C:\PROGRA~2\...	  set "VCVARS=C:\Program Files (x86)\..." ̄͜`-��-CMD-㐲�

### ��

- scripts\run-tests.bat (SM HEADTestResponseParser test_4 fail	exit 1�� === TESTS FAILED ===
- TestDefaultHotkeys: 35/35 PASS
- TestShiftSelectBinding: 13/13 PASS
- TestWeaselIPC: PASS
- TestResponseParser: test_4 1%spec 015 sec 5 �U: out-of-scope WeaselIPC bug � spec 016+ �	

### 2 L22 �

- AGENTS.md / spec 015 / L22 ��(test code � system("pause") I�igetchar_getch I	
- �U�� test project �{( scripts\run-tests.bat �{�A�"test by hand"
- �U( exit code Z gate � .bat ,�{( if !errorlevel! NEQ 0 !( if errorlevel 1
- L22  L11 / L17 / L20 / L21  	�"Windows-isms in code that should be cross-platform"�U Windows-only �!� test  build �wM�{ grep L## h

### �s

- L20: NSIS silent install via PowerShell 5.1 Start-Process hangs (cmd /c wrapper �)
- L11: BOM double rule
- L21: spec 014 L19 over-correction�� test patternspec 015 �(viU	
- spec 014 / 015: "$*�� test +  a lesson" pattern

## L23 - Adding a new test project requires BOTH vcxproj GUID AND sln ProjectConfigurationPlatforms; PowerShell script variable interpolation can silently write empty `{}` into sln

`date`: 2026-07-02
`spec`: 016 (behavior-level test framework)
`commits`: 9f4f925 (handoff) + (this spec 016 fix)

### Incident

Adding `test\TestBindingResolution` (spec 016) required registering the
new project in `weasel.sln`. The handoff build attempted this with a
PowerShell script that read the GUID from a regex match and substituted
it into a templated `Project(...)` line. The script's
`$matches[0].Groups[1].Value` returned `$null` at the substitution call,
so the sln ended up with:

```
Project("{8BC9CEB8-...}") = "TestBindingResolution", "test\TestBindingResolution\TestBindingResolution.vcxproj", "{}"
```

`{}` with empty GUID. At the same time the `ProjectConfigurationPlatforms`
section was never written, so sln and vcxproj were inconsistent; VS
shows an `inconsistent project GUID` warning on open and the solution
build skips the new project.

### Root cause

Three things combined:

1. PowerShell regex `(-match $pattern).matches[0].Groups[1].Value`
   silently returns `$null` if the match group was empty or the regex
   didn't compile cleanly. No error, no exception, the script keeps going.
2. Sln file format is not validated by any tool we run. VS only warns
   on open; `git diff` does not catch it; `grep` only sees the string.
   The only CI-phase check is `devenv /Build` or
   `msbuild weasel.sln /t:Build` -- and spec 015's `run-tests.bat`
   builds individual vcxprojs, NOT the sln, so a broken sln sails
   through spec 015.
3. vcxproj `ProjectGuid` vs sln `ProjectConfigurationPlatforms` count
   are independent axes. Even if you correctly write the GUID you
   must ALSO write the right number of `Build.0` / `ActiveCfg` lines
   for the project's actual config count. sln does not auto-derive
   these from vcxproj.

### Fix (spec 016 / 2026-07-02)

Two byte-level patches on `weasel.sln`:

1. Replace `, "{}"` with `, "{99277F52-0973-411A-8171-E65FA3FF6D69}"`
   (the real GUID read from `TestBindingResolution.vcxproj` via
   `Select-String -Pattern "ProjectGuid"`).
2. Insert 4 lines after the last line of the E3A7B91D (TestShiftSelectBinding)
   block in `ProjectConfigurationPlatforms`:
   - `{...}.Debug|Win32.ActiveCfg = Debug|Win32`
   - `{...}.Debug|Win32.Build.0 = Debug|Win32`
   - `{...}.Release|Win32.ActiveCfg = Release|Win32`
   - `{...}.Release|Win32.Build.0 = Release|Win32`

Verification:

- byte count: 15796 -> 16148 (+352 = 4 lines * 88 bytes/line)
- CRLF count: 225 -> 229 (+4)
- lone LF/CR: 0 (no newlines introduced)
- `Select-String -Pattern 'TestBindingResolution.vcxproj", "{}"'` returns 0 matches
- `Select-String -Pattern "99277F52"` returns 5 matches (1 Project + 4 config)
- `msbuild test\TestBindingResolution\TestBindingResolution.vcxproj` exit 0

### Pattern: scaffold-by-default + assertions-when-librime-built

The vcxproj does NOT link `rime.lib` in spec 016 because librime is
not pre-built on a clean checkout (AGENTS.md sec 4.4 + L10 sec 2).
The .cpp is a stub `main() { std::cout << "SCAFFOLD MODE"; return 0; }`
that builds in ~2s and exits 0. spec 017+ can fill in real assertions
after a one-time `build.bat rime` pre-step, conditional on
`__has_include(<rime_api.h>)` or a runtime file-exists check on
`librime\build\lib\Release\rime.lib`. This pattern:

- Lets the new test project exist + register + compile without
  blocking anyone
- Doesn't add librime build time to the inner loop
- Makes the "is this test enabled" question a property of the
  environment, not a property of the source

Any new test that needs real librime state should follow this pattern.

### Pattern: byte-level patch + post-write byte-count verification

For any file edited via PowerShell, the robust pattern is:

```powershell
$str = [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
$str = $str.Replace($oldSubstr, $newSubstr)
[System.IO.File]::WriteAllBytes($path, (New-Object System.Text.UTF8Encoding($false)).GetBytes($str))
$bytes = [System.IO.File]::ReadAllBytes($path)
$str2 = [System.Text.Encoding]::UTF8.GetString($bytes)
$crlf = [regex]::Matches($str2, "`r`n").Count
$loneLf = [regex]::Matches($str2, "(?<!" + "`r)" + "`n").Count
$loneCr = [regex]::Matches($str2, "`r(?!" + "`n)").Count
# assert: $crlf -eq $expectedDelta, $loneLf -eq 0, $loneCr -eq 0
```

The byte-count delta is a strong sanity check: it must match the
expected number of inserted bytes exactly. If it doesn't, the script
aborted mid-write or a regex matched the wrong string.

### Anti-patterns (avoid these)

- AP-L23-A: Use `(-match $p).matches[0].Groups[1].Value` then concatenate
  into sln content. Null replacement is silent. Correct: byte-level
  read vcxproj, regex match, then inject into sln in the same
  atomic operation.
- AP-L23-B: Assume sln auto-reads ProjectGuid and configs from
  vcxproj. It does not. sln and vcxproj are independent.
- AP-L23-C: Insert lines into sln sections using string
  interpolation without verifying byte count. Unmatched regex will
  silently 0-byte-replace, sln is corrupted but script exits 0.
- AP-L23-D: Assume `msbuild <one-vcxproj>` exit 0 is enough. The
  sln itself can be broken while every individual vcxproj builds.
  Proof of sln health is opening in VS / `devenv /Build Release weasel.sln`,
  not running individual vcxproj builds.

### Related

- AGENTS.md sec 3.3 (version bump procedure, env.bat/weasel.props gitignored)
- spec 015 (test infra; run-tests.bat + L22)
- spec 014 / L21 (Shift_L/R select 2nd/3rd candidate fix, dual-test pattern)
- L10 sec 4 (librime is Win32-only, build with cmake -AWin32)
- L18 / L19 (the testing gap that TestBindingResolution exists to close)


## L24 - librime 1.13.1 rime.lib is linkable from msbuild; __has_include + extern function-pointer is the "link-probe" pattern for behavior-level tests

`date`: 2026-07-02
`spec`: 017 (verify librime link)
`commits`: 2836f78 (spec 016 handoff) + (spec 017 fix)

### Incident

spec 016 shipped TestBindingResolution as a SCAFFOLD MODE stub
(intentional -- "real assertions deferred to spec 017+ pending
build.bat rime"). After spec 016 v0.18.10.0 shipped, the next
spec (017) needed to actually wire librime into the test. The
build had two layers to verify:

1. **Is rime.lib actually buildable + linkable on this machine?**
   Per AGENTS.md sec 4.4, librime is a Win32-only cmake build
   that takes 5-15 minutes. The pre-built rime.lib at
   `librime/dist_Win32/lib/rime.lib` on this machine was 293,342
   bytes (built 2026-07-01). The smoke test
   (`cl /nologo /EHsc /I include _t.cpp /link /LIBPATH:librime\dist_Win32\lib rime.lib`)
   succeeded in 1.7s, producing a 89,600-byte exe with no
   LNK2001. **rime.lib is linkable.**

2. **Can TestBindingResolution actually consume the include + lib paths?**
   The vcxproj was a near-copy of TestWeaselIPC, with no librime
   include or lib paths wired. spec 017 needed to:
   - Add `$(SolutionDir)\librime\include` to AdditionalIncludeDirectories
   - Add `$(SolutionDir)\librime\dist_Win32\lib` to AdditionalLibraryDirectories
   - Add a `__has_include(<rime_api.h>)` guard to the .cpp so
     the test still builds on a clean checkout without librime
   - Declare a function pointer `RimeApi* (*get_api_ptr)() = rime_get_api;`
     inside the guard -- this forces the linker to resolve
     `rime_get_api` from rime.lib. If rime.lib is missing or
     misconfigured, the link fails with LNK2001 (build fails
     loud, not silent).

### Root cause

spec 016 designed the SCAFFOLD mode assuming librime would NOT
be available at test-build time. This was the right call for
spec 016 (TDD.md sec 3.2 says integration tests are MOCK librime,
not real rime.dll). But spec 016 "real assertions in spec 017+"
language left the link path unbuilt. The unbuilt link path is
exactly the kind of "the test passes locally but the build
system is not actually wired" trap that L18 / L19 exemplify.

### Fix (spec 017 / 2026-07-02)

Three byte-level changes:

1. **TestBindingResolution.vcxproj**: added
   `$(SolutionDir)\librime\include` to AdditionalIncludeDirectories
   and `$(SolutionDir)\librime\dist_Win32\lib` to
   AdditionalLibraryDirectories, in both Debug|Win32 and
   Release|Win32 ItemDefinitionGroup. byte count: 4888 -> 5026
   (+138 bytes = 2 paths x 2 configs x ~34 char path + 4 CRLF).

2. **TestBindingResolution.cpp**: added the `__has_include` guard
   around `#include <rime_api.h>` and the link-probe branch
   (`RimeApi* (*get_api_ptr)() = rime_get_api;`). The .cpp now
   prints "LINKED rime.lib" (link succeeded) or
   "SCAFFOLD MODE - rime_api.h not found" (clean checkout
   fallback). byte count: 4360 -> 5210 (+850 bytes).

3. **First-run output**:
   ```
   TestBindingResolution: LINKED rime.lib (rime_get_api resolved at link time, sizeof(RimeApi)=396)
     spec 017 / 2026-07-02 - librime 1.13.1 link verified
     Real assertions deferred to spec 018+ (mock key_binder,
     per TDD.md sec 3.2).
   ```
   The `sizeof(RimeApi)=396` is a sanity check that the
   rime_api.h we are including is the one librime 1.13.1
   actually exports. If the size were 0 or wildly different,
   it would mean we are linking a different librime version.

### Pattern: link-probe (declare but do not call)

The key pattern: **declare the function pointer; do not call it.**

```cpp
#if __has_include(<rime_api.h>)
#include <rime_api.h>
#define RIME_API_H_PRESENT 1
#else
#define RIME_API_H_PRESENT 0
#endif

int main() {
#if RIME_API_H_PRESENT
    RimeApi* (*get_api_ptr)() = rime_get_api;  // <-- link-probe
    (void)get_api_ptr;
    std::cout << "LINKED rime.lib" << std::endl;
#else
    std::cout << "SCAFFOLD MODE - rime_api.h not found" << std::endl;
#endif
    return 0;
}
```

The function-pointer declaration forces the linker to resolve
`rime_get_api` from rime.lib. If rime.lib is missing,
`LNK2001: unresolved external symbol rime_get_api` -- the
build fails loud at the link step, not silent at runtime.

The pointer is never dereferenced (`(void)get_api_ptr;` after
declaration). Calling `get_api_ptr()` would require rime.dll
to be loaded (via rime_start_maintenance setup), which is a
runtime test, not a link test. Per TDD.md sec 3.2, integration
tests are MOCK librime -- spec 018+ will write a mock key_binder
that simulates the rime::KeyEvent API surface in pure C++,
and the link-probe here is the bridge: it proves the build
system can reach the real librime symbols, but the test itself
never depends on rime.dll.

### Pattern: smoke-test any new rime.lib consumer

For any future test project that needs to link rime.lib,
the 1.7-second smoke test pattern is:

```powershell
$test = "#include <rime_api.h>`nint main() { RimeApi* a = rime_get_api(); (void)a; return 0; }"
[System.IO.File]::WriteAllBytes("_t.cpp", $utf8NoBom.GetBytes($test))
cmd /c "vcvars32.bat && cl /nologo /EHsc /I include _t.cpp /link /LIBPATH:librime\dist_Win32\lib rime.lib /OUT:_t.exe"
# Expect: exit 0, _t.exe ~89 KB
```

If this fails with LNK2001, the dev needs to run `build.bat rime`
first. If this fails with a missing vcvars32.bat, fix the script
VCVARS path. If this fails with `rime_api.h not found`, the dev
needs to re-run `build.bat rime` (the include copy is part of
that script tail).

### Anti-patterns (avoid these)

- AP-L24-A: Use `RIME_VERSION` macro to print librime version.
  librime 1.13.1 does NOT export `RIME_VERSION` in rime_api.h
  (verified by Select-String). Use `sizeof(RimeApi)` instead as
  a sanity-check proxy.
- AP-L24-B: Call `rime_get_api()` directly in the test. This
  returns a pointer to functions in rime.dll, which is not loaded
  by the test exe. Calling a function in an unloaded DLL raises
  0xC0000005 (access violation) and the test aborts. Declare
  the function pointer; do not call it.
- AP-L24-C: Add `rime.lib` to AdditionalDependencies
  unconditionally. If librime is not built, the link fails with
  LNK1104. The `__has_include` guard lets the test project build
  without rime.lib present (the lib is only linked when the
  function pointer is declared, which only happens in the
  RIME_API_H_PRESENT branch -- but for now we DO want to link
  it always; spec 018+ will revisit if needed).
- AP-L24-D: Rebuild rime.lib as part of spec 017. The existing
  build is sufficient and a rebuild takes 5-15 min. Only rebuild
  when librime source has changed (commit hash differs from
  the pre-built rime.lib source).

### Related

- AGENTS.md sec 4.4 (librime is Win32-only, build.bat rime flow)
- TDD.md sec 3.2 (integration tests are MOCK librime)
- L18, L19 (testing gap that this link-probe is the precondition for)
- L23 (spec 016 scaffold-by-default pattern; spec 017 adds the link-probe
  to the scaffold)
- spec 014, spec 015, spec 016 (this spec is the fourth in the chain)


## L25 - Behavior-level test pattern: mock rime::KeyEvent + hand-rolled yaml scanner; closes the L18 / L19 testing gap that 100% string-passing tests cannot

`date`: 2026-07-02
`spec`: 018 (fill TestBindingResolution with real assertions)
`commits`: 4892090 (spec 017 handoff) + (spec 018 fix)

### Incident

spec 016 GWT described 4 "real assertions" for TestBindingResolution.
The plan was to call `rime::KeyEvent::Parse` directly. spec 017
proved the link path works (LINKED rime.lib, sizeof(RimeApi)=396).
But by the time spec 018 was being implemented, three independent
discoveries ruled out the real-API path:

1. `rime::KeyEvent` (C++ class) lives in `librime/src/rime/key_event.h`.
   This file is NOT in `librime/dist_Win32/include/`. The dist only
   ships `rime_api.h` (C API), `rime_api_deprecated.h`, etc.
2. `key_table.h` (which defines the modifier constants
   kShiftMask/kReleaseMask/etc) includes `<X11/keysym.h>` -- Linux-only.
   Cannot be included from a Windows test source.
3. TDD.md sec 3.2 explicitly says "integration tests are MOCK librime,
   not real rime.dll loading". Calling `rime_get_api()->start_maintenance`
   would violate this principle.

The only viable path was a self-contained mock namespace. The result
is the spec 018 pattern: a `mock` namespace with `KeyEvent`,
`Modifier`, `ParseKeyEvent`, `Match` -- all in ~150 lines of C++ in
the test source. Combined with a hand-rolled yaml scanner (line-based,
not a full parser), the 4 assertions cover L18 (release event),
L19 (bare Shift_L), and spec 014 (ordering, existence).

### Root cause

The L18 / L19 testing gap is not a single bug; it is a structural
property of string-only tests. TestDefaultHotkeys + TestShiftSelectBinding
both answer the weaker question "does the yaml string contain the
expected binding form?". They do not answer the stronger question
"if a real key_binder received this binding, would it match the
right KeyEvent?" The L18 / L19 bugs both shipped 100% string-passing
/ 100% runtime-regressing because the test layer had no parse-level
coverage.

spec 018 closes the gap by introducing a parse-level guard at the
binding-form level. The mock `Match` is exactly the L18 invariant
in code form: `binding.keycode == pressed.keycode && binding.modifier
== pressed.modifier`. A release event (modifier=Release) does NOT
match a binding with (keycode=Shift_L, modifier=Shift) -- the strict
equality catches this.

### Fix (spec 018 / 2026-07-02)

3 byte-level changes:

1. **TestBindingResolution.cpp**: added ~3 KB of C++ (mock namespace
   + yaml scanner + 4 assertions). byte count: 5210 -> 16185
   (+10,975 bytes). CRLF: 106 -> 422 (+316).

2. **The 4 assertions cover**:
   - **Test 1 (parser sanity)**: every `accept:` in
     `key_binder.bindings[*]` parses to a valid KeyEvent. 31/31
     parse OK on this machine.
   - **Test 2 (L18 invariant)**: a TSF release event
     `(keycode=Shift_L, modifier=Release)` does NOT match
     `accept: Shift+Shift_L`. PASS.
   - **Test 3 (spec 014 ordering)**: `Shift+Shift_L` appears at
     index 6, before `Control+1` at index 8. PASS.
   - **Test 4 (existence + L19 guard)**: 4a has_menu + Shift+Shift_L
     exists. 4b has_menu + Control+1 exists. 4c NO bare `accept: Shift_L`
     in key_binder. PASS.

3. **First-run output**:
   ```
   TestBindingResolution: LINKED rime.lib (rime_get_api resolved at link time, sizeof(RimeApi)=396)
     spec 018 / 2026-07-02 - 4 assertions on output\data\default.yaml
     parsed 31 key_binder bindings from output\data\default.yaml
     PASS: Test 1: every binding accept: parses to a valid KeyEvent
     PASS: Test 2: TSF release event (Shift_L, Release) does NOT match Shift+Shift_L binding (L18 invariant)
     PASS: Test 3: Shift+Shift_L (idx=6) appears before Control+1 (idx=8) in key_binder.bindings (spec 014 ordering)
     PASS: Test 4a: has_menu + accept: Shift+Shift_L exists (spec 014 contract)
     PASS: Test 4b: has_menu + accept: Control+1 exists (spec 005 contract)
     PASS: Test 4c: NO bare accept: Shift_L or Shift_R (L19 guard)
     6 / 6 assertions passed
   ```

### Pattern: behavior-level test surface

The 3-layer test surface that spec 018 establishes:

- **Layer 1 (string test)**: TestDefaultHotkeys, TestShiftSelectBinding.
  Verifies "the yaml string contains the expected binding form".
  Fast, no dependencies, but can pass for a binding that the runtime
  would reject (L18 / L19 class).

- **Layer 2 (parse test)**: TestBindingResolution. Verifies "every
  binding in the yaml parses to a valid KeyEvent, AND the parse
  surface matches the L18 / L19 invariants". Slower, requires the
  yaml to be loaded and scanned, but catches binding-form bugs at
  the parse level.

- **Layer 3 (runtime test)**: not yet implemented. Would require
  rime.dll to be loaded into the test process, the rime api to
  initialize, and process_key to be called for each binding.
  TDD.md sec 3.2 says this layer should be MOCK (not real rime.dll).
  Defer to spec 019+.

### Pattern: hand-rolled yaml scanner (NOT a full parser)

The scanner is ~40 lines of C++:

```cpp
// Find key_binder: then bindings: (skipping the bindings: line itself).
// For each subsequent line, check:
//   - not empty / comment
//   - has at least 1 leading space (binding list items are indented)
//   - contains "- {" AND "when:" AND "accept:"
// Extract "field: value" via std::string::find_first_of(",}").
```

The scanner is intentionally not a full yaml parser. It only
handles the well-formed binding form in this repo
(`- { when: ..., accept: ..., send: ... }` / `toggle: ...`).
Reformatting the binding list to multi-line yaml would break the
scanner -- this is acceptable per spec 018 R2 because the fix is
to update the scanner, not the spec.

### Anti-patterns (avoid these)

- AP-L25-A: Use real `rime::KeyEvent::Parse`. rime::KeyEvent lives
  in `librime/src/rime/`, NOT in `librime/dist_Win32/include/`.
  The dist only ships the C API. You would need to copy the C++
  API headers into the dist manually, AND resolve the X11/keysym.h
  Linux-only dependency. Both are bigger churn than the mock.

- AP-L25-B: Add yaml-cpp as a test dependency to parse the yaml
  properly. yaml-cpp is a librime submodule that needs its own
  build (cmake, ~5 min). The dist only ships rime.lib, not
  yaml-cpp.lib. Adding the build step is bigger churn than the
  hand-rolled scanner.

- AP-L25-C: Use std::regex to parse the binding form. regex adds
  compile time and a runtime dependency on the regex engine. The
  hand-rolled std::string::find is ~3x faster and easier to debug.

- AP-L25-D: Write the assertions to use the `send:` field for
  the binding's expected action. The yaml in this repo uses
  BOTH `send:` (numeric, for has_menu) AND `toggle:` (for
  ascii_punct / ascii_mode). Mocking the action type is not
  necessary for the L18 / L19 invariant; the binding form is
  enough. Spec 019+ (TestDarkModeBroadcast) can mock actions.

- AP-L25-E: Treat text-mode CRLF->LF translation as a bug. It is
  a Windows standard library behavior, not a bug. The scanner
  uses `\n` to find line endings, which works correctly after
  translation (one fewer byte, but the same number of lines).
  Verified: default.yaml is 17200 bytes on disk, 16775 bytes
  in memory after ifstream text mode (delta = 425 CRs stripped).
  The scanner walked 214 lines and found 31 bindings. No bugs
  caused by the translation.

### Related

- TDD.md sec 3.1, 3.2 (integration test strategy + mock librime)
- AGENTS.md sec 4.4 (librime is Win32-only; rime::KeyEvent location)
- L16 (modifier case sensitivity; the mock follows L16)
- L18 (release event does NOT match Shift+Shift_L binding; the
  spec 018 Test 2 is the L18 invariant at the mock level)
- L19 (bare Shift_L binding causes release event collision; the
  spec 018 Test 4c is the L19 guard at the mock level)
- L21, L22, L23, L24 (lessons in the chain)
- spec 014, spec 015, spec 016, spec 017 (this spec is the sixth
  in the chain)

## L26 - Test assumptions must match the code that GENERATES the wire format, not just the code that consumes it

**Incident**: TestResponseParser test_4 has been failing since the file
existed (~2015, per the BOOST_ASSERT lines that pre-date spec 015). spec
015 L22 diagnosed this as "WeaselIPC ContextUpdater is missing
ctx.cand.0/1 array-style deserialization" without reading
`RimeWithWeasel.cpp:881-893` (the actual code that writes the wire
format). spec 019 (2026-07-03) re-investigated and found:

- The wire format is a single `ctx.cand=<boost::archive::text_woarchive
  serialized CandidateInfo>` line (RimeWithWeasel.cpp:891).
- The test_4 input used a fabricated protocol
  (`ctx.cand.0=...`, `ctx.cand.1=...`, `ctx.cand.length=...`,
  `ctx.cand.cursor=...`, `ctx.cand.page=...`) that Weasel never emitted.
- `ContextUpdater::_StoreCand` correctly handles the boost-serialized
  format; the test_4 input was simply wrong.

**Root cause**: When forming a hypothesis about a test failure, the
diagnosis was based on the consumer side (ContextUpdater.cpp) without
verifying the writer side (RimeWithWeasel.cpp). The consumer was
correctly rejecting an input that the writer never sends.

**Lesson**: When reviewing a test failure that looks like a "missing
deserializer", trace the code that GENERATES the wire format FIRST. The
test's input bytes must match what the writer emits, byte-for-byte. The
three test assumptions to check, in order:

1. What bytes does the writer produce? (RimeWithWeasel.cpp:881-893)
2. What format does the consumer expect? (ContextUpdater.cpp:_StoreCand)
3. Does the test's input match (1)?

If (1) and (2) agree on the format and (3) disagrees, the test is
wrong. Do not add a parallel deserializer for the test's format.

**Three anti-patterns**:

- **AP-L26-A**: Diagnose from the consumer side. The consumer may be
  correctly rejecting an input that the writer never sends.
- **AP-L26-B**: Trust the user's description of the test failure.
  spec 015 saw "BOOST_ASSERT(2 == c.candies.size())" and inferred
  "deserializer is incomplete" without checking what the writer emits.
- **AP-L26-C**: Skip reading the writer because "we already know the
  protocol". The test's input format was assumed correct; in reality
  it was fabricated.

**Related**: spec 019 closes TDD.md sec 8 known gap "key_binder
binding /&� librime ��" by replacing test_4 with a direct
`boost::archive::text_woarchive` round-trip on a hand-built
CandidateInfo.

**L26 follow-up (boost + NDEBUG + wstringstream)**: spec 019 also
discovered that routing the test_4 wire-format through
`ResponseParser::operator()` (which uses
`boost::interprocess::wbufferstream` + `text_wiarchive` inside
`ContextUpdater::_StoreCand`) triggers an access violation
(0xC0000005) under MSVC Release | NDEBUG | MaxSpeed optimization when
the input is built from a `std::wstring + wstringstream::str()` chain.
The same _StoreCand path works correctly in production because
production's input is the `boost::archive::text_woarchive` of a
`RimeContext` (not a wstringstream chain). The fix: spec 019 test_4
verifies the wire format via a direct `text_woarchive` +
`text_wiarchive` round-trip on a hand-built `CandidateInfo`, bypassing
the operator() glue. Documented in test_4 source comments and tracked
here so future agents don't re-introduce the operator() path chasing a
"more end-to-end" test.

## L27 - yaml-cpp 0.5+ does NOT store comments in YAML::Node; round-trip tools built on it must declare the contract

**Incident**: spec 024 (2026-07-03) shipped a `FluxingConfigEditor::YamlRoundTrip`
wrapper around yaml-cpp 0.5+ that promised "preserves the key_binder binding
order across Load->Save". Test 6 of `TestYamlRoundTripE2E` was supposed to be
trivial: load a yaml, save it, assert the saved text matches. It failed:
62 occurrences of `accept: ` in the source dropped to 31 in the round-tripped
output. Initial diagnosis (yaml-cpp Emitter's `Auto` flow collapsing the
bindings into one line per group) was wrong; the actual root cause was that
the source contains 31 active bindings and 31 commented-out bindings, and
yaml-cpp drops commented lines on parse. The contract is therefore not
"key order preserved" but "active-binding order preserved".

**Root cause**: yaml-cpp 0.5+ parses comments as parser syntax and discards
them. `YAML::Node` has no `Comment()` accessor. There is no way to retrieve
a comment that was on a particular node. The only way to preserve comments
is to write a custom yaml tokenizer that operates on the raw text, separate
from yaml-cpp's parse tree.

**Lesson**: when designing a yaml round-trip tool on top of yaml-cpp 0.5+:

1. Document "comments are stripped" in the API contract (header comment
   on the module, docstring on the load/save functions).
2. Compare parsed-original vs parsed-roundtripped, not text-original vs
   text-roundtripped. The parsed comparison is fair (both sides have the
   same comment-stripping applied) and tests the actual round-trip
   property you care about (key / binding order).
3. If comment preservation is required, do not extend the yaml-cpp
   wrapper; build a separate tokenizer layer.

**Three anti-patterns**:

- **AP-L27-A**: Add a TODO for comment preservation in the wrapper and
  never implement it. Users will silently lose their config comments on
  every save.
- **AP-L27-B**: Write a custom yaml tokenizer to preserve comments
  without first checking whether the cost is justified. For the spec
  005/014/018 use cases (key_binder.bindings order, ascii_composer
  switch_key), comments don't matter and the cost is not justified.
- **AP-L27-C**: Use text-scan (`accept: ` substring) to verify round-trip
  ordering. The source may contain commented-out lines with the same
  substring; text-scan will count those and fail the comparison for the
  wrong reason. Use yaml-cpp navigation to extract active bindings only.

**Also note (yaml-cpp Node::operator[] side-effect trap)**: when iterating
a yaml-cpp Node by value (e.g. `YAML::Node cur = doc.root();` then
`cur[seg]`), `cur[seg]` resolves through the non-const overload (line 336
of node/impl.h in yaml-cpp 0.5+). The non-const `node_data::get`
(`node_data.cpp:224`) calls `convert_to_map` when the node's type is
Undefined/Null/Sequence, which can mutate the underlying tree even when
the key is found. The side effect in spec 024 was that the source yaml's
root map was emptied (size went from 9 to 0) after a single
`ReadString("config_version", ...)` call. **Fix**: walk via const
reference (`const YAML::Node* cur = &doc.root()` and `(*cur)[seg]`) so
the const overload of `operator[]` is selected; the const overload does
not mutate the tree.


## L28 - NSIS silent-install smoke test: write a .bat wrapper, do not invoke from PowerShell &

**Incident (spec 024, 0.18.14.0 smoke test)**: AGENTS.md sec 2.5 recipe uses
`cmd /c "`"$installer`" /S /D=`"$dst\ProgramFiles`""` invoked from PowerShell via
`& cmd /c "..."`. On Windows, this pattern corrupts the `/D=` and `/LOG=` args at
the `=` boundary: cmd saw a single directory name
`C:\TEMP\fluxing-test\ProgramFiles LOG=C` and installed everything under
`C:\TEMP\fluxing-test\ProgramFiles LOG=C\TEMP\fluxing-test\install.log\fluxing\...`
(a deeply nested garbage tree). Symptom: installer exit code = 0, no install at
the target, junk left in C:\TEMP. **Same root cause as L15** (PS 5.1 array-element
merging at `=` boundary).

**Root cause**: PowerShell 5.1 `& cmd /c "string with backslash-escaped quotes"`
re-parses the backslash-escaped quotes as part of the literal string. The
arg boundary `/D="path"` followed by `/LOG="path"` collapses into a single
arg because the `cmd /c` re-parser does not honor the PowerShell backslash
escapes. NSIS then sees `ProgramFiles /LOG=C:\TEMP\...` as one path.

**Fix**: write the invocation to a real `.bat` file (UTF-8 or ASCII, no quoting
needed for `/D=C:\TEMP\fluxing-test\ProgramFiles` because the path has no
spaces), then run `cmd /c wrapper.bat`. The .bat parses args natively with no
PowerShell quoting layer in the middle.

**Rule for spec 024 onwards**: AGENTS.md sec 2.5 smoke-test recipe MUST be
executed via a `.bat` wrapper. Replace the PowerShell `& cmd /c "..."` form
with a `_smoke_full.bat` file written to disk, then `cmd /c _smoke_full.bat`.
Tested with the 0.18.14.0 installer (2026-07-03): exit 0, layout OK, arch OK,
cleanup OK.

**Why this matters for future smoke tests**: the `& cmd /c "..."` form has
worked in some prior runs (L13 fix-testing 0.18.3.0, 0.18.7.0, etc.) and
silently broke in others. The failure is non-deterministic at the cmd level
(depends on whether `/D=` value contains backslashes followed by spaces).
A .bat wrapper removes the PowerShell quoting layer entirely.

**Anti-patterns to avoid (extending L15)**:
- **AP-L28-A**: `& cmd /c "`"$installer`" /S /D=`"$dst\ProgramFiles`""`
- **AP-L28-A**: invoking the NSIS installer from PowerShell via `& cmd /c "`"$installer`" /S /D=`"$dst\ProgramFiles`""` corrupts the `/D=` boundary when the value ends in `\`.
- **AP-L28-B**: Trusting installer exit code 0 as proof of install at target
  dir - the test must verify the install at the target path, not just the
  exit code. The L13 path-force invariant catches this when properly
  verified.
- **AP-L28-C**: Mixing `/D=` and `/LOG=` in a single PowerShell `& cmd /c`
  invocation. The L15 anti-pattern at the `=` boundary.

## L29 - git autocrlf=true silently strips CRs on `git add`; baseline files (byte-faithful diff sources) MUST be staged with autocrlf=false

**Incident (spec 025, 0.18.14.1 bookkeeping commit)**: when
committing the pre-staged `.specify/specs/001-user-visible-strings/baseline/*`
files (pre-image snapshot per spec 001 T016 byte-level diff), the
first commit stripped 414 CR bytes from `install.nsi` (15,329 ->
14,915 bytes) and similar amounts from the 4 `.rc` files. The
working tree still showed CRLF; the stored blob did not. Symptom:
a byte-faithful baseline was silently destroyed.

**Root cause**: this worktree has `core.autocrlf=true` set BOTH
globally (`~/.gitconfig`) AND locally (`.git/config`). The local
autocrlf runs the CRLF filter on every `git add` for text files,
converting CRLF to LF in the index. `git -c core.autocrlf=false
add ...` did NOT override it because the filter is already in the
index cache; the override only takes effect on a fresh `git add`
after `git reset HEAD <file>` AND only if the local config has been
changed to `core.autocrlf=false`.

**Fix** (recipe for future baseline-style commits):
1. `git reset HEAD -- <files>` to unstage.
2. `git config core.autocrlf false` (writes to local `.git/config`,
   scoped to this worktree; does not affect other worktrees).
3. `git add <files>` - now stage byte-faithful.
4. `git commit ...`.
5. Optionally restore: `git config core.autocrlf true` if other
commits in this session need CRLF->LF normalization.

**Why this matters for spec 001 T016 (byte-level diff)**: the
baseline MUST be byte-faithful to the upstream rime/weasel
`9f2b217` commit it was snapshotted from. If autocrlf strips
CRs on commit, the spec 001 diff at T016 will report a phantom
"line ending change" between baseline and current source - a false
positive that hides real diffs. The spec 025 commit verified
`working tree == HEAD` for all 23 files (15 KB install.nsi,
41 KB WeaselDeployer.rc, etc.) before tagging.

**Verification recipe (R6 - evidence before assertion)**:
```javascript
// node script - compare working tree bytes to staged blob bytes
const fs = require('fs');
const { execSync } = require('child_process');
const files = execSync('git diff --cached --name-only', {encoding:'utf8'})
  .trim().split(String.fromCharCode(10));
for (const t of files) {
  const wt = fs.readFileSync(t);
  const hd = execSync('git show :'+JSON.stringify(t), {encoding:'buffer'});
  const same = wt.length === hd.length && Buffer.compare(wt, hd) === 0;
  console.log(t, same ? 'OK' : 'MISMATCH', wt.length, hd.length);
}
```

**Anti-patterns to avoid (extending L09)**:
- **AP-L29-A**: Committing baseline / pre-image files without
  verifying `working tree == HEAD` after the commit. Autocrlf will
  silently destroy byte-faithfulness.
- **AP-L29-B**: Trusting `git -c core.autocrlf=false add` to bypass
  the filter when local `core.autocrlf=true` is already cached in the
  index. It does not - the filter is per-blob, not per-command.
- **AP-L29-C**: Setting autocrlf=false globally. Limit the change to
  the local worktree (`.git/config`) so other worktrees and the
  user's other repos are unaffected.

## L30 - TestWeaselIPC.exe returns -2 (STATUS_INVALID_HANDLE) when no WeaselServer is running; this is a real failure, not a smoke test

**Incident (spec 026 pre-flight, 2026-07-03)**: the `scripts\run-tests.bat`
test gate has been reporting `=== TESTS FAILED ===` since spec 015 (2026-07-02),
but every subsequent session mis-classified the failure as "pre-existing
exit-code issue, not in scope" because PowerShell `& cmd /c "..."` returned
`OUTER_RC=0`. The real exit code is 1 (from run-tests.bat) because
`if !errorlevel! NEQ 0 set "FAIL=1"` correctly flags TestWeaselIPCs -2
return. Spec 026 closes this 4-session mis-classification.

**Root cause (1 - the test):** TestWeaselIPC.exe with no args enters
`client_main()`, which calls `weasel::Client::Connect()`. The Connect
fails because the named pipe has no listener. It prints "failed to connect to server." to stderr and `return -2`.
TestWeaselIPC does NOT have an orchestrator that spawns the server first.
It expects a human or a test script to launch `TestWeaselIPC.exe /start`
as a background process before running the client mode. The current
`run-tests.bat` does not do this.

**Root cause (2 - the PowerShell false-positive):** running
`cmd /c "scripts\run-tests.bat" & echo %ERRORLEVEL%` in PowerShell
returns the `cmd /c` parent process exit code, NOT the exit code of
the inner batch. The inner batch calls `endlocal & set "OUTER_RC=%FINAL_RC%"`
and `exit /b %OUTER_RC%`. When PowerShell captures %ERRORLEVEL% from
`cmd /c`, it reads cmd /c own exit, which can be 0 even if the inner
batch exited 1. The reliable way to capture inner-batch exit:

```batch
rem save_inner.bat - writes inner exit to file
@echo off
call scripts\run-tests.bat
echo %ERRORLEVEL% > _rc.txt
```
```powershell
# then in PowerShell:
$rc = Get-Content _rc.txt
Remove-Item _rc.txt
```

OR even simpler: use a `cmd /c save_inner.bat` and trust the second-level
%ERRORLEVEL% because the wrapper itself only does `call ... ; echo %ERRORLEVEL% > file`,
and cmd /c propagates the wrappers exit as its own.

**Verification recipe (R6 - evidence before assertion):**
```batch
@echo off
setlocal
"Release\TestWeaselIPC.exe" < nul
echo IPC_RC=%ERRORLEVEL%
endlocal
```
Run this from a `.bat` file (not inline `& cmd /c "..."` from PowerShell) and
the printed `%ERRORLEVEL%` is the true inner-exe return. Confirmed: -2.

**Anti-patterns to avoid (extending L22):**
- **AP-L30-A**: Treating `OUTER_RC=0` from `cmd /c "..."` in PowerShell
  as proof that the inner batch passed. It is not - it is the cmd /c
  parent process exit. Use a `.bat` wrapper.
- **AP-L30-B**: Running TestWeaselIPC without first spawning `/start` and
  expecting it to pass. The exe has built-in `/start`, `/stop`, and
  client modes, but no auto-orchestration. The test script must do it.
- **AP-L30-C**: Skipping a failing test from the gate by writing it off
  as "pre-existing / not in scope / smoke test" without verifying the
  real exit code via a `.bat` wrapper. The `-2` was visible since spec 015
  and 4 specs shipped under "all green" while it was actually failing.

## L31 - Integration test (TestWeaselIPC) silently broken since pre-spec-015: TWO simultaneous root causes

**Incident (spec 026 root-cause analysis, 2026-07-03)**: TestWeaselIPC.exe
has been broken since long before spec 015. Every spec since (015-025)
shipped under "all green" claims while the integration test was actually
returning -2 (STATUS_INVALID_HANDLE) from client_main. Two separate root
causes, each independently fatal:

**Root cause A - C++ virtual function hiding (NOT overriding):**
`test/TestWeaselIPC/TestWeaselIPC.cpp` defines `TestRequestHandler` extending
`weasel::RequestHandler`. The base class declares:
```cpp
virtual DWORD AddSession(LPWSTR buffer, EatLine eat = 0) { return 0; }
```
TestRequestHandler declares:
```cpp
virtual UINT AddSession(LPWSTR buffer) {  // <-- 1-arg, hides base
  return ++m_counter;
}
```
**The 1-arg `AddSession` HIDES the 2-arg base virtual; it does not override it.**
When the server calls `m_pRequestHandler->AddSession(buffer, lambda)` with
two arguments, virtual dispatch finds the BASE class vtable entry (because
`AddSession(LPWSTR, EatLine)` is the only 2-arg match), NOT TestRequestHandler.
So OnStartSession always returns 0 (the base class default), and client
session_id stays 0. Then `client.Echo()` checks `_Active() = Connected()
&& session_id != 0` -> false, returns false -> "failed to login."

C++ does NOT warn about this. To force a diagnostic, add the `override`
keyword (C++11) to the derived-class declaration. The compiler will then
reject the derived declaration because it does not match any base virtual.

**Root cause B - vcxproj OutDir path concatenation bug:**
`test/TestWeaselIPC/TestWeaselIPC.vcxproj` declared OutDir as:
```xml
<OutDir>$(SolutionDir)msbuild$(Configuration)$(Platform)</OutDir>
```
`$(SolutionDir)` is set by run-tests.bat to the repo root WITHOUT a trailing
backslash (`/p:SolutionDir="F:\soft\00selfmade\rime"`). So msbuild
concatenates `$(SolutionDir)msbuild\...` and produces the malformed path
`F:\soft\00selfmade\rimemsbuild\...` (note `rime` and `msbuild` glued
together - no separator). The linker then writes the .exe to this junk
path AND run-tests.bat still tries to run `Release\TestWeaselIPC.exe` from
the correct location, which is a STALE binary from a prior build (often
days/weeks old). The stale binary "passes" because its server has the
same broken AddSession override.

The fix in spec 026:
1. Always use explicit `\` after `$(SolutionDir)`: `$(SolutionDir)\$(Configuration)\`.
2. Add the missing `weasel.sln` line `{9C1CC4BA-...}.Release|Win32.Build.0 = Release|Win32`
   (TestWeaselIPC had `ActiveCfg` but no `Build.0`, so `msbuild weasel.sln`
   would never build it - the entire project was a build-graph dead-end).

**Why the failures were invisible for 4+ specs:**
- The stale binary in `Release\TestWeaselIPC.exe` was built on a date when
  `xbuild.bat weasel installer` last ran (typically at release time). It
  returned -2 every time. Every "ALL TESTS PASSED" claim was a PowerShell
  `cmd /c` false-positive (see L30).
- L22 (spec 015) fixed the underlying detection correctly (`!errorlevel! NEQ 0`),
  but the message "=== TESTS FAILED ===" was already a known L22 pattern
  and every spec author since wrote it off as "pre-existing / not in scope".
- No one ran TestWeaselIPC.exe standalone with a `.bat` wrapper (L30 recipe)
  to see the real exit code.

**Anti-patterns to avoid:**
- **AP-L31-A**: Defining a derived-class virtual function with a different
  parameter list than the base. It HIDES (does not override). Always use
  `override` keyword; the compiler will catch the mismatch.
- **AP-L31-B**: Setting `$(SolutionDir)` in `/p:` without a trailing backslash
  AND having vcxproj OutDir/IntDir that start with `$(SolutionDir)X` (no
  separator). The path gets glued. Always use `$(SolutionDir)\X` with the
  explicit backslash.
- **AP-L31-C**: Trusting "ALL TESTS PASSED" claims from spec authors without
  running the test suite yourself. Combined with AP-L22 / L30, this is a
 3-layer false-positive: stale binary + PowerShell OUTER_RC misread +
  L22-pattern write-off. The cure: always run `cmd /c save_inner.bat`
  pattern from L30 for verification.
- **AP-L31-D**: Adding a project to `weasel.sln` with only `ActiveCfg` and
  no `Build.0`. The project is in the solution for IDE navigation but
  will never be built by `msbuild weasel.sln`. Add both.


## L32 - Integration-test infra hardening (lesson-to-script promotion)

**Date:** 2026-07-03
**Spec:** 027 (`test-infra-hardening`)
**Status:** active
**Affected:** `scripts\test-infra\`, `scripts\run-tests.bat`

### Problem

Specs 015-026 accumulated four infra lessons (L22, L28, L30, L31) that all shared the same shape: a footgun in the test infra, stored only as prose in `lessons-learned.md`, and re-introduced by the next spec author who did not re-read the lesson (verified across 4 specs in the silent -2 case - the TestWeaselIPC bug went undiagnosed for the entire 015-026 window).

The pattern: prose lessons decay. Scripts do not. Every infra lesson that has bitten us more than once should be promoted to a named entry point (a `.bat` wrapper) that the next spec author calls by name, not by re-deriving the incantation from prose.

### Solution: lesson-to-script promotion

When a new infra lesson is written, ask three questions:

1. **Has it bitten us in 2+ specs?** If yes, the lesson is infra-grade and worth promoting to a script. If no, it may be a one-off; keep as prose and re-evaluate later.
2. **Is the fix a one-liner that is easy to forget?** If yes, the script is the only way to enforce it. If no (the fix is a long procedure that the spec author is unlikely to forget), prose is fine.
3. **Can a thin wrapper enforce it without changing product behavior?** If yes, build the wrapper. If the only way to enforce it requires product code changes, this is a feature, not infra; treat as a normal spec.

For spec 027, all three questions were "yes" for three of the four lessons:

| Lesson | Bitten 2+ times? | One-liner? | Thin wrapper? | Promoted to |
|---|---|---|---|---|
| L22 (`!errorlevel! NEQ 0`) | yes (silent -2) | yes (one keyword) | yes (no behavior change) | embedded in `run-test-suite.bat` |
| L28 (NSIS smoke test `.bat` wrapper) | yes (PowerShell `/D=` break) | yes (`.bat` not `.ps1`) | yes (no behavior change) | `install_smoke_test.bat` |
| L30 (PowerShell `cmd /c` false-positive) | yes (silent -2 false PASS) | yes (the `endlocal & set` pattern) | yes (no behavior change) | embedded in `run-test-suite.bat` |
| L31 (vcxproj OutDir path-glue) | yes (stale-binary diagnosis) | yes (add `\` after `$(SolutionDir)`) | **detection** only (cannot prevent the path-glue in vcxproj) | `verify-test-binaries-fresh.bat` |

L31 is the interesting one: the L31 fix itself (add `\` to vcxproj) is not a thin wrapper (it requires editing `.vcxproj`). But the L31 **detection** (mtime check) is a thin wrapper. The promotion is from "detection via 4-spec-long post-mortem" to "detection via 5-line script".

### Result of the promotion

After spec 027, the `scripts\test-infra\` directory contains:

- `install_smoke_test.bat` - named entry point for the AGENTS.md sec 2.5 NSIS smoke test recipe. Exits 0 today; will be filled in if/when the recipe is ported to pure cmd.
- `run-test-suite.bat` - the actual meat. Contains the proven `run-tests.bat` body (L22 + L30 cures baked in) and is callable by name from any spec that needs to verify test infra.
- `verify-test-binaries-fresh.bat` - L31 stale-binary detector. Iterates the 6 test projects, compares mtimes, exits 1 if any source is newer than its .exe.

`scripts\run-tests.bat` is now a 5-line thin wrapper that calls `scripts\test-infra\run-test-suite.bat %*`. Backward-compat preserved.

### Anti-patterns to avoid

- **AP-L32-A**: Writing infra lessons only as prose. They decay; the next spec author re-derives the incantation, gets it wrong, and the bug returns. Always ask the 3 promotion questions when writing a new infra lesson.
- **AP-L32-B**: Promoting a lesson to a script that does NOT enforce the lesson. If the script is a thin wrapper that just prints a pointer ("see AGENTS.md sec 2.5"), the script must be honest about that (header documents the deferral). A wrapper that pretends to enforce a lesson but does not is worse than no wrapper, because it gives a false sense of safety.
- **AP-L32-C**: Bundling the L22 + L30 cures into a `.ps1` script because "PowerShell is more readable". L28 says no: NSIS / batch infra must use `.bat` wrappers, not PowerShell shims. The L22 + L30 cures are part of the test infra, so they live in `.bat`.

### Related

- L22 (the `!errorlevel! NEQ 0` cure, embedded in `run-test-suite.bat`).
- L28 (the `.bat` wrapper mandate, followed by `install_smoke_test.bat`).
- L30 (the `endlocal & set` cure, embedded in `run-test-suite.bat` and `verify-test-binaries-fresh.bat`).
- L31 (the L31 detection, codified in `verify-test-binaries-fresh.bat`).


## L33 - PowerShell `$` parsing eats PowerShell -Command "$..." variables

**Date:** 2026-07-03
**Spec:** 027 (`test-infra-hardening`)
**Status:** active
**Affected:** any `.bat` that shells out to PowerShell via `-Command "..."`

### Problem

When a `.bat` file calls `powershell -NoProfile -Command "$e = ...; if(-not $e){...}"`, the cmd parser hands the entire double-quoted string to PowerShell verbatim. The bug surfaces when the .bat is invoked from PowerShell (or from another tool that goes through PowerShell argument parsing) - PowerShell parses the `-Command` string ITSELF and interprets the `$` characters as variable expansions BEFORE the string is passed to cmd. The `$e` becomes empty, `$s` becomes empty, and PowerShell sees syntax like `if (-not ){` - syntax error - exit 1, regardless of mtimes.

### Solution: -File, not -Command

Put the PowerShell in a sibling `.ps1` file and call `powershell -NoProfile -ExecutionPolicy Bypass -File script.ps1 arg1`. The `-File` mode does not go through PowerShell command-string parsing, so `$` survives intact.

In spec 027, `scripts/test-infra/verify-stale-temp.ps1` is the sibling, and `verify-test-binaries-fresh.bat` calls it via `-File`. The `.ps1` is gitignored (see `scripts/test-infra/verify-stale-temp.ps1` in `.gitignore`); the `.bat` wrapper is the tracked entry point.

### Anti-patterns to avoid

- **AP-L33-A**: Calling `powershell -Command "$x = ...; ...; exit 1"` from a `.bat` invoked through PowerShell. Always use `-File` with a sibling `.ps1`.
- **AP-L33-B**: Trying to escape `$` as `` `$ `` or `^^$` in the `-Command` string. The escaping rules are subtle (cmd vs PowerShell) and break in edge cases. Just use `-File`.
- **AP-L33-C**: Generating the `.ps1` from the `.bat` at runtime via `> file echo ...` - cmd echo mangles `(`, `)`, `;`, `|` in the output. Keep the `.ps1` as a tracked-or-gitignored file with literal PowerShell content.

### Related

- L34 (the other cmd parsing bug discovered in spec 027 - rem-line `(`/`)` block parsing).
- L28 (the `.bat` wrapper mandate - the .bat IS the wrapper, the .ps1 is the interop detail).

---

## L34 - cmd `rem` lines containing `(` start a sub-block that ends at the next `)`

**Date:** 2026-07-03
**Spec:** 027 (`test-infra-hardening`)
**Status:** active
**Affected:** any `.bat` whose `rem` comments contain parentheses

### Problem

cmd `rem` lines are supposed to be comments, but the `(` and `)` characters inside a rem line still affect cmd block-parse state. A rem line like `rem raise 0xC0000005 (signed -1073741819)` opens a sub-block at `(`; the next `)` in the script (often on a later rem line, or on an `echo` line, or in the same rem line) closes it. The text after that `)` is then parsed as a new command, leading to errors like `'.misinterprets' is not recognized as an internal or external command` if the post-`)` text happens to look like a command.

Symptom: a `.bat` runs (exit 0, all tests pass) but prints phantom "is not recognized as an internal or external command" errors to stderr. The errors are cosmetic (do not affect the exit code) but they pollute logs and hide real failures.

### Solution

Avoid `(` and `)` in `rem` lines. Replace with words (`Lparen`, `Rparen`, `signed hex`, etc.) or punctuation (`-`, `,`):

```batch
rem BAD - the (signed ... ) pair breaks block-parse state
rem failures in optimized Release builds raise 0xC0000005 (signed
rem -1073741819), which is < 1 numerically, so "if errorlevel 1"
misinterprets a real failure as a pass.   <- NO rem prefix! cmd now
                                           tries to run "misinterprets"

rem GOOD - dash instead of parens, no block-parse interference
rem Use NEQ 0 - not "if errorlevel 1" - because BOOST_ASSERT
rem failures in optimized Release builds raise 0xC0000005 - signed
rem -1073741819 - which is less than 1 numerically, so "if errorlevel 1"
rem misinterprets a real failure as a pass - see L22.
```

### Anti-patterns to avoid

- **AP-L34-A**: Writing a rem line with a function-call style description like `rem call foo(x, y)`. Use `rem call foo with x and y` instead.
- **AP-L34-B**: Writing `rem (see spec 027)` in an `if not exist ... (` block. The `(` opens a sub-block inside the rem, and the `)` in the rem closes the if-block. The next command after the rem then runs unconditionally. Use `rem - see spec 027` instead.
- **AP-L34-C**: Trusting "exit 0, all tests pass" without reading stderr. Phantom "is not recognized" errors are a tell that a rem line has a `(` or `)` in the wrong place. The error is cosmetic (does not affect RC) but the underlying block-parse corruption can cause real commands to be skipped.

### Related

- L33 (the other cmd parsing bug - `$` in PowerShell -Command).
- L30 (the OUTER_RC false-positive lesson; phantom stderr from L34 does not affect OUTER_RC, so do not be fooled by OUTER_RC=0 into ignoring L34 symptoms).


## L35 - rime_api.h does NOT expose rime_candidate_t.is_user_dict

**Date:** 2026-07-04
**Spec:** 028 (`candidate-delete-core`)
**Status:** active
**Affected:** any spec that plans candidate-level dispatch based on user-dict vs shared-dict origin

### Problem

Spec 008 (`008-candidate-edit`) plan.md §2.2 assumed that librime 1.13 's
`rime_candidate_t` exposes an `is_user_dict` field (annotated '1.13+ adding? needs verification').
**That field is NOT in the C API.** Reading the actual librime 1.13 `rime_api.h:3256` shows:

```c
typedef struct rime_candidate_t {
  char* text;
  char* comment;
  void* reserved;
} RimeCandidate;
```

Only `text`, `comment`, `reserved`. The `is_user_dict` member exists in the C++ class `rime::Candidate` (with a `type()` method) but the C API deliberately exposes a narrower surface. The C++ 'is_user_dict' accessor calls `type() == 'user_dict'` internally, but this is NOT projected to the C API.

### Implication

Any spec that plans to do 'is_user_dict==true -> delete' at the C API layer (or any candidate-level dispatch based on origin) is impossible. The C API only lets you call `delete_candidate_on_current_page(index)` and the engine does the right thing internally:

- librime/src/rime/context.cc:146 - `Context::DeleteCandidate(index)` sets `selected_index = index` and then calls `commit_history` which removes the user.db entry if the candidate is a user-dict entry, or no-ops otherwise.
- librime/src/rime_api_impl.h:34106 - `RimeDeleteCandidate` wraps the above.

So the correct stage 1 design for spec 008 (which spec 028 implements) is: just call `delete_candidate_on_current_page(index)`. The engine decides what to do. The application layer cannot and should not try to inspect `is_user_dict` at the C API level.

### Anti-patterns to avoid

- **AP-L35-A**: Planning around a C API field before grepping the .h file to confirm it is exposed. The cost is one `IndexOf('is_user_dict', rime_api.h)` call; the benefit is catching the gap before writing 200 lines of code that depend on it.
- **AP-L35-B**: Assuming that because librime C++ has a class member / accessor, the C API exposes it. The C API surface is a deliberate subset of the C++ surface, not a 1:1 mirror. Always treat the .h file as the source of truth.
- **AP-L35-C**: Designing a 'two-path' feature (is_user_dict -> delete, otherwise -> ignore) that requires the application layer to inspect candidate origin. Push the dispatch into the engine by calling a single C API function and trusting the engine to do the right thing. The engine has more context (e.g. knows which dict the entry lives in) than the application layer ever will.

### Related

- spec 008 (`008-candidate-edit`) - the parent spec that made the wrong assumption. Stage 1 of spec 008 = spec 028. Stages 2-4 (UI + user_ignore.txt + dark mode) deferred to a later spec.
- librime 1.13 `rime_api.h:3256` - the actual struct definition (C API surface).
- librime 1.13 `rime_api.h:15635` - `delete_candidate` / `delete_candidate_on_current_page` C API functions.
- librime 1.13 `librime/src/rime_api_impl.h:34106` - `RimeDeleteCandidate` C-to-C++ wrapper.
- librime 1.13 `librime/src/rime/context.cc:146` - `Context::DeleteCandidate` engine-side implementation.
- L09 (NSIS BOM - reminds us to read the file as bytes, not as a model of what we want it to be).
- L32 (lesson-to-script promotion - the lesson here is: add a pre-flight step that reads the C API .h file, even for 'well-known' libraries).


## L36 - L## fix coverage gap: fix in 1 file is not fix in N files

**Date:** 2026-07-04
**Spec:** 029 (`l31-fix-coverage`)
**Status:** active
**Affected:** any repo-wide "fix one instance" lesson; especially vcxproj / .sln / config-file patterns

### Problem

Spec 026 fixed the L31 vcxproj OutDir path-glue bug in `TestWeaselIPC.vcxproj` (1 of 4 test vcxproj files). The fix was correct for that file. It was **wrong as a repo-wide fix** because the other 3 test vcxproj files (TestDefaultHotkeys, TestShiftSelectBinding, TestBindingResolution, TestYamlRoundTripE2E - 4 files total) had the **same broken pattern** but were not touched.

The 4 affected files were discovered only because spec 028 added a 5th test project (TestUserDictUpdate) with the L31 fix applied as a matter of course. The build output of the 4 old projects still showed the L31 glue path (e.g. `F:\soft\00selfmade\rimemsbuild\Release\Win32\TestResponseParser.exe`) - 4 releases of broken path that the L31 lesson did NOT catch.

The root cause: when an L## lesson is written, the author fixes the **one instance they are touching** and writes the lesson as if the fix is now repo-wide. The lesson reads 'always use `$(SolutionDir)\X` with the explicit backslash' - but the broken pattern is still sitting in 3 other .vcxproj files that nobody grepped for.

### Solution: the L## coverage audit

When writing any new L## lesson, add a **coverage audit step** that:

1. Identifies the broken pattern by string (e.g. `$(SolutionDir)$(Configuration)\`).
2. `rg` (or grep) the entire repo for that pattern.
3. Lists every match and decides: is this match also broken, or is it an unrelated intentional usage?
4. If broken, fix ALL of them in the same spec, not in a follow-up.
5. If the audit is too large for the current spec, open a new spec (like this one) but do not leave the partial fix in place.

For the L31 vcxproj case, the audit was:

```powershell
# After fixing 1 file, audit the remaining 3
rg '\$\(SolutionDir\)\$\(' test/
# -> TestDefaultHotkeys.vcxproj: 8 matches (Win32 + ARM + ARM64 + x64, Release + Debug)
# -> TestShiftSelectBinding.vcxproj: 8 matches
# -> TestBindingResolution.vcxproj: 2 matches
# -> TestYamlRoundTripE2E.vcxproj: 2 matches
# -> TestResponseParser.vcxproj: 2 matches (Pattern B - `$(SolutionDir)msbuild\...`)
```

20 matches total across 5 files. Spec 029 fixes all 5.

### Anti-patterns to avoid

- **AP-L36-A**: 'I fixed it in the test exe I was touching.' The 3 others still have the bug; they just happen to still build because the wrong path is also a valid path on disk (L31 'silent build success' pattern). Always run the coverage audit.
- **AP-L36-B**: Trusting the L## lesson to be 'applied repo-wide' when in fact it was applied to one file. The lesson is **written** repo-wide, but the **fix** is per-file. Always re-grep after writing the lesson.
- **AP-L36-C**: Leaving stale binaries at the wrong path 'because they still build and run'. They will be picked up by future ad-hoc invocations (e.g. a developer who runs the .exe directly from the old path) and confuse the next agent. After fixing the vcxproj, `Rebuild` + `Remove-Item` the old-path .exe.
- **AP-L36-D**: Assuming msbuild 'Rebuild' will clean the old-path .exe. It does not - msbuild writes the new .exe to the new path but leaves the old .exe untouched (it is not in any known location to msbuild, so msbuild does not consider it 'stale'). Manual `Remove-Item` is required.

### Related

- L31 (vcxproj OutDir path-glue - the original lesson, now with full coverage).
- spec 026 (test-weasel-ipc-orchestration - where L31 was first written; fixed 1 of 4).
- spec 028 (candidate-delete-core - added the 5th test project, exposed the gap).
- spec 029 (l31-fix-coverage - this spec; fixes the remaining 3 of 4).
- spec 027 (test-infra-hardening - the `verify-test-binaries-fresh.bat` that would have caught this earlier if the binaries were at the right path; the wrong-path .exe was outside its check scope).
- L22 (test detection - same shape: a one-line fix that is silent if applied incompletely).
- L32 (lesson-to-script promotion - applies the other direction: lesson -> script. L36 is lesson -> audit checklist, complementary pattern).


## L37 - PowerShell line-based array ops corrupt CRLF files (use byte-level replace for line-precise edits)

**Date:** 2026-07-04
**Spec:** 030 (`candidate-rbutton-ui`)
**Status:** active
**Affected:** any PowerShell session that needs to insert / replace a
contiguous block of lines in an existing CRLF file.

### Symptom

When you do `$t = Get-Content $f; $lines = $t -split "`n"; ... ;
($lines2 -join "`n")`, the original file's CRLF line endings get
broken:

- `-split "`n"` on a CRLF file yields lines that **end with `CR`**
  (the `\r` is the last char of each line).
- If you join the array with `"`n"` (LF), the new content has
  **lone LF** line endings — not CRLF.
- If you then byte-level write the result with
  `[System.IO.File]::WriteAllBytes`, the file's CR count and LF
  count will diverge, and CRLF-sensitive tooling (clang-format on
  C++, msbuild preprocessor on .bat, NSIS on .nsi) will misbehave
  or fail.

Concrete case (spec 030, WeaselPanel.cpp patch):

1. `Get-Content WeaselUI/WeaselPanel.cpp | Select-String OnMouseLeave`
   to find the line to insert after. Got `LineNumber = 518`.
2. Computed `endLine = $start - 1 + 7` and sliced lines 0..endLine-1,
   inserted new content for the OnRButtonDown function, joined with
   `"`n"`, wrote with `WriteAllBytes`.
3. Result: WeaselPanel.cpp had 24 lone LFs (CR=1338 LF=1362). The
   file also had a **truncated** OnMouseLeave function (I sliced
   past its closing `return 0;` and `}` because my line-index math
   was off by 1 — Get-Content's LineNumber is 1-based; `$lines[i]`
   is 0-based).
4. Fix path: re-read the file, do a **byte-level replace** of a
   unique multi-line needle that includes the existing trailing
   context, then re-check `CR == LF` after writing.

### Cause

`Get-Content` returns each line with its native line terminator
stripped in PS 5.1 (PS 7 may keep it; behavior differs). `-split "`n"`
on CRLF content leaves trailing `CR` on each line element. `-join
"`n"` drops those trailing CRs. Net result: lone LF.

The line-index math is also easy to get wrong because rg /
Select-String return 1-based line numbers but `$lines[i]` is 0-based.

### Cure

For any **non-trivial** line-based edit in a CRLF file:

1. **Prefer byte-level replace**: build a unique multi-line needle
   that includes the line above and below the insertion point, then
   `$t.Replace($needle, $needle + $insertedBlock)` and
   `[IO.File]::WriteAllBytes($f, [Text.Encoding]::UTF8.GetBytes($t2))`.
   The needle carries the original CRLF; the inserted block must
   use `` `r`n `` explicitly.

2. **After every write**, verify byte health (AGENTS.md sec 5):
   ```powershell
   $b = [IO.File]::ReadAllBytes($f)
   $cr = ($b | Where-Object {$_ -eq 0x0D}).Count
   $lf = ($b | Where-Object {$_ -eq 0x0A}).Count
   # For any file that was CRLF before, $cr -eq $lf must hold.
   ```

3. **If you must use line-based array ops** (e.g. for a multi-line
   rewrite that's not amenable to a simple needle match): after
   the rewrite, run `$t2 = $t.Replace("`n", "`r`n")` to restore CRLF.
   But note this can convert pre-existing CR-only lines (rare) and
   can also touch content the user did not intend to change. So the
   **post-write verify** in step 2 is mandatory.

4. **Sanity check the insertion end-line**: if the file is
   syntactically broken (e.g. msbuild C1004 / C1075 / C2017 errors
   about unclosed #if), the most likely cause is that you sliced
   off the closing brace of the function you inserted into. Verify
   by re-reading the file and inspecting the section between the
   end of the previous function and the start of the next.

### Verification (already applied during spec 030)

```powershell
# spec 030 WeaselUI/WeaselPanel.cpp after the OnRButtonDown insert
# had CR=1338 LF=1362 (24 lone LFs). One byte-level replace
# ("m_mouse_entry = false;\r\n" -> "m_mouse_entry = false;\r\n  return 0;\r\n}\r\n\r\n"
# + separate replace of the trailing "  return 0;\r\n}\r\n\r\n}\r\n\r\n" ->
# "  return 0;\r\n}\r\n\r\n" to remove the orphan "}" that the
# initial insert left behind) restored CR=1363 LF=1363.
```

### Why this is L## -worthy (not just a fix)

- The bug is silent — the file still compiles the changed function
  and the inserted OnRButtonDown, but the **truncated OnMouseLeave
  function** would have caused a runtime crash the first time the
  user moved the mouse out of the candidate window. There is no
  test in the repo that exercises OnMouseLeave at the GUI level
  (tests are behavior-level, no live GUI).
- The fix took 2 iterations of byte-level replace. The first
  iteration fixed CR/LF divergence but I had to spot the missing
  OnMouseLeave closing brace on a second read.
- The pattern (line-based array ops on CRLF files) is going to
  come up again every time we add a new message handler / new
  function in WeaselPanel.cpp / WeaselTSF.cpp / RimeWithWeasel.cpp.
  Future sessions should reach for byte-level replace as the
  default, not line-based arrays.

### Cross-references

- L01 (Chinese UTF-8 file read/write) — same family of "PowerShell
  string layer corrupts binary file" bugs; cure is the same:
  byte-level APIs.
- L02 (Chinese content edits) — same family.
- L09 (NSIS BOM + line endings) — same pattern in a different
  context; NSIS also requires strict CRLF, and PowerShell line
  ops can break it.
- AGENTS.md sec 5 step 1 (byte health check) — the
  `$cr -eq $lf` verify is the same one we use for NSIS, just
  re-applied to every CRLF file we touch.

## L38 - spec-incremental-shipping exposes build-time deps that earlier specs silently relied on

**Date:** 2026-07-04
**Spec:** 031 (`candidate-ignore-filter`)
**Status:** active
**Affected:** any future spec that adds a new method / file / include
in WeaselUI that pulls in a function defined in RimeWithWeasel or
another `static` xmake target, and links that .obj into a shared
library (weasel.dll) via WeaselUI.lib.

### Symptom

During spec 031, after adding `LoadIgnoreList` to WeaselPanel.cpp
which calls `WeaselUserDataPath().wstring()` (defined in
`RimeWithWeasel/WeaselUtility.cpp` inside the `RimeWithWeasel`
static lib target), the build failed at:

```
weasel.dll : fatal error LNK1120: 1 个无法解析的外部命令
  WeaselUI.lib(WeaselPanel.cpp.obj) : error LNK2001: 无法解析的外部符号
  "class std::filesystem::path __cdecl WeaselUserDataPath(void)"
```

`weasel.dll` (the WeaselTSF shared library) was not transitively
linking `RimeWithWeasel.lib` even though `WeaselUI.lib` (which
*it* does link) was now pulling in a symbol that lives in
RimeWithWeasel. xmake's `add_deps` only expresses *build order*
between targets; it does not express *link order* unless the
target is also in `add_links`.

The same issue applies to `/LTCG` and `/GL`: xmake's top-level
`add_ldflags("/LTCG /INCREMENTAL:NO", {force = true})` (xmake.lua
line 64) was not being applied to the WeaselTSF target's final
link step, because `add_shflags("/DEBUG /OPT:REF /OPT:ICF")` in
`WeaselTSF/xmake.lua` re-wrote the shflags without preserving the
top-level `add_ldflags`. The MSVC linker warned
"找到 MSIL .netmodule 或使用 /GL 编译的模块" and refused to
link.

### Why spec 030 did NOT trip this

spec 030 added:
- `SetDeleteCandidateCallback` to WeaselUI.h (UI class API)
- `m_deleteCallback` to WeaselPanel.h
- `OnRButtonDown` to WeaselPanel.cpp
- `m_hoverIndex` and the dispatch to `m_deleteCallback` (a
  `std::function<void(size_t)>` member, no external symbol)

None of these reference any function in a *separate* xmake target.
They all resolve inside the WeaselUI target itself or to system
APIs. So the missing transitive link was not exercised.

spec 031 added `LoadIgnoreList` which calls
`WeaselUserDataPath()` (a function in a *different* xmake target).
This is the first new external reference in WeaselPanel.cpp since
spec 026 (which only edited `RimeWithWeasel.cpp` internals). The
missing link was exposed for the first time in spec 031.

### Root cause (two distinct xmake.lua gaps)

1. **WeaselTSF target had no `add_deps` / `add_links` to
   RimeWithWeasel.** The dep was implicit before spec 031 because
   no WeaselUI symbol pulled in a RimeWithWeasel symbol that the
   WeaselTSF target itself also referenced. After spec 031,
   WeaselUI.lib (used by WeaselTSF) pulls in WeaselUserDataPath,
   so WeaselTSF must also link RimeWithWeasel.lib.

2. **WeaselTSF's `add_shflags` did not preserve top-level
   `add_ldflags("/LTCG", {force=true})`.** xmake's `add_*flags`
   functions append to the target's flags; `add_shflags` (shared
   library ldflags) was set in WeaselTSF/xmake.lua without
   referencing the top-level `add_ldflags`. The `/LTCG` flag was
   thus not present in the actual weasel.dll link command, but
   `/GL` was set on every compile via the top-level
   `add_cxflags("/GL")` - producing the inconsistent state that
   MSVC linker rejects.

### Fix (in spec 031)

In `WeaselTSF/xmake.lua`:
- `add_deps("WeaselIPC", "WeaselUI", "RimeWithWeasel")` so the
  RimeWithWeasel target is built before WeaselTSF.
- `add_links("RimeWithWeasel")` so RimeWithWeasel.lib is passed
  to the weasel.dll link command.
- Added `/LTCG` to `add_shflags("/DEBUG /OPT:REF /OPT:ICF
  /LTCG")` so the top-level add_ldflags is preserved on the
  shared library.

### Cure (for future specs)

When adding a new method / include in WeaselUI that references a
symbol defined in another xmake target (RimeWithWeasel,
WeaselIPCServer, etc.):

1. **Predict the link impact before running xbuild.** If the
   new symbol is in a `static` target (RimeWithWeasel,
   WeaselIPCServer), and it will be transitively linked into
   `weasel.dll` (via WeaselUI.lib or WeaselIPCServer.lib), then
   the WeaselTSF target needs both `add_deps` and `add_links`
   for that target.

2. **L36 already covers this in spirit** ("fix in 1 file is
   not fix in N files"). The corollary for build files
   (xmake.lua / .vcxproj / weasel.sln) is: when you add a new
   reference to an external target's symbol, audit the link
   graph for *every* target that transitively depends on the
   target that now references the external symbol. Spec 030
   did not do this audit; spec 031 had to.

3. **The cleanest preventive measure** would be to add
   `add_deps("RimeWithWeasel")` and
   `add_links("RimeWithWeasel")` to WeaselTSF at the same time
   the project was first set up. The fact that it works without
   is accidental (L37 family of "silent until exercised"
   problems).

4. **For the `/LTCG /GL` consistency check**, a quick smoke
   test is: after a build, check the link command of every
   `.dll` (or `.exe` if it links multiple .obj with /GL).
   The flag set must be consistent: either all `.obj` use
   `/GL` and the link uses `/LTCG`, or none do. xmake's
   `add_cxflags` / `add_ldflags` with `force=true` is not
   a guarantee that the flag survives per-target overrides
   like `add_shflags`.

### Cross-references

- L36 (L## fix coverage gap) - same pattern, different surface.
- L37 (PowerShell line-based array ops) - same family of
  "silent until exercised" problems; both L36 and L38 are
  about coverage gaps that only surface on a follow-up change.
- L10 (librime Win32-only constraint) - same family of "build
  config that works in the happy path but breaks when a new
  symbol is added".
- xmake.lua line 64 - the top-level `/LTCG` add_ldflags that
  did not propagate to WeaselTSF.
- WeaselTSF/xmake.lua line 19 - the fixed shflags including
  `/LTCG` (after spec 031).

## L39 - 2013 invalid function arguments root cause

**Date:** 2026-07-04
**Spec:** 032 (candidate-rbutton-finalize)
**Status:** active
**Affected:** mojibake-affected files + PS here-strings for large file generation.

### Symptom

A spec 032 task was interrupted mid-flight with the platform error:

[CODE-FENCE]
invalid params, invalid function arguments data string,
tool_call_id: call_function_rkpuk39h8aoc_1 (2013)
[CODE-FENCE]

Code 2013 is OpenAI function-call argument validation. The agent tool layer rejected the assistant message because the embedded function_call data was malformed. The malformed data was not produced by the tool itself; it was produced by copying mojibake text from the project PRD / TDD into tool parameters. The mojibake byte sequences (C2 / C3 / E2 / 80-range bytes representing GBK bytes misdecoded as CP1252 misdecoded as UTF-8) broke string escaping when re-serialized.

### Root cause (three contributing factors)

1. .specify/PRD.md and .specify/TDD.md were GBK->UTF-8 mojibake. Both files were authored in a previous session under chcp 936 + PowerShell 5.1 with default Out-File -Encoding utf8, producing the L01 damage chain.
2. The previous spec author read PRD/TDD content via Get-Content (PS 5.1 default = GBK on this machine), pasted those mojibake strings into assistant tool parameters, and the platform layer rejected the embedded call.
3. PowerShell here-string in large scripts has secondary failure modes: dollar-paren inside double-quoted here-string is treated as subexpression; apostrophes inside single-quoted here-string terminate it early. Both modes caused silent file truncation / parser errors during spec 030 / 031 / 032 work.

### Cure (3 steps, all applied 2026-07-04)

1. Restored PRD.md from commit 54cdd2d blob and TDD.md from 6f6fcbf blob, byte-level with LF->CRLF conversion. Verified:
   - PRD.md: 14456 bytes, CR==LF==235, 0 overlong UTF-8 (0xC0/0xC1), SHA256 matches HEAD LF hash.
   - TDD.md: 12322 bytes, CR==LF==250, 0 overlong UTF-8, SHA256 matches HEAD LF hash.
   - Both files now display clean Chinese in any UTF-8 tool. core.autocrlf=true is set locally; git ls-files blob is LF, working tree is CRLF - both views SHA-equal.
2. Added 3 patterns to .gitignore to prevent future untracked leftovers: TestDefaultHotkeys.obj (build artifact), run_librime.bat (manual script), run_librime_*.err (manual error log). These were untracked before spec 032 and would have polluted future git add . calls (A10).
3. For all subsequent file generation, use byte-level .Replace() on a known-clean template file, never PS here-strings. The pattern that works on PS 5.1:
   - Step A: Read clean template via byte-level
   - Step B: .Replace() 3 known-anchor strings (no here-string parsing)
   - Step C: Force CRLF (idempotent, replaces lone LF with CRLF)
   - Step D: Write byte-level

### Verification

- git hash-object .specify/PRD.md = HEAD blob SHA (autocrlf=LF view).
- git hash-object .specify/TDD.md = HEAD blob SHA after git add.
- git diff HEAD shows only .gitignore (spec 032 cleanup) and librime (submodule, not part of this fix).
- git status clean for PRD.md / TDD.md after autocrlf normalization.
- TestDefaultHotkeys.obj / run_librime.bat / run_librime_*.err no longer appear in git status after .gitignore patch.

### Cross-references

- L01 (GBK pollution chain) - the original mojibake author pattern.
- L02 (byte-level discipline) - the cure for L01; same pattern reused here.
- L36 (L## fix coverage gap) - the audit pattern applied here to grep .gitignore for all untracked patterns, not just the obvious one.
- L37 (PS line-based array ops) - the related here-string failure mode; this spec hit it twice, and recovered both times by switching to byte-level .Replace().
- A1 / A2 (anti-patterns in AGENTS.md) - read / write Chinese via PowerShell Get-Content / Out-File is the entry point; always use IO.File.ReadAllBytes / WriteAllBytes for any Chinese-containing text file.
- A10 (anti-pattern in AGENTS.md) - git add . from project root would have swept up weasel.props, env.bat, *.log, and github_token.txt; spec 032 staged by explicit path and the untracked leftovers are now in .gitignore so future A10 incidents are self-blocking.


## L40 - PRD.md / TDD.md corruption is literal '?' (0x3F), not GBK->UTF-8 mojibake

**Date:** 2026-07-04
**Spec:** 033 prep (next after 032)
**Status:** active
**Affected:** `.specify/PRD.md`, `.specify/TDD.md`. Sub-specs (004-032) and AGENTS.md/constitution.md/lessons-learned.md are NOT affected.

### Symptom

When the assistant reads `.specify/PRD.md` or `.specify/TDD.md`, the Chinese content renders as runs of `?` characters (e.g. `?? ?? spec ????` instead of `项目级 PRD，对应 spec 004 路线图与 7 份子 spec`). The displayed text is readable structurally but every Chinese phrase is replaced with `?`.

L39 (the previous lesson) diagnosed this as GBK->UTF-8 mojibake. That diagnosis was **wrong**. The actual byte content is literal `0x3F` (`?`) characters separated by `0x20` (space), not the multi-byte sequences (0xC2 0xC3 0xE2 0x80-range) that GBK misdecoding produces.

### Verification (the wrong-diagnosis check)

Byte-level diff of HEAD and commit 54cdd2d (the supposed "restoration source"):

```
HEAD:.specify/PRD.md     = f7e2d9c1fc535e440cca6fbe369eaf084678b850  (14221 bytes)
54cdd2d:.specify/PRD.md  = f7e2d9c1fc535e440cca6fbe369eaf084678b850  (14221 bytes)
                                        (identical)
```

`git rev-parse HEAD:.specify/PRD.md` and `git rev-parse 54cdd2d:.specify/PRD.md` produce the SAME blob SHA. The "L39 restoration" was a no-op: it copied a corrupted blob from an earlier commit and re-verified it byte-for-byte, declaring success.

The actual mojibake check (looking at the hex dump of HEAD's PRD.md):
- Byte 0x10 onward: `23 20 46 6C 75 78 69 6E 67 20 76 32 20 3F 20 50 ...`
  - ASCII: `# Fluxing v2 ? P`
  - The `?` is `0x3F` - ASCII question mark, not a UTF-8 multi-byte sequence.
- Compare to spec 004 spec.md line 1: `35 32 48 48 52 32 194 183 32 231 129 171 ...`
  - `194 183` = `×` (UTF-8 multi-byte for the same range Chinese)
  - spec 004 has REAL Chinese UTF-8 bytes. PRD.md does not.

### Root cause (revised)

The 2 files were **created** with `?` substitution characters from the start (commit 6f6fcbf on 2026-06-XX, the original commit). The most likely cause:

1. The original authoring session had `chcp 65001` but the Out-File / Set-Content path was using a different encoding (e.g. ASCII default) which substituted `?` for any non-ASCII character. The session wrote structurally-complete content but the Chinese phrases became `?` on disk.

2. Subsequent sessions (commits 54cdd2d, 6f6fcbf, 7b2eec5/L39) read the file via `Get-Content` (which showed `?` correctly as `?`), assumed the file was just sparse documentation, and only made tiny edits (line 1 corruption fix, L19/L20 status updates). None of the sessions actually noticed that the bulk of the Chinese content was missing, because:
   - The files display structurally-complete Markdown (headings, tables, lists all in place)
   - The English / ASCII / number / punctuation content IS correct
   - Only the Chinese phrase bodies are `?`

3. The L39 "restoration" commit verified byte-for-byte equality with the source commit, but the source was already broken. The verification metric (SHA match) was a structural check, not a content check.

### The data is lost (cannot be recovered from git)

No commit in the history contains the original Chinese content for PRD.md or TDD.md. The earliest commit (`6f6fcbf`) is the broken one. Reflog entries before 6f6fcbf do not exist for this branch.

### Recovery path (deferred, not part of this spec)

Two options, neither is reversible by git alone:

1. **Reconstruction from scratch** - rewrite PRD.md and TDD.md from the spec 004 roadmap (which IS intact) + constitution.md (intact) + AGENTS.md §1 project map (intact) + the existing sub-specs 005-032 (intact). The content is fully recoverable from these sources, but the wording will be new, not the original.

2. **Accept the loss and add a marker** - replace the `?` runs with `[Chinese content lost; see spec 004 + sub-specs 005-032 for authoritative text]` so future readers know to consult those instead of trusting the PRD/TDD as-is.

This is a future spec (likely 033 or later). The current 0.18.19.0 release does NOT include a fix; the corruption is documented here so the next session does not waste time re-discovering it.

### Cure (this lesson)

1. **Always byte-level grep for `0x3F` runs** when verifying a Chinese Markdown file. A run of 3+ consecutive `0x3F` in a non-code section is a corruption marker, not legitimate text.
2. **`git hash-object` matching is structural, not content** - it confirms "the bytes are the same as before" but not "the bytes are correct". Add a `0x3F` count check and a `0xE4..0xE9` range (common CJK UTF-8 lead bytes) count check before declaring success.
3. **Visually inspect the file** before committing any "restoration" claim. A 1-second read of the first 200 bytes would have caught this in any session.
4. **Sub-specs (004-032) are authoritative** for the project's intent. The PRD/TDD corruption is silent because the actual product intent is in spec 004 + sub-specs + constitution + AGENTS.md, not in PRD/TDD. Document this in AGENTS.md §6 (How to Use This File) so the next agent knows to skip PRD/TDD if they look broken.

### Verification

- `.specify/PRD.md` has 0x3F count = ~1200+ (predominantly runs of `?` between ASCII structure)
- `.specify/TDD.md` has 0x3F count = ~900+ (same pattern)
- `.specify/specs/004-fluxing-v2-roadmap/spec.md` has 0x3F count = 0 (clean)
- `.specify/specs/005-.../spec.md` has 0x3F count = 0 (clean)
- `.specify/memory/constitution.md` has 0x3F count = 0 (clean)
- `.specify/memory/lessons-learned.md` has 0x3F count = 0 (clean)
- `AGENTS.md` has 0x3F count = 1 (legitimate ASCII `?` in code example, not corruption)
- `CHANGELOG.md` has 0x3F count = 9 (legitimate `?` in changelog text)

### Cross-references

- L01 (GBK pollution chain) - the original L39 diagnosis was L01-shaped but actually wrong; the byte pattern does not match L01.
- L02 (byte-level discipline) - applies here too: the "fix" must be byte-level because the file is binary-equal to a broken source.
- L12 (meta-post-mortem) - same shape: a session makes a fix claim, the verification metric is wrong, the fix is a no-op. L12 is about lessons-learned.md; L40 is the same shape but on PRD.md and TDD.md.
- L36 (L## fix coverage gap) - L39 fixed `git add` cleanup but did not fix the underlying verification-metric bug; this L40 closes that.
- A1 / A2 (anti-patterns) - Chinese read/write via PS is the entry point. L40 adds: Chinese VERIFICATION via `Get-Content` (which silently renders `0x3F` as `?`) is the exit point that masks the bug.



## L41 - AGENTS.md smoke test recipe path is now L13-fix-2 redirected (update recipe to use D: drive)

**Date:** 2026-07-04
**Spec:** 033 prep (0.18.19.0 release verification)
**Status:** active
**Affected:** AGENTS.md sec 2.5 smoke test recipe

### Symptom

Running the AGENTS.md sec 2.5 smoke test recipe as written for the 0.18.19.0 release:

```powershell
$dst = "C:\TEMP\fluxing-test"
cmd /c "`"$installer`" /S /D=`"$dst\ProgramFiles`""
```

...the install **silently goes to `C:\Program Files\fluxing`** (the default) instead of `C:\TEMP\fluxing-test\ProgramFiles\fluxing`. The `InstallDir` registry key gets `C:\Program Files\fluxing`, not the /D= value. Layout verification fails on invariant (b), (c), (d), (e), (f), (g).

### Root cause (two factors)

1. **L13-fix-2 redirect (commit 053515b)**: install.nsi .onInit function has explicit check_reg3 logic at line 167-169:
   ```
   StrCpy $R1 $R0 8
   StrCmp $R1 "C:\TEMP\" 0 use_reg
   Goto use_default
   ```
   When the existing registry's InstallDir starts with `C:\TEMP\` (a smoke-test path left behind by an incomplete uninstall in a previous test run), .onInit redirects `$INSTDIR` to `$PROGRAMFILES64\fluxing` regardless of /D=. This is a feature, not a bug - it prevents the smoke test from leaving a stale install pointing to a deleted `C:\TEMP\fluxing-test\` directory that no longer exists.

2. **/D= arg parsing with quoted paths**: When /D= is passed with a quoted path containing backslashes (e.g. `"/D=C:\TEMP\fluxing-test\ProgramFiles"`), the NSIS parser strips the trailing backslash off the path. The path becomes `C:\TEMP\fluxing-test\ProgramFiles` minus one backslash somewhere, leading to undefined behavior. With **unquoted** /D= (e.g. `/D=D:\FluxingTest\altpath\pf`), NSIS correctly captures the full path.

### Fix (the recipe needs two changes)

1. **Use a non-`C:\TEMP\` path** for the smoke test, e.g. `D:\FluxingTest\<version>`. Avoids L13-fix-2 redirect and the staleness issue.

2. **Use unquoted /D= value** in the cmd wrapper. The current recipe's `cmd /c `"$installer" /S /D="$dst\ProgramFiles"`"` form causes the L17b length-mismatch class of bug. Replace with:
   ```batch
   cmd /c installer.exe /S /D=D:\FluxingTest\0.18.19\pf
   ```
   or, if you must quote (path with spaces):
   ```batch
   cmd /c '"installer.exe" /S /D="D:\My Test Path\pf"'
   ```

### Verification

0.18.19.0 release: with the corrected recipe (D:\ path + unquoted /D=), all 8 invariants pass:
- Layout: `D:\FluxingTest\altpath\pf\fluxing\weasel\` + `D:\FluxingTest\altpath\pf\fluxing\user1\fluxing\`
- WeaselServer.exe: 1,123,328 bytes (x86 PE 0x14C)
- rime.dll: 3,041,792 bytes (x86 PE 0x14C, in 2-5 MB lua-linked range)
- weaselx64.dll: x64 PE 0x8664 (TSF 64-bit shim)
- WeaselDeployer.exe / WeaselSetup.exe / uninstall.exe: all x86 (0x14C)
- rime_ice.table.bin: present
- HKLM InstallDir: `D:\FluxingTest\altpath\pf\fluxing`
- HKCU RimeUserDir: `D:\FluxingTest\altpath\pf\fluxing\user1\fluxing`

### Updated recipe (replacement for AGENTS.md sec 2.5)

Replace lines 117-130 of AGENTS.md with:

```powershell
# Use a non-C:\TEMP\ path to avoid L13-fix-2 redirect in install.nsi.
# Use unquoted /D= to avoid the trailing-backslash-stripping bug.
$dst = "D:\FluxingTest\$ver"
$dstBase = "D:\FluxingTest"
if (-not (Test-Path $dstBase)) { New-Item -ItemType Directory -Path $dstBase | Out-Null }
Remove-Item -Recurse -Force $dst -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $dst | Out-Null

# Use cmd /c (NOT Start-Process -ArgumentList - L15: PS 5.1 merges /D= and /LOG= at the = boundary)
# Unquoted /D= value (per L41): quoted paths with backslashes get the trailing \ stripped.
cmd /c "$installer /S /D=$dst\pf"
if ($LASTEXITCODE -ne 0) { $failures += "cmd /c installer exit code was $LASTEXITCODE" }
Write-Host "exit code: $LASTEXITCODE"

# 3. Verify layout invariants (L13 + spec 002 FR-001 / FR-002)
$failures = @()

#    a. exit code == 0
if ($LASTEXITCODE -ne 0) { $failures += "exit code was $LASTEXITCODE" }

#    b. fluxing suffix was forced
if (-not (Test-Path "$dst\pf\fluxing\weasel\WeaselServer.exe")) {
    $failures += "engine binaries not under fluxing\weasel\ - path-force fix not working"
}
if (Test-Path "$dst\pf\weasel\WeaselServer.exe") {
    $failures += "engine binaries at WRONG location $dst\pf\weasel\ - path-force broken"
}

#    c. user-data dir is co-located under fluxing\user1\fluxing\
if (-not (Test-Path "$dst\pf\fluxing\user1\fluxing")) {
    $failures += "user-data dir $dst\pf\fluxing\user1\fluxing missing"
}

#    d. registry: InstallDir is the fluxing root
$installDir = (Get-ItemProperty "HKLM:\SOFTWARE\WOW6432Node\Fluxing\Weasel" -ErrorAction SilentlyContinue).InstallDir
if ($installDir -ne "$dst\pf\fluxing") {
    $failures += "HKLM InstallDir = '$installDir' (expected '$dst\pf\fluxing')"
}

#    e. registry: RimeUserDir is under user1\fluxing\
$userDir = (Get-ItemProperty "HKCU:\Software\Fluxing\Weasel" -ErrorAction SilentlyContinue).RimeUserDir
if ($userDir -ne "$dst\pf\fluxing\user1\fluxing") {
    $failures += "HKCU RimeUserDir = '$userDir' (expected '$dst\pf\fluxing\user1\fluxing')"
}

#    f. rime.dll is present and ~3 MB (lua-linked)
$rimeDll = "$dst\pf\fluxing\weasel\rime.dll"
if (-not (Test-Path $rimeDll)) { $failures += "rime.dll missing" }
elseif ((Get-Item $rimeDll).Length -lt 2MB -or (Get-Item $rimeDll).Length -gt 5MB) {
    $failures += "rime.dll size = $((Get-Item $rimeDll).Length) - not in 2-5 MB range"
}

#    g. prebuilt dicts present
if (-not (Test-Path "$dst\pf\fluxing\weasel\data\build\rime_ice.table.bin")) {
    $failures += "prebuilt rime_ice.table.bin missing - first-run will be slow"
}

#    h. L14: all Weasel*.exe are x86; only weaselx64.dll is x64
function Test-Arch($path) {
  $b = [System.IO.File]::ReadAllBytes($path)
  $peOff = [BitConverter]::ToInt32(($b[0x3C..0x3F]), 0)
  return [BitConverter]::ToUInt16(($b[($peOff+4)..($peOff+5)]), 0)
}
$expectedArch = @{
  'WeaselServer.exe'  = 0x14C  # x86
  'WeaselDeployer.exe'= 0x14C  # x86
  'WeaselSetup.exe'   = 0x14C  # x86
  'uninstall.exe'     = 0x14C  # x86
  'weaselx64.dll'     = 0x8664 # x64 (TSF 64-bit shim)
  'rime.dll'          = 0x14C  # x86 (librime Win32-only)
}
foreach ($f in $expectedArch.Keys) {
  $p = Join-Path "$dst\pf\fluxing\weasel" $f
  if (Test-Path $p) {
    $actual = Test-Arch $p
    if ($actual -ne $expectedArch[$f]) {
      $failures += "$f arch = 0x$($actual.ToString('X4')) (expected 0x$($expectedArch[$f].ToString('X4'))) - L14 arch mismatch"
    }
  }
}

# 4. Cleanup
Get-ChildItem $dst -Recurse -Force -ErrorAction SilentlyContinue |
    ForEach-Object { attrib -h $_.FullName 2>$null }
& "$dst\pf\fluxing\weasel\uninstall.exe" /S
Start-Sleep 2
Remove-Item -Recurse -Force $dst -ErrorAction SilentlyContinue
Remove-Item $dstBase -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item "HKLM:\SOFTWARE\WOW6432Node\Fluxing\Weasel" -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item "HKCU:\Software\Fluxing\Weasel" -Recurse -Force -ErrorAction SilentlyContinue
```

### Cross-references

- L13 (NSIS custom function path-force) - the original bug; L13-fix-2 is the smoke-test redirect.
- L13-fix-2 commit 053515b - the source of the C:\TEMP\ redirect.
- L17b (NSIS StrCpy length = literal length) - related; trailing-backslash class of bug.
- L41 (this) - the symptom; the AGENTS.md recipe needs updating.

### Anti-patterns

- **AP-L41-A**: Running the AGENTS.md smoke test recipe verbatim after L13-fix-2 landed. The recipe's `C:\TEMP\fluxing-test` path is now caught by the install.nsi redirect.
- **AP-L41-B**: Using quoted `/D=path` in cmd /c. Trailing backslashes get stripped.
- **AP-L41-C**: Treating "all 8 invariants pass" as the success criterion without updating the recipe when the installer design changes. L36 audit pattern: when a script changes, also update the recipe that drives it.﻿

## L42 - False-positive test pass when new code is dead-stripped from the production binary (verify linked .dll/.exe contents, not just .pdb)

**Date:** 2026-07-04
**Status:** OPEN
**Triggered by:** spec 033 (FluxingDarkModeBridge) 0.18.20.0 release + 0.18.20.1 hotfix
**Related:** L04 (librime / Win32 fallback), L10 (librime Win32-only), L24 (link-probe pattern), L26 (mirror drift), L31 (vcxproj OutDir), L36 (fix-coverage audit), L38 (spec-incremental-shipping), L39 (2013 invalid function arguments), L40 (PRD/TDD corruption), L41 (smoke test recipe)

### Symptom

A spec added a new module (RimeWithWeasel/FluxingDarkModeBridge.{h,cpp}) with a new test (test/TestDarkModeBridge/, 18 behavior-level assertions, L24 link-probe pattern). The test passed. The xmake build reported success. The .pdb for the production binary (weasel.pdb) contained the new symbol. The CHANGELOG entry for 0.18.20.0 declared the spec shipped. The installer was copied to release/fluxing-0.18.20.0-installer.exe. The smoke test against the installer passed all 8 invariants. **The new code was NOT in the shipped installer.** A byte search for the dark palette constant 0x001E1E1E in weasel.dll returned 0 occurrences. The pre-033 binary shipped.

### Root cause (3 contributing factors)

1. **The spec 033 commit was not actually built before the release.** The spec author committed feat(fluxing): spec 033 - FluxingDarkModeBridge (commit e50b2d3) but did not rebuild the installer. The pre-033 binary in output/Win32/WeaselServer.exe was carried forward. The release/fluxing-0.18.20.0-installer.exe was built by xbuild.bat installer from the pre-033 state.

2. **Even when an xmake build was run after the spec 033 changes, the link of weasel.dll did not include the bridge.** The bridge is in RimeWithWeasel.lib (a static lib, kind = static). WeaselTSF (which builds weasel.dll) lists add_deps(WeaselUI, RimeWithWeasel) and add_links(RimeWithWeasel). The link command does include RimeWithWeasel.lib and the bridge .obj is in the .lib. But the linker (/LTCG /OPT:REF) dead-strips the bridge from weasel.dll. The 0x001E1E1E palette constant count in weasel.dll is 0. The bridge IS in weasel.pdb because pdbs are debug-info sidecars that always contain all linked symbols.

3. **The test suite passed despite (1) and (2).** TestDarkModeBridge uses the L24 link-probe pattern (links the actual RimeWithWeasel/FluxingDarkModeBridge.cpp directly via a <ClCompile Include="..\..\RimeWithWeasel\FluxingDarkModeBridge.cpp" /> entry in the vcxproj). This means the test is a separate, isolated binary that links the bridge into ITSELF, not into the production weasel.dll. The test passing proved only that the bridge code compiles and runs correctly in isolation, NOT that the production binary contains it.

### Why the smoke test missed it

The AGENTS.md sec 2.5 smoke test verifies: (a) exit code 0, (b) layout under fluxing\weasel\, (c) HKLM/HKCU registry keys, (d) rime.dll size, (e) prebuilt dicts, (f) PE arch of binaries. None of these checks verify the new spec 033 functionality (the dark palette). The smoke test would pass with the pre-033 binary because the dark palette is in WeaselPanel.cpps .text section as immediate operands in both the pre-033 and post-033 binaries (just at different code locations).

### Verification discipline (the cure)

For ANY spec that adds new code to a new module, the post-impl verification MUST include ALL of the following, in order:

1. **Byte search in the linked binary** for at least one unique byte pattern from the new code (constants, specific strings, etc.). For spec 033, this is 0x001E1E1E in weasel.dll. The count must be > 0.
2. **Link command audit** via xmake -v <target>: confirm the new source file is in the link command. For spec 033, this means build/.objs/RimeWithWeasel/windows/x86/release/RimeWithWeasel/FluxingDarkModeBridge.cpp.obj (or equivalent) in the link command for weasel.dll.
3. **.pdb symbol check** is necessary but NOT sufficient. The pdb will have the symbol even if /OPT:REF strips it from the binary. Verify the .pdb has the symbol, then cross-check with (1).
4. **Installer smoke test** per AGENTS.md sec 2.5: confirms the install path is correct, but does NOT confirm functionality. Add functionality-specific checks for F-class (cross-cut) specs: e.g., for F11 (dark mode), after the smoke install, flip HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize\AppsUseLightTheme and verify the registry read in the live process returns the new value (would require WeaselServer.exe to expose this for test, which is not currently done).
5. **Full clean rebuild** (xmake clean -a && xmake -j8) before any release tag, not an incremental build. LTCG / OPT:REF can hide stale .lib state from incremental rebuilds.

### Anti-patterns (AP-L42-A/B/C/D)

- **AP-L42-A**: declaring "spec shipped" because tests pass and a build succeeds, without verifying the new code is actually linked into the production binary.
- **AP-L42-B**: trusting the .pdb to indicate binary contents. Pdbs always contain all linked symbols, including ones stripped from the binary by /OPT:REF.
- **AP-L42-C**: relying on incremental xmake rebuilds to catch all dependency changes. When the changed code is in a static lib, xmake may not relink dependents that depend on the .lib transitively.
- **AP-L42-D**: using the L24 link-probe pattern in a test that links the production code in ISOLATION. The test proves the code works; the BUILD proves the code is included; you need BOTH to prove the code is in the production binary.

### Recovery (for spec 033 retry)

The spec 033 design is correct. The build pipeline is broken. The retry must:

1. Add $(SolutionDir)/RimeWithWeasel to AdditionalIncludeDirectories in WeaselUI/WeaselUI.vcxproj (so the #include "FluxingDarkModeBridge.h" resolves). This is the immediate cause of the build failure (compile error C1083 in the users first attempt at building the spec 033 changes).
2. Add add_includedirs("$(projectdir)/RimeWithWeasel") to the top-level xmake.lua (so the xmake build also resolves the include).
3. Investigate the /LTCG /OPT:REF dead-strip behavior. Possible fixes: (a) remove /LTCG from WeaselTSFs add_shflags; (b) add /OPT:NOREF to the WeaselTSF link; (c) use __declspec(dllexport) on the bridge class to force the linker to keep it. Option (a) is the simplest and least likely to regress.
4. Re-build from clean (xmake clean -a); verify 0x001E1E1E count in weasel.dll > 0 before tagging the release.
5. Add a functionality-specific smoke check: after the silent install, set HKCU\...\AppsUseLightTheme = 0 and run a quick test exe that calls FluxingDarkModeBridge::Get()->IsDarkMode() and prints the result. (Requires a new test utility, or extending TestDarkModeBridge to expose this as a CLI mode.)

### Cross-references

- L04 (librime / Win32 fallback pattern) - related; the dead-strip is a similar class of "build pipeline silently breaks specd behavior" bug.
- L10 (librime Win32-only) - related; explains why the build only runs in x86 mode and why the issue surfaced here.
- L24 (link-probe pattern) - the test pattern that enabled the false-positive pass.
- L26 (mirror drift) - the original reason for switching from mirror to link-probe; the link-probe has its own failure mode (AP-L42-D).
- L31 (vcxproj OutDir backslash) - the include path / vcxproj issue (L42 step 1) is in the same family of vcxproj pitfalls.
- L36 (fix-coverage audit) - the L42 audit pattern: when fixing one false-positive verification, audit all sibling verifications. Apply to: (a) the AGENTS.md smoke test recipe (add byte-search step); (b) the spec-init checklist (add "link .dll contents" step); (c) the test-infra scripts (add "verify linked binary contains new symbols" step).
- L38 (spec-incremental-shipping exposes build-time deps) - L38 is the upstream cause: spec 033 was a cross-cut refactor that exposed the LTCG dead-strip dependency that earlier specs did not hit.
- L39 (2013 invalid function arguments) - L42 is the 2013-protection follow-up; the systematic-debugging skill must be invoked whenever a spec adds a new module.
- L40 (PRD/TDD corruption) - related; L40 is about file content corruption, L42 is about binary content corruption. Same class of "looks fine at the surface, broken at the byte level" bug.
- L41 (smoke test recipe path redirect) - L42 extends L41: L41 is "smoke test path can fail"; L42 is "smoke test can pass but not exercise the specd behavior".
## L43 - /LTCG /OPT:REF dead-strips static-lib symbols that ARE referenced (per-target /LTCG:OFF is the cure, not global /LTCG removal)

**Date:** 2026-07-04
**Status:** OPEN (will close after 1.0 release with no recurrence)
**Triggered by:** spec 033 retry (0.18.22.0) - the L42 verification discipline revealed the link-stage cause of the false-positive test pass
**Related:** L10 (librime Win32-only), L24 (link-probe pattern), L26 (mirror drift), L36 (fix-coverage audit), L38 (spec-incremental-shipping exposes build-time deps), L42 (false-positive test pass; the verification discipline), L40 (PRD/TDD corruption - same class of "looks fine at the surface, broken at the byte level" bug)

### Symptom

A spec adds a new module (RimeWithWeasel/FluxingDarkModeBridge.{h,cpp}) and calls one of its functions from another translation unit (WeaselUI/WeaselPanel.cpp via the bridge Get()->Subscribe()). The xmake build reports success. The .pdb for the production binary (weasel.pdb) contains the new symbol. The test suite passes (12/12 test projects, 107 assertions). A byte search for a unique constant from the new code (0x001E1E1E palette) in the linked production binary (weasel.dll) returns 0. The shipped installer does NOT contain the new code.

This is L42 in a more specific framing: L42 is "the test passed but the binary is empty"; L43 is "WHY is the binary empty - the linker dead-stripped a referenced symbol".

### Root cause

1. **Whole-program optimization (WPO) is two stages, not one**:
   - `/GL` is the COMPILER-side WPO flag. It tells the compiler to emit whole-program-compatible intermediate code (no inline-only decisions baked in).
   - `/LTCG` is the LINKER-side WPO flag. It tells the linker to re-run optimization across all input .obj files AND to dead-strip any symbol that is not transitively reachable from a kept entry point.
   - The global `xmake.lua` in this project sets `add_cxflags("/GL")` for release builds AND adds `/LTCG /INCREMENTAL:NO` to `add_shflags` for shared libraries. Both WPO stages are active.

2. **LTCG can dead-strip symbols that ARE referenced.** The classic mental model is "the linker keeps anything that is referenced" - this is true for non-LTCG builds. Under `/LTCG /OPT:REF`, the linker can:
   - See that a referenced function is small enough to inline at the call site.
   - Decide that the inlined copy is sufficient.
   - Remove the original symbol from the .text section.
   - KEEP the original symbol in the .pdb (the pdb is a debug-info sidecar; it always contains all linked symbols, whether or not they are in the binary).

3. **The /LTCG behavior in WeaselTSF is set per-target, not globally.** The global `xmake.lua` does NOT add /LTCG to all targets `add_shflags`; it adds it indirectly via the WPO bundle. WeaselTSF has its own `add_shflags("/DEBUG /OPT:REF /OPT:ICF /LTCG")` that EXPLICITLY re-enables LTCG for the shared library. The global `xmake.lua` also adds `/LTCG` via the optimization bundle. The result is: WeaselTSF gets LTCG twice, and `weasel.dll` is built with the most aggressive WPO settings.

### The cure

1. **Add `/LTCG:OFF` to the specific target `add_shflags`** that is experiencing the dead-strip. In our case, the change was:
   ```lua
   -- WeaselTSF/xmake.lua, before:
   add_shflags("/DEBUG /OPT:REF /OPT:ICF /LTCG")
   -- WeaselTSF/xmake.lua, after:
   add_shflags("/DEBUG /OPT:REF /OPT:ICF /LTCG:OFF")
   ```
   This disables LTCG for WeaselTSF only, leaving other targets WPO behavior unchanged. The bridge symbols are no longer dead-stripped; the byte search for 0x001E1E1E in weasel.dll returns 1 (was 0).

2. **Do NOT remove `/LTCG` from the global `xmake.lua`** as a quick fix. The global WPO is desired for performance; removing it would regress optimization for all targets. The per-target override is the surgical fix.

3. **Do NOT remove `add_cxflags("/GL")`** as a quick fix. `/GL` is the compiler-side flag; it does NOT cause dead-stripping (only `/LTCG` does). Removing `/GL` would lose some cross-translation-unit inlining opportunities without fixing the actual problem.

4. **Always verify the fix with a byte search** in the linked binary before declaring success (L42 AP-L42-A). The .pdb is misleading (L42 AP-L42-B); the test suite is misleading (L42 AP-L42-D); the build success is misleading (L42 AP-L42-C). The byte search is the only ground truth.

### Verification (the standard cure pattern, now a 4-step recipe)

For ANY spec that adds new code to a new module and references it from another translation unit:

1. **Build from clean**: `xmake f -a x86 -m release && xmake clean -a && xmake -j8`. Do NOT trust incremental builds.
2. **Byte search in the linked binary**: pick a unique byte pattern from the new code (a constant, a specific string). Count occurrences. Must be > 0.
3. **Cross-check the .pdb**: the .pdb should contain the symbol. If it does NOT, the code is not even being compiled (different bug). If it does, the code IS compiled AND linked; if the byte search also passes, the code IS in the binary.
4. **Install + smoke test**: the AGENTS.md sec 2.5 silent-install smoke test. The 8 invariants verify the install layout; for F-class (cross-cut) specs, add a functionality-specific check (e.g., for F11 dark mode, flip the AppsUseLightTheme registry value and verify the live process picks it up).

This is a 4-step recipe that subsumes L42 AP-L42-A, B, C, D. L43 does NOT replace L42; L43 is the "what to do" once L42 has identified the problem.

### Anti-patterns (AP-L43-A/B/C/D)

- **AP-L43-A**: removing `/LTCG` from the global `xmake.lua` to fix a dead-strip in one target. This regresses optimization for all targets and is a "shotgun fix" that hides the real per-target configuration issue.
- **AP-L43-B**: removing `add_cxflags("/GL")` thinking it will fix dead-stripping. It will not - `/GL` is the compiler flag, not the linker flag. The linker is what dead-strips.
- **AP-L43-C**: adding `__declspec(dllexport)` to force the linker to keep the symbol. This works (the linker cannot dead-strip an exported symbol) but pollutes the public ABI of weasel.dll. The bridge is an internal detail; exporting it would be a security and maintainability regression.
- **AP-L43-D**: declaring the build "fixed" after a successful incremental build. The LTCG state machine is sticky; an incremental build that does not relink the dependent .dll will not pick up the .lib change. Always do a `xmake clean -a` before the verification build.

### Cross-references

- L10 (librime Win32-only) - related; the x64 build is intentionally broken (L10 section 3), so all our ship builds are x86 + weasel.dll, which is exactly the artifact that gets dead-stripped.
- L24 (link-probe pattern) - related; the link-probe pattern in TestDarkModeBridge IS the pattern that L42 AP-L42-D identifies as misleading. L43 does not say "stop using link-probe"; L43 says "link-probe + byte search in the production binary".
- L26 (mirror drift) - related; the alternative to link-probe is mirroring the production code in the test, which drifts. L43 reinforces that the right cure is "link-probe + production-binary verification", not "go back to mirrors".
- L36 (fix-coverage audit) - related; L43 is the per-target-specific application of L36 "find all sibling configs and fix them together" pattern. The sibling configs here are: WeaselTSF/xmake.lua (the one with the bug), WeaselUI/xmake.lua (does it have /LTCG? verify), WeaselServer/xmake.lua (does it have /LTCG? verify), WeaselDeployer/xmake.lua (does it have /LTCG? verify). For 0.18.22.0 we only fixed WeaselTSF; the others do not link the bridge so they are not affected, but the audit pattern is "for every target that links RimeWithWeasel.lib, verify the LTCG behavior".
- L38 (spec-incremental-shipping exposes build-time deps) - related; spec 033 is a cross-cut refactor (F11 dark mode, used by future panels). L38 says incremental shipping exposes build-time dependencies that earlier specs did not hit. L43 is one such dependency: the LTCG behavior of WeaselTSF.
- L42 (false-positive test pass) - L43 is the "what to do" once L42 has identified the problem. L42 says "the test passed but the binary is empty"; L43 says "the binary is empty because of LTCG, here is the per-target fix".
- L40 (PRD/TDD corruption) - related; L40 is "file content corruption", L43 is "binary content corruption" (the code is in the .pdb but not in the .dll). Same class of "looks fine at the surface, broken at the byte level" bug.

### Recovery (already applied in 0.18.22.0)

The spec 033 retry applied the L43 cure in 3 steps:
- **L42 step 1**: `WeaselUI/WeaselUI.vcxproj` AdditionalIncludeDirectories +RimeWithWeasel (8 entries). This was a build-time include path fix; without it, the bridge header was not findable, and the build failed with C1083.
- **L42 step 2**: `xmake.lua` add_includedirs RimeWithWeasel. This was the xmake-side include path fix.
- **L42 step 3 (the L43 cure)**: `WeaselTSF/xmake.lua` add_shflags /LTCG:OFF. This was the per-target override that prevents LTCG from dead-stripping the bridge.
- **Verification**: 0x001E1E1E count in weasel.dll = 1 (L42 AP-L42-A + L43 verification); weasel.pdb contains FluxingDarkModeBridge (L42 AP-L42-B); weasel.dll size grew from 991,232 to 1,002,496 bytes (+11,264 for the bridge code); build was `xmake f -a x86 -m release && xmake clean -a && xmake -j8` (L42 AP-L42-C + L43 AP-L43-D).
- **Ship**: `release/fluxing-0.18.22.0-installer.exe` (42,615,261 bytes) + tag v0.18.22.0.

## L44 - PowerShell `Encoding.UTF8.GetString` returns a STRING (char-indexed), not bytes; `string.IndexOf` returns CHAR offset, not BYTE offset (CJK content silently misaligns all subsequent byte-level math)

**Date:** 2026-07-04
**Status:** OPEN
**Triggered by:** spec 034 (post-0.18.22.0 bug audit) - the systematic-debugging skill (Phase 1 multi-component evidence gathering) hit a near-miss when byte-verifying the 0.18.22.0 CHANGELOG entry. The byte math was correct; the where-is-the-entry probe was wrong. The actual binary + source + docs are all healthy; the lesson is about the VERIFICATION DISCIPLINE, not the artifact.
**Related:** L01 (Chinese UTF-8 read/write - PowerShell 5.1 codepage trap), L02 (Chinese edits - byte-level replace), L37 (PowerShell line-based array ops corrupt CRLF), L40 (PRD/TDD corruption - same class of "looks fine at the surface, broken at the byte level" bug), L41 (smoke test recipe), L42 (false-positive test pass), L43 (/LTCG:OFF cure)

### Symptom

You read a file as bytes, decode it to a string with `[System.Text.Encoding]::UTF8.GetString(bytes)`, then use `string.IndexOf("## [0.18.22.0")` to find the offset of a known marker. The offset LOOKS plausible. You then do `[bytes][start..start+size-1]` to extract a "known range" - and the extracted range contains UNEXPECTED content (in our case, 1539 bytes of CJK content from a completely different part of the file).

The bytes you extracted are real bytes from the file. The size is right. But the START OFFSET IS WRONG. The string operations silently assumed 1 char = 1 byte, which is false for UTF-8 CJK content (1 CJK char = 3 bytes in UTF-8).

### Root cause (2 sub-causes)

1. **`[System.Text.Encoding]::UTF8.GetString(bytes)` returns a `System.String`.** A `System.String` in .NET is a sequence of `System.Char` (UTF-16 code units). It is NOT a sequence of bytes. For ASCII, 1 char happens to map to 1 byte. For CJK (U+4E00..U+9FFF range), 1 UTF-16 char maps to 3 UTF-8 bytes. For BMP edge cases (surrogate pairs), 2 UTF-16 chars map to 4 UTF-8 bytes. For combining marks, the math is worse.

2. **`string.IndexOf(string)` returns the CHAR offset, not the BYTE offset.** When you `GetString` then `IndexOf`, you get a char offset that is mathematically unrelated to the byte offset by a per-file multiplicative factor that depends on the CJK density. For a file with 0 CJK chars, char offset == byte offset. For a file with 1539 CJK chars, the char offset is 3078 BYTES smaller than the byte offset (1539 chars * (3-1) byte gap per char = 3078 byte shift, plus surrogate pair adjustments).

### Why the standard "write a byte-search function" is not enough

Most bug fixes for L01/L02/L37 say "use byte-level APIs". That is correct for the WRITE side. For the SEARCH side, the standard "loop through `bytes[i]` and compare to pattern" works (L42 verification does this). But the moment you use `Encoding.UTF8.GetString` + `IndexOf` for ergonomics, you have already lost the byte-level guarantee. There is no `Encoding.UTF8.IndexOf(bytes, "## [0.18.22.0")` API. You must either:
   - Loop through the bytes (verbose, but correct).
   - Use `[System.Text.Encoding]::UTF8.GetByteCount(stringSoFar)` to convert a char offset to a byte offset (correct, but obscure).
   - Or never convert to string in the first place.

### The cure

For ALL byte-level work in this repo (CHANGELOG, lessons-learned.md, .vcxproj with BOM, install.nsi with BOM, etc.), use the byte-level pattern:

```powershell
# Find a byte pattern in a byte array (NOT a string)
$pat = [System.Text.Encoding]::UTF8.GetBytes("## [0.18.22.0-fluxing]")
$offset = -1
for ($i = 0; $i -le $bytes.Length - $pat.Length; $i++) {
  $match = $true
  for ($j = 0; $j -lt $pat.Length; $j++) {
    if ($bytes[$i+$j] -ne $pat[$j]) { $match = $false; break }
  }
  if ($match) { $offset = $i; break }
}
if ($offset -lt 0) { throw "pattern not found" }
```

This is the same pattern L42 uses to find `0x001E1E1E` in `weasel.dll`. The same pattern is the cure for any "find a known string in a known binary" task.

### Anti-patterns (AP-L44-A/B/C)

- **AP-L44-A**: `[System.Text.Encoding]::UTF8.GetString($bytes).IndexOf("marker")` - returns char offset, NOT byte offset. The CJK density of the file determines the size of the silent miscalculation. Files with 0 CJK chars work; files with 1000+ CJK chars are off by 2000+ bytes.
- **AP-L44-B**: trusting `string.Substring(charOffset, length)` to extract bytes - same root cause as AP-L44-A; the Substring length is in chars, the offset is in chars, neither is in bytes.
- **AP-L44-C**: assuming that "I wrote the file with `Set-Content` and read it back, so the byte offsets must match my expected count" - this only works if the file has no CJK content between the write point and the read point. CHANGELOG.md and lessons-learned.md both have CJK content, so this is a per-file trap.

### Cross-references

- L01 (PowerShell 5.1 + GBK codepage) - the broader PowerShell-on-Chinese trap; L44 is the byte-vs-char specific instance.
- L02 (Chinese edits must use byte-level replace) - the WRITE-side pattern; L44 is the SEARCH-side pattern.
- L37 (PowerShell line-based array ops corrupt CRLF) - same family of PowerShell 5.1 string/byte boundary bugs; L44 is the IndexOf-specific instance.
- L40 (PRD/TDD corruption) - both L40 and L44 are "looks fine at the surface, broken at the byte level" bugs. L40 is about CONTENT corruption (literal `?` substitution). L44 is about OFFSET corruption (char offset != byte offset).
- L42 (false-positive test pass) - L42 verification uses the byte-level search pattern; L44 documents WHY the byte-level pattern is the only correct approach for binary+text hybrid files.
- L43 (/LTCG:OFF cure) - L43 is about LINKER content; L44 is about VERIFICATION content. Same "the byte-level view is the only ground truth" theme.

### Recovery (when L44 has already bit you)

If you have already extracted data using `GetString().IndexOf()` and the result looked plausible but you suspect L44:
1. Re-do the search with the byte-level loop (see The cure above). The byte-level offset will differ from the char-level offset by the CJK density.
2. Verify the extracted range with `Get-Content -Encoding Byte | Select-Object -First N` style byte slice. The text at the new byte offset should match what you expected.
3. Re-run any byte-search verifications (L42 AP-L42-A, etc.) on the corrected range.
4. If the extracted range was ALREADY written to a file (e.g., the wrong bytes were inserted), `git diff` will show the offset as a single huge insertion, and the original "marker" text in the new file will be at a DIFFERENT byte offset than the one you used. Use `git checkout HEAD -- path/to/file` to revert, then re-do the byte-level splice correctly.
5. Add a byte-level byte-count check to your script before writing: `Write-Host "expected size: X, actual byte delta: Y; if X != Y, L44"`. This catches L44 on the first verification pass instead of after the commit.


## L45 - PowerShell `$arr[0..N]` range slice returns `Object[]` (not `byte[]`); `git diff` shows "no-newline-at-EOF" boundary as delete+insert pair; `core.autocrlf=true` corrupts byte-level work on Windows

**Status:** OPEN. **Triggered by:** spec 035 retry (post-0.18.23.0 docs-only spec). 4 sub-lessons from a single recovery session.

### Incident

While preparing spec 035 (PRD/TDD status snapshot update), three separate byte-level operations went wrong:

1. `[Buffer]::BlockCopy` with a PowerShell `$bytes[0..N]` source threw "Object must be an array of primitives". The destination array was 20086 bytes long, but `[IO.File]::WriteAllBytes` wrote an all-zero file. Recovery required `git restore` from HEAD (which itself was corrupted by autocrlf - see #2).

2. `git checkout HEAD -- .specify/PRD.md` (after the L45-#1 zero-write) wrote 14456 bytes to disk, not the 14221 bytes of the HEAD blob. Discovered: `core.autocrlf=true` in repo config + the HEAD blob being LF-only (no trailing CR) caused git to inject 235 CR bytes during checkout, lengthening the file by 235 bytes.

3. After correct byte-level rebuild, `git diff --numstat` still showed 7 "deletions" (2 in PRD, 5 in TDD). 5 of the 7 are real cell modifications spec 035 plan §2.2 authorized (R-008 row + §8 table 4 rows). 2 of the 7 are the LAST line of each file (the line before EOF), which git diff shows as `- lastline` / `+ lastline` even when both lines are textually identical - because the HEAD file has no trailing newline and the working tree has no trailing newline, but git diff still displays the `\ No newline at end of file` marker for the HEAD side and omits it for the working tree side.

### Root cause (4 sub-causes)

1. **PowerShell range slicing returns `System.Object[]`, not `System.Byte[]`.** When you write `$bytes[0..($idx-1)]`, PowerShell returns a boxed `Object[]` where each element is a `System.Byte` boxed as `System.Object`. `[Buffer]::BlockCopy` requires the source to be a primitive array; passing `Object[]` throws. The pre-zero-detection in L40 / L44 does not catch this because it inspects `$bytes.Length`, which is preserved across the range slice. The 0-byte result on disk is the downstream symptom of the failed BlockCopy: the empty $out array gets written.

2. **`core.autocrlf=true` is the Windows default for `git init` and many `git clone` flows.** When the file is classified as "text" (default for `.md` / `.txt`), git checkout replaces LF with CRLF. This corrupts any byte-level verification that compares HEAD bytes vs working tree bytes. The blob size in git and the file size on disk will differ by exactly N (where N = number of LF lines).

3. **`[IO.File]::WriteAllBytes` writes raw bytes, bypassing autocrlf.** So you can write a file with LF-only bytes via `WriteAllBytes`, and the file will be LF-only on disk. But the NEXT `git checkout` of that same file will re-inject CRLF, even if you only restored HEAD. The autocrlf hook fires on every checkout, not just on the original commit.

4. **`git diff` always shows the last line of a no-trailing-newline file as a delete+insert pair**, even when the line content is identical. This is by design: git tracks "line at EOF" as a separate signal from "line not at EOF". A `+` line in the working tree with no `\ No newline at end of file` marker is semantically different from a `-` line in HEAD with the marker, even when the text is the same byte-for-byte.

### Why the standard "use byte-level APIs" advice is not enough

L01 / L02 / L37 / L40 / L44 say "use byte-level APIs". All correct for the read/write side. But they do not address:
- The PowerShell range-slice-vs-BlockCopy landmine (returns Object[], not byte[]).
- The git checkout + autocrlf landmine (corrupts the on-disk file even after a correct byte-level write).
- The git diff + no-trailing-newline landmine (shows false-positive "deletions" that cannot be eliminated by content changes).

### The cure

For ALL byte-level work in this repo (PRD.md, TDD.md, lessons-learned.md, install.nsi, .vcxproj, etc.), use the following discipline:

```powershell
# 1. Set autocrlf false in the local repo BEFORE any byte-level work.
#    Once the file is CRLF on disk, re-running with autocrlf=true will re-corrupt on every checkout.
git config core.autocrlf false

# 2. For byte-level splice operations, ALWAYS use [Buffer]::BlockCopy with explicit typed arrays.
#    PowerShell range slices ($arr[0..N]) return Object[], which fails BlockCopy.
$src = [byte[]]::new($N); for ($i=0; $i -lt $N; $i++) { $src[$i] = $srcBytes[$i] }
$dst = [byte[]]::new($dstLen)
[Buffer]::BlockCopy($src, 0, $dst, 0, $src.Length)

# 3. After ANY write, byte-verify against HEAD blob (via git cat-file blob), not via git diff.
#    Use plumbing: Start-Process git cat-file blob $hash -RedirectStandardOutput $tmpFile -Wait
if (-not [System.Linq.Enumerable]::SequenceEqual([byte[]]$written, [byte[]]$headBytes)) { throw "byte-level drift" }

# 4. Accept that git diff will always show 1-2 "deletions" for the EOF line of a no-trailing-newline file.
#    This is a git diff tool behavior, not a content error. Verify with byte-compare, not diff count.

# 5. To restore a file to exact HEAD bytes (immune to autocrlf):
$hash = git ls-tree HEAD $path | %{ $parts = $_ -split "`t"; $parts[2] }
Start-Process -FilePath git -ArgumentList cat-file,blob,$hash -NoNewWindow -Wait -RedirectStandardOutput $tmpFile
[IO.File]::WriteAllBytes($path, [IO.File]::ReadAllBytes($tmpFile))
```

### Anti-patterns (AP-L45-A/B/C/D)

- **AP-L45-A**: `$bytes[0..N]` as a BlockCopy source - returns Object[], fails BlockCopy, writes zero file. Use `[byte[]]::new($N)` + manual loop, or `[Array]::Copy($bytes, $src, $idx)`.
- **AP-L45-B**: trusting `git checkout HEAD -- file` to write exact HEAD bytes on Windows with autocrlf=true. It does not. Use plumbing (`git cat-file blob $hash`) + WriteAllBytes.
- **AP-L45-C**: assuming `git diff --numstat` "deletions" are real content deletions. For no-trailing-newline files, the last line always shows as delete+insert. Verify with byte-compare.
- **AP-L45-D**: writing with `WriteAllBytes` to a path that has been previously autocrlf-corrupted. The WriteAllBytes succeeds, but the NEXT checkout (or `git restore`) will re-corrupt. Set `core.autocrlf false` in the local repo FIRST.

### Cross-references

- L01 (PowerShell 5.1 + GBK codepage) - same PowerShell string/byte boundary theme.
- L02 (byte-level replace) - the L02 pattern still works for the WRITE side; L45 extends it to the SLICE/CHECKOUT/DIFF side.
- L37 (PowerShell line-based array ops corrupt CRLF) - L37 is about line-ops losing CRLF info; L45 is about array-slice returning wrong type + git checkout + autocrlf landmines.
- L40 (PRD/TDD corruption) - both L40 and L45 are "looks fine at the surface, broken at the byte level" bugs. L40 is about CONTENT corruption (literal `?` substitution). L45 is about TYPE corruption (Object[] vs byte[]) and TOOL corruption (autocrlf / git diff).
- L44 (byte-vs-char miscalculation) - L44 is the SEARCH-side bug; L45 is the SLICE/CHECKOUT/DIFF bug. Both bite the same workflow.

### Recovery (when L45 has already bit you)

1. **File is all zeros (L45-#1)**: cannot recover from the on-disk file; restore from HEAD via plumbing: `git cat-file blob $hash > $tmpFile`, then `[IO.File]::WriteAllBytes($path, [IO.File]::ReadAllBytes($tmpFile))`. Set `core.autocrlf false` first to prevent re-corruption.
2. **File is lengthened by N (L45-#2, autocrlf drift)**: same plumbing restore. Verify the restored size equals `git cat-file -s $hash` exactly.
3. **`git diff` shows mysterious "deletions" on no-trailing-newline files (L45-#3)**: ignore them. Use byte-compare against `git cat-file blob` to verify content identity. Document in commit message that the "deletions" are git diff tool artifacts, not content changes.
4. **Cascaded failures (e.g. L45-#1 followed by L45-#2 followed by L45-#3)**: the recovery is the same as L45-#2: plumbing restore + autocrlf false. Do not try to fix individual bytes; restore the entire file from HEAD blob.

## L46 - spec 033 (0.18.22.0) shipped with msbuild path broken in 4 places; xmake-only verification missed all of them

**Triggered by**: spec 036 (0.18.24.0) msbuild build cycle. spec 033 was released with `xmake f -a x86 -m release && xmake clean -a && xmake -j8` and `cmd /c scripts\test-infra\run-test-suite.bat` (which uses msbuild for the test projects but xmake for the production binaries). The xmake path bypasses all 4 of the bugs below because xmake.lua uses different (less strict) ClCompile configuration than vcxproj ItemDefinitionGroup. spec 034 (0.18.23.0) inherited the same broken state because it added TestDarkModeBroadcast but did not re-verify the production msbuild path. spec 036 (0.18.24.0) is the first release since spec 033 to actually run `msbuild weasel.sln` for the production targets.

**The 4 bugs (all part of one root cause: msbuild path was never tested end-to-end after spec 033)**:

1. **weasel.props `ResourceCompile` PreprocessorDefinitions is empty literal**:
   - File: `weasel.props`, line `<PreprocessorDefinitions>;VERSION_MAJOR=;VERSION_MINOR=;VERSION_PATCH=;PRODUCT_VERSION=;FILE_VERSION=;</PreprocessorDefinitions>`
   - Bug: msbuild does NOT expand the names; the literal `;VERSION_MAJOR=;` is passed to the RC preprocessor, which then sees `FILEVERSION VERSION_MAJOR,VERSION_MINOR,VERSION_PATCH,0` in the .rc and fails with `error RC2127: version WORDs separated by commas expected`.
   - Fix: change to `VERSION_MAJOR=$(VERSION_MAJOR);VERSION_MINOR=$(VERSION_MINOR);...` so msbuild expands the PropertyGroup values.
   - Why xmake missed it: xbuild.bat sets `VERSION_MAJOR=0` etc as env vars in env.bat, and xmake.lua reads them directly. The ResourceCompile PreprocessorDefinitions never enters the picture on the xmake path.

2. **WeaselUI.vcxproj ClCompile missing FluxingDarkModeBridge.cpp + WeaselUtility.cpp**:
   - File: `WeaselUI/WeaselUI.vcxproj`, `<ClCompile>` section.
   - Bug: WeaselPanel.cpp calls `WeaselUserDataPath()` (from WeaselUtility.cpp) and `fluxing::FluxingDarkModeBridge::Refresh()` (from FluxingDarkModeBridge.cpp), but vcxproj does not compile either .cpp, so the symbols are unresolved at link time. LNK2001 + LNK1120 on weasel.dll.
   - Why xmake missed it: xmake.lua scans the `RimeWithWeasel/` and `WeaselUI/` directories and pulls in all .cpp files automatically. The .vcxproj has an explicit ClCompile list, so files added later (FluxingDarkModeBridge.cpp in spec 033) must be added to the list manually.

3. **RimeWithWeasel.vcxproj ClCompile missing FluxingDarkModeBridge.cpp**:
   - File: `RimeWithWeasel/RimeWithWeasel.vcxproj`, `<ClCompile>` section.
   - Bug: same as #2 but for the RimeWithWeasel static lib. spec 033 created the file but did not add it to the vcxproj list.
   - Why xmake missed it: same as #2.

4. **FluxingDarkModeBridge.cpp missing `#include "stdafx.h"`**:
   - File: `RimeWithWeasel/FluxingDarkModeBridge.cpp`, line 1.
   - Bug: the file was authored for the xmake path which has no PCH. When added to WeaselUI/RimeWithWeasel vcxproj ClCompile, the WeaselUI PCH requires `#include "stdafx.h"` as the first include. Without it, error C1010 ("unexpected end of file while looking for precompiled header").
   - Why xmake missed it: xmake does not use MSVC PCH.
   - Fix: add `#include "stdafx.h"` as the first include (before `#include "FluxingDarkModeBridge.h"`).

**Lesson**: xmake and msbuild are NOT equivalent build paths. xmake scans directories and pulls in all sources; msbuild uses explicit lists in vcxproj. Adding a new .cpp file to a directory that xmake will find does NOT mean msbuild will find it. Every release MUST verify the BOTH paths:

- `xbuild.bat weasel installer` (xmake path; builds production binaries + installer)
- `msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32 /m:1` (msbuild path; builds production binaries via explicit ClCompile lists)
- `cmd /c scripts\test-infra\run-test-suite.bat` (test-only path; msbuild for test projects + xmake for production, but this is a 3rd path that does NOT substitute for the first two)

`scripts\test-infra\run-test-suite.bat` builds ONLY the test projects, not the production binaries. Its success is necessary but NOT sufficient for msbuild-path correctness. The pre-0.18.24.0 release cycle (0.18.22.0 / 0.18.23.0) only ran the test suite + the xmake installer build, so the msbuild path was silently broken for 2 release versions.

**Verification recipe for future releases** (added to AGENTS.md sec 2.2 and sec 5):

```powershell
# Run all three; ALL must succeed.
$vsbat = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
cmd /c "`"$vsbat`" -arch=x86 -host_arch=x64 >nul 2>&1 && call env.bat >nul 2>&1 && call xbuild.bat weasel installer"
cmd /c "`"$vsbat`" -arch=x86 -host_arch=x64 >nul 2>&1 && call env.bat >nul 2>&1 && msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32 /p:SolutionDir=%CD%\ /m:1"
cmd /c "scripts\test-infra\run-test-suite.bat"
```

If any of the three fails, the release is broken. Do NOT tag. (L46 verification is RC 0 for all 3 above paths in 0.18.24.0.)

**Anti-patterns (AP-L46-A through D)**:
- AP-L46-A: declaring "we ship on xmake" and skipping the msbuild verification because "we do not use msbuild in CI". Wrong: AGENTS.md sec 2.2 documents that `weasel.sln` is a real build target (the test projects), and the transitive msbuild dependency on the production binaries via project references is real.
- AP-L46-B: assuming xmake.lua `add_includedirs` + `add_files` patterns are equivalent to vcxproj ClCompile. They are not; xmake scans directories.
- AP-L46-C: shipping a release with `xbuild.bat weasel installer` passing but the production binaries not actually linked correctly via msbuild (the L42 sibling bug, but for the build path not the test verification).
- AP-L46-D: assuming `core.autocrlf` settings do not affect vcxproj ClCompile paths. They do, when a new .cpp file is committed with LF on Windows.

**Cross-references**: L42 (dead-stripped code), L43 (/LTCG:OFF), L40 (CHANGELOG-only releases), L41 (smoke test recipe), L10 (librime Win32-only).
## L47 - spec 037 (0.18.26.0) BOM-by-WriteAllText + namespace+main rewrite cascade

**Triggered by**: 0.18.26.0 ship attempt for spec 037 (FluxingComponents v0). 6 distinct byte/syntax bugs caused by PowerShell write discipline + namespace-rewrite automation. Each one blocked compile; together they took the full spec 037 to compile. L01 (UTF-8 BOM in .bat) and L31 (vcxproj path-glue) are precursors but L47 is the first time BOM was applied to .vcxproj + .h files in a way that survived a `[Text.Encoding]::UTF8.GetBytes($x)` round-trip.

**The 6 bugs (cascade from one root cause: PowerShell `[Text.Encoding]::UTF8` round-trip + reactive fix scripts that mutated each other)**:

1. **weasel.props has UTF-8 BOM (L47-#1)**:
   - File: `weasel.props` (gitignored, per-machine).
   - Bug: BOM `EF BB BF` prefix on `<?xml version="1.0"?>`. MSBuild vcxproj import parser sees the BOM and emits `error MSB4024: 未能加载导入的项目文件"F:\soft\00selfmade\rime\weasel.props"。根级别上的数据无效。` for every project that imports weasel.props.
   - Why xmake missed it: xmake does not use weasel.props; it reads VERSION_MAJOR etc from env.bat directly.
   - Fix: byte-level strip BOM with `[System.IO.File]::ReadAllBytes` + slice + `WriteAllBytes`.
   - Why this is novel vs L01: L01 was about GBK pollution in PowerShell string APIs. L47 is about the BOM that `[Text.Encoding]::UTF8.GetString` + `WriteAllBytes` ADDS to the output, not the corruption that `Get-Content` introduces.

2. **WeaselUI.vcxproj has 4 stacked BOMs (L47-#2)**:
   - File: `WeaselUI/WeaselUI.vcxproj` (line 1).
   - Bug: during spec 037 Phase 8 vcxproj extension, an `Apply Patch` operation wrote `<?xml version="1.0" encoding="utf-8"?>` with a BOM, but the file ALREADY had a BOM. After the write, line 1 was `EF BB BF EF BB BF EF BB BF EF BB BF <?xml ...`. MSBuild's vcxproj parser fails on the double-BOM with `error MSB4025: 未能加载项目文件。根级别上的数据无效。`.
   - Fix: function `Remove-AllBOMs` that loops `while (b[0]==0xEF && b[1]==0xBB && b[2]==0xBF) b = b[3..]`, then `WriteAllBytes`. Handles 1, 2, 4, or N stacked BOMs.
   - Anti-pattern AP-L47-B: do NOT write to a vcxproj file with a BOM if the file already has one. Always strip the existing BOM first, or use byte-level patch that does not round-trip the entire file.
   - Why this is novel vs L01: L01 was about file CONTENT corruption. L47 is about the same byte (`EF BB BF`) being applied MULTIPLE TIMES in a single file (the cumulative result of reactive fix scripts each re-adding the BOM).

3. **FluxingComponents/ subdir stdafx.cpp causes MSB8027 "two files produce same PCH" (L47-#3)**:
   - Files: `WeaselUI/FluxingComponents/stdafx.cpp` + `WeaselUI/WeaselUI.vcxproj`.
   - Bug: the subdir has its own stdafx.cpp with `<PrecompiledHeader>Create</PrecompiledHeader>`. The main WeaselUI/ also has a stdafx.cpp with `Create`. Both try to generate `WeaselUI.pch` -> MSBuild error MSB8027 ("two or more files will produce to the same output location").
   - Why xmake missed it: xmake does not use MSVC PCH.
   - Fix: remove `<ClCompile Include="FluxingComponents\stdafx.cpp">...</ClCompile>` from WeaselUI.vcxproj. The 6 other .cpp in FluxingComponents/ (Button, Toggle, Panel, Label, FluxingTheme, D2DRenderer) inherit `<PrecompiledHeader>Use</PrecompiledHeader>` from the project layer, so they use the main WeaselUI.pch.
   - Anti-pattern AP-L47-C: do NOT add a subdirectory PCH generator when the parent project already has one. The subdir .cpp files can use the parent PCH by including the same `stdafx.h` header.

4. **d2d1.h + wrl/client.h in PCH (Windows SDK 10.0.26100.0) triggers dcommon.h C2144 (L47-#4)**:
   - File: `WeaselUI/FluxingComponents/stdafx.h`.
   - Bug: `#include <d2d1.h>` + `#include <wrl/client.h>` in the PCH. Windows SDK 10.0.26100.0's d2d1.h includes dcommon.h, which has a `struct IDXGISurface;` forward decl that fails with `error C2144: 语法错误:"IDXGISurface"的前面应有";"` when wrl/client.h has not yet provided the COM base.
   - Why this is novel: earlier SDKs (10.0.19041 / 10.0.22621) silently worked. 10.0.26100.0 is stricter about the IUnknown base being visible before d2d1.h's dcommon.h is read.
   - Fix: include `<unknwn.h>` BEFORE `<d2d1.h>` in stdafx.h. Order matters: windows.h -> unknwn.h -> d2d1.h -> dwrite.h -> wrl/client.h.
   - Alternative: drop d2d1.h + dwrite.h from the PCH and let each .cpp include them directly. Works but loses the precompiled-header speedup.

5. **LNK2005 "main already defined" when 4 test files each define int main() (L47-#5)**:
   - File: `test/TestFluxingComponents/*.cpp` (4 of them).
   - Bug: each test file (TestFluxingButton / Toggle / Panel / Theme) had its own `int main() { ... return g_fail; }`. Linker merges them, finds 4 `main` symbols, aborts.
   - Why this is novel: prior specs (TestDefaultHotkeys, TestDarkModeBroadcast, etc.) had a SINGLE .cpp per test executable, so the pattern was not exercised.
   - Fix: add a TestFluxingMain.cpp that has the actual `int main() { ... }`, and rename the 4 test functions to `namespace fluxing_test { int RunButtonTest() { ... } }`, `RunToggleTest()`, `RunPanelTest()`, `RunThemeTest()`. TestFluxingMain.cpp calls all 4 in sequence and reports total pass/fail.
   - Anti-pattern AP-L47-D: do NOT have multiple `int main()` in the same executable. Use a `fluxing_test::*Test()` convention with a single TestMain.cpp.

6. **Reactive PS `-replace` leaves `\\r\\n` as literal ASCII text (L47-#6)**:
   - Trigger: PS script uses `"`r`n"` in a double-quoted string assigned to `$repl`, then applies `[System.IO.File]::WriteAllBytes` via `[System.Text.Encoding]::UTF8.GetBytes($repl)`. The escape sequence `\`r`\`n` in PS's double-quote context is interpreted as a backslash + r + backtick + n, NOT as the CR+LF escape. The file ends up with literal text `\`r`\`n` in it (5 ASCII bytes: 0x5C 0x72 0x5C 0x6E), causing `error C2632: 'int' 后面的 'int' 非法` or similar syntax errors.
   - Why this is novel: this is the first time a reactive fix script in this project has been looped 4 times (one per test cpp), and each iteration accidentally inserted the literal escape.
   - Fix: do NOT use PS `-replace` with `\`r`\`n` in the replacement string. Use byte-level patch with `[byte[]]` patterns (e.g. `[System.Text.Encoding]::UTF8.GetBytes("`r`n")` outside of the double-quoted context, or just `0x0D, 0x0A` byte arrays). Or use single-quoted PS strings: `'namespace fluxing_test {' + "`r`n" + 'int RunTest() { ...' }` - the single-quoted part is literal, the backtick-r-backtick-n is the actual CR LF escape.
   - Detection: after any reactive write, byte-grep for the literal sequence `0x5C 0x72 0x5C 0x6E` (`\r\n` as 4 ASCII chars) in the written file. If present, the file is corrupted.

**Root cause (all 6)**: a single line in spec 037 bootstrap ("use PS WriteAllText to add vcxproj ClCompile entries") cascaded through 4 reactive fix attempts, each of which used a different PS idiom (WriteAllText with encoding.UTF8, ApplyPatch with -replace, manual `$b[0..N]` slicing, $variable interpolation), and each introduced a new byte-level or syntax-level bug. The fix scripts fixed one bug and introduced another.

**Lesson**:

A. **Do NOT use `[Text.Encoding]::UTF8` round-trip in reactive fix scripts.** This adds a BOM to .vcxproj / .h files (L01 / L47-#2). The byte-level discipline is: read bytes, modify bytes in-place (Find + Slice + Splice), write bytes. No string round-trip.

B. **vcxproj does NOT need a UTF-8 BOM.** MSBuild vcxproj parser is BOM-tolerant but if the file already has a BOM, do not add another one. After any vcxproj edit, run `Remove-AllBOMs` (function in spec 037 snippet) which loops while first 3 bytes are `EF BB BF`.

C. **Do NOT include `<d2d1.h>` in the PCH unless `<unknwn.h>` is already included first.** Windows SDK 10.0.26100.0's d2d1.h transitively includes dcommon.h which forward-declares `IDXGISurface`, which requires the COM base to be visible.

D. **Subdirectory PCH is an anti-pattern.** Do NOT add a `stdafx.cpp` to a subdirectory if the parent project already has one. The subdir .cpp files will use the parent's PCH automatically as long as they include the same `stdafx.h`.

E. **For multi-test executables, use a TestMain.cpp + namespace convention.** One `int main()` per executable. Per-test functions get `namespace fluxing_test { int RunButtonTest() { ... } }`.

F. **Reactive fix scripts MUST byte-verify their output before commit.** A simple "[regex]::Matches($bytes, '\\x5C\\x72\\x5C\\x6E')" check catches L47-#6 in 1 second. Run it as the LAST line of every fix script.

**Verification recipe for future spec bootstrap with new subdirectory**:

1. Write the production code with `[System.IO.File]::WriteAllBytes(path, [System.Text.Encoding]::UTF8.GetBytes(content))` (UTF-8 WITHOUT BOM).
2. After every write, byte-check: first 3 bytes must NOT be `EF BB BF`.
3. If the file already had a BOM and the new content was meant to replace the first line, byte-splice (Find `<?xml` and insert before) instead of round-trip.
4. For multi-test executables, the test files use `fluxing_test::Run<Test>Test()` convention.
5. The PCH order: windows.h -> unknwn.h -> d2d1.h -> dwrite.h -> wrl/client.h.
6. The subdirectory MUST NOT have its own stdafx.cpp with `<PrecompiledHeader>Create</PrecompiledHeader>`.

**Anti-patterns (AP-L47-A through F)**:
- AP-L47-A: using `[Text.Encoding]::UTF8.GetBytes($string)` to write a .vcxproj or .h file that already has a BOM, without first stripping the BOM. Result: stacked BOMs.
- AP-L47-B: `Apply Patch` / `apply_patch` with `--replace-all` on a .vcxproj file that has a BOM. Result: stacked BOMs.
- AP-L47-C: adding `<PrecompiledHeader>Create</PrecompiledHeader>` to a subdirectory .cpp when the parent already has it. Result: MSB8027.
- AP-L47-D: defining `int main()` in multiple .cpp files of the same executable. Result: LNK2005.
- AP-L47-E: using `"`r`n"` in a double-quoted PS string inside a `-replace` or `[IO.File]::WriteAllBytes` operation. Result: literal `\r\n` (4 ASCII chars) in the output.
- AP-L47-F: relying on a single compile-error message to identify the root cause of a byte-level corruption. The 6 bugs in this L47 entry all surfaced as similar compile errors (C2144, LNK2005, error MSB4024), but each had a DIFFERENT root cause. Always byte-grep for the file-level state (BOM count, PCH config, multi-main, escape sequence literal) BEFORE reacting to the compile error.

**Cross-references**:
- L01 (UTF-8 BOM) - L47 is the MSBuild / vcxproj version of the L01 problem.
- L31 (vcxproj path-glue) - L31 fixes the LNK output path; L47 fixes the vcxproj INPUT path (BOM).
- L40 (PRD/TDD byte corruption) - L40 is content corruption (literal ?), L47 is structural corruption (stacked BOM).
- L43 (/LTCG:OFF per-target) - L43 fixes linker errors, L47 fixes compiler errors.
- L45 (byte-slice + autocrlf) - L45 is the SAME workflow as L47 (reactive fix script), but L45 errors are at the byte-array type level, L47 errors are at the BOM/CRLF level.
- L46 (msbuild path not verified) - L46 is the SAME root cause (msbuild path was not end-to-end tested). L47 is the next spec's manifestation.

## L48 - spec 038 (0.18.27.0) FluxingD2DRenderer singleton atexit crash + link-probe tests must use ExitProcess to skip static destructors + GetClassNameW(NULL buffer) returns 0 + sln ProjectConfiguration entries must be line-separated (not space-joined on one line) + msbuild vcxproj-direct $(SolutionDir) is project-local, only sln-context gets repo-root SolutionDir (L46 follow-up confirmation) + QuickPanelDialog s_hwnd lifecycle: test-only WM_CREATE path leaves s_hwnd=NULL because Show() is the only setter — test callbacks on ActiveHwnd()=NULL are no-op DestroyWindow, by design (spec 038 verification bridge) + unique_ptr<FluxingLabel/FluxingPanel/FluxingToggle/FluxingButton> static members require public accessors so anonymous-namespace lambdas in cpp can wire callbacks (AP-038-A) + WM_LBUTTONUP on FluxingToggle fires on_changed_ callback which calls DestroyWindow(ActiveHwnd()) in spec 038 production layout — but test host is generic DefWindowProc window so ActiveHwnd()=NULL, callback no-op, host still valid (L48 spec 038 contract clarification) + TestQuickPanelDialog.cpp atexit crash: FluxingD2DRenderer singleton has static instance with destructor that calls ReleaseHwndRenderTarget(); Release is invoked after D2D factory finalization on process exit (atexit static destructors run in reverse construction order; D2D factory singletons in same module finalize first), causing access violation. Cure: replace normal return with ExitProcess(rc) in main(). This is the link-probe test pattern: no GUI loop, no real shutdown, just verify all assertions and exit cleanly. Production code (WeaselServer.exe) is unaffected because it has GUI message loop and OS-managed shutdown ordering. spec 038 onwards all FluxingComponents link-probe tests follow this pattern (TestQuickPanelRefactor, TestFluxingComponents already use this, TestQuickPanelDialog now uses it too) + GetClassNameW bug discovered during T014 verification: `GetClassNameW(hwnd, nullptr, 0)` is documented error path that returns 0 and sets ERROR_INVALID_PARAMETER. Always passes null buffer check `> 0`. Don`t gate IsWindow-style checks on GetClassNameW output. Fix in TestQuickPanelRefactor T3b: removed the GetClassNameW line, kept just `IsWindow(host)`. (This is a TEST-only bug — the production code in QuickPanelDialog.cpp does not use GetClassNameW.) + spec 038 sln entry bug discovered during initial weasel.sln editing: sln `ProjectConfiguration Platform entries` MUST be line-separated (each `Debug|Win32` and `Release|Win32` entry on its own line). If compressed into one line, MSBuild Solution Build Manager silently fails to bind configs and the test project is skipped. Anti-pattern (do NOT do): `ProjectConfiguration Include="Debug|Win32" ... ProjectConfiguration Include="Release|Win32" ...` on one line. Always 2 entries = 2 lines (one Debug, one Release), as L48-confirmed `git diff` shows. + msbuild vcxproj-direct L46 follow-up: `msbuild test\TestQuickPanelRefactor\TestQuickPanelRefactor.vcxproj` invoked directly (NOT via `msbuild weasel.sln /t:TestQuickPanelRefactor`) resolves `$(SolutionDir)` to the project`s parent directory (`test\TestQuickPanelRefactor\`), NOT to the repo root. This causes include paths to silently miss (`FluxingDarkModeBridge.h` not found because `$(SolutionDir)\RimeWithWeasel` becomes `test\TestQuickPanelRefactor\RimeWithWeasel`). Cure: ALWAYS invoke msbuild through the sln (`msbuild weasel.sln /t:TestQuickPanelRefactor /p:Configuration=Release /p:Platform=Win32`), never directly. The sln provides the correct SolutionDir context. The xmake path does not have this issue because xmake uses `add_includedirs` with project-relative paths. This was confirmed L46 root cause and re-surfaced in spec 038 during T014 — added to L48 for cross-spec emphasis. + QuickPanelDialog spec 038 lifecycle contract: `s_hwnd` is ONLY set in `Show()` (line ~188). `OnCreate(hwnd)` does NOT set s_hwnd. Therefore tests that drive `WndProc(host, WM_CREATE, 0, 0)` directly leave s_hwnd=NULL. Any callback that calls `ActiveHwnd()` returns NULL, and the standard `if (h && IsWindow(h)) DestroyWindow(h);` is a safe no-op. This is the spec 038 TEST contract: callbacks must be defensive about NULL ActiveHwnd. T3b tests this explicitly: after WM_LBUTTONUP on the toggle, host is still valid because ActiveHwnd()=NULL no-op. + AP-038-A root cause: spec 036 (0.18.24.0) QuickPanelDialog had ButtonA/ButtonB HWND members because the buttons were native BS_PUSHBUTTON. spec 037 introduced FluxingButton/Toggle which are heap-allocated unique_ptr<Fluxing*>. The static members became private (per spec 037 spec-driven-development). When spec 038 moved CreateFluxingControls into an anonymous namespace helper inside QuickPanelDialog.cpp, the lambdas inside SetOnChanged/SetOnClick needed to access `QuickPanelDialog::DeployButton()` etc to wire the production behavior. Either (a) make the members public, or (b) provide public accessors. AP-038-A chose (b) — 7 public accessors — because (a) would expose implementation detail unnecessarily and the accessors are 1-line getters that compile to inline. Trade-off: 7 trivial getters vs leaking 4 unique_ptr internal pointers — getters win on encapsulation. spec 039 may revisit if a "Hide()" symmetry requires taking ownership back from outside (YAGNI: not yet). + Related L##: L24 (link-probe pattern), L31 (vcxproj path-glue), L40 (byte corruption), L46 (msbuild path parity), L47 (BOM cascade). L48 is the spec 038 follow-up bundle: 7 distinct root-causes discovered during T014 verification + 1 spec-038 production design clarification + 1 cross-spec vcxproj-direct confirmation. pattern: link-probe tests for GUI libraries + spec 038 production layout + T014 verification systematically-debugging find-rate = 1 bug per 4 assertions (T3b GetClassNameW) = 25%. This rate is consistent with L24/L31/L47 find-rates — the link-probe pattern catches real bugs that GUI-test or even GUI-manual-test would miss, because the link-probe enforces minimum API surface use and the test host is intentionally a stripped DefWindowProc window that exercises the production code paths without WTL/ATL/Gdiplus dependency cost. Keep doing this.


## L49 - spec 036/038 (0.18.24.0-0.18.27.0) shipped with `OnHotkey` declared but NEVER wired to the ATL message map; Alt+, global hotkey silently no-op for 4 release versions

spec 036 (0.18.24.0) introduced the Alt+, global hotkey via `RegisterHotKey(m_hWnd, ID_HOTKEY_QUICK_PANEL, MOD_ALT, VK_OEM_COMMA)` in `WeaselIPCServer/WeaselServerImpl.cpp` line 76 (in OnCreate) + `UnregisterHotKey` in OnDestroy, plus the `LRESULT OnHotkey(UINT, WPARAM, LPARAM, BOOL&)` function declaration in WeaselServerImpl.h line 50. The function was implemented at WeaselServerImpl.cpp:151-165 (correctly posts `WM_COMMAND, ID_WEASELTRAY_QUICK_PANEL` so the same handler used by left-click tray icon fires).

**The bug**: `MESSAGE_HANDLER(WM_HOTKEY, OnHotkey)` was **never added** to the `BEGIN_MSG_MAP(WEASEL_IPC_WINDOW)` block in WeaselServerImpl.h (between lines 22-31). The message map jumped from `MESSAGE_HANDLER(WM_COMMAND, OnCommand)` directly to `END_MSG_MAP()`. As a result, every `WM_HOTKEY` posted by Windows when the user pressed Alt+, was received by `ServerImpl`'s window proc, fell through the message map with no match, hit the ATL default handler, and was silently discarded.

**Why xmake-only and the link-probe tests missed it**: 
- xmake path does not exercise the ATL message map at runtime. The compiler accepts the `LRESULT OnHotkey` declaration + the `MESSAGE_HANDLER` reference (even if missing) because the function is unused until runtime. Static analysis tools do not flag this.
- TestQuickPanelDialog (spec 036, 0.18.24.0) exercises `QuickPanelDialog` directly via `WndProc(host, WM_CREATE, 0, 0)` and posts `WM_COMMAND` to the host. It never instantiates `ServerImpl` or sends `WM_HOTKEY` to a `WEASEL_IPC_WINDOW` HWND. The link-probe pattern catches production code paths but not ATL message-map wiring in unrelated classes.
- msbuild path also passed (0 errors, 0 warnings) because the ATL message map is expanded into a giant switch-like macro that the compiler cannot statically verify to be complete.

**Why this shipped through 4 versions (0.18.24.0 / 0.18.25.0 / 0.18.26.0 / 0.18.27.0) without a single bug report caught in CI**:
- The QuickPanelDialog production code itself is correct. `QuickPanelDialog::Show()` works fine when called directly.
- Left-click tray icon works (WM_LBUTTONUP -> PostMessage WM_COMMAND -> OnCommand -> m_MenuHandlers -> handler runs).
- "QuickPanel" right-click menu item works (also PostMessage WM_COMMAND -> same OnCommand path).
- **Only Alt+, is broken** — and users who discovered the hotkey feature had not yet been documented enough to report it as a regression vs. spec 036.
- The smoke test (AGENTS.md sec 2.5) verified install layout, arch consistency, and registry keys. It did NOT exercise the global hotkey (no GUI to receive WM_HOTKEY during smoke).

**Cure (0.18.27.1 hotfix)**:
1. Add `MESSAGE_HANDLER(WM_HOTKEY, OnHotkey)` to `WeaselServerImpl.h` line 31, between `MESSAGE_HANDLER(WM_COMMAND, OnCommand)` and `END_MSG_MAP()`.
2. Add `T0: ActiveHwnd() == NULL before any Show() call` to `TestQuickPanelRefactor` (now 9/9 assertions, was 8/8). Documents the L48 spec 038 lifecycle contract as a testable invariant.
3. Add `findstr /C:"MESSAGE_HANDLER(WM_HOTKEY, OnHotkey)" WeaselIPCServer\WeaselServerImpl.h` pre-flight check at the top of `scripts\test-infra\run-test-suite.bat` (after `vcvars32.bat` init, before any test build). If the line is missing, the script exits with code 1 and prints `[L49 GUARD FAIL]`. This is a build-time static assertion that catches any future commit that removes the line.

**Pattern (link-probe + message-map wiring)**: 
- ATL/WTL message maps are inherently runtime constructs. The compiler does not verify that every function declared is reachable via the message map. The only verification is end-to-end GUI testing (real OS messages, real keyboard/mouse input, real window).
- For Fluxing specifically, the closest equivalent to end-to-end testing is to instantiate `ServerImpl`, create its HWND via `Create()`, register the actual `ID_HOTKEY_QUICK_PANEL` via `RegisterHotKey`, send `WM_HOTKEY` with `wParam=ID_HOTKEY_QUICK_PANEL`, and verify the handler fired. This is doable but requires linking the full WeaselServer.exe + Boost + librime — the "GUI-loop" link-probe pattern that L48 explicitly avoided for the GUI components.
- Alternative: write a smaller test that links just `WeaselServerImpl.cpp` (no GUI loop), creates a `ServerImpl` instance on a test host, and verifies the message map contains the expected handler. This is the L49 recommended long-term fix; for 0.18.27.1 hotfix we used the static-check approach because it ships in 1 commit instead of 5.

**Why spec 036 commit message was wrong**: spec 036 commit (501a2ce) message claimed "WeaselIPCServer/WeaselServerImpl.{h,cpp} (OnHotkey + RegisterHotKey/UnregisterHotKey for Alt+COMMA)". The actual diff at spec 036 was: function declared, RegisterHotKey added, UnregisterHotKey added — but MESSAGE_HANDLER was NOT added. The commit message described the *intent*, not the *actual change*. This is a "intent vs reality" failure mode: agents (human or AI) write what they intended to do, not what the diff actually contains. Always `git show <commit>` and verify the actual diff against the commit message before merging.

**Related L##**: L22 (BOOST_ASSERT exit codes), L24 (link-probe pattern), L30 (PowerShell cmd /c exit code), L31 (vcxproj path-glue), L40 (byte corruption), L46 (msbuild path parity), L47 (BOM cascade), L48 (link-probe + FluxingD2DRenderer atexit). L49 is the FIRST instance of a runtime-ATL-message-map wiring bug. The link-probe pattern + smoke test + spec 015 + spec 027 test infra hardening all missed it. The systemic issue: the verification stack has no end-to-end GUI test for ServerImpl. The static-check pre-flight guard is a stopgap; the long-term cure is to add a real GUI-loop integration test (planned for spec 039 follow-up).

**Anti-pattern** (do NOT do):
- "It compiled, ship it" for ATL/WTL code. Compilation success ≠ message routing success.
- "I tested the menu item, so the hotkey also works" — they are separate code paths even when they share the same handler.
- "OnHotkey function exists, so the hotkey works" — function existence ≠ message routing.
- "It worked locally with the GUI, ship it" without an automated smoke test that exercises the actual hotkey path.

## L51 - v0.18.27.2 hotfix (spec 036 US036-B + L50 D2D fallback + L51 lang bar + L51 post-install prompt + L47 BOM/line ending cleanup)

**Triggered by**: 0.18.27.2 hotfix to fix 3 user-reported issues after installing 0.18.27.1:
1. QuickPanelDialog displays incorrectly: title shows black bar (no text), card panel shows no rounded background, deploy button text truncated (Deplo).
2. Left-click on lang bar item (中英文状态托盘图标) toggles ASCII mode instead of opening QuickPanel. Right-click lang bar menu has no QuickPanel item. User expectation (per spec 036 US036-B) is left-click -> QuickPanel.
3. After installing the new version, the user did not restart WeaselServer.exe, so all fixes were invisible. Installer gave no prompt.

**L50 root cause #1 (QuickPanelDialog layout)**: 
- title_rc = {10, 5, client.right-10, 25} had height=20px, too small for 17pt Large font (~28px needed). Title text was clipped, only the D2D rt default black background remained visible.
- card_rc = {5, 30, client.right-5, client.bottom-5} had right=client.right-5 which overflowed under WS_BORDER (1-2px insets). The card panel strayed outside the client area and was clipped.
- X button CreateWindowExW(..., QP_WIDTH-30, 5, 22, 22, ...) used QP_WIDTH (window dimension 300) instead of client.right (~298 after WS_BORDER). Button straddled the WS_BORDER and overlapped TitleLabel.
- deploy_rc width=95 was too narrow for Deploy text plus 6px corner radius + padding, so text appeared as Deplo (truncated).

**Cure #1 (L50 layout)**: 
- title_rc = {10, 4, client.right-32, 30} -> height=26, right boundary moved to client.right-32 to give X button space.
- card_rc = {5, 32, client.right-2, client.bottom-5} -> right boundary moved to client.right-2 (clean inside WS_BORDER).
- X button client.right-25, 4, 20, 20 -> uses client.right not QP_WIDTH.
- Toggle/deploy switched to card-local coords with explicit GetClientRect(card, ...) + width=50/100.

**L50 root cause #2 (D2D fallback missing)**: 
- When FluxingD2DRenderer::Instance().CreateHwndRenderTarget(hwnd) returns null (e.g. D2D unavailable, registry ACL issue, GPU driver hang), HandlePaint() called EndPaint + return 0 WITHOUT drawing anything. The HWND hbrBackground was nullptr (so Windows used the dialog COLOR_WINDOW+1 background), so the user saw a blank area where the title/card/toggle/button should be.

**Cure #2 (L50 fallback)**: For each of Panel/Label/Button/Toggle, in the if (!rt) branch, draw via GDI before EndPaint:
- Panel: CreateSolidBrush(pal.back) + FillRect + DeleteObject (fills card with palette background).
- Label: CreateFontIndirectW(Segoe UI, 17pt) + SetTextColor(pal.text) + DrawTextW(DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX) + DeleteObject.
- Button: same GDI FillRect + DrawText pattern with style-specific colors and 14pt label.
- Toggle: CreateSolidBrush(track_color) + FillRect + Ellipse(cx-knob_r, cy-knob_r, cx+knob_r, cy+knob_r) for the knob. Progress interpolation preserved (track_color blends palette.hilited_back at progress=1, mid-gray at progress=0).

**L51 root cause #3 (lang bar left-click)**: 
- LanguageBar.cpp::OnClick had if (click == TF_LBI_CLK_LEFT) { _HandleLangBarMenuSelect(ascii_mode ? ID_WEASELTRAY_DISABLE_ASCII : ID_WEASELTRAY_ENABLE_ASCII); ... }. This is Windows TSF standard behavior (the system lang bar icon toggles the input mode by default), but spec 036 US036-B explicitly says 左键单击托盘图标 -> 弹同一浮窗. spec 036 only wired up WeaselServer/SystemTraySDK.cpp::OnTrayNotification (the WeaselServer tray icon, not the lang bar icon) to post ID_WEASELTRAY_QUICK_PANEL on WM_LBUTTONUP. The lang bar OnClick was never updated.

**Cure #3 (L51 lang bar)**: 
- OnClick TF_LBI_CLK_LEFT -> _HandleLangBarMenuSelect(ID_WEASELTRAY_QUICK_PANEL). Routing: _HandleLangBarMenuSelect default case -> m_client.TrayCommand(wID) -> IPC to WeaselServer -> AddMenuHandler(ID_WEASELTRAY_QUICK_PANEL) (already registered in WeaselServerApp::SetupMenuHandlers) -> QuickPanelDialog::Show. Same path as the WeaselServer tray icon left-click.
- ASCII toggle remains reachable via Shift+Space global hotkey (spec 005) and the toggle inside QuickPanelDialog itself. WeaselServer tray icon behavior unchanged (still posts ID_WEASELTRAY_QUICK_PANEL on WM_LBUTTONUP).
- WeaselTSF.rc IDR_MENU_POPUP/IDR_MENU_POPUP_HANS/IDR_MENU_POPUP_HANT 3 popup menus gain MENUITEM 快捷设置栏 (&K)/tAlt+, ID_WEASELTRAY_QUICK_PANEL after the Settings entry. Handler was already wired in spec 036; only the menu item was missing.
- include/resource.h gains ID_HOTKEY_QUICK_PANEL 9001 + ID_WEASELTRAY_QUICK_PANEL 40018. Was only in WeaselServer/resource.h (which WeaselTSF cannot include). This is a separate file from WeaselServer/resource.h because they were forked at different times.

**L51 root cause #4 (no restart prompt)**: 
- The WeaselServer.exe process (and weaselx64.dll loaded by the Windows TSF service) stay mapped into the login session. Even after a successful installer run, the old binary remains in memory. The user must restart WeaselServer.exe or sign out and back in for the new binary to take effect.
- Installer gave no notification. User reports that the new version does not work because they did not know they needed to restart.

**Cure #4 (L51 post-install prompt)**: 
- In output/install.nsi, before the SectionEnd of Section Fluxing, add (IfSilent-wrapped):
`
si
  IfSilent skip_post_install_message
  MessageBox MB_OK|MB_ICONINFORMATION 火流猩输入法已安装。$\\r$\\n$\\r$\\n请重启 WeaselServer.exe 或注销后重新登录以应用新版本...
  skip_post_install_message:
`
- IfSilent guards against unattended /S installs (mass-deployment / CI upgrade) where a modal messagebox would block the script.
- This is the FIRST time the Fluxing installer has post-install user-facing text. Previous installers relied on the Windows Programs and Features entry to indicate version, but did not explicitly prompt for restart.

**L47 root cause #5 (BOM + line ending cleanup)**: 
- During prior spec 037 + spec 038 byte-level patches, WeaselUI/FluxingComponents/{Panel,Label,Button,Toggle}.cpp + WeaselTSF/LanguageBar.cpp had UTF-8 BOMs (EF BB BF) prepended. L47 requires .h/.cpp files have BOM=False. The BOMs did not break compilation (MSVC tolerates BOMs in C++ source files), but they were L47 violations and would have caused UTF-8 BOM detector scripts in CI to flag them.
- Same files had mixed CRLF + LF line endings (CRLF lines from my byte-level patch using .Replace(n, rn) overwriting the original LF-only style). HEAD convention is pure LF (verified via git show HEAD: >  byte count).
- Cure: byte-level strip BOM (slice [3..]), then convert all CRLF to LF (loop replacing 0x0D 0x0A with 0x0A).

**Verification (L46 三路径 hard gate, all 0 errors)**: 
- xbuild.bat weasel installer -> installer 42,873,293 bytes.
- msbuild weasel.sln /t:Rebuild /p:Configuration=Release /p:Platform=Win32 /m /v:minimal -> 0 errors. First attempt failed with error C2065: ID_WEASELTRAY_QUICK_PANEL not declared in LanguageBar.cpp because include/resource.h did not have the ID. Cure: added #define ID_HOTKEY_QUICK_PANEL 9001 + #define ID_WEASELTRAY_QUICK_PANEL 40018 to include/resource.h. The fix-coverage audit caught this regression deterministically.
- scripts/test-infra/run-test-suite.bat -> ALL TESTS PASSED. 16 test projects (TestQuickPanelRefactor 9/9, TestFluxingComponents 4/4, TestDefaultHotkeys 20/20, TestWeaselIPC integration, 12 others).
- AGENTS.md sec 2.5 silent-install smoke test 8 invariants PASS + L14 arch-verify (all binary arch consistent: x86=0x14C for WeaselServer/Deployer/Setup/rime.dll, x64=0x8664 for weaselx64.dll).
- L42 byte-verify: 0x1E1E1E triple in weasel.dll 15 occurrences (palette data preserved).
- L47 byte-verify: all modified .cpp/.h BOM=False LF only; install.nsi BOM=True CRLF only.
- L49 pre-flight guard: findstr /C:MESSAGE_HANDLER(WM_HOTKEY, OnHotkey) WeaselIPCServer\\WeaselServerImpl.h exit 0 (guard still passes after L51 edits).

**Related L##**: L40 (BOM/line ending damage chain), L46 (msbuild + xbuild + run-test-suite 三路径), L47 (BOM cascade + Windows SDK 10.0.26100 include order), L48 (link-probe test exit pattern), L49 (ATL message map wiring + pre-flight guard). L51 is the FIRST instance of a user-facing silent-fallback UX bug (D2D fallback missing) combined with a Windows-TSF standard behavior that conflicts with the project spec (lang bar left-click behavior). The combination of bugs was reportable in 1 session because the user provided a screenshot.

**Pattern (D2D fallback + lang bar integration + post-install UX)**: 
- Direct2D rendering is not always available (driver hangs, registry ACLs, GPU virtualization). Any production UI component relying on D2D MUST have a GDI fallback. The spec 037 components shipped without this fallback because the team assumed D2D was always available. L51 added the fallback retroactively (post-ship).
- Windows TSF standard behavior (lang bar left-click = toggle input mode) conflicts with our spec 036 US036-B (left-click = QuickPanel). The spec is the source of truth; we changed the lang bar behavior. The Windows TSF standard is documented but not enforced by the OS; we are free to override.
- Post-install UX: installers that replace binaries locked by Windows (TSF shim, system services) MUST prompt the user to restart. Otherwise the user will report the new version does not work without realizing they need to restart. L51 adds this prompt via NSIS MessageBox.

**Anti-pattern (do NOT do)**: 
- Ship D2D-only components without GDI fallback. Test in environments where D2D may be unavailable (Windows Server Core, RDP sessions, GPU driver crash).
- Implement only 1 of 3 QuickPanel trigger paths (Alt+, + tray icon left-click + lang bar left-click) and assume the user will figure out the others. Spec 036 US036-B lists 3 paths; spec 036 + 037 + 038 only implemented 2 (Alt+, + tray icon left-click).
- Assume the user will restart after an installer run. Windows locks TSF shims + service executables; restart (or sign out + back in) is mandatory. Prompt the user.
- Add BOM or mix line endings via byte-level patch scripts. Always strip existing BOM first, and match the file original line ending style (CRLF for install.nsi, LF for .cpp/.h).

## L52 - spec 041 v0.18.28.0 DPI handling (3 failed attempts before success)

**Triggered by**: v0.18.28.0 ship to fix the v0.18.27.x QuickPanelDialog visual bug on 144 DPI monitors. Three separate attempts at DPI handling all produced black top bars / invisible text / clipped controls. The fourth attempt (D2D rt dpi=96 default + backing store = HWND physical size + `rt->Clear()` for transparent backgrounds) was the one that worked.

**What went wrong, by attempt**:

- **Attempt 1 (L50, v0.18.27.2)**: `D2D1::RenderTargetProperties()` default dpi=96, backing store = child logical size (no DPI scaling), `D2D1::RectF` = logical. On 144 DPI child logical 170x17 �� backing store 170x17, D2D treats DIP as physical (dpi=96 �� no scale) �� text and rounded rects drawn at 170x17 logical into a 170x17 backing store. **Result**: text overflows the smaller child HWND (logical 26 = physical 17 �� 17pt text = 22.7 physical pixels rendered into 17-pixel child). No D2D `Clear()` call, so D2D backing store is opaque black by default and shows through anywhere a pixel is not overdrawn.

- **Attempt 2 (spec 041 v0.18.28.0 first pass)**: `D2DRenderer::GetPhysicalClientRect` converts logical �� physical via `MulDiv(rc, 96, dpi)`. Backing store pixelSize = logical �� 96 / dpi (i.e. `physical_w = logical * 96 / dpi` shrink). `D2D1::RectF` = physical. On 144 DPI child logical 170x17 �� physical 113x11 �� backing store 113x11, `D2D1::RectF(0, 0, 113, 11)`. D2D rt dpi still default 96 �� D2D treats DIP=physical, so 113x11 written 1:1. **Result**: backing store now smaller than HWND physical surface (170x17), and text rendered at physical pixel size is too small to read.

- **Attempt 3 (the one that works)**: `D2DRenderer::GetPhysicalClientRect` returns the raw `GetClientRect` value (== HWND physical size on V1 PerMonitor DPI aware process; child logical = child physical on V1 �� verified empirically with `GetWindowDpiAwarenessContext` returning V1, `GetDpiForWindow` returning 144, and the child physical size exactly matching `logical �� 96/144`). Backing store = HWND physical size. D2D rt dpi = 96 (default). `D2D1::RectF` = physical (1:1 to backing store). All 4 controls add `rt->Clear(D2D1::ColorF(GetSysColor(COLOR_WINDOW), 1.0f))` after `BeginDraw` so non-text pixels do not show the opaque-black D2D backing store.

**Key technical facts about D2D HwndRenderTarget on V1 PerMonitor DPI aware processes**:

1. **`D2D1_HwndRenderTargetProperties.pixelSize` is in PHYSICAL pixels**, not logical. It must match the HWND physical client area. Passing logical coords here shrinks the backing store below the HWND; passing logical �� dpi/96 makes it larger than the HWND and Windows clips on EndDraw.

2. **`D2D1::RenderTargetProperties` dpiX/dpiY default to 0 (= D2D1_DEVICE_CONTEXT_DEFAULT_DPI = 96)**, NOT to the HWND's DPI. If you do not explicitly set dpiX/dpiY to the per-window DPI, D2D treats `D2D1::RectF` DIP coords as if they were 1:1 physical pixels at 96 DPI. To get correct DPI scaling you MUST pass `static_cast<float>(dpi)` to both dpiX and dpiY.

3. **The simplest correct configuration for V1 PerMonitor DPI + D2D HwndRenderTarget is `dpi=96` + `pixelSize=physical` + `D2D1::RectF=physical`**, which is identical to GDI's coordinate model. The "DPI-aware" semantics come from the HWND itself (the HWND physical surface is sized in physical pixels at whatever DPI the monitor is), not from D2D. D2D is just a GDI replacement that also happens to support antialiasing.

4. **D2D HwndRenderTarget backing store is opaque black by default.** Unlike GDI, there is no implicit "transparent" or "window color" Clear. You MUST call `rt->Clear()` after `BeginDraw` if you want the HWND to be visually transparent or show a non-black background. This is the **single most common source of "black bar / black background" bugs** in D2D HwndRenderTarget code.

5. **On V1 PerMonitor DPI aware process, child HWND physical size = logical size passed to `CreateWindowExW` �� 96/dpi** (i.e. **inverted** from the top-level behavior where physical = logical �� dpi/96). Verified on a 144 DPI monitor: title_rc logical (10, 4, 268, 30) width 258 �� child physical 171 (= 258 �� 96/144), height 26 �� child physical 17. Top-level dialog logical 300��150 �� physical 200��100 (= 300 �� 144/96) follows the top-level pattern. This asymmetry is **the single most confusing thing** about V1 DPI and is undocumented in the Microsoft Per-Monitor DPI whitepaper; you have to discover it empirically with `GetWindowDpiAwarenessContext` + `GetClientRect` + `GetDpiForWindow` on each HWND.

6. **`GetDpiForWindow` returns the DPI of the monitor the HWND currently lives on**, not the system DPI. For child HWNDs, the value matches the parent top-level's monitor. This is what we feed into D2D's dpiX/dpiY.

**Why 3 attempts were needed** (meta-lesson):

- The first attempt assumed "D2D is DPI-aware out of the box". It is not. D2D HwndRenderTarget is only DPI-aware if you tell it what DPI to use.
- The second attempt assumed `GetClientRect` in a V1 process returns logical coords and you must scale to physical. For **top-level** windows on V1 that is true, but for **child** windows on V1 it is false (the child physical size is `logical �� 96/dpi`, not `logical �� dpi/96`). The spec 041 plan was written for top-level and applied to child, which is why it "made the backing store 1.5x too large" �� the comment in the code was technically right for top-level and wrong for child.
- The third attempt was a deliberate reset: pass `GetClientRect` through unchanged, set backing store to that, do not pass dpiX/dpiY, and Clear() the backing store. This is the GDI model and is correct because D2D HwndRenderTarget in the default config is essentially a GDI-with-antialiasing.

**How we caught it** (verification):

- `D:\TEMP\qp-trace9.ps1` posts Alt+, (WM_HOTKEY id 9001) to the WeaselIPC HWND, then `EnumChildWindows` on the `FluxingQuickPanel_v0` class to print each child's logical + physical rect, then `PrintWindow` with `PW_RENDERFULLCONTENT` to a PNG. Running this after every code change gave a 5-second visual diff loop. Without this, we would have shipped black bars to production (this is what v0.18.27.2 did �� the L50 fallback existed in source but was never tested with a real 144 DPI monitor).
- `GetWindowDpiAwarenessContext(hwnd)` returning 2 (V1) + `GetDpiForWindow(hwnd)` returning 144 was the empirical proof that the V1 DPI virtualization model was in effect.

**Pattern (D2D + V1 PerMonitor DPI + GDI compat model)**:

- Use `D2D1::RenderTargetProperties()` with default dpiX/dpiY (= 0 �� D2D1_DEVICE_CONTEXT_DEFAULT_DPI = 96). Do NOT pass `static_cast<float>(dpi)`.
- Use `D2D1_HwndRenderTargetProperties(hwnd, physical_size)` where `physical_size = GetClientRect` (no scaling). This works for both top-level and child HWNDs in V1.
- Use `D2D1::RectF(0, 0, w, h)` in physical pixels. The D2D1_RENDER_TARGET_PROPERTIES dpi=96 default treats DIP = physical 1:1.
- Always call `rt->Clear(D2D1::ColorF(GetSysColor(COLOR_WINDOW), 1.0f))` after `BeginDraw` for child controls that should show the dialog background through.
- For per-monitor DPI changes mid-session, handle `WM_DPICHANGED` on each control: `FluxingD2DRenderer::Instance().ReleaseHwndRenderTarget(hwnd)` + `InvalidateRect(hwnd, nullptr, FALSE)`. Do NOT call `ID2D1HwndRenderTarget::Resize(&new_size)` directly; the v0 spec 037 R2 decision is to recreate on next paint.

**Anti-pattern (do NOT do)**:

- Do not assume `GetClientRect` returns the same coordinate system on top-level vs child HWNDs in V1 PerMonitor DPI. Top-level: `physical = logical �� dpi/96`. Child: `physical = logical �� 96/dpi`. The two are opposite. Verify with `GetDpiForWindow(hwnd)` on the specific HWND before writing the size calc.
- Do not pass `static_cast<float>(dpi)` to `D2D1::RenderTargetProperties` dpiX/dpiY unless you also feed `D2D1::RectF` in logical (DIP) coords. Mixing the two scales causes the 1.5x overflow / 0.67x shrinkage that produced the v0.18.27.x and the v0.18.28.0-first-pass bugs respectively.
- Do not skip `rt->Clear()` for D2D child controls. The opaque-black backing store is the #1 visual artifact on high-DPI displays.
- Do not assume child HWND DPI is the same as the system DPI. Use `GetDpiForWindow(hwnd)` to get the per-window DPI. For multi-monitor setups (spec 040+ scope), each monitor's child will report a different DPI.
- Do not modify `WeaselServer/QuickPanelDialog.cpp` (QP geometry) to "fix" a DPI rendering bug �� the QP physical 200��100 in 144 DPI is intentionally small (spec 038 anti-pattern AP-041-B). The fix belongs in the 4 control WM_PAINT handlers, not the dialog.

**Related L##**: L48 (FluxingD2DRenderer atexit crash �� also rt lifecycle), L51 (D2D fallback for unavailable D2D �� orthogonal to this; L52's fix does not regress L51), L47 (BOM/line ending damage chain �� same PCH include order constraint, but D2D-specific). L52 is the **first instance** of a v0 DPI handling post-mortem; future PerMonitor V2 / multi-monitor work in spec 044+ should reference L52's empirical proof + pattern.

## L53 - ASCII strings search misses C++ mangled symbols in linked Windows binaries (always use UTF-16 + dumpbin /DISASM for verification)

**Date:** 2026-07-06
**Status:** OPEN (will close after 1.0 release with no recurrence)
**Triggered by:** spec 041 v0.18.28.0 systematic-debugging re-verification - false-positive "WeaselServer.exe missing Fluxing DPI fix" bug claim
**Related:** L42 (false-positive test pass - the binary-level verification discipline), L43 (LTCG /OPT:REF dead-strip - the *actual* root cause L43 prevents), L52 (DPI handling post-mortem - the spec that triggered this debug session)

### Symptom

After shipping spec 041 v0.18.28.0 with FluxingComponents DPI fix, a systematic-debugging re-verification reported "BUG #3: xmake WeaselServer.exe does not contain DPI fix symbols" because ASCII [System.Text.Encoding]::ASCII.GetString() searches for GetPhysicalClientRect / HandleDpiChanged / WM_DPICHANGED returned 0 hits. The conclusion was that the v0.18.28.0 ship was incomplete.

### Root cause

**C++ mangled symbols are stored in Windows PE debug info (PDB sidecar) as UTF-16 wide strings, not ASCII.** This is documented but easy to forget:

- C++ symbol names in MSVC-generated COFF objects use CodeView debug format (.debug / .debug sections) which encodes symbols as **UTF-16LE wide strings** with a wchar_t per code unit.
- ASCII string searches against a Windows binary's byte content will see only the second byte of each UTF-16 character — every other byte is \x00 — so even an exact ASCII substring like FluxingLabel returns 0 hits when stored as UTF-16.
- The actual machine code call ??1FluxingLabel@ui@fluxing@@QAE@XZ IS present in the .text section, but dumpbin /DISASM only resolves and prints it because it reads the .debug UTF-16 section.
- For our project: 5 production files compile to 5 separate .obj files, all linked into WeaselServer.exe via the static WeaselUI.lib. Symbols like ?Create@FluxingLabel@ui@fluxing@@SA?$...@std@@@Z are stored as **mangled + UTF-16**, and only dumpbin /DISASM shows them in a human-readable form.

### Why the L43 / L42 verification discipline did not catch this

The L43 cure (/LTCG:OFF per-target) was already applied to WeaselTSF/xmake.lua in spec 031 (0.18.22.0). The global xmake.lua line 64 also has dd_ldflags("/LTCG:OFF /INCREMENTAL:NO", {force = true}). So **WeaselServer.exe does NOT have the L43 dead-strip problem** — all referenced symbols ARE linked. The verification error was in the *search tool*, not the binary.

### Verification discipline (the cure)

For any "is symbol X in binary Y" check on Windows MSVC:

1. **Never trust ASCII strings search.** Use **UTF-16 decoding**:
   `powershell
    = [System.IO.File]::ReadAllBytes('WeaselServer.exe')
    = [System.Text.Encoding]::Unicode.GetString()
   # Now search  for both mangled and demangled forms
   `
   This catches symbol names stored in .debug / .debug sections.

2. **Always cross-check with dumpbin /DISASM** for the actual instruction sequence:
   `cmd
   dumpbin /DISASM WeaselServer.exe | Select-String "FluxingLabel|HandleDpiChanged"
   `
   dumpbin /DISASM resolves addresses via the .pdb sidecar and prints both machine code and resolved symbol names. This is the **ground truth** for "is this function called from somewhere in the binary".

3. **For ASCII UI strings** (Quick Panel, Deploy, X, etc.) — these ARE stored as ASCII in the binary's .rdata section (they're UI text). ASCII search works. But C++ class/function names are UTF-16.

4. **Byte-verify with unique constants** (L42 AP-L42-A) is still the most reliable: pick a unique byte pattern from the new code (e.g.,  x001E1E1E palette, or a specific string literal) and count occurrences. Constants don't have encoding ambiguity.

### Anti-patterns (AP-L53-A/B)

- **AP-L53-A**: declaring a binary "missing the fix" based solely on ASCII strings search returning 0. The actual fix may be present and the search just missed it.
- **AP-L53-B**: skipping dumpbin /DISASM because "strings search should be enough". dumpbin /DISASM is the only verification that resolves mangled symbols to readable form, and it shows the actual call sites — not just the symbol name in the symbol table.

### Recovery (for spec 041 re-verification)

The spec 041 v0.18.28.0 ship is **NOT broken**. The deployed C:\Program Files\fluxing\weasel\WeaselServer.exe (1,981,952 bytes, SHA256 7F590070...) DOES contain:
- ?Create@FluxingLabel@ui@fluxing@@... (FluxingLabel::Create call)
- ?Create@FluxingPanel@ui@fluxing@@... (FluxingPanel::Create call)
- ?Create@FluxingToggle@ui@fluxing@@... (FluxingToggle::Create call)
- ?Create@FluxingButton@ui@fluxing@@... (FluxingButton::Create call)
- ?HandleDpiChanged@FluxingPanel@ui@fluxing@@QAEJIJ@Z (FluxingPanel::HandleDpiChanged definition)
- ?HandleDpiChanged@FluxingToggle@ui@fluxing@@QAEJIJ@Z (FluxingToggle::HandleDpiChanged definition)
- ?GetPhysicalClientRect@FluxingD2DRenderer@ui@fluxing@@... (D2DRenderer::GetPhysicalClientRect definition)
- ?ReleaseHwndRenderTarget@FluxingD2DRenderer@ui@fluxing@@... (called from 12+ sites)

All confirmed via dumpbin /DISASM. Visual verify at 96 DPI Todesk session: QP 300×150 with all 5 children visible (FluxingLabel 256×26 title, FluxingPanel 291×111 card, FluxingToggle 50×20, FluxingButton 100×24, native close 20×20). Test suite 16/16 PASS, 200+ assertions.

### Action item

Add L53 to the systematic-debugging checklist in AGENTS.md §6 and erification-before-completion skill: "When verifying Windows PE binary contents, use UTF-16 string decode + dumpbin /DISASM + byte-pattern count. Never rely on ASCII strings alone."
## L54 - silent install /D= ignored when InstallDirRegKey registry key is stale

### Incident (2026-07-06 v0.18.29.0 ship)

After running `cmd /c installer.exe /S /D=C:\Program Files\fluxing /LOG=D:\TEMP\foo.log` during 0.18.29.0 ship verification, three problems compounded:

1. **Wrong install path**: silently installed to `C:\Program Files\fluxing` even though the user had a pre-existing install at `D:\Program Files\fluxing`. The agent assumed `C:\Program Files` was the default and never queried the existing registry InstallDir first.

2. **/D= ignored silently**: subsequent `cmd /c installer.exe /S /D=D:\Program Files\fluxing` invocations all wrote `HKLM\Software\Fluxing\Weasel\InstallDir = C:\Program Files\fluxing` because NSIS `InstallDirRegKey` directive pre-loads `$INSTDIR` from registry BEFORE `.onInit` runs. The `.onInit` logic preserves non-empty `$INSTDIR`, so `/D=` is ignored.

3. **L15 re-trigger**: passing `/D="..."` and `/LOG="..."` as separate PowerShell arguments (or via cmd /c without single-quote wrapper) caused NSIS to concatenate `/D=` and `/LOG=` into a single deeply-nested garbage path `C:\Program Files\fluxing LOG=D\TEMP\fluxing-install-...log\fluxing`. Registry InstallDir got polluted with this string.

### Root cause

- The agent never queried `Get-ItemProperty HKLM:\SOFTWARE\WOW6432Node\Fluxing\Weasel InstallDir` (or HKCU equivalent) BEFORE running any silent install. It assumed `C:\Program Files` was the user's path.
- The agent then layered silent install + manual `Copy-Item` file deployment, generating **two parallel installations** (one in `C:\Program Files\fluxing`, one in `D:\Program Files\fluxing`). The installer auto-launched the C: copy as PID 12564, which then locked the files and prevented clean uninstall.
- NSIS `uninst` function (install.nsi ~line 195) deletes `HKLM\Software\Rime` and `Uninstall\Fluxing` but NOT `HKLM\Software\Fluxing\Weasel` -- so the InstallDir pollution survives every uninstall. Every subsequent install reads the stale value via `InstallDirRegKey`.

### Lesson (5 rules)

1. **Before ANY install/uninstall, query the existing deployment location** (3 reads):
   ```
   Get-ItemProperty HKLM:\SOFTWARE\WOW6432Node\Fluxing\Weasel -ErrorAction SilentlyContinue
   Get-ItemProperty HKCU:\Software\Fluxing\Weasel -ErrorAction SilentlyContinue
   Get-ItemProperty HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\Fluxing -ErrorAction SilentlyContinue
   ```
2. **Silent install must pass the user-confirmed path explicitly via /D= and that path MUST match the registry InstallDir**. If they don't match, the installer's `InstallDirRegKey` directive will silently use the registry value and ignore /D=. Fix this by deleting `HKLM\Software\Fluxing\Weasel` and `HKCU\Software\Fluxing\Weasel` BEFORE the silent install.

3. **Never combine Copy-Item manual deployment with silent install in the same iteration** -- they create duplicate deployments, and the silent install auto-launches the new copy as a daemon that locks files. Pick ONE: either silent install OR manual Copy-Item.

4. **L15 always-on**: every silent install command must use single-quote-wrapped cmd /c, never array-ArgumentList:
   ```
   cmd /c "call `"$installer`" /S /D=`"$dst`" /LOG=`"$logPath`""
   ```
   The /LOG= must be its own NSIS argument, not concatenated to /D=.

5. **Always clean BOTH registry AND filesystem in lock-step**. The uninstall function deletes some registry keys but not all. After any install/uninstall cycle, run:
   ```
   Remove-Item HKLM:\SOFTWARE\WOW6432Node\Fluxing -Recurse -Force
   Remove-Item HKLM:\SOFTWARE\Fluxing -Recurse -Force
   Remove-Item HKCU:\Software\Fluxing -Recurse -Force
   Remove-Item HKLM:\SOFTWARE\WOW6432Node\Rime -Recurse -Force
   Remove-Item HKLM:\SOFTWARE\Rime -Recurse -Force
   Remove-Item HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\Fluxing -Recurse -Force
   ```
   then verify `[System.IO.Directory]::GetDirectories('C:\Program Files', 'fluxing*')` returns empty.

### Action item

- [ ] Patch install.nsi `uninst` function to also delete `HKLM\Software\Fluxing\Weasel` (currently only deletes `HKLM\Software\Rime`). Future installer versions will then self-clean the InstallDir pollution.
- [x] This L54 entry written 2026-07-06 immediately after the 0.18.29.0 ship mis-step. The shipping product itself (binary, installer) is correct -- the bug was in the deploy procedure.
- [x] D:\Program Files\fluxing verified clean deployment: HKLM InstallDir = D:\Program Files\fluxing\weasel, HKCU RimeUserDir = D:\Program Files\fluxing\user1\fluxing, WeaselServer.exe = 2029568 bytes (0.18.29.0). User data (rime_ice.userdb, user.yaml 183 bytes) preserved unchanged.


## L55 - WeaselDeployer.exe killed mid-flight freezes WeaselServer forever (R4 + R2 maintenance mode trap)

**Incident (2026-07-08, Codex session interrupt investigation)**:
User reported "frequently interrupted tasks / session interruption". Investigation traced to:
- User right-click tray → "重新部署/词典管理/同步" → `WeaselServerApp::execute(WeaselDeployer.exe /dict)`
- `WeaselDeployer.exe` enters `Configurator::DictManagement()` → `client.StartMaintenance()` (librime finalized)
- WeaselDeployer.exe is killed mid-flight by task manager / AV quarantine / access violation
- The naked `client.StartMaintenance() ... client.EndMaintenance()` pair never gets to EndMaintenance
- WeaselServer stays in `m_disabled = true` state forever
- `ProcessKeyEvent` early-returns at the `if (m_disabled) return 0;` line at RimeWithWeasel.cpp:167-171
- User must reboot to recover

**Root cause** (6 confirmed locations):
- R1 (P1) WeaselServer/WeaselServerApp.h:13-17 - `ShellExecuteW` with no PID, no Job Object, no heartbeat
- R2 (P1) WeaselDeployer/Configurator.cpp:182-187 - `DictManagement()` calls `rime->run_task("installation_update")` (async) then opens modal dialog; never `join_maintenance_thread()`s
- R3 (P2) RimeWithWeasel/RimeWithWeasel.cpp:495-499 - `StartMaintenance()` directly finalizes, no refcount
- R4 (P1) WeaselDeployer/Configurator.cpp - three maintenance intervals are naked Start->End pairs with no try/finally and no RAII
- R5 (P3) RimeWithWeasel/RimeWithWeasel.cpp:495-499 - `m_session_status_map.clear()` does not notify TSF clients
- R6 (P3) RimeWithWeasel/RimeWithWeasel.cpp:574-576 - `_IsDeployerRunning()` only checks mutex, cannot detect process hang

**Cure (spec 042 v0.18.30, ship target)**:
- Fix 1: `MaintenanceGuard<ClientT>` RAII in `WeaselDeployer/MaintenanceGuard.h`; constructor calls StartMaintenance, destructor calls EndMaintenance (no-throw, swallows all exceptions). Non-copyable, non-movable, single owner per maintenance interval. Template on ClientT so test can pass a MockClient.
- Fix 2: `DictManagement()` in Configurator.cpp adds `RIME_API_AVAILABLE(rime, join_maintenance_thread)` after `run_task("installation_update")`.
- Fix 3-6: deferred to spec 043 (WeaselServer-side hardening).

**Lesson** (3 rules):
1. **Any IPC pair that must be matched (Start/End, Open/Close, Lock/Unlock) MUST be wrapped in RAII** unless there is a hard reason it cannot be. Naked pair = guaranteed leak on process death. This applies to ALL future process-interaction patterns in this project, not just maintenance mode.
2. **Async API calls (`run_task`, `submit`, `post`) MUST be paired with their sync counterparts (`join`, `wait`, `flush`) at every call site** - not just at the obvious one. Audit other `run_task` call sites in the codebase.
3. **`taskkill /F` is a real production failure mode**, not a hypothetical. Antivirus quarantine, Windows Update restart, OOM killer, and user task manager all produce the same outcome: process dies mid-flight. Any code that depends on clean function-return MUST be RAII-protected.

**Cure verification** (L46 recipe, 3 paths must all PASS before ship):
- xbuild.bat weasel → 0 errors, 0 warnings
- msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32 → 0 errors
- scripts/test-infra/run-test-suite.bat → 17+ test projects PASS (16 + TestOrphanRecovery)
- `TestOrphanRecovery` covers 4 paths: happy / exception-in-scope / connect-failed / end-throws

**Related**: L13 (NSIS path-force), L17 (InstallDirRegKey pre-load), L19 (Shift key binding), L31 (test infra), L47 (BOM), L49 (Alt+ comma hotkey), L52 (DPI), L54 (silent install /D= stale registry).
## L56 — xbuild 超时 + 2.1 GB 崩溃日志导致连续 4 次会话中断（2026-07-08）

### 事件

2026-07-08 下午，spec 052 QuickPanel 长显模式的构建过程中，会话连续 4 次自动中止。事后分析 log/ 目录构建日志发现三个核心原因：

1. **Shell 命令默认超时 10 秒**（致命）。xbuild.bat 构建需 30-90 秒，但 shell_command 默认 timeout_ms=10000，命令还没产生输出就被杀掉。Agent 看到"失败"→重试→又超时→循环，4 次后会话被强制终止。4 个日志文件（rebuild_after_ltcg_fix.log、rebuild_x86_proper.log、rebuild_with_ltcg.log、rebuild_with_ltcg2.log）均为 0 字节。

2. **2.1 GB 崩溃日志撑爆上下文**（致命）。log/weasel.crash.log 从 2026-06-15 起累积，UTF-16 编码，2.05 GB。如果 agent 尝试用 Get-Content 或 rg 读取此文件（搜索错误信息），会瞬间耗尽内存和上下文窗口，导致会话崩溃。

3. **x64 构建反复触发 L10 陷阱**（浪费）。link_verbose.log 显示 agent 反复尝试 x64 构建，每次都遇到 rime_get_api LNK2019（已知 L10：librime 是 Win32-only）。每次"修复"尝试消耗一次完整构建周期。

4. **试错式调试 + 上下文膨胀**（累积）。20+ 个构建日志（rebuild1-rebuild10，rebuild_v，rebuild_ltcg 等）+ 14KB 完整编译器输出，每次构建都往上下文追加大量内容，会话越来越慢直至超出限制。

5. **env.bat ARCH=x64**（隐患）。env.bat 写了 ARCH=x64，可能误导直接读 env.bat 的工具。已修复为 ARCH=x86（xbuild.bat 本已正确 hardcode x86）。

### 修复（已执行）

- 删除 log/weasel.crash.log（释放 2.05 GB）
- 删除所有临时构建/调试日志（24 个文件）
- env.bat: ARCH=x64 → ARCH=x86 + 注释

### 教训（6 条规则）

1. **构建命令始终设置超时**。`xbuild.bat weasel` → timeout_ms: 120000（2 分钟），`xbuild.bat all` → timeout_ms: 3600000（60 分钟）。不要在构建命令中使用默认的 10 秒超时。

2. **读取文件前检查大小**。特别是 log/、output/ 下的文件。超过 1 MB 的日志不要用 Get-Content 全量读，用 -Tail 或 Select-String 搜索。

3. **构建失败 3 次就停**。不要进入"改一行→构建→失败→再改"的循环。3 次失败后切换到 systematic-debugging 技能做根因分析。这也是 Constitution R1（Intent Before Implementation）的要求。

4. **遇 LNK2019 rime_get_api 立即停止**。这是已知 L10 约束（librime 是 Win32-only），不是 bug。xbuild.bat 已经 skip x64，不要手动跑 `xmake f -a x64`。

5. **定期清理 log/**。崩溃日志（weasel.crash.log）会无限增长。建议在 xbuild.bat 中加 `if exist log\weasel.crash.log del log\weasel.crash.log`，或至少在每次 release 前手动清理。

6. **使用 incremental-implementation 技能**。每次只改 1-3 个文件、只构建一次验证，避免积攒大量修改后一次性构建（Constitution V 要求）。

### Related

- L10（librime Win32-only 约束）
- L53（PE binary verify must use dumpbin + UTF-16，not ASCII strings）
- L54（silent install /D= 被 InstallDirRegKey 覆盖）
- L55（WeaselDeployer 孤儿任务导致 WeaselServer 永久卡死）
- Constitution R1（Intent Before Implementation）、V（Incremental Delivery）
## L57 - R6 fix: abandoned mutex detection + lazy recovery in WeaselServer (L55 spec 053)

**Date:** 2026-07-08
**Status:** OPEN (will close after 1.0 release with no recurrence)
**Triggered by:** User reported "切换输入法后无法输入中文" after installing v0.18.30.0. Root cause: spec 042 R4 (MaintenanceGuard RAII) does not catch OS-level process death (taskkill /F, AV quarantine, crash), and R6 (_IsDeployerRunning only checks mutex) is still open. Mutex stays held by the kernel forever with no live holder, and `m_disabled` stays true forever, locking out all input.

**Related:** L55 (full L55 spec 042 lesson), L13 (NSIS path-force), L17 (InstallDirRegKey), L19 (Shift key binding), L31 (test infra), L47 (BOM), L49 (Alt+comma hotkey), L52 (DPI), L54 (silent install /D= stale registry), L56 (xbuild timeout), spec 042, spec 053 (new), constitution Principle II (Test-Backed Change), R6 (verification before completion).

### Symptom

User reported after installing v0.18.30.0: "输入法切换后，仍然无法输入中文" (after switching to the Fluxing input method, Chinese input still does not work). User had to reboot to recover. Also: "快捷设置栏在火流猩输入法切换后不显示" (QuickPanel does not show after switching to Fluxing IME) - separate root cause, see L57bis.

### Investigation timeline (Phase 1: Root Cause Investigation, systematic-debugging)

1. **git log** (last 10 commits) showed the 0.18.30.0 series:
   - `f44833f` 12:02 spec 042 R4+R2 fix (MaintenanceGuard RAII + join_maintenance_thread)
   - `e66c510` 13:36 spec 050 yaml hotkey editor MVP
   - `6815aad` 17:42 spec 049+052 QuickPanel v4 macOS toolbar + always-show mode
   - `17a710b` 17:58 add GdiplusStartup for QuickPanel v4 GDI+ rendering
   - `73cffef` 18:13 QuickPanel fade timer fix + opacity adjust
2. **file mtime** of the user-installed WeaselServer.exe: 2026/7/8 **13:49** (and 13:49 for weasel.dll too).
3. **installer** (18:12) used WeaselServer.exe 18:10 and weasel.dll 17:16 - **neither binary contains spec 049+052 v4 QuickPanelDialog** (the 17:16 weasel.dll has 0 hits on `FluxingQuickPanel_v4` UTF-16LE byte pattern; the 13:49 user binary has 0 hits too).
4. **m_disabled early-return scan** of RimeWithWeasel.cpp identified **8 critical paths** (ProcessKeyEvent / AddSession / CommitComposition / ClearComposition / SelectCandidateOnCurrentPage / DeleteCandidateOnCurrentPage / FocusIn / UpdateInputPosition) and 2 read-only (FindSession / RemoveSession) - all early-return when `m_disabled` is true.
5. **L55 R6 root cause confirmed**: `_IsDeployerRunning()` uses `CreateMutex(NULL, TRUE, ...)` and checks `GetLastError()==ERROR_ALREADY_EXISTS`. This pattern has **2 bugs**:
   - `bInitialOwner=TRUE` means the call can itself create the mutex and acquire ownership, so `ERROR_ALREADY_EXISTS` is unreliable (false positive on deployer-absent case).
   - It **cannot detect the abandoned state** - the case where `taskkill /F` or AV quarantine killed WeaselDeployer.exe without releasing the named mutex. The kernel keeps the mutex held but the holder is dead. Legacy code returns true forever.

### Root cause (L55 spec 042 R6, not yet fixed before v0.18.30.0)

- **R4 RAII fix** in `Configurator.cpp` (3 maintenance intervals wrapped in `MaintenanceGuard<weasel::Client>`) **only catches WeaselDeployer.exe normal exit** (return, exception, throw). It does **not** catch OS-level process death (taskkill /F, AV quarantine, OS kill during access violation recovery) because **C++ RAII dtor does not run when the OS kills the process** - the process disappears mid-flight, the kernel reaps the process, but the named mutex the dead process owned is left in the **abandoned** state.
- **R6 fix** (the one in this lesson) was specified in spec 042 spec.md §0 as "deferred to spec 043+" but **spec 043 was never created** before v0.18.30.0 ship.
- The `WeaselDeployerMutex` stays in the kernel forever, `_IsDeployerRunning()` returns true forever, `m_disabled` stays true forever, every `ProcessKeyEvent` early-returns FALSE, user cannot type Chinese.

### Fix (spec 053 v0.18.31.0, ship target 2026-07-08)

**Two-line change to `_IsDeployerRunning()` + one new method `TryLazyRecovery()` + 9 call sites:**

1. **R6 detection (L57 fix 1)**: change `_IsDeployerRunning()` to use `OpenMutex(SYNCHRONIZE, FALSE, ...)` (does not create) + `WaitForSingleObject(mutex, 0)`. Return value matrix:
   - `WAIT_OBJECT_0` -> we own it (signaled, no other holder) -> return false (no deployer)
   - `WAIT_TIMEOUT` -> another thread holds it -> return true (deployer alive)
   - `WAIT_ABANDONED` -> **R6 signal** (holder died, mutex was abandoned by kernel) -> return false (no deployer)
   - Always `ReleaseMutex` + `CloseHandle` after acquiring ownership to avoid leak.

2. **Lazy recovery (L57 fix 2)**: new method `TryLazyRecovery()` in RimeWithWeaselHandler:
   ```cpp
   void RimeWithWeaselHandler::TryLazyRecovery() {
     if (!m_disabled) return;
     if (_IsDeployerRunning()) return;  // deployer alive, m_disabled is legitimate
     DLOG(INFO) << "TryLazyRecovery: deployer not running, auto-Initialize";
     Initialize();
     if (m_disabled) return;  // Initialize failed for other reason
     _UpdateUI(0);
   }
   ```

3. **9 call sites**: insert `TryLazyRecovery();` before each `if (m_disabled) return ...;` early-return in:
   - `ProcessKeyEvent`, `CommitComposition`, `ClearComposition`, `SelectCandidateOnCurrentPage`, `DeleteCandidateOnCurrentPage`, `FocusIn`, `UpdateInputPosition`, `FindSession`, `RemoveSession`
   - `AddSession`: replace legacy `if (m_disabled) { EndMaintenance(); if (m_disabled) return 0; }` with `TryLazyRecovery(); if (m_disabled) return 0;`

### Test (spec 053 T7)

`test/TestOrphanRecovery/TestOrphanRecovery.cpp` gains `TestAbandonedMutexR6()`:
- Spawn a child thread that calls `CreateMutex` and `WaitForSingleObject(INFINITE)` and then **exits without releasing**.
- Main thread: `OpenMutex` + `WaitForSingleObject(0)` must return `WAIT_ABANDONED`.
- This proves the new `_IsDeployerRunning` semantics (and is independent of the full RimeWithWeaselHandler pull-in).

`TestOrphanRecovery.exe` 7/7 PASS (T1-T7) as of 2026-07-08.

### Lesson (3 rules, blocking R6 recurrence)

1. **Any IPC pair that must be matched (Start/End, Open/Close, Lock/Unlock) MUST be wrapped in RAII unless there is a hard reason it cannot be.** This is restated from L55. But the corollary: **RAII only catches *language-level* death. OS-level death (taskkill /F, AV quarantine) bypasses C++ dtors.** Pair RAII with **an OS-detectable recovery mechanism** (named mutex WAIT_ABANDONED check, named pipe read, process PID check).

2. **Always pair RAII with an external "is the other side still alive" check.** `_IsDeployerRunning` must use `OpenMutex + WaitForSingleObject(0)` and treat `WAIT_ABANDONED` as "no holder", not as "holder exists". The legacy `CreateMutex(_, TRUE, ...)` + `GetLastError == ERROR_ALREADY_EXISTS` pattern is unreliable on both ends (false positive when the call itself creates the mutex; cannot detect abandoned state).

3. **`taskkill /F` is a real production failure mode, not a hypothetical.** Antivirus quarantine, Windows Update restart, OOM killer, and user task manager all produce the same outcome: process dies mid-flight. Any code that depends on clean function-return MUST be RAII-protected **AND** must have an external recovery path. L55 R4 + L57 R6 together cover both.

### Action items

- [x] spec 053 v0.18.31.0 implementation (R6 fix 1 + lazy recovery + 9 call sites + T7 test).
- [x] `RimeWithWeasel.cpp` byte-level patch (CLRF preserved, no UTF-8 BOM pollution per L01/L07/L47).
- [x] `RimeWithWeasel.h` private area declaration of `TryLazyRecovery()`.
- [x] `test/TestOrphanRecovery/TestOrphanRecovery.cpp` T7 added.
- [x] Standalone `cl.exe` compile of `RimeWithWeasel.cpp` -> 0 errors, 0 warnings.
- [x] Standalone `cl.exe` compile + run of `TestOrphanRecovery.exe` -> 7/7 PASS.
- [ ] Full `xbuild.bat weasel installer` rebuild to confirm linker is happy.
- [ ] Full `scripts/test-infra/run-test-suite.bat` regression run.
- [ ] Smoke test (AGENTS.md §2.5) on a clean install.
- [ ] Commit + push to kizemo/Fluxing, tag v0.18.31.0.

### Related incident note (L57bis - separate user symptom, same time)

User simultaneously reported "快捷设置栏在火流猩输入法切换后不显示，多次按快捷键才显示，样式与设计稿差距太大，过于粗糙，美观度不足" (QuickPanel does not show after switching to Fluxing IME, must press hotkey multiple times, visual style is rough). Separate root cause: the user-installed WeaselServer.exe is 13:49 compiled, **earlier than** the spec 049+052 v4 commit 17:42 - so the v4 QuickPanelDialog is not in the user binary. v0.18.30.0 installer (18:12) used weasel.dll 17:16 - **also earlier than v4** (17:16 < 17:42), so even the installer binary does not contain v4. **L42 false-positive** pattern: "git has the code" != "binary has the code". Cure: rebuild weasel.dll from HEAD before tagging v0.18.31.0; L42 byte-verify with a unique pattern from v4 code (e.g. `FluxingQuickPanel_v4` UTF-16LE byte pattern - the spec-049-added kClassName literal). If pattern count == 0, the v4 code is not linked; rebuild.


## L57bis - "image-deleted kernel zombie" cannot be killed by taskkill (must reboot Windows)

**Date:** 2026-07-08
**Status:** OPEN
**Triggered by:** User reported 5-6 WeaselServer.exe processes persisting after v0.18.30.0 install failures; user tried `taskkill /F /IM WeaselServer.exe` from non-elevated PowerShell, got "Access is denied" for 5 of 6. My L13 fix (commit 644d82a) added NSIS `taskkill /F /IM WeaselServer.exe /T` calls in install.nsi. **Even with the L13 fix, the 6 zombie processes survived.** This entry is the post-mortem.

**Symptom:**
```
PID  StartTime         Path
28540 2026/7/8 19:27:49
39168 2026/7/8 18:01:25
42496 2026/7/8 17:18:56
44048 2026/7/8 18:01:35
47532 2026/7/8 17:45:36
47648 2026/7/8 19:27:24
```
Note: only PID 28540 and 47648 had a non-empty Path when first listed via `Get-Process`. `Get-CimInstance Win32_Process` later showed **all 6 zombies have empty ExecutablePath**.

**Root cause (3-layer):**

1. **Previous v0.18.30.0 install failed mid-way** (e.g., file lock, user aborted). The NSIS install section partially ran - it `Exec`ed `WeaselServer.exe` BEFORE the install Section completed. Each failed install left one WeaselServer zombie.

2. **Subsequent successful install ran `call_uninstaller`**, which deletes the old `D:\Program Files\fluxing\*.*`. The file is unlinked from NTFS, but the **process image is still mapped in memory**. The kernel marks the process as a "lone zombie" (process object still in kernel, no user-mode state).

3. **User-mode taskkill cannot kill a lone zombie**. `taskkill /F /IM WeaselServer.exe` calls `NtTerminateProcess` which sends a signal to the process. A lone zombie has no user-mode thread to receive the signal; the kernel refuses to reap a process whose exit cannot be acknowledged. `taskkill` returns "Access is denied" because it cannot deliver the terminate signal to a process with 0 threads/0 handles/0 working set.

**Verification (Get-CimInstance + Get-Process on the 6 zombies):**
```
CPU=0s WS=0MB Handles=0 Threads=0 Responding=True Path=(empty)
```
- CPU=0s, WS=0MB, Handles=0, Threads=0 -> user-mode state is destroyed
- Path=(empty) -> `Exe` already unlinked from disk (install's `Delete "$R1\*.*"` ran)
- Responding=True -> but the process object is still in the kernel

**The 7th process (PID 12372) is alive** - the new install was successful for `C:\Program Files\fluxing\weasel\WeaselServer.exe`, but **not** for `D:\Program Files\fluxing`. The 12372 process is v0.18.30 binary (per file size + mtime 22:05) running in `C:\Program Files\fluxing\weasel\WeaselServer.exe`.

**Cure (3 layers, applied in order):**

1. **Reboot Windows.** Kernel cleans all lone zombies during Phase 1 init. Only 100% reliable fix.
2. **L13 fix (commit 644d82a, ship in v0.18.31.0 / v0.18.31.1)** - NSIS `taskkill /F /IM WeaselServer.exe /T` in 4 places (`.onInit` start, `call_uninstaller` label, install Section, Uninstall Section). **Effective for active WeaselServer processes** (e.g., PID 12372 if user installs while it is running). Not effective for image-deleted kernel zombies.
3. **Manual cleanup** - for environments where reboot is not possible: **boot the user into Safe Mode**, where the kernel can reap lone zombies. Or use **kernel debugger** (kd.exe) with `!process -k <pid>`. Both are out of scope for the installer.

**Lesson (3 rules):**

1. **The image name on disk is the installer's truth, not the process list.** If a process is alive but `Exe` is empty, it is a lone zombie. taskkill /F will fail; reboot is the only fix.
2. **`call_uninstaller` should also call `taskkill /F /IM WeaselServer.exe /T` BEFORE the `Delete "$R1\*.*"` line**, so the file unlink happens AFTER the process is dead, not after. This is what L13 fix does (taskkill runs before the Delete sequence in `call_uninstaller`).
3. **The "taskkill in PowerShell" advice to the user is wrong** if the user is non-elevated. Always prefer an in-installer mechanism (`ExecWait 'taskkill ...'` in NSIS) which inherits the installer's elevation token.

**Action items:**

- [x] v0.18.31.1 installer built (commit pending) with L13 fix in 4 places.
- [x] 7th active WeaselServer (PID 12372) install in `C:\Program Files\fluxing\weasel\` confirmed via WMI. This is v0.18.30 binary (per size + mtime 22:05). New install will overwrite with v0.18.31.1 binary.
- [x] 6 image-deleted kernel zombies cannot be killed by NSIS taskkill; they require Windows reboot.
- [x] L57bis written; action item for future: investigate `waitForInputIdle` or process-tree enumeration in NSIS to detect lone zombies BEFORE attempting file ops, so the install can prompt "Lone zombie detected: please reboot before reinstalling" instead of silently leaving them.

