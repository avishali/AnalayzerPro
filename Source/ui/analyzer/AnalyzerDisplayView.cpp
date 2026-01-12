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

static float mapDbToDisplayDb (float db,
                               float srcMinDb,
                               float dstMinDb) noexcept
{
    // Map a dB value from [srcMinDb..0] into [dstMinDb..0] so that it renders at the same Y
    // when the plot range is dstMinDb..0.
    const float clamped = juce::jlimit (srcMinDb, 0.0f, db);
    const float y01 = clamped / srcMinDb;        // both negative => 0..1
    const float mapped = y01 * dstMinDb;         // dstMinDb negative
    return juce::jlimit (dstMinDb, 0.0f, mapped);
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
void AnalyzerDisplayView::paint (juce::Graphics& g)
{
    // Background is handled by RTADisplay
    juce::ignoreUnused (g);
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
            if (bin < static_cast<int> (snapshot.fftPeakDb.size()))
            {
                maxPeakDb = juce::jmax (maxPeakDb, snapshot.fftPeakDb[idx]);
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
        for (int bin = lowerBin; bin <= upperBin && bin < static_cast<int> (snapshot.fftPeakDb.size()); ++bin)
        {
            const std::size_t idx = static_cast<std::size_t> (bin);
            maxPeakDb = juce::jmax (maxPeakDb, snapshot.fftPeakDb[idx]);
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
        const float fftMinDb = lastAppliedMinDb_;
        const float peakMinDb = dbRangeToMinDb (peakDbRange_);

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
                        float mapped = mapDbToDisplayDb (peakDb, peakMinDb, fftMinDb);
                        if (flashActive)
                            mapped = juce::jmin (0.0f, mapped + 2.0f);
                        fftPeakDbDisplay_[i] = mapped;
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
                        float mapped = mapDbToDisplayDb (peakDb, peakMinDb, fftMinDb);
                        if (flashActive)
                            mapped = juce::jmin (0.0f, mapped + 2.0f);
                        bandsPeakDbDisplay_[i] = mapped;
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
                        float mapped = mapDbToDisplayDb (peakDb, peakMinDb, fftMinDb);
                        if (flashActive)
                            mapped = juce::jmin (0.0f, mapped + 2.0f);
                        logPeakDbDisplay_[i] = mapped;
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
    
    // Route data STRICTLY by mode (FFT data only sent in FFT mode)
    switch (currentMode_)
    {
        case Mode::FFT:
        {
            // FFT mode: validate data and feed to RTADisplay
            // Check if FFT vectors are empty (defensive)
            if (fftBinCount <= 0 || snapshot.fftSize <= 0)
            {
                rtaDisplay.setNoData ("FFT bins empty");
                rtaDisplay.repaint();
                break;
            }
            
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
                repaint();
                break;  // Do NOT call setFFTData() with mismatched bins
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
                fftPeakDb_.resize (validBinsSize, -120.0f);  // Fill with floor if no peaks
#if JUCE_DEBUG && ANALYZERPRO_FFT_DEBUG_LINE
                if (!fftDebugLine_.isEmpty())
                    fftDebugLine_ += " PEAK MISMATCH (ignored)";
#endif
            }
            
            // Sanitize dB values (snapshot.fftDb contains dB already; just clamp/sanitize)
            float minVal = std::numeric_limits<float>::max();
            float maxVal = std::numeric_limits<float>::lowest();
            
            for (size_t i = 0; i < validBinsSize; ++i)
            {
                // Sanitize dB values (snapshot contains dB already; no conversion needed)
                fftDb_[i] = sanitizeDb (fftDb_[i]);
                
                if (usePeaks)
                {
                    fftPeakDb_[i] = sanitizeDb (fftPeakDb_[i]);
                }
                
                // Track min/max for debug overlay
                minVal = juce::jmin (minVal, fftDb_[i]);
                maxVal = juce::jmax (maxVal, fftDb_[i]);
            }
            
            // Store debug values
            lastMinDb_ = minVal;
            lastMaxDb_ = maxVal;
            lastPeakDb_ = maxVal;
            lastBins_ = validBins;
            lastFftSize_ = snapshot.fftSize;
            
#if JUCE_DEBUG && ANALYZERPRO_FFT_DEBUG_LINE
            // DEBUG TEMP: remove once FFT validated
            // Update debug line: UI=<mode> bins=<count> min=<minDb> max=<maxDb> fftSize=<size>
            juce::String modeStr = "FFT";
            if (currentMode_ == Mode::BAND)
                modeStr = "BANDS";
            else if (currentMode_ == Mode::LOG)
                modeStr = "LOG";
            
            fftDebugLine_ = juce::String ("UI=") + modeStr
                          + juce::String (" bins=") + juce::String (validBins)
                          + juce::String (" min=") + juce::String (minVal, 1) + "dB"
                          + juce::String (" max=") + juce::String (maxVal, 1) + "dB"
                          + juce::String (" fftSize=") + juce::String (snapshot.fftSize);
#endif

#if defined(PLUGIN_DEV_MODE) && PLUGIN_DEV_MODE
            // Temporary debug overlay: UI=mode / RTADisplay=mode / bins / min/max dB
            juce::String uiModeStr = "FFT";
            if (currentMode_ == Mode::BAND)
                uiModeStr = "BANDS";
            else if (currentMode_ == Mode::LOG)
                uiModeStr = "LOG";
            
            juce::String rtaModeStr = "FFT";
#if JUCE_DEBUG
            if (lastSentRtaMode_ == 1)
                rtaModeStr = "LOG";
            else if (lastSentRtaMode_ == 2)
                rtaModeStr = "BANDS";
#endif
            
            devModeDebugLine_ = juce::String ("UI=") + uiModeStr
                              + juce::String (" / RTADisplay=") + rtaModeStr
                              + juce::String (" / bins=") + juce::String (validBins)
                              + juce::String (" / min=") + juce::String (minVal, 1) + "dB"
                              + juce::String (" max=") + juce::String (maxVal, 1) + "dB";
#endif
            
            // Remap peak trace into FFT/grid dB space using the separate Peak Range.
            if (usePeaks)
            {
                const float fftMinDb = lastAppliedMinDb_;
                const float peakMinDb = dbRangeToMinDb (peakDbRange_);
                fftPeakDbDisplay_.resize (validBinsSize);

                const bool flash = (peakFlashActive_ && juce::Time::getMillisecondCounterHiRes() < peakFlashUntilMs_);
                for (size_t i = 0; i < validBinsSize; ++i)
                {
                    float peakDb = fftPeakDb_[i];

                    // UI-only fix: if engine floor is hit, fall back to current trace to avoid a flat line.
                    if (isInvalidPeakDb (peakDb))
                        peakDb = fftDb_[i];

                    float mapped = mapDbToDisplayDb (peakDb, peakMinDb, fftMinDb);
                    if (flash)
                        mapped = juce::jmin (0.0f, mapped + 2.0f); // subtle visual flash
                    fftPeakDbDisplay_[i] = mapped;
                }
            }

            // Feed RTADisplay with FFT data (ONLY in FFT mode)
            rtaDisplay.setFFTData (fftDb_, usePeaks ? &fftPeakDbDisplay_ : nullptr);
            
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
            const bool usePeaks = !bandsPeakDb_.empty() && bandsPeakDb_.size() == bandsDb_.size() && bandsDb_.size() == bandCentersHz_.size();
            if (usePeaks)
            {
                const float fftMinDb = lastAppliedMinDb_;
                const float peakMinDb = dbRangeToMinDb (peakDbRange_);
                bandsPeakDbDisplay_.resize (bandsPeakDb_.size());
                const bool flash = (peakFlashActive_ && juce::Time::getMillisecondCounterHiRes() < peakFlashUntilMs_);
                for (size_t i = 0; i < bandsPeakDb_.size(); ++i)
                {
                    float peakDb = bandsPeakDb_[i];
                    if (isInvalidPeakDb (peakDb))
                        peakDb = bandsDb_[i];
                    float mapped = mapDbToDisplayDb (peakDb, peakMinDb, fftMinDb);
                    if (flash)
                        mapped = juce::jmin (0.0f, mapped + 2.0f);
                    bandsPeakDbDisplay_[i] = mapped;
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
            
            // Feed RTADisplay with log data
            const bool usePeaks = !logPeakDb_.empty() && logPeakDb_.size() == logDb_.size();
            if (usePeaks)
            {
                const float fftMinDb = lastAppliedMinDb_;
                const float peakMinDb = dbRangeToMinDb (peakDbRange_);
                logPeakDbDisplay_.resize (logPeakDb_.size());
                const bool flash = (peakFlashActive_ && juce::Time::getMillisecondCounterHiRes() < peakFlashUntilMs_);
                for (size_t i = 0; i < logPeakDb_.size(); ++i)
                {
                    float peakDb = logPeakDb_[i];
                    if (isInvalidPeakDb (peakDb))
                        peakDb = logDb_[i];
                    float mapped = mapDbToDisplayDb (peakDb, peakMinDb, fftMinDb);
                    if (flash)
                        mapped = juce::jmin (0.0f, mapped + 2.0f);
                    logPeakDbDisplay_[i] = mapped;
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
