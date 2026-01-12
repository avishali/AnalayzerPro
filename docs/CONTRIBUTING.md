# Contributing to AnalyzerPro / MelechDSP

Thank you for contributing to this project.

AnalyzerPro is a **real-time audio application** with strict DSP, performance, and architectural constraints.  
Contributions are welcome, but **changes must follow the rules below** to protect stability and longevity.

If you plan to use Cursor or any AI coding assistant, **this document is mandatory reading**.

---

## Core Principles

This project prioritizes:

- Real-time (RT) safety
- Clear separation between Engine and UI
- Minimal, reviewable diffs
- Long-term maintainability over speed

If a change violates any of these, it will not be accepted.

---

## The PROMPTS System (Required)

All non-trivial changes **must be driven by a prompt** located in the `PROMPTS/` folder.

### Why prompts are required
- Prevents uncontrolled refactors
- Enforces scope discipline
- Makes intent explicit
- Allows safe AI-assisted development

> **Rule:** ONE prompt = ONE task

Do not bundle unrelated fixes into a single change.

---

## Before You Start

1. Read:
   - `PROMPTS/README.md`
   - `PROMPTS/PROMPTS_INDEX.md`

2. Identify whether a prompt already exists for your task.
   - If yes, use it.
   - If no, create a new prompt using a template from `PROMPTS/TEMPLATES/`.

3. If using Cursor or another agent:
   - Paste `PROMPTS/00_SYSTEM_RULES.txt` first.
   - Then paste **one task prompt only**.

---

## Subsystem Responsibilities (Non-Negotiable)

### Engine / DSP
- AnalyzerEngine and DSP code must be **RT-safe**
- No allocations, resizes, or object creation on the audio thread
- No UI logic, visualization, or dB mapping

### UI / Visualization
- Owns all:
  - dB range mapping
  - grid layout
  - animation
  - peak/FFT visual behavior
- Must NOT modify engine buffers or DSP state

### Parameters (APVTS)
- APVTS is the single source of truth
- Cache raw parameter pointers when appropriate
- Avoid per-block or per-frame parameter churn

---

## What NOT to Do

The following will cause a PR to be rejected:

- Redefining classes inside `.cpp` files
- Rewriting entire files without explicit instruction
- Inventing new architecture, managers, or abstractions
- Mixing UI and engine responsibilities
- Making “cleanup” refactors unrelated to the task
- Introducing RT-unsafe behavior
- Applying multiple prompts at once

If a task feels large:
> Split it into phases and use multiple prompts.

---

## Using AI / Cursor (Allowed, Controlled)

AI tools are allowed **only when used through the PROMPTS system**.

When using an agent:
- Do not let it “improve” unrelated code
- Do not accept changes you do not fully understand
- Review diffs manually before committing

If the agent output violates a prompt:
> Reject it and retry with a tighter scope.

---

## After Completing a Prompt

Once a prompt has been successfully applied:

1. Move it to:
   - `PROMPTS/99_ARCHIVE/`
   **or**
   - Rename it with a `DONE_` prefix

2. Ensure:
   - The build passes
   - No new warnings were introduced
   - Behavior matches the prompt’s acceptance criteria

---

## Commit & PR Guidelines

- Commits should map cleanly to a single prompt
- Reference the prompt file in the commit message, e.g.: