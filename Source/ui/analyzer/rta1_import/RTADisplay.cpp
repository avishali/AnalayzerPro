#include "RTADisplay.h"
#include <mdsp_ui/Theme.h>
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

// NOTE (audit trail):
// "RTADisplay.cpp touched for build fix only (API enum rename); no functional changes."

#define MDSP_TRACE_SHIMMER_V2 0       // Default OFF: Subtle animated shimmer on peak highlight
#define MDSP_TRACE_GLOW_FALLOFF_V2 1  // Default ON:  Frequency-weighted glow falloff (less muddy LF)

//==============================================================================
// TRACE_VISUAL_POLISH_V1: Silk trace rendering with glow + AA + perceptual thickness
//==============================================================================
static void drawSilkTrace (juce::Graphics& g,
                           const juce::Path& path,
                           juce::Colour coreColour,
                           float baseThicknessPx,
                           float viewportWidth,
                           bool isPeakTrace,
                           float energyMul, // New V2: boosts glow
                           bool useShimmer) // New V2: animated highlight
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
RTADisplay::RTADisplay()
    : freqHover_ ({})  // Style will be set via buildFreqAxisConfig if needed
    , dbHover_ ({})    // Style will be set via buildDbAxisConfig if needed
    , peakSnap_ ([]() {
        mdsp_ui::PeakSnapStyle style;
        style.snapPx = 8.0f;
        style.releasePx = 16.0f;
        style.searchRadiusPx = 20.0f;
        style.epsPosPx = 0.5f;
        style.epsValue = 0.1f;
        return style;
    }())
{
    constexpr float kGridMinDb = -120.0f;

    // Initialize state with defaults
    state.minHz = 20.0f;
    state.maxHz = 20000.0f;
    state.topDb = 0.0f;
    state.bottomDb = kGridMinDb;
    // Initialize coordinate mapping factors
    logMinFreq = std::log10 (state.minHz);
    logFreqRange = std::log10 (state.maxHz) - logMinFreq;
    dbRange = state.topDb - state.bottomDb;
}

RTADisplay::~RTADisplay() = default;

//==============================================================================
void RTADisplay::setBandData (const std::vector<float>& currentDb, const std::vector<float>* peakDbNullable)
{
    // B1: Every setter updates state fields and calls repaint()
    state.bandsDb = currentDb;
    if (peakDbNullable != nullptr)
        state.bandsPeakDb = *peakDbNullable;
    else
        state.bandsPeakDb.clear();
    state.status = DataStatus::Ok;
    repaint();
}

void RTADisplay::setViewMode (int mode)
{
    if (state.viewMode != mode)
    {
        state.viewMode = mode;
#if JUCE_DEBUG
        // Keep debug overlay in sync with the active view mode.
        debugViewMode = mode;
#endif
        geometryValid = false;
        hoveredBandIndex = -1;  // A3: Clear hover on view change
        repaint();
    }
}

void RTADisplay::setFFTData (const std::vector<float>& fftBinsDb, const std::vector<float>* peakBinsDbNullable)
{
    // B1: Every setter updates state fields and calls repaint()
    state.fftDb = fftBinsDb;
    if (peakBinsDbNullable != nullptr)
        state.fftPeakDb = *peakBinsDbNullable;
    else
        state.fftPeakDb.clear();
    state.status = DataStatus::Ok;
    repaint();
}

void RTADisplay::setLogData (const std::vector<float>& logBandsDb, const std::vector<float>* peakBandsDbNullable)
{
    // B1: Every setter updates state fields and calls repaint()
    // B4: No logCentersHz stored - will compute from index on-the-fly in paint
    state.logDb = logBandsDb;
    if (peakBandsDbNullable != nullptr)
        state.logPeakDb = *peakBandsDbNullable;
    else
        state.logPeakDb.clear();
    state.status = DataStatus::Ok;
    repaint();
}

void RTADisplay::setBandCenters (const std::vector<float>& centersHz)
{
    // B1: Every setter updates state fields and calls repaint()
    // B7: updateGeometry() called from setter (message thread), not from paint
    state.bandCentersHz = centersHz;
    geometryValid = false;  // A3: Mark geometry dirty
    hoveredBandIndex = -1;  // A3: Clear hover on centers change
    updateGeometry();  // B7: Allowed here (message thread)
    repaint();
}

// B4: setLogCenters() removed - log centers computed on-the-fly from index
// This method is no longer needed, but kept for API compatibility (no-op)
void RTADisplay::setLogCenters (const std::vector<float>&)
{
    // B4: Log mode computes centers on-the-fly, no storage needed
    // No-op for API compatibility
}

void RTADisplay::setFftMeta (double sampleRate, int fftSize)
{
    // B1: Every setter updates state
    state.sampleRate = sampleRate;
    state.fftSize = fftSize;
    repaint();
}

void RTADisplay::setFrequencyRange (float minHz, float maxHz)
{
    // B1: Every setter updates state
    state.minHz = minHz;
    state.maxHz = maxHz;
    logMinFreq = std::log10 (state.minHz);
    logFreqRange = std::log10 (state.maxHz) - logMinFreq;
    geometryValid = false;
    repaint();
}

void RTADisplay::setDbRange (float top, float bottom)
{
    // B1: Every setter updates state
    state.topDb = top;
    state.bottomDb = bottom;
    dbRange = state.topDb - state.bottomDb;
    repaint();
}

void RTADisplay::setNoData (const juce::String& reason)
{
    // B1: Every setter updates state
    state.status = DataStatus::NoData;
    state.noDataReason = reason;
    repaint();
}

void RTADisplay::setDisplayGainDb (float db)
{
    displayGainDb = juce::jlimit (-24.0f, 24.0f, db);
#if JUCE_DEBUG
    DBG ("DisplayGain=" << displayGainDb << "dB");
#endif
    repaint();
}

void RTADisplay::setTiltMode (TiltMode mode)
{
    tiltMode = mode;
    repaint();
}

void RTADisplay::setTraceConfig (const TraceConfig& config)
{
    // B1: Every setter updates state
    traceConfig_ = config;
    repaint();
}

