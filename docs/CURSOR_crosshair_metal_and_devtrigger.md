# Cursor task — crosshair on the Metal render thread + a PT-reliable dev-panel trigger

Two fixes.

## A — Port the analyzer crosshair to the Metal render thread (smoothness)
The crosshair currently lives in the CPU chrome (captured ~10fps) → too laggy. Render it on the Metal
render thread so it tracks the cursor 1:1. Keep the existing legend/selection logic (which trace is
selected) — only the crosshair DRAW moves to Metal. The legend squares can stay in chrome.

### Plumb hover state into the frame — `MetalHostShared.h` `MetalAnalyzerFrame`
```cpp
bool  crosshairActive = false;
float crosshairXPx     = 0.0f;   // cursor X in DRAWABLE px (clamped to plot)
int   crosshairTraceId = -1;     // which trace the crosshair reads (TraceId; -1 = none/auto)
MetalColour crosshairColour;     // from theme
```

### Fill it — `AnalyzerDisplayView` (fillMetalAnalyzerFrame)
From the hover controller (`FftHoverController` / `RTADisplayController` — the same source that drives
the chrome crosshair), set `crosshairActive` when the mouse is over the plot, `crosshairXPx` =
hovered X mapped to drawable px (use the same `plotRectPx` mapping / backingScale the traces use),
`crosshairTraceId` = the selected legend trace, `crosshairColour` from theme. Set inactive on
mouseExit. (Mouse events already reach the JUCE view under the BackingLayer.)

### Draw it — `MetalHost.mm` (new `drawCrosshair`, called in `drawRenderPass` AFTER traces, BEFORE the
rail clip is irrelevant — but DO honor the rail clip: skip if `crosshairXPx >= railClipRightPx(...)`)
- Scissor to the plot rect (like `drawAnalyzerFrame`).
- Vertical line at `crosshairXPx` across the plot height (thin quad, ~1px, translucent crosshair colour).
- Find the selected trace's dB at that X: pick the matching render-thread array by `crosshairTraceId`
  (`analyzerPeakDb` / `analyzerSmoothedDb` / `analyzerSmoothedDb{L,R,Mid,Side,Mono}` / peak-hold), map
  X→bin and dB→Y using the SAME mapping `buildTraceVerticesFromDb` uses, then draw a short horizontal
  tick + a small dot at (crosshairXPx, y). Reuse `emitGlowRibbon`/quad helpers; preallocated buffers,
  no render-thread allocations.
- If `crosshairTraceId < 0` or arrays not ready, draw just the vertical line.

Acceptance: hovering the analyzer shows a crosshair that tracks the cursor with no lag; the dot sits on
the selected trace's curve; clicking a legend square changes which trace it reads; leaving hides it.

## B — Make the dev look panel openable in Pro Tools (keypress doesn't work there)
The panel is toggled by a ⌘⇧L keypress in `MainView::keyPressed`, but AAX/PT editors don't get keyboard
focus, so it never fires. Add a **mouse trigger** (AAX gets mouse events fine), gated by
`ANALYZERPRO_DEV_LOOK_PANEL`. Mirror MasterLimiter's button approach:

- Add a small gated `juce::TextButton devLookButton_ { "dev" }` to `FooterBar` (only added/visible when
  `ANALYZERPRO_DEV_LOOK_PANEL`), placed discreetly at the far edge of the footer (small, low-contrast).
  Expose `std::function<void()> onDevLookClicked;` on FooterBar; `devLookButton_.onClick` fires it.
- In `MainView`: wire `footer_.onDevLookClicked = [this]{ if (onToggleDevLookPanel) onToggleDevLookPanel(); };`
- Keep the existing ⌘⇧L keypress as a secondary path (harmless).

Acceptance: in PT, a small "dev" button in the footer opens/closes the DEV Look window; works without
keyboard focus. Compiles out when `ANALYZERPRO_DEV_LOOK_PANEL` is 0.
