#include "RTADisplayRenderer.h"
#include "../rta/RTACurveHelpers.h"
#include <mdsp_ui/AxisRenderer.h>
#include <mdsp_ui/AxisInteraction.h>
#include <mdsp_ui/PlotFrameRenderer.h>
#include <mdsp_ui/SeriesRenderer.h>
#include <mdsp_ui/BarsRenderer.h>
#include <mdsp_ui/TextOverlayRenderer.h>
#include <mdsp_ui/LegendRenderer.h>
#include <mdsp_ui/AreaFillRenderer.h>
#include <mdsp_ui/ValueReadoutRenderer.h>
#include <mdsp_ui/ScaleLabelRenderer.h>
#include <mdsp_ui/AxisHoverController.h>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <limits>
#include <juce_gui_basics/juce_gui_basics.h>

#define MDSP_TRACE_SHIMMER_V2 0       // Default OFF: Subtle animated shimmer on peak highlight
#define MDSP_TRACE_GLOW_FALLOFF_V2 1  // Default ON:  Frequency-weighted glow falloff (less muddy LF)

//==============================================================================
// Weighting Helper Functions
//==============================================================================

float RTADisplayRenderer::getBS468WeightingDb (float freqHz)
{
    // ITU-R 468-4 Weighting
    const float f_kHz = freqHz / 1000.0f;
    const float f = f_kHz; 
    
    const double a1 = 1.0458849;
    const double b2 = 1.6620626;
    const double c2 = 0.3181829;
    const double b3 = 0.5057538;
    const double c3 = 0.1691696;
    const double gainScale = 1.24633263;
    
    const double f2 = static_cast<double>(f * f);
    
    const double den1 = f2 + a1*a1;
    const double term2_real = c2 - f2;
    const double term2_imag = b2 * f;
    const double den2 = term2_real*term2_real + term2_imag*term2_imag;
    
    const double term3_real = c3 - f2;
    const double term3_imag = b3 * f;
    const double den3 = term3_real*term3_real + term3_imag*term3_imag;
    
    const double den = den1 * den2 * den3;
    
    if (den == 0.0) return -120.0f;
    
    const double num = gainScale * f;
    const double magSq = (num * num) / den;
    
    return static_cast<float>(10.0 * std::log10(magSq));
}

//==============================================================================
// TRACE_VISUAL_POLISH_V1: Silk trace rendering with glow + AA + perceptual thickness
//==============================================================================
void RTADisplayRenderer::drawSilkTrace (juce::Graphics& g,
                                        const juce::Path& path,
                                        juce::Colour coreColour,
                                        float baseThicknessPx,
                                        float viewportWidth,
                                        bool isPeakTrace,
                                        float energyMul,
                                        bool useShimmer)
{
    if (path.isEmpty())
        return;
    
    // Perceptual thickness scaling based on viewport width
    const float widthScale = juce::jlimit (0.9f, 1.4f, 0.9f + 0.0015f * viewportWidth);
    const float thickness = baseThicknessPx * widthScale;
    
    // Glow pass (wider, semi-transparent)
    const float glowWidth = thickness * (isPeakTrace ? 3.5f : 2.8f);
    // V2: Apply energy multiplier to glow alpha
    const float glowAlpha = (isPeakTrace ? 0.12f : 0.10f) * energyMul;
    
    juce::PathStrokeType glowStroke (glowWidth, 
                                      juce::PathStrokeType::curved, 
                                      juce::PathStrokeType::rounded);
                                      
#if MDSP_TRACE_GLOW_FALLOFF_V2
    // V2: Use gradient to reduce glow in LF (left) and boost HF (right)
    juce::Colour glowCol = coreColour.withAlpha (glowAlpha);
    // Left: 70% of alpha (tame LF), Right: 125% of alpha (sparkle HF)
    juce::ColourGradient grad (glowCol.withAlpha (glowAlpha * 0.7f), 0.0f, 0.0f,
                               glowCol.withAlpha (glowAlpha * 1.25f), viewportWidth, 0.0f, false);
    g.setGradientFill (grad);
#else
    g.setColour (coreColour.withAlpha (glowAlpha));
#endif

    g.strokePath (path, glowStroke);
    
    // Core pass (main trace)
    const float coreAlpha = isPeakTrace ? 0.90f : 0.75f;
    
    juce::PathStrokeType coreStroke (thickness,
                                      juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded);
    g.setColour (coreColour.withAlpha (coreAlpha));
    g.strokePath (path, coreStroke);
    
    // Highlight pass (peak only - subtle bright center)
    if (isPeakTrace)
    {
        const float hiWidth = thickness * 0.5f;
        // V2: Shimmer modulation
        float hiAlpha = 0.30f;
        if (useShimmer)
        {
            // Subtle slow breathe: 0.5Hz
            const float t = (float)juce::Time::getMillisecondCounterHiRes() * 0.001f;
            const float mod = 0.5f + 0.5f * std::sin (t * 3.0f); // 0..1
            // Modulate +/- 10%
            hiAlpha *= (0.9f + 0.2f * mod); 
        }
        
        juce::PathStrokeType hiStroke (hiWidth,
                                        juce::PathStrokeType::curved,
                                        juce::PathStrokeType::rounded);
        g.setColour (coreColour.brighter (0.15f).withAlpha (hiAlpha));
        g.strokePath (path, hiStroke);
    }
}

//==============================================================================
void RTADisplayRenderer::paint (juce::Graphics& g,
                                 const RTADisplayModel& model,
                                 const RTAGeometry& geometry,
                                 const RTADisplayController& controller,
                                 const mdsp_ui::Theme& theme,
                                 float displayGainDb,
                                 rta::TiltMode tiltMode,
                                 const RTADisplayModel::TraceConfig& traceConfig,
                                 juce::Rectangle<int> bounds,
                                 std::function<mdsp_ui::AnalyzerRenderState()> getRenderState)
{
    if (getRenderState)
    {
        mdsp_ui::AnalyzerRenderer::paint (g, bounds, theme, getRenderState());
        const auto& s = model.getState();
        paintInteractionOverlays (g, s, geometry, controller, theme, displayGainDb);
        return;
    }

    const auto& s = model.getState();
    
    // Draw background (grid) from cache
    if (model.isBackgroundValid())
    {
        g.drawImageAt (model.getCachedBackground(), 0, 0);
    }
    
    // Draw mode-specific content
    if (s.viewMode == 0) // FFT
    {
        paintFFTMode (g, s, geometry, controller, theme, displayGainDb, tiltMode, traceConfig, const_cast<RTADisplayModel&>(model));
    }
    else if (s.viewMode == 1) // Log
    {
        paintLogMode (g, s, geometry, controller, theme, displayGainDb);
    }
    else if (s.viewMode == 2) // Bands
    {
        paintBandsMode (g, s, geometry, controller, theme, displayGainDb);
    }
    
    // Draw interaction overlays
    paintInteractionOverlays (g, s, geometry, controller, theme, displayGainDb);
}

//==============================================================================
void RTADisplayRenderer::drawGrid (juce::Graphics& g,
                                    const RenderState& s,
                                    const RTAGeometry& geometry,
                                    const mdsp_ui::Theme& theme,
                                    float displayGainDb)
{
    // B3: Pure function - uses only state reference, no member mutations
    
    // Define plot bounds
    const juce::Rectangle<int> plotBounds (static_cast<int> (geometry.getPlotAreaLeft()),
                                           static_cast<int> (geometry.getPlotAreaTop()),
                                           static_cast<int> (geometry.getPlotAreaWidth()),
                                           static_cast<int> (geometry.getPlotAreaHeight()));
    
    // Build dB axis ticks (Left edge) - only major ticks (every 12dB) get labels
    juce::Array<mdsp_ui::AxisTick> dbTicks;
    for (int db = static_cast<int> (s.topDb); db >= static_cast<int> (s.bottomDb); db -= 6)
    {
        const float y = geometry.dbToYPx (static_cast<float> (db) + displayGainDb);
        if (y >= geometry.getPlotAreaTop() && y <= geometry.getPlotAreaTop() + geometry.getPlotAreaHeight())
        {
            const float posPx = y - geometry.getPlotAreaTop();  // Offset from plot top
            const bool isMajor = (db % 12 == 0);
            juce::String label = isMajor ? (juce::String (db) + " dB") : juce::String();
            dbTicks.add ({ posPx, label, isMajor });
        }
    }

    // Build frequency axis ticks (Bottom edge)
    juce::Array<mdsp_ui::AxisTick> freqTicks;
    const float freqGridPoints[] = { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };
    for (float freq : freqGridPoints)
    {
        if (freq >= s.minHz && freq <= s.maxHz)
        {
            const float x = geometry.freqHzToXPx (freq);
            const float posPx = x - geometry.getPlotAreaLeft();  // Offset from plot left
            juce::String label;
            if (freq >= 1000.0f)
                label = juce::String (freq / 1000.0f, 1) + "k";
            else
                label = juce::String (static_cast<int> (freq));
            // Mark major frequencies: endpoints (20, 20k) and round numbers (100, 1k, 10k)
            auto isMajorFreq = [](float f)
            {
                const float majors[] = { 20.0f, 100.0f, 1000.0f, 10000.0f, 20000.0f };
                for (float m : majors)
                    if (std::abs (f - m) < 0.1f) return true;
                return false;
            };
            const bool isMajor = isMajorFreq (freq);
            freqTicks.add ({ posPx, label, isMajor });
        }
    }
    
    // Draw horizontal grid lines (from dB ticks) - minor first, then major
    if (! dbTicks.isEmpty())
    {
        const float plotWidth = static_cast<float> (plotBounds.getWidth());
        
        // Minor horizontal grid lines
        juce::Array<mdsp_ui::AxisTick> minorHorizontalTicks;
        for (const auto& tick : dbTicks)
        {
            if (! tick.major)
                minorHorizontalTicks.add ({ tick.posPx, juce::String(), false });
        }
        if (! minorHorizontalTicks.isEmpty())
        {
            mdsp_ui::AxisStyle gridMinorStyle;
            gridMinorStyle.ticksOnly = true;
            gridMinorStyle.clipTicksToPlot = true;
            gridMinorStyle.tickAlpha = 0.20f;
            gridMinorStyle.tickThickness = 1.0f;
            gridMinorStyle.minorTickLengthPx = plotWidth;
            gridMinorStyle.majorTickLengthPx = plotWidth;
            mdsp_ui::AxisRenderer::draw (g, plotBounds, theme, minorHorizontalTicks, mdsp_ui::AxisEdge::Left, gridMinorStyle);
        }
        
        // Major horizontal grid lines
        juce::Array<mdsp_ui::AxisTick> majorHorizontalTicks;
        for (const auto& tick : dbTicks)
        {
            if (tick.major)
                majorHorizontalTicks.add ({ tick.posPx, juce::String(), true });
        }
        if (! majorHorizontalTicks.isEmpty())
        {
            mdsp_ui::AxisStyle gridMajorStyle;
            gridMajorStyle.ticksOnly = true;
            gridMajorStyle.clipTicksToPlot = true;
            gridMajorStyle.tickAlpha = 0.35f;
            gridMajorStyle.tickThickness = 1.0f;
            gridMajorStyle.minorTickLengthPx = plotWidth;
            gridMajorStyle.majorTickLengthPx = plotWidth;
            mdsp_ui::AxisRenderer::draw (g, plotBounds, theme, majorHorizontalTicks, mdsp_ui::AxisEdge::Left, gridMajorStyle);
        }
    }
    
    // Draw vertical grid lines (from frequency ticks) - minor first, then major
    if (! freqTicks.isEmpty())
    {
        const float plotHeight = static_cast<float> (plotBounds.getHeight());
        
        // Minor vertical grid lines
        juce::Array<mdsp_ui::AxisTick> minorVerticalTicks;
        for (const auto& tick : freqTicks)
        {
            if (! tick.major)
                minorVerticalTicks.add ({ tick.posPx, juce::String(), false });
        }
        if (! minorVerticalTicks.isEmpty())
        {
            mdsp_ui::AxisStyle gridMinorStyle;
            gridMinorStyle.ticksOnly = true;
            gridMinorStyle.clipTicksToPlot = true;
            gridMinorStyle.tickAlpha = 0.20f;
            gridMinorStyle.tickThickness = 1.0f;
            gridMinorStyle.minorTickLengthPx = plotHeight;
            gridMinorStyle.majorTickLengthPx = plotHeight;
            mdsp_ui::AxisRenderer::draw (g, plotBounds, theme, minorVerticalTicks, mdsp_ui::AxisEdge::Bottom, gridMinorStyle);
        }
        
        // Major vertical grid lines
        juce::Array<mdsp_ui::AxisTick> majorVerticalTicks;
        for (const auto& tick : freqTicks)
        {
            if (tick.major)
                majorVerticalTicks.add ({ tick.posPx, juce::String(), true });
        }
        if (! majorVerticalTicks.isEmpty())
        {
            mdsp_ui::AxisStyle gridMajorStyle;
            gridMajorStyle.ticksOnly = true;
            gridMajorStyle.clipTicksToPlot = true;
            gridMajorStyle.tickAlpha = 0.35f;
            gridMajorStyle.tickThickness = 1.0f;
            gridMajorStyle.minorTickLengthPx = plotHeight;
            gridMajorStyle.majorTickLengthPx = plotHeight;
            mdsp_ui::AxisRenderer::draw (g, plotBounds, theme, majorVerticalTicks, mdsp_ui::AxisEdge::Bottom, gridMajorStyle);
        }
    }

    // Draw dB axis (Left edge) with labels
    if (! dbTicks.isEmpty())
    {
        mdsp_ui::AxisStyle dbStyle;
        dbStyle.tickAlpha = 0.35f;
        dbStyle.labelAlpha = 0.90f;
        dbStyle.tickThickness = 1.0f;
        dbStyle.labelFontHeight = 10.0f;
        dbStyle.labelInsetPx = 6.0f;
        dbStyle.ticksOnly = false;
        dbStyle.clipTicksToPlot = true;
        mdsp_ui::AxisRenderer::draw (g, plotBounds, theme, dbTicks, mdsp_ui::AxisEdge::Left, dbStyle);
    }

    // Draw frequency axis (Bottom edge) with labels
    if (! freqTicks.isEmpty())
    {
        mdsp_ui::AxisStyle freqStyle;
        freqStyle.tickAlpha = 0.35f;
        freqStyle.labelAlpha = 0.90f;
        freqStyle.tickThickness = 1.0f;
        freqStyle.labelFontHeight = 10.0f;
        freqStyle.labelInsetPx = 6.0f;
        freqStyle.ticksOnly = false;
        freqStyle.clipTicksToPlot = true;
        mdsp_ui::AxisRenderer::draw (g, plotBounds, theme, freqTicks, mdsp_ui::AxisEdge::Bottom, freqStyle);
    }
    
    // Draw scale labels
    {
        const juce::Rectangle<float> plotBoundsFloat (plotBounds.toFloat());
        
        mdsp_ui::ScaleLabelStyle scaleLabelStyle;
        scaleLabelStyle.fontHeightPx = 10.0f;
        scaleLabelStyle.alpha = 0.6f;
        scaleLabelStyle.insetPx = 6.0f;
        scaleLabelStyle.rotateForVertical = true;
        
        // Bottom edge: "Hz"
        mdsp_ui::ScaleLabel hzLabel;
        hzLabel.text = "Hz";
        hzLabel.enabled = true;
        mdsp_ui::ScaleLabelRenderer::draw (g, plotBoundsFloat, mdsp_ui::ScaleLabelEdge::Bottom, hzLabel, scaleLabelStyle, theme);
        
        // Left edge: "dB"
        mdsp_ui::ScaleLabel dbLabel;
        dbLabel.text = "dB";
        dbLabel.enabled = true;
        mdsp_ui::ScaleLabelRenderer::draw (g, plotBoundsFloat, mdsp_ui::ScaleLabelEdge::Left, dbLabel, scaleLabelStyle, theme);
    }
}

//==============================================================================
void RTADisplayRenderer::paintBandsMode (juce::Graphics& g,
                                         const RenderState& s,
                                         const RTAGeometry& geometry,
                                         const RTADisplayController& controller,
                                         const mdsp_ui::Theme& theme,
                                         float displayGainDb)
{
    // B3: Pure function - uses only state reference, no member mutations
    
    // B7: Never calls updateGeometry() from paint
    if (!geometry.isGeometryValid() || s.bandCentersHz.empty() || s.bandsDb.empty())
        return;
    
    // Check size mismatch
    if (s.bandsDb.size() != s.bandCentersHz.size() || 
        geometry.getBandGeometry().size() != s.bandCentersHz.size())
        return;
    
    // B3: Decide locally if peaks should be drawn (no state mutation)
    const bool hasPeaks = (s.bandsPeakDb.size() == s.bandsDb.size() && !s.bandsPeakDb.empty());
    
    // Draw thin vertical markers at each band center frequency (subtle)
    g.setColour (theme.grid.withAlpha (0.2f));
    for (size_t i = 0; i < s.bandCentersHz.size() && i < geometry.getBandGeometry().size(); ++i)
    {
        const float x = geometry.getBandGeometry()[i].xCenter;
        g.drawVerticalLine (static_cast<int> (x), geometry.getPlotAreaTop(), geometry.getPlotAreaTop() + geometry.getPlotAreaHeight());
    }

    // Draw band bars using BarsRenderer
    const int numBands = static_cast<int> (std::min (s.bandsDb.size(), geometry.getBandGeometry().size()));
    if (numBands > 0)
    {
        // Use fixed stack array to avoid heap allocations (cap at 4096)
        static constexpr int kMaxBars = 4096;
        const int barsToDraw = std::min (numBands, kMaxBars);
        
        float xLeft[kMaxBars];
        float xRight[kMaxBars];
        float yTop[kMaxBars];
        
        const float bottomY = geometry.getPlotAreaTop() + geometry.getPlotAreaHeight();

        // Collect bar geometry
        for (int i = 0; i < barsToDraw; ++i)
        {
            const auto idx = static_cast<size_t> (i);
            xLeft[i] = geometry.getBandGeometry()[idx].xLeft;
            xRight[i] = geometry.getBandGeometry()[idx].xRight;
            
            // Apply display gain before geometry mapping
            const float db = s.bandsDb[idx];
            yTop[i] = geometry.dbToYPx (db + displayGainDb);
        }
        
        // Render bars
        const juce::Rectangle<int> plotBounds (static_cast<int> (geometry.getPlotAreaLeft()),
                                                static_cast<int> (geometry.getPlotAreaTop()),
                                                static_cast<int> (geometry.getPlotAreaWidth()),
                                                static_cast<int> (geometry.getPlotAreaHeight()));
        
        mdsp_ui::BarsStyle barsStyle;
        barsStyle.fillAlpha = 0.7f;
        barsStyle.clipToPlot = true;
        barsStyle.minBarWidthPx = 1.0f;
        
        mdsp_ui::BarsRenderer::drawBars (g, plotBounds, theme,
                                          xLeft, xRight, yTop, barsToDraw,
                                          bottomY,
                                          theme.accent, barsStyle);
    }

    // Draw peak trace (using local hasPeaks boolean, NO state mutation) using SeriesRenderer
    if (hasPeaks)
    {
        const juce::Rectangle<float> plotBounds (geometry.getPlotAreaLeft(), geometry.getPlotAreaTop(), geometry.getPlotAreaWidth(), geometry.getPlotAreaHeight());
        const int numBandsToDraw = static_cast<int> (std::min (s.bandsPeakDb.size(), geometry.getBandGeometry().size()));
        
        mdsp_ui::SeriesStyle peakStyle;
        peakStyle.strokeThickness = 1.5f;
        peakStyle.alpha = 0.8f;
        peakStyle.clipToPlot = true;
        peakStyle.minXStepPx = 1.0f;
        peakStyle.minYStepPx = 0.5f;
        peakStyle.useRoundedJoins = true;
#if JUCE_DEBUG
        peakStyle.decimationMode = controller.getUseEnvelopeDecimator()
            ? mdsp_ui::DecimationMode::Envelope
            : mdsp_ui::DecimationMode::Simple;
#else
        peakStyle.decimationMode = mdsp_ui::DecimationMode::Simple;
#endif
        peakStyle.envelopeMinBucketPx = 1.0f;
        peakStyle.envelopeDrawVertical = true;

        mdsp_ui::SeriesRenderer::drawPathFromMapping (g, plotBounds, theme, numBandsToDraw,
            [&geometry] (int i) -> float
            {
                const auto idx = static_cast<size_t> (i);
                return geometry.getBandGeometry()[idx].xCenter;
            },
            [&s, &geometry, displayGainDb] (int i) -> float
            {
                const auto idx = static_cast<size_t> (i);
                float peakDb = s.bandsPeakDb[idx];
                peakDb = juce::jlimit (s.bottomDb, 0.0f, peakDb);
                return geometry.dbToYPx (peakDb + displayGainDb);
            },
            theme.seriesPeak, peakStyle);
    }
    
    // Draw legend overlay
    {
        const juce::Rectangle<float> legendPlotBounds (geometry.getPlotAreaLeft(), geometry.getPlotAreaTop(), geometry.getPlotAreaWidth(), geometry.getPlotAreaHeight());
        mdsp_ui::LegendItem legendItems[2];
        legendItems[0].label = "Bands";
        legendItems[0].colour = theme.accent;
        legendItems[0].enabled = true;
        legendItems[1].label = "Peak";
        legendItems[1].colour = theme.seriesPeak;
        legendItems[1].enabled = hasPeaks;
        
        mdsp_ui::LegendStyle legendStyle;
        legendStyle.fontHeightPx = 10.0f;
        legendStyle.drawFrame = true;
        legendStyle.frameCornerRadiusPx = 4.0f;
        legendStyle.frameFillAlpha = 0.80f;
        legendStyle.frameBorderAlpha = 0.90f;
        
        mdsp_ui::LegendRenderer::draw (g, legendPlotBounds, theme, legendItems, 2, mdsp_ui::LegendEdge::TopRight, legendStyle);
    }

    // Draw cursor and readout (with defensive bounds checking)
    const int hoveredBandIndex = controller.getHoveredBandIndex();
    if (hoveredBandIndex >= 0 
        && hoveredBandIndex < static_cast<int> (s.bandsDb.size()) 
        && hoveredBandIndex < static_cast<int> (geometry.getBandGeometry().size())
        && hoveredBandIndex < static_cast<int> (s.bandCentersHz.size()))
    {
        const int safeIndex = hoveredBandIndex;
        const auto idx = static_cast<size_t> (safeIndex);
        const float x = geometry.getBandGeometry()[idx].xCenter;
        const float currentDb = s.bandsDb[idx];
        const float peakDb = (hasPeaks && idx < s.bandsPeakDb.size())
            ? s.bandsPeakDb[idx] : -1000.0f;
        const float centerFreq = s.bandCentersHz[idx];

        // Vertical cursor line
        g.setColour (theme.text.withAlpha (0.5f));
        g.drawVerticalLine (static_cast<int> (x), geometry.getPlotAreaTop(), geometry.getPlotAreaTop() + geometry.getPlotAreaHeight());

        // Tooltip box using PlotFrameRenderer
        const float tooltipX = juce::jmin (x + 10.0f, geometry.getPlotAreaLeft() + geometry.getPlotAreaWidth() - 120.0f);
        const float tooltipY = geometry.getPlotAreaTop() + 10.0f;
        const float tooltipW = 110.0f;
        const float tooltipH = hasPeaks ? 50.0f : 35.0f;

        mdsp_ui::PlotFrameStyle tooltipStyle;
        tooltipStyle.cornerRadiusPx = 4.0f;
        tooltipStyle.borderThicknessPx = 1.0f;
        tooltipStyle.borderAlpha = 0.9f;
        tooltipStyle.fillAlpha = 0.9f;
        tooltipStyle.drawFill = true;
        tooltipStyle.drawBorder = true;
        tooltipStyle.clipToFrame = false;
        
        const juce::Rectangle<float> tooltipBounds (tooltipX, tooltipY, tooltipW, tooltipH);

        // Format frequency
        juce::String freqStr;
        if (centerFreq >= 1000.0f)
            freqStr = juce::String (centerFreq / 1000.0f, 2) + " kHz";
        else
            freqStr = juce::String (centerFreq, 1) + " Hz";

        // Draw tooltip using ValueReadoutRenderer
        mdsp_ui::ValueReadoutLine tooltipLines[3];
        tooltipLines[0].left = "Fc:";
        tooltipLines[0].right = freqStr;
        tooltipLines[0].enabled = true;
        tooltipLines[1].left = "Cur:";
        tooltipLines[1].right = juce::String (currentDb, 1) + " dB";
        tooltipLines[1].enabled = true;
        tooltipLines[2].left = "Peak:";
        tooltipLines[2].right = juce::String (peakDb, 1) + " dB";
        tooltipLines[2].enabled = (hasPeaks && peakDb > s.bottomDb);
        
        mdsp_ui::ValueReadoutStyle readoutStyle;
        readoutStyle.fontHeightPx = 10.0f;
        readoutStyle.drawFrame = true;
        readoutStyle.cornerRadiusPx = 4.0f;
        readoutStyle.frameFillAlpha = 0.9f;
        readoutStyle.frameBorderAlpha = 0.9f;
        readoutStyle.textAlpha = 1.0f;
        readoutStyle.disabledTextAlpha = 0.55f;
        
        const int numTooltipLines = (hasPeaks && peakDb > s.bottomDb) ? 3 : 2;
        mdsp_ui::ValueReadoutRenderer::drawAt (g, tooltipBounds, theme, tooltipLines, numTooltipLines, readoutStyle);
    }
}

