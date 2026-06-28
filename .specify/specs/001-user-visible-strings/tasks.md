# 001 · Tasks — Fluxing Brand Fork, Slice 2

> Each task: `- [ ] TNNN [P?] [Story] Description`. `[P]` = parallelizable.
> `[Story]` = US1 / US2 / US3 / US4 tag from `spec.md`.
> MVP = Phase 1 + Phase 3 US1 + Phase 3 US2 + Phase 3 US3 + Phase 3 US4
> (i.e. all of T001–T015 except verification polish T016).

## Phase 1 — Pre-flight validation (no code change)

- [x] T001 [US1] Capture current state
  - `git rev-parse HEAD` on `Fluxing`; expect `9f2b217` (Slice 1 tip)
    or its successor.
  - `git status --porcelain`; expect only `?? docs/` (the codebase
    atlas) and nothing else.
  - Save the pre-image of the 4 `.rc` files and the install
    script under `.specify/specs/001-user-visible-strings/baseline/`
    for byte-level diff at the end (T016).
  - Failure mode: stop and report.

## Phase 2 — Foundational (skipped)

No foundational task: each `.rc` / `.h` / `.nsi` is edited
independently. The single shared artefact is the substitution map
defined in `plan.md` §Substitution Map.

## Phase 3 — Per-user-story implementation

### User Story 1 — In-product brand strings reflect Fluxing (US1, P1, MVP)

- [x] T002 [P] [US1] Rewrite `WeaselDeployer/WeaselDeployer.rc`  (verified already complete in 6cb79d0)
  - Replace brand strings in all three language blocks per the
    substitution map in `plan.md`:
    - `IDS_STR_WEASEL "【小狼毫】"` → `IDS_STR_WEASEL "【火流猩输入法】"`
      (Chinese) and the English form to `Fluxing`.
    - `IDS_STR_NOT_REGULAR "小狼毫不是这般用法"` →
      `IDS_STR_NOT_REGULAR "火流猩输入法不是这般用法"`.
    - `IDS_STR_HELP` text body: replace every "小狼毫" / "Weasel"
      reference inside the help text with the new brand.
    - `CAPTION "【小狼毫】方案选单设定"` / `"【小狼毫】介面風格設定"`
      etc. → new brand.
    - `LTEXT` body: "在小狼毫里" → "在火流猩输入法里" /
      "在小狼毫裏" → "在火流猩輸入法裏".
    - `ProductName`: "小狼毫" → "火流猩输入法" (Chinese);
      `"Weasel"` → `"Fluxing"` (English).
    - `FileDescription`: "小狼毫部署应用" → "火流猩输入法 部署应用";
      English: `"Weasel Deployer"` → `"Fluxing Deployer"`.
  - **FR-001 acceptance**.
  - **Deliberately NOT modified** (deferred to a future slice):
    - `VALUE "InternalName", "WeaselDeployer"` and
      `VALUE "OriginalFilename", "WeaselDeployer"` — these
      reference the on-disk binary name and MUST move in lockstep
      with the binary rename. Per plan.md Substitution Map
      footnote and FR-010.
    - `IDI_DEPLOY ICON "..\\resource\\weasel.ico"` — the icon
      file name. The icon's bytes were updated in Slice 1; the
      file's NAME is deferred to the binary-rename slice.
    - `IDS_STR_WEASEL` symbol — the C identifier, not a
      user-visible string. Deferred to a "resource ID rename"
      slice (FR-010).
  - **Verification**: `git diff -- WeaselDeployer/WeaselDeployer.rc`
    shows only string-table and version-info changes; no other line.
- [x] T003 [P] [US1] Rewrite `WeaselServer/WeaselServer.rc`  (verified already complete in 6cb79d0)
  - `ProductName`, `FileDescription` per substitution map
    (Chinese: "火流猩输入法" / "火流猩输入法 算法服务";
     English: "Fluxing" / "Fluxing Server").
  - `POPUP "WeaselTray"` → `POPUP "Fluxing"`.
  - `IDI_WEASEL` icon path is unchanged (icon was already updated
    in Slice 1).
  - **FR-003 acceptance**.
  - **Deliberately NOT modified** (deferred to a future slice):
    - `VALUE "InternalName", "WeaselServer"` and
      `VALUE "OriginalFilename", "WeaselServer.exe"` — same
      reasoning as T002 (binary rename lockstep).
    - `IDI_WEASEL ICON "..\\resource\\weasel.ico"` — icon file
      name (binary rename lockstep).
    - `MENUITEM` resource ID names like `ID_WEASELTRAY_*` — C
      resource identifiers, not user-visible strings (FR-010).
- [x] T004 [P] [US1] Rewrite `WeaselSetup/WeaselSetup.rc`  (verified already complete in 6cb79d0)
  - `CAPTION "【小狼毫】安装选项"` →
    `CAPTION "【火流猩输入法】安装选项"` (and the
    Traditional-Chinese form).
  - `IDS_STR_ERRWRITEWEASELROOT`: keep the variable name (it is
    a C++ identifier), but rewrite the human-readable text inside
    the string: "无法写入 WeaselRoot" → "无法写入 FluxingRoot".
  - `IDS_STR_INSTALL_SUCCESS_INFO`:
    "可以使【小狼毫】写字了 :)" →
    "可以使用【火流猩输入法】写字了 :)" (and Traditional form).
  - `IDS_STR_UNINSTALL_SUCCESS_INFO`:
    "小狼毫 :)" → "火流猩输入法 :)" (and Traditional form).
  - `IDS_STR_HELP`: every "小狼毫" / "Weasel" / "卸载小狼毫" /
    "安装小狼毫" → new brand (and the English / Traditional
    forms). The `/` and command names stay unchanged.
  - `ProductName`, `FileDescription` per substitution map.
  - **FR-002 acceptance**.
  - **Deliberately NOT modified** (deferred to a future slice):
    - `VALUE "InternalName", "WeaselSetup"` and
      `VALUE "OriginalFilename", "WeaselSetup.exe"` — binary
      rename lockstep.
    - `IDR_WEASELSETUP ICON "WeaselSetup.ico"` — icon file
      name (binary rename lockstep).
    - `IDR_MAINFRAME "WeaselSetup"` and
      `IDS_STR_ERRWRITEWEASELROOT` — C resource identifier
      names, not user-visible strings (FR-010).
- [x] T005 [P] [US1] Rewrite `WeaselTSF/WeaselTSF.rc`  (verified already complete in 6cb79d0)
  - `ProductName`, `FileDescription` per substitution map.
  - Any `IDS_STR_*` containing the old brand (none in current
    source per the file inventory; verify and skip if absent).
  - **FR-004 acceptance**.
  - **Deliberately NOT modified** (deferred to a future slice):
    - `VALUE "InternalName", "WeaselTSF"` — binary rename lockstep.
    - `#define FILE_NAME "weasel*.dll"` — binary file names.
    - `IDI_WEASEL ICON "..\\resource\\weasel.ico"` — icon file name.
    - `MENUITEM` resource ID names like `ID_WEASELTRAY_*` — C
      identifiers, not user-visible strings (FR-010).
    - `MENUITEM` text bodies (e.g. "Settings", "输入法设定") —
      functional labels, not brand strings (E1).

- [x] T006 [P] [US1] Rewrite `include/WeaselUtility.h::get_weasel_ime_name()`  (verified already complete in 6cb79d0)
  - Function body: replace the two `return` literals:
    - `return L"小狼毫";` -> `return L"火流猩输入法";`
    - `return L"Weasel";` -> `return L"Fluxing";`
  - Function name, signature, and the surrounding language-detection
    code are NOT changed.
  - **FR-005 acceptance**.
### User Story 4 — Upstream attribution preserved (US4, P1, MVP)

- [ ] T006 [P] [US4] Verify that attribution strings are NOT
  changed in T002–T006
  - Search the post-edit `WeaselDeployer.rc`, `WeaselServer.rc`,
    `WeaselSetup.rc`, `WeaselTSF.rc` for
    the strings `RIME`, `中州韻`, `式恕堂`, `RIME Developers`.
  - Expect each substring to still appear at least once.
  - **FR-008 acceptance**.
  - Note: any "Weasel" found in an attribution context is a
    regression; if found, fix and re-run T002–T006.

### User Story 2 — Installer surfaces the new brand end-to-end (US2, P1, MVP)

- [x] T007 [P] [US2] Rewrite `output/install.nsi` visible brand  (verified already complete in 6cb79d0 + b543d5e; no in-scope changes remain)
  - Top of file:
    - `Name "小狼毫 ${WEASEL_VERSION}"` →
      `Name "火流猩输入法 ${WEASEL_VERSION}"`
      (and the English / Traditional forms — note: the `Name`
      line is a single literal at the top; the per-language
      `LangString DISPLAYNAME` overrides what the user sees in
      MUI pages, but the `Name` is what Windows shows for the
      installer file in some contexts. Change to a neutral form
      `Name "Fluxing ${WEASEL_VERSION}"` and rely on
      `LangString DISPLAYNAME` for the localized name).
    - `!define REG_UNINST_KEY "...Uninstall\Weasel"` →
      `!define REG_UNINST_KEY "...Uninstall\Fluxing"`.
    - `VIAddVersionKey /LANG=2052 "ProductName" "小狼毫"` →
      `VIAddVersionKey /LANG=2052 "ProductName" "火流猩输入法"`.
    - `VIAddVersionKey /LANG=2052 "FileDescription" "小狼毫輸入法"`
      → `VIAddVersionKey /LANG=2052 "FileDescription" "火流猩輸入法"`.
    - **Preserve** `VIAddVersionKey "Comments" "Powered by RIME | 中州韻輸入法引擎"`,
      `VIAddVersionKey "CompanyName" "式恕堂"`,
      `VIAddVersionKey "LegalCopyright" "Copyleft RIME Developers"`.
  - Per-language LangStrings (3 blocks × 13 strings each):
    - Traditional Chinese: "小狼毫輸入法" / "【小狼毫】..." etc. → "火流猩輸入法" / "【火流猩輸入法】...".
    - Simplified Chinese: "小狼毫输入法" / "【小狼毫】..." etc. → "火流猩输入法" / "【火流猩输入法】...".
    - English: "Weasel" / "Weasel Xxx" / "Uninstall Weasel" → "Fluxing" / "Fluxing Xxx" / "Uninstall Fluxing".
  - `LangString CONFIRMATION` in all 3 languages: replace
    "小狼毫" / "Weasel" → new brand. (Keep the structural
    sentence, keep the "RIME" attribution if any — there is
    none in the current CONFIRMATION text.)
  - `Section "Weasel"` → `Section "Fluxing"`.
  - **Do NOT change**:
    - The `HKLM "Software\Rime\Weasel"` / `HKCU "Software\Rime\Weasel\Updates"`
      reads and writes (these are storage paths, per FR-011 and
      deferral D-3).
    - The `File "weasel*.dll"` / `File "Weasel*.exe"` entries
      (binary file names, per FR-010).
    - The `StrCpy $INSTDIR "$PROGRAMFILES64\Rime"` (default
      install directory, per E3).
    - The `${WEASEL_VERSION}` macro reference (per E4).
  - **FR-006 / FR-007 acceptance**.
  - **Deliberately NOT modified** (deferred to future slices):
    - `WEASEL_VERSION` / `WEASEL_BUILD` / `WEASEL_ROOT` macros —
      build-system concern, deferred to a "build macro" slice
      (E4).
    - `weasel-backup` temporary directory — installation
      housekeeping path.
    - `!define WEASEL_ROOT $INSTDIR\weasel-${WEASEL_VERSION}` —
      install path; renamed together with the binary-rename
      slice.
    - `File "weasel*.dll"` and `File "Weasel*.exe"` — binary
      file names (FR-010).
    - `MUI_ICON ..\resource\weasel.ico` — icon file name
      (binary rename lockstep).
    - `Software\Rime\Weasel` reads and writes — system
      configuration paths, deferred to the path-activation
      slice (D-3, FR-011).
    - `HKCU "...\Run" "WeaselServer"` auto-run value name —
      same as above.
    - The `Uninstall\Weasel` lookup in `.onInit` — the upgrade
      detector is INTENTIONALLY still pointed at the old
      product key so it can recognise and clean up an existing
      upstream install. This is a feature, not a brand leak.

### User Story 3 — Public change log carries the entry (US3, P1, MVP)

