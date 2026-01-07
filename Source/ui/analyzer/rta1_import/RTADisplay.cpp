#include "RTADisplay.h"
#include <mdsp_ui/Theme.h>
#include <mdsp_ui/AxisRenderer.h>
#include <mdsp_ui/AxisInteraction.h>
#include <mdsp_ui/PlotFrameRenderer.h>
#include <mdsp_ui/SeriesRenderer.h>
#include <mdsp_ui/BarsRenderer.h>
#include <cmath>
#include <type_traits>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <limits>
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
RTADisplay::RTADisplay()
{
    // Initialize state with defaults
    state.minHz = 20.0f;
    state.maxHz = 20000.0f;
    state.topDb = 0.0f;
    state.bottomDb = -90.0f;
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
    bool newHoverActive = false;
    float newHoverFreqHz = 0.0f;
    int newHoverTickIndex = -1;
    float newHoverPosPx = 0.0f;
    
    if (state.viewMode == 2)  // Bands mode
    {
        // Only use bandGeometry + state.bandCentersHz for Bands mode
        if (!geometryValid || bandGeometry.empty() || state.bandsDb.empty() || state.bandCentersHz.empty() ||
            state.bandsDb.size() != state.bandCentersHz.size() || bandGeometry.size() != state.bandCentersHz.size())
        {
            hoveredBandIndex = -1;
            hoverActive = false;
            return;
        }
        
        // Find nearest band using geometry
        newHovered = findNearestBand (x);
        newHovered = juce::jlimit (-1, static_cast<int> (state.bandsDb.size()) - 1, newHovered);
    }
    else if (state.viewMode == 1)  // Log mode
    {
        // B6: For Log mode, compute hovered index from x using inverse log mapping
        if (state.logDb.empty())
        {
            hoveredBandIndex = -1;
            hoverActive = false;
            return;
        }
        
        newHovered = findNearestLogBand (x, state);
        
        // Frequency-axis hover using AxisInteraction
        if (x >= plotAreaLeft && x <= plotAreaLeft + plotAreaWidth && 
            y >= plotAreaTop && y <= plotAreaTop + plotAreaHeight)
        {
            // Build frequency ticks (same as in drawGrid)
            juce::Array<mdsp_ui::AxisTick> freqTicks;
            const float freqGridPoints[] = { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };
            for (float freq : freqGridPoints)
            {
                if (freq >= state.minHz && freq <= state.maxHz)
                {
                    const float tickX = freqToX (freq, state);
                    const float posPx = tickX - plotAreaLeft;
                    juce::String label;
                    if (freq >= 1000.0f)
                        label = juce::String (freq / 1000.0f, 1) + "k";
                    else
                        label = juce::String (static_cast<int> (freq));
                    const bool isMajor = (freq == 20.0f || freq == 100.0f || freq == 1000.0f || freq == 10000.0f || freq == 20000.0f);
                    freqTicks.add ({ posPx, label, isMajor });
                }
            }
            
            // Build axis mapping
            mdsp_ui::AxisMapping freqMapping;
            freqMapping.scale = mdsp_ui::AxisScale::Log10;
            freqMapping.minValue = state.minHz;
            freqMapping.maxValue = state.maxHz;
            
            // Hit test
            const float posPx = x - plotAreaLeft;
            mdsp_ui::AxisSnapOptions snapOpts;
            snapOpts.mode = mdsp_ui::SnapMode::NearestLabelledTick;
            snapOpts.maxSnapDistPx = 12.0f;
            
            mdsp_ui::AxisHitTest hit = mdsp_ui::AxisInteraction::hitTest (posPx, plotAreaWidth, freqMapping, freqTicks, snapOpts);
            
            if (hit.active)
            {
                newHoverActive = true;
                newHoverFreqHz = hit.snapped ? hit.snappedValue : hit.value;
                newHoverTickIndex = hit.tickIndex;
                newHoverPosPx = hit.posPx;  // Store resolved position
            }
        }
    }
    else  // FFT mode (viewMode == 0)
    {
        // B6: For FFT mode: either disable hover (set hovered=-1) or map to nearest freq/bin (optional)
        // For now, disable hover in FFT mode
        newHovered = -1;
        newHoverActive = false;
    }
    
    bool needsRepaint = false;
    if (newHovered != hoveredBandIndex)
    {
        hoveredBandIndex = newHovered;
        needsRepaint = true;
    }
    // Use epsilon comparison for frequency to avoid float equality issues
    constexpr float kFreqEpsHz = 0.1f;
    constexpr float kPosEpsPx = 0.5f;
    const bool freqChanged = std::abs (newHoverFreqHz - lastHoverFreqHz) > kFreqEpsHz;
    const bool posChanged = std::abs (newHoverPosPx - lastHoverPosPx) > kPosEpsPx;
    if (newHoverActive != hoverActive || 
        newHoverTickIndex != lastHoverTickIndex ||
        freqChanged || posChanged)
    {
        hoverActive = newHoverActive;
        lastHoverFreqHz = newHoverFreqHz;
        lastHoverTickIndex = newHoverTickIndex;
        lastHoverPosPx = newHoverPosPx;
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
    if (hoverActive)
    {
        hoverActive = false;
        lastHoverFreqHz = 0.0f;
        lastHoverTickIndex = -1;
        lastHoverPosPx = 0.0f;
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
        g.setColour (theme.warning);
        g.setFont (smallFont);
        const juce::String message = "NO DATA: " + s.noDataReason;
        g.drawText (message, getLocalBounds(), juce::Justification::centred);
        return;
    }

    // B3: Render based on view mode using local references (NO state mutations)
    if (s.viewMode == 2)  // Bands mode
    {
        // Validate sizes before painting
        if (s.bandsDb.empty() || s.bandCentersHz.empty() || 
            s.bandsDb.size() != s.bandCentersHz.size())
        {
            g.setColour (theme.warning);
            g.setFont (smallFont);
            g.drawText ("NO DATA: BANDS size mismatch", getLocalBounds(), juce::Justification::centred);
            return;
        }
        paintBandsMode (g, s, theme);
    }
    else if (s.viewMode == 1)  // Log mode
    {
        // Validate log data exists
        if (s.logDb.empty())
        {
            g.setColour (theme.warning);
            g.setFont (smallFont);
            g.drawText ("NO DATA: LOG empty", getLocalBounds(), juce::Justification::centred);
            return;
        }
        paintLogMode (g, s, theme);
    }
    else  // FFT mode (viewMode == 0)
    {
        // Validate FFT data exists
        if (s.fftDb.empty())
        {
            g.setColour (theme.warning);
            g.setFont (smallFont);
            g.drawText ("NO DATA: FFT empty", getLocalBounds(), juce::Justification::centred);
            return;
        }
        paintFFTMode (g, s, theme);
    }

#if JUCE_DEBUG
    // Draw debug overlay (top-left corner)
    juce::String modeStr = (debugViewMode == 0) ? "FFT" : ((debugViewMode == 1) ? "LOG" : "BANDS");
    juce::String bandModeStr = (debugBandMode == 0) ? "1/3 Oct" : "1 Oct";
    juce::String debugText;
    debugText += "Mode: " + modeStr;
    if (debugViewMode == 2)
        debugText += " (" + bandModeStr + ")";
    debugText += "\n";
    debugText += "Gen: " + juce::String (debugStructuralGen) + "\n";
    debugText += "FFT: " + juce::String (debugFFTSize) + " " + (debugFFTValid ? "ok" : "X") + "\n";
    debugText += "LOG: " + juce::String (debugLogSize) + " " + (debugLogValid ? "ok" : "X") + "\n";
    debugText += "BANDS: " + juce::String (debugBandsSize) + " " + (debugBandsValid ? "ok" : "X");
    if (debugViewMode == 2 && debugBandsSize > 0)
    {
        debugText += "\n";
        debugText += "dB: " + juce::String (debugMinDb, 1) + " to " + juce::String (debugMaxDb, 1);
        if (debugPeakMaxDb > debugMinDb)
            debugText += "\nPeak: " + juce::String (debugPeakMinDb, 1) + " to " + juce::String (debugPeakMaxDb, 1);
    }
    
    g.setColour (theme.warning.withAlpha (0.8f));
    g.setFont (smallFont);
    const int textWidth = 180;
    const int textHeight = 140;
    const int margin = 10;
    g.drawText (debugText,
                margin, margin,
                textWidth, textHeight,
                juce::Justification::topLeft);
#endif
}

// Helper functions defined above (freqToX, dbToY, computeLogFreqFromIndex, findNearestLogBand)

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
            const bool isMajor = (freq == 20.0f || freq == 100.0f || freq == 1000.0f || freq == 10000.0f || freq == 20000.0f);
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
        peakStyle.decimationMode = mdsp_ui::SeriesStyle::DecimationMode::Simple;
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
                const float db = s.bandsPeakDb[idx];
                return dbToY (db, s);
            },
            theme.seriesPeak, peakStyle);
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
        mdsp_ui::PlotFrameRenderer::draw (g, tooltipBounds, theme, tooltipStyle);

        g.setFont (smallFont);
        g.setColour (theme.text);

        // Format frequency
        juce::String freqStr;
        if (centerFreq >= 1000.0f)
            freqStr = juce::String (centerFreq / 1000.0f, 2) + " kHz";
        else
            freqStr = juce::String (centerFreq, 1) + " Hz";

        g.drawText ("Fc: " + freqStr, static_cast<int> (tooltipX + 5), static_cast<int> (tooltipY + 5), 100, 12, juce::Justification::left);
        g.drawText ("Cur: " + juce::String (currentDb, 1) + " dB", static_cast<int> (tooltipX + 5), static_cast<int> (tooltipY + 18), 100, 12, juce::Justification::left);
        
        if (hasPeaks && peakDb > s.bottomDb)
            g.drawText ("Peak: " + juce::String (peakDb, 1) + " dB", static_cast<int> (tooltipX + 5), static_cast<int> (tooltipY + 31), 100, 12, juce::Justification::left);
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
    
    // B4: Draw band bars - compute x positions from index on-the-fly
    g.setColour (theme.accent.withAlpha (0.7f));
    for (int i = 0; i < numBands; ++i)
    {
        // Compute left/right boundaries in log space
        const float logPosLow = logMin + (logRange * static_cast<float> (i)) / static_cast<float> (numBands);
        const float logPosHigh = logMin + (logRange * static_cast<float> (i + 1)) / static_cast<float> (numBands);
        const float fL = std::pow (10.0f, logPosLow);
        const float fR = std::pow (10.0f, logPosHigh);
        
        // Map to x coordinates
        const float xL = freqToX (fL, s);
        const float xR = freqToX (fR, s);
        
        // Apply display gain in dbToY, so just pass raw dB (clamping happens in dbToY)
        const auto idx = static_cast<size_t> (i);
        const float db = s.logDb[idx];
        const float y = dbToY (db, s);
        const float bottomY = plotAreaTop + plotAreaHeight;

        if (y < bottomY && xR > xL)
        {
            g.fillRect (xL, y, xR - xL, bottomY - y);
        }
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
        peakStyle.decimationMode = mdsp_ui::SeriesStyle::DecimationMode::Simple;
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
                const float db = s.logPeakDb[idx];
                return dbToY (db, s);
            },
            theme.seriesPeak, peakStyle);
    }
    
    // Frequency-axis hover readout (Log mode only)
    if (hoverActive && state.viewMode == 1)
    {
        // Draw vertical cursor line at resolved position (if snapped)
        if (lastHoverTickIndex >= 0)
        {
            const juce::Rectangle<int> plotBounds (static_cast<int> (plotAreaLeft),
                                                   static_cast<int> (plotAreaTop),
                                                   static_cast<int> (plotAreaWidth),
                                                   static_cast<int> (plotAreaHeight));
            const float cursorX = mdsp_ui::AxisInteraction::cursorLineX (plotBounds, lastHoverPosPx);
            g.setColour (theme.text.withAlpha (0.25f));
            g.drawVerticalLine (static_cast<int> (cursorX), plotAreaTop, plotAreaTop + plotAreaHeight);
        }
        
        // Draw frequency readout box (bottom-left inside plot area)
        juce::String freqStr = mdsp_ui::AxisInteraction::formatFrequencyHz (lastHoverFreqHz);
        
        const float readoutX = plotAreaLeft + 8.0f;
        const float readoutY = plotAreaTop + plotAreaHeight - 28.0f;
        const float readoutW = 80.0f;
        const float readoutH = 20.0f;
        
        mdsp_ui::PlotFrameStyle readoutStyle;
        readoutStyle.cornerRadiusPx = 3.0f;
        readoutStyle.borderThicknessPx = 1.0f;
        readoutStyle.borderAlpha = 0.9f;
        readoutStyle.fillAlpha = 0.9f;
        readoutStyle.drawFill = true;
        readoutStyle.drawBorder = true;
        readoutStyle.clipToFrame = false;
        
        const juce::Rectangle<float> readoutBounds (readoutX, readoutY, readoutW, readoutH);
        mdsp_ui::PlotFrameRenderer::draw (g, readoutBounds, theme, readoutStyle);
        
        g.setFont (smallFont);
        g.setColour (theme.text);
        g.drawText ("f: " + freqStr, static_cast<int> (readoutX + 4), static_cast<int> (readoutY + 2), 
                    static_cast<int> (readoutW - 8), static_cast<int> (readoutH - 4), juce::Justification::centredLeft);
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

    // B5: Draw FFT spectrum as polyline using log mapping (matches grid) using SeriesRenderer
    const int numBins = static_cast<int> (s.fftDb.size());
    // Calculate frequency per bin: binHz = i * (sampleRate / fftSize)
    const double binWidthHz = s.sampleRate / static_cast<double> (s.fftSize);

#if JUCE_DEBUG
    // Compute min/max for debug overlay (throttled logging)
    static uint64_t lastMinMaxLogMs = 0;
    const uint64_t nowMs = static_cast<uint64_t> (juce::Time::getMillisecondCounterHiRes());
    if (nowMs - lastMinMaxLogMs > 1000 && !s.fftDb.empty())  // Once per second
    {
        float minDb = s.fftDb[static_cast<size_t> (0)];
        float maxDb = s.fftDb[static_cast<size_t> (0)];
        for (float db : s.fftDb)
        {
            minDb = juce::jmin (minDb, db);
            maxDb = juce::jmax (maxDb, db);
        }
        DBG ("FFT range: min=" + juce::String (minDb, 1) + " dB max=" + juce::String (maxDb, 1) + " dB");
        lastMinMaxLogMs = nowMs;
    }
#endif

    const juce::Rectangle<float> plotBounds (plotAreaLeft, plotAreaTop, plotAreaWidth, plotAreaHeight);
    mdsp_ui::SeriesStyle spectrumStyle;
    spectrumStyle.strokeThickness = 1.5f;
    spectrumStyle.alpha = 0.7f;
    spectrumStyle.clipToPlot = true;
    spectrumStyle.minXStepPx = 1.0f;
    spectrumStyle.minYStepPx = 0.5f;
    spectrumStyle.useRoundedJoins = true;
#if JUCE_DEBUG
    spectrumStyle.decimationMode = useEnvelopeDecimator
        ? mdsp_ui::DecimationMode::Envelope
        : mdsp_ui::DecimationMode::Simple;
#else
    spectrumStyle.decimationMode = mdsp_ui::SeriesStyle::DecimationMode::Simple;
#endif
    spectrumStyle.envelopeMinBucketPx = 1.0f;
    spectrumStyle.envelopeDrawVertical = true;

    mdsp_ui::SeriesRenderer::drawPathFromMapping (g, plotBounds, theme, numBins,
        [&s, binWidthHz, this] (int i) -> float
        {
        const float freq = static_cast<float> (i * binWidthHz);
        if (freq < s.minHz || freq > s.maxHz)
                return std::numeric_limits<float>::quiet_NaN();  // Skip outside range
            return freqToX (freq, s);
        },
        [&s, binWidthHz, this] (int i) -> float
        {
            const float freq = static_cast<float> (i * binWidthHz);
            if (freq < s.minHz || freq > s.maxHz)
                return std::numeric_limits<float>::quiet_NaN();  // Skip outside range
            const auto idx = static_cast<size_t> (i);
            const float db = s.fftDb[idx];
            return dbToYWithCompensation (db, freq, s);
        },
        theme.accent, spectrumStyle);

    // B5: Draw peak trace if available (using local hasPeaks boolean, NO state mutation) using SeriesRenderer
    if (hasPeaks)
    {
        mdsp_ui::SeriesStyle peakStyle;
        peakStyle.strokeThickness = 1.0f;
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
        peakStyle.decimationMode = mdsp_ui::SeriesStyle::DecimationMode::Simple;
#endif
        peakStyle.envelopeMinBucketPx = 1.0f;
        peakStyle.envelopeDrawVertical = true;

        mdsp_ui::SeriesRenderer::drawPathFromMapping (g, plotBounds, theme, numBins,
            [&s, binWidthHz, this] (int i) -> float
            {
            const float freq = static_cast<float> (i * binWidthHz);
            if (freq < s.minHz || freq > s.maxHz)
                    return std::numeric_limits<float>::quiet_NaN();  // Skip outside range
                return freqToX (freq, s);
            },
            [&s, binWidthHz, this] (int i) -> float
            {
                const float freq = static_cast<float> (i * binWidthHz);
                if (freq < s.minHz || freq > s.maxHz)
                    return std::numeric_limits<float>::quiet_NaN();  // Skip outside range
                const auto idx = static_cast<size_t> (i);
                const float db = s.fftPeakDb[idx];
                return dbToYWithCompensation (db, freq, s);
            },
            theme.seriesPeak, peakStyle);
    }
}
