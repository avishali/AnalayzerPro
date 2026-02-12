#include "AnalyzerDisplayView.h"
#include <mdsp_gui/dsp/AnalyzerSettings.h>
#include <mdsp_ui/Theme.h>
#include <cmath>
#include <limits>

#if !defined(ANALYZERPRO_MODE_DEBUG_OVERLAY)
#define ANALYZERPRO_MODE_DEBUG_OVERLAY 1
#endif

#if !defined(ANALYZERPRO_FFT_DEBUG_LINE)
#define ANALYZERPRO_FFT_DEBUG_LINE 1
#endif

//==============================================================================
static float dbRangeToMinDb (AnalyzerDisplayView::DbRange r) noexcept
{
    switch (r)
    {
        case AnalyzerDisplayView::DbRange::Minus60:  return -60.0f;
        case AnalyzerDisplayView::DbRange::Minus90:  return -90.0f;
        case AnalyzerDisplayView::DbRange::Minus120: return -120.0f;
    }
    return -120.0f;
}

// UI-only sentinel used to detect invalid peak values coming from legacy paths.
// This is NOT a display floor and must not track user-selected dB range.
static constexpr float kUiPeakInvalidSentinelDb = -90.0f;

static inline bool isInvalidPeakDb (float db) noexcept
{
    return db <= kUiPeakInvalidSentinelDb;
}



AnalyzerDisplayView::AnalyzerDisplayView (AnalayzerProAudioProcessor& processor)
    : audioProcessor (processor)
#if JUCE_DEBUG
    , lastDebugLogTime_ (juce::Time::getCurrentTime())
#endif
{
    addAndMakeVisible (rtaDisplay);
    // DISABLED: spectrumEngine was causing flat yellow line at maximum
    // addAndMakeVisible (spectrumEngine);
    // spectrumEngine.setAudioBufferQueue (&audioProcessor.getSpectrumBufferQueue());

    // DISABLED: Use mdsp_gui default "Yellow Peak" aesthetic (sharp yellow stroke, gradient fill)
    // spectrumEngine.setStyle (mdsp::gui::SpectrumComponent::Style{});

    // DISABLED: Initial FFT order: 4096 (order 12) for high-resolution spectrum
    // spectrumEngine.setFftOrder (mdsp::gui::SpectrumComponent::defaultFftOrder);
    // Initialize RTADisplay with default ranges
    rtaDisplay.setFrequencyRange (20.0f, 20000.0f);
    targetMinDb_ = dbRangeToMinDb (dbRange_);
    minDbAnim_.reset (60.0, 0.20);
    minDbAnim_.setCurrentAndTargetValue (targetMinDb_);
    lastAppliedMinDb_ = targetMinDb_;
    rtaDisplay.setDbRange (0.0f, lastAppliedMinDb_);
    appliedDbRange_ = dbRange_;
    
    // Initialize band centers
    bandCentersHz_ = generateThirdOctaveBands();
    
    // Sync initial mode to RTADisplay (currentMode_ defaults to FFT)
    rtaDisplay.setViewMode (toRtaMode (currentMode_));
#if JUCE_DEBUG
    lastSentRtaMode_ = toRtaMode (currentMode_);
#endif
    
#if JUCE_DEBUG && ANALYZERPRO_MODE_DEBUG_OVERLAY
    addAndMakeVisible (modeOverlay_);
    modeOverlay_.setInterceptsMouseClicks (false, false);
    updateModeOverlayText();
#endif
    
    // Start timer for snapshot updates (~60 Hz) and dB range animation
    startTimerHz (60);
    

}

void AnalyzerDisplayView::setDbRange (DbRange r)
{
    if (dbRange_ == r)
        return;

    dbRange_ = r;
    targetMinDb_ = dbRangeToMinDb (dbRange_);

    minDbAnim_.reset (60.0, 0.20);
    minDbAnim_.setTargetValue (targetMinDb_);
    repaint();
}

void AnalyzerDisplayView::setDbRangeFromChoiceIndex (int idx)
{
    idx = juce::jlimit (0, 2, idx);
    setDbRange (static_cast<DbRange> (idx));
}

void AnalyzerDisplayView::setPeakDbRange (DbRange r)
{
    if (peakDbRange_ == r)
        return;

    peakDbRange_ = r;
    peakScaleDirty_ = true;
    repaint();
}

void AnalyzerDisplayView::resetSessionMarker()
{
    sessionMarkerValid_ = false;
    sessionMarkerDb_ = -1000.0f;
    sessionMarkerBin_ = -1;
    // Force immediate update to display
    rtaDisplay.setSessionMarker (false, -1, -1000.0f);
}

void AnalyzerDisplayView::resetViewPeaks()
{
    // Clear UI-side latch buffer
    std::fill (uiHeldPeak_.begin(), uiHeldPeak_.end(), -120.0f);

    // CRITICAL: Clear UI-side peak trace buffers that retain stale max values
    // These are copies from snapshots and won't reset automatically
    std::fill (fftPeakDb_.begin(), fftPeakDb_.end(), -120.0f);
    std::fill (fftDb_.begin(), fftDb_.end(), -120.0f);
    std::fill (fftPeakDbDisplay_.begin(), fftPeakDbDisplay_.end(), -120.0f);

    // Clear multi-trace UI buffers
    std::fill (scratchPowerL_.begin(), scratchPowerL_.end(), -120.0f);
    std::fill (scratchPowerR_.begin(), scratchPowerR_.end(), -120.0f);
    std::fill (scratchPowerMid_.begin(), scratchPowerMid_.end(), -120.0f);
    std::fill (scratchPowerSide_.begin(), scratchPowerSide_.end(), -120.0f);
    std::fill (scratchPowerMono_.begin(), scratchPowerMono_.end(), -120.0f);

    // Clear session marker
    resetSessionMarker();

    // Trigger flash for visual feedback
    triggerPeakFlash();

    // Force remap/repaint
    peakScaleDirty_ = true;
    repaint();
}

void AnalyzerDisplayView::triggerPeakFlash()
{
    peakFlashActive_ = true;
    peakFlashUntilMs_ = juce::Time::getMillisecondCounterHiRes() + 150.0;
    peakScaleDirty_ = true;
    repaint();
}

AnalyzerDisplayView::~AnalyzerDisplayView()
{
    shutdown();
}

void AnalyzerDisplayView::shutdown()
{
    if (isShutdown)
        return;

    isShutdown = true;

    stopTimer();          // CRITICAL
    //cancelPendingUpdate(); // if AsyncUpdater ever used later

    // Shutdown complete
}

//==============================================================================
// dB sanitization helper: clamps to [-120, 24] dB and replaces non-finite with floor
static inline float sanitizeDb (float db) noexcept
{
    if (!std::isfinite (db))
        return -200.0f;
    // Use -200 dB internal floor to avoid hard-clamping artifacts (flat/squared segments)
    // in smoothed traces. The display range handles the visible floor.
    return juce::jlimit (-200.0f, 24.0f, db);
}

#if JUCE_DEBUG && ANALYZERPRO_MODE_DEBUG_OVERLAY
static const char* uiModeToString (AnalyzerDisplayView::Mode m) noexcept
{
    switch (m)
    {
        case AnalyzerDisplayView::Mode::FFT:
            return "FFT";
        case AnalyzerDisplayView::Mode::LOG:
            return "LOG";
        case AnalyzerDisplayView::Mode::BAND:
            return "BANDS";
        default:
            return "UNKNOWN";
    }
}

static const char* rtaModeToString (int rtaMode) noexcept
{
    // RTADisplay: 0=FFT, 1=LOG, 2=BAND
    switch (rtaMode)
    {
        case 0:
            return "FFT";
        case 1:
            return "LOG";
        case 2:
            return "BANDS";
        default:
            return "UNKNOWN";
    }
}
#endif

//==============================================================================
//==============================================================================
void AnalyzerDisplayView::paint (juce::Graphics& g)
{
    // Background is handled by RTADisplay
    juce::ignoreUnused (g);
}

void AnalyzerDisplayView::paintOverChildren (juce::Graphics& g)
{
    // Bypass Overlay
    if (audioProcessor.getBypassState())
    {
        const mdsp_ui::Theme theme (mdsp_ui::ThemeVariant::Dark); // Stick to Dark for now
        g.setColour (theme.background.withAlpha (0.6f));
        g.fillAll();
        
        g.setColour (theme.danger);
        g.setFont (juce::Font (juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 24.0f, juce::Font::bold)));
        g.drawText ("BYPASS", getLocalBounds(), juce::Justification::centred);
    }

    // Default theme (Dark variant) for debug overlays
    // Note: AnalyzerDisplayView doesn't use UiContext yet (Phase 2 may add it)
    const mdsp_ui::Theme theme (mdsp_ui::ThemeVariant::Dark);
