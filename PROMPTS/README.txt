# PROMPTS Index — AnalyzerPro / MelechDSP

This index maps product features and maintenance themes to the corresponding Cursor prompt files under `PROMPTS/`.

Conventions:
- Apply prompts one at a time.
- Engine prompts must remain RT-safe.
- UI prompts must not modify DSP/Engine state.

---

## Audio I/O and Standalone Routing (macOS)

### Standalone loopback / aggregate routing
- `01_ENGINE/` (if engine/device layer changes are needed)
- `03_UI_CONTROLS/` (if UI selects devices)
- (Project-specific) add prompt(s) here when stabilized:
  - `01_ENGINE/00_macos_loopback_routing_standalone.txt` (recommended to create)

---

## FFT Pipeline

### Fix FFT display sizing / bin count contract
- `02_UI_ANALYZER/01_fft_bin_count_contract.txt`
- `02_UI_ANALYZER/02_fft_display_fix_use_fftBinCount.txt`

### RT-safe FFT resize (no allocations on audio thread)
- `01_ENGINE/01_rt_safe_fft_resize.txt`
- `01_ENGINE/02_deferred_fft_request_apply.txt`

### Processor wiring (FFT size param → requestFftSize)
- `01_ENGINE/02_deferred_fft_request_apply.txt`
- (Optional) create:
  - `01_ENGINE/06_processor_fft_param_request_only.txt`

---

## Peak / Hold System

### Reset Peaks (engine-level)
- `01_ENGINE/04_reset_peaks_engine_only.txt`

### Reset Peaks (UI button + shortcut + visual feedback)
- `03_UI_CONTROLS/01_headerbar_reset_peaks_button.txt`
- `03_UI_CONTROLS/02_reset_peaks_keyboard_shortcut.txt`
- `02_UI_ANALYZER/05_peak_flash_feedback_ui_only.txt`

### Peak mode model cleanup (Freeze vs Hold mode separation)
- `01_ENGINE/03_peak_hold_model_cleanup.txt`

### Peak decay models (dB/sec vs time constant)
- `01_ENGINE/05_peak_decay_models_db_vs_tc.txt`

---

## dB Range and Grid

### User-selectable dB range (-60 / -90 / -120)
- `02_UI_ANALYZER/03_db_range_user_selectable.txt`

### Persist dB range via APVTS (session recall)
- `03_UI_CONTROLS/03_db_range_persistence_apvts.txt`

### Animated grid transitions
- `02_UI_ANALYZER/` (recommended prompt to add if not yet created)
  - `02_UI_ANALYZER/07_animate_db_range_transitions.txt` (recommended to create)

### Independent FFT vs Peak ranges (advanced)
- `02_UI_ANALYZER/04_independent_fft_vs_peak_range.txt`

---

## UI Correctness and Guardrails

### Extract “legacy peak invalid sentinel” constant (no behavior change)
- `02_UI_ANALYZER/06_extract_peak_sentinel_constant.txt`

### Layout polish and clipping fixes (HeaderBar / prompt box / controls)
- `03_UI_CONTROLS/04_headerbar_layout_polish.txt`

---

## Performance and Churn Reduction

### Apply analyzer params only once (avoid per-block setter spam)
- `04_PERFORMANCE/01_apply_analyzer_params_once.txt`

### Cache APVTS raw pointers (avoid getRawParameterValue lookups)
- `04_PERFORMANCE/02_cache_apvts_raw_pointers.txt`

### Guard against null in Release (no jassert-only safety)
- `04_PERFORMANCE/03_guard_null_params_release.txt`

### Reduce processBlock churn (only set when changed)
- `04_PERFORMANCE/04_reduce_processBlock_churn.txt`

---

## Refactor Guards (defensive prompts)

### No RT allocations rules (keep agents honest)
- `05_REFACTOR_GUARDS/01_no_rt_allocations_rules.txt`

### Prevent class redefinition / file rewrite patterns
- `05_REFACTOR_GUARDS/02_no_cpp_class_redefinition.txt`

### UI owns mapping, engine owns math (layering discipline)
- `05_REFACTOR_GUARDS/03_ui_owns_mapping_engine_owns_math.txt`

---

## Templates (blank prompt skeletons)

- `TEMPLATES/TEMPLATE_ENGINE_TASK.txt`
- `TEMPLATES/TEMPLATE_UI_ANALYZER_TASK.txt`
- `TEMPLATES/TEMPLATE_UI_CONTROLS_TASK.txt`
- `TEMPLATES/TEMPLATE_PERFORMANCE_TASK.txt`
- `TEMPLATES/TEMPLATE_REFACTOR_GUARD.txt`
- `TEMPLATES/TEMPLATE_MULTI_STEP_FEATURE.txt`

---

## Archive

Applied or deprecated prompts belong in:
- `99_ARCHIVE/`

Recommendation:
- Prefix applied prompts with `DONE_` before moving to archive, e.g.
  - `DONE_03_db_range_user_selectable.txt`

---

# PROMPTS — How We Use Cursor Prompts in This Repo

This folder defines the **official workflow** for making changes to AnalyzerPro / MelechDSP using Cursor (or other coding agents).

The goal is **correctness, RT safety, and long-term maintainability** — not speed or “clever” refactors.

If you are contributing to this repo, **read this before running any agent**.

---

## What is a Prompt?

A prompt in this repo is:
- A **single, scoped task**
- Written in plain text
- Intended to be pasted directly into Cursor
- Designed to produce **small, reviewable diffs**

**Rule of thumb**  
> ONE prompt = ONE change

If a task feels large, it must be split into phases.

---

## Folder Structure Overview

See `PROMPTS_INDEX.md` for a full feature → prompt mapping.

High-level meaning:

- `01_ENGINE/`  
  DSP, AnalyzerEngine, RT-critical logic  
  **RT safety is mandatory**

- `02_UI_ANALYZER/`  
  AnalyzerDisplayView, FFT/Peak visualization, grid, animation  
  **UI only — no DSP**

- `03_UI_CONTROLS/`  
  HeaderBar, buttons, combos, keyboard shortcuts

- `04_PERFORMANCE/`  
  Churn reduction, caching, micro-optimizations  
  **No behavior changes**

- `05_REFACTOR_GUARDS/`  
  Defensive prompts to stop agents from doing unsafe rewrites

- `TEMPLATES/`  
  Blank task skeletons for new prompts

- `99_ARCHIVE/`  
  Applied or deprecated prompts

---

## Required Workflow (Do Not Skip)

### 1. Choose the right prompt
Find the feature in `PROMPTS_INDEX.md` and select the appropriate prompt file.

If none exists:
- Copy a template from `TEMPLATES/`
- Create a new prompt with **explicit scope**

---

### 2. Paste system rules first (recommended)
Before any task prompt, paste:

- `00_SYSTEM_RULES.txt`

This prevents Cursor from:
- Rewriting files
- Breaking RT safety
- Inventing architecture

---

### 3. Paste ONE task prompt
Do not combine prompts.
Do not “just also fix X”.

If something feels missing:
- Stop
- Create a follow-up prompt

---

### 4. Review the diff manually
Before accepting changes, verify:

- Only listed files were modified
- No unexpected refactors
- No RT allocations were introduced
- No UI ↔ Engine responsibility leaks

---

### 5. Archive the prompt
Once applied successfully:

- Move it to `99_ARCHIVE/`
- Or rename it with `DONE_` prefix

Example: