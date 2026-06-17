# Cursor task — restore crosshair + selectable trace-legend squares in Metal mode

These existed in the CPU renderer and were lost in the Metal port. The machinery still exists — we're
RE-ENABLING it for the Metal/chrome path, not building new. Render both in the CPU **chrome** layer
(user accepted ~10fps chrome-rate for the crosshair; smooth Metal port is a later follow-up).

Spans the shared SDK (`third_party/melechdsp-hq/shared/mdsp_ui` — build uses the sibling
`../melechdsp-hq` via `MELECHDSP_HQ_ROOT`) and AnalyzerPro. SDK is shared by other plugins — keep the
behaviour identical for the non-suppressed (non-Metal) case (no double-draw, no regressions).

## Background (already in code)
- Trace legend: `RTADisplayRenderer::paintFFTMode` builds `legendItems` (`appendLegend` ~line 353) and
  draws them via `LegendRenderer`. `LegendItem` has a `crosshairSelected` flag; clicking a legend item
  selects which trace the crosshair reads — `FftHoverController::setCrosshairTrace` /
  `RTADisplayController::setCrosshairTrace`. BUT the legend is built INSIDE `paintFFTMode`, which is
  gated by `! suppressDynamicTraces` (true in Metal) → suppressed → no legend.
- Crosshair: drawn in `paintInteractionOverlays` (called unconditionally) but is currently off / not
  fed hover in Metal mode.

## 1. Legend must render even when dynamic traces are suppressed (SDK)
In `RTADisplayRenderer.cpp`, MOVE the legend build+draw (the `legendItems`/`appendLegend` block and the
`LegendRenderer` draw) OUT of `paintFFTMode` and into `paintInteractionOverlays` (which is always
called), so it draws in BOTH suppressed (Metal) and non-suppressed (CPU) cases. Ensure it's drawn
exactly ONCE — remove it from `paintFFTMode` so non-Metal plugins don't double-draw. The legend needs
the trace-config + colours + which-traces-have-data; pass whatever it needs through to
`paintInteractionOverlays` (it already receives `traceConfig`, `model`, `theme`).

## 2. Make the legend squares clickable → select crosshair trace
The legend items already carry `crosshairSelected`. Ensure the hit-testing that maps a click on a
legend square to `setCrosshairTrace(thatTrace)` is active in Metal mode (it lives in the
controller/hover path — `FftHoverController`/`RTADisplayController`). The crosshair then snaps to /
reads the selected trace's curve. Show the selected square highlighted (`crosshairSelected = true`).

## 3. Re-enable the crosshair + feed hover (AnalyzerPro + SDK)
- Find why the crosshair is off in Metal mode (a disable flag or hover not fed) and re-enable it.
- Ensure mouse-move over the analyzer feeds the hover X (freq) into the controller so
  `paintInteractionOverlays` draws the crosshair at the hovered frequency, reading the
  selected trace's dB. The analyzer JUCE component still receives mouse events under the Metal backing
  layer — confirm `AnalyzerDisplayView`/`RTADisplay` get `mouseMove`/`mouseExit` and forward to the
  controller. A repaint on hover will refresh the chrome capture (chrome-rate; acceptable for now).

## 4. Chrome must recapture on hover/selection change
The crosshair + legend live in the chrome texture. Make sure a mouse-move / legend-click marks the
analyzer dirty so the next chrome capture includes the updated crosshair/selection (otherwise it won't
update). Use the existing chrome-dirty/repaint path.

## Acceptance (Claude builds/signs/installs; human verifies in PT)
- Active traces show as colored square + label indicators on the analyzer (RMS/Peak/L/R/M/S/Mono/Stereo).
- Clicking a square selects it (highlighted); the crosshair then follows THAT trace's curve.
- Hovering the analyzer shows the crosshair at the cursor frequency reading the selected trace (may lag
  slightly at chrome-rate — expected). Leaving the analyzer hides it.
- Non-Metal / other mdsp_ui plugins: legend still shows exactly as before (no double-draw, no change).
- No crash.
