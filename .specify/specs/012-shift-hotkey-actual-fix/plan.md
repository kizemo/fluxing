# 012 · Plan — Shift 切换中英 / 候选上屏 真正生效

## Summary

This plan delivers a single-file fix to `output/data/default.yaml` that
re-establishes the Shift_L / Shift_R key bindings promised by spec 005
(中英切换 + 上屏第 2/3 候选). The fix is **4 in-place line edits + 4 line
deletions** inside the existing `key_binder/bindings` list, plus a
revision of `test\TestDefaultHotkeys\TestDefaultHotkeys.cpp` to match
this spec's narrowed scope (single-key Shift_L/R only, no Shift+l/r
combination path — per user request 2026-06-30).

The bug originated from spec 005 design.md writing `accept: shift+l`
(lowercase modifier), which librime 1.13.1 silently rejects via
`RimeGetModifierByName()`. The fix uses the canonical
`Shift+Shift_L` / `Shift+Shift_R` forms that match the `KeyEvent` that
TSF actually delivers.

## Technical Context

- **Language / version**: YAML 1.2 (configuration) + C++ (test cpp).
- **Build system**: xmake (primary, per `xbuild.bat weasel installer`)
  for installer; MSBuild via `weasel.sln` for the test binary.
- **Dependencies (unchanged)**: librime 1.13.1 (submodule at
  `librime/src/rime/`), already verified to fail on lowercase
  modifier names in `RimeGetModifierByName` (line 24 of
  `librime/src/rime/key_table.cc`).
- **Storage / files touched**:
  - `output/data/default.yaml` — 4 line edits + 4 line deletions in
    one YAML list.
  - `test/TestDefaultHotkeys/TestDefaultHotkeys.cpp` — drop assertions
    about `Shift+l` / `Shift+r` (lines 33-40, 44-45 in current file);
    add assertions for the new `Shift+Shift_L` / `Shift+Shift_R` forms.
  - `.specify/memory/lessons-learned.md` — L04 勘误 + L16 新增.
  - (no installer changes; this is a config-only fix)
- **Platform**: Windows 8.1 ~ Windows 11; x86 (per L10/L14, librime is
  Win32-only, WeaselServer is x86).
- **Project type**: Windows TSF IME front-end (no source-code changes
  in the main binary in this slice; only the test cpp is touched).
- **Performance**: not applicable.
- **Constraints**:
  - YAML must stay UTF-8 (no BOM, per L11) and CRLF.
  - C++ test file must follow `.clang-format` and be rebuilt via
    `msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32`.
  - Conventional Commits with the `fluxing` scope (per P4 amendment 1).
  - All changes must pass the revised `TestDefaultHotkeys.exe`
    assertions.
  - No new third-party dependency (P3).
- **Scale**: 1 commit, 4 file paths touched.

## Constitution Check (against `.specify/memory/constitution.md` v1.1.0)

| Principle | Pass? | Notes |
|---|---|---|
| I. Intent Before Implementation | ✅ | `spec.md` Goal + 3 prioritized user stories; this plan opens with "re-establish spec 005 promise, single-key only". |
| II. Test-Backed Change | ✅ | Revised `TestDefaultHotkeys.exe` asserts the new `Shift+Shift_L/R` bindings; existing assertions for `Shift+l/r` are dropped (out of scope). |
| III. Spec-Artifact Discipline | ✅ | This directory contains `spec.md` + `plan.md` + `tasks.md`. |
| IV. Structured Clarification | ✅ | User explicitly clarified scope on 2026-06-30 (single-key only, no Shift+l/r). |
| V. Incremental Delivery | ✅ | Single-file YAML fix + test cpp revision. |

| Hard rule | Pass? | Notes |
|---|---|---|
| R1 (intent + acceptance in chat) | ✅ | First message of this turn stated What/Why/How-verified. |
| R2 (no tech words in spec.md) | ✅ | spec.md mentions "librime 1.13.1" only as a citation in the L04 勘误 / L16 lesson context. |
| R3 (priority + US tag in tasks.md) | ✅ | All 8 tasks carry P1/P2 priority and link back to US1-A..US1-D. |
| R4 (Constitution Check in plan.md) | ✅ | This section. |
| R5 (≤ 4 h, 1-3 files per task) | ✅ | T001, T002, T004, T005 each touch exactly one file. |
| R6 (paste test output or document manual verification) | ✅ | T003 mandates running the test binary; T007 mandates reading the log. |
| R7 (one source of truth; spec-check on conflict) | ✅ | spec.md §2.2 is the single source of truth for the binding list. |
| R8 (specs versioned in git) | ✅ | The 3 new documents are committed in the same atomic commit as the YAML fix. |
| R9 (lookup beats memory) | ✅ | Source-of-truth citations: `librime/src/rime/key_table.cc:7-26`, `librime/src/rime/key_event.cc:60-90`, `librime/src/rime/key_event.h:64`, `WeaselTSF/KeyEventSink.cpp:31`. |

## Source-of-Truth Citations

| Claim | File:line | Verified by |
|---|---|---|
| librime modifier names are case-sensitive, only first-letter-capitalized | `librime/src/rime/key_table.cc:7-26` | direct read |
| `KeyEvent::Parse` returns false on bad modifier | `librime/src/rime/key_event.cc:60-90` | direct read |
| `KeyEvent::operator==` compares both keycode and modifier | `librime/src/rime/key_event.h:64` | direct read |
| TSF injects `SHIFT_MASK` when `VK_SHIFT` is held | `WeaselTSF/KeyEventSink.cpp:31` | direct read |
| rime.log records `parse error: unrecognized modifier 'shift'` | `log/rime.weasel.DY-DELL.duanyi.log.*.20260629-215458.*.log` | grep |

## Risk Register

| ID | Risk | Mitigation |
|---|---|---|
| R1 | `Shift+Shift_L` mask is `Shift | Shift` = `Shift`; TSF mask is also `Shift`; should match. | Verified by reading both source files. T003 test confirms the binding is parseable. T007 manual log inspection confirms runtime match. |
| R2 | WeaselServer caches parsed bindings; restart needed for changes to take effect. | `xbuild.bat installer` triggers an uninstall + reinstall, or `WeaselDeployer.exe /deploy` + restart WeaselServer. |
| R3 | User has `default.custom.yaml` with schema_list patch; could conflict. | Verified: `D:\Program Files\fluxing\user1\fluxing\default.custom.yaml` only patches `schema_list`, not `key_binder`. No interaction. |
| R4 | Removing `Shift+l` / `Shift+r` paths is permanent (until user re-adds). | User confirmed this is intended (2026-06-30 chat). |
| R5 | Test cpp needs rebuild (msbuild, not xmake). User must run msbuild before the test. | T003 explicitly runs `msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32`. |

## Deliverable

- 1 commit on the `Fluxing` branch.
- The commit message uses the `fluxing:` scope per P4.
- No upstream PR (per P8 waiver, brand-fork only).
- Optional lightweight tag (user decides; this slice is small).