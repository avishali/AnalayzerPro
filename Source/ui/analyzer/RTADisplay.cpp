#include "RTADisplay.h"
#include <mdsp_ui/rta/RTADisplayRenderer.h>
#include <mdsp_ui/rta/RTACurveHelpers.h>
#include <mdsp_ui/rta/RTADisplayModel.h>
#include <mdsp_ui/Theme.h>
#include <cmath>
#include <cstdint>
//#include <algorithm>
//#include <limits>
#include <juce_gui_basics/juce_gui_basics.h>

static const char* kBuildStamp = "BUILD_STAMP: 2026-02-16_XXYY_CURSOR";
// NOTE (audit trail):
// "Slice 5: All paint logic moved to RTADisplayRenderer. RTADisplay is now a thin wrapper."

//==============================================================================
RTADisplay::RTADisplay()
    : controller_ (model_, geometry_)
{
    const auto& s = model_.getState();
    geometry_.updateConfig (s.minHz, s.maxHz, s.topDb, s.bottomDb,
                           s.sampleRate, s.fftSize);
    controller_.setCallbacks ({
        [this]() { repaint(); },
        [this](int px, int py, int pw, int ph) { repaint (px, py, pw, ph); }
    });
    controller_.setDisplayGainDb (displayGainDb);
    controller_.setTiltMode (tiltMode);
    startTimerHz (60);
}

RTADisplay::~RTADisplay() = default;

void RTADisplay::timerCallback()
{
    controller_.onTimerTick();
}

//==============================================================================
void RTADisplay::setBandData (const std::vector<float>& currentDb, const std::vector<float>* peakDbNullable)
{
    model_.setBandData (currentDb, peakDbNullable);
    repaint();
}

void RTADisplay::setViewMode (int mode)
{
    const auto& s = model_.getState();
    if (s.viewMode != mode)
    {
        model_.setViewMode (mode);
#if JUCE_DEBUG
        debugViewMode = mode;
#endif
        controller_.onViewModeChanged();
        repaint();
    }
}

void RTADisplay::setFFTData (const std::vector<float>& fftBinsDb, 
                             const std::vector<float>* peakBinsDbNullable,
                             const std::vector<float>* peakHoldBinsDbNullable)
{
    model_.setFFTData (fftBinsDb, peakBinsDbNullable, peakHoldBinsDbNullable);
    controller_.onFftDataChanged();
    repaint();
}

void RTADisplay::setLogData (const std::vector<float>& logBandsDb, const std::vector<float>* peakBandsDbNullable)
{
    model_.setLogData (logBandsDb, peakBandsDbNullable);
    repaint();
}

void RTADisplay::setBandCenters (const std::vector<float>& centersHz)
{
    model_.setBandCenters (centersHz);
    controller_.onStructuralReset();
    const auto& s = model_.getState();
    geometry_.updateBandCenters (s.bandCentersHz);
    repaint();
}

void RTADisplay::setLogCenters (const std::vector<float>&)
{
    model_.setLogCenters ({});
}

void RTADisplay::setFftMeta (double sampleRate, int fftSize)
{
    const auto& s = model_.getState();
    const bool metaChanged = (std::abs (s.sampleRate - sampleRate) > 1e-5 || s.fftSize != fftSize);
    model_.setFftMeta (sampleRate, fftSize);
    if (metaChanged)
        controller_.onStructuralReset();
    const auto& s2 = model_.getState();
    geometry_.updateConfig (s2.minHz, s2.maxHz, s2.topDb, s2.bottomDb,
                           s2.sampleRate, s2.fftSize);
    repaint();
}

void RTADisplay::setFrequencyRange (float minHz, float maxHz)
{
    const auto& s = model_.getState();
    if (std::abs (s.minHz - minHz) > 1e-5f || std::abs (s.maxHz - maxHz) > 1e-5f)
    {
        model_.setFrequencyRange (minHz, maxHz);
        const auto& s2 = model_.getState();
        geometry_.updateConfig (s2.minHz, s2.maxHz, s2.topDb, s2.bottomDb,
                               s2.sampleRate, s2.fftSize);
        geometry_.updateBandCenters (s2.bandCentersHz);
        repaint();
    }
}

void RTADisplay::setDbRange (float topDb, float bottomDb)
{
    const auto& s = model_.getState();
    if (std::abs (s.topDb - topDb) > 1e-5f || std::abs (s.bottomDb - bottomDb) > 1e-5f)
    {
        model_.setDbRange (topDb, bottomDb);
        const auto& s2 = model_.getState();
        geometry_.updateConfig (s2.minHz, s2.maxHz, s2.topDb, s2.bottomDb,
                               s2.sampleRate, s2.fftSize);
        repaint();
    }
}

