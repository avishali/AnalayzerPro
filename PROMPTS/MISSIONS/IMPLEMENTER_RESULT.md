# IMPLEMENTER RESULT

## Mission ID: SCOPE_MODE_VISUAL_APPLY_V3

### Files Changed
1.  `Source/ui/analyzer/StereoScopeView.h`: Updated `setChannelMode` to:
    - Clear the `accumImage_` buffer (invalidate cache)
    - Reset hold state
    - Force a repaint
    - This ensures that when the user switches modes, the old "decay trails" are immediately removed and the new mode allows fresh rendering.

### Mode Propagation Verification
- **Trace**: `MainView::parameterChanged("scopeChannelMode")` -> calls `stereoScopeView_.setChannelMode(val)`.
- **Apply**: `StereoScopeView::setChannelMode` receives the value, stores it in `channelMode_`, and now resets visuals.
- **Render**: `StereoScopeView::renderScopeToImage` reads `channelMode_` and branches:
    - `case MidSide`: Uses `x = Side, y = Mid`
    - `case Stereo`: Uses `x = L, y = R`
    - Logic is demonstrably correct and reachable.

### Why this fixes the visual issue
Previously, `accumImage_` retained the old pixel data (decaying slowly) after a mode switch. Since the renderer is additive (drawing over existing pixels), the user would see a mix of old and new data, or just the old data dominating if the signal was similar, making it look like "nothing happened" or "broken visuals" until it fully decayed. By explicitly clearing `accumImage_`, the switch is instant and obvious.

### Safety
- Preserved `jlimit` clamping in `MainView`.
- Preserved bounds checks `min(limit, buffer.size)` in `StereoScopeView`.
- Clearing `accumbImage_` is thread-safe on the Message Thread (where `setChannelMode` is called).

### Verification Steps
1.  **Build**: Verify Release build succeeds.
2.  **Runtime**:
    - Switch Scope Mode. The display should "blink" (clear) and immediately start drawing the new shape.
    - Check Mono signal: Stereo = Diagonal, M/S = Vertical.
    - Check Hard Pan Left: Stereo = Horizontal/Vertical (depending on axis mapping), M/S = Diagonally offset (Side != 0).
