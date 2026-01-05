## AI-Assisted Development Workflow

This repository uses a **strict Architecture → Implementation workflow** when working with AI tools such as ChatGPT and Cursor.

All non-trivial changes **must** follow the process defined in:

👉 **[`SOP.md`](./SOP.md)** — *ChatGPT → Cursor Engineering Workflow*

### Summary
- **ChatGPT** is used for architecture, system design, and review
- **Cursor AI** is used strictly for scoped implementation
- Architecture and implementation are never mixed
- All changes require explicit scope, constraints, and stop conditions

This process ensures predictable refactors, hardware-safe abstractions, and long-term maintainability.

If you are contributing to this repository, **read `SOP.md` before making changes**.








# SOP — ChatGPT → Cursor Engineering Workflow

## Purpose
This repository follows a strict **Architecture → Implementation** workflow to ensure:
- Predictable refactors
- Hardware-safe abstractions
- Stable JUCE + CMake builds
- AI assistance without architectural drift

This SOP is mandatory for all non-trivial changes.

---

## Roles & Responsibilities

### ChatGPT (Architect / Reviewer)
- Owns system design and architecture
- Defines interfaces, contracts, and boundaries
- Produces implementation instructions
- Reviews diffs and validates intent

### Cursor AI (Implementation Agent)
- Executes instructions exactly as given
- Modifies code only within defined scope
- Does not invent APIs or redesign systems
- Stops on ambiguity

Cursor is **not** allowed to make architectural decisions.

---

## Standard Workflow

### Phase 1 — Intent Definition
The human defines:
- Objective (what & why)
- Constraints (JUCE, CMake, real-time safe, etc.)
- Scope (files/modules affected)
- Risk tolerance (safe vs aggressive)

No code changes occur in this phase.

---

### Phase 2 — Architecture Design (ChatGPT)
ChatGPT produces:
- System boundaries
- File/folder layout
- Interfaces & contracts
- Migration plan
- Explicit non-goals

No implementation yet.

---

### Phase 3 — Cursor Handoff (Critical)
All Cursor work must be driven by a **Handoff Package** with this structure: