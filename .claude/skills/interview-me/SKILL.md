---
name: interview-me
description: Extract what the user actually wants vs. what they say. Use when ask is underspecified ("build X" without "for whom" / "why now" / "success criterion" / "binding constraint"). One question + best guess per turn. STOP guessing and start asking.
---

# Interview-Me: Close the Intent Gap

> The cheapest moment to find the gap between what people ask for and what they actually want is **before** any plan/spec/code. Once you've started building, switching costs are real and the user rationalizes the wrong thing into "good enough."

## When to use

- Ask missing: **who** / **why** / **success** / **constraint**
- Conventional request ("build me X", "make it faster") that you can't unpack
- You're tempted to fill in silent assumptions
- Two reasonable values in tension (simplicity vs flexibility)

## When NOT to use

- Ask is already crisp (use brainstorming or spec-init directly)
- Bug fix (use systematic-debugging)
- User explicitly says "just do it"

## Procedure

### Step 1: Detect the gap

List which of these the ask is missing:
- [ ] **Who** (target user / audience)
- [ ] **Why now** (urgency, what triggered the request)
- [ ] **Success** (observable signal that "this is done")
- [ ] **Constraint** (must-not-violate: time, money, platform, compat)

If ≥2 are missing → run interview-me.

### Step 2: First question + best guess

Open with: "Before I start, let me check my understanding. My guess is [X]. [Question]?"

Example:
- User: "I want to add a candidate delete feature"
- Bad response: "What kind of delete? (right-click menu / hotkey / both? warn before delete? undo?)"
- Good response: "Quick check before I design — when a user deletes a candidate, do you want a 1-step undo (recent delete list) or no undo at all? My guess: 1-step undo for 5s window, because delete is destructive."

### Step 3: One question per turn

Never batch questions. Each turn = one question. The user can always say "you decide" to skip.

### Step 4: Predict and confirm

When you reach ≥95% confidence (you can predict what the user will say), state your predicted answer and ask: "If I write the spec with these answers, will you approve? [summary]?"

### Step 5: Hand off

Once approved → brainstorming (one more pass on the design) → spec-init.

## Anti-patterns

- "I think this is a good idea" without user confirmation
- Asking 8 questions at once
- "Just make it production-ready" (vague)
- Skipping the constraint question
