# Verification Report — v0.19.0.35 — 5 Axis — Phrase + UserDict fix

**Date**: 2026-07-19 16:42
**Verifier**: verification-before-completion subagent
**Subject**: Fluxing `release/fluxing-0.19.0.35-installer.exe` real verification (md5 `df94e18f6de6f7d2e8fbf5fb65a5c440`)

---

### Verdict: **HOLD** — DO NOT SHIP

**Critical finding**: The v0.19.0.35 installer contains a **stale binary** (md5 `389610fb` = v0.19.0.34-era build from 20:37 on Jul 18). The 20:51 `fa19604` commit message claims Phrase 4 P0 fixes (TreeView, ImmAssociateContext, default-first-select, keyboard handler, round-corner delete) but the new source has a **C++ syntax error** that prevents compile → binary never refreshed.

---

### 5-axis verification

| Axis | Status | Evidence |
|---|---|---|
| **1. build (incremental)** | **PASS** (silent stale) | `_buildflow.cmd` exit OK; binaries untouched (timestamp `WeaselServer.exe = 20:37` < source edit `20:47:34` < commit `fa19604 20:51:28`) |
| **1'. build (force rebuild)** | **FAIL** | `msbuild WeaselServer.vcxproj /t:Rebuild`: `PhrasesDialog.cpp(813,20) C2601 PopulateListImpl 本地函数定义是非法` + `(834,21) C2601 CenterOnPrimaryMonitor` + `(310,66) C1075 unmatched {`. Compile aborted; **no fresh binary produced**. |
| **2. unit tests** | **PASS (irrelevant to new source)** | `TestPhrasesDialog` 69/69 + `TestUserDictionary` 26/26 + `v0_19_0_32_e2e` 78/0 — BUT these tests build from sources at different timestamps (their `.vcxproj` files reference same `.cpp` but compile via separate `test/` projects) and never compile `WeaselServer/PhrasesDialog.cpp` into the `WeaselServer.exe` that gets installed. So test PASS ≠ binary correctness. |
| **3. 真 user flow 4 hotkey** | **FAIL (binary evidence)** | 4 hotkeys work in user flow (per prior turn subagent) but the binary shipped has the **RoundRectRgn + SetWindowRgn still active** (visible 1 hit each in PE imports). fa19604 commit message explicitly says "删 SetWindowRgn 圆角" — but binary retains it, confirming 20:37 stale binary. |
| **4. Phrase 4 P0 真 verify** | **FAIL** | (4a) `SysTreeView32`: 0 hits → no TreeView control. (4b) `ImmAssociateContext` + `ImmCreateContext`: 0 hits each → IME never wired. (4c) `ListView_SetItemState` selection: 0 → no default-first-select. (4d) Keyboard handler: source has 5 `case VK_*` lines but binary **never recompiled** so v0.19.0.32 老版 (cd6f61a9) 行为 仍 active. (4e) Chinese IME: 0 IMM API hits. |
| **5. install.nsi L72-fix 真 work 概率** | **MEDIUM-LOW** | L72-fix code (commit 9b3e0824, file `output/install.nsi` ~line 425-505) + retry-loops (24239f49) are syntactically valid and present in repo HEAD, but **they're protecting against a problem (mmap-lock-during-File) that was masked by the compile-error** — installer runs, taskkill retries, L72-fix Rename-then-File all execute, but they reproduce the same **stale 20:37 binary every time** because `output/Win32/WeaselServer.exe` itself is stale. So L72-fix **silently succeeds** writing the same old binary → user sees zero change → **真 root cause = stale binary, not installer lock race**. |

---

### Detailed evidence

#### Timeline contradiction

| Time | Event |
|---|---|
| 20:31:50 | commit `c8ca88fc` (UserDict kDialogH 480→600) |
| 20:32:20 | commit `2d3ab1db` (UserDict WM_KILLFOCUS whitelist) |
| 20:32:54 | commit `17274cf7` (UserDict Import/Export real picker) |
| 20:35:57 | commit `12ce8c75` (UserDict FlushSave immediate) |
| **20:37** | `output/Win32/WeaselServer.exe` built (md5 `389610fb`) |
| **20:47:34** | `WeaselServer/PhrasesDialog.cpp` source edit (fa196049 changes) |
| **20:50** | NSIS rebuild `release/fluxing-0.19.0.35-installer.exe` (md5 `df94e18f`) |
| 20:51:28 | commit `fa19604` "v0.19.0.35 - 修 Phrase 4 P0 bug + 恢复 v0.19.0.25 UX" |

**Issue**: NSIS rebuild at 20:50 happened **3 min AFTER** source edit at 20:47:34 — that suggests NSIS triggered a re-link. But **the source edit introduced a syntax error** so the vcxproj didn't actually recompile; NSIS just bundled the still-stale 20:37 binary (389610fb unchanged across all four extract dirs v9, v9a, _extract_v8, etc.).

#### The syntax error in source

`WeaselServer/PhrasesDialog.cpp` line 337-345 (the `SavePhrases` lambda):

```cpp
auto escape = [](const std::wstring& s) {
  std::wstring r;
  for (wchar_t c : s) {
    if (c == L'\\') r += L"\\\\";
    else if (c == L'"') r += L"\\\"";
    else r += c;
  }
  return r;
}        // <-- MISSING SEMICOLON HERE
```

The lambda ends with `}` (closes lambda body) but is missing the trailing `;` that terminates the declaration. Since C++11 lambdas only need `};` if assigned to a variable declared with `auto x = [...](...){...};`. The bare `}` is a syntax error and MSVC aborts the translation unit at line 310 (the `SavePhrases` signature).

#### Stale binary reverse-evidence

`fa196049` commit message states:
> 2. Show() 删 SetWindowRgn 圆角...