void RTADisplay::setLRPowerData (const float* powerL, const float* powerR, int binCount)
{
    // B1: Every setter updates state fields and calls repaint()
    
    // Guard: invalid inputs
    if (powerL == nullptr || powerR == nullptr || binCount <= 0)
    {
        state.lrBinCount = 0;
        state.lDbL.clear();
        state.lDbR.clear();
        state.stereoDb.clear();
        state.monoDb.clear();
        state.midDb.clear();
        state.sideDb.clear();
        state.hasValidMultiTraceData = false;
        return; 
    }
    
    // Update bin count and storage
    state.lrBinCount = binCount;
    if (state.lDbL.size() != static_cast<size_t> (binCount))
    {
        // Allocation allowed only when bin count changes (amortized)
        size_t sz = static_cast<size_t> (binCount);
        state.lDbL.resize (sz);
        state.lDbR.resize (sz);
        state.stereoDb.resize (sz);
        state.monoDb.resize (sz);
        state.midDb.resize (sz);
        state.sideDb.resize (sz);
    }
    
    constexpr float kMinPower = 1.0e-20f; // Stable floor
    state.hasValidMultiTraceData = true;
    
    // Compute Derived Traces (L, R, Stereo, Mono, Mid, Side) using Magnitude Domain
    // SOP Step 2: "Compute derived mags... convert back to power... convert to dB"
    for (int i = 0; i < binCount; ++i)
    {
        const float pL = std::max (powerL[i], kMinPower);
        const float pR = std::max (powerR[i], kMinPower);
        
        // 1. Magnitude
        const float magL = std::sqrt (pL);
        const float magR = std::sqrt (pR);
        
        // 2. Derive Mags
        // Stereo: max(L, R) envelope for visibility
        const float magStereo = std::max (magL, magR);
        const float magMono   = 0.5f * (magL + magR); 
        const float magMid    = 0.5f * (magL + magR); 
        const float magSide   = 0.5f * std::abs (magL - magR); // Abs for magnitude
        
        // 3. Power + dB
        size_t idx = static_cast<size_t>(i);
        
        // L / R
        state.lDbL[idx] = 10.0f * std::log10 (pL);
        state.lDbR[idx] = 10.0f * std::log10 (pR);
        
        // Stereo
        state.stereoDb[idx] = 10.0f * std::log10 (std::max (magStereo * magStereo, kMinPower));
        
        // Mono
        state.monoDb[idx] = 10.0f * std::log10 (std::max (magMono * magMono, kMinPower));
        
        // Mid
        state.midDb[idx] = 10.0f * std::log10 (std::max (magMid * magMid, kMinPower));
        
        // Side
        state.sideDb[idx] = 10.0f * std::log10 (std::max (magSide * magSide, kMinPower));
    }
    
    // Repaint is triggered by the caller (AnalyzerDisplayView) or implicitly here?
    // The pattern in this file is that setters call repaint(), but AnalyzerDisplayView 
    // also calls repaint() at the end of the update block. 
    // We'll call it here to be consistent with B1 rule ("Every setter updates state... and calls repaint").
    repaint();
}
float RTADisplay::computeTiltDb (float freqHz) const
{
    // For DC (i==0) or very low frequencies, no tilt
    if (freqHz <= 0.0f)
        return 0.0f;
    
    // Clamp frequency to minimum before log2
    const float clampedFreq = juce::jmax (1.0f, freqHz);
    
    // Reference frequency: 1000 Hz
    constexpr float f0 = 1000.0f;
    
    // Compute octaves from reference
    const float octaves = std::log2 (clampedFreq / f0);
    
    // Apply tilt based on mode
    float slopeDbPerOct = 0.0f;
    switch (tiltMode)
    {
        case TiltMode::Flat:
            slopeDbPerOct = 0.0f;
            break;
        case TiltMode::Pink:
            // Pink noise has -3 dB/oct slope, so apply +3 dB/oct compensation to flatten it
            slopeDbPerOct = +3.0f;
            break;
        case TiltMode::White:
            // White noise is flat in power/Hz, but apply -3 dB/oct for perceptual compensation
            slopeDbPerOct = -3.0f;
            break;
    }
    
    return slopeDbPerOct * octaves;
}

float RTADisplay::dbToYWithCompensation (float db, float freqHz, const RenderState& s) const
{
    // Apply display gain and tilt compensation
    const float tiltDb = computeTiltDb (freqHz);
    const float dbWithCompensation = db + displayGainDb + tiltDb;
    
    // Clamp to display range
    const float clampedDb = juce::jlimit (s.bottomDb, s.topDb, dbWithCompensation);
    
    const float range = s.topDb - s.bottomDb;
    if (range <= 0.0f)
        return plotAreaTop;
    const float normalized = (s.topDb - clampedDb) / range;
    return plotAreaTop + normalized * plotAreaHeight;
}

void RTADisplay::setHoldStatus (bool isHoldOn)
{
    state.isHoldOn = isHoldOn;
    // No repaint needed for just hold status unless it affects overlay
}

void RTADisplay::setSessionMarker (bool visible, int bin, float db)
{
    // Only repaint if changed
    if (state.sessionMarkerVisible != visible || 
        state.sessionMarkerBin != bin || 
        std::abs (state.sessionMarkerDb - db) > 1e-4f)
    {
        state.sessionMarkerVisible = visible;
        state.sessionMarkerBin = bin;
        state.sessionMarkerDb = db;
        repaint();
    }
}

void RTADisplay::checkStructuralGeneration (uint32_t currentGen)
{
    if (currentGen != lastStructuralGen)
    {
        lastStructuralGen = currentGen;
        // Clear cached state on structural change
        hoveredBandIndex = -1;
        geometryValid = false;
        bandGeometry.clear();
        // Clear state data arrays to force re-validation
        state.bandsDb.clear();
        state.bandsPeakDb.clear();
        state.bandCentersHz.clear();
        state.logDb.clear();
        state.logPeakDb.clear();
        state.fftDb.clear();
        state.fftPeakDb.clear();
        // Set NoData status
        state.status = DataStatus::NoData;
        state.noDataReason = "structural change";
        repaint();
    }
}

#if JUCE_DEBUG
void RTADisplay::setDebugInfo (int viewMode, size_t fftSize, size_t logSize, size_t bandsSize,
                                bool fftValid, bool logValid, bool bandsValid, uint32_t structuralGen,
                                int bandMode, float minDb, float maxDb, float peakMinDb, float peakMaxDb)
{
    debugViewMode = viewMode;
    debugFFTSize = fftSize;
    debugLogSize = logSize;
    debugBandsSize = bandsSize;
    debugFFTValid = fftValid;
    debugLogValid = logValid;
    debugBandsValid = bandsValid;
    debugStructuralGen = structuralGen;
    debugBandMode = bandMode;
    debugMinDb = minDb;
    debugMaxDb = maxDb;
    debugPeakMinDb = peakMinDb;
    debugPeakMaxDb = peakMaxDb;
}
#endif

//==============================================================================
void RTADisplay::resized()
{
    updateGeometry();
}

