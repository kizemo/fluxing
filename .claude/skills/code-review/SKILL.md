---
name: code-review
description: Multi-axis code review. Use before merging any PR, after a feature, when reviewing self/another agent/human code. Covers 5 axes: correctness, readability, architecture, security, performance.
---

# Code Review and Quality

> Multi-dimensional review with quality gates. Approve when change **definitely improves overall code health** — perfect code doesn't exist. Don't block on personal preference.

## 5 axes of review

### 1. Correctness (highest priority)

- Does it do what spec.md says?
- Are edge cases handled (null, empty, max-size, off-by-one)?
- Are invariants documented and preserved?
- For Rime/Weasel: are L10 / L14 / L17 / L19 constraints respected?
- For TSF: is the new code called from the right thread (TSF thread model)?

### 2. Readability

- Can a new team member understand this in 5 minutes?
- Are names descriptive (`m_session_status_map` not `m_ssm`)?
- Are complex conditions broken into named booleans?
- Is the function length < 100 lines?

### 3. Architecture

- Does the change belong in this module? (WeaselTSF should not have IPC logic)
- Does it cross a module boundary unnecessarily?
- Does it introduce a new public API? If yes, is it justified?
- For Fluxing brand-fork: is the change within P8 waiver scope (see constitution §P8)?

### 4. Security

- Untrusted input handling: every file path, env var, registry value is hostile until validated
- IPC: WeaselIPCServer uses SID-scoped pipe SA (per code-map §2.3.1) — don't bypass
- Registry: writes should go through `RimeWithWeaselHandler` not direct Win32 API in app code
- DACL: `SecurityAttribute.cpp` in WeaselIPCServer is the only place to set DACL
- File I/O: validate path doesn't contain `..`, doesn't resolve outside expected root

### 5. Performance

- Did they profile before optimizing? (YAGNI)
- O(n²) in a hot path? (WeaselPanel::DoPaint runs at 60 FPS)
- Blocking I/O in TSF thread? (L07 lesson — `_EnsureServerConnected` is async)
- Unnecessary string copy? (use `const std::wstring&` not `std::wstring`)

## Review procedure

1. **Read spec.md first** — know the intent
2. **Skim plan.md** — know the approach
3. **Read tasks.md** — know what was supposed to be done
4. **Diff against master/kizemo/Fluxing** — what's the actual change?
5. **For each file**:
   - Correctness: trace 1 happy path + 1 error path
   - Readability: skim, count lines, check names
   - Architecture: check if it belongs here
   - Security: check input handling
   - Performance: check hot paths

## Approval standard

> Approve when change definitely improves overall code health, even if not perfect. Don't block because it isn't exactly how you would have written it.

If improvements are **important** but not blocking: comment + approve.  
If **must fix before merge**: block.

## Output format

```
Review of <PR-title-or-commit>:

✅ APPROVED with 2 comments

Comments (non-blocking):
- L45: Consider extracting magic number 4096 to named constant
- L102: Spec says US001-B should also reset; verify with manual test

Test verification:
- scripts\test-infra\run-test-suite.bat: 16/16 PASS (re-run to confirm)
- L14 byte-verify: WeaselServer.exe arch = 0x14C x86 ✓
```