//==============================================================================
void RTADisplayRenderer::paintLogMode (juce::Graphics& g,
                                        const RenderState& s,
                                        const RTAGeometry& geometry,
                                        const RTADisplayController& controller,
                                        const mdsp_ui::Theme& theme,
                                        float displayGainDb)
{
    // B3: Pure function - uses only state reference, no member mutations
    
    // B4: Compute log centers on-the-fly from index (no logCentersHz storage)
    // B7: Never calls updateGeometry() from paint
    
    if (s.logDb.empty())
        return;
    
    const int numBands = static_cast<int> (s.logDb.size());
    const float logMin = std::log10 (s.minHz);
    const float logMax = std::log10 (s.maxHz);
    const float logRange = logMax - logMin;
    
    // B3: Decide locally if peaks should be drawn (no state mutation)
    const bool hasPeaks = (s.logPeakDb.size() == s.logDb.size() && !s.logPeakDb.empty());
    
    // B4: Draw band bars using BarsRenderer - compute x positions from index on-the-fly
    if (numBands > 0)
    {
        // Use fixed stack array to avoid heap allocations (cap at 4096)
        static constexpr int kMaxBars = 4096;
        const int barsToDraw = std::min (numBands, kMaxBars);
        
        float xLeft[kMaxBars];
        float xRight[kMaxBars];
        float yTop[kMaxBars];
        
        const float bottomY = geometry.getPlotAreaTop() + geometry.getPlotAreaHeight();
        
        // Collect bar geometry
        for (int i = 0; i < barsToDraw; ++i)
        {
            // Compute left/right boundaries in log space
            const float logPosLow = logMin + (logRange * static_cast<float> (i)) / static_cast<float> (numBands);
            const float logPosHigh = logMin + (logRange * static_cast<float> (i + 1)) / static_cast<float> (numBands);
            const float fL = std::pow (10.0f, logPosLow);
            const float fR = std::pow (10.0f, logPosHigh);
            
            // Map to x coordinates
            xLeft[i] = geometry.freqHzToXPx (fL);
            xRight[i] = geometry.freqHzToXPx (fR);
        
            // Apply display gain before geometry mapping
            const auto idx = static_cast<size_t> (i);
            const float db = s.logDb[idx];
            yTop[i] = geometry.dbToYPx (db + displayGainDb);
        }
        
        // Render bars
        const juce::Rectangle<int> plotBounds (static_cast<int> (geometry.getPlotAreaLeft()),
                                                static_cast<int> (geometry.getPlotAreaTop()),
                                                static_cast<int> (geometry.getPlotAreaWidth()),
                                                static_cast<int> (geometry.getPlotAreaHeight()));
        
        mdsp_ui::BarsStyle barsStyle;
        barsStyle.fillAlpha = 0.7f;
        barsStyle.clipToPlot = true;
        barsStyle.minBarWidthPx = 1.0f;
        
        mdsp_ui::BarsRenderer::drawBars (g, plotBounds, theme,
                                          xLeft, xRight, yTop, barsToDraw,
                                          bottomY,
                                          theme.accent, barsStyle);
    }

    // B4: Draw peak trace - compute centers from index on-the-fly using SeriesRenderer
    if (hasPeaks)
    {
        const juce::Rectangle<float> plotBounds (geometry.getPlotAreaLeft(), geometry.getPlotAreaTop(), geometry.getPlotAreaWidth(), geometry.getPlotAreaHeight());
        mdsp_ui::SeriesStyle peakStyle;
        peakStyle.strokeThickness = 1.5f;
        peakStyle.alpha = 0.8f;
        peakStyle.clipToPlot = true;
        peakStyle.minXStepPx = 1.0f;
        peakStyle.minYStepPx = 0.5f;
        peakStyle.useRoundedJoins = true;
#if JUCE_DEBUG
        peakStyle.decimationMode = controller.getUseEnvelopeDecimator()
            ? mdsp_ui::DecimationMode::Envelope
            : mdsp_ui::DecimationMode::Simple;
#else
        peakStyle.decimationMode = mdsp_ui::DecimationMode::Simple;
#endif
        peakStyle.envelopeMinBucketPx = 1.0f;
        peakStyle.envelopeDrawVertical = true;

        mdsp_ui::SeriesRenderer::drawPathFromMapping (g, plotBounds, theme, numBands,
            [&geometry, numBands, &s] (int i) -> float
            {
                const float centerFreq = geometry.computeLogFreqFromIndex (i, numBands, s.minHz, s.maxHz);
                return geometry.freqHzToXPx (centerFreq);
            },
            [&s, &geometry, displayGainDb] (int i) -> float
            {
                const auto idx = static_cast<size_t> (i);
                float peakDb = s.logPeakDb[idx];
                peakDb = juce::jlimit (s.bottomDb, 0.0f, peakDb);
                return geometry.dbToYPx (peakDb + displayGainDb);
            },
            theme.seriesPeak, peakStyle);
    }
    
    // Draw legend overlay
    {
        const juce::Rectangle<float> legendPlotBounds (geometry.getPlotAreaLeft(), geometry.getPlotAreaTop(), geometry.getPlotAreaWidth(), geometry.getPlotAreaHeight());
        mdsp_ui::LegendItem legendItems[2];
        legendItems[0].label = "Log";
        legendItems[0].colour = theme.accent;
        legendItems[0].enabled = true;
        legendItems[1].label = "Peak";
        legendItems[1].colour = theme.seriesPeak;
        legendItems[1].enabled = hasPeaks;
        
        mdsp_ui::LegendStyle legendStyle;
        legendStyle.fontHeightPx = 10.0f;
        legendStyle.drawFrame = true;
        legendStyle.frameCornerRadiusPx = 4.0f;
        legendStyle.frameFillAlpha = 0.80f;
        legendStyle.frameBorderAlpha = 0.90f;
        
        mdsp_ui::LegendRenderer::draw (g, legendPlotBounds, theme, legendItems, 2, mdsp_ui::LegendEdge::TopRight, legendStyle);
    }
    
    // 2D cursor readout (freq + dB) for Log mode with peak snap
    const mdsp_ui::PeakSnapState& peakSnapState = controller.getPeakSnap().state();
    const mdsp_ui::AxisHoverState& freqHoverState = controller.getFreqHover().state();
    const mdsp_ui::AxisHoverState& dbHoverState = controller.getDbHover().state();
    
    // Determine if we should show readout (peak snap active OR axis hover active)
    const bool hasFreq = peakSnapState.snappedActive || freqHoverState.active;
    const bool hasDb = dbHoverState.active;
    
    if ((hasFreq || hasDb) && s.viewMode == 1)
    {
        const juce::Rectangle<float> plotBoundsFloat (geometry.getPlotAreaLeft(), geometry.getPlotAreaTop(), geometry.getPlotAreaWidth(), geometry.getPlotAreaHeight());
        const juce::Rectangle<int> plotBounds (static_cast<int> (geometry.getPlotAreaLeft()),
                                               static_cast<int> (geometry.getPlotAreaTop()),
                                               static_cast<int> (geometry.getPlotAreaWidth()),
                                               static_cast<int> (geometry.getPlotAreaHeight()));
        
        // Determine frequency value (peak snap takes priority)
        float freqHz = 0.0f;
        float freqCursorXPx = 0.0f;
        bool freqActive = false;
        if (peakSnapState.snappedActive)
        {
            freqHz = peakSnapState.snappedFreqHz;
            freqCursorXPx = peakSnapState.snappedXPx - geometry.getPlotAreaLeft(); // Convert to relative
            freqActive = true;
        }
        else if (freqHoverState.active)
        {
            freqHz = freqHoverState.value;
            freqCursorXPx = freqHoverState.cursorPosPx;
            freqActive = true;
        }
        
        // Determine dB value (from axis hover)
        float dbVal = 0.0f;
        float dbCursorYPx = 0.0f;
        bool dbActive = false;
        if (dbHoverState.active)
        {
            dbVal = dbHoverState.value;
            dbCursorYPx = dbHoverState.cursorPosPx;
            dbActive = true;
        }
        else if (peakSnapState.snappedActive)
        {
            // Use peak snap dB value as fallback
            dbVal = peakSnapState.snappedDb;
            dbActive = true;
        }
        
        // Draw vertical cursor line at resolved position (if freq active)
        if (freqActive)
        {
            const float cursorX = mdsp_ui::AxisInteraction::cursorLineX (plotBounds, freqCursorXPx);
            g.setColour (theme.text.withAlpha (0.25f));
            g.drawVerticalLine (static_cast<int> (cursorX), geometry.getPlotAreaTop(), geometry.getPlotAreaTop() + geometry.getPlotAreaHeight());
        }
        
        // Draw horizontal cursor line at resolved position (if db active and snapped and valid frame)
        const bool canDrawCursorY = (dbActive && dbHoverState.active && dbHoverState.snappedTickIndex >= 0) && s.hasValidSpectrumFrame;
        if (canDrawCursorY)
        {
            const float cursorY = geometry.getPlotAreaTop() + dbCursorYPx;
            g.setColour (theme.text.withAlpha (0.25f));
            g.drawHorizontalLine (static_cast<int> (cursorY), geometry.getPlotAreaLeft(), geometry.getPlotAreaLeft() + geometry.getPlotAreaWidth());
        }
        
        // Build ValueReadoutLine array (stack-allocated, max 2 lines)
        mdsp_ui::ValueReadoutLine readoutLines[2];
        int numLines = 0;
        
        if (freqActive)
        {
            readoutLines[numLines].left = "f:";
            readoutLines[numLines].right = mdsp_ui::AxisInteraction::formatFrequencyHz (freqHz);
            readoutLines[numLines].enabled = true;
            numLines++;
        }
        
        if (dbActive)
        {
            readoutLines[numLines].left = "dB:";
            readoutLines[numLines].right = mdsp_ui::AxisInteraction::formatDb (dbVal);
            readoutLines[numLines].enabled = true;
            numLines++;
        }
        
        if (numLines > 0)
        {
            // Measure text to compute accurate frame bounds (using GlyphArrangement like ValueReadoutRenderer)
            const juce::Font font (juce::FontOptions().withHeight (10.0f));
            g.setFont (font);
            
            float maxLineWidth = 0.0f;
            for (int i = 0; i < numLines; ++i)
            {
                if (!readoutLines[i].enabled)
                    continue;
                
                float lineWidth = 0.0f;
                if (readoutLines[i].left.isEmpty())
                {
                    juce::GlyphArrangement glyphs;
                    glyphs.addFittedText (font, readoutLines[i].right, 0.0f, 0.0f, 10000.0f, 10.0f, juce::Justification::left, 1);
                    lineWidth = glyphs.getBoundingBox (0, -1, true).getWidth();
                }
                else
                {
                    juce::GlyphArrangement leftGlyphs;
                    leftGlyphs.addFittedText (font, readoutLines[i].left, 0.0f, 0.0f, 10000.0f, 10.0f, juce::Justification::left, 1);
                    const float leftWidth = leftGlyphs.getBoundingBox (0, -1, true).getWidth();
                    
                    juce::GlyphArrangement rightGlyphs;
                    rightGlyphs.addFittedText (font, readoutLines[i].right, 0.0f, 0.0f, 10000.0f, 10.0f, juce::Justification::right, 1);
                    const float rightWidth = rightGlyphs.getBoundingBox (0, -1, true).getWidth();
                    
                    lineWidth = leftWidth + rightWidth + 20.0f; // Add gap between left and right
                }
                maxLineWidth = std::max (maxLineWidth, lineWidth);
            }
            
            // Compute frame dimensions
            const float padding = 4.0f;
            const float lineHeight = 12.0f;
            const float lineGap = 2.0f;
            const float frameWidth = maxLineWidth + (padding * 2.0f);
            const float frameHeight = (static_cast<float> (numLines) * lineHeight) +
                                     (static_cast<float> (numLines - 1) * lineGap) +
                                     (padding * 2.0f);
            
            // Compute frame bounds: anchor near cursor, clamp to plot
            // Use cursor position from frequency (X) if available, otherwise use plot center
            const float anchorX = freqActive ? (geometry.getPlotAreaLeft() + freqCursorXPx) : (geometry.getPlotAreaLeft() + geometry.getPlotAreaWidth() * 0.5f);
            const float anchorY = dbActive ? (geometry.getPlotAreaTop() + dbCursorYPx) : (geometry.getPlotAreaTop() + geometry.getPlotAreaHeight() * 0.5f);
            
            // Position readout box: bottom-left of cursor, offset slightly
            float readoutX = anchorX + 10.0f;
            float readoutY = anchorY - frameHeight - 5.0f;
            
            // Clamp to plot bounds
            readoutX = juce::jmax (geometry.getPlotAreaLeft() + 5.0f, juce::jmin (readoutX, geometry.getPlotAreaLeft() + geometry.getPlotAreaWidth() - frameWidth - 5.0f));
            readoutY = juce::jmax (geometry.getPlotAreaTop() + 5.0f, juce::jmin (readoutY, geometry.getPlotAreaTop() + geometry.getPlotAreaHeight() - frameHeight - 5.0f));
            
            const juce::Rectangle<float> frameBounds (readoutX, readoutY, frameWidth, frameHeight);
            
            // Build ValueReadoutStyle (match existing tooltip style)
            mdsp_ui::ValueReadoutStyle readoutStyle;
            readoutStyle.fontHeightPx = 10.0f;
            readoutStyle.paddingPx = 4.0f;
            readoutStyle.cornerRadiusPx = 3.0f;
            readoutStyle.frameFillAlpha = 0.9f;
            readoutStyle.frameBorderAlpha = 0.9f;
            readoutStyle.textAlpha = 1.0f;
            readoutStyle.disabledTextAlpha = 0.55f;
            readoutStyle.drawFrame = true;
            readoutStyle.clipToFrame = true;
            
            // Draw readout using ValueReadoutRenderer
            mdsp_ui::ValueReadoutRenderer::drawAt (g, frameBounds, theme, readoutLines, numLines, readoutStyle);
        }
    }
}

