# EXTRACTION_MAP.md — AnalyzerPro ➜ melechdsp-hq (Option B)

**Goal:** Extract reusable components from AnalyzerPro into the shared repo (`melechdsp-hq`) while keeping AnalyzerPro shippable at every step.

**Namespace / module style:** **Option B**
- `mdsp_core`
- `mdsp_dsp`
- `mdsp_ui`

---

## 0) Principles (Non‑Negotiables)

1. **No big‑bang rewrite.** Move in slices; every slice builds + passes smoke test.
2. **Hard boundary:** DSP/logic must not depend on UI/paint.
3. **Product layer stays thin:** APVTS, parameter IDs, layout decisions remain in AnalyzerPro.
4. **Short classes:** one reason to change per class (Engine / Snapshot / Renderer / Controller / Binder).
5. **No circular deps:**
   - `mdsp_core` has **no** dependency on `mdsp_dsp` / `mdsp_ui`
   - `mdsp_dsp` may depend on `mdsp_core`
   - `mdsp_ui` may depend on `mdsp_core` (and only depends on `mdsp_dsp` if explicitly justified)

---

## 1) Ownership Map (What lives where)

### 1.1 Stays in AnalyzerPro (Product‑Specific)
These remain in AnalyzerPro **now** (may be revisited later as `mdsp_plugin`, but not in the initial extraction).

**Plugin integration**
- `Source/PluginProcessor.*`
- `Source/PluginEditor.*`

**Parameters / Presets / Control glue**
- `Source/parameters/`
- `Source/presets/`
- `Source/control/`

**Host / hardware integration**
- `Source/audio/`
- `Source/hardware/`

**Product UI layout**
- Anything that is purely “AnalyzerPro layout/arrangement/branding”

---

### 1.2 Moves to `melechdsp-hq/shared/mdsp_core` (Core utilities)
Move code that is broadly reusable and not DSP‑specific and not UI‑specific:

Typical candidates in AnalyzerPro:
- clamp/limits helpers, safe float checks, denormals helpers
- small timing helpers
- invariant/assert helpers
- generic containers, small string/path helpers

**Destination**
- `melechdsp-hq/shared/mdsp_core/include/mdsp_core/...`
- `melechdsp-hq/shared/mdsp_core/src/...`

---

### 1.3 Moves to `melechdsp-hq/shared/mdsp_dsp` (DSP + analysis engines)
Move any code that processes samples or computes analyzer state:

**Analyzer core**
- `Source/analyzer/AnalyzerEngine.*` → `shared/mdsp_dsp/analyzer/`
- `Source/analyzer/AnalyzerSnapshot.*` → `shared/mdsp_dsp/analyzer/`

**Loudness**
- `Source/loudness/*` → `shared/mdsp_dsp/loudness/`

**Scope / meter signal processors**
- Any *signal processing* (not painting) currently embedded in UI classes:
  - candidate destination: `shared/mdsp_dsp/scopes/`

**Ballistics / smoothing**
- Ensure AnalyzerPro uses the shared implementations:
  - `shared/mdsp_dsp/MeterBallistics`
  - `shared/mdsp_dsp/Smoother`

**Hard rule**
- `mdsp_dsp` must not include JUCE UI headers (no `juce_gui_basics`, no `Graphics`).
  - JUCE `juce_audio_basics` / `juce_dsp` are fine.

---

### 1.4 Moves to `melechdsp-hq/shared/mdsp_ui` (UI components + renderers + controllers)
Move any code that paints or handles interaction and is reusable:

**Renderers**
- FFT plot renderer(s)
- grid/axis renderer(s)
- scope renderer(s)
- meters renderers
- hover/crosshair overlays

**Controllers**
- mouse drag/scroll controllers
- zoom/pan controllers
- hover smoothing controller (UI‑side only)

**Destination**
- `shared/mdsp_ui/renderers/`
- `shared/mdsp_ui/controllers/`
- `shared/mdsp_ui/components/` (reusable widgets)

**Contract**
UI takes:
- snapshot/model data (immutable view)
- viewport bounds
- theme

UI must not:
- compute FFT / loudness / ballistics (those belong to `mdsp_dsp`)

---

## 2) Extraction Order (CRITICAL)

### Phase 1 — Easy wins (foundation + workflow)
**Objective:** prove we can move code into HQ and consume it cleanly.

1. Extract small utilities → `mdsp_core`
2. Unify AnalyzerPro to use shared:
   - `mdsp_dsp::MeterBallistics`
   - `mdsp_dsp::Smoother`
3. Add/confirm smoke test checklist and keep it updated.

**Exit criteria**
- AnalyzerPro builds
- No behavior change visible
- Smoke test passes

---

### Phase 2 — DSP Foundation (move engines + snapshots)
**Objective:** AnalyzerPro DSP is now mostly shared.

1. Move `AnalyzerSnapshot` → `mdsp_dsp/analyzer`
2. Move `AnalyzerEngine` → `mdsp_dsp/analyzer`
3. Move loudness modules → `mdsp_dsp/loudness`
4. Any scope/trace processors → `mdsp_dsp/scopes`

**Exit criteria**
- AnalyzerPro builds
- Analyzer output matches baseline (within expected tolerance)
- Smoke test passes

---

### Phase 3 — UI modularization (Renderer / Controller / Model split)
**Objective:** UI becomes composable and reusable.

1. Identify large UI classes (e.g. analyzer display view) and split into:
   - `*Model` (UI state, derived UI metrics)
   - `*Renderer` (paint only)
   - `*Controller` (interaction only)
2. Move reusable renderers/controllers/components to `mdsp_ui`
3. Keep AnalyzerPro layout composition in product repo.

**Exit criteria**
- UI behavior matches baseline
- Less coupling; shorter classes
- Smoke test passes

---

### Phase 4 — Cleanup & API stabilization
1. Remove duplication
2. Tighten includes + reduce compile dependencies
3. Document APIs and add minimal unit tests (optional, targeted)

---

## 3) Slice Workflow (How each extraction is executed)

For each slice (e.g., “AnalyzerSnapshot move”):

1. Create destination folder in HQ under the correct module.
2. Move code with minimal edits (don’t refactor yet).
3. Create a clean public header API in `include/`.
4. Wire HQ CMake target and export include dirs.
5. Update AnalyzerPro to include from HQ, remove local copy.
6. Build + run smoke test.
7. Then refactor to shorten classes (split responsibilities).

---

## 4) Class Shortening Rules (Applied during/after moves)

**If a class contains both DSP and UI paint → split immediately.**

Target patterns:
- `Engine` (DSP) — prepare/process/publish snapshot
- `Config` — validated config struct
- `Snapshot` — plain data container
- `Renderer` — paint only
- `Controller` — mouse/keyboard only
- `Binder/Adapter` — connects APVTS ↔ Config/Model

---

## 5) Immediate Next Slice (Kickoff)

**Phase 1 / Slice A**
- Make AnalyzerPro use **only** shared `mdsp_dsp::MeterBallistics` and `mdsp_dsp::Smoother`
- Remove duplicate/parallel implementations in AnalyzerPro if present
- Confirm no UI dependencies leaked into `mdsp_dsp`

---

## 6) Appendix — Optional future module

If we later find strong reuse across products for:
- APVTS binding
- parameter metadata/IDs
- preset serialization
- A/B comparison
then create:
- `melechdsp-hq/shared/mdsp_plugin`

Not part of initial extraction.

---
