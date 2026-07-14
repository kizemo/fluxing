# Fluxing UI Family — Design Philosophy

> **Movement**: *Liquid Discipline*
> **Date**: 2026-07-14
> **Author**: Claude (v0.19.0.28 design pass)
> **Applies to**: QuickPanelDialog, PhrasesDialog v2, UserDictionary, ShortcutSettings, and all future modal-class UI components of the Fluxing / 火流猩输入法 family.

---

## 1. Manifest

Fluxing is a RIME-derived input method; its UI does not perform. It **serves**. Every modal sits on the desktop like a sheet of glass held up to the screen — light passes through, the room behind remains visible. The user is doing something else; our job is to be available, not to be looked at.

This philosophy, *Liquid Discipline*, treats restraint as the highest form of craftsmanship. A painstakingly balanced negative space. A meticulously calibrated hairline. A corner radius so deliberate it becomes invisible. A typeface that whispers because the loudest thing in the room is whatever the user is writing.

We reject the AI-default of saturated accents, drop shadows used as crutches, and icons that shout. The brand orange (#FF5F31) appears in 1.4% of every modal's pixels — only where it earns its place by being the answer to a question the user already asked. The remaining 98.6% is a cool, glassy white-grey that lets content breathe.

---

## 2. Form and Space

Form follows what the user is *about to do*, not what we want them to notice. The window is rectangular — no hero curves, no asymmetric gimmicks — but its corners are rounded with `radius.lg = 14` so the eye never trips. The radius is large enough to declare "I am a sheet, not a card," small enough to feel professional rather than playful.

Internal spacing follows an 8-pixel baseline. Buttons hug each other with `space.sm = 8`. Rows of text separate by `space.lg = 11` — enough to scan by column, not so much that list density suffers. A 1-pixel hairline border in `elevation.hairline` (≈ 8% alpha black) defines the sheet without asserting it. Inside the sheet, content groups itself by whitespace alone; we do not paint sub-regions in alternating greys.

The title bar is unadorned. No logo. No system menu. No drag-handle icon. The user knows it's a window because it floats. Drag is implemented by gesture, not pictogram.

---

## 3. Color and Material

The palette is a single gesture: a glass.

- **kBgTop** `RGB(245, 245, 248)` — the top edge of the glass, lifted slightly by the room's cool light.
- **kBgBot** `RGB(220, 222, 230)` — the bottom edge, denser, where light cannot reach.
- **kTextColor** `RGB(30, 30, 40)` — words, almost-black, never pure black; pure black is the colour of machines, not people.
- **kSelBg** `RGB(255, 235, 220)` — selection, a peach whisper over the glass; it is the only warm pixel on the sheet and is reserved exclusively for "you are about to do this."
- **kAccent** `RGB(255, 95, 49)` — Fluxing orange; never decorative, always a confirmation. Used 1.4% of the time. Used at full saturation. Used once per modal.

The gradient from kBgTop to kBgBot is `per-pixel alpha 220 → 80` (top opaque, bottom translucent), exactly as on QuickPanel. The user perceives depth without being told there is depth.

There are no drop shadows on dialogs — only on hover-states that need them. Hairlines carry the visual weight that shadows would have stolen.

---

## 4. Type and Rhythm

The typeface is **Segoe UI Variable** at 14 px body, 17 px label, 14 px button. It is the most-tested UI face on Windows; it fails to look custom, which is the highest compliment a UI face can receive.

Numbers in tables and weights are tabular figures (`tnum`); letters are proportional. This way a column of user-dict weights reads as a column, not as a shuffled hand of digits.

Labels sit above their inputs, never to the left. We respect the user's downward eye-sweep and never punish it with row-spanning headings. Placeholder text is a `kTextColor @ 50%` — present but never competing with what the user has chosen to write.

Disabled controls are not greyed; they are `kTextColor @ 35%` and otherwise unchanged. We do not announce that something is unavailable. We let the user discover it.

---

## 5. Composition and Hierarchy

Every modal answers one question. PhrasesDialog v2 answers "what do you want to type." UserDictionary answers "what is in your dictionary." ShortcutSettings answers "what key does what." The title is the question. The body is the answer. The buttons at the bottom are the *next question* — a single-line confirmation: yes, edit, no.

List density is generous. We never paginate. We never show "showing 1–50 of 247." We let the user scroll, with a trackbar that whispers rather than shouts.

Search is always present, always at the top-right of the body, never hidden behind a magnifier icon. Search results replace the full list, not filter it.

The selected row wears the peach kSelBg. Not because it is *important*, but because the user has chosen it, and chosen things deserve to be visible.

---

## 6. Craft

Every modal is the product of master-level execution. Every hairline is one pixel, never 0.5. Every border-radius is an integer. Every margin is from a 4-pixel scale. Every animation timing is a multiple of 50 ms — never a coincidence.

We do not add features because they would be interesting. We remove them because they are not essential. YAGNI is a craft discipline here: the restraint to leave a feature out is harder than the cleverness to add it.

We do not borrow visual language from the operating system. We borrow it from the physical world: paper, light, glass, the pause before speaking.

When in doubt, we choose the lighter touch.

---

## 7. Subtle Reference

*Liquid Discipline* draws its soul from the *maison de verre* — Pierre Chareau's 1932 glass house in Paris. A house that one must inhabit without touching. A house that reveals what is behind it without exposing what is in front of it. The discipline is in knowing when to stop.

Our modals are not houses; they are passages. The user passes through them on the way to their sentence.

---

## 8. Anti-patterns (rejected)

| Anti-pattern | Rejected because |
|---|---|
| Drop shadows as depth signal | We have per-pixel alpha. Shadows would compete with it. |
| Brand orange as accent on borders | Brand orange is for *answers*. Borders are *questions*. |
| Icons in table rows | The text is the data. Icons are decoration. |
| Toggle switches for "I have read this" | Disabled controls are sufficient. |
| Modal-in-modal-in-modal | One question per modal. The next question is in the body. |
| Multi-step wizards | The user is in flow. Wizards break flow. |
| Animated transitions | A sheet that moves is a sheet that interrupted you. |
| Headings in title bar | The title bar holds the question. Body holds the answer. |