# Trace Colors — Phase 2 Handoff (for Cursor)

Phase 1 (already shipped) added user-customisable **spectrum trace colours**:
- `Source/ui/theme/TraceColors.{h,cpp}` — `TraceColorStore` (a `<TraceColors>` child of
  `apvts.state`, persists with session + presets), `ColorSwatch`, `LiveColourSelector`.
- 8 traces via `enum class TraceId { LR, Mono, L, R, Mid, Side, Rms, Peak }`.
- Rail UI: a colour swatch per trace in the **Traces** module of `ControlRail`, plus
  Reset / Save-Default buttons.
- Live recolour: `AnalyzerDisplayView`'s `setGetTheme` lambda reads the store into
  `theme_.seriesX` every frame.

Phase 2 has two parts. **Part A is product-only. Part B re-touches canonical
melechdsp-hq** and therefore needs the commit → re-vendor → submodule-bump cycle
(same as UI_OVERHAUL_03 — see PROMPTS/MISSIONS/UI_OVERHAUL_03_COMMIT_HANDOFF.md).

---

## Part A — Colour choice for the phase scopes and the meters (RMS + Peak)

**Product-only.** These components currently read hardcoded shared-theme colours:
- `Source/ui/meters/StereoScopeComponent.cpp` (line ~196: `ui_.theme().seriesPeak`)
- `Source/ui/meters/PhaseFanScopeComponent.cpp` (line ~180: `ui_.theme().seriesPeak`)
- `Source/ui/meters/MeterComponent.cpp` (lines ~324/333/374: `theme.seriesPeak`; the bar
  fill / peak + rms numeric readouts)

Unlike `AnalyzerDisplayView`, these use the **shared `ui_.theme()`** directly (not a local
`theme_` copy), so they can't ride the existing per-frame getTheme injection.

**Do this:**
1. Give each of the three components a `setTraceColorStore (AnalyzerPro::TraceColorStore*)`
   setter + non-owning pointer (mirror `AnalyzerDisplayView::setTraceColorStore`).
2. In their `paint()`, read colours from the store instead of `theme.seriesPeak`:
   - Meters: the **Peak** readout/bar → `store->get(TraceId::Peak)`; the **RMS** readout
     → `store->get(TraceId::Rms)`. (MeterComponent currently uses `seriesPeak` for both —
     split so RMS mode uses the RMS colour and Peak mode uses the Peak colour.)
   - Phase / stereo scopes → see decision below.
3. Wire from `MainView` where the store is created (next to the existing
   `rail_.setTraceColorStore(...)` / `analyzerView_.setTraceColorStore(...)` calls):
   `inputMeters_`, `outputMeters_`, `stereoScopeComponent_`, `phaseFanScopeComponent_`.
4. Trigger a repaint when colours change. The meters/scopes already repaint on a timer,
   so reading the store live in paint() is enough; no extra notification needed.

**DECISION (ask the owner): how should the scopes map to colours?**
- (a) Reuse `TraceId::Peak` (scopes follow the Peak colour) — zero new state, simplest.
- (b) Add **new** entries to the store for "Stereo Scope" and "Phase Scope" colours
  (extend `TraceId` + `traceColorDefs()` + swatches). More control, more UI.
Recommend (a) for a first pass unless the owner wants independent scope colours.

---

## Part B — Opacity + colour fill (like RMS) for the other traces

The user wants, per trace, in the Traces colour settings: an **opacity** control and a
**fill** toggle (the filled area under the curve, as RMS already has).

### How rendering works today (verified)
- Trace colours come from the theme via
  `mdsp_ui::rta::MultiTracePainter::traceColourFor(theme, TraceId)`
  (`melechdsp-hq/shared/mdsp_ui/src/rta/MultiTracePainter.cpp`).
- **RMS and Peak get gradient fills**, hardcoded in the painters, e.g.
  `FftModePainter.cpp` lines ~52-69 (RMS: `colRms.withAlpha(0.35f → 0.05f)` gradient,
  `fillPath` closed to the bottom) and ~82-99 (Peak). Same pattern in
  `LogModePainter.cpp` and `BandsModePainter.cpp`, and the shared helper
  `RtaPaintUtils.cpp` (~line 206+).
- **The other traces (LR/Mono/L/R/Mid/Side) are line-only**, drawn by
  `MultiTracePainter` with a per-row alpha (`traceColourFor(theme, row.id).withAlpha(row.alpha)`,
  RtaPaintUtils ~line 231) — no fill.
- The painters take per-trace show flags via `TraceConfig`
  (`melechdsp-hq/shared/mdsp_gui/include/mdsp_gui/analyzer/AnalyzerDisplayWidget.h` line ~44,
  and `RTADisplayModel::TraceConfig`). They do **not** currently receive per-trace opacity
  or fill flags — opacity is hardcoded per trace, fill is hardcoded to RMS/Peak only.

### What this requires
This is a **shared-renderer (canonical melechdsp-hq) change** plus product store/UI work.

**In canonical melechdsp-hq (then re-vendor + bump submodules):**
1. Extend `TraceConfig` (`AnalyzerDisplayWidget.h`) and `RTADisplayModel::TraceConfig`
   with per-trace `opacity` (0..1) and `fillEnabled` (bool) — e.g. small arrays indexed by
   TraceId, or per-field members matching the existing show flags.
2. Thread those through `setTraceConfig` into the painters.
3. In `MultiTracePainter` / `FftModePainter` / `LogModePainter` / `BandsModePainter`:
   - **Opacity:** multiply each trace's draw alpha by its `opacity` instead of using a
     hardcoded constant (today painters do `.withAlpha(0.95f)` etc., which *overrides*
     alpha — change to multiply by the per-trace opacity).
   - **Fill:** when `fillEnabled` for a trace, draw the same closed-path gradient fill the
     RMS path uses (factor the RMS fill block in `FftModePainter`/`RtaPaintUtils` into a
     reusable helper `drawTraceFill(g, path, colour, bottomY, alpha)` and call it for any
     trace whose fill flag is set). Note multi-traces need their closed fill path built
     (see `MultiTracePathCache` / `FftPathBuilder`).
4. Keep defaults equal to today's look (RMS+Peak fill on, others off; current alphas).

**In the product (AnalyzerPro):**
5. Extend `TraceColorStore` to store, per trace, alongside the colour: `opacity` (float)
   and `fillEnabled` (bool) — extra properties on each trace's node in the `<TraceColors>`
   child (persists with session/presets automatically). Add getters/setters + defaults.
6. Build the renderer `TraceConfig` from the store (where the product currently calls
   `setTraceConfig` / builds the analyzer render config) so opacity + fill reach the painters.
7. Rail UI (Traces module, `ControlRail`): beside each trace's existing swatch, add an
   **opacity slider** (or draggable value) and a small **fill toggle**. Reuse the
   `placeTraceRow` layout; reserve a bit more row width/height. Apply changes live via the
   store (display already reads it per frame).

### Persistence
Opacity + fill live in the same `<TraceColors>` apvts.state child as colours, so they
persist with the DAW session and inside presets for free. Include them in
`saveAsUserDefault()` / `resetToDefaults()` / the user-default XML.

---

## Propagation reminder (Part B only)
Part B edits canonical `melechdsp-hq`. After it builds green:
1. Commit + push canonical HQ (`git@github.com:avishali/melechdsp-hq.git`, branch `master`).
2. Bump `third_party/melechdsp-hq` submodule SHA in AnalyzerPro (and MasterLimiter if it
   should track the same HQ — the renderer change is analyzer-only, so MasterLimiter may
   not need it; bump only if you want both on the same HQ commit).
3. Verify a default-path AnalyzerPro build (no `-DMELECHDSP_HQ_ROOT`) picks up the change.
   Dev builds against canonical use `-DMELECHDSP_HQ_ROOT=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq`
   and `JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE` (needed whenever CMakeLists changes
   trigger a reconfigure).

## Suggested sequencing
Ship **Part A first** (product-only, low risk, immediately visible), then **Part B**
(shared renderer) as its own change with the HQ commit/bump cycle.

## Done criteria
- Phase scopes + meters (RMS and Peak) follow user-chosen colours.
- Each trace has an opacity control and a fill toggle in the Traces settings; fill matches
  the RMS gradient style; defaults reproduce the current look.
- Colours + opacity + fill persist with session, presets, and Save-Default; Reset restores
  defaults. Canonical HQ committed and submodule bumped; default-path build green.
