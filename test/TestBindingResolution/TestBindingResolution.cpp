// TestBindingResolution.cpp : Behavior-level test for spec 005 v1.1
// key_binder bindings. Scaffolds TDD.md sec 3 TestBindingResolution.
// See .specify\\specs\\016-behavior-level-test-framework\\spec.md for context.
//
// SCAFFOLD MODE (spec 016 / 2026-07-02):
// - The test project builds and runs
// - main() returns 0 with a "SCAFFOLD MODE" message
// - Real assertions (parsing default.yaml, building KeyEvent, driving
//   librime's key_binder) are STUBBED and documented in comments
// - The scaffold will be filled in once librime 1.13+ is built
//   (output\\Win32\\rime.lib exists) and the rime_api.h C API can be linked
//
// Why SCAFFOLD first (per spec 016 sec 2.4 R1):
// - librime is Win32-only and built with cmake (AGENTS.md sec 4.4)
// - librime is NOT built in the spec 015 build loop (scripts\run-tests.bat)
// - A test that requires librime would block CI until librime is built
// - SCAFFOLD mode lets the test project exist + register + compile
//   without blocking anything

#include "stdafx.h"
#include <iostream>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    std::cout << "TestBindingResolution: SCAFFOLD MODE - no assertions yet" << std::endl;
    std::cout << "  spec 016 / 2026-07-02 - see .specify\\specs\\016-behavior-level-test-framework\\spec.md" << std::endl;
    std::cout << "  When librime 1.13+ is built (output\\Win32\\rime.lib exists)," << std::endl;
    std::cout << "  fill in the assertions below to close the L18 / L19 testing gap." << std::endl;
    return 0;
}

// =====================================================================
// PLANNED ASSERTIONS (spec 016 sec 2.3) - to be filled in when librime
// is available.
//
// Required setup (one-time, per AGENTS.md sec 4.4):
//   1. Build librime: cd librime && cmake -AWin32 ..
//   2. Verify rime.lib exists at librime\build\lib\Release\rime.lib
//   3. Verify rime.dll exists at output\Win32\rime.dll (copied by build.bat)
//
// Required .vcxproj additions (see TestBindingResolution.vcxproj):
//   - AdditionalIncludeDirectories: \librime\include
//   - AdditionalLibraryDirectories: \librime\build\lib\Release
//   - AdditionalDependencies: rime.lib
//   - PreprocessorDefinitions: RIME_IMPORTS (so rime_api.h uses dllimport)
//
// Required test framework code:
//   #include <rime_api.h>           // librime C API
//   #include <yaml-cpp/yaml.h>      // vendored in librime\include\yaml-cpp
//   #include <boost/detail/lightweight_test.hpp>  // like other tests
//
// Test 1: parse default.yaml and verify the spec 014 binding form
//   - Load output\data\default.yaml
//   - Walk to key_binder.bindings.has_menu[*]
//   - For each binding, parse accept: as rime::KeySequence via
//     rime::KeySequence::Parse (librime C++ wrapper)
//   - Assert that the binding ccept: Shift+Shift_L parses to a
//     single KeyEvent with keycode=Shift_L, modifier=kShiftMask
//   - Assert that there is NO binding ccept: Shift_L (bare,
//     modifier=0) in the key_binder - this is the L19 guard
//
// Test 2: verify release event does NOT match the has_menu binding
//   - Initialize rime::KeyEvent with keycode=Shift_L, modifier=kReleaseMask
//     (simulating a TSF release event)
//   - Verify that this KeyEvent does NOT match ccept: Shift+Shift_L
//   - This is the L18 invariant: the spec 014 binding form MUST
//     distinguish release (modifier=kReleaseMask) from press-with-modifier
//     (modifier=kShiftMask)
//
// Test 3: verify has_menu binding order (per spec 014 / L21 sec 1.3)
//   - Verify ccept: Shift+Shift_L appears BEFORE ccept: Control+1
//     in the key_binder.bindings.has_menu list
//   - librime uses first-match-wins, so order matters for consistency
//
// Test 4: verify has_menu binding resolution
//   - Initialize rime and load key_binder via rime_get_api()->start_maintenance
//   - For each spec 005 binding, simulate a process_key call and verify
//     the expected action is returned
//   - This is the FULL runtime test that L18 / L19 lacked
//
// Reference: librime\src\rime\key_event.h for KeyEvent API
//            librime\include\yaml-cpp for YAML parsing
//            output\data\default.yaml for the binding source of truth
// =====================================================================