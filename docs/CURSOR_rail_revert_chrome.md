# Cursor task — REVERT the rail to the chrome-clipped overlay (working buttons + popups)

The Metal-composited rail (rail captured to its own texture, composited on top) gives transparency +
smooth fade BUT breaks interaction: buttons need manual re-capture to show state, and JUCE popups
(color-picker CallOutBox) open and immediately close because the rail is hidden/re-captured each frame.
**Revert to the chrome-clipped rail**: the rail is a normal visible JUCE component in the chrome
texture, and the Metal analyzer/scopes are CLIPPED out of the rail's rectangle so the chrome rail shows
on top. This is the build that had working buttons + color picker. Also make open/close **instant**
(no fade) to avoid the chrome-rate jank.

Decision: reliability over transparency. Accept the rail shows the chrome (grid/labels) behind its
translucent 0.60 panel — NOT the live moving traces.

## Remove the Metal rail-composite path
- `MetalHostShared.h` `MetalAnalyzerFrame`: remove `railTexActive`, `railTexRectPx`, `railTexOpacity`.
  Restore `bool railOverlayActive = false;` + `MetalRectPx railOverlayRectPx;`.
- `MetalHost.{h,mm}`: remove `drawRailOverlay`, `setRailFrame`, the rail texture ring/upload, and the
  `drawRailOverlay(...)` call in `drawRenderPass`. RESTORE the rail clip: a `railClipRightPx(frame, defaultRight)`
  helper (returns `min(defaultRight, frame->railOverlayRectPx.x)` when `railOverlayActive`) applied to the
  scissor right-edge in `drawAnalyzerFrame`, `drawPhaseFanFrame`, `drawGonioFrame`, `drawMeterBars`, and
  `drawCrosshair` (skip the draw if clipped width ≤ 0, guarding NSUInteger underflow as before).
- `MetalEditorRenderer.{h,mm}` + `IEditorSurface.h`: remove `captureRailFrame`, `requestRailCapture`,
  the rail payload pool, and the `kRailCaptureTimerId` handling. (Keep chrome capture + requestChromeCapture.)
- `PluginEditor.{h,cpp}`: remove `setMetalRailCaptureCallback` wiring (keep the chrome-capture callback).

## Restore the chrome-clipped rail in MainView
- The rail is a normal visible chrome component again:
  - Remove `paintMetalRailCapture`, the rail texture capture, `railTexOpacity_`, `railFade*`, the
    periodic `railRecaptureTick_` recapture in `timerCallback`, `requestMetalRailCapture`, and
    `syncRailViewportVisualState` setting `railViewport_.setAlpha(0)`.
  - Do NOT hide the rail during chrome capture — it MUST be in the chrome now. Remove the
    rail `setVisible(false)/setVisible(true)` bracketing tied to `setMetalTraceSuppressedForChromeCapture`
    (keep the trace suppression itself; just stop hiding the rail).
  - `resized()`: keep the OVERLAY layout (analyzer/scopes/loudness use FULL width; the rail does NOT
    carve layout space). Position `railViewport_` as a right-edge overlay over the content bounds,
    `setVisible(railIsOpen_)`, and `toFront(false)` so it's topmost for hit-testing.
  - In `fillMetalAnalyzerFrame`: set `frame.railOverlayActive = railIsOpen_ && railViewport_.isVisible()`
    and `frame.railOverlayRectPx = componentBoundsToMetalRectPx(editor, railViewport_, railViewport_.getLocalBounds(), backingScale)` (drives the Metal clip).
  - `ControlRail::paint` keeps the translucent `theme.panel.withAlpha(0.60f)` so the chrome grid shows
    through.
- `toggleRail()`: make it INSTANT — `railIsOpen_ = !railIsOpen_; railViewport_.setVisible(railIsOpen_);
  if (railIsOpen_) railViewport_.toFront(false); resized(); header_.setRailOpen(railIsOpen_);`. No
  opacity/width animation, no `railAnimator_` for the rail.

## Acceptance (Claude builds/signs/installs; human verifies in PT)
- Rail buttons respond and visibly change state; the trace-colour picker opens and STAYS open and is
  usable. Rail opens/closes instantly (no jank). Analyzer/scopes stay full-size (no reflow); the rail
  sits on top, translucent enough to see the grid behind. No crash.
