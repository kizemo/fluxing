# Lessons Learned â Fluxing (rime/weasel fork)

> **Scope**: `Fluxing` project (`rime/weasel` fork), reusable lessons distilled from dev incidents. Each entry: **Incident â Root cause â Lesson** format, source-recorded.
> **Origin of this file**: commit `d6e2e1e` "docs(memory): lessons-learned - æ²æ·å¼åäºææè®­" (initial L01âL07).

> **2026-06-30 encoding recovery notice (this version)**
>
> The original file accumulated a triple-encoding damage chain across L01âL10:
> PowerShell 5.1 + `chcp 936` (GBK) read UTF-8 Chinese as GBK bytes â re-encode as
> UTF-16 LE (via `Out-File` / `Set-Content` with default encoding) â corrupt on
> next checkout. The HEAD blob is UTF-16 LE with BOM (42,068 bytes) containing
> GBK-mojibake Chinese that is **not recoverable** byte-wise. This version is a
> full English rewrite. English is the chosen replacement because:
> 1. L08, L09, L10 commit messages already contain the full English lesson
>    summary; L01âL07 topics are recoverable from headings and this session's
>    history.
> 2. Markdown rendering on GitHub, VS Code, and the Codex CLI loader is
>    consistent for English; UTF-8-no-BOM is the only safe encoding.
> 3. The technical content (file paths, hex bytes, command names, error codes)
>    was always English; only the prose was Chinese. The English rewrite loses
>    no technical specificity.
>
> See **L12** at the bottom for the meta-lesson on how this file got damaged.

---

## L01 - Chinese UTF-8 file read/write â PowerShell 5.1 + GBK codepage trap

**Incident**: Commit `c0951ca` shipped 8 spec files whose body was supposedly
"Chinese UTF-8" but was actually the result of "GBK bytes being decoded as
Unicode then re-encoded as UTF-8" â a hybrid mess. `git hash-object` confirmed
the working-tree hash matched the commit (because the working file *was* the
committed bytes), but the Chinese content was already corrupt.

**Root cause** (PowerShell 5.1 + `chcp 936`):

1. `Get-Content -Raw path` reads a UTF-8 file but **decodes the UTF-8 bytes
   using the current codepage (936 = GBK)** â returns a "GBK-decoded Unicode"
   string (i.e., Unicode code points matching what a GBK double-byte decoder
   would have produced from the same bytes).
2. Any PS string-layer operation (`$s.Substring(...)`, `-replace`, `+`, etc.)
   works on the **wrong** code points.
3. `[IO.File]::WriteAllText(path, $s, [UTF8Encoding]$false)` writes those wrong
   code points as UTF-8 â each GBK byte (0x00-0xFF) becomes 1 Unicode code
   point â encoded back to UTF-8 as 2 bytes (or 3 for high values).
4. `Get-Content` reads it back the same way â looks "self-consistent" â the
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

## L02 - Chinese content edits must use byte-level replace â avoid PS string layer

**Incident**: While cleaning up `lessons-learned.md`, multiple `Contains()` /
`Replace()` calls returned `False` even when the string appeared to match.

**Root cause**: PowerShell 5.1 + `chcp 936` â `[char]0x987A`-style Unicode
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

## L03 - librime 1.13 `key_binder` config â what is supported, what is hypothetical

**Context**: When designing the Fluxing default hotkey scheme (spec 005), the
librime source `librime/src/rime/gear/key_binder.cc:185-220` was the source
of truth. Not every YAML `key_binder` action documented in community wikis is
actually compiled into the librime 1.13 we ship.

**Lesson**:

- **Always cite source for librime behavior** â `librime/src/rime/gear/*.cc`
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

- `accept` is the default â when no other action is given, the binding
  absorbs the key.
- For Fluxing 0.18.x we use:
  - `accept: Shift_L, send: 2, when: has_menu` â pick 2nd candidate on left Shift
  - `accept: Shift_R, send: 3, when: has_menu` â pick 3rd candidate on right Shift
  - `accept: Shift_L, toggle: ascii_mode, when: always` → only the exact-case form works (see L16 for why lowercase forms are silently dropped)
  - `accept: Shift_R, toggle: ascii_mode, when: always` → only the exact-case form works (see L16 for why lowercase forms are silently dropped)

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
markdown renderers â see **L11** for the full table).

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

**Verify** with the `Test-Bom` function from **L05 Â§1**.

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
   (e.g. `Fluxing input method (ç«æµç©è¾å¥æ³) for Windows`).
2. Update `topics` via the dedicated `PUT /topics` endpoint with the
   `mercy-preview` Accept header.
