---
name: systematic-debugging
description: Use when encountering any bug, test failure, or unexpected behavior, BEFORE proposing fixes. NO FIXES WITHOUT ROOT CAUSE INVESTIGATION. 5 phases, mandatory sequence.
---

# Systematic Debugging

> Iron law: **NO FIXES WITHOUT ROOT CAUSE INVESTIGATION FIRST**. Symptom fixes are failure. Quick patches mask underlying issues and create new bugs.

## When to use

- ANY technical issue: test failure, production bug, unexpected behavior, perf regression, build break, integration fail
- ESPECIALLY under time pressure (emergencies make guessing tempting)
- ESPECIALLY when "just one quick fix" seems obvious
- ESPECIALLY when previous fix didn't work

## The 5 Phases (mandatory sequence)

### Phase 1: Root Cause Investigation

**Goal**: Find the actual cause, not the symptom.

Activities:
- Reproduce the bug deterministically
- Read the error message FULLY (no scrolling past)
- Check git blame for recent changes in the area
- Run with verbose logging
- Read source code of the failing component (don't guess from name)
- Use `codebase-recon` if unfamiliar with the code path
- For Rime/Weasel: check `lessons-learned.md` for similar past bugs (L## lookup)

**Output**: A hypothesis about the actual cause, expressed as:
> "The bug is caused by [specific thing] in [specific file:function], because [evidence]."

### Phase 2: Hypothesis Test

Design a test that **disconfirms** your hypothesis if it's wrong:
- Add a log line
- Run a minimal reproducer
- Check a specific value at a specific point

If the test confirms → Phase 3.
If the test disconfirms → back to Phase 1 with new hypothesis.

### Phase 3: Fix

**The fix is the smallest change that addresses Phase 1's root cause**, not the symptom.

Checklist:
- Does my fix address Phase 1's root cause, or just hide the symptom?
- Did I introduce a new invariant the code relies on?
- Is there a regression test I can add?
- Have I read AGENTS.md L## for similar past issues?

### Phase 4: Verify

**Run the test suite + manual verification + L## byte-verify if applicable**:
- For Rime/Weasel: `scripts/test-infra/run-test-suite.bat`
- For installer changes: AGENTS.md §2.5 silent-install smoke test
- For linker/PE changes: L14 arch-verify (PE machine type 0x14C/0x8664)
- For line-ending changes: L47 byte-verify (no 0xC0/0xC1 in source)
- For dark-mode changes: L42 byte-verify (0x001E1E1E in weasel.dll)

### Phase 5: Document

Add an L## entry to `.specify/memory/lessons-learned.md`:
- Symptom (1 paragraph)
- Root cause (specific)
- Fix (1-2 sentences)
- Lessons (3-5 numbered)
- Related (L## cross-references)
- Anti-patterns (named AP-##-A, B, C...)

Per R6 hard rule: "Done" requires pasted test output OR documented manual verification steps.

## Anti-patterns

- "It compiled, ship it" (NSIS L09 / L13 / L17 territory)
- "Let me try X" (random fix loop, no Phase 1)
- "The error says Y so the cause must be Y" (correlation ≠ causation)
- "I'll just add a try/catch" (hides symptom)