void RTADisplay::updateGeometry()
{
    // B2: Geometry cache derived only from state + component bounds, never mutated in paint
    // B7: Never called from paint - only from resized() and setBandCenters()
    
    auto bounds = getLocalBounds().toFloat();
    
    // Reserve space for labels
    const float leftMargin = 50.0f;
    const float rightMargin = 10.0f;
    const float topMargin = 10.0f;
    const float bottomMargin = 30.0f;

    plotAreaLeft = leftMargin;
    plotAreaTop = topMargin;
    plotAreaWidth = bounds.getWidth() - leftMargin - rightMargin;
    plotAreaHeight = bounds.getHeight() - topMargin - bottomMargin;

    // B2: Guardrails - if bounds too small, clear geometry
    if (plotAreaWidth <= 1.0f || plotAreaHeight <= 1.0f)
    {
        bandGeometry.clear();
        geometryValid = false;
        return;
    }

    // B2: Update band geometry - only read from state.bandCentersHz
    if (state.bandCentersHz.empty())
    {
        bandGeometry.clear();
        geometryValid = true;  // Valid but empty
        return;
    }
    
    bandGeometry.resize (state.bandCentersHz.size());

    for (size_t i = 0; i < state.bandCentersHz.size(); ++i)
    {
        const float centerFreq = state.bandCentersHz[i];
        float xCenter = frequencyToX (centerFreq);  // Uses member variables (logMinFreq, logFreqRange, etc.)

        // Compute band width from adjacent bands or use fixed ratio
        float xLeft, xRight;
        if (i == 0)
        {
            // First band: use next band or fixed width
            if (state.bandCentersHz.size() > 1)
            {
                const float nextCenter = frequencyToX (state.bandCentersHz[static_cast<size_t> (1)]);
                const float width = (nextCenter - xCenter) * 0.5f;
                xLeft = xCenter - width;
                xRight = xCenter + width;
            }
            else
            {
                xLeft = xCenter - 5.0f;
                xRight = xCenter + 5.0f;
            }
        }
        else if (i == state.bandCentersHz.size() - 1)
        {
            // Last band: use previous band
            const float prevCenter = frequencyToX (state.bandCentersHz[i - 1]);
            const float width = (xCenter - prevCenter) * 0.5f;
            xLeft = xCenter - width;
            xRight = xCenter + width;
        }
        else
        {
            // Middle bands: use adjacent centers
            const float prevCenter = frequencyToX (state.bandCentersHz[i - 1]);
            const float nextCenter = frequencyToX (state.bandCentersHz[i + 1]);
            xLeft = (prevCenter + xCenter) * 0.5f;
            xRight = (xCenter + nextCenter) * 0.5f;
        }

        // Clamp to plot area
        xLeft = juce::jmax (plotAreaLeft, xLeft);
        xRight = juce::jmin (plotAreaLeft + plotAreaWidth, xRight);

        bandGeometry[i].xCenter = xCenter;
        bandGeometry[i].xLeft = xLeft;
        bandGeometry[i].xRight = xRight;
    }

    geometryValid = true;
}

//==============================================================================
float RTADisplay::frequencyToX (float freqHz) const
{
    // B2: Guardrails - handle invalid log ranges
    if (freqHz <= 0.0f || logFreqRange <= 0.0f)
        return plotAreaLeft;
    
    const float logFreq = std::log10 (freqHz);
    const float normalized = (logFreq - logMinFreq) / logFreqRange;
    return plotAreaLeft + normalized * plotAreaWidth;
}

//==============================================================================
// Helper functions for paint methods (use RenderState for data, member vars for geometry)
float RTADisplay::freqToX (float freqHz, const RenderState& s) const
{
    // B2: Guardrails - handle invalid log ranges
    if (freqHz <= 0.0f || s.maxHz <= s.minHz || s.minHz <= 0.0f)
        return plotAreaLeft;
    
    const float logMin = std::log10 (s.minHz);
    const float logMax = std::log10 (s.maxHz);
    const float logRange = logMax - logMin;
    if (logRange <= 0.0f)
        return plotAreaLeft;
    
    const float logFreq = std::log10 (freqHz);
    const float normalized = (logFreq - logMin) / logRange;
    return plotAreaLeft + normalized * plotAreaWidth;  // Uses member variables for geometry
}

float RTADisplay::dbToY (float db, const RenderState& s) const
{
    // Apply display gain offset (UI-only, affects rendering, not DSP)
    const float dbWithGain = db + displayGainDb;
    // Clamp to display range
    const float clampedDb = juce::jlimit (s.bottomDb, s.topDb, dbWithGain);
    
    const float range = s.topDb - s.bottomDb;
    if (range <= 0.0f)
        return plotAreaTop;
    const float normalized = (s.topDb - clampedDb) / range;
    return plotAreaTop + normalized * plotAreaHeight;  // Uses member variables for geometry
}

float RTADisplay::computeLogFreqFromIndex (int index, int numBands, float minHz, float maxHz) const
{
    // B4: Compute log frequency from index (for log mode rendering)
    if (numBands <= 0 || index < 0 || index >= numBands)
        return minHz;
    
    const float logMin = std::log10 (minHz);
    const float logMax = std::log10 (maxHz);
    const float logRange = logMax - logMin;
    const float logPos = logMin + (static_cast<float> (index) + 0.5f) / static_cast<float> (numBands) * logRange;
    return std::pow (10.0f, logPos);
}

int RTADisplay::findNearestLogBand (float x, const RenderState& s) const
{
    // B6: Find nearest log band index from x position
    if (x < plotAreaLeft || x > plotAreaLeft + plotAreaWidth || s.logDb.empty())
        return -1;
    
    const float logMin = std::log10 (s.minHz);
    const float logMax = std::log10 (s.maxHz);
    const float logRange = logMax - logMin;
    if (logRange <= 0.0f)
        return -1;
    
    // Inverse mapping: x -> normalized -> log position -> index
    const float normalized = (x - plotAreaLeft) / plotAreaWidth;
    const float logPos = logMin + normalized * logRange;
    
    // Map log position directly to index (no need to compute freq)
    const float normFromFreq = (logPos - logMin) / logRange;
    const int numBands = static_cast<int> (s.logDb.size());
    const int idx = juce::jlimit (0, numBands - 1, static_cast<int> (normFromFreq * numBands));
    
    return idx;
}

int RTADisplay::findNearestBand (float x) const
{
    // B6: Only used for Bands mode - uses bandGeometry
    if (!geometryValid || bandGeometry.empty())
        return -1;

    int nearest = -1;
    float minDist = std::numeric_limits<float>::max();

    for (size_t i = 0; i < bandGeometry.size(); ++i)
    {
        const float dist = std::abs (x - bandGeometry[i].xCenter);
        if (dist < minDist)
        {
            minDist = dist;
            nearest = static_cast<int> (i);
        }
    }

    return nearest;
}

//==============================================================================
void RTADisplay::mouseMove (const juce::MouseEvent& e)
{
    // B6: Make mouseMove safe and mode-aware without relying on bandGeometry in modes that don't have it
    const float x = static_cast<float> (e.x);
    const float y = static_cast<float> (e.y);
    int newHovered = -1;
    bool needsRepaint = false;
    
    if (state.viewMode == 2)  // Bands mode
    {
        // Only use bandGeometry + state.bandCentersHz for Bands mode
        if (!geometryValid || bandGeometry.empty() || state.bandsDb.empty() || state.bandCentersHz.empty() ||
            state.bandsDb.size() != state.bandCentersHz.size() || bandGeometry.size() != state.bandCentersHz.size())
        {
            hoveredBandIndex = -1;
            if (freqHover_.deactivate() || dbHover_.deactivate())
                repaint();
            return;
        }
        
        // Find nearest band using geometry
        newHovered = findNearestBand (x);
        newHovered = juce::jlimit (-1, static_cast<int> (state.bandsDb.size()) - 1, newHovered);
    }
    
    // Update axis hover controllers (for all modes that support hover)
    if (x >= plotAreaLeft && x <= plotAreaLeft + plotAreaWidth && 
        y >= plotAreaTop && y <= plotAreaTop + plotAreaHeight)
    {
        // Build frequency axis config
        const FreqAxisConfig freqConfig = buildFreqAxisConfig (state);
        freqHover_.setStyle (freqConfig.style);
        
        // Update frequency hover controller (X-axis)
        const float cursorXpx = x - plotAreaLeft;
        if (freqHover_.updateFromCursorPx (cursorXpx, plotAreaWidth, freqConfig.mapping, freqConfig.ticks))
            needsRepaint = true;
        
        // Build dB axis config
        const DbAxisConfig dbConfig = buildDbAxisConfig (state);
        dbHover_.setStyle (dbConfig.style);
        
        // Update dB hover controller (Y-axis)
        // Note: Y increases downward in screen coords, but dbMapping maps bottomDb (min) to 0, topDb (max) to plotHeight
        const float cursorYpx = y - plotAreaTop;
        if (dbHover_.updateFromCursorPx (cursorYpx, plotAreaHeight, dbConfig.mapping, dbConfig.ticks))
            needsRepaint = true;
    }
    else
    {
        // Outside plot bounds - deactivate both controllers
        if (freqHover_.deactivate() || dbHover_.deactivate())
            needsRepaint = true;
    }
    
    // Mode-specific hover logic
    if (state.viewMode == 1)  // Log mode
    {
        if (state.logDb.empty())
        {
            hoveredBandIndex = -1;
            if (peakSnap_.deactivate())
                needsRepaint = true;
            if (needsRepaint)
                repaint();
            return;
        }
        
        // Update peak snap controller for Log mode
        if (x >= plotAreaLeft && x <= plotAreaLeft + plotAreaWidth && 
            y >= plotAreaTop && y <= plotAreaTop + plotAreaHeight)
        {
            // Build frequency array for peak snap (compute on-the-fly, no allocation)
            const int numBands = static_cast<int> (state.logDb.size());
            static constexpr int kMaxBands = 4096;
            const int bandsToUse = std::min (numBands, kMaxBands);
            
            // Use stack arrays to avoid heap allocations
            float freqHz[kMaxBands];
            float db[kMaxBands];
            
            const float logMin = std::log10 (state.minHz);
            const float logMax = std::log10 (state.maxHz);
            const float logRange = logMax - logMin;
            
            for (int i = 0; i < bandsToUse; ++i)
            {
                // Compute log frequency from index (same as computeLogFreqFromIndex)
                const float logPos = logMin + (static_cast<float> (i) + 0.5f) / static_cast<float> (numBands) * logRange;
                freqHz[i] = std::pow (10.0f, logPos);
                const auto idx = static_cast<size_t> (i);
                db[i] = (idx < state.logDb.size()) ? state.logDb[idx] : std::numeric_limits<float>::quiet_NaN();
            }
            
            // Build frequency axis mapping (same as buildFreqAxisConfig)
            mdsp_ui::AxisMapping freqMapping;
            freqMapping.scale = mdsp_ui::AxisScale::Log10;
            freqMapping.minValue = state.minHz;
            freqMapping.maxValue = state.maxHz;
            
            const juce::Rectangle<float> plotBoundsFloat (plotAreaLeft, plotAreaTop, plotAreaWidth, plotAreaHeight);
            
            if (peakSnap_.updateFromCursorX (x, plotBoundsFloat, freqMapping, freqHz, db, bandsToUse))
                needsRepaint = true;
        }
        else
        {
            if (peakSnap_.deactivate())
                needsRepaint = true;
        }
        
        newHovered = findNearestLogBand (x, state);
    }
    else if (state.viewMode == 0)  // FFT mode
    {
        // For now, disable band hover in FFT mode
        newHovered = -1;
    }
    if (newHovered != hoveredBandIndex)
    {
        hoveredBandIndex = newHovered;
        needsRepaint = true;
    }
    
    if (needsRepaint)
        repaint();
}

void RTADisplay::mouseExit (const juce::MouseEvent&)
{
    bool needsRepaint = false;
    if (hoveredBandIndex >= 0)
    {
        hoveredBandIndex = -1;
        needsRepaint = true;
    }
    if (freqHover_.deactivate() || dbHover_.deactivate())
    {
        needsRepaint = true;
    }
    if (needsRepaint)
        repaint();
    }

#if JUCE_DEBUG
void RTADisplay::mouseDown (const juce::MouseEvent& e)
{
    // Debug-only: Toggle envelope decimator with Shift+Click
    if (e.mods.isShiftDown())
    {
        useEnvelopeDecimator = ! useEnvelopeDecimator;
        repaint();
        DBG ("RTADisplay: envelope decimator " << (useEnvelopeDecimator ? "ON" : "OFF"));
    }
}
#endif

//==============================================================================
void RTADisplay::paint (juce::Graphics& g)
{
    // B3: Use local references to state (NO mutations)
    const auto& s = state;
    mdsp_ui::Theme theme;
    
    // Background
    g.fillAll (theme.background);

    // Draw grid (always draw grid)
    drawGrid (g, s, theme);

    // If no data, show message and return
    if (s.status == DataStatus::NoData)
    {
        const juce::String message = "NO DATA: " + s.noDataReason;
        mdsp_ui::TextOverlayStyle noDataStyle;
        noDataStyle.colourOverride = theme.warning;
        noDataStyle.fontHeightPx = 10.0f;
        noDataStyle.justification = juce::Justification::centred;
        mdsp_ui::TextOverlayRenderer::draw (g, juce::Rectangle<float> (getLocalBounds().toFloat()), theme, message, noDataStyle);
        return;
    }
    
    // B3: Render based on view mode using local references (NO state mutations)
    if (s.viewMode == 2)  // Bands mode
    {
        // Validate sizes before painting
        if (s.bandsDb.empty() || s.bandCentersHz.empty() || 
            s.bandsDb.size() != s.bandCentersHz.size())
        {
            mdsp_ui::TextOverlayStyle noDataStyle;
            noDataStyle.colourOverride = theme.warning;
            noDataStyle.fontHeightPx = 10.0f;
            noDataStyle.justification = juce::Justification::centred;
            mdsp_ui::TextOverlayRenderer::draw (g, juce::Rectangle<float> (getLocalBounds().toFloat()), theme, "NO DATA: BANDS size mismatch", noDataStyle);
            return;
        }
        paintBandsMode (g, s, theme);
    }
    else if (s.viewMode == 1)  // Log mode
    {
        // Validate log data exists
        if (s.logDb.empty())
        {
            mdsp_ui::TextOverlayStyle noDataStyle;
            noDataStyle.colourOverride = theme.warning;
            noDataStyle.fontHeightPx = 10.0f;
            noDataStyle.justification = juce::Justification::centred;
            mdsp_ui::TextOverlayRenderer::draw (g, juce::Rectangle<float> (getLocalBounds().toFloat()), theme, "NO DATA: LOG empty", noDataStyle);
            return;
        }
        paintLogMode (g, s, theme);
    }
    else if (s.viewMode == 0)  // FFT mode
    {
        // Validate FFT data exists
        if (s.fftDb.empty())
        {
            mdsp_ui::TextOverlayStyle noDataStyle;
            noDataStyle.colourOverride = theme.warning;
            noDataStyle.fontHeightPx = 10.0f;
            noDataStyle.justification = juce::Justification::centred;
            mdsp_ui::TextOverlayRenderer::draw (g, juce::Rectangle<float> (getLocalBounds().toFloat()), theme, "NO DATA: FFT empty", noDataStyle);
            return;
        }
        paintFFTMode (g, s, theme);
    }
}

// Helper functions defined above (freqToX, dbToY, computeLogFreqFromIndex, findNearestLogBand)

//==============================================================================
RTADisplay::FreqAxisConfig RTADisplay::buildFreqAxisConfig (const RenderState& s) const
{
    FreqAxisConfig config;
    
    const float freqGridPoints[] = { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };
    for (float freq : freqGridPoints)
    {
        if (freq >= s.minHz && freq <= s.maxHz)
        {
            const float x = freqToX (freq, s);
            const float posPx = x - plotAreaLeft;
            juce::String label;
            if (freq >= 1000.0f)
                label = juce::String (freq / 1000.0f, 1) + "k";
            else
                label = juce::String (static_cast<int> (freq));
            auto isMajorFreq = [](float f)
            {
                const float majors[] = { 20.0f, 100.0f, 1000.0f, 10000.0f, 20000.0f };
                for (float m : majors)
                    if (std::abs (f - m) < 0.1f) return true;
                return false;
            };
            const bool isMajor = isMajorFreq (freq);
            config.ticks.add ({ posPx, label, isMajor });
        }
    }
    
    config.mapping.scale = mdsp_ui::AxisScale::Log10;
    config.mapping.minValue = s.minHz;
    config.mapping.maxValue = s.maxHz;
    
    config.snap.mode = mdsp_ui::SnapMode::NearestLabelledTick;
    config.snap.maxSnapDistPx = 12.0f;
    
    config.style.snap = config.snap;
    config.style.epsPosPx = 0.5f;
    config.style.epsValue = 0.1f;
    
    return config;
}

RTADisplay::DbAxisConfig RTADisplay::buildDbAxisConfig (const RenderState& s) const
{
    DbAxisConfig config;
    
    // Build dB ticks: every 6dB, major at every 12dB (with labels)
    for (int db = static_cast<int> (s.topDb); db >= static_cast<int> (s.bottomDb); db -= 6)
    {
        const float y = dbToY (static_cast<float> (db), s);
        if (y >= plotAreaTop && y <= plotAreaTop + plotAreaHeight)
        {
            const float posPx = y - plotAreaTop;  // Offset from plot top
            const bool isMajor = (db % 12 == 0);
            juce::String label = isMajor ? (juce::String (db) + " dB") : juce::String();
            config.ticks.add ({ posPx, label, isMajor });
        }
    }
    
    config.mapping.scale = mdsp_ui::AxisScale::Linear;
    config.mapping.minValue = s.bottomDb;  // Note: bottomDb is more negative (lower value)
    config.mapping.maxValue = s.topDb;     // topDb is less negative (higher value)
    
    config.snap.mode = mdsp_ui::SnapMode::NearestLabelledTick;
    config.snap.maxSnapDistPx = 12.0f;
    
    config.style.snap = config.snap;
    config.style.epsPosPx = 0.5f;
    config.style.epsValue = 0.1f;
    
    return config;
}

