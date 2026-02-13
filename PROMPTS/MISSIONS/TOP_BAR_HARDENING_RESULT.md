# Top Bar Pixel-Snapping Hardening — Deliverables

## FILES CHANGED

- Source/ui/layout/HeaderBar.cpp

## SUMMARY

- **Clamp stroke radius safely:** In HeaderBarLookAndFeel::drawButtonBackground, strokeRadius is computed as `juce::jmax(0.0f, radius - 0.5f)` then passed through `snapRadius()` so the clamped value is snapped to the nearest 0.5 and never negative.

- **Derive inside-stroke rect from snapped fill rect:** Fill rect is snapped once as `fillR = snapRectToPixels(button.getLocalBounds().toFloat())`. Stroke rect is computed only from fillR: `strokeR = insetForInsideStroke(fillR, 1.0f)`. All fill and stroke drawing use fillR and strokeR; nothing is recomputed from original bounds. drawButtonText uses the same fillR for the snapped text bounds.

- **Prevent cumulative rounding drift in button row layout:** In HeaderBar::resized(), the canonical row is snapped once with `row = snapRectToPixels(barArea.reduced(paddingX, paddingY).toNearestInt())`. buttonH and y remain integer. Right zone is an integer rect `rightZone = Rectangle(row.getRight() - rightZoneWidth, row.getY(), rightZoneWidth, row.getHeight())`. Buttons are placed left→right with a single running x: start `x = rightZone.getX()`, then for each control `setBounds(x, y, w, buttonH)` and `x += w + gapX` (all integer arithmetic). The last control (rail toggle) uses `railW = juce::jmin(smallBtnW, rightZone.getRight() - x)` so it does not overflow the row.

## UNIFIED DIFFS

See: `git diff --no-color Source/ui/layout/HeaderBar.cpp`

(Full diff includes prior top-bar polish; hardening-specific edits are: drawButtonBackground uses fillR/strokeR and snapRadius(jmax(0, radius-0.5f)); drawButtonText uses fillR; resized() uses snapRectToPixels(row), integer rightZone, and left-to-right placement with x += w + gapX and railW = jmin(smallBtnW, rightZone.getRight() - x).)
