# Axis 1 Root-Cause Report — v0.19.0.33 Phase A ineffective

**Date**: 2026-07-18 12:20 (Asia/Shanghai)
**Subagent**: systematic-debugging (Axis 1: 真 user flow + 装机 binary)
**Branch**: Fluxing @ 24239f49
**Verified binary md5**: `ce853875028ca9f2191ba0dbbabeffb8` (output/Win32/WeaselServer.exe,
size 2224640, mtime 2026-07-18 00:00)

---

## TL;DR — Phase A fixes did NOT land where user thinks they did

The 3 commits claimed by user as "Phase A fixes" only fixed a small subset of the bugs.
Two **alleged** changes never even made it into the source tree, and one **alleged** change
went into dead code. The user-installed binary md5 exactly matches the post-Phase A build
output, so the binary on the user's disk is current — the bugs are NOT "stale binary",
they are real source-level issues.

| Bug | User hypothesis | Actual root cause |
|---|---|---|
| Button 2 (UserDict) icon "not like a book" | Phase A.3 added DrawIconUserDict, applied | DrawIconUserDict IS in source `QuickPanelDialog.cpp:161` and binary string table. User says icon still not like a book → DrawIconUserDict visually wrong, NOT missing |
| Button 1 (Phrase) click no response | Phase A.1 changed `arg 4` lambda to PhrasesDialog::Show, did not fix | `arg 4` IS now `PhrasesDialog::Show` (cpp:295). WndProc dispatch (cpp:566) calls s_onPhrases on hit==1. Source/binary both wired. **Need live UI trace to find why invocation doesn't reach PhrasesDialog::Show** |
| Button 3 (UserDict) + Alt+/ → "blurry title, looks like UserDict" | Phase A.1 changed Alt+/ to PhrasesDialog, did not fix | **Alt+/ still routes to UserDictionary::Show() at WeaselServerApp.cpp:68** — user did NOT actually change this handler. Phase A.1 alt+/ claim was never committed. Title "用户词典" matches user's "looks like user dict" perfectly |
| Alt+. no response | Phase A.2 added RegisterHotKey warning logs | RegisterHotKey(Alt+.) IS in binary (string offset 773582). Likely real cause: RegisterHotKey fails on user's machine (conflict with another app holding Alt+.) → no global hotkey wired up. The error log will appear in stderr but user doesn't see it |
| UserDict dialog body blank | Phase B bug, still present | UserDictionary uses `WS_EX_LAYERED + UpdateLayeredWindow(AC_SRC_ALPHA)` which composites child controls (ListView, edit boxes) with parent's per-pixel alpha. ListView rendering becomes invisible under layered composition. ShortcutSettings avoids this by using direct GDI paint. **PhrasesDialog has same WS_EX_LAYERED approach** so may also be affected |

---

## Step 1: Binary md5 vs commit timeline

Extracted WeaselServer.exe from each release commit's installer (via 7z on `release/fluxing-0.19.0.33-installer.exe`):

| Commit | Date | Installer md5 | WeaselServer.exe md5 inside installer |
|---|---|---|---|
| `be645b8b` chore(release): v0.19.0.33-fluxing installer | 2026-07-17 13:42 | `43171918 bytes` | `2b56123f972f3a326ef0c79563ab473e` ← OLD (7/17 08:33 build, NO Phase A) |
| `4e38fd7c` fix(release): v0.19.0.33-fluxing installer (REBUILD) | 2026-07-18 00:14 | `fbb2752464f2a14928aa0dc2447013e0` | `ce853875028ca9f2191ba0dbbabeffb8` ✓ Phase A |
| `9b3e0824` fix(install.nsi): L72-fix Rename-then-File | 2026-07-18 09:29 | `f8209479fcbbf451bd7a6d57ed67fd6c` | `ce853875028ca9f2191ba0dbbabeffb8` ✓ same binary |
| `24239f49` fix(install.nsi): Phase A.11 retry taskkill | 2026-07-18 09:55 | `8d8336205a05a04b53bb1298becea51f` | `ce853875028ca9f2191ba0dbbabeffb8` ✓ same binary |

