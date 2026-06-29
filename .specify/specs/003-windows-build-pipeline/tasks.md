# 003 — Tasks — Windows Build Pipeline

> Each task: `- [x] TNNN [P?] [Story] Description` (or `[ ]` if pending).

## Phase 1 — Source edits

- [x] T001 [P] [US2] `output/install.nsi` L135/137/139/144/146:
  `$PROGRAMFILES*\Rime` → `$PROGRAMFILES*\Fluxing` (5 sites).
- [x] T002 [P] [US2] `output/install.nsi` BOM: 4× → 1× UTF-8 BOM
  (NSIS 3.x rejects the 4× variant).

## Phase 2 — Shim headers (in-repo)

- [x] T003 [P] [US1] `librime/include/X11/keysym.h` + `keysymdef.h`
  (X.Org MIT, from peer fork `F:\soft\02office\rime\weasel\librime\include\X11\`).
- [x] T004 [P] [US1] `librime/include/utf8.h` + `librime/include/utf8/*.h`
  (Nemanja Trifunovic MIT).
- [x] T005 [P] [US1] `librime/include/darts.h` + `COPYING.darts-clone`
  (Daisuke Okanohara BSD).

## Phase 3 — Build artifacts

- [x] T006 [US1] librime x64 deps: glog, gtest, leveldb, marisa,
  opencc, yaml-cpp (all built, all `8664` machine).
- [x] T007 [US1] librime x64 rime: `librime\dist_x64\lib\rime.{dll,lib}`
  (2.7 MB dll, statically linked against all 6 deps).
- [x] T008 [US1] librime x86 deps: same 6 deps, all `14C` machine.
- [x] T009 [US1] librime x86 rime: `librime\dist_x86\lib\rime.{dll,lib}`
  (2.3 MB dll, all x86).
- [x] T010 [US1] Top-level: `lib/rime.lib` (x86), `lib64/rime.lib`
  (x64), `include/rime_api*.h`, `output/rime.dll` (x64),
  `output/Win32/rime.dll` (x86).
- [x] T011 [US1] weasel x64 (msbuild Release|x64): `output\weaselx64.dll`,
  `output\WeaselServer.exe`, `output\WeaselDeployer.exe`.
- [x] T012 [US1] weasel x86 (msbuild Release|Win32): `output\weasel.dll`,
  `output\Win32\WeaselServer.exe`, `output\Win32\WeaselDeployer.exe`,
  `output\Win32\rime_deployer.exe`, `output\Win32\rime_dict_manager.exe`,
  `output\Win32\rime_patch.exe`, `output\Win32\rime.dll`.
- [x] T013 [US1] `weasel.props` generated from `weasel.props.template`
  via `cscript.exe render.js`.

## Phase 4 — Installer

- [x] T014 [US1/US2] NSIS build: `output\archives\fluxing-0.17.4.0-installer.exe`
  (10.5 MB, lzma-compressed).
- [x] T015 [US1] Copy to `release/fluxing-0.17.4.0-installer.exe`.

## Phase 5 — Spec/plan/tasks (this file)

- [x] T016 [P] [US3] `.specify/specs/003-windows-build-pipeline/{spec,plan,tasks}.md`.

## Phase 6 — Commit + push (next)

- [ ] T017 [US1] Stage all changes, commit
  `feat(fluxing): end-to-end Windows build pipeline (spec 003)`,
  push to `origin/Fluxing`.

## Future TODO (per spec 003, deferred)
- Update `URLInfoAbout`/`HelpLink` in `output/install.nsi` to the actual
  Fluxing GitHub URLs **after the first release is published**.