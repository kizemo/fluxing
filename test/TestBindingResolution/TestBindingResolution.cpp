// TestBindingResolution.cpp : Behavior-level test for spec 005 v1.1
// key_binder bindings. spec 018 (2026-07-02) replaces the SCAFFOLD /
// LINKED stub with 4 real assertions that close the L18 / L19 testing
// gap (per TDD.md sec 3.2 mock-librime principle).
//
// See .specify\\specs\\018-fill-binding-resolution\\spec.md for context.
// Related: spec 014 (shift select 2nd/3rd candidate contract),
// spec 016 (scaffold), spec 017 (link-probe), L16 (modifier case),
// L18 (release event), L19 (bare Shift_L).

#include "stdafx.h"

#if __has_include(<rime_api.h>)
#include <rime_api.h>
#define RIME_API_H_PRESENT 1
#else
#define RIME_API_H_PRESENT 0
#endif

#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace mock {

// Modifier bits. Names chosen to mirror librime key_table.h values
// (kShiftMask=1, kControlMask=4, kReleaseMask=256). The mock uses
// distinct bit positions so a kRelease bit does not collide with
// kShift in equality comparison (which is the L18 invariant).
enum Modifier : int {
    kNone     = 0,
    kShift    = 1 << 0,   // mirrors kShiftMask
    kLock     = 1 << 1,
    kControl  = 1 << 2,   // mirrors kControlMask
    kAlt      = 1 << 3,   // mirrors kMod1Mask / kAltMask
    kSuper    = 1 << 6,   // mirrors kMod4Mask
    kRelease  = 1 << 8,   // mirrors kReleaseMask (X11 bit 15 -> 256)
};

struct KeyEvent {
    int keycode;   // X11 keysym-equivalent
    int modifier;  // OR of Modifier bits
    std::string repr() const;
};

// Modifier name -> bit. Case-sensitive first letter per L16.
// `Shift` parses; `shift` is a parse error.
static const std::map<std::string, int>& modifier_table() {
    static const std::map<std::string, int> t = {
        {"Shift",   kShift},
        {"Control", kControl},
        {"Alt",     kAlt},
        {"Super",   kSuper},
        {"Release", kRelease},
    };
    return t;
}

// Key name -> keycode (X11 keysym values; Windows has the same
// values in rime_api_deprecated.h RIME_KEY_* constants). Only the
// names that actually appear in output\data\default.yaml are listed.
static const std::map<std::string, int>& keyname_table() {
    static const std::map<std::string, int> t = {
        {"Shift_L",     0xffe1},
        {"Shift_R",     0xffe2},
        {"Control_L",   0xffe3},
        {"Control_R",   0xffe4},
        {"space",       0x0020},
        {"Tab",         0xff09},
        {"Left",        0xff51},
        {"Right",       0xff53},
        {"Up",          0xff52},
        {"Down",        0xff54},
        {"Page_Up",     0xff55},
        {"Page_Down",   0xff56},
        {"Home",        0xff50},
        {"End",         0xff57},
        {"comma",       0x002c},
        {"period",      0x002e},
        {"bracketleft", 0x005b},
        {"bracketright",0x005d},
        {"grave",       0x0060},
        {"exclam",      0x0021},
        {"at",          0x0040},
        {"dollar",      0x0024},
        {"Return",       0xff0d},
        {"asterisk",     0x002a},
        {"plus",         0x002b},
        {"minus",        0x002d},
        {"slash",        0x002f},
        {"numbersign",   0x0023},
        {"KP_0",         0xffb0},
        {"KP_1",         0xffb1},
        {"KP_2",         0xffb2},
        {"KP_3",         0xffb3},
        {"KP_4",         0xffb4},
        {"KP_5",         0xffb5},
        {"KP_6",         0xffb6},
        {"KP_7",         0xffb7},
        {"KP_8",         0xffb8},
        {"KP_9",         0xffb9},
        {"KP_Decimal",   0xffae},
        {"KP_Multiply",  0xffaa},
        {"KP_Add",       0xffab},
        {"KP_Subtract",  0xffad},
        {"KP_Divide",    0xffaf},
        {"KP_Enter",     0xff8d},
    };
    return t;
}