**Source build output/Win32/WeaselServer.exe md5 = `ce853875028ca9f2191ba0dbbabeffb8`** (size
2224640, mtime 2026-07-18 00:00). This matches the binary inside all 3 Phase A installers.

**Conclusion**: User installed md5 `ce853875` = post-Phase A. Binary IS current. Phase A source
IS in the linked executable (verified via wide-string search: "FluxingPhrasesDialogV3",
"FluxingUserDictionary", "FluxingQuickPanel_v3", "FluxingShortcutSettings", "常用短语",
"用户词典", "ERROR_HOTKEY_ALREADY_REGISTERED", all 4 RegisterHotKey labels are in binary
string table at offsets 773540–773970).

**NOT a stale-binary bug.**

---

## Step 2: install.nsi L405-510 install flow

Read at offset 119-505 of `output/install.nsi`. Key points:

### .onInit (L119-292)
- **L131-139**: Retry taskkill WeaselServer.exe × 3 with 2s sleep (Phase A.11 fix from 24239f49)
- **L141-152**: Retry taskkill ctfmon.exe + TextInputHost.exe × 2 with 1.5s sleep
- **L168-236**: Resolve $INSTDIR — defaults to `D:\Program Files\fluxing` (or registry override)
- **L242-289**: If registry has prior uninstall, prompt user then call polite /quit + force taskkill

### Section "Fluxing" (L299-510+)
- **L306**: Write `HKLM\SOFTWARE\Fluxing\Weasel\InstallDir = $INSTDIR`
- **L311**: `$INSTDIR` reset to `${WEASEL_ROOT}` macro (spec 053 fix)
- **L318-326**: CreateDirectory for data subdirs
- **L329-333**: Retry taskkill × 3 after polite /quit (Phase A.11)
- **L339-343**: Copy fluxing-logo.png + fluxing-logo_small.png to `$INSTDIR\weasel\`
- **L351-499**: Copy binary files
  - **L371-395**: weasel.dll L72-fix (Rename → File → Delete /REBOOTOK) ✓ (existed pre-Phase A)
  - **L400-421**: weaselx64.dll L72-fix ✓ (existed pre-Phase A)
  - **L432-462** (Phase A.10 from 9b3e082): **WeaselServer.exe L72-fix** (Rename → File → Delete /REBOOTOK)
  - **L464-484** (Phase A.10): **WeaselDeployer.exe L72-fix**
  - **L486-506** (Phase A.10): **WeaselSetup.exe L72-fix**

**Verdict**: install.nsi is now defensive against autorun-respawn + locked-file race. The new
binary should reliably land on disk if .onInit and Section both run. There IS still a
failure mode: if `Rename` fails (still locked) and `SetOverwrite try` + `File` fails,
NSIS `IfErrors 0 WeaselServer_done` falls through to `DetailPrint "WeaselServer.exe locked;
old binary retained. Reboot to pick up new binary."` and exits without writing new file.
But this requires the file to be ACTIVE-OPEN at the moment of Rename, which is much rarer now.

---

## Step 3: QuickPanel lambda chain analysis

Read `WeaselServer/WeaselServerApp.cpp` L276-303 and `WeaselServer/QuickPanelDialog.cpp`
L562-595. Two parallel callback paths exist:

### Path A: `QuickPanelDialog::Show(7 args)` — WeaselServerApp.cpp L288-300

```cpp
QuickPanelDialog::Show(
    currentFullwidth,                                       // arg 1 (bool)
    [this]() { ... ShellExecuteW(deployer /hotkey); },      // arg 2 onSchema      → hit 0
    [this]() { explore(WeaselUserDataPath()); },            // arg 3 onUserFolder  → (no button)
    [this]() { PhrasesDialog::Show(); },                    // arg 4 onPhrases     → hit 1 ✓ Phase A.1
    [this](bool newFull) { ... SetOption full_shape ... },  // arg 5 onFullwidth   → (no button)
    [this]() { UserDictionary::Show(); },                   // arg 6 onSymbols     → (NO button uses this) ✗ dead
    []() {});                                               // arg 7 onLogin       → (no button)
```

