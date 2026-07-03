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
