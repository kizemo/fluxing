# CONTEXT.md — Project Domain Map for AI Skills

> **Audience**: engineering skills (`/improve-codebase-architecture`,
> `/diagnosing-bugs`, `/tdd`, and friends) that need to load project
> vocabulary, invariants, and layout before they read code. This is the
> *first file* those skills should read. For a full operations manual
> see [AGENTS.md](AGENTS.md); for project-specific rules see
> [.specify/memory/constitution.md](.specify/memory/constitution.md);
> for an end-to-end system walkthrough see
> [docs/Fluxing-code-map/01-overview.md](docs/Fluxing-code-map/01-overview.md).
> For the live, dated, in-flight knowledge of this fork see
> [.specify/memory/project-knowledge.md](.specify/memory/project-knowledge.md).

---

## 1. Identity

| Field | Value |
|---|---|
| Product (user-facing) | **Fluxing** / **火流猩输入法** |
| Product (upstream) | **Weasel** / **小狼毫** (RIME Windows frontend) |
| Repo fork | `kizemo/fluxing` on the `Fluxing` branch |
| Upstream | `rime/weasel` (`origin` remote, **read-only**) |
| Engine | `librime` (C library, vendored as a git submodule) |
| License | GPLv3 — all changes must remain GPL-compatible (constitution §P-license) |
| Latest released | see `CHANGELOG.md` top entry (currently 0.21.0.1) |
| Active specs | 60 entries under `.specify/specs/` (spec 000–077, 编号有跳号与重号) — see `ls .specify/specs/` |

> The `Fluxing` brand fork is permitted by constitution P8 (Amendment 1).
> The fork may rebrand strings/CLSIDs/paths/registry/binary names, but it
> **must not** modify RIME engine behavior, the IPC wire-protocol
> *semantics*, the `librime` / `Boost` / `OpenCC` contents, or the
> `.clang-format` style.

---

## 2. Module map (what lives where)

| Directory | Role | Read before touching |
|---|---|---|
| `WeaselTSF/` | TSF text service (the in-process IME front-end for Win32/Win64/ARM) | Anything in the TSF thread or composition path |
| `WeaselServer/` | Background RIME supervisor + tray icon + QuickPanel + WinSparkle | Anything user-visible from the tray, or the long-lived RIME process |
| `WeaselDeployer/` | Settings/deploy/dict-management GUI | Anything user-editable via the deployer |
| `WeaselUI/` | Candidate-box UI static lib (D2D/DirectWrite); also hosts `FluxingComponents/` | Candidate rendering, panel layout, custom controls |
| `WeaselIPC/` `WeaselIPCServer/` | Named-pipe client/server static libs (the `PipeChannel` protocol) | Any cross-process message: TSF↔Server, Deployer↔Server |
| `RimeWithWeasel/` | librime C-API adapter (WeaselServer ↔ librime) | Anything that calls librime from the server side |
| `WeaselSetup/` | NSIS install/uninstall (x86 only) | `InstallDir` math, registry writes, CLSID/Profile GUID |
| `FluxingConfigEditor/` | yaml-cpp round-trip wrapper (preserves key order) | UI writes to `*.custom.yaml` without going through `RimeLeversApi` |
| `include/` | Public headers (IPC, UI, RimeWithWeasel, WeaselConstants, WeaselUtility, KeyEvent, WTL) | Anything cross-module |
| `librime/` | **Submodule** — RIME engine C library | Do not edit; see L10. The vendored plugin path is `thirdparty/librime-lua/` |
| `plum/` | **Submodule** — schema manager (auto-cloned by `get-rime.ps1`) | Schema distribution / candidate-source updates |
| `thirdparty/librime-lua/` | Vendored librime-lua plugin source | Lua scripting; re-copied into `librime/plugins/lua/` on every build (L10 §4) |
| `output/data/` | Deployed RIME/YAML (read-only at runtime, overwritten on upgrade) | When reasoning about what the *deployed* user sees |
| `output/install.nsi` | NSIS installer script (UTF-8 with BOM, CRLF line endings — L09) | Any install/upgrade/uninstall behavior |
| `test/` | Unit tests (x86, Release only): `TestDefaultHotkeys`, `TestResponseParser`, `TestWeaselIPC`, plus the Fluxing-specific test suite | Every change should be paired with a test where feasible (constitution R2) |
| `release/` | Historical installer binaries (git-tracked) | Cut a new installer by *adding* a file here, not by editing old ones |
| `docs/Fluxing-code-map/` | Read-only map of upstream Weasel (snapshot @ weasel 0.17.4). Describes the *codebase*; not a source of decisions | When you need to understand the upstream API surface |
| `docs/design/` | HTML mockups for the Quick Settings panel (v1/v2/v3) | UI design questions, especially the quick-settings tray surface |
| `docs/agents/` | Configuration for the Matt Pocock engineering skills | If you are running a skill and it does not behave, read this folder |
| `docs/adr/` | Architecture Decision Records (newly empty — see §7) | Before making a record-worthy design choice |
| `.specify/specs/NNN-*/` | Per-feature spec/plan/tasks triples | When implementing a feature that has a spec, the spec is authoritative |
| `.specify/memory/constitution.md` | Binding principles I–V, hard rules R1–R9, project rules P1–P8 | **Always.** Read first. |
| `.specify/memory/lessons-learned.md` | L01–L## incident post-mortems | When the bug looks familiar, check the lessons index |
| `.specify/memory/project-knowledge.md` | Live, dated knowledge of the Fluxing fork (deployment paths, registry keys, NSIS traps) | When the question is about the *current* D:/HKCU/HKLM state |
| `AGENTS.md` | Operating manual (build, test, lint, anti-patterns) | Always |
| `CHANGELOG.md` | Per-release changelog (extract via `extract_changelog.ps1` for releases) | When cutting a release |

---

## 3. Vocabulary

Terms that an engineering skill will see in code, comments, issues, and
specs. Use these strings verbatim when searching or filing issues.

### 3.1 Runtime terms

| Term | Meaning |
|---|---|
| **TSF** | Text Services Framework — Windows Vista+ IME API. `ITfTextInputProcessor`, `ITfContext`, `ITfComposition` |
| **TSF thread** | The thread TSF calls into. **Never block it for I/O** (constitution P2) |
| **CLSID** | TSF text-service GUID. Fluxing fork regenerates this (constitution P8) |
| **Profile GUID** | TSF input-profile GUID; also regenerated by the fork |
| **RIME** | The input method engine (librime); key tables, schemas, OpenCC tables |
| **librime** | RIME C library; vendored as a git submodule at `librime/` |
| **RimeApi / RimeTraits / RimeLevel** | librime C API entry points |
| **RimeLeversApi** | librime levers (user-config) module — `custom_settings_init`, `customize_string`, `save_settings` etc. |
| **PipeChannel / WeaselIPC** | Named-pipe transport between front-ends and `WeaselServer.exe`. `\\.\pipe\<user>...` |
| **IPC text protocol** | `key=value` lines (`action=session / context / status / style / commit`) + `.\n` terminator |
| **WeaselServer** | The single RIME supervisor process. Holds the leveldb LOCK on `*.userdb/` while running |
| **WeaselDeployer** | GUI for settings, deployment, dict management |
| **WinSparkle** | Third-party auto-updater; driven by `update/appcast.xml` |
| **PPL** | Protected Process Light. `WeaselServer.exe` runs as PPL; `taskkill` cannot terminate it |
| **Maintenance mode** | `WeaselIPC::Client::StartMaintenance() / EndMaintenance()` — temporarily releases the userdb lock so other tools can `export_user_dict` / `import_user_dict` |

### 3.2 Configuration terms

| Term | Meaning |
|---|---|
| **shared_data_dir** | `<install_dir>/data/` — default RIME config (overwritten on upgrade) |
| **user_data_dir** | `<RimeUserDir>`, default `%AppData%\Rime` (upstream) or `%AppData%\Fluxing` (fork, see P8) |
| **default.yaml** | Global RIME config; ascii_composer + key_binder live here |
| **weasel.yaml** | UI style (font, colors, candidate-box geometry) |
| **`<schema>.schema.yaml`** | Schema definition (rime_ice, double_pinyin_sogou, etc.) |
| **custom_phrase.txt** | Common-phrase file (format: `phrase<TAB>code<TAB>weight`) |
| **`<schema>.userdb/`** | User dictionary — LevelDB. Library is locked by WeaselServer while running |
| **`<config>.custom.yaml`** | User override file — written by RimeLeversApi or YamlRoundTrip; deep-merged on top of `default.yaml` |
| **installation.yaml** | Installer metadata (version, install date) |
| **Signature block** | Top-of-`*.custom.yaml` signed block that marks the file as written by a trusted source |

### 3.3 Process / build terms

| Term | Meaning |
|---|---|
| **deploy** | `WeaselDeployer /deploy` — recompile `*.table.bin` from sources, restart RIME |
| **installer** | `xbuild.bat weasel installer` — also runs NSIS, produces `output/archives/fluxing-<ver>-installer.exe` |
| **xbuild.bat** | xmake wrapper, inner-loop build (30–90 s) |
| **build.bat** | msbuild + cmake end-to-end (5–60 min) |
| **RELEASE_BUILD=1** | Required in `env.bat` for release builds (L10 §6 — `git describe` is unreliable) |
| **PPL caveat** | New WeaselServer.exe requires user logout/reboot to load (L17/L18) |
| **`/D=` flag** | NSIS silent-install `InstallDir` override. L13/L17: silent install requires explicit `/D=`; `InstallDirRegKey` runs before `.onInit` |

### 3.4 Engineering-skill terms

| Term | Meaning |
|---|---|
| **needs-triage / needs-info / ready-for-agent / ready-for-human / wontfix** | The five triage states (`docs/agents/triage-labels.md`) |
| **CONTEXT.md** (this file) | Domain vocabulary + layout. Skills read this first |
| **ADR** | Architecture Decision Record under `docs/adr/NNNN-*.md` |
| **spec / plan / tasks** | The three SDD artifacts in `.specify/specs/NNN-*/` |
| **L##** | A lessons-learned entry in `.specify/memory/lessons-learned.md` |
| **P# / R#** | A project-specific rule or hard rule from the constitution |
| **origin / kizemo** | Git remotes. `origin = rime/weasel` (read-only); `kizemo = kizemo/fluxing` (the fork) |

---

## 4. Invariants (rules a skill must not break)

These are the **non-negotiables** distilled from the constitution and
incident history. If a change appears to break one of these, escalate
to the user before proceeding.

1. **The user-data path is a one-shot migration, not a rewrite.** P6 +
   P8-waiver: `%AppData%\Rime` is migrated to `%AppData%\Fluxing` exactly
   once, with `HKCU\Software\Fluxing\ImportedFrom` recording the source.
   A re-migration or path change is a roll-back scenario, not a normal
   change. (`project-knowledge.md` §2)
2. **The IPC wire protocol semantics are not for re-invention.** P2: new
   message types require updates in **both** the sender and the receiver,
   and the message catalog in `WeaselIPCData.h`.
3. **Do not block the TSF thread for I/O.** P2. (Repeatedly violated in
   the past; see L10 / L17 / L18.)
4. **NSIS installer script must remain UTF-8 with BOM + CRLF.** L09.
   A BOM-stripped installer is the #1 cause of GBK/UTF-8 confusion in
   silent installs.
5. **Silent install requires explicit `/D=<path>`.** L13 / L17. Unknown
   CLI args are concatenated into `$INSTDIR` and corrupt the registry.
6. **Only the brand-fork branch may diverge from upstream strings/CLSIDs/
   paths.** P8 (scope is the `Fluxing` branch; rebinding happens in
   lockstep, not piecemeal).
7. **The fork must not modify the `librime` / `Boost` / `OpenCC` contents**
   or `.clang-format`. P8 out-of-scope list.
8. **`librime/` is a submodule. Do not edit inside it.** P3 / L10.
   Lua plugin changes go in `thirdparty/librime-lua/` and are re-copied
   on every `build.bat rime`.
9. **All user-facing strings stay in Simplified Chinese.** P2 / P5.
10. **`weasel.props` / `env.bat` / `msbuild*.log` are local-only.**
    Listed in `.gitignore`; never commit (anti-pattern A3, A10).
11. **PowerShell text I/O on CJK files must be byte-level.** L01 / L02 /
    A1. Use `[IO.File]::ReadAllBytes` / `WriteAllBytes` or set
    `[Console]::OutputEncoding = UTF8` and use `Out-File -Encoding utf8`.
    The `lessons-learned.md` file itself is UTF-16 LE with BOM after a
    triple-encoding damage chain — treat it as read-only text.
12. **Cumulative WeaselServer.exe change requires logout or reboot** for
    the new binary to load (PPL protects the running process — L17 / L18).
13. **User-dictionary operations must enter maintenance mode first.**
    `client.StartMaintenance()` → do work → `client.EndMaintenance()`.
    Otherwise `export_user_dict` fails on the leveldb LOCK. (`project-knowledge.md` §A7)
14. **A new `*.custom.yaml` written without the Signature block is not
    trusted** and will be overwritten on the next deploy. Use
    `RimeLeversApi::save_settings` unless you have a reason to bypass it.
15. **Conventional Commits + module scope.** P4. Use one of
    `WeaselTSF / WeaselServer / WeaselDeployer / WeaselSetup / WeaselUI /
    WeaselIPC / RimeWithWeasel / librime / installer / docs / ci /
    fluxing`. CHANGELOG entry under `### 主要更新` for user-visible changes.

---

## 5. Recurring failure patterns (read before debugging)

The constitution §7 and `AGENTS.md` §7 list anti-patterns observed in
this project. The most common ones an engineering skill will hit:

- **A1 / L01**: PowerShell GBK pollution when reading/writing Chinese.
- **A2 / L09**: Patching `output/install.nsi` via string APIs loses the
  BOM and flips line endings.