#if JUCE_DEBUG
    // Debug overlay: mode, sequence, bins, fftSize, meta, drop reason (top-left)
    g.setFont (juce::Font (juce::FontOptions().withHeight (10.0f)));
    g.setColour (theme.text.withAlpha (0.7f));
    
    juce::String modeStr = "FFT";
    if (currentMode_ == Mode::BAND)
        modeStr = "BANDS";
    else if (currentMode_ == Mode::LOG)
        modeStr = "LOG";
    
    juce::String debugText = "mode=" + modeStr
                           + " bins=" + juce::String (lastBins_) + "/" + juce::String (expectedBins_)
                           + " meta=" + juce::String (static_cast<int> (lastMetaSampleRate_)) + "," + juce::String (lastMetaFftSize_);
    
    if (!dropReason_.isEmpty())
    {
        debugText += " " + dropReason_;
        g.setColour (theme.danger.withAlpha (0.85f));
    }
    
    g.drawText (debugText, 8, 8, 500, 12, juce::Justification::centredLeft);
    
#if ANALYZERPRO_FFT_DEBUG_LINE
    // DEBUG TEMP: remove once FFT validated
    // FFT debug line: mode + bins + min/max dB + fftSize
    if (!fftDebugLine_.isEmpty())
    {
        g.setColour (theme.accent.withAlpha (0.8f));
        g.setFont (juce::Font (juce::FontOptions().withHeight (10.0f)));
        g.drawText (fftDebugLine_, 8, 22, 600, 12, juce::Justification::centredLeft);
    }
#endif
#endif

#if defined(PLUGIN_DEV_MODE) && PLUGIN_DEV_MODE
    // Temporary debug overlay: UI=mode / RTADisplay=mode / bins / min/max dB
    if (!devModeDebugLine_.isEmpty())
    {
        g.setColour (theme.warning.withAlpha (0.90f));
        g.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)));
        g.drawText (devModeDebugLine_, 8, 38, 700, 14, juce::Justification::centredLeft);
    }
#endif
}

//==============================================================================
void AnalyzerDisplayView::mouseDown (const juce::MouseEvent& e)
{
    dragStartPos_ = e.position;
    dragStartDbRange_ = dbRange_;
}

void AnalyzerDisplayView::mouseDrag (const juce::MouseEvent& e)
{
    const float dy = e.position.y - dragStartPos_.y;
    
    // Y-Axis interaction: Drag Vertical to change DbRange
    // Threshold: 60 pixels per step feels sufficient
    // Drag Up (negative Y) -> Increase Range (more negative, e.g. -120dB)
    // Drag Down (positive Y) -> Decrease Range (less negative, e.g. -60dB)
    // Map:
    // -60  (Index 0)
    // -90  (Index 1)
    // -120 (Index 2)
    
    // If we drag DOWN (+Y), we want to go from -120(2) to -60(0). So current - steps.
    // If we drag UP (-Y), we want to go from -60(0) to -120(2). So current + steps.
    
    // logic: 
    // deltaY positive (Down): should reduce index?
    // -120 to -60 is moving "Up" visually? No.
    // Range -120 is "Larger" range.
    // Range -60 is "Smaller" range (Zooms in).
    // Usually Drag Down -> Zoom In. Drag Up -> Zoom Out.
    // Zoom In = -60. Zoom Out = -120.
    // So Down (+Y) -> Index 0 (-60).
    // Up (-Y) -> Index 2 (-120).
    
    // Index increases with visual height?
    // 0: -60
    // 1: -90
    // 2: -120
    
    // Step = dy / 60.
    // If dy = +60 (Down), Step = 1.
    // If I want Down -> Index 0.
    // If Start is 2 (-120). Down(+60) -> 1 (-90).
    // So target = Start - Step.
    
    const int steps = static_cast<int> (dy / 60.0f);
    
    if (steps != 0)
    {
        int startIdx = static_cast<int> (dragStartDbRange_);
        int targetIdx = juce::jlimit (0, 2, startIdx - steps);
        
        DbRange nextRange = static_cast<DbRange> (targetIdx);
        
        if (nextRange != dbRange_)
        {
            setDbRange (nextRange);
            if (onDbRangeUserChanged)
                onDbRangeUserChanged (nextRange);
        }
    }
}

void AnalyzerDisplayView::resized()
{
    auto bounds = getLocalBounds();
    rtaDisplay.setBounds (bounds);
    // DISABLED: spectrumEngine.setBounds (bounds);
    // Keep AnalyzerEngine-driven display on top (timerCallback feeds rtaDisplay via getLatestSnapshot).
    rtaDisplay.toFront (false);
#if JUCE_DEBUG && ANALYZERPRO_MODE_DEBUG_OVERLAY
    modeOverlay_.setBounds (8, 8, 260, 18);
    modeOverlay_.toFront (false);
#endif


    // Bottom-right corner reserved for future controls?
    // auto r = getLocalBounds().reduced (10);
}

//==============================================================================
//==============================================================================
std::vector<float> AnalyzerDisplayView::generateThirdOctaveBands()
{
    // Standard 1/3-octave band centers from ~20 Hz to ~20 kHz (31 bands)
    // ISO 266:1997 standard frequencies
    std::vector<float> centers;
    centers.reserve (31);
    
    // 1/3-octave centers: f = 1000 * 10^(n/10) where n ranges from -20 to +10
    // Filter to 20 Hz - 20 kHz range
    const float bandCenters[] = {
        20.0f, 25.0f, 31.5f, 40.0f, 50.0f, 63.0f, 80.0f, 100.0f, 125.0f, 160.0f,
        200.0f, 250.0f, 315.0f, 400.0f, 500.0f, 630.0f, 800.0f, 1000.0f, 1250.0f, 1600.0f,
        2000.0f, 2500.0f, 3150.0f, 4000.0f, 5000.0f, 6300.0f, 8000.0f, 10000.0f, 12500.0f, 16000.0f, 20000.0f
    };
    
    for (float center : bandCenters)
    {
        if (center >= 20.0f && center <= 20000.0f)
            centers.push_back (center);
    }
    
    return centers;
}

//==============================================================================
void AnalyzerDisplayView::convertFFTToBands (const AnalyzerSnapshot& snapshot, std::vector<float>& bandsDb, std::vector<float>& bandsPeakDb)
{
    if (bandCentersHz_.empty())
    {
        bandCentersHz_ = generateThirdOctaveBands();
        rtaDisplay.setBandCenters (bandCentersHz_);
    }
    
    const size_t numBands = bandCentersHz_.size();
    bandsDb.resize (numBands, -120.0f);
    bandsPeakDb.resize (numBands, -120.0f);
    
    const double sampleRate = snapshot.sampleRate;
    const int fftSize = snapshot.fftSize;
    const int fftBinCount = (snapshot.fftBinCount > 0) ? snapshot.fftBinCount : snapshot.numBins;
    const double binWidthHz = sampleRate / static_cast<double> (fftSize);
    
    // For each band, compute lower and upper frequency edges
    // 1/3-octave: lower = center / 10^(1/6), upper = center * 10^(1/6)
    const double thirdOctaveRatio = std::pow (10.0, 1.0 / 6.0);  // ~1.122462
    
    for (size_t bandIdx = 0; bandIdx < numBands; ++bandIdx)
    {
        const float centerFreq = bandCentersHz_[bandIdx];
        const double lowerFreq = centerFreq / thirdOctaveRatio;
        const double upperFreq = centerFreq * thirdOctaveRatio;
        
        // Find FFT bins that fall within this band
        int lowerBin = static_cast<int> (std::floor (lowerFreq / binWidthHz));
        int upperBin = static_cast<int> (std::ceil (upperFreq / binWidthHz));
        
        // Clamp to valid bin range
        lowerBin = juce::jmax (0, lowerBin);
        upperBin = juce::jmin (fftBinCount - 1, upperBin);
        
        // If lowerBin > upperBin, collapse to nearest valid bin
        if (lowerBin > upperBin)
        {
            const int centerBin = (lowerBin + upperBin) / 2;
            lowerBin = juce::jlimit (0, fftBinCount - 1, centerBin);
            upperBin = lowerBin;
        }
        
        // Sum power (not dB) of bins within band for average level
        double sumPower = 0.0;
        int binCount = 0;
        float maxPeakDb = -120.0f;
        
        for (int bin = lowerBin; bin <= upperBin; ++bin)
        {
            // Convert dB to linear power for averaging
            const std::size_t idx = static_cast<std::size_t> (bin);
            const float db = snapshot.fftDb[idx];
            const float power = std::pow (10.0f, db / 10.0f);
            sumPower += power;
            binCount++;
            
            // For peak: use maximum (not sum) - more stable and correct
            if (bin < static_cast<int> (fftPeakDb_.size()))
            {
                maxPeakDb = juce::jmax (maxPeakDb, fftPeakDb_[idx]);
            }
        }
        
        // Convert summed power back to dB (use average for proper band level)
        if (binCount > 0 && sumPower > 0.0)
        {
            const double avgPower = sumPower / static_cast<double> (binCount);
            bandsDb[bandIdx] = 10.0f * static_cast<float> (std::log10 (avgPower));
        }
        else
        {
            bandsDb[bandIdx] = -120.0f;  // Floor
        }
        
        // Peak is already in dB, just use the maximum
        bandsPeakDb[bandIdx] = maxPeakDb;
    }
}

