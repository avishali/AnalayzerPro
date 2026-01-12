# SOP — AI-Assisted Engineering Workflow (Antigravity)

## Purpose

This repository uses a **strict, file-driven, multi-agent workflow** for all AI-assisted development.

The goals are:
- Zero architectural drift
- Deterministic, auditable changes
- Safe use of AI in a JUCE + CMake + hardware-adjacent codebase
- Clear separation of intent, implementation, and verification

This SOP is **mandatory** for all non-trivial changes.

---

## Core Principles

1. **Files are the source of truth**
   - Not chat logs
   - Not UI state
   - Not agent memory

2. **Architecture and implementation are never mixed**
3. **Only one role is active at a time**
4. **Every mission has an explicit STOP**
5. **Verification is independent of implementation**
6. **CI enforces compliance**

---

## Tooling Model

### Primary Tools
- **ChatGPT** — Architecture, intent, review, governance
- **Antigravity** — Execution engine (code edits + verification)
- **GitHub Actions** — Enforcement and audit

### Environment
- **Local repository environment (macOS)**
- Native JUCE, Xcode, CMake toolchain
- Submodules required and trusted

---

## Authoritative Files

All AI-assisted work is governed by these files:

- `PROMPTS/MISSIONS/CURRENT_MISSION.txt`  
- `PROMPTS/RUNBOOKS/MULTI_AGENT_EXECUTION_V1.txt`  
- `PROMPTS/ROLE_DEFINITIONS.md`  
- `PROMPTS/MISSIONS/LAST_RESULT.md`  

If instructions conflict:
1. CURRENT_MISSION.txt
2. RUNBOOK
3. ROLE_DEFINITIONS.md
4. CI rules

(in that order)

---

## Roles (Summary)

### IMPLEMENTER
- Makes scoped code changes only
- No builds
- No verification
- Writes `IMPLEMENTER_RESULT.md`
- STOP

### VERIFIER
- No code edits
- Audits scope
- Runs build
- Validates acceptance criteria
- Writes `VERIFIER_RESULT.md` + `LAST_RESULT.md`
- STOP

### QUICK_FIX
- Trivial fixes only
- ≤ 2 files
- One build max
- Writes `LAST_RESULT.md`
- STOP if not trivial

Roles are **file-selected**, not UI-selected.

---

## Standard Workflow (Canonical)

### Phase 0 — Mission Authoring (Human / ChatGPT)

The mission author defines:
- Objective (one sentence)
- Scope (hard boundaries)
- Task breakdown (finite steps)
- Acceptance criteria
- Active role

This is written to: