# AxisInteraction adoption inventory
grep -R --line-number "AxisInteraction" Source/ui | sed -n '1,200p'
#:229: command not found: and
Source/ui/views/PhaseCorrelationView.cpp:109:    mdsp_ui::AxisRenderer::draw (g, meterPlotBounds, theme, correlationTicks, mdsp_ui::AxisEdge::Bottom, meterStyle);
Source/ui/analyzer/rta1_import/RTADisplay.cpp:745:            mdsp_ui::AxisRenderer::draw (g, plotBounds, theme, minorHorizontalTicks, mdsp_ui::AxisEdge::Left, gridMinorStyle);
Source/ui/analyzer/rta1_import/RTADisplay.cpp:764:            mdsp_ui::AxisRenderer::draw (g, plotBounds, theme, majorHorizontalTicks, mdsp_ui::AxisEdge::Left, gridMajorStyle);
Source/ui/analyzer/rta1_import/RTADisplay.cpp:789:            mdsp_ui::AxisRenderer::draw (g, plotBounds, theme, minorVerticalTicks, mdsp_ui::AxisEdge::Bottom, gridMinorStyle);
Source/ui/analyzer/rta1_import/RTADisplay.cpp:808:            mdsp_ui::AxisRenderer::draw (g, plotBounds, theme, majorVerticalTicks, mdsp_ui::AxisEdge::Bottom, gridMajorStyle);
Source/ui/analyzer/rta1_import/RTADisplay.cpp:823:        mdsp_ui::AxisRenderer::draw (g, plotBounds, theme, dbTicks, mdsp_ui::AxisEdge::Left, dbStyle);
Source/ui/analyzer/rta1_import/RTADisplay.cpp:837:        mdsp_ui::AxisRenderer::draw (g, plotBounds, theme, freqTicks, mdsp_ui::AxisEdge::Bottom, freqStyle);
#:232: command not found: and
Source/ui/analyzer/rta1_import/RTADisplay.cpp:4:#include <mdsp_ui/AxisInteraction.h>
Source/ui/analyzer/rta1_import/RTADisplay.cpp:482:        // Frequency-axis hover using AxisInteraction
Source/ui/analyzer/rta1_import/RTADisplay.cpp:517:            mdsp_ui::AxisHitTest hit = mdsp_ui::AxisInteraction::hitTest (posPx, plotAreaWidth, freqMapping, freqTicks, snapOpts);
Source/ui/analyzer/rta1_import/RTADisplay.cpp:1052:            const float cursorX = mdsp_ui::AxisInteraction::cursorLineX (plotBounds, lastHoverPosPx);
Source/ui/analyzer/rta1_import/RTADisplay.cpp:1058:        juce::String freqStr = mdsp_ui::AxisInteraction::formatFrequencyHz (lastHoverFreqHz);