//==============================================================================
void AnalyzerDisplayView::convertFFTToLog (const AnalyzerSnapshot& snapshot, std::vector<float>& logDb, std::vector<float>& logPeakDb)
{
    constexpr int numLogBins = 256;
    constexpr float minFreq = 20.0f;
    constexpr float maxFreq = 20000.0f;
    constexpr float powerFloor = 1.0e-20f;
    constexpr float dbFloor = -120.0f;
    
    logDb.resize (numLogBins, dbFloor);
    logPeakDb.resize (numLogBins, dbFloor);
    
    const double sampleRate = snapshot.sampleRate;
    const int fftBinCount = (snapshot.fftBinCount > 0) ? snapshot.fftBinCount : snapshot.numBins;
    const double binWidthHz = sampleRate / static_cast<double> (snapshot.fftSize);
    
    const double logMin = std::log10 (static_cast<double> (minFreq));
    const double logMax = std::log10 (static_cast<double> (maxFreq));
    const double logRange = logMax - logMin;
    
    std::array<float, numLogBins> logPower;
    std::fill (logPower.begin(), logPower.end(), powerFloor);
    
    for (int logIdx = 0; logIdx < numLogBins; ++logIdx)
    {
        const double logPos = logMin + (logRange * static_cast<double> (logIdx)) / static_cast<double> (numLogBins - 1);
        const double centerFreq = std::pow (10.0, logPos);
        const double nextLogPos = (logIdx < numLogBins - 1) 
            ? (logMin + (logRange * static_cast<double> (logIdx + 1)) / static_cast<double> (numLogBins - 1))
            : logMax;
        const double prevLogPos = (logIdx > 0)
            ? (logMin + (logRange * static_cast<double> (logIdx - 1)) / static_cast<double> (numLogBins - 1))
            : logMin;
        const double lowerFreq = std::pow (10.0, (logPos + prevLogPos) / 2.0);
        const double upperFreq = std::pow (10.0, (logPos + nextLogPos) / 2.0);
        
        int lowerBin = static_cast<int> (std::floor (lowerFreq / binWidthHz));
        int upperBin = static_cast<int> (std::ceil (upperFreq / binWidthHz));
        lowerBin = juce::jmax (0, lowerBin);
        upperBin = juce::jmin (fftBinCount - 1, upperBin);
        
        if (lowerBin > upperBin)
        {
            const int centerBin = static_cast<int> (std::round (centerFreq / binWidthHz));
            lowerBin = juce::jlimit (0, fftBinCount - 1, centerBin);
            upperBin = lowerBin;
        }
        
        double sumPower = 0.0;
        int binCount = 0;
        for (int bin = lowerBin; bin <= upperBin; ++bin)
        {
            const std::size_t idx = static_cast<std::size_t> (bin);
            const float db = snapshot.fftDb[idx];
            const float power = std::pow (10.0f, db / 10.0f);
            sumPower += power;
            binCount++;
        }
        
        if (binCount > 0 && sumPower > 0.0)
            logPower[static_cast<std::size_t> (logIdx)] = static_cast<float> (sumPower / static_cast<double> (binCount));
        
        float maxPeakDb = dbFloor;
        for (int bin = lowerBin; bin <= upperBin && bin < static_cast<int> (fftPeakDb_.size()); ++bin)
            maxPeakDb = juce::jmax (maxPeakDb, fftPeakDb_[static_cast<std::size_t> (bin)]);
        logPeakDb[static_cast<std::size_t> (logIdx)] = maxPeakDb;
    }
    
    const float octaves = snapshot.smoothingOctaves;
    const bool applyGaussian = (!snapshot.engineDidSpectralSmooth && octaves > 0.0f);
    if (applyGaussian)
    {
        logGaussian_.setConfig (octaves);
        logGaussian_.process (logPower.data(), numLogBins);
    }
    
    for (int logIdx = 0; logIdx < numLogBins; ++logIdx)
    {
        const float p = juce::jmax (powerFloor, logPower[static_cast<std::size_t> (logIdx)]);
        logDb[static_cast<std::size_t> (logIdx)] = (p > powerFloor) 
            ? juce::jmax (dbFloor, 10.0f * std::log10 (p)) 
            : dbFloor;
    }
}

int AnalyzerDisplayView::toRtaMode (Mode m) noexcept
{
    // RTADisplay: 0=FFT, 1=LOG, 2=BAND
    switch (m)
    {
        case Mode::FFT:
            return 0;
        case Mode::LOG:
            return 1;
        case Mode::BAND:
            return 2;
        default:
            jassertfalse;
            return 0;
    }
}

#if JUCE_DEBUG
void AnalyzerDisplayView::assertModeSync() const
{
    // RTADisplay doesn't expose getMode(), so we assert against cached value
    jassert (lastSentRtaMode_ == toRtaMode (currentMode_));
}
#endif

#if JUCE_DEBUG && ANALYZERPRO_MODE_DEBUG_OVERLAY
void AnalyzerDisplayView::updateModeOverlayText()
{
    const juce::String dbg = juce::String ("UI=") + uiModeToString (currentMode_)
                           + juce::String (" / RTADisplay=") + rtaModeToString (lastSentRtaMode_);
    modeOverlay_.setText (dbg);
}
#endif

void AnalyzerDisplayView::setMode (Mode mode)
{
    // UI selection is authoritative - update local mode
    currentMode_ = mode;
    
    // CRITICAL: Always sync RTADisplay to UI mode immediately
    // UI mode is authoritative - RTADisplay must match
    const int rtaMode = toRtaMode (currentMode_);
    rtaDisplay.setViewMode (rtaMode);
#if JUCE_DEBUG
    lastSentRtaMode_ = rtaMode;
    assertModeSync();
#endif

    // DISABLED: Sync shared spectrum engine analysis mode (Line / Log / Band)
    // mdsp::gui::SpectrumComponent::AnalysisMode specMode = mdsp::gui::SpectrumComponent::AnalysisMode::Log;
    // switch (currentMode_)
    // {
    //     case Mode::FFT:  specMode = mdsp::gui::SpectrumComponent::AnalysisMode::Line; break;
    //     case Mode::LOG:  specMode = mdsp::gui::SpectrumComponent::AnalysisMode::Log;  break;
    //     case Mode::BAND:  specMode = mdsp::gui::SpectrumComponent::AnalysisMode::Band; break;
    //     default:         specMode = mdsp::gui::SpectrumComponent::AnalysisMode::Log;  break;
    // }
    // spectrumEngine.setAnalysisMode (specMode);

#if JUCE_DEBUG && ANALYZERPRO_MODE_DEBUG_OVERLAY
    updateModeOverlayText();
#endif
    
    // Force repaint
    repaint();
}

void AnalyzerDisplayView::setSpectrumFftOrder (int order)
{
    // DISABLED: spectrumEngine.setFftOrder (order);
    juce::ignoreUnused(order);
}

void AnalyzerDisplayView::setSpectrumDecayRate (float decay)
{
    // DISABLED: spectrumEngine.setDecayRate (decay);
    juce::ignoreUnused(decay);
}

