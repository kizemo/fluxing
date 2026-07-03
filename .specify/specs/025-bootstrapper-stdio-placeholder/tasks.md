# 025 - Tasks (placeholder)

## Phase 1 - Pre-flight (no code change)

- [ ] T001 verify spec 011 has shipped
  - Check `.specify/specs/011-fluxing-bootstrapper/` for production code
  - Check release notes / installer for the bootstrapper binary

## Phase 2 - Implementation (when unblocked)

- [ ] T002 [P] create test/TestBootstrapperStdio/TestBootstrapperStdio.cpp
- [ ] T003 [P] create test/TestBootstrapperStdio/TestBootstrapperStdio.vcxproj
- [ ] T004 [P] create test/TestBootstrapperStdio/TestBootstrapperStdio.vcxproj.filters
- [ ] T005 [P] create test/TestBootstrapperStdio/stdafx.{h,cpp}, targetver.h
- [ ] T006 add TestBootstrapperStdio to weasel.sln (L23 pattern)
- [ ] T007 add TestBootstrapperStdio to scripts/run-tests.bat (5 -> 7 entries)
- [ ] T008 run scripts/run-tests.bat (expect 7/7 green)
- [ ] T009 build installer, run silent-install smoke test (L28 .bat wrapper)
- [ ] T010 commit, tag, push