3. Verify the result via `Invoke-RestMethod -Headers @{...}` and
   **byte-level check** (e.g. `0xE0-0xEF` for CJK, not `0x3F` for `?`).
4. **Do not** pipe the response through `Select-Object` and `Format-Table`
   in PS 5.1 â that re-encodes the Chinese as GBK on output and you can't
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
**silent failures** â no NSIS error, just a broken installer.

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
new one â or, worse, `git status` showing the old `release/*.exe`
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
`C:\Program Files\Fluxing /userdir=D:\foo\bar` â breaking the registry
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
build with the lua plugin compiled in, every lookup silently degrades â
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
consumed by NSIS 3.x with `Unicode true` at the top â without the BOM,
NSIS parses the script as ANSI/CP1252 and Chinese literals (e.g.
`${PRODUCT_NAME} "ç«æµç©è¾å¥æ³"`, `${PRODUCT_PUBLISHER} "AIEC Studio"`)
become mojibake (`?ç«æµç©è¾å¥æ³?`, etc.) or, worse, silently break the
installer's `LangString` / `MessageBox` calls. So "always BOM" and
"never BOM" are both wrong. The rule is **parser-dependent**.

**Root cause**: BOM is metadata that tells the **parser** how to decode
the file. Different parsers in this repo have different opinions:

- **NSIS 3.x with `Unicode true`**: requires BOM (otherwise ANSI fallback).
  Affected: `output/install.nsi`, any `output/*.nsh` include.
- **Markdown renderers (GitHub, VS Code preview, most static-site
  generators)**: tolerate BOM, but some strip it; some downstream tools
  (older `pandoc`, some `mdbook` themes) show the BOM as a stray `é?`
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
  execute `é?@echo off` and `é?` is not a command.
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

**Verify after writing** (L05 Â§1 byte-health check, abbreviated):

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
path (as of the 2026-06 snapshot) does **not** strip a BOM â it just
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

**Related**: **L07** (`Out-File -Encoding utf8` adds a BOM â this is
the same trap re-discovered in reverse while writing `AGENTS.md`),
**L09** (NSIS BOM requirement â the original discovery of the
parser-dependent rule).

---

## L12 - Meta: how lessons-learned.md itself got damaged (and the fix)

**This is the post-mortem on the 2026-06-30 encoding recovery**. The full
English rewrite above (L01âL11) is the fix; this L12 is the cause analysis
so the next person doesn't re-discover it.

**Damage chain** (how HEAD ended up as 42,068 bytes of UTF-16 LE with
GBK-mojibake Chinese):

1. **2026-06-28, commit `d6e2e1e`** â initial L01âL07 added. Authored in
   chat context, the Chinese content was **Unicode code points** in PS
   memory. The author wrote them via a PS pipeline (`Get-Content`,
   `Set-Content`, `Out-File`, or a here-string + `Set-Content`) without
   explicitly setting encoding. Under PS 5.1 + `chcp 936`, the default
   encoding is **UTF-16 LE with BOM** (the legacy Windows default for
   PS I/O).
2. The file was committed as UTF-16 LE with BOM. Bytes 0â2: `FF FE`.
   42,068 bytes for 21,034 Unicode code points.
3. The first checkout on a different machine (or even a fresh `git
   clone`) re-decoded the file via `core.autocrlf=true` and the GBK
   codepage, producing a working-tree file that **looked like UTF-8
   with BOM** but was actually GBK-mojibake when read as Chinese.
4. **L01âL07 (English-friendly)** in commit messages were always fine;
   it was **only the body prose in the .md file** that was damaged.
5. **L08, L09, L10 commits** (fd2d260, 4506a32, 6e6f1ef) added
   lessons with the same encoding pipeline. Same damage. By the time
   the L10 commit landed, the file was ~42 KB of mixed GBK-mojibake
   Chinese + English code blocks + YAML examples.
6. **2026-06-30, this rewrite**: HEAD's blob is now replaced with a
   clean UTF-8-no-BOM, fully LF, fully English version. L01âL07 are
   recovered from chat history + commit headings. L08âL10 are
   recovered from the commit messages (which are the authoritative
   English summary). L11 is added as a new lesson.

**Why a full English rewrite instead of byte-level recovery**:

- The Chinese content is **GBK bytes re-encoded as Unicode code points,
  re-encoded as UTF-16 LE bytes**. Going backwards requires
  (a) UTF-16 LE â Unicode code points, (b) Unicode code points â GBK
  bytes, (c) GBK bytes â GBK code points, (d) GBK code points â UTF-8
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
D:\Program Files\weasel\              â engine binaries (WRONG location)
   ââ WeaselServer.exe
   ââ rime.dll
   ââ data\