void AnalyzerDisplayView::timerCallback()
{
    // Early return if shutdown (do not rely on isTimerRunning())
    if (isShutdown)
        return;

    // Read trace configuration from APVTS and pass to RTADisplay
    auto& apvts = audioProcessor.getAPVTS();
    RTADisplay::TraceConfig traceConfig;
    
    // Helper lambda with null check
    auto getBoolParam = [&apvts](const char* id) -> bool {
        auto* param = apvts.getRawParameterValue(id);
        return (param != nullptr) ? (param->load() > 0.5f) : false;
    };
    
    traceConfig.showLR   = getBoolParam("TraceShowLR");
    traceConfig.showMono = getBoolParam("analyzerShowMono");
    traceConfig.showL    = getBoolParam("analyzerShowL");
    traceConfig.showR    = getBoolParam("analyzerShowR");
    traceConfig.showMid  = getBoolParam("analyzerShowMid");
    traceConfig.showSide = getBoolParam("analyzerShowSide");
    traceConfig.showRMS  = getBoolParam("analyzerShowRMS");
    
    // Read Weighting (Choice 0=None, 1=A, 2=BS.468-4)
    auto* pWeight = apvts.getRawParameterValue("analyzerWeighting");
    traceConfig.weightingMode = (pWeight != nullptr) ? (int)pWeight->load() : 0;
    currentWeightingMode_ = traceConfig.weightingMode; // Store for updateFromSnapshot
    
    // Release Time (PeakDecay): single control for ballistics on all traces (RMS + L/R/Mid/Side/Mono)
    auto* pRelease = apvts.getRawParameterValue("PeakDecay");
    if (pRelease != nullptr)
        releaseMs_ = pRelease->load();
        
    // Read Smoothing (Fractional Octave)
    auto* pSmoothing = apvts.getRawParameterValue("Averaging");
    if (pSmoothing != nullptr)
    {
        constexpr float kSmoothingOctaves[] = { 0.0f, 1.0f/24.0f, 1.0f/12.0f, 1.0f/6.0f, 1.0f/3.0f, 1.0f };
        constexpr int kNumOpts = static_cast<int> (std::size (kSmoothingOctaves));
        const int index = juce::jlimit (0, kNumOpts - 1, static_cast<int> (pSmoothing->load()));
        
        if (index != lastSmoothingIdx_)
        {
            lastSmoothingIdx_ = index;
            smoothingOctaves_ = kSmoothingOctaves[index];
            ++smoothingGen_; // SMOOTHING_RENDERING_STABILITY_V2
            
            // Reset ballistics state to prevent glitches during smoothing transitions
            rmsState_.clear();
        }
    }

    // Push mdsp_gui spectrum settings: Smoothing, Tilt, Range, FFT order, Peak decay, DisplayGain
    {
        mdsp::gui::AnalyzerSettings specSettings;
        const float displayGainDb = [&apvts]()
        {
            auto* p = apvts.getRawParameterValue ("DisplayGain");
            return (p != nullptr) ? p->load() : 0.0f;
        }();
        specSettings.rangeMinDb = dbRangeToMinDb (dbRange_) + displayGainDb;
        specSettings.rangeMaxDb = 0.0f + displayGainDb;
        auto* pFftSize = apvts.getRawParameterValue ("FftSize");
        if (pFftSize != nullptr)
        {
            const int sizes[] = { 1024, 2048, 4096, 8192 };
            constexpr int kNumFftSizes = 4;
            const float raw = pFftSize->load();
            const int idx = juce::jlimit (0, kNumFftSizes - 1, juce::roundToInt (raw));
            const int fftSize = sizes[static_cast<size_t> (idx)];
            specSettings.fftOrder = static_cast<int> (std::log2 (fftSize));
        }
        auto* pSmooth = apvts.getRawParameterValue ("Averaging");
        if (pSmooth != nullptr)
        {
            constexpr float kSmoothingOctaves[] = { 0.0f, 1.0f/24.0f, 1.0f/12.0f, 1.0f/6.0f, 1.0f/3.0f, 1.0f };
            constexpr int kNumOpts = static_cast<int> (std::size (kSmoothingOctaves));
            const int idx = juce::jlimit (0, kNumOpts - 1, juce::roundToInt (pSmooth->load()));
            const float oct = kSmoothingOctaves[static_cast<size_t> (idx)];
            specSettings.smoothingAlpha = juce::jmap (oct, 0.0f, 1.0f, 0.0f, 0.9f);
        }
        auto* pDecay = apvts.getRawParameterValue ("PeakDecay");
        if (pDecay != nullptr)
        {
            const float ms = pDecay->load();
            specSettings.peakDecayRate = juce::jlimit (0.0f, 1.0f, (ms - 100.0f) / 4900.0f);
        }
        auto* pTilt = apvts.getRawParameterValue ("Tilt");
        if (pTilt != nullptr)
        {
            const int idx = juce::jlimit (0, 2, juce::roundToInt (pTilt->load()));
            if (idx == 1) specSettings.tiltDbPerOct = 4.5f;
            else if (idx == 2) specSettings.tiltDbPerOct = -4.5f;
            else specSettings.tiltDbPerOct = 0.0f;
        }
        // DISABLED: spectrumEngine.setSettings (specSettings);
    }
    
#if JUCE_DEBUG
    // One-shot debug logging
    static bool logged = false;
    if (!logged)
    {
        DBG("TraceConfig: L=" << (int)traceConfig.showL << " R=" << (int)traceConfig.showR 
            << " Mono=" << (int)traceConfig.showMono << " Mid=" << (int)traceConfig.showMid 
            << " Side=" << (int)traceConfig.showSide << " RMS=" << (int)traceConfig.showRMS);
        logged = true;
    }
#endif
    
    rtaDisplay.setTraceConfig(traceConfig);

    // Animate dB range changes (grid + FFT + peak mapping all derive from RTADisplay bottomDb).
    const float minDb = minDbAnim_.getNextValue();
    if (std::abs (minDb - lastAppliedMinDb_) > 1.0e-4f)
    {
        rtaDisplay.setDbRange (0.0f, minDb);
        lastAppliedMinDb_ = minDb;
    }

    const bool flashActive = (peakFlashActive_ && juce::Time::getMillisecondCounterHiRes() < peakFlashUntilMs_);
    if (peakFlashActive_ && !flashActive)
    {
        peakFlashActive_ = false;
        peakScaleDirty_ = true; // ensure we remap once without the flash boost
    }

    // If the FFT range is animating or Peak Range changed, remap peaks into the current FFT/grid space.
    if ((minDbAnim_.isSmoothing() || peakScaleDirty_ || flashActive) && hasLastValid_)
    {
        // REMOVED Independent Peak Scaling.
        // To ensure Peak Trace is always >= RMS Trace visually on the same grid,
        // we MUST render them using the SAME dB Scale (the Graph/FFT Scale).
        // Separate Peak Scaling caused visual alignment bugs.

        switch (currentMode_)
        {
            case Mode::FFT:
            {
                const bool hasPeaks = (!fftPeakDb_.empty() && fftPeakDb_.size() == fftDb_.size());
                if (hasPeaks)
                {
                    fftPeakDbDisplay_.resize (fftPeakDb_.size());
                    for (size_t i = 0; i < fftPeakDb_.size(); ++i)
                    {
                        float peakDb = fftPeakDb_[i];
                        if (isInvalidPeakDb (peakDb))
                            peakDb = fftDb_[i];
                        if (i < fftDb_.size())
                            peakDb = juce::jmax (peakDb, fftDb_[i]);
                        if (flashActive)
                            peakDb = juce::jmin (0.0f, peakDb + 2.0f);
                        fftPeakDbDisplay_[i] = peakDb;
                    }
                    rtaDisplay.setFFTData (fftDb_, 
                                           &fftPeakDbDisplay_,
                                           !peakHoldDbDisplay_.empty() ? &peakHoldDbDisplay_ : nullptr);
                }
                break;
            }
            case Mode::BAND:
            {
                const bool hasPeaks = (!bandsPeakDb_.empty() && bandsPeakDb_.size() == bandsDb_.size());
                if (hasPeaks)
                {
                    bandsPeakDbDisplay_.resize (bandsPeakDb_.size());
                    for (size_t i = 0; i < bandsPeakDb_.size(); ++i)
                    {
                        float peakDb = bandsPeakDb_[i];
                        if (isInvalidPeakDb (peakDb))
                            peakDb = bandsDb_[i];

                         // NO MAPPING
                        float result = peakDb;
                        if (flashActive)
                            result = juce::jmin (0.0f, result + 2.0f);
                        bandsPeakDbDisplay_[i] = result;
                    }
                    rtaDisplay.setBandData (bandsDb_, &bandsPeakDbDisplay_);
                }
                break;
            }
            case Mode::LOG:
            {
                const bool hasPeaks = (!logPeakDb_.empty() && logPeakDb_.size() == logDb_.size());
                if (hasPeaks)
                {
                    logPeakDbDisplay_.resize (logPeakDb_.size());
                    for (size_t i = 0; i < logPeakDb_.size(); ++i)
                    {
                        float peakDb = logPeakDb_[i];
                        if (isInvalidPeakDb (peakDb))
                            peakDb = logDb_[i];

                        // NO MAPPING
                        float result = peakDb;
                        if (flashActive)
                            result = juce::jmin (0.0f, result + 2.0f);
                        logPeakDbDisplay_[i] = result;
                    }
                    rtaDisplay.setLogData (logDb_, &logPeakDbDisplay_);
                }
                break;
            }
        }

        peakScaleDirty_ = false;
    }

    if (minDbAnim_.isSmoothing())
        repaint();

    // Apply any pending FFT resize on the message thread (RT-safe: allocations happen here, not on audio thread).
    audioProcessor.getAnalyzerEngine().applyPendingFftSizeIfNeeded();
    
    // Pull latest snapshot from analyzer engine (UI thread, reuse member snapshot)
    const bool gotSnapshot = audioProcessor.getAnalyzerEngine().getLatestSnapshot (snapshot_);
    
    if (!gotSnapshot)
    {
        // No new snapshot - hold last valid frame (do not touch RTADisplay)
        return;
    }
    
    // CRITICAL: Only update if snapshot is valid AND has bins (prevents blinking to floor)
    // Gate explicitly on isValid && fftBinCount > 0 to ensure smooth updates
    const int fftBinCount = (snapshot_.fftBinCount > 0) ? snapshot_.fftBinCount : snapshot_.numBins;
    if (!snapshot_.isValid || fftBinCount <= 0)
    {
        // Invalid snapshot - hold last valid frame (do not touch RTADisplay)
        return;
    }
    
    // Valid snapshot - store as last valid and update display
    // getLatestSnapshot() already handles torn reads with retry loop, so we update every valid snapshot
        lastValidSnapshot_ = snapshot_;
        hasLastValid_ = true;
            updateFromSnapshot (snapshot_);
}

void AnalyzerDisplayView::updateFromSnapshot (const AnalyzerSnapshot& snapshot)
{
    const int fftBinCount = (snapshot.fftBinCount > 0) ? snapshot.fftBinCount : snapshot.numBins;
    if (!snapshot.isValid || fftBinCount <= 0)
        return;
    
    // CRITICAL: Synchronize RTADisplay mode BEFORE any data feeding
    // UI mode is authoritative - ensure RTADisplay always matches
    const int rtaMode = toRtaMode (currentMode_);
    rtaDisplay.setViewMode (rtaMode);
#if JUCE_DEBUG
    lastSentRtaMode_ = rtaMode;
    assertModeSync();
#endif
    
    // ALWAYS call setFftMeta when snapshot has valid meta (required before first data frame)
    if (snapshot.fftSize > 0 && snapshot.sampleRate > 0.0)
    {
        rtaDisplay.setFftMeta (snapshot.sampleRate, snapshot.fftSize);
        lastMetaSampleRate_ = snapshot.sampleRate;
        lastMetaFftSize_ = snapshot.fftSize;
        expectedBins_ = snapshot.fftSize / 2 + 1;
        fftMetaReady_ = true;
        // DISABLED: spectrumEngine.prepare (snapshot.sampleRate);
    }
    
    // Update Hold Status
    isHoldOn_ = snapshot.isHoldOn;
    // rtaDisplay.setHoldStatus (isHoldOn_);
    
    // Route data STRICTLY by mode (FFT data only sent in FFT mode)
    // -------------------------------------------------------------------------
    // 1. DATA PREPARATION (Common for ALL modes)
    // -------------------------------------------------------------------------
    
    // Bin contract: snapshot.fftBinCount should equal (fftSize/2 + 1)
    const int validBins = fftBinCount;
    const int expectedBins = snapshot.fftSize / 2 + 1;
    
    if (validBins != expectedBins_ || validBins != expectedBins)
    {
        binMismatch_ = true;
#if JUCE_DEBUG
        dropReason_ = "DROP: bin mismatch (" + juce::String (validBins) + " != " + juce::String (expectedBins_) + " expected " + juce::String (expectedBins) + ")";
#endif
#if JUCE_DEBUG && ANALYZERPRO_FFT_DEBUG_LINE
        fftDebugLine_ = dropReason_;
#endif
        if (currentMode_ == Mode::FFT)
        {
             rtaDisplay.setNoData ("Bin Mismatch");
             rtaDisplay.repaint();
        }
        return; // Skip update on mismatch
    }
    
    binMismatch_ = false;
#if JUCE_DEBUG
    dropReason_.clear();
#endif
    
    smoother_.setEngineDidSpectralSmooth (snapshot.engineDidSpectralSmooth);
    smoother_.setUseUILogGaussianOnly (snapshot.useUILogGaussianOnly);
    
    // Resize member vectors to expected size
    const size_t validBinsSize = static_cast<size_t> (validBins);
    const size_t safeCopyBins = std::min (validBinsSize, static_cast<size_t> (AnalyzerSnapshot::kMaxFFTBins));
    fftDb_.resize (validBinsSize);
    fftPeakDb_.resize (validBinsSize);

    // Copy from snapshot arrays into member vectors (bounds-checked)
    jassert (snapshot.fftDb.size() >= safeCopyBins);
    std::copy (snapshot.fftDb.begin(),
               snapshot.fftDb.begin() + static_cast<std::ptrdiff_t> (safeCopyBins),
               fftDb_.begin());

    // Copy peak bins: validate size matches expected bins; if mismatch ignore peaks
    bool usePeaks = false;
    if (snapshot.fftPeakDb.size() >= safeCopyBins)
    {
        std::copy (snapshot.fftPeakDb.begin(),
                   snapshot.fftPeakDb.begin() + static_cast<std::ptrdiff_t> (safeCopyBins),
                   fftPeakDb_.begin());
        usePeaks = true;
    }
    else
    {
        // Peak mismatch: ignore peaks for this frame
        fftPeakDb_.clear();
        fftPeakDb_.resize (validBinsSize, -121.0f);  // Fill with floor if no peaks
    }

    // Centralized Latch: Apply True Freeze logic to fftPeakDb_ BEFORE mode conversion
    // This ensures BAND and LOG modes also inherit the frozen peak values.
    const bool holdOn = snapshot.isHoldOn;
    
    if (usePeaks)
    {
         if (uiHeldPeak_.size() != validBinsSize)
         {
             uiHeldPeak_.resize (validBinsSize);
             std::fill (uiHeldPeak_.begin(), uiHeldPeak_.end(), -120.0f);
             uiHoldActive_ = false; 
         }
         
         for (size_t i = 0; i < validBinsSize; ++i)
         {
             float incomingDb = sanitizeDb (fftPeakDb_[i]); // Sanitize first
             
             if (holdOn)
             {
                 float heldDb = uiHeldPeak_[i];
                 heldDb = juce::jmax (heldDb, incomingDb);
                 uiHeldPeak_[i] = heldDb;
                 incomingDb = heldDb;
             }
             else
             {
                 uiHeldPeak_[i] = incomingDb;
             }
             // Write back to source so all modes use the latched value
             fftPeakDb_[i] = incomingDb;
         }
    }
    
    // -------------------------------------------------------------------------
    // 1b. WEIGHTING + SMOOTHING + BALLISTICS PIPELINE
    // -------------------------------------------------------------------------

    // A. Rebuild weighting table (kept for overlay/visual indicator)
    const int weightingMode = currentWeightingMode_;
    const int currentFftSize = snapshot.fftSize;
    const double currentSampleRate = snapshot.sampleRate;

    rebuildWeightingTable (weightingMode, currentSampleRate, currentFftSize);

    // B. Weighting is now applied ENGINE-SIDE on the power spectrum BEFORE
    //    octave smoothing (AnalyzerEngine::computeFFT). The snapshot dB values
    //    already include weighting. No UI-side weighting addition needed.

    // Sanitize
    for (auto& v : fftDb_) v = sanitizeDb(v);
    for (auto& v : fftPeakDb_) v = sanitizeDb(v);
    
    // -------------------------------------------------------------------------
    // Session Marker Logic (Calculate on Peak Data)
    // -------------------------------------------------------------------------
    // const bool holdOn = snapshot.isHoldOn; // Already defined above
    
    // Detect new session (off -> on)
    if (holdOn && !lastHoldState_)
    {
        sessionMarkerValid_ = false;
        sessionMarkerDb_ = -1000.0f;
    }
    // Detect clear (on -> off)
    else if (!holdOn && lastHoldState_)
    {
        sessionMarkerValid_ = false; 
    }
    
    // Reset if meta changed
    if (snapshot.fftSize != lastFftSize_ || 
        std::abs(snapshot.sampleRate - lastMetaSampleRate_) > 1.0)
    {
        sessionMarkerValid_ = false;
        sessionMarkerDb_ = -1000.0f;
    }
    
    lastHoldState_ = holdOn;
    
    // Scan for new max if Hold is active
    if (holdOn && usePeaks && !fftPeakDb_.empty())
    {
        float currentMax = -1000.0f;
        int maxBin = -1;
        
        for (size_t i = 0; i < fftPeakDb_.size(); ++i)
        {
            if (fftPeakDb_[i] > currentMax)
            {
                currentMax = fftPeakDb_[i];
                maxBin = (int)i;
            }
        }
        
        // Update session max if we found a higher peak
        // Use epsilon to avoid noise updates
        if (currentMax > (sessionMarkerDb_ + 0.1f))
        {
            sessionMarkerDb_ = currentMax;
            sessionMarkerBin_ = maxBin;
            sessionMarkerValid_ = true;
        }
    }
    
    // C. Min/Max Stats (Post-Weighting)
    float minVal = std::numeric_limits<float>::max(); // Re-declare with type
    float maxVal = std::numeric_limits<float>::lowest();
    
    for (size_t i = 0; i < validBinsSize; ++i)
    {
        // Check bounds again just in case
        if (i < fftDb_.size())
        {
             minVal = juce::jmin (minVal, fftDb_[i]);
             maxVal = juce::jmax (maxVal, fftDb_[i]);
        }
    }
    
    // D. RMS Ballistics (Time Smoothing)
    // Applied to the now-weighted fftDb_
    applyBallistics (fftDb_.data(), rmsState_, validBinsSize, releaseMs_);
    
    // Multi-Trace Processing (moved here to share weighting table)
    // =========================================================================
    // MULTI-TRACE PROCESSING (Using Engine-Side RMS Data)
    // =========================================================================

    if (snapshot.multiTraceEnabled)
    {
        // Resize and initialize scratch buffers with floor value
        const size_t validBinsSz = static_cast<size_t> (validBins);
        constexpr float kDbFloor = -200.0f;

        if (scratchPowerL_.size() != validBinsSz) scratchPowerL_.resize (validBinsSz, kDbFloor);
        else std::fill (scratchPowerL_.begin(), scratchPowerL_.end(), kDbFloor);

        if (scratchPowerR_.size() != validBinsSz) scratchPowerR_.resize (validBinsSz, kDbFloor);
        else std::fill (scratchPowerR_.begin(), scratchPowerR_.end(), kDbFloor);

        if (scratchPowerMid_.size() != validBinsSz) scratchPowerMid_.resize (validBinsSz, kDbFloor);
        else std::fill (scratchPowerMid_.begin(), scratchPowerMid_.end(), kDbFloor);

        if (scratchPowerSide_.size() != validBinsSz) scratchPowerSide_.resize (validBinsSz, kDbFloor);
        else std::fill (scratchPowerSide_.begin(), scratchPowerSide_.end(), kDbFloor);

        if (scratchPowerMono_.size() != validBinsSz) scratchPowerMono_.resize (validBinsSz, kDbFloor);
        else std::fill (scratchPowerMono_.begin(), scratchPowerMono_.end(), kDbFloor);

        // Copy engine-processed dB arrays (already have audio-thread ballistics + smoothing)
        // Safe bounds check: ensure we don't read beyond snapshot arrays or write beyond scratch buffers
        const size_t maxSnapshotBins = AnalyzerSnapshot::kMaxFFTBins;
        const size_t safeBins = std::min (validBinsSz, std::min (maxSnapshotBins, snapshot.fftDbLRms.size()));
        jassert (scratchPowerL_.size() >= safeBins);
        jassert (snapshot.fftDbLRms.size() >= safeBins);

        std::copy (snapshot.fftDbLRms.begin(), snapshot.fftDbLRms.begin() + static_cast<std::ptrdiff_t> (safeBins), scratchPowerL_.begin());
        std::copy (snapshot.fftDbRRms.begin(), snapshot.fftDbRRms.begin() + static_cast<std::ptrdiff_t> (safeBins), scratchPowerR_.begin());
        std::copy (snapshot.fftDbMidRms.begin(), snapshot.fftDbMidRms.begin() + static_cast<std::ptrdiff_t> (safeBins), scratchPowerMid_.begin());
        std::copy (snapshot.fftDbSideRms.begin(), snapshot.fftDbSideRms.begin() + static_cast<std::ptrdiff_t> (safeBins), scratchPowerSide_.begin());
        std::copy (snapshot.fftDbMonoRms.begin(), snapshot.fftDbMonoRms.begin() + static_cast<std::ptrdiff_t> (safeBins), scratchPowerMono_.begin());

#if JUCE_DEBUG
        static int copyDebugCounter = 0;
        if (copyDebugCounter++ < 5)
        {
            DBG("After copy: L[0]=" << scratchPowerL_[0] << " R[0]=" << scratchPowerR_[0]
                << " Mid[0]=" << scratchPowerMid_[0] << " Side[0]=" << scratchPowerSide_[0]
                << " validBins=" << validBins);
        }
#endif

        // Weighting is now applied ENGINE-SIDE to L/R power before smoothing.
        // Multi-trace dB values from snapshot already include weighting.

        // Sanitize
        for (auto& v : scratchPowerL_) v = sanitizeDb (v);
        for (auto& v : scratchPowerR_) v = sanitizeDb (v);
        for (auto& v : scratchPowerMid_) v = sanitizeDb (v);
        for (auto& v : scratchPowerSide_) v = sanitizeDb (v);
        for (auto& v : scratchPowerMono_) v = sanitizeDb (v);

        // NOTE: UI-side ballistics REMOVED for multi-traces (Fix 3)
        // The engine (AnalyzerEngine::computeFFT) already applies full RMS ballistics
        // to smoothedLRms_, smoothedRRms_, etc. which are copied to snapshot.fftDbLRms, etc.
        // Applying ballistics again here caused "double smoothing" - sluggish response
        // and deviation from expected ballistics curve (attack/release ~2x longer than intended)
    }

    lastMinDb_ = minVal;
    lastMaxDb_ = maxVal;
    lastPeakDb_ = maxVal;
    lastBins_ = validBins;
    lastFftSize_ = snapshot.fftSize;

    // -------------------------------------------------------------------------
    // 2. MODE-SPECIFIC RENDERING
    // -------------------------------------------------------------------------
    switch (currentMode_)
    {
        case Mode::FFT:
        {
            // FFT mode: validate data and feed to RTADisplay
            
            // Remap peak trace into FFT/grid dB space using the separate Peak Range.
            if (usePeaks)
            {
                fftPeakDbDisplay_.resize (validBinsSize);
                const bool flash = (peakFlashActive_ && juce::Time::getMillisecondCounterHiRes() < peakFlashUntilMs_);

                for (size_t i = 0; i < validBinsSize; ++i)
                {
                    // Peak trace = main signal peak only (not multi-trace envelope)
                    float peakDb = fftPeakDb_[i];
                    if (i < fftDb_.size())
                        peakDb = juce::jmax (peakDb, fftDb_[i]);
                    if (flash)
                        peakDb = juce::jmin (0.0f, peakDb + 2.0f);
                    fftPeakDbDisplay_[i] = peakDb;
                }

                // Peak Hold logic moved outside to be independent
            }
            
            // Extract Peak Hold from snapshot (Independent of Peak Trace)
            bool usePeakHold = false;
            fftPeakHoldDb_.resize (validBinsSize);

            {
                const size_t safeHoldBins = std::min (validBinsSize,
                                                      std::min (static_cast<size_t> (AnalyzerSnapshot::kMaxFFTBins),
                                                                snapshot.fftPeakHoldDb.size()));
                jassert (fftPeakHoldDb_.size() >= safeHoldBins);
                if (safeHoldBins > 0)
                {
                    std::copy (snapshot.fftPeakHoldDb.begin(),
                               snapshot.fftPeakHoldDb.begin() + static_cast<std::ptrdiff_t> (safeHoldBins),
                               fftPeakHoldDb_.begin());
                    usePeakHold = true;
                }
                else
                {
                    std::fill (fftPeakHoldDb_.begin(), fftPeakHoldDb_.end(), -120.0f);
                }
            }

            // Peak dominance: clamp all traces to peak so nothing draws above the peak trace
            if (usePeaks && fftPeakDbDisplay_.size() == validBinsSize)
            {
                for (size_t i = 0; i < validBinsSize; ++i)
                {
                    const float peakDb = fftPeakDbDisplay_[i];
                    if (i < fftDb_.size())
                        fftDb_[i] = juce::jmin (fftDb_[i], peakDb);
                    if (i < scratchPowerL_.size())
                        scratchPowerL_[i] = juce::jmin (scratchPowerL_[i], peakDb);
                    if (i < scratchPowerR_.size())
                        scratchPowerR_[i] = juce::jmin (scratchPowerR_[i], peakDb);
                    if (i < scratchPowerMid_.size())
                        scratchPowerMid_[i] = juce::jmin (scratchPowerMid_[i], peakDb);
                    if (i < scratchPowerSide_.size())
                        scratchPowerSide_[i] = juce::jmin (scratchPowerSide_[i], peakDb);
                    if (i < scratchPowerMono_.size())
                        scratchPowerMono_[i] = juce::jmin (scratchPowerMono_[i], peakDb);
                }
            }

            // Feed RTADisplay with FFT data (ONLY in FFT mode)
            // Send data to Display (including session marker)
            rtaDisplay.setFFTData (fftDb_, 
                                   usePeaks ? &fftPeakDbDisplay_ : nullptr,
                                   usePeakHold ? &fftPeakHoldDb_ : nullptr);
            rtaDisplay.setSessionMarker (sessionMarkerValid_, sessionMarkerBin_, sessionMarkerDb_);
            
            // Multi-trace: Feed L/R/Mid/Side/Mono power data if available
            // Logic moved to Step 1b to unify weighting application and ballistics
            if (snapshot.multiTraceEnabled &&
                !scratchPowerL_.empty() && !scratchPowerR_.empty() &&
                scratchPowerL_.size() == static_cast<size_t>(validBins) &&
                scratchPowerR_.size() == static_cast<size_t>(validBins))
            {
#if JUCE_DEBUG
                 static int debugCounter = 0;
                 if (debugCounter++ < 5)
                 {
                     DBG("Multi-trace data: validBins=" << validBins
                         << " L[0]=" << scratchPowerL_[0] << " R[0]=" << scratchPowerR_[0]
                         << " Mid[0]=" << scratchPowerMid_[0] << " Side[0]=" << scratchPowerSide_[0]);
                 }
#endif
                 rtaDisplay.setMultiTraceData (scratchPowerL_.data(), scratchPowerR_.data(),
                                               scratchPowerMid_.data(), scratchPowerSide_.data(), scratchPowerMono_.data(),
                                               validBins);
            }
#if JUCE_DEBUG
            else if (snapshot.multiTraceEnabled)
            {
                static bool warnedOnce = false;
                if (!warnedOnce)
                {
                    DBG("Multi-trace DISABLED: enabled=" << (int)snapshot.multiTraceEnabled
                        << " L.size=" << scratchPowerL_.size() << " R.size=" << scratchPowerR_.size()
                        << " validBins=" << validBins);
                    warnedOnce = true;
                }
            }
#endif
            
            ++traceDataGen_; // SMOOTHING_RENDERING_STABILITY_V2: data changed
            rtaDisplay.setGenerations (traceDataGen_, smoothingGen_);
            
            // Force repaint after data update
            rtaDisplay.repaint();
            break;
        }
        
        case Mode::BAND:
        {
            // BANDS mode: Convert FFT bins to 1/3-octave bands
            if (fftBinCount <= 0 || snapshot.fftSize <= 0 || snapshot.sampleRate <= 0.0)
            {
                rtaDisplay.setNoData ("Invalid snapshot for BANDS");
                break;
            }
            
            // Initialize band centers if needed (band centers are independent of FFT size)
            if (bandCentersHz_.empty())
            {
                bandCentersHz_ = generateThirdOctaveBands();
            }
            
            // CRITICAL: Always set band centers before setting band data (ensures size matching)
            rtaDisplay.setBandCenters (bandCentersHz_);
            
            // Convert FFT bins to bands
            convertFFTToBands (snapshot, bandsDb_, bandsPeakDb_);
            
            // CRITICAL: Ensure sizes match exactly (bandCentersHz.size() == bandsDb.size() == bandsPeakDb.size())
            jassert (bandCentersHz_.size() == bandsDb_.size());
            jassert (bandsDb_.size() == bandsPeakDb_.size());
            
            // Feed RTADisplay with band data
            const bool useBandPeaks = !bandsPeakDb_.empty() && bandsPeakDb_.size() == bandsDb_.size() && bandsDb_.size() == bandCentersHz_.size();
            if (useBandPeaks)
            {
                // REMOVED Independent Peak Scaling
                // const float fftMinDb = lastAppliedMinDb_;
                // const float peakMinDb = dbRangeToMinDb (peakDbRange_);
                bandsPeakDbDisplay_.resize (bandsPeakDb_.size());
                
                const bool flash = (peakFlashActive_ && juce::Time::getMillisecondCounterHiRes() < peakFlashUntilMs_);

                for (size_t i = 0; i < bandsPeakDb_.size(); ++i)
                {
                     float peakDb = bandsPeakDb_[i]; // Already latched
                     
                     // NO MAPPING
                     // float mapped = mapDbToDisplayDb (peakDb, peakMinDb, fftMinDb);
                     
                     if (flash)
                        peakDb = juce::jmin (0.0f, peakDb + 2.0f);
                        
                     bandsPeakDbDisplay_[i] = peakDb;
                }
                rtaDisplay.setBandData (bandsDb_, &bandsPeakDbDisplay_);
            }
            else
            {
                rtaDisplay.setBandData (bandsDb_, nullptr);
            }
            
#if JUCE_DEBUG
            // Debug logging (once per second)
            bandsFedCount_++;
            const auto now = juce::Time::getCurrentTime();
            if (now.toMilliseconds() - lastDebugLogTime_.toMilliseconds() >= 1000)
            {
                float minDb = bandsDb_.empty() ? -120.0f : bandsDb_[0];
                float maxDb = bandsDb_.empty() ? -120.0f : bandsDb_[0];
                for (const float db : bandsDb_)
                {
                    minDb = juce::jmin (minDb, db);
                    maxDb = juce::jmax (maxDb, db);
                }
                DBG ("MODE=BANDS fedCount=" << bandsFedCount_ << " min=" << minDb << "dB max=" << maxDb << "dB");
                lastDebugLogTime_ = now;
            }
            jassert (toRtaMode (currentMode_) == 2);  // Assert we're calling the matching setter
#endif
            break;
        }
        
        case Mode::LOG:
        {
            // LOG mode: Convert FFT bins to log-spaced bins
            if (fftBinCount <= 0 || snapshot.fftSize <= 0 || snapshot.sampleRate <= 0.0)
            {
                rtaDisplay.setNoData ("Invalid snapshot for LOG");
                break;
            }
            
            // Convert FFT bins to log-spaced bins
            convertFFTToLog (snapshot, logDb_, logPeakDb_);
            
            // Feed RTADisplay with LOG data
            const bool useLogPeaks = !logPeakDb_.empty() && logPeakDb_.size() == logDb_.size();
            if (useLogPeaks)
            {
                // REMOVED Independent Peak Scaling
                // const float fftMinDb = lastAppliedMinDb_;
                // const float peakMinDb = dbRangeToMinDb (peakDbRange_);
                logPeakDbDisplay_.resize (logPeakDb_.size());
                
                const bool flash = (peakFlashActive_ && juce::Time::getMillisecondCounterHiRes() < peakFlashUntilMs_);

                for (size_t i = 0; i < logPeakDb_.size(); ++i)
                {
                     float peakDb = logPeakDb_[i]; // Already latched
                     
                     // NO MAPPING
                     // float mapped = mapDbToDisplayDb (peakDb, peakMinDb, fftMinDb);
                     
                     if (flash)
                         peakDb = juce::jmin (0.0f, peakDb + 2.0f);
                         
                     logPeakDbDisplay_[i] = peakDb;
                }
                rtaDisplay.setLogData (logDb_, &logPeakDbDisplay_);
            }
            else
            {
                rtaDisplay.setLogData (logDb_, nullptr);
            }
            
#if JUCE_DEBUG
            // Debug logging (once per second)
            logFedCount_++;
            const auto now = juce::Time::getCurrentTime();
            if (now.toMilliseconds() - lastDebugLogTime_.toMilliseconds() >= 1000)
            {
                float minDb = logDb_.empty() ? -120.0f : logDb_[0];
                float maxDb = logDb_.empty() ? -120.0f : logDb_[0];
                for (const float db : logDb_)
                {
                    minDb = juce::jmin (minDb, db);
                    maxDb = juce::jmax (maxDb, db);
                }
                DBG ("MODE=LOG fedCount=" << logFedCount_ << " min=" << minDb << "dB max=" << maxDb << "dB");
                lastDebugLogTime_ = now;
            }
            jassert (toRtaMode (currentMode_) == 1);  // Assert we're calling the matching setter
#endif
            break;
        }
        
        default:
        {
            jassertfalse;  // Unknown mode
            break;
        }
    }
}

//==============================================================================
void AnalyzerDisplayView::applyBallistics (float* data, std::vector<float>& state, size_t numBins, float releaseMs)
{
    if (state.size() != numBins)
    {
        state.resize (numBins, -200.0f);
        std::fill (state.begin(), state.end(), -200.0f);
    }

    // Assuming UI updates at 60Hz.
    // Using fixed dt ensures consistent ballistics regardless of FFT rate jitter.
    const float dt = 1.0f / 60.0f;

    const float attSec = kRmsAttackMs / 1000.0f;
    const float relSec = releaseMs / 1000.0f;

    // Coefficient = 1 - exp(-dt / tau)
    // Represents the fraction of the distance covered in one frame.
    const float attCoeff = 1.0f - std::exp (-dt / attSec);
    const float relCoeff = 1.0f - std::exp (-dt / relSec);

    for (size_t i = 0; i < numBins; ++i)
    {
        float in = data[i];

        // Sanitize input
        if (!std::isfinite(in)) in = -200.0f;

        float current = state[i];

        // Sanitize state
        if (!std::isfinite(current)) current = -200.0f;

        // Ballistics Logic (dB domain)
        if (in > current)
        {
            // Attack
            current += (in - current) * attCoeff;
        }
        else
        {
            // Release
            current += (in - current) * relCoeff;
        }

        // Clamp floor (relaxed to avoid flat segments in smoothed traces)
        if (current < -200.0f) current = -200.0f;

        state[i] = current;
        data[i] = current;
    }
}

//==============================================================================
// SmoothingProcessor Implementation
//==============================================================================
void AnalyzerDisplayView::SmoothingProcessor::setConfig (float octaves, int fftSize)
{
    // Recompute bounds only if config changed (octaves or fftSize)
    if (std::abs(smoothingOctaves_ - octaves) < 1e-4f && currentFFTSize_ == fftSize && !smoothLowBounds.empty())
        return;
        
    smoothingOctaves_ = octaves;
    currentFFTSize_ = fftSize;
    
    // Bounds calculation matches AnalyzerEngine::updateSmoothingBounds
    const int numBins = fftSize / 2 + 1;
    smoothLowBounds.resize (static_cast<size_t> (numBins));
    smoothHighBounds.resize (static_cast<size_t> (numBins));
    prefixSumMag.resize (static_cast<size_t> (numBins + 1));
    
    if (smoothingOctaves_ <= 0.0f)
        return;

    const double octaveFactor = std::pow (2.0, static_cast<double> (smoothingOctaves_) * 0.5);
    const double invOctaveFactor = 1.0 / octaveFactor;
    
    for (int i = 0; i < numBins; ++i)
    {
        if (i == 0)
        {
            smoothLowBounds[0] = 0;
            smoothHighBounds[0] = 0;
            continue;
        }
        
        int low = static_cast<int> (std::floor (static_cast<double> (i) * invOctaveFactor));
        int high = static_cast<int> (std::ceil (static_cast<double> (i) * octaveFactor));
        
        low = juce::jlimit (0, numBins - 1, low);
        high = juce::jlimit (0, numBins - 1, high);
        
        if (low > i) low = i;
        if (high < i) high = i;
        
        smoothLowBounds[static_cast<size_t> (i)] = low;
        smoothHighBounds[static_cast<size_t> (i)] = high;
    }
}