//==============================================================================
void RTADisplay::drawGrid (juce::Graphics& g, const RenderState& s, const mdsp_ui::Theme& theme)
{
    // B3: Pure function - uses only state reference, no member mutations
    
    // Define plot bounds
    const juce::Rectangle<int> plotBounds (static_cast<int> (plotAreaLeft),
                                           static_cast<int> (plotAreaTop),
                                           static_cast<int> (plotAreaWidth),
                                           static_cast<int> (plotAreaHeight));
    
    // Build dB axis ticks (Left edge) - only major ticks (every 12dB) get labels
    juce::Array<mdsp_ui::AxisTick> dbTicks;
    for (int db = static_cast<int> (s.topDb); db >= static_cast<int> (s.bottomDb); db -= 6)
    {
        const float y = dbToY (static_cast<float> (db), s);
        if (y >= plotAreaTop && y <= plotAreaTop + plotAreaHeight)
        {
            const float posPx = y - plotAreaTop;  // Offset from plot top
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
            const float x = freqToX (freq, s);
            const float posPx = x - plotAreaLeft;  // Offset from plot left
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
void RTADisplay::paintBandsMode (juce::Graphics& g, const RenderState& s, const mdsp_ui::Theme& theme)
{
    // B3: Pure function - uses only state reference, no member mutations
    
    // B7: Never calls updateGeometry() from paint
    if (!geometryValid || s.bandCentersHz.empty() || s.bandsDb.empty())
        return;
    
    // Check size mismatch
    if (s.bandsDb.size() != s.bandCentersHz.size() || 
        bandGeometry.size() != s.bandCentersHz.size())
        return;
    
    // B3: Decide locally if peaks should be drawn (no state mutation)
    const bool hasPeaks = (s.bandsPeakDb.size() == s.bandsDb.size() && !s.bandsPeakDb.empty());
    
    // Draw thin vertical markers at each band center frequency (subtle)
    g.setColour (theme.grid.withAlpha (0.2f));
    for (size_t i = 0; i < s.bandCentersHz.size() && i < bandGeometry.size(); ++i)
    {
        const float x = bandGeometry[i].xCenter;
        g.drawVerticalLine (static_cast<int> (x), plotAreaTop, plotAreaTop + plotAreaHeight);
    }

    // Draw band bars using BarsRenderer
    const int numBands = static_cast<int> (std::min (s.bandsDb.size(), bandGeometry.size()));
    if (numBands > 0)
    {
        // Use fixed stack array to avoid heap allocations (cap at 4096)
        static constexpr int kMaxBars = 4096;
        const int barsToDraw = std::min (numBands, kMaxBars);
        
        float xLeft[kMaxBars];
        float xRight[kMaxBars];
        float yTop[kMaxBars];
        
        const float bottomY = plotAreaTop + plotAreaHeight;

        // Collect bar geometry
        for (int i = 0; i < barsToDraw; ++i)
        {
            const auto idx = static_cast<size_t> (i);
            xLeft[i] = bandGeometry[idx].xLeft;
            xRight[i] = bandGeometry[idx].xRight;
            
            // Apply display gain in dbToY, so just pass raw dB (clamping happens in dbToY)
            const float db = s.bandsDb[idx];
            yTop[i] = dbToY (db, s);
        }
        
        // Render bars
        const juce::Rectangle<int> plotBounds (static_cast<int> (plotAreaLeft),
                                                static_cast<int> (plotAreaTop),
                                                static_cast<int> (plotAreaWidth),
                                                static_cast<int> (plotAreaHeight));
        
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
        const juce::Rectangle<float> plotBounds (plotAreaLeft, plotAreaTop, plotAreaWidth, plotAreaHeight);
        const int numBandsToDraw = static_cast<int> (std::min (s.bandsPeakDb.size(), bandGeometry.size()));
        
        mdsp_ui::SeriesStyle peakStyle;
        peakStyle.strokeThickness = 1.5f;
        peakStyle.alpha = 0.8f;
        peakStyle.clipToPlot = true;
        peakStyle.minXStepPx = 1.0f;
        peakStyle.minYStepPx = 0.5f;
        peakStyle.useRoundedJoins = true;
#if JUCE_DEBUG
        peakStyle.decimationMode = useEnvelopeDecimator
            ? mdsp_ui::DecimationMode::Envelope
            : mdsp_ui::DecimationMode::Simple;
#else
        peakStyle.decimationMode = mdsp_ui::DecimationMode::Simple;
#endif
        peakStyle.envelopeMinBucketPx = 1.0f;
        peakStyle.envelopeDrawVertical = true;

        mdsp_ui::SeriesRenderer::drawPathFromMapping (g, plotBounds, theme, numBandsToDraw,
            [this] (int i) -> float
            {
                const auto idx = static_cast<size_t> (i);
                return bandGeometry[idx].xCenter;
            },
            [&s, this] (int i) -> float
            {
                const auto idx = static_cast<size_t> (i);
                float peakDb = s.bandsPeakDb[idx];
                peakDb = juce::jlimit (s.bottomDb, 0.0f, peakDb);
                return dbToY (peakDb, s);
            },
            theme.seriesPeak, peakStyle);
    }
    
    // Draw legend overlay
    {
        const juce::Rectangle<float> legendPlotBounds (plotAreaLeft, plotAreaTop, plotAreaWidth, plotAreaHeight);
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
    if (hoveredBandIndex >= 0 
        && hoveredBandIndex < static_cast<int> (s.bandsDb.size()) 
        && hoveredBandIndex < static_cast<int> (bandGeometry.size())
        && hoveredBandIndex < static_cast<int> (s.bandCentersHz.size()))
    {
        const int safeIndex = hoveredBandIndex;
        const auto idx = static_cast<size_t> (safeIndex);
        const float x = bandGeometry[idx].xCenter;
        const float currentDb = s.bandsDb[idx];
        const float peakDb = (hasPeaks && idx < s.bandsPeakDb.size())
            ? s.bandsPeakDb[idx] : -1000.0f;
        const float centerFreq = s.bandCentersHz[idx];

        // Vertical cursor line
        g.setColour (theme.text.withAlpha (0.5f));
        g.drawVerticalLine (static_cast<int> (x), plotAreaTop, plotAreaTop + plotAreaHeight);

        // Tooltip box using PlotFrameRenderer
        const float tooltipX = juce::jmin (x + 10.0f, plotAreaLeft + plotAreaWidth - 120.0f);
        const float tooltipY = plotAreaTop + 10.0f;
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
void RTADisplay::paintLogMode (juce::Graphics& g, const RenderState& s, const mdsp_ui::Theme& theme)
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
        
        const float bottomY = plotAreaTop + plotAreaHeight;
        
        // Collect bar geometry
        for (int i = 0; i < barsToDraw; ++i)
    {
        // Compute left/right boundaries in log space
        const float logPosLow = logMin + (logRange * static_cast<float> (i)) / static_cast<float> (numBands);
        const float logPosHigh = logMin + (logRange * static_cast<float> (i + 1)) / static_cast<float> (numBands);
        const float fL = std::pow (10.0f, logPosLow);
        const float fR = std::pow (10.0f, logPosHigh);
        
        // Map to x coordinates
            xLeft[i] = freqToX (fL, s);
            xRight[i] = freqToX (fR, s);
        
        // Apply display gain in dbToY, so just pass raw dB (clamping happens in dbToY)
            const auto idx = static_cast<size_t> (i);
            const float db = s.logDb[idx];
            yTop[i] = dbToY (db, s);
        }
        
        // Render bars
        const juce::Rectangle<int> plotBounds (static_cast<int> (plotAreaLeft),
                                                static_cast<int> (plotAreaTop),
                                                static_cast<int> (plotAreaWidth),
                                                static_cast<int> (plotAreaHeight));
        
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
        const juce::Rectangle<float> plotBounds (plotAreaLeft, plotAreaTop, plotAreaWidth, plotAreaHeight);
        mdsp_ui::SeriesStyle peakStyle;
        peakStyle.strokeThickness = 1.5f;
        peakStyle.alpha = 0.8f;
        peakStyle.clipToPlot = true;
        peakStyle.minXStepPx = 1.0f;
        peakStyle.minYStepPx = 0.5f;
        peakStyle.useRoundedJoins = true;
#if JUCE_DEBUG
        peakStyle.decimationMode = useEnvelopeDecimator
            ? mdsp_ui::DecimationMode::Envelope
            : mdsp_ui::DecimationMode::Simple;
#else
        peakStyle.decimationMode = mdsp_ui::DecimationMode::Simple;
#endif
        peakStyle.envelopeMinBucketPx = 1.0f;
        peakStyle.envelopeDrawVertical = true;

        mdsp_ui::SeriesRenderer::drawPathFromMapping (g, plotBounds, theme, numBands,
            [&s, numBands, this] (int i) -> float
            {
            const float centerFreq = computeLogFreqFromIndex (i, numBands, s.minHz, s.maxHz);
                return freqToX (centerFreq, s);
            },
            [&s, this] (int i) -> float
            {
                const auto idx = static_cast<size_t> (i);
                float peakDb = s.logPeakDb[idx];
                peakDb = juce::jlimit (s.bottomDb, 0.0f, peakDb);
                return dbToY (peakDb, s);
            },
            theme.seriesPeak, peakStyle);
    }
    
    // Draw legend overlay
    {
        const juce::Rectangle<float> legendPlotBounds (plotAreaLeft, plotAreaTop, plotAreaWidth, plotAreaHeight);
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
    const mdsp_ui::PeakSnapState& peakSnapState = peakSnap_.state();
    const mdsp_ui::AxisHoverState& freqHoverState = freqHover_.state();
    const mdsp_ui::AxisHoverState& dbHoverState = dbHover_.state();
    
    // Determine if we should show readout (peak snap active OR axis hover active)
    const bool hasFreq = peakSnapState.snappedActive || freqHoverState.active;
    const bool hasDb = dbHoverState.active;
    
    if ((hasFreq || hasDb) && state.viewMode == 1)
    {
        const juce::Rectangle<float> plotBoundsFloat (plotAreaLeft, plotAreaTop, plotAreaWidth, plotAreaHeight);
        const juce::Rectangle<int> plotBounds (static_cast<int> (plotAreaLeft),
                                               static_cast<int> (plotAreaTop),
                                               static_cast<int> (plotAreaWidth),
                                               static_cast<int> (plotAreaHeight));
        
        // Determine frequency value (peak snap takes priority)
        float freqHz = 0.0f;
        float freqCursorXPx = 0.0f;
        bool freqActive = false;
        if (peakSnapState.snappedActive)
        {
            freqHz = peakSnapState.snappedFreqHz;
            freqCursorXPx = peakSnapState.snappedXPx - plotAreaLeft; // Convert to relative
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
            g.drawVerticalLine (static_cast<int> (cursorX), plotAreaTop, plotAreaTop + plotAreaHeight);
        }
        
        // Draw horizontal cursor line at resolved position (if db active and snapped)
        if (dbActive && dbHoverState.active && dbHoverState.snappedTickIndex >= 0)
        {
            const float cursorY = plotAreaTop + dbCursorYPx;
            g.setColour (theme.text.withAlpha (0.25f));
            g.drawHorizontalLine (static_cast<int> (cursorY), plotAreaLeft, plotAreaLeft + plotAreaWidth);
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
            const float anchorX = freqActive ? (plotAreaLeft + freqCursorXPx) : (plotAreaLeft + plotAreaWidth * 0.5f);
            const float anchorY = dbActive ? (plotAreaTop + dbCursorYPx) : (plotAreaTop + plotAreaHeight * 0.5f);
            
            // Position readout box: bottom-left of cursor, offset slightly
            float readoutX = anchorX + 10.0f;
            float readoutY = anchorY - frameHeight - 5.0f;
            
            // Clamp to plot bounds
            readoutX = juce::jmax (plotAreaLeft + 5.0f, juce::jmin (readoutX, plotAreaLeft + plotAreaWidth - frameWidth - 5.0f));
            readoutY = juce::jmax (plotAreaTop + 5.0f, juce::jmin (readoutY, plotAreaTop + plotAreaHeight - frameHeight - 5.0f));
            
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

void RTADisplay::paintFFTMode (juce::Graphics& g, const RenderState& s, const mdsp_ui::Theme& theme)
{
    // B3: Pure function - uses only state reference, no member mutations
    
    // B5: FFT mode uses log mapping to match grid scale
    // B7: Never calls updateGeometry() from paint
    
    if (s.fftDb.empty() || s.fftSize <= 0 || s.sampleRate <= 0.0)
        return;

    // B3: Decide locally if peaks should be drawn (no state mutation)
    const bool hasPeaks = (s.fftPeakDb.size() == s.fftDb.size() && !s.fftPeakDb.empty());

    // B5: Smooth Path Construction
    // -------------------------------------------------------------------------
    const int numBins = static_cast<int> (s.fftDb.size());
    const int expectedBins = s.fftSize / 2 + 1;
    const int availableBins = std::min (numBins, expectedBins);
    
    if (availableBins <= 0)
        return;

    const double binWidthHz = s.sampleRate / static_cast<double> (s.fftSize);
    
    // Determine visible bin range
    const int firstBin = juce::jlimit (0, availableBins - 1,
                                      static_cast<int> (std::ceil (static_cast<double> (s.minHz) / binWidthHz)));
    const int lastBin  = juce::jlimit (0, availableBins - 1,
                                      static_cast<int> (std::floor (static_cast<double> (s.maxHz) / binWidthHz)));

    if (lastBin < firstBin)
        return;

    // Reusable Scratch Buffers (Message Thread Safe)
    static std::vector<juce::Point<float>> scratchPoints;
    static juce::Path scratchPath;
    
    // Ensure capacity
    if (scratchPoints.capacity() < 16384)
        scratchPoints.reserve (16384);
        
    // Lambda to build safe point list AND track max visible dB (for energy boost)
    auto buildPoints = [&](const std::vector<float>& data, std::vector<juce::Point<float>>& pts, float& maxDbInView)
    {
        pts.clear();
        maxDbInView = -200.0f; // Reset max
        
        for (int i = firstBin; i <= lastBin; ++i)
        {
            const size_t idx = static_cast<size_t> (i);
            const float db = data[idx];
            
            if (!std::isfinite (db)) continue;

            if (db > maxDbInView)
                maxDbInView = db;

            const float freq = static_cast<float> (static_cast<double> (i) * binWidthHz);
            
            // Apply compensation + mapping
            const float x = freqToX (freq, s);
            const float y = dbToYWithCompensation (db, freq, s);
            
            pts.emplace_back (x, y);
        }
    };
    
    // Lambda to smooth path
    auto smoothPath = [&](juce::Path& p, const std::vector<juce::Point<float>>& pts)
    {
        p.clear();
        if (pts.empty()) return;
        
        p.startNewSubPath (pts[0]);
        
        if (pts.size() < 3)
        {
            for (size_t i = 1; i < pts.size(); ++i)
                p.lineTo (pts[i]);
            return;
        }
        
        // Quadratic Bezier Smoothing
        for (size_t i = 1; i < pts.size() - 2; ++i)
        {
            const auto& p1 = pts[i];
            const auto& p2 = pts[i+1];
            const auto mid = (p1 + p2) * 0.5f;
            p.quadraticTo (p1, mid);
        }
        
        // Connect last few points linearly
        const auto& secondLast = pts[pts.size() - 2];
        const auto& last = pts[pts.size() - 1];
        
        p.quadraticTo (secondLast, last);
    };

    // Lambda to build and draw a generic trace (for multi-channel restoration)
    auto buildTracePath = [&](juce::Graphics& gTarget, juce::Path& p, const std::vector<float>& dataVector, 
                              juce::Colour col, float thickness, float viewWidth, bool isPeak)
    {
        if (dataVector.empty()) return;

        float visibleMaxDb = -200.0f;
        buildPoints (dataVector, scratchPoints, visibleMaxDb);
        
        if (!scratchPoints.empty())
        {
            smoothPath (p, scratchPoints);
            
            // local energy calculation
            const float eNorm = juce::jlimit (0.0f, 1.0f, (visibleMaxDb - (s.bottomDb + 20.0f)) / 40.0f);
            const float eMul = static_cast<float> (juce::jmap (eNorm, 0.0f, 1.0f, 0.95f, 1.25f));
            
            drawSilkTrace (gTarget, p, col, thickness, viewWidth, isPeak, eMul, false);
        }
    };

    const juce::Rectangle<float> plotBounds (plotAreaLeft, plotAreaTop, plotAreaWidth, plotAreaHeight);
    
    // Channel Colors (Local definition as they are missing from Theme)
    const juce::Colour colL    = juce::Colour (0xff29b6f6); // Light Blue 400
    const juce::Colour colR    = juce::Colour (0xffef5350); // Red 400
    const juce::Colour colMid  = juce::Colour (0xff66bb6a); // Green 400
    const juce::Colour colSide = juce::Colour (0xffab47bc); // Purple 400
    const juce::Colour colMono = juce::Colour (0xffffee58); // Yellow 400 (Distinct from white text)

    // MULTI_TRACE_RENDER_RESTORE_V1: Draw Order: Side -> Mid -> L -> R -> Mono -> Stereo -> RMS -> Peak
    // ---------------------------------------------------------------------------------------

    // Startup Guard: If no valid multi-trace data has ever been received, skip these traces 
    // to avoid the "flat line" artifact (vertical drop at start).
    if (s.hasValidMultiTraceData && s.lrBinCount > 0)
    {
        // 1. Side
        if (traceConfig_.showSide && !s.sideDb.empty())
        {
            buildTracePath (g, scratchPath, s.sideDb, colSide, 1.1f, plotAreaWidth, false);
        }

        // 2. Mid
        if (traceConfig_.showMid && !s.midDb.empty())
        {
            buildTracePath (g, scratchPath, s.midDb, colMid, 1.1f, plotAreaWidth, false);
        }
        
        // 3. Left
        if (traceConfig_.showL && !s.lDbL.empty())
        {
            buildTracePath (g, scratchPath, s.lDbL, colL, 1.1f, plotAreaWidth, false);
        }

        // 4. Right
        if (traceConfig_.showR && !s.lDbR.empty())
        {
            buildTracePath (g, scratchPath, s.lDbR, colR, 1.1f, plotAreaWidth, false);
        }

        // 5. Mono
        if (traceConfig_.showMono && !s.monoDb.empty())
        {
            buildTracePath (g, scratchPath, s.monoDb, colMono, 1.3f, plotAreaWidth, false);
        }

        // 6. Stereo (ShowLR toggle maps to this combined trace for visibility)
        if (traceConfig_.showLR && !s.stereoDb.empty())
        {
            // Stereo gets a special color or re-uses L/R? 
            // Often stereo is just white/bright or the "main" color.
            // Let's use a distinct Cyan/White for Stereo envelope.
            const juce::Colour colStereo = juce::Colour (0xffe0f7fa); // Cyan 50
            buildTracePath (g, scratchPath, s.stereoDb, colStereo, 1.3f, plotAreaWidth, false);
        }
    }

    // 6. Draw Spectrum (RMS) - Main Trace
    // -------------------------------------------------------------
    if (traceConfig_.showRMS)
    {
        // Re-use logic or inline? We'll stick to inline for RMS/Peak as it was special (and main trace)
        // actually we can use buildTracePath if we want, but let's keep the existing structure minimally modified if possible,
        // BUT the prompt implies strict ordering.
        // And I already wrapped it.
        
        float fftMaxDb = -200.0f;
        buildPoints (s.fftDb, scratchPoints, fftMaxDb);
        if (!scratchPoints.empty())
        {
            smoothPath (scratchPath, scratchPoints);
            
            const float energyNorm = juce::jlimit (0.0f, 1.0f, (fftMaxDb - (s.bottomDb + 20.0f)) / 40.0f);
            const float energyMul = static_cast<float> (juce::jmap (energyNorm, 0.0f, 1.0f, 0.95f, 1.25f));

            drawSilkTrace (g, scratchPath, theme.accent, 1.5f, plotAreaWidth, false, energyMul, false);
        }
    }

    // 7. Draw Peak Trace
    // -------------------------------------------------------------
    if (hasPeaks)
    {
        float peakMaxDb = -200.0f;
        buildPoints (s.fftPeakDb, scratchPoints, peakMaxDb);
        if (!scratchPoints.empty())
        {
            smoothPath (scratchPath, scratchPoints);

            const float energyNorm = juce::jlimit (0.0f, 1.0f, (peakMaxDb - (s.bottomDb + 20.0f)) / 40.0f);
            const float energyMul = static_cast<float> (juce::jmap (energyNorm, 0.0f, 1.0f, 0.95f, 1.25f));
            
            const bool useShimmer = (MDSP_TRACE_SHIMMER_V2 != 0);
            drawSilkTrace (g, scratchPath, theme.seriesPeak, 1.2f, plotAreaWidth, true, energyMul, useShimmer);
        }
    }
    
    // Draw legend overlay
    {
        const juce::Rectangle<float> legendPlotBounds (plotAreaLeft, plotAreaTop, plotAreaWidth, plotAreaHeight);
        mdsp_ui::LegendItem legendItems[2];
        legendItems[0].label = "FFT";
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
    // Draw Session Marker (Tick)
    if (s.sessionMarkerVisible && s.viewMode == 0) // Only in FFT mode
    {
        const float x = freqToX (s.fftSize > 0 ? static_cast<float> (s.sessionMarkerBin * s.sampleRate / s.fftSize) : 0.0f, s);
        
        // Guard against out of bounds
        if (x >= plotAreaLeft && x <= (plotAreaLeft + plotAreaWidth))
        {
             const float y = dbToY (s.sessionMarkerDb, s);
             
             // Draw small tick and dot
             g.setColour (theme.seriesPeak.brighter(0.3f));
             g.drawLine (x, y - 4.0f, x, y + 4.0f, 2.0f);
             g.fillEllipse (x - 2.0f, y - 2.0f, 4.0f, 4.0f);
        }
    }
}

