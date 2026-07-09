---
name: brainstorming
description: HARD-GATE before any creative work — feature design, component design, behavior change. Explores user intent, requirements, and design through one-question-at-a-time dialogue. Do NOT code before design is approved.
---

# Brainstorming: Turn Ideas Into Designs

> HARD-GATE: Do NOT write code, scaffold projects, or take implementation action until design is presented AND user approves.

## When to use

- New feature being designed
- Behavior change requested
- User says "I want X" without details

## When NOT to use

- Bug fix (use `systematic-debugging` first)
- Refactor with clear scope
- Pure information request

## Anti-pattern: "This Is Too Simple"

> Every project goes through this process. A todo list, a single-function utility, a config change — all of them. The design can be short (1-2 sentences for very simple), but you MUST present it and get approval.

## Procedure

### Step 1: Context probe

Before asking the first question, explore project context:
- Read `.specify/PRD.md` and `.specify/memory/constitution.md`
- Check recent commits in `git log --oneline -20`
- Look at related spec directories in `.specify/specs/`
- Run `codebase-recon` if unfamiliar

### Step 2: One-question-at-a-time

Ask **one** question per turn. Never dump a 10-question list. Each question should:
- Be specific and answerable
- Be the most important next unknown
- Come with your best guess attached ("My guess is X, agree?")

### Step 3: 4 pre-flight questions (per spec-init)

Once you have a vague-but-not-yet-crisp idea, ask:
1. **What is being built?**
2. **Who benefits and how?**
3. **What's the smallest shippable slice?**
4. **What is OUT of scope for v1?**

### Step 4: Design proposal

Once you can predict what the user will say:
- Present a 1-paragraph design summary
- List 2-3 alternatives with tradeoffs
- Recommend one with rationale
- Ask: "Approve? If yes, I'll write spec.md / plan.md / tasks.md"

### Step 5: Hand off to spec-init

After approval, invoke `spec-init` to produce the 3 artifacts.

## Common mistakes to avoid

- Asking 5 questions at once (paralyzes user)
- Skipping the 4 pre-flight questions
- Coding before approval
- Vague design summary ("make it better" — be specific)
