#include "stdafx.h"
#include <KeyEvent.h>
#include <KeyNameMapper.h>

bool ConvertKeyEvent(UINT vkey,
                     KeyInfo kinfo,
                     const LPBYTE keyState,
                     weasel::KeyEvent& result) {
  const BYTE KEY_DOWN = 0x80;

  // set mask
  result.mask = ibus::NULL_MASK;

  if ((keyState[VK_SHIFT] & KEY_DOWN) != 0)
    result.mask |= ibus::SHIFT_MASK;

  // The CapsLock latch is deliberately NOT reported as a modifier bit.
  //
  // The latch is a persistent hardware state, not a modifier of the key
  // being pressed, and the case it produces is already carried by the
  // character itself. Reporting it had two consequences, and both of them
  // were fatal (ADR 0014):
  //
  //   * librime compares key events exactly (key code and modifier), so
  //     every binding lost its match while the latch was on;
  //   * librime's ASCII composer answers kRejected for a key that carries
  //     the latch bit, and kRejected breaks the whole processor chain
  //     (librime/src/rime/engine.cc), so the candidate selector, the pager
  //     and the hotkey binder never saw the key at all.
  //
  // The latch key itself is still reported as its own key, with its own
  // behaviour (switch the input mode, or hand the key back to the system
  // while already in English) unchanged.
  //
  // NOTE: the old code also toggled the bit back on the latch key with
  // `^=`. Both sites have to go together -- `^=` is an exclusive or, so
  // leaving it behind would put a bit into the mask that was never set
  // (0 ^ LOCK == LOCK) and flip "switch to English" into "switch to
  // Chinese".
  if ((keyState[VK_CONTROL] & KEY_DOWN) != 0)
    result.mask |= ibus::CONTROL_MASK;

  if ((keyState[VK_MENU] & KEY_DOWN) != 0)
    result.mask |= ibus::ALT_MASK;

  if (kinfo.isKeyUp)
    result.mask |= ibus::RELEASE_MASK;

  // set keycode
  // The character a chord produces is resolved by the shared unit, with the
  // very rules this file used to apply (ADR 0011 D1/D2): the special keys
  // come from the shared table, every other key is resolved through
  // ToUnicodeEx() with Ctrl and Alt cleared.
  const int code = fluxing::ResolveCharacter(vkey, kinfo, keyState, NULL);
  if (code) {
    result.keycode = code;
    return true;
  }

  result.keycode = 0;
  return false;
}

/*
struct ModifierNameTableEntry
{
        LPCTSTR name;
        ibus::Modifier modifier;
};

ModifierNameTableEntry MODIFIER_NAME_TABLE[] = {
        { L"Shift", ibus::SHIFT_MASK },
        { L"CapsLock", ibus::LOCK_MASK },
        { L"Ctrl", ibus::CONTROL_MASK },
        { L"Alt", ibus::MOD1_MASK },
        { L"SUPER", ibus::SUPER_MASK },
        { L"Hyper", ibus::HYPER_MASK },
        { L"Meta", ibus::META_MASK },
        { L"Release", ibus::RELEASE_MASK },
        { NULL, ibus::NULL_MASK }
 };
*/
