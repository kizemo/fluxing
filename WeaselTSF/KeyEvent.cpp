#include "stdafx.h"
#include <KeyEvent.h>
#include <KeyNameMapper.h>

bool ConvertKeyEvent(UINT vkey,
                     KeyInfo kinfo,
                     const LPBYTE keyState,
                     weasel::KeyEvent& result) {
  const BYTE KEY_DOWN = 0x80;
  const BYTE TOGGLED = 0x01;

  // set mask
  result.mask = ibus::NULL_MASK;

  if ((keyState[VK_SHIFT] & KEY_DOWN) != 0)
    result.mask |= ibus::SHIFT_MASK;

  if ((keyState[VK_CAPITAL] & TOGGLED) != 0)
    result.mask |= ibus::LOCK_MASK;

  if ((keyState[VK_CONTROL] & KEY_DOWN) != 0)
    result.mask |= ibus::CONTROL_MASK;

  if ((keyState[VK_MENU] & KEY_DOWN) != 0)
    result.mask |= ibus::ALT_MASK;

  if (kinfo.isKeyUp)
    result.mask |= ibus::RELEASE_MASK;

  if (vkey == VK_CAPITAL && !kinfo.isKeyUp) {
    // NOTE: rime assumes XK_Caps_Lock to be sent before modifier changes,
    // while VK_CAPITAL has the modifier changed already.
    // so it is necessary to revert LOCK_MASK.
    result.mask ^= ibus::LOCK_MASK;
  }

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
