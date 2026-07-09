---
name: verification-before-completion
description: Use when about to claim work is complete, fixed, or passing, before committing or creating PRs. Iron law: NO COMPLETION CLAIMS WITHOUT FRESH VERIFICATION EVIDENCE.
---

# Verification Before Completion

> **Core principle**: Evidence before claims, always. **Violating the letter of this rule is violating the spirit of this rule.**

## Iron Law

```
NO COMPLETION CLAIMS WITHOUT FRESH VERIFICATION EVIDENCE
```

If you haven't run the verification command in **this message**, you cannot claim it passes.

## The Gate Function

```
BEFORE claiming any status or expressing satisfaction:

1. IDENTIFY: What command proves this claim?
2. RUN: Execute the FULL command (fresh, complete)
3. READ: Full output, check exit code, count failures
4. VERIFY: Does output confirm the claim?
   - If NO: State actual status with evidence
   - If YES: State claim WITH evidence (paste exit code + key output)
5. ONLY THEN: Make the claim
```

Skip any step = lying, not verifying.

## Project-Specific Verification Commands

| Claim | Command |
|---|---|
| Tests pass | `cmd /c "scripts\test-infra\run-test-suite.bat"` |
| Build succeeds | `xmake build WeaselServer` (or `msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32`) |
| Installer is clean | AGENTS.md §2.5 silent-install smoke test recipe |
| PE arch consistent | L14 byte-verify (x86=0x14C, x64=0x8664) |
| Source file byte-healthy | L47 byte-verify (0xC0/0xC1 = 0, BOM only on .nsi/.rc) |
| Dark-mode bits present | L42 byte-verify (0x001E1E1E in weasel.dll) |
| UTF-8 clean (no 0x3F substitution) | L40 byte-verify (0x3F count vs expected) |
| Hotkey routing works | L49 pre-flight guard in run-test-suite.bat |

## Output Format

Always show:
1. Command run (with full path)
2. Exit code
3. Key output lines (PASS/FAIL counts, byte values, etc.)

Example:
```
$ /d/Program\ Files/LLVM/bin/clang-format.exe --version
clang-format version 18.1.8
exit 0
```

## Common Failures

- "I think it works" (no command run)
- "I ran it earlier in the session" (stale, run fresh)
- "It's the same code I tested before" (but did you run **this** version?)
- "The build succeeded on the test machine" (run on **this** machine, with **this** toolchain)