//==============================================================================
void RTADisplayRenderer::paintFFTMode (juce::Graphics& g,
                                        const RenderState& s,
                                        const RTAGeometry& geometry,
                                        const RTADisplayController& controller,
                                        const mdsp_ui::Theme& theme,
                                        float displayGainDb,
                                        rta::TiltMode tiltMode,
                                        const RTADisplayModel::TraceConfig& traceConfig,
                                        RTADisplayModel& modelMutable)
{
    // Ensure paths are built
    modelMutable.ensurePathsBuilt (geometry, displayGainDb, tiltMode, traceConfig);
    
    if (!modelMutable.arePathsValid())
    {
        DBG("paintFFTMode: paths still invalid after rebuild");
        return;
    }

    const float viewWidth = geometry.getPlotAreaWidth();
    
    g.saveState();
    g.reduceClipRegion (juce::Rectangle<int> (static_cast<int> (geometry.getPlotAreaLeft()),
                                               static_cast<int> (geometry.getPlotAreaTop()),
                                               static_cast<int> (geometry.getPlotAreaWidth()),
                                               static_cast<int> (geometry.getPlotAreaHeight())));
    
    const juce::Colour colSide = juce::Colour(0xffe91e63);
    const juce::Colour colMid  = juce::Colour(0xff00bcd4);
    const juce::Colour colLeft = juce::Colour(0xff4caf50);
    const juce::Colour colRight = juce::Colour(0xfff44336);
    const juce::Colour colStereo = juce::Colour(0xff9c27b0);
    const juce::Colour colMono = juce::Colour(0xffffeb3b);
    const juce::Colour colRms  = theme.accent;

    const auto& c = traceConfig;
    if (c.showSide) drawSilkTrace(g, modelMutable.getCachedSidePath(),  colSide,   1.8f, viewWidth, false, 1.0f, false);
    if (c.showMid)  drawSilkTrace(g, modelMutable.getCachedMidPath(),   colMid,    1.8f, viewWidth, false, 1.0f, false);
    if (c.showL)    drawSilkTrace(g, modelMutable.getCachedLPath(),     colLeft,   1.8f, viewWidth, false, 1.0f, false);
    if (c.showR)    drawSilkTrace(g, modelMutable.getCachedRPath(),     colRight,  1.8f, viewWidth, false, 1.0f, false);
    
    if (c.showLR && !modelMutable.getCachedStereoPath().isEmpty()) 
        drawSilkTrace(g, modelMutable.getCachedStereoPath(), colStereo, 1.8f, viewWidth, false, 1.0f, false);

    if (c.showMono) drawSilkTrace(g, modelMutable.getCachedMonoPath(), colMono, 1.8f, viewWidth, false, 1.0f, false);

    if (c.showRMS)
    {
        if (!modelMutable.getCachedFftPath().isEmpty())
        {
            juce::Path fillPath = modelMutable.getCachedFftPath();
            const auto pathBounds = fillPath.getBounds();
            if (pathBounds.getWidth() > 1.0f)
            {
                // Close the path: line to bottom-right, then bottom-left, then back to start
                const float bottomY = geometry.getPlotAreaTop() + geometry.getPlotAreaHeight();
                fillPath.lineTo (pathBounds.getRight(), bottomY);
                fillPath.lineTo (pathBounds.getX(), bottomY);
                fillPath.closeSubPath();
                
                // Create vertical gradient (trace color at top, transparent at bottom)
                juce::ColourGradient gradient (
                    colRms.withAlpha (0.35f),  // Top: semi-transparent trace color
                    0.0f, geometry.getPlotAreaTop(),
                    colRms.withAlpha (0.05f),  // Bottom: nearly transparent
                    0.0f, bottomY,
                    false);  // Not radial
                
                g.setGradientFill (gradient);
                g.fillPath (fillPath);
            }
        }

        drawSilkTrace(g, modelMutable.getCachedFftPath(), colRms, 2.0f, viewWidth, false, 1.0f, false);
    }
    
    if (!modelMutable.getCachedPeakPath().isEmpty())
    {
        juce::Path peakFillPath = modelMutable.getCachedPeakPath();
        const auto peakBounds = peakFillPath.getBounds();
        if (peakBounds.getWidth() > 1.0f)
        {
            const float bottomY = geometry.getPlotAreaTop() + geometry.getPlotAreaHeight();
            peakFillPath.lineTo (peakBounds.getRight(), bottomY);
            peakFillPath.lineTo (peakBounds.getX(), bottomY);
            peakFillPath.closeSubPath();
            
            juce::ColourGradient peakGradient (
                theme.seriesPeak.withAlpha (0.15f),
                0.0f, geometry.getPlotAreaTop(),
                theme.seriesPeak.withAlpha (0.02f),
                0.0f, bottomY,
                false);
            
            g.setGradientFill (peakGradient);
            g.fillPath (peakFillPath);
        }
    }
    
    if (!modelMutable.getCachedPeakPath().isEmpty())
        drawSilkTrace(g, modelMutable.getCachedPeakPath(), theme.seriesPeak, 1.2f, viewWidth, true, 1.2f, true);

    const int weightingMode = c.weightingMode;
    RTADisplayModel::RenderConfigKey currentKey;
    currentKey.fftSize = s.fftSize;
    currentKey.sampleRate = s.sampleRate;
    currentKey.minHz = s.minHz;
    currentKey.maxHz = s.maxHz;
    currentKey.plotWidth = geometry.getPlotAreaWidth();
    currentKey.isLog = true;

    if (weightingMode != modelMutable.getLastWeightingMode() || currentKey != modelMutable.getLastWeightingKey())
    {
        juce::Path weightingPath;
        if (weightingMode > 0)
        {
            std::vector<juce::Point<float>> wPts;
            wPts.reserve (1024);
            const float startX = geometry.getPlotAreaLeft();
            const float endX = geometry.getPlotAreaLeft() + geometry.getPlotAreaWidth();
            const float step = 1.0f;
            
            for (float x = startX; x <= endX; x += step)
            {
                const float norm = (x - geometry.getPlotAreaLeft()) / geometry.getPlotAreaWidth();
                const float logMin = std::log10 (s.minHz);
                const float logMax = std::log10 (s.maxHz);
                const float logRange = logMax - logMin;
                const float logFreq = logMin + norm * logRange;
                const float freq = std::pow (10.0f, logFreq);
                
                float db = 0.0f;
                if (weightingMode == 1) db = mdsp_ui::rta::getAWeightingDb (freq);
                else if (weightingMode == 2) db = getBS468WeightingDb (freq);
                
                const float y = geometry.dbToYPx (db + displayGainDb);
                wPts.emplace_back (x, y);
            }
            
            if (!wPts.empty())
            {
                weightingPath.startNewSubPath (wPts[0]);
                for (size_t k = 1; k < wPts.size(); ++k)
                    weightingPath.lineTo (wPts[k]);
            }
        }
        modelMutable.setWeightingPath (weightingPath, weightingMode, currentKey);
    }

    // 2. Selection Overlay
    if (controller.isSelectionActive())
    {
        if (!juce::ModifierKeys::getCurrentModifiersRealtime().isAnyMouseButtonDown())
        {
            const_cast<RTADisplayController&>(controller).clearSelectionIfStuck();
        }
        else
        {
            g.setColour (theme.accent.withAlpha (0.2f));
            g.fillRect (controller.getSelectionRect());
            g.setColour (theme.accent.withAlpha (0.6f));
            g.drawRect (controller.getSelectionRect());
        }
    }

    // 3. Legend
    {
        const juce::Rectangle<float> legendPlotBounds (geometry.getPlotAreaLeft(), geometry.getPlotAreaTop(), geometry.getPlotAreaWidth(), geometry.getPlotAreaHeight());
        
        std::vector<mdsp_ui::LegendItem> legendItems;
        legendItems.reserve(9); // Max possible items

        // 1. RMS (Main FFT)
        if (c.showRMS)
            legendItems.push_back({ "RMS", colRms, true });

        if (!modelMutable.getCachedPeakPath().isEmpty())
            legendItems.push_back({ "Peak", theme.seriesPeak, true });
        
        if (c.showL && !s.lDbL.empty())
            legendItems.push_back({ "L", colLeft, true });

        if (c.showR && !s.lDbR.empty())
            legendItems.push_back({ "R", colRight, true });

        if (c.showMid && !s.midDb.empty())
            legendItems.push_back({ "Mid", colMid, true });

        if (c.showSide && !s.sideDb.empty())
            legendItems.push_back({ "Side", colSide, true });

        if (c.showMono && !s.monoDb.empty())
            legendItems.push_back({ "Mono", colMono, true });
            
        if (c.showLR && !modelMutable.getCachedStereoPath().isEmpty())
             legendItems.push_back({ "Stereo", colStereo, true });
        
        if (!legendItems.empty())
        {
            mdsp_ui::LegendStyle legendStyle;
            legendStyle.fontHeightPx = 10.0f;
            legendStyle.drawFrame = true;
            legendStyle.frameCornerRadiusPx = 4.0f;
            legendStyle.frameFillAlpha = 0.80f;
            legendStyle.frameBorderAlpha = 0.90f;
            
            mdsp_ui::LegendRenderer::draw (g, legendPlotBounds, theme, legendItems.data(), 
                                          static_cast<int>(legendItems.size()), 
                                          mdsp_ui::LegendEdge::TopRight, legendStyle);
        }
    }

    // 4. Session Marker
    if (s.sessionMarkerVisible && s.viewMode == 0)
    {
        const float x = geometry.freqHzToXPx (s.fftSize > 0 ? static_cast<float> (s.sessionMarkerBin * s.sampleRate / s.fftSize) : 0.0f);
        if (x >= geometry.getPlotAreaLeft() && x <= (geometry.getPlotAreaLeft() + geometry.getPlotAreaWidth()))
        {
             const float y = geometry.dbToYPx (s.sessionMarkerDb + displayGainDb);
             g.setColour (theme.seriesPeak.brighter(0.3f));
             g.drawLine (x, y - 4.0f, x, y + 4.0f, 2.0f);
             g.fillEllipse (x - 2.0f, y - 2.0f, 4.0f, 4.0f);
        }
    }

    // 5. FFT crosshair and readout - always visible when hovering, smooth tracking
    const float fftMouseX = controller.getFftHoverMouseXpx();
    const float fftSnappedX = controller.getFftHoverSnappedXpx();
    const float crosshairX = (std::isfinite (fftMouseX) && fftMouseX >= geometry.getPlotAreaLeft() && fftMouseX <= geometry.getPlotAreaLeft() + geometry.getPlotAreaWidth())
        ? fftMouseX : fftSnappedX;
    if (controller.isFftHoverActive() && std::isfinite (crosshairX))
    {
        const juce::Colour crosshairCol = theme.text.withAlpha (0.45f);
        g.setColour (crosshairCol);
        g.drawVerticalLine (static_cast<int> (crosshairX), geometry.getPlotAreaTop(), geometry.getPlotAreaTop() + geometry.getPlotAreaHeight());

        float crosshairY = 0.0f;
        if (controller.isHoverDbHasValue())
        {
            crosshairY = controller.getFftHoverSnappedYpx();
            if (std::isfinite (crosshairY))
            { /* use it */ }
            else if (std::isfinite (controller.getHoverYSmoothPx()))
                crosshairY = controller.getHoverYSmoothPx();
            else
            {
                const float yFromTarget = geometry.dbToYPx (controller.getHoverDbTarget() + displayGainDb);
                crosshairY = std::isfinite (yFromTarget) ? yFromTarget : (geometry.getPlotAreaTop() + geometry.getPlotAreaHeight() * 0.5f);
            }
        }
        else
        {
            crosshairY = geometry.dbToYPx (s.bottomDb + displayGainDb);
            if (!std::isfinite (crosshairY))
                crosshairY = geometry.getPlotAreaTop() + geometry.getPlotAreaHeight();
        }
        const bool canDrawHoverY = controller.isFftHoverActive() && controller.isFftHoverDbValid() && std::isfinite (controller.getHoverYSmoothPx()) && s.hasValidSpectrumFrame;
        if (canDrawHoverY)
            g.drawHorizontalLine (static_cast<int> (crosshairY), geometry.getPlotAreaLeft(), geometry.getPlotAreaLeft() + geometry.getPlotAreaWidth());
        g.fillEllipse (crosshairX - 2.5f, crosshairY - 2.5f, 5.0f, 5.0f);

        const juce::String& fftReadoutText = controller.getFftHoverReadoutText();
        const float fftReadoutWidth = controller.getFftHoverReadoutWidth();
        if (fftReadoutText.isNotEmpty() && fftReadoutWidth > 0.0f)
        {
            const float pad = 6.0f;
            const float fontH = 10.0f;
            g.setFont (juce::FontOptions().withHeight (fontH));
            const float tw = fftReadoutWidth;
            const float th = fontH + 4.0f;
            const float anchorY = crosshairY;
            float rx = crosshairX + 8.0f;
            float ry = anchorY - th - 4.0f;
            if (rx + tw + pad > geometry.getPlotAreaLeft() + geometry.getPlotAreaWidth())
                rx = crosshairX - tw - pad - 8.0f;
            if (ry < geometry.getPlotAreaTop())
                ry = anchorY + 6.0f;
            if (ry + th > geometry.getPlotAreaTop() + geometry.getPlotAreaHeight())
                ry = geometry.getPlotAreaTop() + geometry.getPlotAreaHeight() - th - 4.0f;
            if (rx < geometry.getPlotAreaLeft() + 2.0f)
                rx = geometry.getPlotAreaLeft() + 2.0f;
            const juce::Rectangle<float> readoutRect (rx, ry, tw + pad * 2.0f, th);
            g.setColour (theme.background.withAlpha (0.92f));
            g.fillRoundedRectangle (readoutRect, 3.0f);
            g.setColour (theme.text.withAlpha (0.9f));
            g.drawRoundedRectangle (readoutRect, 3.0f, 1.0f);
            g.setColour (theme.text);
            g.drawText (fftReadoutText, readoutRect.reduced (pad, 2.0f), juce::Justification::centredLeft, true);
        }
    }

    g.restoreState();
}

