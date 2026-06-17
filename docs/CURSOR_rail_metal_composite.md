# Cursor task — rail as a Metal-composited translucent overlay (fix transparency + fade fps + crosshair)

The rail is currently a JUCE **chrome** element clipped out of the Metal draws. Two problems both come
from that: (a) it can only show the static chrome behind it (looks opaque, not the live analyzer), and
(b) the open/close fade animates the chrome alpha, which is captured at ~10fps → janky. Fix by
compositing the rail as its OWN texture ON TOP of the live Metal content.

## PART A — rail overlay texture (transparency + smoothness)

### Capture the rail to a transparent texture
- Render `railViewport_` (the rail panel + controls) into a `juce::Image` with a **transparent**
  background (ARGB, cleared to transparent; the panel paints at its own alpha, controls opaque). Mirror
  the existing chrome capture path (`MetalEditorRenderer::captureChromeFrame` /
  `MetalHost` chrome texture upload) but for the rail bounds only. Recapture only when the rail content
  is dirty (interaction / resize) — NOT every frame.
- Upload it to a dedicated Metal texture (reuse the chrome upload/sampler/`chromePipeline` machinery).

### Publish rail overlay state into the frame / host
Add to `MetalAnalyzerFrame` (or a small host-side struct, like the chrome frame):
```cpp
bool        railTexActive = false;
MetalRectPx railTexRectPx;     // where to blit it, drawable px
float       railTexOpacity = 0.0f;  // 0..1 for the open/close fade
```
`MainView`/`AnalyzerDisplayView` set `railTexRectPx` (= `railViewport_` bounds → drawable px) and
`railTexActive` when open.

### Composite it last — `MetalHost.mm drawRenderPass`
- **Remove the rail clip** (`railClipRightPx` everywhere) — the analyzer/scopes/meters now draw FULL
  (so live traces exist behind the rail).
- AFTER `drawAnalyzerFrame`/scopes/meters/crosshair, if `railTexActive`, draw a textured quad of the
  rail texture at `railTexRectPx` with alpha blending and overall opacity `railTexOpacity`. This puts
  the translucent rail (live traces visible through the panel) on top, controls crisp.

### Fade on the render thread, not chrome
- `toggleRail`: instead of animating the JUCE component alpha (chrome-rate), animate `railTexOpacity`
  0→1 (open) / 1→0 (close). Drive it from a render-thread or 60fps message-timer value published to the
  host, so the fade is smooth. Keep `railViewport_` interactive (it still receives mouse), just don't
  rely on its alpha for the visual fade.
- Keep the panel's own translucency (e.g. ~0.6–0.7) inside the rail capture so live traces show through.

Acceptance: opening the rail shows a translucent panel with the LIVE moving analyzer visible behind it;
controls are crisp and clickable; the open/close fade is smooth (60fps), not steppy. Closed → full
analyzer, no rail.

## PART B — crosshair coordinate fix (it's currently invisible/gone)

The Metal crosshair feed (`AnalyzerDisplayView::fillMetalAnalyzerFrame`) hardcodes `kPlotLeft = 50` and
tests `hoverX >= kPlotLeft`, but the CPU crosshair (which worked) uses `geometry.getPlotAreaLeft()` —
if those differ, `crosshairActive` never turns on. Fix the feed to use the SAME plot-left the hover
controller reports against (derive it from the RTADisplay geometry / `getPlotAreaLeft()`, not a hardcoded
50), so `crosshairXPx` maps correctly. Verify `drawCrosshair` is reached (it's only called inside the
`hasSuppliedTrace` branch — fine when any trace is on). The line was also very faint; it's been bumped
to alpha 0.85 — keep it clearly visible (≥1.5px wide).

Acceptance: hovering the analyzer shows the crosshair tracking the cursor across the WHOLE plot width
(not offset/clipped), reading the selected trace.
