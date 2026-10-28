# 0015. The Shift left/right convention gets one source of truth, and the front-end popup menu gets the always-show entry

- **状态**: Proposed
- **日期**: 2026-10-28
- **关联**: spec 086、任务总账本轮条目
- **相关**: ADR 0013（快捷设置栏常驻模式——本条补的是它的**入口**）、ADR 0014（CapsLock 锁存状态——与本条正交）

## Context

Two defects came back from the user's on-machine acceptance of 1.1.0.

**Defect A — the bare Shift picked the mirrored candidate.** The user reported that
a bare left Shift took the *third* candidate and a bare right Shift took the
*second* — exactly inverted.

The wire contract for this channel was already fixed and already pinned by a
test: `wParam bit0 = is_left (0=L, 1=R)`. Tracing the whole chain showed that
every link was correct except one:

- the front end decided left vs. right correctly (scan code → key name);
- the state machine's own mapping (left → candidate #2, right → candidate #3)
  was correct;
- the **client** encoded the bit exactly as the contract says (`is_left ? 0 : 1`);
- the **server** read that same bit **the other way round** (`(wParam & 1u) != 0`).

So the two ends of one wire format disagreed, and the disagreement was
invisible from either side alone. Nothing in the codebase compared the two
sides; each side just looked locally correct.

**Defect B — the always-show entry was missing from the popup menu.** The
previous change (ADR 0013) shipped a "keep the quick panel on screen" switch
into the **tray icon** menu. The user went looking for it in the **input
method's own popup menu** (the one that opens when right-clicking the language
bar) and found only the "quick settings bar" entry.

The reason is structural: **there are two menu resources**, not one. The tray
icon menu lives in the server's resource script; the language bar's popup menu
lives in the front end's resource script, in three language variants. The
previous change edited the server's copy. The front end's copies never had the
entry. The command id was also missing from the shared header the front end
includes, so the entry could not even be expressed there.

The routing was never missing: the front end already forwards unknown menu
selections to the server's tray-command channel, and the server already had a
handler for this command. Only the **menu item** was absent.

The acceptance item from the previous change therefore could not be found by
the user: the path they used did not contain the feature.

## Decision

- **D1 — The convention stays as it is documented.** Bit 0 of the Shift
  message's `wParam` means *is-left*, encoded as **0 for left and 1 for right**.
  We do not redefine the wire format; we make the code obey it.

- **D2 — One source of truth for the convention.** The encoding and the
  decoding move into a single pair of pure functions in the shared header
  (`EncodeShiftIsLeft` / `DecodeShiftIsLeft`). The client calls the encoder,
  the server calls the decoder, and **neither side spells the bit out as a
  literal any more**. A one-sided change of the convention can no longer
  happen silently, because there is only one place where it is written down.

- **D3 — The regression assertion pins the bit values, not just the round
  trip.** The new test asserts that left encodes to 0, right encodes to 1, and
  that decoding 0 yields "left" and decoding 1 yields "right". A test that
  only asserted "encode then decode returns the original" would **not** catch
  the defect that actually happened: with both ends inverted the round trip
  still closes. Pinning the numbers is what makes the assertion falsifiable.

- **D4 — The shared header owns the command id.** The id for the always-show
  command moves into the shared resource header, with a comment stating that it
  must stay equal to the server's own copy. Both resource scripts — the
  server's tray menu and the front end's popup menus — then reference the
  same number, and the server's existing handler is reused unchanged.

- **D5 — All three front-end popup menus get the entry.** The English,
  Traditional Chinese and Simplified Chinese menu variants are edited
  together; the Traditional variant uses Traditional characters. The server's
  tray menu is left as it is — it already has the entry.

- **D6 — The protocol is not extended.** The front end's popup menu already
  forwards unrecognised selections to the server's tray-command channel, and
  the server already handles this command, so the feature needs **no new
  message type**. `WeaselIPCData.h` is untouched, and the server / front end /
  deployer do not need to be synchronised.

- **D7 — No check mark in the popup menu.** The switch's current state is
  shown only in the server's tray menu. Marking it in the front end's popup
  menu would require a new round-trip message to query the state, which is a
  protocol extension. We record it as a known limitation instead of quietly
  growing the protocol.

## Consequences

- **What gets fixed.** A bare left Shift takes the **second** candidate and a
  bare right Shift takes the **third**, matching the user's expectation and the
  on-screen hint. The user can finally reach the always-show switch from the
  input method's own popup menu, and it behaves exactly as it does from the
  tray icon.

- **What the user may notice.** The popup menu has one more entry. Nothing
  else about the menu, the tray icon, or the Shift behaviour changes.

- **What stays broken on purpose.** The popup menu shows no check mark for
  the always-show switch; its state is visible only in the tray icon menu
  (D7).

- **Why the defect was invisible until now.** The two ends of the wire format
  were each locally plausible, and the existing test pinned the *contract*
  rather than the *implementation*: it asserted on the raw bit the client
  produces, and the server was never exercised against it. D2 and D3 close
  that gap at the level where the disagreement actually lived.

- **Why the menu entry was invisible until now.** The feature was shipped into
  one of two menu resources. The acceptance checklist in the previous change
  described the *tray* path, which is why it looked covered. D4 and D5 make
  the entry exist on both paths, and the resource-level check in the
  verification step reads the built binaries rather than the sources.

- **Risk of the change itself.** Small and bounded: two replaced expressions
  on each side of one channel, one new pair of pure functions, one new
  constant, three menu rows. The Shift state machine, the candidate selection
  logic and the key-name mapping are all untouched.

## Evidence

The build products were inspected at the resource level, because a source-level
reading cannot tell whether a menu entry actually reached the binary.

Before this change — searching the three menu resources for the entry:

| binary | MENU 105 / 108 / 109 contain the always-show entry |
|---|---|
| `output/Win32/WeaselServer.exe` (server tray menu) | **yes** |
| `output/weasel.dll` (front end, 32-bit) | **no** |
| `output/weaselx64.dll` (front end, 64-bit) | **no** |

After this change the two front-end binaries report the entry in all three of
their menu variants.

The Shift direction is pinned by the new regression assertion
(`Test_EncodeDecode_RoundTrip`), which asserts the encoded bit values
directly rather than only the round trip, so that an inversion on either side
— or on both sides at once — fails the test.

## Open questions

- Should the front end's popup menu show the current state of the always-show
  switch? That needs a state query round trip (D7); it is a separate piece of
  work with its own protocol change.
- The 64-bit front end on the acceptance machine was still running the build
  from the day before, because the installer could not replace a file the
  system held open. The installer scheduled the replacement for the next
  start-up, so the machine needs a restart before that part of the acceptance
  can be completed. This is a deployment matter, not a defect in this change.