- **A4 / L10 §2**: Trusting `cmake install(TARGETS rime)` to also install
  the import `.lib` on MSVC — it does not; the post-install manual copy
  in `build.bat` is mandatory.
- **A6 / L10 §5**: Passing unknown CLI args to the silent installer —
  they get concatenated into `$INSTDIR` and corrupt the user-data
  registry key.
- **A7 / L10 §4**: Editing `librime/plugins/lua/` directly — gets wiped
  on the next `build.bat rime`. Edit `thirdparty/librime-lua/` instead.
- **A8 / L10 §6**: Trusting `git describe --tags` for version in
  `build.bat` — set `RELEASE_BUILD=1` in `env.bat` for release builds.
- **A11 / L14**: Installing the x64 Weasel*.exe on x64 Windows — librime
  is Win32-only; the resulting install crashes with `0xC000007B`.
  Install from `output\Win32\*` (x86); `weaselx64.dll` is the only x64
  file shipped.
- **L54**: Before any silent install, clear the `HKLM\Software\Fluxing\Weasel`
  registry key; otherwise the installer uses a stale path from a prior
  install.
- **L18 / L19 / L21**: Shift-modifier key bindings interacting with
  TSF release events. The solution is to set `Shift_L: noop` /
  `Shift_R: noop` in `ascii_composer/switch_key/` and bind the actual
  toggle via `key_binder` instead.
- **userdb LOCK**: any code path that calls
  `RimeLeversApi::export_user_dict` / `import_user_dict` from outside
  the WeaselServer process must use `StartMaintenance` / `EndMaintenance`
  via the IPC client, or it will fail on the leveldb LOCK.

---

## 6. Quick command cheatsheet

```powershell
# Build (inner loop)
xbuild.bat weasel installer

# Build (CI parity / monthly hygiene)
build.bat all

# Run all tests
msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32
test\TestDefaultHotkeys\Release\TestDefaultHotkeys.exe
test\TestResponseParser\Release\TestResponseParser.exe
test\TestWeaselIPC\Release\TestWeaselIPC.exe

# Lint
./clang-format.sh -i

# Check installed path
Get-ItemProperty "HKLM:\SOFTWARE\WOW6432Node\Fluxing\Weasel" -ErrorAction SilentlyContinue
Get-ItemProperty "HKCU:\Software\Fluxing\Weasel" -ErrorAction SilentlyContinue

# Silent install (must pass /D= explicitly; clear registry first)
Remove-Item "HKLM:\SOFTWARE\WOW6432Node\Fluxing" -Recurse -Force
Remove-Item "HKCU:\Software\Fluxing" -Recurse -Force
cmd /c "`"$installer`" /S /D=`"D:\Program Files\fluxing`""

# Trigger deploy
ShellExecuteW(NULL, NULL, L"WeaselDeployer.exe", L"/deploy", NULL, SW_SHOWNORMAL)
```

For the full, dated cheatsheet (including PPL caveats, NSIS silent-mode
flags, and the DictManagementDialog workflow) see
`.specify/memory/project-knowledge.md` §11.

---

## 7. ADRs (Architecture Decision Records)

`docs/adr/` is currently empty. When a skill or a manual decision
produces a record-worthy architectural choice, follow the convention in
`docs/adr/README.md`:

- File: `NNNN-kebab-case-summary.md` (4-digit zero-padded; starts at `0001`)
- Sections: `## Status` (Proposed / Accepted / Superseded),
  `## Context`, `## Decision`, `## Consequences`
- Cite the originating issue/spec number in the header
- Update this file's §3 vocabulary section if the decision introduces
  new domain terms

This file is the *consumer* of ADRs; the ADRs themselves are the
*record*.

---

## 8. Cross-references

- **Operating manual**: [AGENTS.md](AGENTS.md)
- **Project constitution**: [.specify/memory/constitution.md](.specify/memory/constitution.md)
- **Lessons learned (L01–L##)**: [.specify/memory/lessons-learned.md](.specify/memory/lessons-learned.md)
- **Live fork knowledge (dated)**: [.specify/memory/project-knowledge.md](.specify/memory/project-knowledge.md)
- **Upstream code map (snapshot)**: [docs/Fluxing-code-map/00-index.md](docs/Fluxing-code-map/00-index.md)
- **Specs index**: `.specify/specs/` (run `ls .specify/specs/`)
- **Triage label vocabulary**: [docs/agents/triage-labels.md](docs/agents/triage-labels.md)
- **Issue tracker setup**: [docs/agents/issue-tracker.md](docs/agents/issue-tracker.md)
- **Domain-doc consumer rules**: [docs/agents/domain.md](docs/agents/domain.md)

---

*Last updated: 2026-10-22. The §3 vocabulary table and §4 invariants are
the canonical skill-facing reference; everything else defers to the
linked documents.*
