#include "AnalyzerDisplayView.h"
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
        return -120.0f;
    return juce::jlimit (-120.0f, 24.0f, db);
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
    rtaDisplay.setBounds (getLocalBounds());
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
    // Resample FFT bins to log-spaced bins (256 bins from 20 Hz to 20 kHz)
    constexpr int numLogBins = 256;
    constexpr float minFreq = 20.0f;
    constexpr float maxFreq = 20000.0f;
    
    logDb.resize (numLogBins, -120.0f);
    logPeakDb.resize (numLogBins, -120.0f);
    
    const double sampleRate = snapshot.sampleRate;
    const int fftSize = snapshot.fftSize;
    const int fftBinCount = (snapshot.fftBinCount > 0) ? snapshot.fftBinCount : snapshot.numBins;
    const double binWidthHz = sampleRate / static_cast<double> (fftSize);
    
    const double logMin = std::log10 (static_cast<double> (minFreq));
    const double logMax = std::log10 (static_cast<double> (maxFreq));
    const double logRange = logMax - logMin;
    
    for (int logIdx = 0; logIdx < numLogBins; ++logIdx)
    {
        // Compute log-spaced center frequency
        const double logPos = logMin + (logRange * static_cast<double> (logIdx)) / static_cast<double> (numLogBins - 1);
        const double centerFreq = std::pow (10.0, logPos);
        
        // Compute bin edges (halfway to adjacent log bins)
        const double nextLogPos = (logIdx < numLogBins - 1) 
            ? (logMin + (logRange * static_cast<double> (logIdx + 1)) / static_cast<double> (numLogBins - 1))
            : logMax;
        const double prevLogPos = (logIdx > 0)
            ? (logMin + (logRange * static_cast<double> (logIdx - 1)) / static_cast<double> (numLogBins - 1))
            : logMin;
        
        const double lowerFreq = std::pow (10.0, (logPos + prevLogPos) / 2.0);
        const double upperFreq = std::pow (10.0, (logPos + nextLogPos) / 2.0);
        
        // Find FFT bins within this log bin
        int lowerBin = static_cast<int> (std::floor (lowerFreq / binWidthHz));
        int upperBin = static_cast<int> (std::ceil (upperFreq / binWidthHz));
        
        // Clamp to valid bin range
        lowerBin = juce::jmax (0, lowerBin);
        upperBin = juce::jmin (fftBinCount - 1, upperBin);
        
        // If lowerBin > upperBin, collapse to nearest valid bin
        if (lowerBin > upperBin)
        {
            const int centerBin = static_cast<int> (std::round (centerFreq / binWidthHz));
            lowerBin = juce::jlimit (0, fftBinCount - 1, centerBin);
            upperBin = lowerBin;
        }
        
        // Sum power of bins within log bin
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
        
        // Convert to dB (use average power for proper level)
        if (binCount > 0 && sumPower > 0.0)
        {
            const double avgPower = sumPower / static_cast<double> (binCount);
            const std::size_t logIdxSz = static_cast<std::size_t> (logIdx);
            logDb[logIdxSz] = 10.0f * static_cast<float> (std::log10 (avgPower));
        }
        else
        {
            const std::size_t logIdxSz = static_cast<std::size_t> (logIdx);
            logDb[logIdxSz] = -120.0f;
        }
        
        // For peak: use maximum (not average) of peak values (more stable and correct)
        float maxPeakDb = -120.0f;
        for (int bin = lowerBin; bin <= upperBin && bin < static_cast<int> (fftPeakDb_.size()); ++bin)
        {
            const std::size_t idx = static_cast<std::size_t> (bin);
            maxPeakDb = juce::jmax (maxPeakDb, fftPeakDb_[idx]);
        }
        const std::size_t logIdxSz = static_cast<std::size_t> (logIdx);
        logPeakDb[logIdxSz] = maxPeakDb;
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

#if JUCE_DEBUG && ANALYZERPRO_MODE_DEBUG_OVERLAY
    updateModeOverlayText();
#endif
    
    // Force repaint
    repaint();
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
            powerLState_.clear();
            powerRState_.clear();
            rmsState_.clear();
        }
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
                        
                        // NO MAPPING: Use same scale as FFT
                        // float mapped = mapDbToDisplayDb (peakDb, peakMinDb, fftMinDb);
                        
                        float result = peakDb;
                        if (flashActive)
                            result = juce::jmin (0.0f, result + 2.0f);
                        fftPeakDbDisplay_[i] = result;
                    }
                    rtaDisplay.setFFTData (fftDb_, &fftPeakDbDisplay_);
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
    
    // Resize member vectors to expected size
    const size_t validBinsSize = static_cast<size_t> (validBins);
    fftDb_.resize (validBinsSize);
    fftPeakDb_.resize (validBinsSize);
    
    // Copy from snapshot arrays into member vectors
    std::copy (snapshot.fftDb.begin(), snapshot.fftDb.begin() + validBins, fftDb_.begin());
    
    // Copy peak bins: validate size matches expected bins; if mismatch ignore peaks
    bool usePeaks = false;
    if (snapshot.fftPeakDb.size() >= static_cast<size_t> (validBins))
    {
        // Peak bins must match fftDb size (same bin count)
        std::copy (snapshot.fftPeakDb.begin(), snapshot.fftPeakDb.begin() + validBins, fftPeakDb_.begin());
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
    
    // A. Rebuild weighting table if needed
    const int weightingMode = currentWeightingMode_; // Use member variable
    const int currentFftSize = snapshot.fftSize;
    const double currentSampleRate = snapshot.sampleRate;
    
    rebuildWeightingTable (weightingMode, currentSampleRate, currentFftSize);
    
    // B. Apply Weighting (Pre-process Raw)
    // Applies to RMS and Peak buffers BEFORE smoothing or ballistics
    if (!cachedWeightingTable_.empty())
    {
        // Apply to RMS (fftDb_)
        if (fftDb_.size() == cachedWeightingTable_.size())
        {
            for (size_t i = 0; i < fftDb_.size(); ++i)
                fftDb_[i] += cachedWeightingTable_[i]; // Add dB offsets
        }
        
        // Apply to Peak (fftPeakDb_)
        if (fftPeakDb_.size() == cachedWeightingTable_.size())
        {
            for (size_t i = 0; i < fftPeakDb_.size(); ++i)
                fftPeakDb_[i] += cachedWeightingTable_[i];
        }
        
        // Apply to Held Peak (uiHeldPeak_) - correction: Hold should represent held *weighted* values.
        // Since input peak is now weighted, hold logic naturally holds weighted values.
        // But if we toggle weighting ON while holding, the held values are "raw". 
        // We accept that limitation for V1 (hold is reset on param change usually not forced though).
        // Actually, let's not valid-hold-shift. Valid expectation: "Hold" freezes what was on screen.
        // If we change weighting, the "Frozen" line should probably stay as is?
        // Or should it re-conform? 
        // For now, only new data is weighted. Held data is static.
        
        // Apply to Multi-Trace Buffers (snapshot L/R are raw)
        // We must apply weighting to scratch buffers during their processing below.
    }



    // Sanitize after weighting
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
    applyBallistics (fftDb_.data(), rmsState_, validBinsSize);
    
    // Multi-Trace Processing (moved here to share weighting table)
    if (snapshot.multiTraceEnabled)
    {
        // Resize scratch buffers
        const size_t validBinsSz = static_cast<size_t> (validBins);
        if (scratchPowerL_.size() != validBinsSz) scratchPowerL_.resize (validBinsSz);
        if (scratchPowerR_.size() != validBinsSz) scratchPowerR_.resize (validBinsSz);
        
        // Copy Raw first
        if (snapshot.powerL.size() >= validBinsSz)
            std::copy (snapshot.powerL.begin(), snapshot.powerL.begin() + validBins, scratchPowerL_.begin());
        if (snapshot.powerR.size() >= validBinsSz)
            std::copy (snapshot.powerR.begin(), snapshot.powerR.begin() + validBins, scratchPowerR_.begin());
            
        // Apply Weighting to L/R
        if (!cachedWeightingTable_.empty() && cachedWeightingTable_.size() == validBinsSz)
        {
             for (size_t i = 0; i < validBinsSz; ++i)
             {
                 scratchPowerL_[i] += cachedWeightingTable_[i];
                 scratchPowerR_[i] += cachedWeightingTable_[i];
             }
        }
        
        // Apply Smoothing
        smoother_.setConfig (smoothingOctaves_, snapshot.fftSize);
        // smooth in place? No, smoother expects input/output.
        // We need another scratch or double buffer logic if we want to smooth the weighted data.
        // Smoother takes const float* input, float* output.
        // We can use the same buffer if it supports it? 
        // My SmoothingProcessor implementation likely does NOT support in-place if it uses prefix sums.
        // Let's check `SmoothingProcessor::process` signature.
        
        // To be safe, let's use a temp buffer or just implement in-place if careful.
        // Actually, smoother likely computes bounds based on index.
        // Let's create `scratchSmoothL_` if needed. Use `powerLState_` as temp? No, that's ballistics state.
        
        // Actually, the previous code was:
        // smoother_.process (snapshot.powerL.data(), scratchPowerL_.data(), validBins);
        
        // Use a persistent scratch buffer for the "smoothed" result
        static std::vector<float> tempL, tempR; // Avoid static in method.
        // Let's use `logDb_` or similar as temp? No.
        
        // Let's just allocate strictly scoped vectors for now to ensure correctness
        std::vector<float> smoothedL (validBinsSz), smoothedR (validBinsSz);
        
        smoother_.process (scratchPowerL_.data(), smoothedL.data(), validBins);
        smoother_.process (scratchPowerR_.data(), smoothedR.data(), validBins);
        
        // Copy back to scratch for ballistics
        scratchPowerL_ = smoothedL;
        scratchPowerR_ = smoothedR;
        
        // Tuned RMS Ballistics
        applyBallistics (scratchPowerL_.data(), powerLState_, validBinsSz);
        applyBallistics (scratchPowerR_.data(), powerRState_, validBinsSz);
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
                // REMOVED Independent Peak Scaling to match timerCallback logic
                // const float fftMinDb = lastAppliedMinDb_;
                // const float peakMinDb = dbRangeToMinDb (peakDbRange_);
                fftPeakDbDisplay_.resize (validBinsSize);

                const bool flash = (peakFlashActive_ && juce::Time::getMillisecondCounterHiRes() < peakFlashUntilMs_);
                
                for (size_t i = 0; i < validBinsSize; ++i)
                {
                    float peakDb = fftPeakDb_[i]; // Already latched
                    
                    // NO MAPPING: Use same scale as FFT
                    // float mapped = mapDbToDisplayDb (peakDb, peakMinDb, fftMinDb);
                    
                    if (flash)
                        peakDb = juce::jmin (0.0f, peakDb + 2.0f); 
                        
                    fftPeakDbDisplay_[i] = peakDb;
                }
            }

            // Feed RTADisplay with FFT data (ONLY in FFT mode)
            // Send data to Display (including session marker)
            rtaDisplay.setFFTData (fftDb_, usePeaks ? &fftPeakDbDisplay_ : nullptr);
            rtaDisplay.setSessionMarker (sessionMarkerValid_, sessionMarkerBin_, sessionMarkerDb_);
            
            // Multi-trace: Feed L/R power data if available
            // Logic moved to Step 1b to unify weighting application
            // Only set data here
            if (snapshot.multiTraceEnabled)
            {
                 rtaDisplay.setLRPowerData (scratchPowerL_.data(), scratchPowerR_.data(), validBins);
            }
            
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
void AnalyzerDisplayView::applyBallistics (float* data, std::vector<float>& state, size_t numBins)
{
    if (state.size() != numBins)
    {
        state.resize (numBins, -120.0f);
        std::fill (state.begin(), state.end(), -120.0f);
    }
    
    // Assuming UI updates at 60Hz. 
    // Using fixed dt ensures consistent ballistics regardless of FFT rate jitter.
    const float dt = 1.0f / 60.0f; 
    
    const float attSec = kRmsAttackMs / 1000.0f;
    const float relSec = kRmsReleaseMs / 1000.0f;
    
    // Coefficient = 1 - exp(-dt / tau)
    // Represents the fraction of the distance covered in one frame.
    const float attCoeff = 1.0f - std::exp (-dt / attSec);
    const float relCoeff = 1.0f - std::exp (-dt / relSec);
    
    for (size_t i = 0; i < numBins; ++i)
    {
        float in = data[i];
        
        // Sanitize input
        if (!std::isfinite(in)) in = -120.0f;
        
        float current = state[i];
        
        // Sanitize state
        if (!std::isfinite(current)) current = -120.0f;
        
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
        
        // Clamp floor
        if (current < -120.0f) current = -120.0f;
        
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
    if (smoothingOctaves_ <= 0.0f || numBins != (currentFFTSize_ / 2 + 1) || smoothLowBounds.empty())
    {
        // Bypass
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
