# Cursor Implementer Prompt — AAX Resize Lock + Size Presets (AnalyzerPro)

> Scoped from a codebase audit (2026-06-03). Read the **Context** block first — most of the
> original "optimization" tasks are already implemented in the HQ render submodule. Do NOT
> rebuild them. Your real work is in the **AnalyzerPro repo only**.

---

## Context — read before touching anything

The analyzer paint surface (grid, traces, scopes, FFT) is **not** in this repo. It lives in the
git submodule `third_party/melechdsp-hq` (`mdsp_ui` + `mdsp_gui`). That submodule already
implements, and you must NOT duplicate or modify it in this task:

- **Static-layer caching** — `BackgroundGridCache` + `RTADisplayCacheCoordinator`. Background/
  grid/labels are cached as an image and rebuilt **only** on viewMode / freq-range / dB-range /
  gain / theme / plot-rect changes (see `RTADisplayInvalidationPolicy`). This is correct and complete.
- **Paint-timing diagnostics** — `RTADisplay::setPaintTimingCallback(float paintMs)` already feeds
  `AnalyzerDisplayView::accumulateDiagnosticsAndMaybeHud()`, which emits per-second `paint/s`,
  `paint_ms(last)`, `render_fps`, `render_late_cnt/s`, format name and scale in debug builds.
- **Per-surface repaint** — there is no full-editor repaint in animation paths. The analyzer,
  meters and scopes are independent child components that each repaint themselves. Do NOT
  "fix" this; do NOT add `repaint(bounds)` plumbing to the analyzer.

Do **not** edit anything under `third_party/melechdsp-hq/`. If you believe a change is needed
there, STOP and leave a note instead — it is shared by every Melech-DSP plugin and needs its own PR.

**No DSP / audio-thread changes. No visual/design changes across any format.**

---

## Task 1 — Disable free live resize in AAX; add fixed size presets (PRIMARY)

File: `Source/PluginEditor.cpp` (constructor + `resized()`), `Source/PluginEditor.h`.
Currently the constructor does, unconditionally:

```cpp
setResizable (true, true);
setResizeLimits (1100, 720, 4096, 4096);
```

Requirements:

1. **AAX builds only** (`#if JucePlugin_Build_AAX`): disable corner-drag continuous resize
   — `setResizable(false, false)` and do not install a `ResizableCornerComponent`.
   **VST3 / AU / Standalone: behavior must stay exactly as today** (keep the current
   `setResizable(true,true)` + `setResizeLimits` path unchanged).
2. Add three **discrete** size presets: **100% / 125% / 150%**, based off the current base
   size (min 1100×720 is 100%). Switching a preset calls `setSize()` to the computed dimensions
   — a single discrete step, never a drag. Clamp to display work-area so 150% can't exceed screen.
3. Presets apply in **all formats** (they're a nice-to-have for VST3/Standalone too), but in AAX
   they are the *only* way to resize. Expose them via a small menu/buttons consistent with the
   existing `HeaderBar` style (see `Source/ui/layout/HeaderBar.cpp` for the button idiom).
4. Persist the selected preset alongside the existing editor-size persistence
   (`audioProcessor.setEditorSize(...)` / `getEditorWidth/Height()` in `PluginProcessor`). Add a
   parallel `setEditorSizePreset/getEditorSizePreset` if a clean spot exists; otherwise derive the
   preset from the restored size. Don't break existing state restore (`PluginEditor.cpp` ~line 47).

## Task 2 — Centralize UI update rates; AAX analyzer = 24 Hz (SECONDARY)

Today rates are scattered: `kAnalyzerDisplayTimerHz=30` and `ANALYZERPRO_MAX_RENDER_HZ` (default 60)
in `Source/ui/analyzer/AnalyzerDisplayView.cpp`; `startTimerHz(30)` in `MainView.cpp`,
`MeterGroupComponent.cpp`, `StereoScopeView.cpp`; `15` in loudness/label components.

1. Create **one** config header, e.g. `Source/config/UiRates.h`, with named constants:
   analyzer data rate, analyzer max render rate, meter rate, scope rate.
2. Provide AAX-specific values via `#if JucePlugin_Build_AAX`: **analyzer render cap = 24 Hz**.
   Keep VST3/AU/Standalone at current values. Meter/scope rates may be separately configurable
   in the same header (default them to today's values — do not change non-AAX behavior).
3. Replace the scattered literals with the new constants. Keep the existing
   `ANALYZERPRO_MAX_RENDER_HZ` CMake override working (the header should respect it if defined).
4. Do **not** alter the experimental `ANALYZERPRO_AAX_USE_VBLANK_UI_TICK` path's semantics — just
   source its numbers from the new header.

## Task 3 — Top up diagnostics with the two missing fields (SECONDARY)

In `AnalyzerDisplayView::accumulateDiagnosticsAndMaybeHud()` (the HUD line ~`AnalyzerDisplayView.cpp:976`),
add to the existing debug line:

1. **selected size preset** (100/125/150) — from Task 1.
2. **repaint-requests/sec** — count `repaint()` requests separately from actual `paint/s` (which
   already exists). A simple message-thread counter incremented where the view requests repaints.

Keep it debug-only. Don't add a parallel diagnostics system — extend the existing one.

---

## Out of scope (note only, do not implement here)

- Two per-frame `juce::Path` copies in `RTADisplayRenderer::paint` (`getCachedFftPath()` copied to
  close for fill). Minor, lives in the HQ submodule, already cheap because path *construction* is
  cached. Leave a one-line note; it gets its own HQ PR if pursued.

## Acceptance

- AAX: no free corner-drag resize; 100/125/150 presets work as discrete steps; visuals identical
  to VST3/Standalone; analyzer/meters/scopes still animate.
- VST3 / AU / Standalone: resize and appearance **unchanged**.
- One UI-rates config header; AAX analyzer render capped at 24 Hz; non-AAX rates unchanged.
- Debug HUD shows size preset + repaint-requests/sec alongside existing paint metrics.
- No edits under `third_party/melechdsp-hq/`. No DSP/audio-thread changes.
- Builds clean: `cmake --build build-debug --target AnalyzerPro_Standalone` (and VST3/AAX targets).
```
