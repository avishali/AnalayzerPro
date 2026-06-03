# Verifier Prompt — AAX Resize Lock + Size Presets (AnalyzerPro)

> Companion to `cursor_aax_ui_stability_prompt.md`. Verifies that change WITHOUT assuming the
> original (wrong) scope. Most "optimization" items were already shipped in the HQ submodule —
> here you confirm they are *untouched*, not that they were added.

## Ground rules for the reviewer

- The analyzer/scope/grid render code lives in `third_party/melechdsp-hq/` (`mdsp_ui` + `mdsp_gui`).
  Caching, invalidation policy, and paint-timing diagnostics already existed there before this change.
- A correct change touches **only the AnalyzerPro repo** (`Source/...`, `CMakeLists.txt`). Any diff
  under `third_party/melechdsp-hq/` is a **FAIL** for this task unless explicitly agreed.

## A. Code inspection (do this first — cheap, catches scope errors)

1. `git status` / diff: confirm **no changes under `third_party/melechdsp-hq/`**.
2. Confirm **no DSP/audio-thread changes** (nothing in `Source/dsp_adapters`, `Source/analyzer/AnalyzerEngine*`,
   `PluginProcessor` audio path; editor-size persistence on the processor is fine).
3. `Source/PluginEditor.cpp`: AAX path uses `setResizable(false,false)` and no corner resizer;
   non-AAX path still `setResizable(true,true)` + `setResizeLimits(...)` as before.
4. Size presets are **discrete `setSize()` steps** (100/125/150), clamped to screen work-area —
   not a continuous drag. State restore (existing `getEditorWidth/Height`) still works.
5. One UI-rates config header exists (e.g. `Source/config/UiRates.h`); scattered `startTimerHz`/
   `kAnalyzerDisplayTimerHz`/`ANALYZERPRO_MAX_RENDER_HZ` literals now source from it; AAX analyzer
   render cap = 24 Hz via `#if JucePlugin_Build_AAX`; non-AAX values unchanged; CMake
   `ANALYZERPRO_MAX_RENDER_HZ` override still honored.
6. Diagnostics: existing HUD line in `AnalyzerDisplayView::accumulateDiagnosticsAndMaybeHud()` now
   also prints **size preset** and **repaint-requests/sec**, alongside the pre-existing `paint/s`,
   `paint_ms`, `render_fps`, format, scale. Existing diagnostics not removed or duplicated.
7. Confirm NOT done (these were already correct — flag if the change "added" them):
   - No new background/grid/label caching in `Source/` (it belongs to the submodule).
   - No new dirty-region repaint plumbing on the analyzer view.

## B. Build

```
cmake --build build-debug --target AnalyzerPro_Standalone
cmake --build build-debug --target AnalyzerPro_VST3
cmake --build build-debug --target AnalyzerPro_AAX
```
All three must build clean.

## C. Runtime

1. **Standalone**: resize behavior unchanged (free drag works); appearance unchanged; presets
   100/125/150 resize discretely.
2. **VST3** (in any host): resize behavior unchanged; appearance unchanged.
3. **AAX in Pro Tools**:
   - No free corner-drag resize.
   - 100/125/150 presets present and each is a single discrete, stable resize.
   - Visual design (grid, glow, traces, meters, scopes, colors, typography, layout) matches
     VST3/Standalone — not simplified.
   - Analyzer, meters, scopes still animate.
4. Debug logs (AAX): line shows `format=AAX`-equivalent name, `paint/s`, `paint_ms`,
   `repaint-requests/sec`, size preset.

## Pass condition

AAX has no free live resize and has working discrete 100/125/150 presets; VST3/AU/Standalone are
behaviorally and visually unchanged; AAX looks identical to other formats (not cheaper); the diff is
confined to the AnalyzerPro repo with no DSP changes.

## Note (not a failure for this task)

Reduced AAX *jitter* is a goal but may be host-side: Pro Tools/macOS is known to batch/delay
`repaint()` → `paint()` on AAX. If jitter persists, the diagnostics (paint/s vs repaint-requests/s)
should make host-side batching visible. That investigation, plus the two per-frame `Path` copies in
the HQ `RTADisplayRenderer::paint`, are separate follow-ups — not part of this verification.