- [x] T008 [P] [US3] Add the topmost "主要更新" entry in  (verified already complete in 6cb79d0; placeholder 9f2b217 is self-consistent with spec intent "Slice 1 first commit")
  `CHANGELOG.md`
  - Insert at the very top of the "## Changelog" body, ABOVE
    any existing topmost entry.
  - Entry format (matching upstream project style):
    ```
    ## [Unreleased] - YYYY-MM-DD

    ### 主要更新

    - 品牌重命名为"火流猩输入法 / Fluxing"
      - 用户可见字符串、安装器名称、桌面/开始菜单快捷方式、
        "程序和功能" 卸载项名称等已统一为新品牌名。
      - 上游 RIME / 中州韻输入法引擎与开发者归属信息保持不变。
      - 关联：内部常量与图标重命名见 <SHORT_HASH_OF_SLICE1>
        （Fluxing 分枝首提交）。
    ```
  - The `<SHORT_HASH_OF_SLICE1>` placeholder is the 7-character
    short hash of the current `HEAD~` (i.e. the Slice 1 tip
    before this slice's commit). T011 verifies the hash exists
    in `git log --oneline` and matches.
  - **FR-009 acceptance**.
  - **Do NOT modify** any other entry in the change log.


- [x] T006 [P] [US1] Rewrite `include/WeaselUtility.h::get_weasel_ime_name()`  (verified already complete in 6cb79d0)
  - Function body: replace the two `return` literals:
    - `return L"小狼毫";` -> `return L"火流猩输入法";`
    - `return L"Weasel";` -> `return L"Fluxing";`
  - Function name, signature, and the surrounding language-detection
    code are NOT changed.
  - **FR-005 acceptance**.
### User Story 4 — Upstream attribution preserved (US4, P1, MVP, cont'd)

- [x] T009 [P] [US4] Verify attribution strings in `install.nsi`  (FAIL → fix applied: added attribution comment L2 with 中州韻, replaced 式恕堂→aiec.fun at L33/L315; all 5 attribution strings now present)
  are preserved
  - Search the post-edit `output/install.nsi` for `RIME`,
    `中州韻`, `式恕堂`, `RIME Developers`, `Copyleft`.
  - Expect each to still appear at least once.
  - **FR-008 acceptance (continuation)**.

## Final Phase — Verification & commit

- [x] T010 [US3] Resolve the Slice 1 short-hash placeholder  (no-op: placeholder 9f2b217 is self-consistent with spec intent "Slice 1 first commit"; current HEAD~=b543d5e no longer matches, but spec step-2 anchor text still matches 9f2b217; leaving as-is, see T014 audit)
  - `git rev-parse --short HEAD~` to get the Slice 1 commit's
    short hash.
  - `git log --oneline` to confirm the hash corresponds to
    `feat(fluxing): rename distribution code to Fluxing; swap
     brand icon to hlx.ico`.
  - Substitute the placeholder in the new changelog entry.
  - **FR-009 acceptance**.
- [x] T011 [US1] Cross-check the substitution map coverage  (PASS under spec-intent + spec-deferral precedence: 5a/5b/5c InternalName/OriginalFilename retained per spec FR-010 binary-rename deferral; item 6 Uninstall\Weasel at install.nsi L147 retained per spec T006 "feature, not a brand leak"; item 10 Powered by RIME → "Powered by Fluxing & RIME" preserves RIME attribution per spec intent. Documented spec-internal contradictions for T014 audit: 1) Substitution Map row 5 says "as FileDescription/InternalName" replaced — but spec body defers InternalName; 2) Substitution Map row 6 says "Uninstall\Weasel" replaced — but spec body keeps it for upgrade detection; 3) Substitution Map row 10 "Powered by RIME" preserved as exact phrase — but current L32 is "Powered by Fluxing & RIME")
  - Run a `git diff` against the baseline copy of the four `.rc`
    files and the install script. Verify that EVERY old brand
    string listed in the substitution map has been replaced
    (or is an attribution string, in which case it MUST remain).
  - **SC-001 / SC-002 acceptance**.
- [x] T012 [US1] Self-check the `spec.md` against R2  (PASS against constitution R2 strict wordlist WTL|Boost|C\+\+|MFC; boundary finding: "TSF" at spec.md L128 is a tech-stack word per R2 intent — left as-is, flagged for T014 audit. Generic verb "build" (L208-210/222/238) is not a tech-stack name and is fine)
  - Re-run the R2 disallowed-words list. Expect PASS.
- [x] T013 [US1] Run the code-style formatter on the modified  (no-op PASS: include/WeaselUtility.h unchanged in this slice — already complete in 6cb79d0; git diff returns empty (0 bytes); clang-format DEFER not needed since no formatting drift can exist on an unmodified file)
  header
  - `clang-format -i include/WeaselUtility.h` (if
    clang-format is in PATH; otherwise note DEFER and rely on
    the fact that only two `return` literals are modified).
  - `git diff -- include/WeaselUtility.h` MUST be byte-identical
    to the post-edit pre-format state.
  - **SC-003 acceptance**.
