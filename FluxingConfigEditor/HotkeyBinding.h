// spec 050 T001 - HotkeyBinding.h
//
// Data structure for one key_binder/bindings entry in default.yaml
// (or user custom.yaml). Reuses YamlRoundTrip for I/O. Independent
// of any UI framework; can be unit-tested with pure C++ + ASSERT.
//
// librime key_binder format reference (from output/data/default.yaml):
//   key_binder:
//     bindings:
//       - { when: composing, accept: Shift+Tab, send: Shift+Left }
//       - { when: has_menu, accept: comma, send: Page_Up }
//       - { when: always, toggle: ascii_mode, accept: Shift+space }
//       - { when: always, select: .next, accept: Control+Shift+1 }
//
// `when` is one of: always, composing, has_menu, paging (or empty/unset).
// `accept` is the key combination the user types. Examples:
//   "comma" (no modifier), "Shift+Tab" (Shift+Tab), "Control+Shift+F1",
//   "Shift+Shift_L" (mod=Shift, key=Shift_L - L21 pattern), "KP_0".
// `send`/`toggle`/`select` are mutually exclusive actions.
//   send: literal key to produce, e.g. "Page_Up", "2", "Return".
//   toggle: option name, e.g. "ascii_mode", "ascii_punct",
//           "traditionalization", "full_shape".
//   select: scheme selector, e.g. ".next" (next scheme in list).

#pragma once

#include <string>
#include <vector>

namespace fluxing {

enum class HotkeyWhen {
  Always,
  Composing,
  HasMenu,
  Paging,
  Empty  // unmapped / unset (used in some legacy entries)
};

enum class HotkeyAction {
  Send,
  Toggle,
  Select
};

// One entry in key_binder.bindings. Mirrors the librime yaml format
// but with strongly-typed enums for the discriminator fields.
struct HotkeyBinding {
  HotkeyWhen when = HotkeyWhen::Empty;
  std::wstring accept;          // "Control+Shift+F1"
  HotkeyAction action = HotkeyAction::Send;
  std::string action_value;     // "2" (send) / "ascii_mode" (toggle) / ".next" (select)

  // Empty binding (used for "reset" before re-population).
  bool IsEmpty() const { return accept.empty(); }

  // Serialize to the librime yaml inline-map form, e.g.
  //   { when: composing, accept: Shift+Tab, send: Shift+Left }
  // `indent` is the leading spaces per line (typically 4 within a
  // list of bindings under "key_binder:"). The single-line form is
  // preferred by librime and what default.yaml uses; multi-line is
  // also valid but uglier.
  std::string ToYamlInline(int indent = 4) const;

  // Parse the inverse: "{ when: ..., accept: ..., send/toggle/select: ... }"
  // into a HotkeyBinding. Returns true on success, false on malformed input.
  // This is a permissive parser - it does not validate every field, just
  // extracts the discriminator (when + accept + one of send/toggle/select).
  static bool FromYamlInline(const std::string& line, HotkeyBinding* out);
};

// Convert enums to/from their librime string representations. Centralized
// here so the dialog and the linter stay in sync.
const wchar_t* WhenToString(HotkeyWhen w);
HotkeyWhen WhenFromString(const std::wstring& s);
const char* ActionToString(HotkeyAction a);
HotkeyAction ActionFromString(const std::string& s);

// Normalize an "accept" string for display. Examples:
//   "comma" -> "comma" (unchanged)
//   "shift+tab" -> "Shift+Tab" (capitalize each token)
//   "ctrl+shift+f1" -> "Control+Shift+F1" (expand "ctrl" -> "Control")
//   "shift+space" -> "Shift+space" (preserve librime's lowercase space)
std::wstring NormalizeAccept(const std::wstring& accept);

// L17/L18/L19 linter: returns nullptr if accept is safe, otherwise a
// static-string warning message suitable for MessageBox. Used by
// HotkeyEditorDialog::OnSave before persisting.
const wchar_t* LintAcceptForRiskyBindings(const std::wstring& accept);

}  // namespace fluxing