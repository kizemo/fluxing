// PaletteFieldTest.cpp - spec 040 T003 (2026-07-12)
//
// Layout assertions + field round-trip tests for the extended
// Palette struct (10 new fields added in spec 040 T001-T002).
//
// Layout invariants (compile-time; R-007 mitigation):
//   - offsetof(Palette, back)         == 0   (backward compat, byte-equal
//                                              to spec 033 consumers)
//   - offsetof(Palette, text)         == 4
//   - offsetof(Palette, hilited_back) == 8
//   - offsetof(Palette, hilited_text) == 12
//   - sizeof(Palette)                 == 52  (4 DWORDs + 8 DWORDs + 2 BYTEs,
//                                              padded to 4-byte boundary)
//   - sizeof(Palette) % 4             == 0   (4-byte alignment)
//
// Field values verified against docs/design/FLUENT-UI-TOKENS.md §3.1
// and the Decision 1 change to kPaletteLight.hilited_text
// (0x000080 navy -> 0x0A84FF Apple systemBlue).
//
// Integration:
//   PaletteFieldTest.cpp exports RunPaletteFieldTests() returning
//   0 on all-pass / 1 on any-fail. TestDarkModeBridge.cpp's main()
//   calls this after the spec 033 T0-T8 block. Both files compile
//   into a single TestDarkModeBridge.exe.

#include "stdafx.h"
#include "FluxingDarkModeBridge.h"
#include <cstddef>
#include <cstdio>

// ---------------------------------------------------------------------------
// Compile-time layout assertions (fire at compile time, not runtime).
// A layout regression MUST NOT ship silently; if any of these fail the
// test exe will not link.
// ---------------------------------------------------------------------------
static_assert(offsetof(fluxing::Palette, back) == 0,
              "spec 040 R-007: Palette.back must be at offset 0 "
              "(backward compat with spec 033 consumers)");
static_assert(offsetof(fluxing::Palette, text) == 4,
              "spec 040 R-007: Palette.text must be at offset 4");
static_assert(offsetof(fluxing::Palette, hilited_back) == 8,
              "spec 040 R-007: Palette.hilited_back must be at offset 8");
static_assert(offsetof(fluxing::Palette, hilited_text) == 12,
              "spec 040 R-007: Palette.hilited_text must be at offset 12");
static_assert(sizeof(fluxing::Palette) == 52,
              "spec 040 R-007: Palette must be 52 bytes "
              "(4 + 8 DWORDs = 48, +2 BYTEs = 50, padded to 52)");
static_assert(sizeof(fluxing::Palette) % 4 == 0,
              "spec 040 R-007: Palette must be 4-byte aligned");

namespace {

int g_pass = 0;
int g_fail = 0;

void Check(bool cond, const char* name) {
  if (cond) {
    ++g_pass;
    std::printf("  PASS: %s\n", name);
  } else {
    ++g_fail;
    std::printf("  FAIL: %s\n", name);
  }
}

}  // namespace