//==============================================================================
void RTADisplayRenderer::paintInteractionOverlays (juce::Graphics& g,
                                                    const RenderState& s,
                                                    const RTAGeometry& geometry,
                                                    const RTADisplayController& controller,
                                                    const mdsp_ui::Theme& theme,
                                                    float displayGainDb)
{
    g.saveState();
    g.reduceClipRegion (juce::Rectangle<int> (static_cast<int> (geometry.getPlotAreaLeft()),
                                               static_cast<int> (geometry.getPlotAreaTop()),
                                               static_cast<int> (geometry.getPlotAreaWidth()),
                                               static_cast<int> (geometry.getPlotAreaHeight())));

    if (controller.isSelectionActive())
    {
        if (!juce::ModifierKeys::getCurrentModifiersRealtime().isAnyMouseButtonDown())
            const_cast<RTADisplayController&>(controller).clearSelectionIfStuck();
        else
        {
            g.setColour (theme.accent.withAlpha (0.2f));
            g.fillRect (controller.getSelectionRect());
            g.setColour (theme.accent.withAlpha (0.6f));
            g.drawRect (controller.getSelectionRect());
        }
    }

    if (s.sessionMarkerVisible && s.viewMode == 0)
    {
        const float x = geometry.freqHzToXPx (s.fftSize > 0 ? static_cast<float> (s.sessionMarkerBin * s.sampleRate / s.fftSize) : 0.0f);
        if (x >= geometry.getPlotAreaLeft() && x <= (geometry.getPlotAreaLeft() + geometry.getPlotAreaWidth()))
        {
            const float y = geometry.dbToYPx (s.sessionMarkerDb + displayGainDb);
            g.setColour (theme.seriesPeak.brighter (0.3f));
            g.drawLine (x, y - 4.0f, x, y + 4.0f, 2.0f);
            g.fillEllipse (x - 2.0f, y - 2.0f, 4.0f, 4.0f);
        }
    }

    const float mouseX = controller.getFftHoverMouseXpx();
    const float snappedX = controller.getFftHoverSnappedXpx();
    const float crosshairX = (std::isfinite (mouseX) && mouseX >= geometry.getPlotAreaLeft() && mouseX <= geometry.getPlotAreaLeft() + geometry.getPlotAreaWidth())
        ? mouseX : snappedX;
    if (controller.isFftHoverActive() && std::isfinite (crosshairX))
    {
        g.setColour (theme.text.withAlpha (0.45f));
        g.drawVerticalLine (static_cast<int> (crosshairX), geometry.getPlotAreaTop(), geometry.getPlotAreaTop() + geometry.getPlotAreaHeight());
        float crosshairY = 0.0f;
        if (controller.isHoverDbHasValue())
        {
            crosshairY = controller.getFftHoverSnappedYpx();
            const float smoothY = controller.getHoverYSmoothPx();
            const float targetY = geometry.dbToYPx (controller.getHoverDbTarget() + displayGainDb);
            if (!std::isfinite (crosshairY) && std::isfinite (smoothY)) crosshairY = smoothY;
            else if (!std::isfinite (crosshairY)) crosshairY = targetY;
            if (!std::isfinite (crosshairY)) crosshairY = geometry.getPlotAreaTop() + geometry.getPlotAreaHeight() * 0.5f;
        }
        else
        {
            crosshairY = geometry.dbToYPx (s.bottomDb + displayGainDb);
            if (!std::isfinite (crosshairY)) crosshairY = geometry.getPlotAreaTop() + geometry.getPlotAreaHeight();
        }
        const bool canDrawHoverY = controller.isFftHoverActive() && controller.isFftHoverDbValid() && std::isfinite (controller.getHoverYSmoothPx()) && s.hasValidSpectrumFrame;
        if (canDrawHoverY)
            g.drawHorizontalLine (static_cast<int> (crosshairY), geometry.getPlotAreaLeft(), geometry.getPlotAreaLeft() + geometry.getPlotAreaWidth());
        g.fillEllipse (crosshairX - 2.5f, crosshairY - 2.5f, 5.0f, 5.0f);
        const juce::String& readoutText = controller.getFftHoverReadoutText();
        const float readoutWidth = controller.getFftHoverReadoutWidth();
        if (readoutText.isNotEmpty() && readoutWidth > 0.0f)
        {
            const float pad = 6.0f, fontH = 10.0f;
            g.setFont (juce::FontOptions().withHeight (fontH));
            const float tw = readoutWidth, th = fontH + 4.0f;
            float rx = crosshairX + 8.0f, ry = crosshairY - th - 4.0f;
            if (rx + tw + pad > geometry.getPlotAreaLeft() + geometry.getPlotAreaWidth()) rx = crosshairX - tw - pad - 8.0f;
            if (ry < geometry.getPlotAreaTop()) ry = crosshairY + 6.0f;
            if (ry + th > geometry.getPlotAreaTop() + geometry.getPlotAreaHeight()) ry = geometry.getPlotAreaTop() + geometry.getPlotAreaHeight() - th - 4.0f;
            if (rx < geometry.getPlotAreaLeft() + 2.0f) rx = geometry.getPlotAreaLeft() + 2.0f;
            const juce::Rectangle<float> readoutRect (rx, ry, tw + pad * 2.0f, th);
            g.setColour (theme.background.withAlpha (0.92f));
            g.fillRoundedRectangle (readoutRect, 3.0f);
            g.setColour (theme.text.withAlpha (0.9f));
            g.drawRoundedRectangle (readoutRect, 3.0f, 1.0f);
            g.setColour (theme.text);
            g.drawText (readoutText, readoutRect.reduced (pad, 2.0f), juce::Justification::centredLeft, true);
        }
    }
    g.restoreState();
}

