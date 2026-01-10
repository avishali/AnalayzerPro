# MELECHDSP — PRE-COMMIT CHECKLIST (UI / DESIGN SYSTEM)
**Version**: v1  
**Applies to**: AnalyzerPro + all future plugins using mdsp_ui  
**Purpose**: Enforce RUNBOOK_INDEX.txt and prevent UI entropy

---

## REQUIRED BEFORE COMMIT

### [ ] 1) RUNBOOK COMPLIANCE
- [ ] I used the correct runbook for this change.
- [ ] I did NOT mix multiple runbooks in one session.
- [ ] I respected every STOP marker.
- [ ] If this commit is a follow-up step, it is clearly labeled in the commit message.

**Evidence to include in PR/commit notes:**
- Runbook name:
- Step number(s):

---

### [ ] 2) SCOPE SAFETY (UI-ONLY)
- [ ] No DSP changes.
- [ ] No engine logic changes.
- [ ] No parameter definition changes.
- [ ] No threading/timer changes.
- [ ] No performance-critical algorithm changes.

**If ANY of the above changed:**
- STOP and split into a non-UI commit (do not merge).

---

### [ ] 3) DESIGN TOKENS ENFORCEMENT (NO NEW MAGIC VALUES)
- [ ] UI components do NOT instantiate `mdsp_ui::Theme` locally.
  - Must use UiContext: `ui_.theme()`
- [ ] Layout constants (padding/gap/heights/radii) are not hardcoded.
  - Must use UiContext: `ui_.metrics()`
- [ ] Font sizes are not hardcoded.
  - Must use UiContext: `ui_.type()`

**Allowed exception (temporary):**
- A single local constant is allowed ONLY if:
  - (a) it is internal to a component
  - (b) it is not a design token
  - (c) it is documented with: `// TEMP: local constant (move to Metrics if reused)`

---

### [ ] 4) SINGLE SOURCE OF TRUTH FOR WINDOW SIZE
- [ ] Only `PluginEditor` owns window size and resize limits.
- [ ] No `setSize()` calls inside `MainView` or child views.
- [ ] `MainView` and children must lay out using `getLocalBounds()` only.

---

### [ ] 5) UiContext LIFETIME AND INJECTION
- [ ] Exactly one `UiContext` instance is owned by `PluginEditor` (or top-level host).
- [ ] All views/components receive `UiContext` by reference.
- [ ] No `Theme`/`Metrics` copies stored in components (reference access only).

---

### [ ] 6) VISUAL STABILITY (NO UNINTENDED UI CHANGE)
- [ ] I verified the UI looks identical (or intended changes are explicitly listed).
- [ ] I checked:
  - [ ] Header alignment
  - [ ] ControlRail spacing
  - [ ] Meter rail widths
  - [ ] Analyzer plot bounds/padding
  - [ ] Footer spacing
- [ ] Any intentional visual change is documented:
  - What changed:
  - Why:
  - Before/after screenshot (if applicable)

---

### [ ] 7) DEBUG BEHAVIOR
- [ ] Debug overlays remain `JUCE_DEBUG`-only (or equivalent).
- [ ] No debug drawing leaks into Release builds.
- [ ] No debug logging added in `paint()` or high-frequency UI code paths.

---

### [ ] 8) PERFORMANCE HYGIENE (UI PATHS)
- [ ] No allocations added in `paint()` or `resized()`.
- [ ] No repeated expensive operations in `paint()` (e.g., new `Path`s each frame without caching).
- [ ] No timers added unless explicitly required and justified.

---

### [ ] 9) FILE HYGIENE
- [ ] No large refactors unrelated to the stated UI change.
- [ ] No formatting-only edits.
- [ ] No dead code added.
- [ ] New files (if any) match existing project conventions.

---

### [ ] 10) BUILD + BASIC SMOKE CHECK
- [ ] Build succeeds (Debug).
- [ ] Build succeeds (Release) OR not applicable and documented.
- [ ] Plugin loads in at least one host (or the existing smoke host).
- [ ] Resizing works (if resizable) without layout collapse.

---

### [ ] 11) THEME VARIANT READINESS (IF TOUCHING mdsp_ui TOKENS)
- [ ] Dark theme remains correct.
- [ ] Light palette is not broken (even if stubbed).
- [ ] No UI component assumes dark-only conditions.

---

### [ ] 12) COMMIT MESSAGE STANDARD (RECOMMENDED)
- [ ] Commit message includes:
  - `[UI]` prefix
  - Runbook name + step(s)
  - Short intent

**Example:**
```
[UI] ANALYZERPRO_UI_V1 Step 1 — UiContext injection for Header/Footer/Rail
```

---

## FAIL CONDITIONS (DO NOT COMMIT)

- ❌ UI introduces new magic numbers across multiple files without using Metrics/Theme/Type.
- ❌ Components construct Theme locally.
- ❌ Views call `setSize()` (except PluginEditor).
- ❌ DSP/engine/threading changes mixed with UI changes.
- ❌ Visual output changed unintentionally and not documented.

---

## END
