# Cursor task — phase-fan contour + peak-hold: real thickness + glow (ribbons, not 1px lines)

In `MetalHost.mm` `drawPhaseFanFrame`, the contour (`phaseFanContourRNorm`) and the peak-hold
(`phaseFanPeakRNorm`) are drawn via `appendPhaseFanLineSegments` → `MTLPrimitiveTypeLineStrip` =
1px hardware lines. They have NO width and NO glow, so the look tunables (which only affect the
triangle-fan FILL) don't touch them. Render them as tessellated **ribbons** (like the analyzer traces'
`emitGlowRibbon`) so they get real thickness + glow, with the peak-hold emphasized.

## 1. New tunables — `MetalHostShared.h` `MetalLookTunables`
```cpp
float phaseFanLineWidth     = 2.0f;   // contour core thickness (px)
float phaseFanPeakWidth     = 2.6f;   // peak-hold core thickness (px) — a touch thicker
float phaseFanLineGlowMult  = 3.0f;   // glow half-width multiple
float phaseFanLineGlowAlpha = 0.20f;  // glow alpha
float phaseFanLineCoreAlpha = 0.95f;  // core alpha
```
Expose these in the dev panel (DevLookControlsComponent) under a "Phase-fan" group.

## 2. Ribbon-tessellate the contour points — `MetalHost.mm`
Add a helper that, given the ordered phase-fan point list in drawable px (the same points
`appendPhaseFanLineSegments` maps from `phaseFanContourRNorm`/`phaseFanPeakRNorm`), tessellates a
ribbon: for each segment, compute the 2D perpendicular normal (mirror `centerlineNormalPx` used by the
analyzer ribbon) and emit a quad of the given half-width; do a wide translucent glow pass then a solid
core pass (reuse the `emitGlowRibbon`/`ColourVertex` machinery and the `phaseFanLineBuffer`, growing it
if needed). Preallocated buffers, no render-thread allocation.

Replace the two line-strip draws:
- **Contour:** one ribbon — width `frame->look.phaseFanLineWidth`, glow
  `phaseFanLineGlowMult`/`phaseFanLineGlowAlpha`, core `phaseFanLineCoreAlpha`, colour
  `phaseFanColour`.
- **Peak-hold** (when `phaseFanPeakHoldEnabled`): one ribbon — width `phaseFanPeakWidth`, same glow,
  brighter core (e.g. `phaseFanColour` brightened ~0.2 like the analyzer peak-hold ceiling) so it
  reads as the held envelope above the live contour. Drop the old 0.22/0.98 double-line-strip.

Keep the existing fill + fill-glow passes unchanged. Keep the panel scissor. Watch the line-vertex
budget (`kMaxPhaseFanLineVertices` / `kMaxPhaseFanLineDraws`) — a ribbon uses ~6 verts per segment vs
2 for a strip, so bump those constants (and the buffer) to cover contour + peak-hold + glow passes.

## Acceptance
- Phase-fan contour and especially the peak-hold are visibly thicker with a soft glow (not hairlines);
  the peak-hold reads as a brighter held ring above the live contour. Dev-panel "Phase-fan" sliders
  change them live. No crash, no buffer overflow (budgets bumped).
