#ifndef FLUXING_KEY_NAME_MAPPER_H_
#define FLUXING_KEY_NAME_MAPPER_H_

// The one place that knows
//   * which character a keyboard chord produces (the front end's rules), and
//   * how librime names that character in an `accept:` pattern.
//
// ADR 0011 D1: the settings dialog, the shortcut panel and the acceptance
// gate all consume this unit, so they cannot drift apart. The unit is header
// only and free of GUI and of librime dependencies: it needs the Windows
// keyboard API and the key table generated from librime's own table.

#include <windows.h>

#include <KeyEvent.h>

#include <cstddef>
#include <cstring>
#include <string>

#include "keysyms_generated.inc"

namespace fluxing {

// ---------------------------------------------------------------------------
// librime's own key table
// ---------------------------------------------------------------------------

// The key code librime uses for a key name, or 0 when librime does not know
// the name. Mirrors RimeGetKeycodeByName().
inline int KeycodeByName(const std::string& name) {
  size_t count = 0;
  const keysyms::NamedKey* table = keysyms::KeyNames(&count);
  for (size_t i = 0; i < count; ++i) {
    if (name == table[i].name)
      return table[i].keycode;
  }
  return 0;
}

// The modifier mask librime uses for a modifier name, or 0 when the name is
// unknown. Mirrors RimeGetModifierByName().
inline int ModifierByName(const std::string& name) {
  size_t count = 0;
  const keysyms::NamedModifier* table = keysyms::ModifierNames(&count);
  for (size_t i = 0; i < count; ++i) {
    if (name == table[i].name)
      return table[i].mask;
  }
  return 0;
}

// The name librime gives to a key code, or an empty string when librime has
// no name for it. Mirrors RimeGetKeyName().
inline std::string KeyName(int keycode) {
  size_t count = 0;
  const keysyms::NamedKey* table = keysyms::KeyNames(&count);
  for (size_t i = 0; i < count; ++i) {
    if (table[i].keycode == keycode)
      return std::string(table[i].name);
  }
  return std::string();
}

// The name librime gives to a modifier mask, or an empty string.
inline std::string ModifierName(int mask) {
  size_t count = 0;
  const keysyms::NamedModifier* table = keysyms::ModifierNames(&count);
  for (size_t i = 0; i < count; ++i) {
    if (table[i].mask == mask)
      return std::string(table[i].name);
  }
  return std::string();
}

// ---------------------------------------------------------------------------
// wide characters
// ---------------------------------------------------------------------------

// UTF-8 to UTF-16. The accept patterns themselves are ASCII, but the one
// character form carries a character code and the dialog works in wide
// characters: widening the bytes one by one would corrupt anything above
// 0x7F.
inline bool NarrowToWide(const std::string& narrow, std::wstring* out) {
  if (!out)
    return false;
  out->clear();
  if (narrow.empty())
    return true;
  const int need =
      MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, narrow.data(),
                          static_cast<int>(narrow.size()), NULL, 0);
  if (need <= 0)
    return false;
  out->resize(static_cast<size_t>(need));
  const int got =
      MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, narrow.data(),
                          static_cast<int>(narrow.size()), &(*out)[0], need);
  if (got != need) {
    out->clear();
    return false;
  }
  return true;
}