void RTADisplay::setNoData (const juce::String& reason)
{
    model_.setNoData (reason);
    repaint();
}

void RTADisplay::setDisplayGainDb (float db)
{
    displayGainDb = juce::jlimit (-24.0f, 24.0f, db);
    controller_.setDisplayGainDb (displayGainDb);
#if JUCE_DEBUG
    DBG ("DisplayGain=" << displayGainDb << "dB");
#endif
    const auto& s = model_.getState();
    geometry_.updateConfig (s.minHz, s.maxHz, s.topDb, s.bottomDb,
                           s.sampleRate, s.fftSize);
    model_.invalidatePaths();
    repaint();
}

void RTADisplay::setTiltMode (TiltMode mode)
{
    if (tiltMode != mode)
    {
        tiltMode = mode;
        controller_.setTiltMode (mode);
        model_.invalidatePaths();
        repaint();
    }
}

void RTADisplay::setTraceConfig (const TraceConfig& config)
{
    if (traceConfig_.showL == config.showL &&
        traceConfig_.showR == config.showR &&
        traceConfig_.showMono == config.showMono &&
        traceConfig_.showMid == config.showMid &&
        traceConfig_.showSide == config.showSide &&
        traceConfig_.showLR == config.showLR &&
        traceConfig_.showRMS == config.showRMS &&
        traceConfig_.weightingMode == config.weightingMode)
    {
        return;
    }

    traceConfig_ = config;
    model_.invalidatePaths();
    repaint();
}

void RTADisplay::setMultiTraceData (const float* powerL, const float* powerR,
                                    const float* powerMid, const float* powerSide, const float* powerMono,
                                    int binCount)
{
    model_.setMultiTraceData (powerL, powerR, powerMid, powerSide, powerMono, binCount);
    repaint();
}
float RTADisplay::computeTiltDb (float freqHz) const
{
    return mdsp_ui::rta::computeTiltDb (freqHz, tiltMode);
}

float RTADisplay::dbToYWithCompensation (float db, float freqHz, const RenderState& s) const
{
    (void) s;
    const float tiltDb = computeTiltDb (freqHz);
    const float weightingDb = 0.0f;  // Weighting applied engine-side before display
    return geometry_.dbToYWithCompensation (db + displayGainDb, freqHz, tiltDb, weightingDb);
}

void RTADisplay::setHoldStatus (bool isHoldOn)
{
    model_.setHoldStatus (isHoldOn);
}

void RTADisplay::setSessionMarker (bool visible, int bin, float db)
{
    const auto& s = model_.getState();
    if (s.sessionMarkerVisible != visible || 
        s.sessionMarkerBin != bin || 
        std::abs (s.sessionMarkerDb - db) > 1e-4f)
    {
        model_.setSessionMarker (visible, bin, db);
        repaint();
    }
}

void RTADisplay::checkStructuralGeneration (uint32_t currentGen)
{
    if (currentGen != model_.getLastStructuralGen())
    {
        model_.checkStructuralGeneration (currentGen);
        controller_.onStructuralReset();
        repaint();
    }
}

void RTADisplay::setGenerations (uint32_t traceDataGen, uint32_t smoothingGen)
{
    model_.setGenerations (traceDataGen, smoothingGen);
}

void RTADisplay::invalidatePaths()
{
    model_.invalidatePaths();
}

void RTADisplay::invalidateBackground()
{
    model_.invalidateBackground();
    repaint();
}

void RTADisplay::setGetRenderState (std::function<mdsp_ui::AnalyzerRenderState()> cb)
{
    getRenderState_ = std::move (cb);
}

void RTADisplay::setGetTheme (std::function<const mdsp_ui::Theme&()> cb)
{
    getTheme_ = std::move (cb);
}

void RTADisplay::refreshBackground()
{
    const auto& s = model_.getState();
    const mdsp_ui::Theme& theme = (getTheme_ ? getTheme_() : fallbackTheme_);
    model_.refreshBackground (geometry_, s, displayGainDb, theme,
                              [this](juce::Graphics& g, const RenderState& state, const mdsp_ui::Theme& th) {
                                  renderer_.drawGrid (g, state, geometry_, th, displayGainDb);
                              });
}

