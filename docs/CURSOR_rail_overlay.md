# Cursor task (follow-up) — rail overlay must sit IN FRONT of the Metal content

The rail overlay (translucent, no reflow) works, BUT the Metal-rendered analyzer/scopes draw OVER it,
hiding the rail's controls. Reason: `MetalHost.mm` `drawRenderPass` composites the chrome texture
(which contains the rail) FIRST, then `drawAnalyzerFrame`/`drawPhaseFanFrame`/`drawGonioFrame`/
`drawMeterBars` on top. So the rail (chrome) is always under the Metal content where they overlap.

Fix: when the rail is open, **clip the Metal content out of the rail's rectangle** so the chrome rail
shows there. The rail is always docked on the RIGHT edge, so each Metal draw just clamps its scissor's
RIGHT edge to the rail's LEFT edge.

## 1. Plumb the rail rect into the frame — `MetalHostShared.h`
Add to `struct MetalAnalyzerFrame`:
```cpp
bool        railOverlayActive = false;
MetalRectPx railOverlayRectPx;   // rail bounds in DRAWABLE pixels (backing-scaled)
```

## 2. Fill it — `MainView::fillMetalAnalyzerFrame` (`MainView.cpp`)
When the rail is open/visible (alpha > 0), set:
```cpp
frame.railOverlayActive = railIsOpen_ && railViewport_.isVisible();
if (frame.railOverlayActive)
    frame.railOverlayRectPx = componentBoundsToMetalRectPx (editor, railViewport_,
                                                            railViewport_.getLocalBounds(), backingScale);
```
(Use the same `editor`/`backingScale` mapping the meters/scopes already use in this function.)

## 3. Clip the Metal draws — `MetalHost.mm`
Add a tiny helper and apply it to the scissor in `drawAnalyzerFrame`, `drawPhaseFanFrame`,
`drawGonioFrame`, and per-bar in `drawMeterBars`: if `frame->railOverlayActive`, clamp the scissor's
right edge so it never enters the rail rect:
```objcpp
// returns a right-edge limit in drawable px; content must not draw at/right of this
auto railClipRight = [&](float defaultRight) -> float {
    if (frame == nullptr || ! frame->railOverlayActive) return defaultRight;
    return juce::jmin (defaultRight, frame->railOverlayRectPx.x);   // rail's left edge
};
```
Apply by replacing each draw's `scissorRight`/right-edge computation with
`juce::jmin(scissorRight, railClipRight(scissorRight))` (and skip the draw if the clipped width ≤ 0).
For `drawAnalyzerFrame` that's the `scissorRight` used for the plot scissor; for the scopes/meters it's
the right edge of their per-panel/per-bar scissor. Meters live outside the rail so they'll usually be
unaffected — the clamp is a no-op for them, which is correct.

## Acceptance
- With the rail open, its controls/text are fully visible and clickable — no analyzer trace or scope
  draws over them. The rail stays translucent (the chrome grid/background shows through the 88% panel).
- Rail closed → analyzer/scopes render full width as before. No crash, no perf change.