But scanning the installed binary ASCII strings:

```
           CreateRoundRectRgn: 1   <-- still present, commit said deleted
                 SetWindowRgn: 1   <-- still present, commit said deleted
            ImmAssociateContext: 0 <-- commit said added, absent
               ImmCreateContext: 0 <-- commit said added, absent
                 SysTreeView32: 0 <-- commit message implies TreeView, absent
```

Round-corner + IMM + TreeView APIs should have been removed/added respectively. Reverse pattern proves the binary **never received fa19604 changes**.

#### L72-fix (Axis 5) install.nsi analysis

L72-fix (commit 9b3e0824, "Rename-then-File 应用到 WeaselServer.exe/Deployer/Setup") + retry-loops (24239f49) are **correct patterns** on their own:

```nsi
${For} $R9 1 3
  ExecWait 'taskkill /F /IM WeaselServer.exe /T'
  Sleep 2000
${Next}
```

```nsi
; L72-fix Rename + try + REBOOTOK Delete
File "Win32\WeaselServer.exe"  (with prior Rename to .old.tmp + try + IfErrors 0)
```

**However**: L72-fix protects against "mmap-locked old binary resists overwrite" — if the source were good, it would correctly swap fresh binary in. With broken source, the swap is no-op (same stale binary → same stale bytes). L72-fix is **conditional correctness** — it depends on `output/Win32/WeaselServer.exe` being up-to-date.

**Real install.nsi strengthening recommendation** (for next Phase D — not in this scope):

1. Add post-install `certutil -hashfile` check that compares installed `WeaselServer.exe` md5 against expected `389610fb...wait ACTUAL expected should be the v0.19.0.35 source. If mismatch → abort + message "stale binary in installer, refuse to ship"`.
2. Or, simpler: the buildflow NSIS stage should **fail-fast** if `output/Win32/WeaselServer.exe` mtime < `WeaselServer/PhrasesDialog.cpp` mtime.
3. Or pre-tag build verification: a `git grep -l "binary.*stale\|TODO.*rebuild" output/Win32/ || warn` style check.

---

### Failure analysis (HOLD root cause)

| # | Fail | File:Line | Cause |
|---|---|---|---|
| 1 | **Syntax error in PhrasesDialog.cpp** | `WeaselServer/PhrasesDialog.cpp:337-345` (lambda `escape` 缺 `;` 终止符) | fa19604 commit 编写 source 时漏 `;` → MSVC C2601 abort |
| 2 | **Compile silently aborted, link step skipped** | `WeaselServer/WeaselServer.vcxproj` msbuild `/t:Build` (incremental) | MSBuild incremental build saw vcxproj mtime unchanged (Jul 15) → 19:45; but PhrasesDialog.cpp mtime changed 20:47. Incremental still tried to recompile one .obj; that obj-compile failed; link step proceeded with **stale** `.obj` from prev build → linked stale binary that **looks** updated because pdb/timestamp get refreshed but actual code didn't. |
| 3 | **NSIS rebundled stale binary** | `output/install.nsi` (post-24239f49) | NSIS ran with `output/Win32/WeaselServer.exe` (md5 389610fb) → installer `df94e18f` packs the same stale binary |
| 4 | **commit fa196049 lacked any verify-by-build-evidence gate** | n/a | Commit message claims "Verification 5/5 PASS" with "md5 parity" but md5 == md5 just means "binary unchanged", not "binary correct". A **post-save build gate** that re-extracts installer → md5 WeaselServer.exe and asserts it differs from `output/Win32/WeaselServer.exe` to detect stale relink is needed. |
| 5 | **Phase 1 subagent root-cause analysis missed this** | `report-4bug-v0.19.0.35-fix.md` (referenced by prior turn) | Prior subagent only checked "md5 src == md5 extracted" (= true for both stale and fresh), did not PE-import-scan to confirm Phrase fixes compiled into binary. |

---

### L100+ lessons to flush (after fix ships)

- **L100-Z1**: "Binary md5 parity == no-op" is **not** a verification result. PE import table ASCII-string scan for **claimed-added / claimed-removed APIs** is the real binary-evidence gate.
- **L100-Z2**: Build error must `exit 1` loudly. MSBuild `/t:Rebuild` returning nonzero is signal; `_buildflow.cmd` should `if errorlevel 1 exit /b 1`.
- **L100-Z3**: NSIS Stage must `if not exist output\Win32\WeaselServer.exe exit /b 1` and `if "%~t1" LSS "%~t2"` (mtime-deps) before invoking makensis.
- **L100-Z4**: Commit message "Verification 5/5 PASS" with no concrete build-output log + no binary-evidence PE scan is **performative**, not real. Add a "binary evidence" subsection with `strings <binary> | grep <new_api>` output.

---

### Action required (HOLD → fix path)

1. **`WeaselServer/PhrasesDialog.cpp` line 345** — add `;` after `}` to terminate the lambda declaration. (Note: subagent only verifies; the actual edit must be done by a subsequent fix-Phase subagent or by human reviewer, with full re-build + re-NSIS + re-verify.)
2. Force clean rebuild: `msbuild WeaselServer\WeaselServer.vcxproj /t:Rebuild` → confirm Exit 0 + new binary mtime + new md5 ≠ `389610fb`.
3. Re-run this 5-axis verification → confirm Axis 4 binary evidence (ImmAssociateContext ≥1, SysTreeView32 ≥1, SetWindowRgn = 0).
4. Re-NSIS `release/fluxing-0.19.0.35-installer.exe` with corrected binary.
5. Add a binary-evidence gate (L100-Z1 / Z4) to the verification-before-completion skill so this class of "md5-parity-but-stale-binary" can never recur.

**Until then: HOLD — do not ship v0.19.0.35 to any user.**