//==============================================================================
void RTADisplayRenderer::paintLegacyTraceOverlays (juce::Graphics& g,
                                                    const RenderState& s,
                                                    const RTAGeometry& geometry,
                                                    const RTADisplayModel& model,
                                                    const mdsp_ui::Theme& theme,
                                                    const RTADisplayModel::TraceConfig& traceConfig)
{
    if (!model.arePathsValid()) return;

    const float viewWidth = geometry.getPlotAreaWidth();
    g.saveState();
    g.reduceClipRegion (juce::Rectangle<int> (static_cast<int> (geometry.getPlotAreaLeft()),
                                               static_cast<int> (geometry.getPlotAreaTop()),
                                               static_cast<int> (geometry.getPlotAreaWidth()),
                                               static_cast<int> (geometry.getPlotAreaHeight())));

    const juce::Colour colSide  = juce::Colour (0xffe91e63);
    const juce::Colour colMid   = juce::Colour (0xff00bcd4);
    const juce::Colour colLeft  = juce::Colour (0xff4caf50);
    const juce::Colour colRight = juce::Colour (0xfff44336);
    const juce::Colour colStereo = juce::Colour (0xff9c27b0);
    const juce::Colour colMono  = juce::Colour (0xffffeb3b);
    const juce::Colour colRms   = theme.accent;

    const auto& c = traceConfig;
    if (c.showSide) drawSilkTrace (g, model.getCachedSidePath(),   colSide,  1.8f, viewWidth, false, 1.0f, false);
    if (c.showMid)  drawSilkTrace (g, model.getCachedMidPath(),    colMid,   1.8f, viewWidth, false, 1.0f, false);
    if (c.showL)    drawSilkTrace (g, model.getCachedLPath(),      colLeft,  1.8f, viewWidth, false, 1.0f, false);
    if (c.showR)    drawSilkTrace (g, model.getCachedRPath(),     colRight, 1.8f, viewWidth, false, 1.0f, false);
    if (c.showLR && !model.getCachedStereoPath().isEmpty())
        drawSilkTrace (g, model.getCachedStereoPath(), colStereo, 1.8f, viewWidth, false, 1.0f, false);
    if (c.showMono) drawSilkTrace (g, model.getCachedMonoPath(),  colMono,  1.8f, viewWidth, false, 1.0f, false);

    if (c.showRMS && !model.getCachedFftPath().isEmpty())
    {
        juce::Path fillPath = model.getCachedFftPath();
        const auto pathBounds = fillPath.getBounds();
        if (pathBounds.getWidth() > 1.0f)
        {
            const float bottomY = geometry.getPlotAreaTop() + geometry.getPlotAreaHeight();
            fillPath.lineTo (pathBounds.getRight(), bottomY);
            fillPath.lineTo (pathBounds.getX(), bottomY);
            fillPath.closeSubPath();
            juce::ColourGradient gradient (colRms.withAlpha (0.35f), 0.0f, geometry.getPlotAreaTop(),
                                           colRms.withAlpha (0.05f), 0.0f, bottomY, false);
            g.setGradientFill (gradient);
            g.fillPath (fillPath);
        }
    }

    if (!model.getCachedPeakPath().isEmpty())
    {
        juce::Path peakFillPath = model.getCachedPeakPath();
        const auto peakBounds = peakFillPath.getBounds();
        if (peakBounds.getWidth() > 1.0f)
        {
            const float bottomY = geometry.getPlotAreaTop() + geometry.getPlotAreaHeight();
            peakFillPath.lineTo (peakBounds.getRight(), bottomY);
            peakFillPath.lineTo (peakBounds.getX(), bottomY);
            peakFillPath.closeSubPath();
            juce::ColourGradient peakGradient (theme.seriesPeak.withAlpha (0.15f), 0.0f, geometry.getPlotAreaTop(),
                                              theme.seriesPeak.withAlpha (0.02f), 0.0f, bottomY, false);
            g.setGradientFill (peakGradient);
            g.fillPath (peakFillPath);
        }
    }

    const juce::Rectangle<float> legendPlotBounds (geometry.getPlotAreaLeft(), geometry.getPlotAreaTop(), geometry.getPlotAreaWidth(), geometry.getPlotAreaHeight());
    std::vector<mdsp_ui::LegendItem> legendItems;
    legendItems.reserve (9);
    if (c.showRMS) legendItems.push_back ({ "RMS", colRms, true });
    if (!model.getCachedPeakPath().isEmpty()) legendItems.push_back ({ "Peak", theme.seriesPeak, true });
    if (c.showL && !s.lDbL.empty()) legendItems.push_back ({ "L", colLeft, true });
    if (c.showR && !s.lDbR.empty()) legendItems.push_back ({ "R", colRight, true });
    if (c.showMid && !s.midDb.empty()) legendItems.push_back ({ "Mid", colMid, true });
    if (c.showSide && !s.sideDb.empty()) legendItems.push_back ({ "Side", colSide, true });
    if (c.showMono && !s.monoDb.empty()) legendItems.push_back ({ "Mono", colMono, true });
    if (c.showLR && !model.getCachedStereoPath().isEmpty()) legendItems.push_back ({ "Stereo", colStereo, true });
    if (!legendItems.empty())
    {
        mdsp_ui::LegendStyle legendStyle;
        legendStyle.fontHeightPx = 10.0f;
        legendStyle.drawFrame = true;
        legendStyle.frameCornerRadiusPx = 4.0f;
        legendStyle.frameFillAlpha = 0.80f;
        legendStyle.frameBorderAlpha = 0.90f;
        mdsp_ui::LegendRenderer::draw (g, legendPlotBounds, theme, legendItems.data(),
                                      static_cast<int> (legendItems.size()),
                                      mdsp_ui::LegendEdge::TopRight, legendStyle);
    }

    g.restoreState();
}
