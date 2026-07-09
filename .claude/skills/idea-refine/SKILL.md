---
name: idea-refine
description: Refine raw ideas into sharp, actionable concepts. Use when an idea is still vague, need to stress-test assumptions, or want options before converging. Triggers on "ideate", "refine this idea", "stress-test my plan".
---

# Idea Refine

> Refine raw ideas into sharp, actionable concepts worth building through structured divergent and convergent thinking.

## How it works

```
1. Understand & Expand (divergent)
2. Evaluate & Converge
3. Sharpen & Ship
```

## Usage

This skill is primarily an interactive dialogue. Invoke with an idea, and the agent guides you.

## Step 1: Understand & Expand (divergent)

Restate the idea, ask sharpening questions, generate variations.

Sharpening questions (pick 1-2 per turn):
- What does "good" look like? (measurable)
- What's the 1-line user benefit?
- What's the smallest thing we could ship that proves value?
- Who would use this who doesn't currently? Who would NOT use this?
- What breaks if we don't ship this?

Variations (brainstorm 3-5):
- **Conservative**: minimal change, low risk, smaller value
- **Aggressive**: bigger change, higher risk, larger value
- **Adjacent**: different problem, similar solution
- **Inversion**: what if we did the opposite?
- **Hybrid**: combine 2 of the above

## Step 2: Evaluate & Converge

For each variation:
- Effort (hours / days / weeks)
- Risk (low / med / high)
- Value (low / med / high)
- Reversibility (can we roll back?)

Cluster: which 2-3 are worth pursuing?

## Step 3: Sharpen & Ship

Produce a one-pager:
```markdown
# Idea: <name>

## Problem
<1 sentence: who hurts, how, how often>

## Solution
<1 sentence: what we build>

## Out of scope
<explicit non-goals>

## Success criteria
<observable signal of done>

## MVP slice
<smallest shippable>

## Effort
<estimate>

## Risks
<2-3 max>
```

## Trigger phrases

- "Refine this idea"
- "Stress-test this plan"
- "I have a vague idea: X"
- "What if we did Y instead?"

## Anti-patterns

- Refining without user input (echo chamber)
- Producing 10 variations without converging
- "Perfect is the enemy of good" — but don't ship bad ideas either
- No time box (idea-refine in 15 min, not 2 hours)