int RunPaletteFieldTests() {
  using fluxing::Palette;
  using fluxing::FluxingDarkModeBridge;

  std::printf("\n--- Palette field tests (spec 040 T003) ---\n");

  // -----------------------------------------------------------------
  // F1: Dark palette byte values (existing 4 fields, byte-equal to spec 033)
  // -----------------------------------------------------------------
  {
    auto* bridge = FluxingDarkModeBridge::Get();
    bridge->SetDarkForTest(true);
    Palette p = bridge->CurrentPalette();
    Check(p.back == 0x1E1E1E, "F1a: dark back == 0x1E1E1E");
    Check(p.text == 0xE0E0E0, "F1b: dark text == 0xE0E0E0");
    Check(p.hilited_back == 0x2D2D30, "F1c: dark hilited_back == 0x2D2D30");
    Check(p.hilited_text == 0xFFFFFF, "F1d: dark hilited_text == 0xFFFFFF");
  }

  // -----------------------------------------------------------------
  // F2: Light palette byte values (existing 4 fields; hilited_text
  //     CHANGED from 0x000080 to 0x0A84FF per Decision 1)
  // -----------------------------------------------------------------
  {
    auto* bridge = FluxingDarkModeBridge::Get();
    bridge->SetDarkForTest(false);
    Palette p = bridge->CurrentPalette();
    Check(p.back == 0xF0F0F0, "F2a: light back == 0xF0F0F0");
    Check(p.text == 0x000000, "F2b: light text == 0x000000");
    Check(p.hilited_back == 0xD0D0D0, "F2c: light hilited_back == 0xD0D0D0");
    Check(p.hilited_text == 0x0A84FF,
          "F2d: light hilited_text == 0x0A84FF (Apple systemBlue, Decision 1 CHANGED)");
  }

  // -----------------------------------------------------------------
  // F3: Dark palette new fields (spec 040 T001-T002)
  // -----------------------------------------------------------------
  {
    auto* bridge = FluxingDarkModeBridge::Get();
    bridge->SetDarkForTest(true);
    Palette p = bridge->CurrentPalette();
    Check(p.panel_glass_top == 0xC8E1FA,
          "F3a: dark panel_glass_top == 0xC8E1FA (QuickPanel L87)");
    Check(p.panel_glass_bot == 0x9BC3F0,
          "F3b: dark panel_glass_bot == 0x9BC3F0 (QuickPanel L87)");
    Check(p.icon_dim == 0x3C3C43,
          "F3c: dark icon_dim == 0x3C3C43 (RGB 60,60,67)");
    Check(p.accent_primary == 0xFF5F31,
          "F3d: dark accent_primary == 0xFF5F31 (品牌橙, Decision 1)");
    Check(p.accent_secondary == 0x9B51E0,
          "F3e: dark accent_secondary == 0x9B51E0 (品牌紫)");
    Check(p.hairline == 0x333333,
          "F3f: dark hairline == 0x333333 (10% white on dark)");
    Check(p.highlight_top == 0xFFFFFF,
          "F3g: dark highlight_top == 0xFFFFFF");
    Check(p.destructive == 0xD04545,
          "F3h: dark destructive == 0xD04545");
    Check(p.alpha_panel_top == 220,
          "F3i: dark alpha_panel_top == 220 (L87 value 0xDC)");
    Check(p.alpha_panel_bot == 80,
          "F3j: dark alpha_panel_bot == 80 (L87 value 0x50)");
  }

  // -----------------------------------------------------------------
  // F4: Light palette new fields (hairline differs from dark)
  // -----------------------------------------------------------------
  {
    auto* bridge = FluxingDarkModeBridge::Get();
    bridge->SetDarkForTest(false);
    Palette p = bridge->CurrentPalette();
    Check(p.panel_glass_top == 0xC8E1FA,
          "F4a: light panel_glass_top == 0xC8E1FA");
    Check(p.panel_glass_bot == 0x9BC3F0,
          "F4b: light panel_glass_bot == 0x9BC3F0");
    Check(p.icon_dim == 0x3C3C43, "F4c: light icon_dim == 0x3C3C43");
    Check(p.accent_primary == 0xFF5F31, "F4d: light accent_primary == 0xFF5F31");
    Check(p.accent_secondary == 0x9B51E0, "F4e: light accent_secondary == 0x9B51E0");
    Check(p.hairline == 0xEBEBEB,
          "F4f: light hairline == 0xEBEBEB (8% black on light, differs from dark)");
    Check(p.highlight_top == 0xFFFFFF, "F4g: light highlight_top == 0xFFFFFF");
    Check(p.destructive == 0xD04545, "F4h: light destructive == 0xD04545");
    Check(p.alpha_panel_top == 220, "F4i: light alpha_panel_top == 220");
    Check(p.alpha_panel_bot == 80, "F4j: light alpha_panel_bot == 80");
  }

  // -----------------------------------------------------------------
  // F5: Round-trip — construct palette manually, verify all 14
  //     fields are independent (no aliasing / packing bugs)
  // -----------------------------------------------------------------
  {
    Palette p{};  // value-initialized to zero
    p.back = 0x111111;
    p.text = 0x222222;
    p.hilited_back = 0x333333;
    p.hilited_text = 0x444444;
    p.panel_glass_top = 0x555555;
    p.panel_glass_bot = 0x666666;
    p.icon_dim = 0x777777;
    p.accent_primary = 0x888888;
    p.accent_secondary = 0x999999;
    p.hairline = 0xAAAAAA;
    p.highlight_top = 0xBBBBBB;
    p.destructive = 0xCCCCCC;
    p.alpha_panel_top = 0xAB;
    p.alpha_panel_bot = 0xCD;
    Check(p.back == 0x111111, "F5a: round-trip back");
    Check(p.text == 0x222222, "F5b: round-trip text");
    Check(p.hilited_back == 0x333333, "F5c: round-trip hilited_back");
    Check(p.hilited_text == 0x444444, "F5d: round-trip hilited_text");
    Check(p.panel_glass_top == 0x555555, "F5e: round-trip panel_glass_top");
    Check(p.panel_glass_bot == 0x666666, "F5f: round-trip panel_glass_bot");
    Check(p.icon_dim == 0x777777, "F5g: round-trip icon_dim");
    Check(p.accent_primary == 0x888888, "F5h: round-trip accent_primary");
    Check(p.accent_secondary == 0x999999, "F5i: round-trip accent_secondary");
    Check(p.hairline == 0xAAAAAA, "F5j: round-trip hairline");
    Check(p.highlight_top == 0xBBBBBB, "F5k: round-trip highlight_top");
    Check(p.destructive == 0xCCCCCC, "F5l: round-trip destructive");
    Check(p.alpha_panel_top == 0xAB, "F5m: round-trip alpha_panel_top");
    Check(p.alpha_panel_bot == 0xCD, "F5n: round-trip alpha_panel_bot");
  }

  std::printf("Palette field tests: %d / %d passed\n", g_pass, g_pass + g_fail);
  return g_fail == 0 ? 0 : 1;
}