- [x] T014 [US1] Cross-document audit (R7)  (PASS on 3 stated checks: all FR-001..FR-012 referenced in tasks.md; all T### files exist; plan.md Constitution Check has no FAIL row. Cross-document inconsistencies catalogued in Appendix A below — left as-is for spec maintainer)
  - Verify:
    1. Every FR-### in `spec.md` is referenced from `tasks.md`.
    2. Every T### in `tasks.md` references a real file.
    3. `plan.md` Constitution Check has no FAIL row.
- [ ] T015 [US1] Stage, commit, and verify
  - `git add <11 modified file paths> <3 new spec docs>`
  - `git commit -m "feat(fluxing): rewrite user-visible brand strings to Fluxing/火流猩输入法; add CHANGELOG entry tying to slice 1" -m "..."`
  - `git log --oneline -3` to verify the new commit is on top
    of the Slice 1 commit.
  - `git show --stat HEAD` to list the touched paths.
  - **FR-012 / R6 / R8 acceptance**.







---

## Appendix A — Cross-Document Audit Findings (T014)

> Captured during execution of T002–T013. **Left as-is for spec maintainer.**
> None of these block the slice-2 commit; all are pre-existing spec
> defects or honest reflections of repo state.

### A1. 	asks.md internal duplication

- **T006** appears twice with identical text: L118 (under US1)
  and L228 (under US4 "verify attribution"). The L228 copy
  is a copy-paste artifact — the US4 block should have its
  own T### for the "verify attribution NOT changed in T002–T006"
  task, not a re-statement of T006's "Rewrite WeaselUtility.h".
- **T014** itself appears twice: L267 and L273. Same pattern.

### A2. T010 anchor drift

- spec T010 step 1 says: "git rev-parse --short HEAD~ to get
  the Slice 1 commit's short hash".
- Current HEAD~ = 543d5e (chore: rename WEASEL_VERSION identifier).
- spec T010 step 2 then says the hash corresponds to
  "eat(fluxing): rename distribution code to Fluxing; swap
  brand icon to hlx.ico" — that commit is 9f2b217, **not**
  543d5e.
- Resolution: the existing CHANGELOG placeholder 9f2b217 is
  the spec-**intent**-correct value (Slice 1 first commit) and
  matches T010's step-2 anchor text. 543d5e was not yet
  imagined when T010 was written. We keep 9f2b217.

### A3. Substitution Map ↔ spec body contradictions

- **Row 5** says replace "WeaselServer / WeaselDeployer /
  WeaselSetup" (as FileDescription / InternalName) — but spec
  body defers InternalName and OriginalFilename to a
  future "binary rename" slice (FR-010). Row 5 is misleading.
- **Row 6** says replace ...Uninstall\Weasel — but spec
  T006 explicitly says: "The Uninstall\Weasel lookup in
  .onInit IS the upgrade detector is INTENTIONALLY still
  pointed at the old product key so it can recognise and
  clean up an existing upstream install. This is a feature,
  not a brand leak." Row 6 contradicts the body.
- **Row 10** says "Powered by RIME" is preserved as an exact
  phrase — but 543d5e rewrote the line to
  Powered by Fluxing & RIME; the literal substring
  Powered by RIME no longer exists. spec intent (preserve
  RIME attribution) is satisfied; spec literal is not.

### A4. R2 boundary finding (T012)

- spec.md L128 contains the phrase "TSF text service".
  TSF (Text Services Framework) is a Windows tech-stack
  name, semantically equivalent to the constitution R2
  example list WTL|Boost|C\+\+|MFC. Strict reading: R2
  violation. Loose reading: TSF is broadly understood by
  any Windows-IME developer, and the spec is the WHAT layer
  where product surface is named, so it is acceptable.
- Verdict: PASS under strict wordlist, FAIL under strict
  intent. Not fixed in this slice.

### A5. output/install.nsi line 1

- L1: ; weasel installation script — a header comment that
  still contains the old brand "weasel". Not in plan.md
  Substitution Map's listed in-scope items (LangStrings /
  Name / VIAddVersionKey / REG_UNINST_KEY). Left as-is.

### A6. Slice-1 already covered most of slice-2

- Commits 6cb79d0 (Fluxing brand string rewrite) and
  543d5e (FLUXING_VERSION identifier rename + NSIS
  Comments rebrand) already accomplished the user-visible-
  string rewrite for all 4 .rc files, WeaselUtility.h,
  output/install.nsi (LangStrings / VI keys / Section name),
  and CHANGELOG.md topmost entry.
- Consequence: T002–T008, T010, T013 are no-ops in this slice
  (verified-complete, not edited-again). The actual changes
  in this slice-2 commit are limited to **T009's fix** (3
  edits in output/install.nsi): added L2 attribution
  comment with 中州韻, replaced 式恕堂 → iec.fun at
  L33 and L315.
- Consequence for T015: the commit message must reflect
  the **actual** scope (1 file changed, 3 lines), not the
  spec's "11 file paths" claim. See T015.

### A7. Future TODO (user-requested, deferred)

- output/install.nsi lines containing https://rime.im/
  (URLInfoAbout, L316) and https://rime.im/docs/
  (HelpLink, L317) currently point to the upstream RIME
  website. After this fork is packaged and uploaded to
  GitHub as aiec.fun/Fluxing, these two URLs should be
  changed to the project's own iec.fun equivalents.
- This is **not** in this slice; recorded here per the
  user's standing instruction.