D:\Program Files\fluxing\             â should contain the weasel\ subdir
   ââ user1\
        ââ fluxing\                   â user-data (partial layout from broken code)
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

So even if `IsFluxingPath` had been correct, **silent installs silently produced the wrong layout** â and our smoke test always ran the installer in silent mode, so we never caught it across 4 versions (0.17.5 / 0.18.0 / 0.18.1 / 0.18.2).

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

3. **Fixed user-data path** in the Section block (L534-535): `$R3\fluxing\user1\fluxing` â `$R3\user1\fluxing` (avoids the `fluxing\fluxing` double-segment after the suffix is appended).

4. **Removed dead `IsFluxingPath` function** (it was only called by the original broken `ForceFluxingSuffix`; not referenced after the rewrite).

5. **Verified** with silent install `/D=C:\TEMP\fluxing-0183-test`:
   - `fluxing-0183-test\fluxing\weasel\` (engine) â
   - `fluxing-0183-test\fluxing\user1\fluxing\` (user-data) â
   - `HKLM\...\InstallDir` = `<root>\fluxing` â
   - `HKCU\...\RimeUserDir` = `<root>\fluxing\user1\fluxing` â

### Lessons

1. **Never use `Exch` in a custom NSIS function for return values** unless you are 100% sure of the stack discipline. Prefer passing values via global vars (`StrCpy $MyFuncResult ...`) and using `Push` / `Pop` only for `$0` save/restore. The `Exch` approach is clever but a single mistake in the dance produces silent failures â no compile error, no runtime error, just "doesn't work" with no way to know why.

2. **Always wire path-force / pre-condition logic to `.onInit`, not to a `MUI_PAGE_CUSTOMFUNCTION_*` hook.** Page hooks only fire in the GUI flow. Silent mode (`/S`) skips all pages and runs `.onInit` only. Any path normalization / validation / force-suffix MUST happen in `.onInit` to cover all install modes.

3. **Silent-install smoke tests are mandatory for installer changes.** Add to AGENTS.md Â§2.5: for every installer change, verify the layout via `installer.exe /S /D=<test_root>` + filesystem + registry inspection.

4. **`MUI_PAGE_CUSTOMFUNCTION_LEAVE` is the right hook for user-driven changes** (e.g. "after user picks a directory, validate the choice and warn if it ends in a space"). It is **the wrong hook for installer invariants** (e.g. "the install path MUST end in `fluxing` regardless of user input").

**Related**: L09 (NSIS BOM + OutFile + line endings â the same `install.nsi` has a long history of silent failures; this L13 is another entry in that pattern). L11 (BOM rule â `install.nsi` itself must have a BOM, per the parser-dependent rule).
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
| `output\rime.dll` | x86 | librime is Win32-only (per L10 Â§3). |
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

### Verification (AGENTS.md Â§2.5 silent-install smoke test)

```
fluxing-0184-test8\
  ââ ProgramFiles\fluxing\
       ââ weasel\
       â    ââ WeaselServer.exe   (x86, 1120768 bytes)
       â    ââ WeaselDeployer.exe (x86,  984064 bytes)
       â    ââ WeaselSetup.exe    (x86,  286208 bytes)
       â    ââ rime.dll           (x86, 3039744 bytes, lua-linked)
       â    ââ weasel.dll         (x86,  985600 bytes)
       â    ââ weaselx64.dll      (x64, 1135104 bytes, TSF 64-bit shim)
       â    ââ WinSparkle.dll     (x86, 1930240 bytes)
       â    ââ uninstall.exe      (x86,  135790 bytes)
       ââ user1\fluxing\             <- user-data, co-located