// buildFftPaths and buildDecimatedPath moved to RTADisplayModel

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
    geometry_.updateLayout (getLocalBounds().toFloat());
    const auto& s = model_.getState();
    geometry_.updateBandCenters (s.bandCentersHz);
    invalidateBackground();
    invalidatePaths();
    controller_.onLayoutChanged();
}

//==============================================================================
// Mapping functions now handled by RTAGeometry - these are thin wrappers for compatibility
int RTADisplay::findNearestBand (float x) const
{
    return geometry_.findNearestBand (x);
}

int RTADisplay::findNearestLogBand (float x, const RenderState& s) const
{
    return geometry_.findNearestLogBand (x, s.logDb);
}

float RTADisplay::mapXToFreqFFT (float x, const RenderState& s) const
{
    const float plotLeft = geometry_.getPlotAreaLeft();
    const float plotWidth = geometry_.getPlotAreaWidth();
    if (plotWidth <= 0.0f || s.maxHz <= s.minHz || s.minHz <= 0.0f)
        return s.minHz;
    const float maxHz = s.getEffectiveMaxHz();
    if (maxHz <= s.minHz)
        return s.minHz;
    const float norm = (x - plotLeft) / plotWidth;
    const float logMin = std::log10 (s.minHz);
    const float logMax = std::log10 (maxHz);
    const float logRange = logMax - logMin;
    const float logFreq = logMin + norm * logRange;
    return std::pow (10.0f, juce::jlimit (logMin, logMax, logFreq));
}

int RTADisplay::mapFreqToBinIndex (float freqHz, const RenderState& s) const
{
    return geometry_.mapFreqToBinIndex (freqHz, s.sampleRate, s.fftSize);
}

float RTADisplay::getActiveTraceDbAtBin (int binIndex, const RenderState& s) const
{
    const size_t idx = static_cast<size_t> (binIndex);
    if (idx >= s.fftDb.size())
        return -200.0f;
    if (!s.fftPeakDb.empty() && s.fftPeakDb.size() == s.fftDb.size() && idx < s.fftPeakDb.size())
    {
        const float v = s.fftPeakDb[idx];
        return std::isfinite (v) ? v : -200.0f;
    }
    const float v = s.fftDb[idx];
    return std::isfinite (v) ? v : -200.0f;
}

void RTADisplay::mouseMove (const juce::MouseEvent& e)
{
    controller_.onMouseMove (e);
}

void RTADisplay::mouseExit (const juce::MouseEvent& e)
{
    controller_.onMouseExit (e);
}

void RTADisplay::mouseDown (const juce::MouseEvent& e)
{
    controller_.onMouseDown (e);
}

void RTADisplay::mouseDrag (const juce::MouseEvent& e)
{
    controller_.onMouseDrag (e);
}

void RTADisplay::mouseUp (const juce::MouseEvent& e)
{
    controller_.onMouseUp (e);
}

//==============================================================================
void RTADisplay::paint (juce::Graphics& g)
{
    #if JUCE_DEBUG
    g.setColour(juce::Colours::white);
    g.drawText(kBuildStamp, 10, 10, 600, 20, juce::Justification::left);
    #endif
    const mdsp_ui::Theme& theme = (getTheme_ ? getTheme_() : fallbackTheme_);
    
    // Compute intended background bounds (must match what refreshBackground builds for)
    const auto intendedBounds = juce::Rectangle<int> (0, 0,
                                                      static_cast<int>(geometry_.getPlotAreaWidth() + 60),
                                                      static_cast<int>(geometry_.getPlotAreaHeight() + 40));
    
    // Ensure background is built before painting
    if (!model_.isBackgroundValid() || model_.getCachedBackground().getBounds() != intendedBounds)
    {
        refreshBackground();
    }
    
    // Draw cached background image using bounds
    const auto& bg = model_.getCachedBackground();
    const auto b = bg.getBounds();
    if (bg.isValid() && b.getWidth() > 0 && b.getHeight() > 0)
    {
        g.drawImageWithin (bg, b.getX(), b.getY(), b.getWidth(), b.getHeight(), juce::RectanglePlacement::stretchToFit);
    }
    
    // Draw traces and overlays via renderer (renderer must NOT draw grid)
    if (getRenderState_)
    {
        renderer_.paint (g, model_, geometry_, controller_, theme, displayGainDb, tiltMode, traceConfig_,
                         getLocalBounds(), getRenderState_);
    }
    else
    {
        renderer_.paint (g, model_, geometry_, controller_, theme, displayGainDb, tiltMode, traceConfig_,
                         getLocalBounds(), nullptr);
    }
}