void AnalyzerDisplayView::SmoothingProcessor::process (const float* inputPower, float* outputPower, int numBins)
{
    if (engineDidSpectralSmooth_ || useUILogGaussianOnly_)
    {
        if (inputPower != outputPower)
            std::copy (inputPower, inputPower + numBins, outputPower);
        return;
    }
    if (smoothingOctaves_ <= 0.0f || numBins != (currentFFTSize_ / 2 + 1) || smoothLowBounds.empty())
    {
        if (inputPower != outputPower)
            std::copy (inputPower, inputPower + numBins, outputPower);
        return;
    }
    
    // 1. Prefix Sum
    if (static_cast<int> (prefixSumMag.size()) != numBins + 1)
        prefixSumMag.resize (static_cast<size_t> (numBins + 1));
        
    prefixSumMag[0] = 0.0f;
    for (int i = 0; i < numBins; ++i)
    {
        prefixSumMag[static_cast<size_t> (i + 1)] = prefixSumMag[static_cast<size_t> (i)] + inputPower[i];
    }
    
    // 2. Apply bounds
    for (int i = 0; i < numBins; ++i)
    {
        const int low = smoothLowBounds[static_cast<size_t> (i)];
        const int high = smoothHighBounds[static_cast<size_t> (i)];
        const int count = high - low + 1;
        
        if (count > 0)
        {
            const float sum = prefixSumMag[static_cast<size_t> (high + 1)] - prefixSumMag[static_cast<size_t> (low)];
            outputPower[i] = sum / static_cast<float> (count);
        }
        else
        {
            outputPower[i] = inputPower[i];
        }
    }
}

void AnalyzerDisplayView::LogGaussianSmoother::setConfig (float octaves)
{
    if (std::abs (smoothingOctaves_ - octaves) < 1e-6f && radius_ > 0)
        return;
    smoothingOctaves_ = octaves;
    
    if (octaves <= 0.0f)
    {
        radius_ = 0;
        return;
    }
    constexpr double kLogFreqMinHz = 20.0;
    constexpr double kLogFreqMaxHz = 20000.0;
    const double totalOctaves = std::log2 (kLogFreqMaxHz / kLogFreqMinHz);
    const double binsPerOctave = static_cast<double> (kMaxBins) / totalOctaves;
    const double sigmaBins = juce::jmax (0.5, binsPerOctave * static_cast<double> (octaves) / 2.355);
    const int radius = juce::jmin (static_cast<int> (std::floor (3.0 * sigmaBins + 0.5)),
                                   (static_cast<int> (weights_.size()) - 1) / 2);
    radius_ = juce::jmax (0, radius);
    const int half = radius_;
    const double sigma = static_cast<double> (sigmaBins);
    float sum = 0.0f;
    for (int d = -half; d <= half; ++d)
    {
        const float w = static_cast<float> (std::exp (-0.5 * static_cast<double> (d * d) / (sigma * sigma)));
        weights_[static_cast<size_t> (d + half)] = w;
        sum += w;
    }
    if (sum > 1e-10f)
        for (int i = 0; i <= 2 * half; ++i)
            weights_[static_cast<size_t> (i)] /= sum;
}

void AnalyzerDisplayView::LogGaussianSmoother::process (float* powerInOut, int numBins)
{
    if (radius_ <= 0 || numBins > kMaxBins)
        return;
    const int half = radius_;
    for (int i = 0; i < numBins; ++i)
        scratch_[static_cast<size_t> (i)] = powerInOut[i];
    for (int i = 0; i < numBins; ++i)
    {
        float sum = 0.0f;
        for (int d = -half; d <= half; ++d)
        {
            const int j = juce::jlimit (0, numBins - 1, i + d);
            sum += scratch_[static_cast<size_t> (j)] * weights_[static_cast<size_t> (d + half)];
        }
        powerInOut[i] = sum;
    }
}

void AnalyzerDisplayView::rebuildWeightingTable (int mode, double sampleRate, int fftSize)
{
    // Lazy check
    if (mode == lastWeightingMode_ && 
        std::abs(sampleRate - lastWeightingSampleRate_) < 0.1 && 
        fftSize == lastWeightingFftSize_)
    {
        return;
    }

    lastWeightingMode_ = mode;
    lastWeightingSampleRate_ = sampleRate;
    lastWeightingFftSize_ = fftSize;
    
    // Resize table
    // Table matches fftSize / 2 (or bin count)
    // Actually fftSize includes mirrors? No, bins is fftSize/2.
    // Wait, validBins derived from fftSize usually ~fftSize/2.
    // Let's use a safe upper bound or resize to fftSize/2 + 1
    const size_t numBins = static_cast<size_t> (fftSize / 2) + 1;
    if (cachedWeightingTable_.size() != numBins)
        cachedWeightingTable_.resize (numBins);
        
    if (mode == 0) // None
    {
        cachedWeightingTable_.clear(); // Empty means no weighting
        return;
    }
    
    const float binWidthHz = static_cast<float> (sampleRate) / static_cast<float> (fftSize);
    
    for (size_t i = 0; i < numBins; ++i)
    {
        float freq = static_cast<float>(i) * binWidthHz;
        
        // Avoid DC numerical issues (though definitions usually handle f=0)
        if (freq < 1.0f) freq = 1.0f; 
        
        float db = 0.0f;
        
        if (mode == 1) // A-Weighting
        {
            db = getAWeightingDb (freq);
        }
        else if (mode == 2) // BS.468-4
        {
            db = getBS468WeightingDb (freq);
        }
        
        cachedWeightingTable_[i] = db;
    }
}

float AnalyzerDisplayView::getAWeightingDb (float freqHz)
{
    // IEC 61672-1:2002 standard A-weighting
    // Ra(f) = (12194^2 * f^4) / ( (f^2 + 20.6^2) * sqrt((f^2 + 107.7^2)*(f^2 + 737.9^2)) * (f^2 + 12194^2) )
    // A(f) = 20*log10(Ra(f)) + 2.0 (approx to normalize 1kHz = 0dB)
    
    const float f2 = freqHz * freqHz;
    const float f4 = f2 * f2;
    
    const float c1 = 12194.0f * 12194.0f;
    const float c2 = 20.6f * 20.6f;
    const float c3 = 107.7f * 107.7f;
    const float c4 = 737.9f * 737.9f;
    const float c5 = 12194.0f * 12194.0f;
    
    const float num = c1 * f4;
    const float den = (f2 + c2) * std::sqrt((f2 + c3) * (f2 + c4)) * (f2 + c5);
    
    if (den == 0.0f) return -120.0f;
    
    float gain = num / den;
    return 20.0f * std::log10(gain) + 2.0f; 
}

float AnalyzerDisplayView::getBS468WeightingDb (float freqHz)
{
    // ITU-R 468-4 Weighting (Approximation)
    // Uses the formula from standard docs, freq in kHz
    
    const float f_kHz = freqHz / 1000.0f;
    const float f = f_kHz; 
    
    // Formula from ITU-R 468-4: 
    // H(s) pole-zero formulation
    // Coefficients
    const double a1 = 1.0458849;
    const double b2 = 1.6620626;
    const double c2 = 0.3181829;
    const double b3 = 0.5057538;
    const double c3 = 0.1691696;
    const double gainScale = 1.24633263;
    
    const double f2 = static_cast<double>(f * f);
    
    // Denominator parts squared magnitudes
    const double den1 = f2 + a1*a1;
    const double term2_real = c2 - f2;
    const double term2_imag = b2 * f;
    const double den2 = term2_real*term2_real + term2_imag*term2_imag;
    
    const double term3_real = c3 - f2;
    const double term3_imag = b3 * f;
    const double den3 = term3_real*term3_real + term3_imag*term3_imag;
    
    const double den = den1 * den2 * den3;
    
    if (den == 0.0) return -120.0f;
    
    // Numerator
    const double num = gainScale * f; // magnitude of j*f is f
    const double magSq = (num * num) / den;
    
    // Convert to dB
    // 10 * log10(magSq)
    // The gainScale is designed such that at 1kHz (f=1), gain is 0dB (unity).
    
    float db = static_cast<float>(10.0 * std::log10(magSq));
    
    // Offset correction if needed (usually BS.468 has 0dB gain at 1kHz defined by this formula)
    // But typical measurements add extra gain offset of +12.2dB relative to A-weighting noise floor?
    // No, weighting curve itself is relative gain vs frequency.
    // We stick to the standard formula which gives 0dB at 1kHz.
    
    return db;
}
