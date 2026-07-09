---
paths:
  - "build.bat"
  - "xbuild.bat"
  - "env.bat"
  - "weasel.sln"
  - "xmake.lua"
  - "**/xmake.lua"
  - "**/*.props"
---

# Build & toolchain rules

The project uses **two stacked build systems**. Pick the right one based on
what you are trying to verify.

| System | Entry point | Use case | Latency |
|---|---|---|---|
| xmake | `xbuild.bat` | inner-loop (most edits) | 30–90 s |
| MSBuild | `build.bat all` + `weasel.sln` | CI parity / monthly hygiene | 5–60 min |

## Hard rules

- **Release build requires `RELEASE_BUILD=1` in `env.bat`**. Do **not** use
  `git describe --tags` for the release version — L10 §6, the tag is
  unreliable across forks. Set the env var explicitly.
- **`weasel.props` and `env.bat` are gitignored**. Never commit them. If you
  need an env value to be reproducible, write a `setup-shims.ps1` under
  `tools/` instead.
- **VS solution is the source of truth for project membership** (P3). `xmake.lua`
  is a mirror. If they disagree, update the `.sln` first, then mirror to xmake.
- **PowerShell on CJK text must be byte-level**. UTF-8 BOM on
  `output/install.nsi`, otherwise install.rb treats it as GBK. See
  `lessons-learned.md` L01, L02, anti-pattern A1.

## NSIS install / uninstall (`output/install.nsi`)

Cumulative pre-flight (do in order):

1. `clang-format` is irrelevant here; this is a NSIS script.
2. **BOM must be `EF BB BF`** (UTF-8); line endings must be CRLF.
   Do **not** read it via plain `File.ReadAllText`. Use
   `File.ReadAllBytes` then check `[0..2] == 0xEF 0xBB 0xBF`.
3. Silent install must pass `/D=<path>` explicitly. Unknown CLI args get
   concatenated into `$INSTDIR` and corrupt the user-data registry key
   (L13, L17).
4. **Before silent-install tests, clear these HKLM/HKCU keys** (L54):
   - `HKLM\Software\Fluxing\Weasel`
   - `HKCU\Software\Fluxing`
   (Otherwise the installer uses a stale path.)
5. Run the **end-to-end silent-install smoke test** defined in
   `AGENTS.md §2.5`. This is mandatory for any `install.nsi` change — not
   skippable.
