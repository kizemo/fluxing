// PaletteFieldTest.h - spec 040 T003 (2026-07-12)
//
// Header for the extended Palette field tests. See PaletteFieldTest.cpp
// for layout invariants and field round-trip assertions.
//
// Integration: TestDarkModeBridge.cpp's main() calls RunPaletteFieldTests()
// AFTER the spec 033 T0-T8 block. Returns 0 on all-pass / 1 on any-fail.
// All assertions inside use static_assert (compile-time layout) and a
// printf-based Check() helper (runtime field values). Both files compile
// into a single TestDarkModeBridge.exe.

#pragma once

int RunPaletteFieldTests();