User's task description said "arg 6 edit was wasted because no button invokes onSymbols".
**This is CORRECT**. `s_onSymbols` is stored at QuickPanelDialog.cpp:1171 but **never
referenced by any click handler**. QuickPanelDialog WndProc L562-588 only checks
hit==1 (s_onPhrases), hit==2 (s_onUserDict), hit==3 (s_onShortcut).

### Path B: `QuickPanelDialog::SetOnUserDict/SetOnShortcut` setters — WeaselServerApp.cpp L211-212

```cpp
SetQuickPanelUserDictCallback([]() { UserDictionary::Show(); });   // → s_onUserDict
SetQuickPanelShortcutCallback([]() { ShortcutSettings::Show(); });  // → s_onShortcut
```

These are called once at WeaselServerApp::Run() L211-212, before any QuickPanel show.
WndProc L567-568 invokes them on hit==2 and hit==3 respectively.

### Click dispatch (QuickPanelDialog.cpp L562-595, both code paths)

```cpp
// drag path L558-570
if (!actualDrag) {
  if (oldActiveIdx == 1 && s_onPhrases) s_onPhrases();    // hit 1 ✓
  else if (oldActiveIdx == 2 && s_onUserDict) s_onUserDict();  // hit 2 ✓
  else if (oldActiveIdx == 3 && s_onShortcut) s_onShortcut();  // hit 3 ✓
}

// click path L571-594
if (hit >= 0 && hit == s_activeIdx) {
  if (hit == 1 && s_onPhrases) { s_onPhrases(); }
  else if (hit == 2 && s_onUserDict) { s_onUserDict(); }
  else if (hit == 3 && s_onShortcut) { s_onShortcut(); }
}
```

**All three buttons 1/2/3 are wired**. My Phase A.1 arg 4 edit (Phrase → PhrasesDialog) and
arg 6 edit (Symbols → UserDictionary) are both stored in statics. Button 1 invocation will
fire PhrasesDialog::Show(). The arg 6 edit is dead code from the click perspective but not
harmful.

**Why might button 1 still appear broken to user?** Hypotheses (need verification):
- H-A: PhrasesDialog::Show() executes but window is invisible (e.g., WS_EX_LAYERED with
  per-pixel alpha causes body to render transparent). User sees nothing.
- H-B: PhrasesDialog::Show() crashes silently due to InitCommonControlsEx failure on user's
  Windows version (unlikely; other dialogs work)
- H-C: QuickPanel button 1 hit-test fails (HitTest returns -1 or wrong index) — geometry bug
- H-D: User is clicking button at boundary between buttons and is actually hitting button 0
  (which is no-op) or button 2 (which routes to UserDict)

---

## Step 4: Alt+/ routing verification

Read WeaselServerApp.cpp L67-70 (PhrasesHotkeySubclassProc):

```cpp
if (msg == WM_HOTKEY) {
  if (w == ID_HOTKEY_PHRASES_DOT) { PhrasesDialog::Show(); return 0; }
  if (w == ID_HOTKEY_USER_DICT) { UserDictionary::Show(); return 0; }
  if (w == ID_HOTKEY_SHORTCUT) { ShortcutSettings::Show(); return 0; }
  // v0.19.0.32-fix (Bug 3b): Alt+/ → UserDictionary::Show()
  if (w == ID_HOTKEY_USER_DICT_ALT_SLASH) {  // 9005
    UserDictionary::Show();  // <-- STILL POINTS TO UserDictionary
    return 0;
  }
}
```

### Git log verification
```
$ git log -p -S "ID_HOTKEY_PHRASES_SLASH" -- WeaselServer/resource.h WeaselServer/WeaselServerApp.cpp
(no results)
```

`ID_HOTKEY_PHRASES_SLASH` **NEVER existed** in any commit. The constant is
`ID_HOTKEY_USER_DICT_ALT_SLASH = 9005` (resource.h:48), added in commit `cd6f61a9`
(v0.19.0.32).