// Single ASCII character (0-9, a-z) -> keycode. Used for keys like
// "1", "2", "a", "b" in `accept: Control+1`, `accept: Shift+a`.
static int char_keycode(char c) {
    if (c >= '0' && c <= '9') return c;
    if (c >= 'a' && c <= 'z') return c;
    if (c >= 'A' && c <= 'Z') return c;
    return 0;  // unknown
}

// Parse a single key name (after modifiers stripped). Returns 0 on
// unknown. Does NOT consume input.
static int parse_keyname(const std::string& name) {
    if (name.empty()) return 0;
    auto it = keyname_table().find(name);
    if (it != keyname_table().end()) return it->second;
    if (name.size() == 1) return char_keycode(name[0]);
    return 0;
}

// Parse "Shift+Shift_L" / "Control+1" / "Shift+space" / "Release+Shift_L"
// into a KeyEvent. Returns false on parse error.
// Case-sensitive first letter on modifier names per L16.
static bool ParseKeyEvent(const std::string& s, KeyEvent* out) {
    if (!out) return false;
    out->keycode = 0;
    out->modifier = kNone;
    if (s.empty()) return false;

    // Split by '+'. The last token is the key name; the rest are
    // modifiers.
    std::vector<std::string> tokens;
    size_t start = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') {
            tokens.push_back(s.substr(start, i - start));
            start = i + 1;
        }
    }
    tokens.push_back(s.substr(start));

    if (tokens.empty()) return false;
    std::string keyname = tokens.back();
    tokens.pop_back();

    int mod = kNone;
    for (const auto& tok : tokens) {
        if (tok.empty()) return false;
        auto it = modifier_table().find(tok);
        if (it == modifier_table().end()) {
            // Case-sensitive first letter: `shift` (lowercase s) is
            // not in the table. Reject.
            return false;
        }
        mod |= it->second;
    }

    int kc = parse_keyname(keyname);
    if (kc == 0) return false;

    out->keycode = kc;
    out->modifier = mod;
    return true;
}

std::string KeyEvent::repr() const {
    static const std::vector<std::pair<int, std::string>> mod_names = {
        {kRelease, "Release"},
        {kSuper,   "Super"},
        {kAlt,     "Alt"},
        {kControl, "Control"},
        {kShift,   "Shift"},
    };
    std::string r;
    int m = modifier;
    for (const auto& p : mod_names) {
        if (m & p.first) {
            if (!r.empty()) r += "+";
            r += p.second;
            m &= ~p.first;
        }
    }
    if (!r.empty()) r += "+";
    // Render keycode as hex if not a printable ASCII char
    if (keycode >= 0x20 && keycode <= 0x7e) r += (char)keycode;
    else {
        char buf[16];
        snprintf(buf, sizeof(buf), "0x%04x", keycode);
        r += buf;
    }
    return r;
}

// Mirrors librime 1.13 key_event.h:64 KeyEvent::operator==.
// This is the L18 invariant in code form: strict keycode+modifier
// comparison. A binding with (Shift_L, Shift) does NOT match a
// press event with (Shift_L, Release).
static bool Match(const KeyEvent& binding, const KeyEvent& pressed) {
    return binding.keycode == pressed.keycode &&
           binding.modifier == pressed.modifier;
}

}  // namespace mock