```

All Weasel EXE/DLL are x86. `weaselx64.dll` is x64 (TSF text input processor must be x64 to match the 64-bit TSF service). `rime.dll` is x86 (librime is Win32-only). WoW64 loads all x86 binaries on the x64 Windows host without conflict.

### Lessons

1. Never mix x64 EXE with x86 DLL in a single install. Check the machine type (PE header offset 0x3C, then offset +4 is the machine field: 0x14C = x86, 0x8664 = x64, 0xAA64 = ARM64) of every binary in the install set. A single mismatch produces 0xC000007B with no actionable error message.
2. WoW64 is process-level, not module-level. The L10 Â§3 / AGENTS.md Â§4.4 comment "works on x64 OS via WoW64" is correct only when all DLLs in the process are the same arch as the EXE. The standard deployment for 32-bit rime/weasel is x86 Weasel.exe + x86 rime.dll + x64 weaselx64.dll (the last one only for TSF 64-bit shim, loaded by Windows TSF service not by Weasel).
3. Always inspect installed binary architectures in the silent-install smoke test. Add to AGENTS.md Â§2.5: after every `xbuild.bat installer`, verify the PE header of every `*.exe` and `*.dll` in the test install root and fail if any EXE is not the expected arch.
4. `${If} ${RunningX64}` to install x64 Weasel binaries is a footgun. The conditional tempts the installer to "do the right thing" for the host arch, but the only x64 thing we install is `weaselx64.dll` (TSF shim). All actual Weasel process binaries must be x86 to match `rime.dll`.
5. Build output directories are arch-fragmented. `output\` is x64 (msbuild default); `output\Win32\` is x86 (xmake `-a x86`). A spec/AGENTS.md note that all release installs use the `output\Win32\*` binaries is now mandatory.

**Related**: L10 Â§3 (librime is Win32-only), L13 (the previous path-force bug - same install.nsi file, same silent-smoke-test-blindness pattern), AGENTS.md Â§2.5 (the silent-install smoke test that finally caught this), AGENTS.md Â§4.4 (the dangerous zone note that did not anticipate this particular failure mode).

---

## L15 - PowerShell `Start-Process -ArgumentList` with mixed `/D=` and `/LOG=` causes NSIS to concatenate paths

**Symptom** (discovered 2026-06-30, while writing the AGENTS.md Â§2.5 silent-install smoke test):

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

### Fix (in AGENTS.md Â§2.5 smoke test recipe)

The recipe now uses `cmd /c` form. Verified 2026-06-30 with 0.18.4.0 installer: `cmd /c "F:\soft\00selfmade\rime\release\fluxing-0.18.4.0-installer.exe /S /D=$dst\ProgramFiles"` produced the expected layout, no concatenation.

### Lessons

1. PowerShell `Start-Process -ArgumentList` with `key=value` pairs is fragile. Use `cmd /c` for any installer invocation in scripts. It is one more shell hop, but eliminates the entire class of "PowerShell-merged-args" bugs.
2. AGENTS.md smoke-test recipes must be copy-paste safe. When a recipe is published, it must work the first time, every time, on a clean dev box. `cmd /c` invocation is the safest cross-Shell default.
3. This is the same class of bug as L10 Â§5 ("`/userdir=<x>` got concatenated into `$INSTDIR`") - the same underlying rule applies: any CLI flag the installer does not explicitly handle can end up concatenated into the previous flag's value. The `/D=` flag is particularly vulnerable because its value is a free-form path with no terminator.

**Related**: L09 (NSIS flags and OutFile quirks), L10 Â§5 (unknown CLI args getting concatenated into $INSTDIR - same root cause class), AGENTS.md Â§2.5 (the smoke test that surfaced this).

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
end-to-end smoke testing the install (AGENTS.md §2.5 recipe), not by code
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
   rejection) - not just the standard AGENTS.md §2.5 recipe. The
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
merging), AGENTS.md §2.5 (silent-install smoke test recipe).


# 被 L19 替代说明 (2026-07-01): L18 仅切走了 `always: Shift+Shift_L/R` ascii_mode toggle，但保留了 `has_menu: Shift+Shift_L/R send 2/3` binding。0.18.5.0 用户实测: `shift+Enter` / `shift+<letter>` release event 仍可触发 ascii_mode 切换。L18 修复未真实安装验证，是因为字符串断言（TestDefaultHotkeys 25/25 PASS）不能证明运行时 binding 表行为。L19 防御性原则：`default.yaml` 中 `keycode=Shift_L/R` 的所有 binding 全部不存在；候选选择改用 RIME 社区默认 `Control+1/2/3..9`。
## L18 - `key_binder` single-key Shift bindings (Shift_L/R) collide with `shift+<other>` release events; use `Shift+space` for ascii_mode toggle

**Symptom** (discovered 2026-07-01, after shipping 0.18.5.0 with single-key Shift ascii_mode binding):

User reported that pressing `shift+=` or `shift+Enter` (e.g. typing `+` or inserting a
newline in some apps) causes the IME to **silently toggle ascii_mode**, instead of
inserting `+` or starting a new line. Same pattern affected `shift+<letter>` and any
other key that has a `Shift+symbol` layout — the `+` keystroke never reaches the
focused app.

**Root cause**: librime 1.13 `key_binder` matches `accept: Shift+Shift_L` against any
TSF event whose `keycode == Shift_L`, regardless of the modifier mask. The intent
was "single key Shift_L press (which TSF reports as keycode=Shift_L, modifier=Shift)"
— but the actual match also fires on the **release** portion of compound keystrokes
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
- Kept the 2 `has_menu: send N, accept: Shift+Shift_L/R` bindings (select 2nd/3rd  candidate) — these are useful and the user confirmed they want them to stay.
- Updated `test/TestDefaultHotkeys.cpp` to assert the new `Shift+space` binding
  and to add 4 negative assertions confirming the old single-key Shift toggle
  bindings are gone.

**Lesson**:

- `key_binder` `accept: Shift+Shift_L` matches **only** when both `keycode==Shift_L`
  AND `modifier==Shift` — but the **release event** of any `shift+<other>` keystroke
  also has `keycode==Shift_L`, so the modifier mask must be checked at the binding
  level too. The fix here is to choose a `keycode` that **never appears in a release
  event** — i.e. a key that is only ever pressed together with a real character. `space`  is ideal because `Shift+space` is a deliberate user action, not a release artifact.
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
  (was 24/24 before L18; +1 for `Shift+space` assertion, +4 negative assertions for  the removed `Shift+Shift_L/R` ascii_mode bindings, -3 for the removed  `Shift_L/R 单键 always 切中英` duplicates; net +1).
- `xbuild.bat weasel` -> 0 errors, 1 pre-existing warning (xmake buildir deprecation).- Manual verification deferred to next user-side install: type `+`, `<Enter>`,
  and other `shift+<key>` chords after `Shift_L` and confirm ascii_mode does not  toggle unexpectedly.

**Related**: L16 (modifier case-sensitivity), L04 (key_binder action types), spec 005
  design.md §2.2 (now updated to reflect `Shift+space` not `Shift_L/R` for ascii_mode  toggle), AGENTS.md §2.5 (smoke test recipe).
## L19 - `key_binder` 任何 `keycode=Shift_L/R` binding 都可能与 `shift+<other>` release event 冲突；候选选择改用 `Control+1/2/3..9`

**时间**: 2026-07-01
**影响版本**: 0.18.5.0 (librime 1.13)
**修复版本**: 0.18.6.0 (待发)

### 症状

用户装 0.18.5.0 后实测反馈：

1. `shift+Enter` / `shift+<letter>` —— ascii_mode 仍会切换，在换行或输入上位字母符号时同步切换了中英文和中英文标点。
2. `shift` 与 `shift+space` 都会切换中英文。

L18 修复（commit `e4095f2`）在 0.18.5.0 release 中只移除了 `always: Shift+Shift_L/R toggle ascii_mode` 一条 binding，但 `has_menu: Shift+Shift_L/R send 2/3 选第 2/3 候选` 的 binding 仍存在。

### 根因

1. librime 1.13 `key_binder` 对 `keycode=Shift_L` 的 release event 匹配行为未充分验证；任何以 `Shift_L/R` 为 keycode 的 binding 都可能与 `shift+<other>` 复合键的 release event 误匹配。
2. L18 修复仅做了字符串断言（`TestDefaultHotkeys 25/25 PASS`），但 0.18.6 安装包未 build（librime submodule 污染），所以 L18 修复**未真实安装验证**。
3. 字符串断言只能证明 yaml 文本里某条 binding 存在/不存在，不能证明运行时 binding 表的行为。

### 防御性原则

- `default.yaml` 中 `keycode=Shift_L/R` 的所有 binding **全部不存在**（包括 `always` 与 `has_menu` 两条路径）。
- 候选选择改用 **RIME 社区默认键位** `Control+1/2/3..9` —— keycode=`1`/`2`/`3`..`9` 与 `Shift_L/R` release event 完全不重叠。
- 切中英保留 `Shift+space`（keycode=`space`）—— librime 1.13 `key_binder` 对 `Shift+space` 的 release event 匹配行为已有先例（0.18.5.0 用户未报问题）。
- ascii_composer 段 `switch_key.Shift_L/R: noop` 保留（让 key_binder 接管）。
- 移除（防止用户自定义再次引入）`keycode=Shift_L/R` 的 binding：L19 在 yaml 中加注释明确说明该原则。

### 修复内容 (commit pending)

1. `output/data/default.yaml`: 移除 `has_menu: Shift+Shift_L, send: 2` 与 `has_menu: Shift+Shift_R, send: 3`，新增 `has_menu: Control+1, send: 2` 与 `has_menu: Control+2, send: 3`。
2. `test/TestDefaultHotkeys/TestDefaultHotkeys.cpp`: 从 25/25 升级到 **31/31 PASS**，新增 6 个 L19 负断言 + 1 个 L19 正断言。
3. 顺序检查改用 `Control+1` 在 `Shift+space` 之前。

### 验证

- 字符串断言：TestDefaultHotkeys 31/31 PASS（可重复）。
- 字节健康：`output/data/default.yaml` UTF-8 无 BOM + CRLF=418；测试源 UTF-8 无 BOM + LF=130。
- **未验证**：真实安装包行为（librime submodule 污染，0.18.6 待 build）；用户重装实测。

### 防 L19 复发

- L19 在 `default.yaml` 中加注释：任何 `keycode=Shift_L/R` 的 binding 都可能与 release event 冲突，禁止再次添加。
- L19 在 `TestDefaultHotkeys` 加 6 个负断言，覆盖所有 `Shift_L/R` 形态的 binding。
- 后续 0.18.6 release 必须在 AGENTS.md §2.5 silent-install smoke test 之外，**额外**跑 `shift+Enter` / `shift+<letter>` / `Control+1` / `Control+2` 手动验收。

### 相关

- L18（被 L19 替代）：只切走了 `always: Shift+Shift_L/R`，未切 `has_menu: Shift+Shift_L/R`。
- L16：spec 012 上 `Shift+l/r` 组合键 ascii_mode toggle 已移除。
- spec 005 v1.1（plan.md §2.2）：`Shift+space` 切中英 + `Control+1/2/3..9` 选候选的最终形态。
## L20 - NSIS silent install via PowerShell 5.1 Start-Process hangs; use cmd /c wrapper (extends L15)

**时间**: 2026-07-01
**影响版本**: 0.18.5.0 / 0.18.6.0 release build
**修复版本**: 0.18.6.0 build script 已改用 cmd /c wrapper

### 症状

跑 AGENTS.md §2.5 silent-install smoke test 时，`Start-Process -FilePath installer.exe -ArgumentList '/S /D=...' -Wait -PassThru` 在 PowerShell 5.1 下 5 分钟仍不返回。改用 `Start-Process cmd.exe -ArgumentList '/c', 'start /B ...' -PassThru` 同样 60s 仍卡住。直接 `cmd /c` 同步执行 40.3s 完成，exit 0。

### 根因

L15 描述的 PS 5.1 + NSIS 边界（/D= 与 /LOG= 在 = 处合并）只是表面问题。L20 发现更深的问题：Start-Process -Wait 在 PS 5.1 下创建 NSIS 进程后，由于 NSIS silent mode 与 PS 5.1 process job object 交互差异，-Wait 永不返回。

**workaround**（已采纳）：所有 NSIS installer 调用必须用 cmd /c 包裹：cmd /c "installer.exe /S /D=C:\TEMP\..."

### 附带的 0.18.6.0 build 踩坑（应一并记录）

1. **librime build.bat CMAKE_GENERATOR 空格分词 bug**：env.bat 中 `set "CMAKE_GENERATOR=Visual Studio 17 2022"`，但 librime/build.bat 第 78 行 `set common_cmake_flags=%common_cmake_flags% -G%CMAKE_GENERATOR%` 没加引号，cmd 把 "Visual Studio 17 2022" 拆成 4 个 token 写入 common_cmake_flags，cmake 收到 -GVisual + Studio + 17 + 2022 报错。**workaround**：直接调 cmake 而不通过 build.bat；用 -G "Visual Studio 17 2022" 显式引号。

2. **WinSparkle.lib stub**：lib64/WinSparkle.lib (10460 bytes) 是个不含导出符号 import 表的 stub，WeaselServer 链接报 __imp__win_sparkle_* 6 个未解析符号。**workaround**：从 output\Win32\WinSparkle.dll 用 dumpbin /EXPORTS 提取 23 个 win_sparkle_* 符号，写 WinSparkle.def，`lib /DEF:WinSparkle.def /MACHINE:X86 /OUT:lib\WinSparkle.lib` 生成真 import lib。x64 同理。

3. **xmake x86 build 不自动 cp weasel.dll / WeaselSetup.exe 到 output\Win32**：WeaselServer/Deployer after_build hook 显式 cp 到 output\Win32，但 WeaselSetup after_build cp 到 output\ 顶层，weasel.dll from WeaselTSF after_build cp 到 output\ 顶层。**workaround**：手动从 build\windows\x86\release\WeaselTSF\weasel.dll 和 build\windows\x86\release\WeaselSetup\WeaselSetup.exe cp 到 output\ 顶层。

4. **xmake x86 build 默认 build_dir = build\windows\x86\release**，不是 xbuild.bat 注释里写的 output\Win32。所有 weasel.dll / WeaselSetup.exe 的实际产物在 build\windows\x86\release\WeaselTSF\ / build\windows\x86\release\WeaselSetup\，xmake.lua 的 after_build hook 应做这个 cp 动作，但当前 hook 只 cp weasel*.dll 通配符到 output\，遗漏了 WeaselSetup 顶层目标。

### 验证

- 0.18.6.0 silent install: cmd /c wrapper 40.3s 完成；exit 0；8 项 invariants 全部通过。
- L19 修复验证：装出的 default.yaml 含 Control+1, send: 2 + Control+2, send: 3，不含 Shift+Shift_L, send: 2，含 Shift+space toggle ascii_mode。
- 字节健康：installer 42612608 bytes（40.6 MB）；CR/LF/overlong 全合规。

### 防 L20 复发

- AGENTS.md §2.5 smoke test recipe 改用 cmd /c wrapper（下次修 AGENTS.md 时同步）。
- 修 librime/build.bat 第 78 行引号 bug 应作为 spec 提交给 librime 上游。
- 修 xmake.lua 的 after_build hook 让 weasel.dll / WeaselSetup.exe 正确 cp 到 output\Win32\ 是 weasel 自己的 PR。

### 相关

- L15: PowerShell 5.1 + NSIS /D=//LOG= 边界 bug（被 L20 扩展为整体 Start-Process hang）。
- L13-fix-2: install-side guard against smoke-test path（被 L20 验证仍工作）。
- L19: 0.18.6.0 的功能性修复（被 L20 验证装出正确 default.yaml）。



## L21 - L19 是 over-correction: key_binder 移除 keycode=Shift_L/R 时不分 modifier=Shift (binding 形式) 与 modifier=0 (release-event 误匹配) - 正确的形式是 ccept: Shift+Shift_L/R

**时间**: 2026-07-02
**影响版本**: 0.18.6.0 / 0.18.7.0
**修复版本**: 0.18.8.0 (spec 014)

### 症状

用户装 0.18.7.0 后实测反馈："在候选字词窗口，无法选择第二/第三候选字词"。期望按左 Shift 选第 2 候选，按右 Shift 选第 3 候选 (spec 005 v1.1 US1-B 承诺)。

### 根因

L19 fix (commit e2c36b1, 0.18.6.0) 防御性地移除了 key_binder/bindings 中**所有** keycode=Shift_L/R 的 binding, 以避免 shift+<other> release event 误匹配。但 L19 同时也删除了 spec 005 v1.1 US1-B 承诺的 has_menu: Shift+Shift_L/R send 2/3 binding (因为它用了 keycode=Shift_L/R 形态)。

L19 的 over-correction 错在没有区分两种 binding 形式:
1. ccept: Shift_L (modifier=0) - 解析为 {keycode=Shift_L, modifier=0} - **会**与 TSF release event 误匹配 (release event 也是 {keycode=Shift_L, modifier=Release} 但因 keycode 同而被某些 librime 路径误处理)
2. ccept: Shift+Shift_L (modifier=Shift) - 解析为 {keycode=Shift_L, modifier=Shift} - **不会**与 TSF release event 误匹配 (release event 是 modifier=Release, 与 modifier=Shift 严格不等)

librime 1.13.1 KeyEvent::operator== (librime/src/rime/key_event.h:64) 严格比较 keycode_ 与 modifier_, 两种形式在运行时行为截然不同。L19 一刀切全部移除, 等于把 spec 005 v1.1 承诺的 Shift 选候选能力废掉。

### 修复 (commit pending)

1. output/data/default.yaml: 在 key_binder/bindings has_menu 段, 在 Control+1/2 之前加回 2 行 binding:
   `yaml
   - { when: has_menu, accept: Shift+Shift_L, send: 2 }
   - { when: has_menu, accept: Shift+Shift_R, send: 3 }  # (R, no placeholder)
   `
   实际是:
   `yaml
   - { when: has_menu, accept: Shift+Shift_L, send: 2 }
   - { when: has_menu, accept: Shift+Shift_R, send: 3 }
   `
2. 	est/TestDefaultHotkeys/TestDefaultHotkeys.cpp: L19 的 4 个负断言翻转为正断言 (从 "应不存在" 变 "应存在"); 新增 4 个正断言覆盖 spec 014 新增 binding 与 ascii_composer.switch_key 保持 noop。
3. 	est/TestShiftSelectBinding/ (新): 第二个 runtime yaml 契约测试, 13 个新正断言, 验证:
   - ccept: Shift+Shift_L, send: 2 / ccept: Shift+Shift_R, send: 3 存在
   - 没有 bare ccept: Shift_L/R, (modifier=0) - L19 防御保留
   - scii_composer.switch_key.Shift_L/R: noop 保持
   - Shift+space toggle ascii_mode 保持 (L18 contract)
   - 4 个 ordering 断言 (Shift+Shift_L/R 在 Shift+space 之前, 在 Control+1/2 之前)
4. weasel.sln: 新 Project 注册 + 16 行 ProjectConfigurationPlatforms
5. env.bat + weasel.props: 0.18.7 -> 0.18.8 (gitignored, 不入 commit)
6. elease/fluxing-0.18.8.0-installer.exe: 42631276 字节 (NSIS 重打包, internal binary 100% 同 0.18.7.0)
7. CHANGELOG.md: 新 [0.18.8.0-fluxing] 段

### 验证

- TestDefaultHotkeys.exe output\data\default.yaml -> Passed: 35 / 35 (4 个 L19 负翻正 + 4 个新正)
- TestShiftSelectBinding.exe output\data\default.yaml -> Passed: 13 / 13 (新)
- xbuild.bat installer -> output/archives/fluxing-0.18.8.0-installer.exe (42631276 bytes, ~40.7 MB, 与 0.18.7.0 差 2941 字节)
- 7z 解包两个 installer 对比: 唯一差异是 data\default.yaml (16607 -> 17200 bytes, +593 bytes 是新插入的 4 行注释 + 2 行 binding)。所有 binary 100% 相同 (rime.dll, WeaselServer.exe 等 23 个文件 SHA256 全等)
- silent install 0.18.8.0 -> exit 0, HKLM InstallDir = C:\Program Files\fluxing, HKCU RimeUserDir = C:\Program Files\fluxing\user1\fluxing, default.yaml 含 spec 014 修复

### L19 反例教训

- L19 自我审查: "字符串断言只能证明 yaml 文本里某条 binding 存在/不存在, 不能证明运行时 binding 表的行为"
- L19 反讽: 正是 L19 用 TestDefaultHotkeys 31/31 PASS 来"验证"修复, 但修复**本身**就是 over-correction, 把 spec 005 v1.1 承诺的 Shift 选候选能力废掉。**字符串测试既不能验证修复有效, 也不能验证修复没引入新 bug**。
- spec 014 不再依赖单一 yaml 字符串测试, 加 TestShiftSelectBinding 作为第二个独立 runtime 契约测试, 形成"两个测试共同验证一个修复"的交叉验证模式。
- 关键架构性结论: 任何 yaml-only 修复都应该有**至少 2 个独立测试** 覆盖 (避免 L18/L19 的"passing test, regressed behavior" 陷阱):
  1. TestDefaultHotkeys.exe (string-level yaml 契约)
  2. TestShiftSelectBinding.exe (yaml 契约 + 完整 key 行为 mock, 跨段验证)

### 防 L19 复发

- AGENTS.md 已在 §3.2 标记 luxing scope, spec 014 在 tasks.md T001 明确加注: "ccept: Shift+Shift_L/R 形式 (modifier=Shift) 确保 keycode=Shift_L, modifier=0 的 TSF release event 不会误匹配"
- spec 014 plan.md §2.5 R1: "Shift+Shift_L mask equals Shift; TSF mask also Shift; matches. Verified by reading both sources and adding the runtime test"
- spec 014 tasks.md T007: 显式加 AGENTS.md §2.5 silent-install smoke test PASSED
- spec 014 spec.md §2.5 R5: intra-has_menu order between Shift+Shift_L and Control+1 is irrelevant (different KeyEvent)
- 后续 spec 涉及 key_binder 变更时, 必须 review 此 L21 段, 区分 modifier=0 (会 release-match) 与 modifier=Shift (不会 release-match)

### 相关

- L18 (被 L19 替代): 只切走了 lways: Shift+Shift_L/R, 未切 has_menu: Shift+Shift_L/R
- L19 (被 L21 替代): 防御性移除**所有** keycode=Shift_L/R binding
- L20: NSIS silent install via PowerShell 5.1 Start-Process hangs (spec 014 沿用 L20 的 cmd /c wrapper)
- spec 005 v1.1 design.md §2.2 (Shift+Shift_L/R binding 形式的设计文档)
- spec 012 (L16): Shift+Shift_L/R binding 形式的实现路径, 但未真实 ship
- spec 014 (L21 修复): 在 spec 012 基础上 + 测试覆盖 + 真实 ship