// UTF-16 to UTF-8.
inline bool WideToNarrow(const std::wstring& wide, std::string* out) {
  if (!out)
    return false;
  out->clear();
  if (wide.empty())
    return true;
  const int need =
      WideCharToMultiByte(CP_UTF8, 0, wide.data(),
                          static_cast<int>(wide.size()), NULL, 0, NULL, NULL);
  if (need <= 0)
    return false;
  out->resize(static_cast<size_t>(need));
  const int got = WideCharToMultiByte(CP_UTF8, 0, wide.data(),
                                      static_cast<int>(wide.size()), &(*out)[0],
                                      need, NULL, NULL);
  if (got != need) {
    out->clear();
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// accept patterns
// ---------------------------------------------------------------------------

// Why an accept pattern could not be built. The codes stay ASCII on purpose:
// the settings dialog turns them into its own Simplified Chinese message
// (constitution P2), the acceptance gate prints them verbatim.
enum AcceptError {
  kAcceptOk = 0,
  kAcceptNoName,       // librime has no name for the character
  kAcceptModifierKey,  // the chord is a modifier key itself
  kAcceptUnprintable,  // the character cannot be written as a pattern
  kAcceptWinKey,       // the Windows key is never reported by the front end
  kAcceptNoCharacter   // the chord produces no character at all
};

inline const char* AcceptErrorText(AcceptError error) {
  switch (error) {
    case kAcceptNoName:
      return "no-name";
    case kAcceptModifierKey:
      return "modifier-key";
    case kAcceptUnprintable:
      return "unprintable";
    case kAcceptWinKey:
      return "win-key";
    case kAcceptNoCharacter:
      return "no-character";
    default:
      return "ok";
  }
}

// The Simplified Chinese message for every refusal. The error codes stay
// ASCII on purpose (the acceptance gate prints them verbatim); the wording is
// what the user reads (constitution P2). Both capture paths share this
// wording, so the two dialogs cannot drift apart (ADR 0011 D1).
inline const wchar_t* AcceptErrorMessage(AcceptError error) {
  switch (error) {
    case kAcceptNoName:
      return L"\x8be5\x7ec4\x5408\x952e\x65e0\x6cd5\x8868\x8fbe\xff0c\x8bf7"
             L"\x6309\x4f4f\x0020\x0053\x0068\x0069\x0066\x0074\x0020\x518d"
             L"\x8bd5";
    case kAcceptModifierKey:
      return L"\x4fee\x9970\x952e\x4e0d\x80fd\x5355\x72ec\x4f5c\x4e3a\x5feb"
             L"\x6377\x952e\xff0c\x8bf7\x518d\x6309\x4e00\x4e2a\x666e\x901a"
             L"\x952e";
    case kAcceptUnprintable:
      return L"\x8be5\x6309\x952e\x65e0\x6cd5\x4f5c\x4e3a\x5feb\x6377\x952e";
    case kAcceptWinKey:
      return L"\x4e0d\x652f\x6301\x0020\x0057\x0069\x006e\x0020\x952e\xff0c"
             L"\x8bf7\x6539\x7528\x0020\x0043\x0074\x0072\x006c\x0020\x002f"
             L"\x0020\x0053\x0068\x0069\x0066\x0074\x0020\x002f\x0020\x0041"
             L"\x006c\x0074\x0020\x7ec4\x5408";
    case kAcceptNoCharacter:
    default:
      return L"\x672a\x77e5\x6309\x952e\xff0c\x8bf7\x91cd\x8bd5";
  }
}

// A parsed accept pattern, the result of librime's own parse rules
// (librime/src/rime/key_event.cc:51-82).
struct AcceptKey {
  bool ok;
  int keycode;
  int modifier;
  std::string error;
  AcceptKey() : ok(false), keycode(0), modifier(0) {}
};

// Parse an accept pattern exactly the way librime does: a one character
// pattern is the character code itself, a longer pattern is split on '+',
// every part before the last must be a modifier name and the last part must
// be a key name librime knows.
inline AcceptKey ParseAccept(const std::string& repr) {
  AcceptKey out;
  if (repr.empty()) {
    out.error = "empty pattern";
    return out;
  }
  if (repr.size() == 1) {
    // librime: keycode_ = static_cast<int>(repr[0]);
    out.ok = true;
    out.keycode = static_cast<int>(static_cast<unsigned char>(repr[0]));
    out.modifier = 0;
    return out;
  }
  int mask = 0;
  size_t start = 0;
  for (;;) {
    const size_t found = repr.find('+', start);
    if (found == std::string::npos)
      break;
    const std::string token = repr.substr(start, found - start);
    const int modifier = ModifierByName(token);
    if (!modifier) {
      out.error = "unrecognized modifier '" + token + "'";
      return out;
    }
    mask |= modifier;
    start = found + 1;
  }
  const std::string token = repr.substr(start);
  const int keycode = KeycodeByName(token);
  if (!keycode) {
    out.error = "unrecognized key '" + token + "'";
    return out;
  }
  out.ok = true;
  out.keycode = keycode;
  out.modifier = mask;
  return out;
}

// The same parse for a pattern that is already in wide characters.
inline AcceptKey ParseAcceptWide(const std::wstring& repr) {
  AcceptKey out;
  std::string narrow;
  if (!WideToNarrow(repr, &narrow)) {
    out.error = "invalid pattern";
    return out;
  }
  return ParseAccept(narrow);
}

// Whether two accept patterns mean the same chord. librime compares the key
// code and the modifier mask, not the spelling of the pattern, so the
// comparison has to be semantic as well: "Shift+Control+F" and
// "Control+Shift+F" are the same binding (ADR 0011 D1).
inline bool AcceptEquals(const std::wstring& a, const std::wstring& b) {
  if (a == b)
    return true;
  const AcceptKey ka = ParseAcceptWide(a);
  const AcceptKey kb = ParseAcceptWide(b);
  if (!ka.ok || !kb.ok)
    return false;
  return ka.keycode == kb.keycode && ka.modifier == kb.modifier;
}

// Key codes that stand for a modifier key. librime knows their names, but a
// binding must never name one: the front end eats the shift keys and the
// Windows key is refused outright (ADR 0011 D6).
inline bool IsModifierKeyCode(int keycode) {
  switch (keycode) {
    case ibus::Shift_L:
    case ibus::Shift_R:
    case ibus::Control_L:
    case ibus::Control_R:
    case ibus::Alt_L:
    case ibus::Alt_R:
    case ibus::Meta_L:
    case ibus::Meta_R:
    case ibus::Super_L:
    case ibus::Super_R:
    case ibus::Caps_Lock:
      return true;
    default:
      return false;
  }
}

// The modifier names of a mask, in the order librime itself writes them
// (librime/src/rime/key_event.cc:19-33 walks the mask bit by bit).
inline bool AppendModifiers(int modifier, std::string* out) {
  for (int bit = 0; bit < 31; ++bit) {
    const int mask = 1 << bit;
    if (!(modifier & mask))
      continue;
    const std::string name = ModifierName(mask);
    if (name.empty())
      return false;
    out->append(name);
    out->append("+");
  }
  return true;
}

// Build the librime accept pattern for a key code and a librime modifier
// mask.
//
// librime compares the key code and the modifier mask exactly (ADR 0011 F1),
// so a pattern has to carry both. A character librime has no name for cannot
// be written down once a modifier is involved: the one character form of an
// accept pattern only works without modifiers. Such a chord is refused, never
// rewritten (ADR 0011 D3).
inline bool BuildAccept(int keycode,
                        int modifier,
                        std::string* out,
                        AcceptError* error) {
  if (error)
    *error = kAcceptOk;
  if (!out)
    return false;
  out->clear();

  if (IsModifierKeyCode(keycode)) {
    if (error)
      *error = kAcceptModifierKey;
    return false;
  }

  const std::string name = KeyName(keycode);
  if (name.empty()) {
    if (modifier) {
      if (error)
        *error = kAcceptNoName;
      return false;
    }
    if (keycode < 0x20 || keycode > 0x7e) {
      if (error)
        *error = kAcceptUnprintable;
      return false;
    }
    // The one character form: librime takes the character code itself.
    out->assign(1, static_cast<char>(keycode));
    return true;
  }

  std::string prefix;
  if (!AppendModifiers(modifier, &prefix)) {
    if (error)
      *error = kAcceptNoName;
    return false;
  }
  *out = prefix + name;
  return true;
}

// The same pattern in wide characters, for the settings dialog.
inline bool BuildAccept(int keycode,
                        int modifier,
                        std::wstring* out,
                        AcceptError* error) {
  if (!out)
    return false;
  std::string narrow;
  if (!BuildAccept(keycode, modifier, &narrow, error))
    return false;
  return NarrowToWide(narrow, out);
}

// ---------------------------------------------------------------------------
// chord -> character
// ---------------------------------------------------------------------------

// The key code a virtual key maps to, without asking the keyboard layout.
// This is the front end's own table, moved here so that the capture dialog
// and the front end can never disagree about it (ADR 0011 D1).
inline int TranslateKeycode(UINT vkey, KeyInfo kinfo) {
  switch (vkey) {
    case VK_BACK:
      return ibus::BackSpace;
    case VK_TAB:
      return ibus::Tab;
    case VK_CLEAR:
      return ibus::Clear;
    case VK_RETURN: {
      if (kinfo.isExtended == 1)
        return ibus::KP_Enter;
      else
        return ibus::Return;
    }
    case VK_SHIFT: {
      if (kinfo.scanCode == 0x36)
        return ibus::Shift_R;
      else
        return ibus::Shift_L;
    }
    case VK_CONTROL: {
      if (kinfo.isExtended == 1)
        return ibus::Control_R;
      else
        return ibus::Control_L;
    }
    case VK_MENU:
      return ibus::Alt_L;
    case VK_PAUSE:
      return ibus::Pause;
    case VK_CAPITAL:
      return ibus::Caps_Lock;

    case VK_KANA:
      return ibus::Hiragana_Katakana;
    // case VK_JUNJA:	return 0;
    // case VK_FINAL:	return 0;
    case VK_KANJI:
      return ibus::Kanji;

    case VK_ESCAPE:
      return ibus::Escape;

    case VK_CONVERT:
      return ibus::Henkan;
    case VK_NONCONVERT:
      return ibus::Muhenkan;
      // case VK_ACCEPT:	return 0;
      // case VK_MODECHANGE:	return 0;

    case VK_SPACE:
      return ibus::space;
    case VK_PRIOR:
      return ibus::Prior;
    case VK_NEXT:
      return ibus::Next;
    case VK_END:
      return ibus::End;
    case VK_HOME:
      return ibus::Home;
    case VK_LEFT:
      return ibus::Left;
    case VK_UP:
      return ibus::Up;
    case VK_RIGHT:
      return ibus::Right;
    case VK_DOWN:
      return ibus::Down;
    case VK_SELECT:
      return ibus::Select;
    case VK_PRINT:
      return ibus::Print;
    case VK_EXECUTE:
      return ibus::Execute;
    // case VK_SNAPSHOT:	return 0;
    case VK_INSERT:
      return ibus::Insert;
    case VK_DELETE:
      return ibus::Delete;
    case VK_HELP:
      return ibus::Help;

    case VK_LWIN:
      return ibus::Meta_L;
    case VK_RWIN:
      return ibus::Meta_R;
    // case VK_APPS:	return 0;
    // case VK_SLEEP:	return 0;
    case VK_NUMPAD0:
      return ibus::KP_0;
    case VK_NUMPAD1:
      return ibus::KP_1;
    case VK_NUMPAD2:
      return ibus::KP_2;
    case VK_NUMPAD3:
      return ibus::KP_3;
    case VK_NUMPAD4:
      return ibus::KP_4;
    case VK_NUMPAD5:
      return ibus::KP_5;
    case VK_NUMPAD6:
      return ibus::KP_6;
    case VK_NUMPAD7:
      return ibus::KP_7;
    case VK_NUMPAD8:
      return ibus::KP_8;
    case VK_NUMPAD9:
      return ibus::KP_9;
    case VK_MULTIPLY:
      return ibus::KP_Multiply;
    case VK_ADD:
      return ibus::KP_Add;
    case VK_SEPARATOR:
      return ibus::KP_Separator;
    case VK_SUBTRACT:
      return ibus::KP_Subtract;
    case VK_DECIMAL:
      return ibus::KP_Decimal;
    case VK_DIVIDE:
      return ibus::KP_Divide;
    case VK_F1:
      return ibus::F1;
    case VK_F2:
      return ibus::F2;
    case VK_F3:
      return ibus::F3;
    case VK_F4:
      return ibus::F4;
    case VK_F5:
      return ibus::F5;
    case VK_F6:
      return ibus::F6;
    case VK_F7:
      return ibus::F7;
    case VK_F8:
      return ibus::F8;
    case VK_F9:
      return ibus::F9;
    case VK_F10:
      return ibus::F10;
    case VK_F11:
      return ibus::F11;
    case VK_F12:
      return ibus::F12;
    case VK_F13:
      return ibus::F13;
    case VK_F14:
      return ibus::F14;
    case VK_F15:
      return ibus::F15;
    case VK_F16:
      return ibus::F16;
    case VK_F17:
      return ibus::F17;
    case VK_F18:
      return ibus::F18;
    case VK_F19:
      return ibus::F19;
    case VK_F20:
      return ibus::F20;
    case VK_F21:
      return ibus::F21;
    case VK_F22:
      return ibus::F22;
    case VK_F23:
      return ibus::F23;
    case VK_F24:
      return ibus::F24;

    case VK_NUMLOCK:
      return ibus::Num_Lock;
    case VK_SCROLL:
      return ibus::Scroll_Lock;

    case VK_LSHIFT:
      return ibus::Shift_L;
    case VK_RSHIFT:
      return ibus::Shift_R;
    case VK_LCONTROL:
      return ibus::Control_L;
    case VK_RCONTROL:
      return ibus::Control_R;
    case VK_LMENU:
      return ibus::Alt_L;
    case VK_RMENU:
      return ibus::Alt_R;

    case VK_OEM_AUTO:
      return ibus::Zenkaku_Hankaku;
    case VK_OEM_ENLW:
      return ibus::Zenkaku_Hankaku;
  }
  return ibus::Null;
}

// The character a chord produces under the given keyboard layout.
//
// The rules are the front end's own: the special keys come from the table
// above, every other key is resolved through ToUnicodeEx() with Ctrl and Alt
// cleared, so that only Shift decides the case of the character
// (ADR 0011 F2).
inline int ResolveCharacter(UINT vkey,
                            KeyInfo kinfo,
                            const BYTE* key_state,
                            HKL hkl) {
  const int code = TranslateKeycode(vkey, kinfo);
  if (code)
    return code;

  const int buf_len = 8;
  WCHAR buf[buf_len];
  BYTE table[256];
  // Clear the Ctrl and Alt state so that ToUnicodeEx() returns the
  // character the key produces without them.
  std::memcpy(table, key_state, sizeof(table));
  table[VK_CONTROL] = 0;
  table[VK_MENU] = 0;
  const int ret = ToUnicodeEx(vkey, UINT(kinfo), table, buf, buf_len, 0, hkl);
  if (ret == 1)
    return static_cast<int>(buf[0]);
  return 0;
}

// ---------------------------------------------------------------------------
// chord -> accept pattern
// ---------------------------------------------------------------------------

// The outcome of capturing a chord: the accept pattern the settings dialog
// has to write down, and the (key code, modifier) pair that pattern means.
struct CapturedAccept {
  bool ok;
  std::wstring pattern;
  int keycode;
  int modifier;
  AcceptError error;
  CapturedAccept() : ok(false), keycode(0), modifier(0), error(kAcceptOk) {}
};

// Capture a chord the way the settings dialog has to.
//
// One keyboard snapshot gives both the character the key produces and the
// modifiers that were held down, so the pattern can never mix a character
// from one snapshot with a modifier from another.
//
// The modifiers are the ones the front end reports: Shift, Control and Alt
// (ADR 0011 D3). The lock modifier is deliberately left out -- whether the
// front end should report CapsLock at all is still open (ADR 0011 Q2, task
// T20) and must not be decided here.
//
// The Windows key is refused outright: the front end never reports it, so a
// binding made with it could never fire (ADR 0011 D6).
inline CapturedAccept CaptureAccept(UINT vkey,
                                    KeyInfo kinfo,
                                    const BYTE* key_state,
                                    HKL hkl) {
  CapturedAccept out;
  if (!key_state) {
    out.error = kAcceptNoCharacter;
    return out;
  }
  if (vkey == VK_LWIN || vkey == VK_RWIN || (key_state[VK_LWIN] & 0x80) != 0 ||
      (key_state[VK_RWIN] & 0x80) != 0) {
    out.error = kAcceptWinKey;
    return out;
  }

  int modifier = 0;
  if ((key_state[VK_SHIFT] & 0x80) != 0)
    modifier |= ModifierByName("Shift");
  if ((key_state[VK_CONTROL] & 0x80) != 0)
    modifier |= ModifierByName("Control");
  if ((key_state[VK_MENU] & 0x80) != 0)
    modifier |= ModifierByName("Alt");
  out.modifier = modifier;

  const int keycode = ResolveCharacter(vkey, kinfo, key_state, hkl);
  out.keycode = keycode;
  if (!keycode) {
    out.error = kAcceptNoCharacter;
    return out;
  }

  std::string narrow;
  AcceptError error = kAcceptOk;
  if (!BuildAccept(keycode, modifier, &narrow, &error)) {
    out.error = error;
    return out;
  }
  if (!NarrowToWide(narrow, &out.pattern)) {
    out.error = kAcceptNoCharacter;
    return out;
  }
  out.ok = true;
  return out;
}

}  // namespace fluxing

#endif  // FLUXING_KEY_NAME_MAPPER_H_
