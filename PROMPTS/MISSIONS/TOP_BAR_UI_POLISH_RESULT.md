# Top Bar UI Polish — Deliverables

## FILES CHANGED

- **Source/ui/layout/PixelSnap.h** (new)
- **Source/ui/layout/HeaderBar.h** (modified)
- **Source/ui/layout/HeaderBar.cpp** (modified)

## SUMMARY

- **Height normalization:** Top bar uses a single canonical button row: `row = barArea.reduced(paddingX, paddingY)`, `buttonH = jlimit(1, available, round(rowHeightTarget))`, `y = round(rowCentreY - buttonH*0.5f)`. All buttons (Preset, Save, A, B, BYPASS, Rail Toggle) and the peak range combo share the same integer height and vertical centre; gap and widths use integer pixels.
- **Inside stroke:** Header bar buttons use a dedicated `HeaderBarLookAndFeel` that draws a 1px stroke inside the fill: `strokeBounds = insetForInsideStroke(snappedBounds, 1.0f)`, `strokeRadius = radius - 0.5f`, then `drawRoundedRectangle(strokeBounds, strokeRadius, 1.0f)`. Fill and stroke use the same snapped rect and radius logic so the stroke follows the fill; no outer halo.
- **Pixel snapping:** Added `PixelSnap.h` with `snapToPixel(float)`, `snapRectToPixels(Rectangle<int>|Rectangle<float>)`, `insetForInsideStroke(Rectangle<float>, strokePx)`, and `snapRadius(float)`. All top-bar button fill and stroke rects (and radius derived from snapped height) are snapped so edges land on integer pixels at 100% and non-integer scaling.

## UNIFIED DIFFS

### New file: Source/ui/layout/PixelSnap.h

```diff
+ #pragma once
+
+ #include <juce_graphics/juce_graphics.h>
+ #include <cmath>
+
+ namespace AnalyzerPro
+ {
+ namespace Layout
+ {
+
+ /** Snaps a value to the nearest integer pixel (for sub-pixel rounding at HiDPI). */
+ inline float snapToPixel (float v) noexcept
+ {
+     return std::round (v);
+ }
+
+ /** Snaps a rectangle so x, y, width, height are integer pixels (edges land on pixel boundaries). */
+ inline juce::Rectangle<int> snapRectToPixels (juce::Rectangle<int> r) noexcept
+ {
+     return juce::Rectangle<int> (
+         static_cast<int> (std::round (static_cast<float> (r.getX()))),
+         static_cast<int> (std::round (static_cast<float> (r.getY()))),
+         static_cast<int> (std::round (static_cast<float> (r.getWidth()))),
+         static_cast<int> (std::round (static_cast<float> (r.getHeight()))));
+ }
+
+ /** Snaps a float rect to integer pixel boundaries. */
+ inline juce::Rectangle<float> snapRectToPixels (juce::Rectangle<float> r) noexcept
+ {
+     return juce::Rectangle<float> (
+         std::round (r.getX()),
+         std::round (r.getY()),
+         std::round (r.getWidth()),
+         std::round (r.getHeight()));
+ }
+
+ /** Returns rect inset by half the stroke width (for 1px inside stroke), then snapped. */
+ inline juce::Rectangle<float> insetForInsideStroke (juce::Rectangle<float> r, float strokePx) noexcept
+ {
+     const float half = strokePx * 0.5f;
+     return snapRectToPixels (r.reduced (half));
+ }
+
+ /** Snaps corner radius to nearest 0.5 for crisp edges at non-integer scale. */
+ inline float snapRadius (float radius) noexcept
+ {
+     return std::round (radius * 2.0f) * 0.5f;
+ }
+
+ } // namespace Layout
+ } // namespace AnalyzerPro
```

### Source/ui/layout/HeaderBar.h

```diff
--- a/Source/ui/layout/HeaderBar.h
+++ b/Source/ui/layout/HeaderBar.h
@@ -6,6 +6,7 @@
 #include "../../presets/PresetManager.h"
 #include "../../presets/ABStateManager.h"
 #include <functional>
+#include <memory>
 
 namespace AnalyzerPro { class ControlBinder; }
@@ -53,5 +54,7 @@ private:
     juce::ToggleButton bypassButton; // Bound to param
     juce::ToggleButton railToggleButton; // Toggle control rail visibility
 
+    std::unique_ptr<juce::LookAndFeel> headerBarLook_;
+
     JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderBar)
 };
```

### Source/ui/layout/HeaderBar.cpp

See full unified diff from: `git diff --no-color Source/ui/layout/HeaderBar.cpp`

(Summary of changes: added PixelSnap.h, LayoutConstants.h, ButtonStyle.h, UiContext.h, cmath; anonymous namespace with getStateColours, HeaderBarLookAndFeel implementing drawButtonBackground with snapped rects + inside stroke and drawButtonText; constructor sets headerBarLook_ on all six top-bar buttons; destructor clears L&F on each; resized() rewritten to canonical row with integer buttonH, y, gapX, and setBounds(x, y, w, buttonH) for all controls.)
