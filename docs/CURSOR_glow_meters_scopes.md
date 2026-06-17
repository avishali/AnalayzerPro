# Cursor task — add glow to Metal meters + phase/gonio scopes

Goal: the analyzer traces now have a glow/shadow/highlight (see `drawTracePayloadFromDb`). The Metal
meter bars and the two scopes still render flat. Add a subtle, consistent glow so they match the
traces' richness. Keep it tasteful (low alpha) — this is polish, values are tunable. All in
`Source/ui/analyzer/metal/MetalHost.mm`. ARC-off rules apply; reuse existing buffers/pipelines, no new
per-frame allocations on the render thread.

## 1. Meter bars — `drawMeterBars` (~line 2357), `appendMeterQuad` (~2317)
Before emitting the solid main-bar quads for each bar, emit a **wider, very translucent halo quad**
using the bar's main colour so the bar reads as glowing:
- halo rect = `(left - 3, mainTop - 3, right + 3, barBottom)` (a few px wider/taller than the bar),
  colour = `bar.mainColour`, top alpha ~`0.12`, bottom alpha ~`0.03` (via `appendMeterQuad`'s
  gradient args), drawn BEFORE the existing main quads so the solid bar sits on top.
- Also add a small soft glow around the **peak cap** line (a short translucent quad a couple px
  taller/wider than the cap) using the peak colour, alpha ~`0.15`.
Keep the existing scissor per bar. If the vertex budget (`appendMeterQuad` returning false) is hit,
just skip the halo for that bar (don't break the main bar).

## 2. Phase-fan — `drawPhaseFanFrame` (~line 2034)
It draws a triangle-fan fill + a contour line-strip (`phaseFanFillBuffer` / `phaseFanLineBuffer`).
Add a glow by drawing the **contour a second time as a translucent, slightly outward-offset pass**,
or simplest: before the existing fill, draw the fill once more scaled ~3–4% larger about the fan
centre (cx,cy) at low alpha (~`0.10`) in the fan colour — a soft halo behind the contour. Reuse the
existing buffer if you can stage the scaled verts; otherwise add a dedicated small glow buffer sized
like the line buffer. Keep it within the existing panel scissor.

## 3. Goniometer — `drawGonioFrame` (~line 2186)
Points are drawn as small quads (`gonioPointBuffer`). Add glow by emitting, for the live points, a
**second larger quad per point at low alpha** (e.g. 2× the point size, alpha ~`0.12`) BEFORE the
solid point, in the same colour — a soft bloom around each dot. Keep the age-faded history/hold
points as-is (or give them a much fainter halo). Respect the existing per-frame point budget; if near
the cap, skip the halo rather than dropping real points.

## Acceptance
- Meter bars, phase-fan, and goniometer all show a soft glow consistent with the analyzer traces;
  no flicker, no perf regression, no crash. Values subtle and tunable.
