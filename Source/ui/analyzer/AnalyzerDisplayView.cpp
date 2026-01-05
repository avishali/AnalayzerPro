#include "AnalyzerDisplayView.h"
#include <cmath>
#include <limits>

#if !defined(ANALYZERPRO_MODE_DEBUG_OVERLAY)
#define ANALYZERPRO_MODE_DEBUG_OVERLAY 1
#endif

#if !defined(ANALYZERPRO_FFT_DEBUG_LINE)
#define ANALYZERPRO_FFT_DEBUG_LINE 1
#endif

//==============================================================================
AnalyzerDisplayView::AnalyzerDisplayView (AnalayzerProAudioProcessor& processor)
    : audioProcessor (processor)
{
    addAndMakeVisible (rtaDisplay);
    // Initialize RTADisplay with default ranges
    rtaDisplay.setFrequencyRange (20.0f, 20000.0f);
    rtaDisplay.setDbRange (0.0f, -90.0f);
    
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
    
    // Start timer for snapshot updates (~50 Hz)
    startTimer (20);  // 20ms = 50 Hz
    
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
#if JUCE_DEBUG
    // Debug overlay: mode, sequence, bins, fftSize, meta, drop reason (top-left)
    g.setFont (juce::Font (juce::FontOptions().withHeight (10.0f)));
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    
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
        g.setColour (juce::Colours::red.withAlpha (0.8f));
    }
    
    g.drawText (debugText, 8, 8, 500, 12, juce::Justification::centredLeft);
    
#if ANALYZERPRO_FFT_DEBUG_LINE
    // DEBUG TEMP: remove once FFT validated
    // FFT debug line: mode + bins + min/max dB + fftSize
    if (!fftDebugLine_.isEmpty())
    {
        g.setColour (juce::Colours::cyan.withAlpha (0.8f));
        g.setFont (juce::Font (juce::FontOptions().withHeight (10.0f)));
        g.drawText (fftDebugLine_, 8, 22, 600, 12, juce::Justification::centredLeft);
    }
#endif
#endif

#if defined(PLUGIN_DEV_MODE) && PLUGIN_DEV_MODE
    // Temporary debug overlay: UI=mode / RTADisplay=mode / bins / min/max dB
    if (!devModeDebugLine_.isEmpty())
    {
        g.setColour (juce::Colours::yellow.withAlpha (0.9f));
        g.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)));
        g.drawText (devModeDebugLine_, 8, 38, 700, 14, juce::Justification::centredLeft);
    }
    
    // TEMP ROUTING PROBE: remove after FFT confirmed.
    // Input signal probe: channel count, RMS, and peak levels
    // Interpretation: If RMS/PEAK stay near -160 dB while playing -> host not sending audio (routing/bypass issue)
    //                 If RMS/PEAK move but FFT doesn't -> analyzer feed wrong (feeding zeros/muted buffer)
    const float inputRms = audioProcessor.getUiInputRmsDb();
    const float inputPeak = audioProcessor.getUiInputPeakDb();
    const int inputCh = audioProcessor.getUiNumCh();
    const juce::String inputProbeText = juce::String ("IN ch=") + juce::String (inputCh)
                                      + juce::String (" RMS=") + juce::String (inputRms, 1) + "dB"
                                      + juce::String (" PEAK=") + juce::String (inputPeak, 1) + "dB";
    g.setColour (juce::Colours::lime.withAlpha (0.9f));
    g.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)));
    g.drawText (inputProbeText, 8, 56, 500, 14, juce::Justification::centredLeft);
#endif
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
    // Early return if mode hasn't changed
    if (currentMode_ == mode)
    {
        // Even if mode hasn't changed, ensure RTADisplay is synchronized
        // This handles cases where RTADisplay might have been reset or desynced
        const int rtaMode = toRtaMode (currentMode_);
        rtaDisplay.setViewMode (rtaMode);
#if JUCE_DEBUG
        lastSentRtaMode_ = rtaMode;
#endif
        return;
    }
    
    currentMode_ = mode;
    
    // Forward to RTADisplay using mapping helper
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
    
    // Pull latest snapshot from analyzer engine (UI thread, reuse member snapshot)
    // Get latest snapshot (two-read stability check handles sequence internally)
    if (audioProcessor.getAnalyzerEngine().getLatestSnapshot (snapshot_))
    {
        // Check if we have new data by comparing snapshot metadata
        // (getLatestSnapshot already ensures stable read, so we can update)
        updateFromSnapshot (snapshot_);
    }
}

void AnalyzerDisplayView::updateFromSnapshot (const AnalyzerSnapshot& snapshot)
{
    if (!snapshot.isValid || snapshot.numBins <= 0)
        return;
    
    // CRITICAL: Synchronize RTADisplay mode BEFORE any data feeding
    // This ensures RTADisplay is in the correct mode for the data we're about to send
    // ALWAYS call setViewMode (even if mode hasn't changed) to ensure mode is synchronized
    const int rtaMode = toRtaMode (currentMode_);
    rtaDisplay.setViewMode (rtaMode);
#if JUCE_DEBUG
    lastSentRtaMode_ = rtaMode;
    assertModeSync();
    // Force update RTADisplay debug info to match actual mode
    // Note: RTADisplay's debug overlay uses debugViewMode, but rendering uses state.viewMode
    // The debug overlay may be stale, but rendering should work correctly
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
            if (snapshot.numBins <= 0 || snapshot.fftSize <= 0)
            {
                rtaDisplay.setNoData ("FFT bins empty");
                rtaDisplay.repaint();
                break;
            }
            
            // Bin contract: snapshot.numBins should equal (fftSize/2 + 1)
            const int validBins = snapshot.numBins;
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
            
            // Feed RTADisplay with FFT data (ONLY in FFT mode)
            rtaDisplay.setFFTData (fftDb_, usePeaks ? &fftPeakDb_ : nullptr);
            
            // Force repaint after data update
            rtaDisplay.repaint();
            break;
        }
        
        case Mode::BAND:
        {
            // BANDS mode: do NOT feed FFT data
            // Snapshot only has FFT data, bands not wired yet
#if JUCE_DEBUG
            dropReason_ = "NO DATA: BANDS not wired";
#endif
            rtaDisplay.setNoData ("BANDS not wired");
            rtaDisplay.repaint();
            break;
        }
        
        case Mode::LOG:
        {
            // LOG mode: do NOT feed FFT data
            // Snapshot only has FFT data, log not wired yet
#if JUCE_DEBUG
            dropReason_ = "NO DATA: LOG not wired";
#endif
            rtaDisplay.setNoData ("LOG not wired");
            rtaDisplay.repaint();
            break;
        }
        
        default:
        {
            jassertfalse;  // Unknown mode
            break;
        }
    }
}
