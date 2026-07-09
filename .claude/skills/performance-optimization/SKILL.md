---
name: performance-optimization
description: Measure before optimizing. Profile first, find bottleneck, fix, measure again. Use when perf requirements exist or Core Web Vitals / load times regress.
---

# Performance Optimization

> Measure before optimizing. Performance work without measurement is guessing — and guessing leads to premature optimization that adds complexity without improving what matters.

## When to use

- Perf requirements exist in spec (e.g., SC-002: ≤80MB 常驻, SC-003: 60 FPS)
- User reports slow behavior
- Suspect change introduced regression

## When NOT to use

- No evidence of problem (YAGNI)
- "I think this might be slow" (measure first)

## Procedure

### Step 1: Define target

Be specific:
- "WeaselPanel at 60 FPS, P95 ≤16ms" (not "fast")
- "Memory ≤80MB including all P1 features" (not "small")
- "First deploy on cold start ≤5s" (not "responsive")

### Step 2: Measure baseline

```bash
# CPU sampling
xperf -on PROC_THREAD+LOADER -stackwalk Profile

# Memory
Get-Process WeaselServer | Select-Object WorkingSet, PrivateMemorySize

# For WeaselPanel
# Use TraceLogging in WeaselUI + Analyze with WPA
```

### Step 3: Find the actual bottleneck

NOT "the panel is slow" but:
- "OnPaint: 12ms / 16ms budget" → D2D render is the bottleneck
- "ProcessKeyEvent: 8ms" → IPC roundtrip
- "leveldb compaction: 30ms every 5s" → background thread

### Step 4: Optimize the ONE bottleneck

- Fix the one thing measured as the worst
- Don't "improve" the 2nd-worst while you're at it (scope creep)
- Apply: cache, batch, pre-allocate, lazy init, SIMD, etc.

### Step 5: Re-measure

Same tool, same workload. Verify improvement (e.g., 12ms → 5ms = 2.4× faster).

### Step 6: Add to L## if it's a real lesson

If the optimization revealed a counter-intuitive truth → L## entry.

## Fluxing-specific hot paths

- **WeaselPanel::DoPaint**: 60 FPS, monitor on every frame
- **RimeWithWeaselHandler::ProcessKeyEvent**: ≤1ms budget
- **librime::process_key**: 1-3ms (lua_processor is slowest)
- **WeaselUI/FluxingComponents::HandlePaint**: 16ms budget
- **D2D rt creation**: expensive, cache (L52)
- **L12 user_dict leveldb**: lock contention with Deployer (L55)

## Anti-patterns

- "Let me add a cache" (cache what? size? TTL? invalidation?)
- "This loop is O(n²)" (n=10 → premature; n=10000 → yes optimize)
- "I think it's the IPC" (measure first)
- Premature micro-optimization
- "Make it async" (async adds complexity, justify with evidence)