// Read entire file as string. Returns empty string on error.
static std::string ReadFile(const char* path) {
    std::ifstream f(path);
    if (!f.is_open()) return std::string();
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// One binding parsed from key_binder.bindings[*].
struct Binding {
    std::string when;
    std::string accept;       // raw text
    std::string send;
    mock::KeyEvent parsed;    // parsed from accept
    bool parse_ok;
};

// Walk default.yaml and extract the key_binder.bindings[*] entries.
// Returns the count parsed (also fills `out`).
// This is NOT a general yaml parser; it is a hand-rolled scanner
// for the specific format `- { when: ..., accept: ..., send: ... }`.
static std::vector<Binding> ScanDefaultYaml(const std::string& content) {
    std::vector<Binding> out;
    // Find "key_binder:" then "bindings:" then walk the file line by
    // line. A binding is a line that starts (after optional leading
    // spaces) with "- {" and contains "when:" and "accept:". Stop
    // when we hit a non-indented line that is NOT a binding -- that
    // is the next top-level yaml key (e.g. "recognizer:", "translator:",
    // or the end of the key_binder block).
    size_t kb = content.find("key_binder:");
    if (kb == std::string::npos) return out;
    size_t bn = content.find("bindings:", kb);
    if (bn == std::string::npos) return out;

    // Skip the "bindings:" line itself (no leading spaces, not a binding).
    size_t first_eol = content.find('\n', bn);
    if (first_eol == std::string::npos) first_eol = content.size();
    size_t pos = first_eol + 1;
    while (pos < content.size()) {
        size_t eol = content.find('\n', pos);
        if (eol == std::string::npos) eol = content.size();
        std::string line = content.substr(pos, eol - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        pos = eol + 1;

        if (line.empty()) continue;
        size_t lead = 0;
        while (lead < line.size() && (line[lead] == ' ' || line[lead] == '\t')) lead++;
        std::string trimmed = line.substr(lead);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        // Stop on a non-indented top-level yaml key.
        if (lead == 0 && trimmed[0] != '-') break;

        if (trimmed.find("- {") == std::string::npos) continue;
        if (trimmed.find("when:") == std::string::npos) continue;
        if (trimmed.find("accept:") == std::string::npos) continue;

        auto extract_field = [&trimmed](const std::string& field) -> std::string {
            std::string key = field + ": ";
            size_t p = trimmed.find(key);
            if (p == std::string::npos) {
                key = field + ":";
                p = trimmed.find(key);
                if (p == std::string::npos) return std::string();
                p += key.size();
            } else {
                p += key.size();
            }
            size_t end = trimmed.find_first_of(",}", p);
            if (end == std::string::npos) end = trimmed.size();
            std::string v = trimmed.substr(p, end - p);
            while (!v.empty() && (v.back() == ' ' || v.back() == '\t')) v.pop_back();
            while (!v.empty() && (v.front() == ' ' || v.front() == '\t')) v.erase(v.begin());
            return v;
        };

        Binding b;
        b.when = extract_field("when");
        b.accept = extract_field("accept");
        b.send = extract_field("send");
        b.parse_ok = mock::ParseKeyEvent(b.accept, &b.parsed);
        out.push_back(b);
    }
    return out;
}


int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    int passed = 0, failed = 0;
    auto check = [&](const char* desc, bool cond) {
        if (cond) { std::cout << "  PASS: " << desc << std::endl; passed++; }
        else      { std::cout << "  FAIL: " << desc << std::endl; failed++; }
    };

#if RIME_API_H_PRESENT
    // Spec 017 link-probe: declare rime_get_api as a function pointer
    // to force the linker to resolve the symbol from rime.lib. The
    // pointer is never dereferenced (would need rime.dll at runtime;
    // we respect TDD.md sec 3.2 mock-librime principle).
    RimeApi* (*get_api_ptr)() = rime_get_api;
    (void)get_api_ptr;
    std::cout << "TestBindingResolution: LINKED rime.lib"
              << " (rime_get_api resolved at link time, sizeof(RimeApi)=" << sizeof(RimeApi) << ")"
              << std::endl;
    std::cout << "  spec 018 / 2026-07-02 - 4 assertions on output\\data\\default.yaml"
              << std::endl;
#else
    std::cout << "TestBindingResolution: SCAFFOLD MODE - rime_api.h not found" << std::endl;
    std::cout << "  spec 016 / 2026-07-02 - run build.bat rime to enable link-probe" << std::endl;
#endif

    // Read default.yaml (argv[1] override matches TestDefaultHotkeys / TestShiftSelectBinding)
    const char* path = (argc > 1) ? argv[1] : "output\\data\\default.yaml";
    std::string content = ReadFile(path);
    if (content.empty()) {
        std::cout << "  FAIL: cannot read " << path << std::endl;
        return 1;
    }
    std::vector<Binding> bindings = ScanDefaultYaml(content);
    std::cout << "  parsed " << bindings.size() << " key_binder bindings from " << path
              << std::endl;
    if (bindings.empty()) {
        std::cout << "  FAIL: no bindings found (yaml scanner broken?)" << std::endl;
        return 1;
    }

    // ---- Test 1: parser sanity ----
    // Every accept: in key_binder.bindings[*] must parse to a valid KeyEvent.
    int t1_ok = 0, t1_fail = 0;
    for (const auto& b : bindings) {
        if (b.parse_ok) t1_ok++;
        else t1_fail++;
    }
    check("Test 1: every binding accept: parses to a valid KeyEvent",
          t1_fail == 0 && t1_ok > 0);
    if (t1_fail > 0) {
        std::cout << "    (debug: " << t1_fail << " bindings failed to parse)" << std::endl;
    }

    // ---- Test 2: spec 077 inverted (Shift+Shift_L binding removed in T15) ----
    // Originally this tested L18 invariant: TSF release event does NOT match
    // Shift+Shift_L binding. After spec 077 T15 the binding is gone, so the
    // contract is now "no Shift+Shift_L binding exists". We assert the new
    // contract and keep the original L18 invariant via Test 4c (no bare
    // Shift_L binding exists).
    bool has_shift_shift_l = false;
    for (const auto& b : bindings) {
        if (b.accept == "Shift+Shift_L" && b.parse_ok) has_shift_shift_l = true;
    }
    check("Test 2: spec 077 — no Shift+Shift_L binding exists (was L18 invariant)",
          !has_shift_shift_l);

    // ---- Test 3: spec 077 ordering — Shift+Shift_L removed, so idx_shift == -1 ----
    // Originally tested spec 014 ordering. After spec 077 the binding is gone;
    // we assert the binding does not appear in the list (idx == -1).
    int idx_shift = -1, idx_ctrl = -1;
    for (size_t i = 0; i < bindings.size(); ++i) {
        if (idx_shift < 0 && bindings[i].accept == "Shift+Shift_L") idx_shift = (int)i;
        if (idx_ctrl  < 0 && bindings[i].accept == "Control+1")     idx_ctrl  = (int)i;
    }
    {
        std::stringstream ss;
        ss << "Test 3: spec 077 — Shift+Shift_L not in bindings list (idx="
           << idx_shift << "), Control+1 still at idx=" << idx_ctrl;
        check(ss.str().c_str(), idx_shift < 0 && idx_ctrl >= 0);
    }

    // ---- Test 4: spec 077 inverted + L19 guard ----
    // (a) has_menu + Shift+Shift_L binding must NOT exist (spec 077 inverted)
    // (b) has_menu + Control+1 binding must still exist (spec 005 contract)
    // (c) NO bare `accept: Shift_L` (L19 form) in key_binder
    bool has_menu_shift = false, has_menu_ctrl = false, bare_shift = false;
    for (const auto& b : bindings) {
        if (b.when == "has_menu" && b.accept == "Shift+Shift_L") has_menu_shift = true;
        if (b.when == "has_menu" && b.accept == "Control+1")     has_menu_ctrl  = true;
        // L19 guard: bare `accept: Shift_L` (not `Shift+Shift_L`).
        // Match the exact pattern with comma terminator or space suffix.
        if (b.accept == "Shift_L" || b.accept == "Shift_L," ||
            b.accept == "Shift_R" || b.accept == "Shift_R,") {
            bare_shift = true;
        }
    }
    check("Test 4a: spec 077 — has_menu + accept: Shift+Shift_L must NOT exist",
          !has_menu_shift);
    check("Test 4b: has_menu + accept: Control+1 exists (spec 005 contract)",
          has_menu_ctrl);
    check("Test 4c: NO bare accept: Shift_L or Shift_R (L19 guard)",
          !bare_shift);

    std::cout << "  " << passed << " / " << (passed + failed) << " assertions passed" << std::endl;
    return failed == 0 ? 0 : 1;
}