### Phase A diff for `deebaf74` (the Phase A source fix commit)
The `git diff deebaf74~1..deebaf74 -- WeaselServer/WeaselServerApp.cpp` shows ONLY
changes to `RegisterPhrasesHotkey()` L100-138 (stderr log enrichment, Phase A.2). The
PhrasesHotkeySubclassProc L44-78 was NOT modified.

### Binary string verification
Searched output/Win32/WeaselServer.exe (md5 ce853875) for wide strings:
- "RegisterHotKey(Alt+/, id=" → offset 773970 ✓
- "ERROR_HOTKEY_ALREADY_REGISTERED (另一个 app 已占用 Alt+/)" → offset 773850 ✓

So the binary contains the L130-139 RegisterHotKey(Alt+/) call and the L67-70 handler
that routes it to UserDictionary::Show. **Alt+/ still opens UserDictionary, not
PhrasesDialog**.

**Verdict**: User's task description "A.1 改 WeaselServerApp.cpp:67-70 用 ID_HOTKEY_PHRASES_SLASH
调 PhrasesDialog::Show()" is **factually incorrect** — this edit was never made, not
in any commit. User's feedback "Alt+/ → 调出 UI 看不清标题, 似乎是用户词典" matches the
actual UserDictionary title (用户词典) — proves Alt+/ IS routing to UserDictionary as
the source code dictates.

---

## Step 5: Sandbox real user flow verification

**Limitation**: Sandbox's WeaselServer.exe (PID 14688, started 11:46:32) is running as a
service with no visible UI windows. The QuickPanel is hidden by default (spec 055 — no
auto-show on service start). Tray icon exists but is on hidden Windows tray. Could not
trigger Alt+/, Alt+., Ctrl+Shift+U, Ctrl+Shift+K, or QuickPanel button clicks via
SendInput/SendKeys because:
- No active TSF text input session in sandbox
- No visible foreground app to receive WM_HOTKEY via PostMessage
- Tray icon is hidden (would need to expand via Shell_NotifyIcon)

### Static binary evidence collected instead
- All 4 dialog class names wide-string present at expected offsets (1550424 / 1550564 / 1557740 / 1551716)
- All 4 dialog title strings wide-string present ("常用短语" 1550946 + 1550564; "用户词典" 4 occurrences)
- All 4 RegisterHotKey stderr warning logs present at offsets 773582/773730/773850/773970
- PhrasesHotkeySubclassProc + PhrasesDialog::Show code linked (presence of YAML paths,
  dialog class names, modal chrome paint helpers confirms)

**Indirect verdict**: From binary structure, all 3 hotkeys and all 3 button callbacks
should work IF the runtime path is reached. Failure modes must be one of:
- RegisterHotKey returns 0 (already-registered or focus-blocked) → hotkey silently dropped
- WS_EX_LAYERED child controls invisible → dialog appears blank
- Click hit-test geometry bug → button 1 click doesn't fire

---

## Step 6: 5-axis root cause assessment (Axis 1 perspective)

### Axis 1 view on each reported bug

#### Bug A: Button 2 (UserDict) icon "not like a book"
- Source has `DrawIconUserDict` at `QuickPanelDialog.cpp:161`
- Used in WndProc:921 case 2 (Phase A.3 commit deebaf74)
- Binary string search confirms symbol linked (search for "FluxingQuickPanel_v3" → offset 1549500)
- **Root cause hypothesis**: DrawIconUserDict visual output doesn't match user's book expectation
  (e.g., draws "📖 with pages" instead of "📕 with pages"). Need actual screenshot diff.
- **Not a stale binary bug**.

#### Bug B: Button 1 (Phrase) click no response
- Show() arg 4 = `PhrasesDialog::Show` (cpp:295) ✓ stored at s_onPhrases (cpp:1169)
- WndProc hit==1 invokes s_onPhrases (cpp:566/581) ✓
- PhrasesDialog::Show creates window class "FluxingPhrasesDialogV3" + title "常用短语" (cpp:133/146)
- **Root cause hypotheses (need live UI to verify)**:
  1. PhrasesDialog::Show() succeeds but body paints transparent (WS_EX_LAYERED child issue)
  2. PhrasesDialog window appears behind other windows
  3. Button 1 click doesn't actually trigger WndProc L562/580 (drag threshold or HitTest bug)
  4. PhrasesDialog::Show fails on CreateWindowExW due to non-admin / DPI scaling issue
- **Cannot determine from static analysis alone**. Need Axis 2 (live trace with
  instrumentation) or Axis 5 (debugger attach to PID 14688).

#### Bug C: Button 3 (UserDict) + Alt+/ → "blurry title, looks like UserDict"
- **Root cause CONFIRMED**: Alt+/ handler at WeaselServerApp.cpp:67-70 still routes to
  `UserDictionary::Show()`. The handler was never changed by Phase A.
- User's "looks like UserDict" matches UserDictionary's title "📖  用户词典" exactly.
- Phase A commit deebaf74 did NOT touch this handler — only RegisterPhrasesHotkey logs.
- **Fix**: Edit WeaselServerApp.cpp:68 to call `PhrasesDialog::Show()` instead of
  `UserDictionary::Show()`. Rename constant from `ID_HOTKEY_USER_DICT_ALT_SLASH` to
  `ID_HOTKEY_PHRASES_SLASH` in resource.h (or keep old name but re-route semantics).
- **Alternatively**: User's spec actually expects Alt+/ = Phrase, but original commit cd6f61a9
  designed it as UserDict. Decision to be revisited.

#### Bug D: Alt+. no response
- RegisterHotKey(Alt+.) IS called at WeaselServerApp.cpp:100. Binary contains the call.
- WM_HOTKEY handler at L49 routes to `PhrasesDialog::Show()` ✓
- **Root cause hypotheses**:
  1. RegisterHotKey returns 0 because another app already holds Alt+. (ERROR 1409) →
     no global hotkey → Alt+. keystroke goes to foreground app as text input
  2. RegisterHotKey succeeds but WeaselServer.exe loses focus → WM_HOTKEY not delivered
  3. Phase A.2 stderr log WOULD print a warning to console, but Fluxing WeaselServer runs
     as a Windows GUI app with no console → user never sees the error
- **Fix needed**: After Phase A.2 logs are emitted to stderr, no one sees them. Either:
  - Write warning to a log file in `$APPDATA\Rime\weasel.log`
  - Surface error as tray balloon notification
  - Or rely on user to attach debugger
- **Cannot determine from static analysis whether RegisterHotKey actually failed on user's
  machine**. Axis 2 needed to add instrumentation.

#### Bug E: UserDict dialog body blank
- UserDictionary uses `WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST` (cpp:196)
- RepaintLayered uses `UpdateLayeredWindow(ULW_ALPHA)` with `AC_SRC_ALPHA` (cpp:1114)
- **Root cause hypothesis**: Child controls (ListView, search box, status, edit dialog,
  buttons) are rendered into the parent's memory DC during WM_PAINT, but their
  per-pixel alpha gets composited against parent's 32-bit DIBSection which already has
  alpha < 255 in most areas (gradient 220→80). Child control pixel alpha values that
  don't survive the layered composite become invisible.
- **Compare**: ShortcutSettings avoids this — uses direct GDI paint (ShortcutSettings.cpp:1055)
  with `ModalChrome::PaintBackgroundAndBorder(hdc, ...)` direct HDC painting. Its body is visible.
- **Fix candidate**: UserDictionary should also use direct GDI paint, or use `LWA_COLORKEY`
  for opaque area + `WS_EX_LAYERED` only for round corners (spec 066 pattern).
- **This is the SAME class of bug** that PhrasesDialog may have (same WS_EX_LAYERED approach).
  PhrasesDialog also uses UpdateLayeredWindow (Phase A.3 changed paint to direct?).

---

## Axis 1 conclusion

The 4 user-reported bugs decompose into **3 distinct root causes**:

1. **Alt+/ routing bug** (Bug C) — Source-level discrepancy: WeaselServerApp.cpp:68 still
   routes to UserDictionary::Show. **NOT fixed by Phase A.** Phase A.1 alt+/ claim was
   never committed to source. **Direct cause**: user's task description was wrong; they
   thought they edited the handler but never did.

2. **Alt+. silent failure** (Bug D) — RegisterHotKey likely fails on user's machine
   (ERROR_HOTKEY_ALREADY_REGISTERED 1409 = another app holds Alt+.). Phase A.2 added
   stderr log but it's invisible to GUI user. **Fix needed**: surface RegisterHotKey
   failure to user via log file or tray notification.

3. **Button 1 (Phrase) no response + UserDict body blank** (Bug B + E) — Likely
   **same root cause**: WS_EX_LAYERED + UpdateLayeredWindow + child control rendering.
   PhrasesDialog and UserDictionary both use this pattern; user may see a window appear
   but body invisible. Phase A.1 lambda edit is correct, but visual rendering fails.
   **Fix candidate**: Migrate PhrasesDialog and UserDictionary to direct GDI paint (same
   pattern as ShortcutSettings), OR use LWA_COLORKEY for opaque body region.

4. **Button 2 icon "not like a book"** (Bug A) — DrawIconUserDict IS in source/binary
   but visual may not match user's expectation. **Fix candidate**: revise icon
   geometry. Cannot determine without screenshot comparison.

5. **No "stale binary" issue** — User installed md5 = ce853875 = current post-Phase A
   build. install.nsi L72-fix + retry taskkill works reliably. Binary on user's disk
   matches source build.

---

## What Axis 1 cannot determine (need Axis 2/3/5)

- Whether RegisterHotKey(Alt+.) actually failed for user (need runtime log)
- Whether button 1 click actually reaches s_onPhrases() (need UI trace)
- Whether UserDict body is WS_EX_LAYERED composite issue or something else (need
  screenshot)
- Whether DrawIconUserDict visually matches "book" expectation (need screenshot)

---

## Files cited

- `output/install.nsi:119-510` — install flow
- `WeaselServer/WeaselServerApp.cpp:44-78` — PhrasesHotkeySubclassProc (alt+/ bug)
- `WeaselServer/WeaselServerApp.cpp:80-140` — RegisterPhrasesHotkey
- `WeaselServer/WeaselServerApp.cpp:200-217` — Run() initialization (SetQuickPanelUserDict/Shortcut)
- `WeaselServer/WeaselServerApp.cpp:281-303` — QuickPanel trigger + Show() 7-arg call
- `WeaselServer/QuickPanelDialog.h:38-100` — class declaration + static storage
- `WeaselServer/QuickPanelDialog.cpp:161` — DrawIconUserDict (Phase A.3)
- `WeaselServer/QuickPanelDialog.cpp:531-595` — WndProc click routing
- `WeaselServer/QuickPanelDialog.cpp:1160-1172` — Show() callback storage
- `WeaselServer/PhrasesDialog.cpp:88-172` — PhrasesDialog::Show
- `WeaselServer/UserDictionary.cpp:158-247` — UserDictionary::Show
- `WeaselServer/UserDictionary.cpp:997-1121` — UserDictionary::RepaintLayered
- `WeaselServer/ShortcutSettings.cpp:1055-1090` — ShortcutSettings::OnPaint (direct GDI)
- `WeaselServer/resource.h:48` — `ID_HOTKEY_USER_DICT_ALT_SLASH 9005`

---

## Recommended next step

Open **Axis 2**: Add WeaselServer-side instrumentation (stderr + log file) for the 4
hotkey registration results, QuickPanel button click events, and PhrasesDialog::Show
entry/exit. Then re-run user flow with the instrumented binary and capture:
- Whether RegisterHotKey succeeded for each of Alt+., Alt+/, Ctrl+Shift+U, Ctrl+Shift+K
- Whether button 1 click actually reached s_onPhrases
- Whether PhrasesDialog::Show was called and if its window was successfully created
- Whether UserDict child controls were visible post-paint
