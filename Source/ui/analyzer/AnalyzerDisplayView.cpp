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

// Spectrum Y-axis headroom; RenderState topDb must never be 0.
static constexpr float kSpectrumTopDb = 6.0f;

static inline bool isInvalidPeakDb (float db) noexcept
{
    return db <= kUiPeakInvalidSentinelDb;
}

void AnalyzerDisplayView::applyLogSmoothingThunk (float* power, int bins, void* userData) noexcept
{
#if JUCE_DEBUG
    jassert (userData != nullptr);
#endif
    if (power == nullptr || bins <= 0 || userData == nullptr)
        return;

    auto* self = static_cast<AnalyzerDisplayView*> (userData);
    self->logGaussian_.process (power, bins);
}

AnalyzerDisplayView::AnalyzerDisplayView (mdsp_ui::UiContext& ui, AnalayzerProAudioProcessor& processor)
    : ui_ (ui),
      audioProcessor (processor),
      navOverlay_ (ui)
#if JUCE_DEBUG
    , lastDebugLogTime_ (juce::Time::getCurrentTime())
#endif
{
    theme_.seriesRms = juce::Colours::lightblue.withAlpha (theme_.seriesRms.getFloatAlpha());
    theme_.seriesPeak = juce::Colour (0xffffff33).withAlpha (theme_.seriesPeak.getFloatAlpha());
    theme_.seriesHold = theme_.seriesPeak;
    addAndMakeVisible (analyzerBridgeWidget_);
    // Analyzer display widget owns render/model/controller internals.
    analyzerBridgeWidget_.setGetTheme ([this]() -> const mdsp_ui::Theme& { return theme_; });

    // ── Nav overlay ──────────────────────────────────────────────────────
    // Icon painters: all lambdas receive (Graphics&, iconBounds). Colour is
    // set by FloatingIconPanel before calling — just draw the shape.

    // ◄  Pan left
    navOverlay_.addButton ("Pan left (lower frequencies)",
        [] (juce::Graphics& g, juce::Rectangle<float> r)
        {
            juce::Path p;
            p.addTriangle (r.getRight(), r.getY(),
                           r.getRight(), r.getBottom(),
                           r.getX(),     r.getCentreY());
            g.fillPath (p);
        },
        [this] { panFrequencyOctaves (-1.0f); });

    // ►  Pan right
    navOverlay_.addButton ("Pan right (higher frequencies)",
        [] (juce::Graphics& g, juce::Rectangle<float> r)
        {
            juce::Path p;
            p.addTriangle (r.getX(),     r.getY(),
                           r.getX(),     r.getBottom(),
                           r.getRight(), r.getCentreY());
            g.fillPath (p);
        },
        [this] { panFrequencyOctaves (1.0f); });

    // +  Zoom in
    navOverlay_.addButton ("Zoom in (narrow frequency range)",
        [] (juce::Graphics& g, juce::Rectangle<float> r)
        {
            const float t = 1.5f, cx = r.getCentreX(), cy = r.getCentreY();
            const float hw = r.getWidth()  * 0.42f;
            const float hh = r.getHeight() * 0.42f;
            g.fillRect (cx - hw, cy - t * 0.5f, hw * 2.0f, t);
            g.fillRect (cx - t * 0.5f, cy - hh, t, hh * 2.0f);
        },
        [this] { zoomFrequency (2.0f, std::sqrt (viewFreqMin_ * viewFreqMax_)); });

    // −  Zoom out
    navOverlay_.addButton ("Zoom out (wider frequency range)",
        [] (juce::Graphics& g, juce::Rectangle<float> r)
        {
            const float t = 1.5f, cx = r.getCentreX(), cy = r.getCentreY();
            const float hw = r.getWidth() * 0.42f;
            g.fillRect (cx - hw, cy - t * 0.5f, hw * 2.0f, t);
        },
        [this] { zoomFrequency (0.5f, std::sqrt (viewFreqMin_ * viewFreqMax_)); });

    // Separator before reset
    navOverlay_.setSeparatorsBefore ({ 4 });

    // ↺  Reset
    navOverlay_.addButton ("Reset to full range (20 Hz \xe2\x80\x93 20 kHz)",
        [] (juce::Graphics& g, juce::Rectangle<float> r)
        {
            const float cx = r.getCentreX(), cy = r.getCentreY();
            const float rad = r.getWidth() * 0.38f;
            const float startA = juce::MathConstants<float>::pi * 0.35f;
            const float endA   = juce::MathConstants<float>::pi * 2.25f;
            juce::Path arc;
            arc.addArc (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f, startA, endA, true);
            g.strokePath (arc, juce::PathStrokeType (1.5f));
            // Arrow head at end of arc
            const float ax = cx + rad * std::cos (endA);
            const float ay = cy + rad * std::sin (endA);
            const float tx = -std::sin (endA), ty = std::cos (endA);
            const float hs = 3.5f;
            juce::Path head;
            head.addTriangle (ax + tx * hs,  ay + ty * hs,
                              ax - tx * hs,  ay - ty * hs,
                              ax + std::cos (endA) * hs * 1.4f,
                              ay + std::sin (endA) * hs * 1.4f);
            g.fillPath (head);
        },
        [this] { resetFrequencyView(); });

    addAndMakeVisible (navOverlay_);

    // DISABLED: Use mdsp_gui default "Yellow Peak" aesthetic (sharp yellow stroke, gradient fill)

    // DISABLED: Initial FFT order: 4096 (order 12) for high-resolution spectrum
    // Initialize analyzer display ranges
    analyzerBridgeWidget_.setFrequencyRange (20.0f, 20000.0f);
    targetMinDb_ = dbRangeToMinDb (dbRange_);
    minDbAnim_.reset (60.0, 0.20);
    minDbAnim_.setCurrentAndTargetValue (targetMinDb_);
    lastAppliedMinDb_ = targetMinDb_;
    analyzerBridgeWidget_.setDbRange (kSpectrumTopDb, lastAppliedMinDb_);
    appliedDbRange_ = dbRange_;
    
    // Initialize band centers
    bandCentersHz_ = mdsp_ui::analyzer::makeStandardThirdOctaveCenters();
    renderStateProvider_.setBandCenters (bandCentersHz_);
    
    // Sync initial mode to analyzer display widget (currentMode_ defaults to FFT)
    analyzerBridgeWidget_.setViewMode (toRtaMode (currentMode_));

#if JUCE_DEBUG && ANALYZERPRO_MODE_DEBUG_OVERLAY
    addAndMakeVisible (modeOverlay_);
    modeOverlay_.setInterceptsMouseClicks (false, false);
    updateModeOverlayText();
#endif
    
    analyzerBridgeWidget_.setSnapshotSupplier ([this] (AnalyzerSnapshot& out)
    {
        return audioProcessor.getAnalyzerEngine().getLatestSnapshot (out);
    });
    analyzerBridgeWidget_.setSnapshotValidator ([] (const AnalyzerSnapshot& s)
    {
        const int fftBinCount = (s.fftBinCount > 0) ? s.fftBinCount : s.numBins;
        return s.isValid && fftBinCount > 0;
    });
    analyzerBridgeWidget_.setOnSnapshot ([this] (const AnalyzerSnapshot& s)
    {
        handlePumpedSnapshot (s);
    });
    analyzerBridgeWidget_.start (60);

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
    analyzerBridgeWidget_.setDbRange (kSpectrumTopDb, targetMinDb_);
    kickSnapshotPumpImmediate();
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
    analyzerBridgeWidget_.setPeakDbRange (dbRangeToMinDb (peakDbRange_));
    kickSnapshotPumpImmediate();
    repaint();
}

void AnalyzerDisplayView::resetSessionMarker()
{
    renderStateProvider_.resetSessionMarker();
    // Force immediate update to display
    analyzerBridgeWidget_.setSessionMarker (false, -1, -1000.0f);
}

void AnalyzerDisplayView::resetViewPeaks()
{
    // Clear bridge-owned hold-latch buffer
    analyzerBridgeWidget_.clearHoldLatch();

    // CRITICAL: Clear UI-side peak trace buffers that retain stale max values
    // These are copies from snapshots and won't reset automatically
    std::fill (fftPeakDb_.begin(), fftPeakDb_.end(), -120.0f);
    std::fill (fftDb_.begin(), fftDb_.end(), -120.0f);
    std::fill (fftPeakDbDisplay_.begin(), fftPeakDbDisplay_.end(), -120.0f);
    renderStateProvider_.resetPeakCache();

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

    analyzerBridgeWidget_.stop();
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

#endif

//==============================================================================
//==============================================================================
void AnalyzerDisplayView::paint (juce::Graphics& g)
{
    // Background is handled by analyzerBridgeWidget_ child component.
    juce::ignoreUnused (g);
}

void AnalyzerDisplayView::paintOverChildren (juce::Graphics& g)
{
    // Bypass Overlay
    if (audioProcessor.getBypassState())
    {
        const mdsp_ui::Theme theme (mdsp_ui::ThemeVariant::Custom);
        g.setColour (theme.background.withAlpha (0.6f));
        g.fillAll();
        
        g.setColour (theme.danger);
        g.setFont (juce::Font (juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 24.0f, juce::Font::bold)));
        g.drawText ("BYPASS", getLocalBounds(), juce::Justification::centred);
    }

    // Theme for debug overlays (matches plugin theme variant)
    const mdsp_ui::Theme theme (mdsp_ui::ThemeVariant::Custom);
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
    // Temporary debug overlay: UI/widget mode / bins / min/max dB
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
    dragStartPos_     = e.position;
    dragStartDbRange_ = dbRange_;
    dragStartFreqMin_ = viewFreqMin_;
    dragStartFreqMax_ = viewFreqMax_;
    dragAxisLocked_   = false;
    dragIsHorizontal_ = false;
}

void AnalyzerDisplayView::mouseDrag (const juce::MouseEvent& e)
{
    const float dx = e.position.x - dragStartPos_.x;
    const float dy = e.position.y - dragStartPos_.y;

    // Lock dominant axis after an 8 px threshold to avoid diagonal ambiguity
    if (! dragAxisLocked_)
    {
        if (std::abs (dx) >= 8.0f || std::abs (dy) >= 8.0f)
        {
            dragIsHorizontal_ = std::abs (dx) >= std::abs (dy);
            dragAxisLocked_   = true;
        }
        else
        {
            return;
        }
    }

    if (dragIsHorizontal_)
    {
        // Horizontal drag → pan frequency (grab paradigm: drag right = lower freqs)
        const float w = static_cast<float> (getWidth());
        if (w <= 0.0f) return;
        const float logSpan = std::log2 (dragStartFreqMax_) - std::log2 (dragStartFreqMin_);
        const float octaveShift = -(dx / w) * logSpan;
        const float newLogMin = std::log2 (dragStartFreqMin_) + octaveShift;
        const float newLogMax = std::log2 (dragStartFreqMax_) + octaveShift;
        setFrequencyView (std::pow (2.0f, newLogMin), std::pow (2.0f, newLogMax));
    }
    else
    {
        // Vertical drag → change dB range (60 px per discrete step)
        // Drag up (-Y) → more range (-120 dB).  Drag down (+Y) → less range (-60 dB).
        const int steps = static_cast<int> (dy / 60.0f);
        if (steps != 0)
        {
            const int targetIdx = juce::jlimit (0, 2, static_cast<int> (dragStartDbRange_) - steps);
            const DbRange nextRange = static_cast<DbRange> (targetIdx);
            if (nextRange != dbRange_)
            {
                setDbRange (nextRange);
                if (onDbRangeUserChanged)
                    onDbRangeUserChanged (nextRange);
            }
        }
    }
}

void AnalyzerDisplayView::mouseWheelMove (const juce::MouseEvent& e,
                                          const juce::MouseWheelDetails& wheel)
{
    const bool isCtrlOrCmd = e.mods.isCtrlDown() || e.mods.isCommandDown();
    const float absDx = std::abs (wheel.deltaX);
    const float absDy = std::abs (wheel.deltaY);

    if (absDx > absDy && ! isCtrlOrCmd)
    {
        // Horizontal scroll → pan frequency
        // deltaX positive = scroll right → higher frequencies
        const float octaveShift = wheel.deltaX * 0.8f;
        panFrequencyOctaves (octaveShift);
    }
    else if (isCtrlOrCmd && absDy > 0.001f)
    {
        // Ctrl/Cmd + vertical scroll → zoom frequency around cursor
        const float centerHz = pixelToFreq (e.position.x);
        const float factor = 1.0f + wheel.deltaY * 1.5f;
        if (factor > 0.1f)
            zoomFrequency (factor, centerHz);
    }
    else if (! isCtrlOrCmd && absDy > absDx && absDy > 0.001f)
    {
        // Plain vertical scroll → step dB range (up = more range, down = less)
        const int currentIdx = static_cast<int> (dbRange_);
        const int delta = wheel.deltaY > 0 ? 1 : -1;          // scroll up → index up → more range
        const int targetIdx = juce::jlimit (0, 2, currentIdx + delta);
        const DbRange nextRange = static_cast<DbRange> (targetIdx);
        if (nextRange != dbRange_)
        {
            setDbRange (nextRange);
            if (onDbRangeUserChanged)
                onDbRangeUserChanged (nextRange);
        }
    }
}

void AnalyzerDisplayView::mouseMagnify (const juce::MouseEvent& e, float scaleFactor)
{
    // Trackpad pinch / touchscreen pinch → zoom frequency around the gesture centre
    if (scaleFactor > 0.01f)
        zoomFrequency (scaleFactor, pixelToFreq (e.position.x));
}

//==============================================================================
// Frequency view helpers
//==============================================================================

float AnalyzerDisplayView::pixelToFreq (float xPx) const noexcept
{
    const float w = static_cast<float> (getWidth());
    if (w <= 0.0f) return viewFreqMin_;
    const float t = juce::jlimit (0.0f, 1.0f, xPx / w);
    return std::pow (2.0f, std::log2 (viewFreqMin_) + t * (std::log2 (viewFreqMax_) - std::log2 (viewFreqMin_)));
}

void AnalyzerDisplayView::setFrequencyView (float minHz, float maxHz)
{
    const float absLogMin = std::log2 (kAbsFreqMin);
    const float absLogMax = std::log2 (kAbsFreqMax);

    float logMin = std::log2 (juce::jmax (kAbsFreqMin, minHz));
    float logMax = std::log2 (juce::jmin (kAbsFreqMax, maxHz));

    // Enforce minimum span
    if (logMax - logMin < kMinFreqSpanOctaves)
    {
        const float centre = (logMin + logMax) * 0.5f;
        logMin = centre - kMinFreqSpanOctaves * 0.5f;
        logMax = centre + kMinFreqSpanOctaves * 0.5f;
    }

    // Clamp while preserving span
    if (logMin < absLogMin) { logMax += absLogMin - logMin; logMin = absLogMin; }
    if (logMax > absLogMax) { logMin -= logMax - absLogMax; logMax = absLogMax; }
    logMin = juce::jmax (logMin, absLogMin);
    logMax = juce::jmin (logMax, absLogMax);

    viewFreqMin_ = std::pow (2.0f, logMin);
    viewFreqMax_ = std::pow (2.0f, logMax);
    analyzerBridgeWidget_.setFrequencyRange (viewFreqMin_, viewFreqMax_);
}

void AnalyzerDisplayView::zoomFrequency (float factor, float centerHz)
{
    centerHz = juce::jlimit (kAbsFreqMin, kAbsFreqMax, centerHz);
    const float logMin    = std::log2 (viewFreqMin_);
    const float logMax    = std::log2 (viewFreqMax_);
    const float logCenter = std::log2 (centerHz);
    const float logSpan   = logMax - logMin;
    const float newSpan   = logSpan / juce::jmax (0.05f, factor);
    const float ratio     = (logCenter - logMin) / juce::jmax (1e-6f, logSpan);
    setFrequencyView (std::pow (2.0f, logCenter - ratio * newSpan),
                      std::pow (2.0f, logCenter + (1.0f - ratio) * newSpan));
}

void AnalyzerDisplayView::panFrequencyOctaves (float octaves)
{
    setFrequencyView (std::pow (2.0f, std::log2 (viewFreqMin_) + octaves),
                      std::pow (2.0f, std::log2 (viewFreqMax_) + octaves));
}

void AnalyzerDisplayView::resetFrequencyView()
{
    setFrequencyView (kAbsFreqMin, kAbsFreqMax);
}

void AnalyzerDisplayView::resized()
{
    auto bounds = getLocalBounds();
    analyzerBridgeWidget_.setBounds (bounds);
    analyzerBridgeWidget_.toFront (false);

    // Nav overlay: bottom-right corner, above everything
    const int margin = 8;
    const int ow = navOverlay_.preferredWidth();
    const int oh = navOverlay_.preferredHeight();
    navOverlay_.setBounds (bounds.getRight()  - ow - margin,
                           bounds.getBottom() - oh - margin,
                           ow, oh);
    navOverlay_.toFront (false);

#if JUCE_DEBUG && ANALYZERPRO_MODE_DEBUG_OVERLAY
    modeOverlay_.setBounds (8, 8, 260, 18);
    modeOverlay_.toFront (false);
#endif
}

int AnalyzerDisplayView::toRtaMode (Mode m) noexcept
{
    // Widget mode mapping: 0=FFT, 1=LOG, 2=BAND
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

#if JUCE_DEBUG && ANALYZERPRO_MODE_DEBUG_OVERLAY
void AnalyzerDisplayView::updateModeOverlayText()
{
    const juce::String dbg = juce::String ("UI=") + uiModeToString (currentMode_);
    modeOverlay_.setText (dbg);
}
#endif

void AnalyzerDisplayView::setMode (Mode mode)
{
    // UI selection is authoritative - update local mode
    currentMode_ = mode;
    
    // UI mode is authoritative; push directly into the bridge widget.
    const int rtaMode = toRtaMode (currentMode_);
    analyzerBridgeWidget_.setMode (static_cast<mdsp::gui::AnalyzerDisplayWidget::Mode> (rtaMode));
    analyzerBridgeWidget_.setViewMode (rtaMode);
    mdsp::gui::AnalyzerRenderStateProviderConfig providerCfg;
    providerCfg.mode = rtaMode;
    providerCfg.showLR = traceConfig_.showLR;
    providerCfg.showMono = traceConfig_.showMono;
    providerCfg.showL = traceConfig_.showL;
    providerCfg.showR = traceConfig_.showR;
    providerCfg.showMid = traceConfig_.showMid;
    providerCfg.showSide = traceConfig_.showSide;
    providerCfg.showRMS = traceConfig_.showRMS;
    providerCfg.weightingMode = traceConfig_.weightingMode;
    providerCfg.holdReleaseMs = traceConfig_.holdReleaseMs;
    renderStateProvider_.setConfig (providerCfg);

    // DISABLED: Sync shared spectrum engine analysis mode (Line / Log / Band)
    // mdsp::gui::SpectrumComponent::AnalysisMode specMode = mdsp::gui::SpectrumComponent::AnalysisMode::Log;
    // switch (currentMode_)
    // {
    //     case Mode::FFT:  specMode = mdsp::gui::SpectrumComponent::AnalysisMode::Line; break;
    //     case Mode::LOG:  specMode = mdsp::gui::SpectrumComponent::AnalysisMode::Log;  break;
    //     case Mode::BAND:  specMode = mdsp::gui::SpectrumComponent::AnalysisMode::Band; break;
    //     default:         specMode = mdsp::gui::SpectrumComponent::AnalysisMode::Log;  break;
    // }

#if JUCE_DEBUG && ANALYZERPRO_MODE_DEBUG_OVERLAY
    updateModeOverlayText();
#endif
    
    kickSnapshotPumpImmediate();
    // Force repaint
    repaint();
}

void AnalyzerDisplayView::setSpectrumFftOrder (int order)
{
    juce::ignoreUnused(order);
    analyzerBridgeWidget_.setSpectrumFftOrder (order);
}

void AnalyzerDisplayView::setSpectrumDecayRate (float decay)
{
    juce::ignoreUnused(decay);
    analyzerBridgeWidget_.setSpectrumDecayRate (decay);
}

void AnalyzerDisplayView::setDisplayGainDb (float db)
{
    analyzerBridgeWidget_.setDisplayGainDb (db);
}

void AnalyzerDisplayView::setTiltMode (TiltMode mode)
{
    analyzerBridgeWidget_.setTiltMode (static_cast<int> (mode));
}

void AnalyzerDisplayView::setTraceConfig (const mdsp::gui::AnalyzerDisplayWidget::TraceConfig& cfg)
{
    traceConfig_ = cfg;
    releaseMs_ = cfg.holdReleaseMs;
    currentWeightingMode_ = cfg.weightingMode;
    analyzerBridgeWidget_.setTraceConfig (traceConfig_);
    mdsp::gui::AnalyzerRenderStateProviderConfig providerCfg;
    providerCfg.mode = toRtaMode (currentMode_);
    providerCfg.showLR = traceConfig_.showLR;
    providerCfg.showMono = traceConfig_.showMono;
    providerCfg.showL = traceConfig_.showL;
    providerCfg.showR = traceConfig_.showR;
    providerCfg.showMid = traceConfig_.showMid;
    providerCfg.showSide = traceConfig_.showSide;
    providerCfg.showRMS = traceConfig_.showRMS;
    providerCfg.weightingMode = traceConfig_.weightingMode;
    providerCfg.holdReleaseMs = traceConfig_.holdReleaseMs;
    renderStateProvider_.setConfig (providerCfg);
    kickSnapshotPumpImmediate();
}

void AnalyzerDisplayView::kickSnapshotPumpImmediate()
{
    if (isShutdown)
        return;

    analyzerBridgeWidget_.requestEmitSoon();
    analyzerBridgeWidget_.emitNowIfDirty (true);
}

void AnalyzerDisplayView::timerCallback()
{
    // Early return if shutdown (do not rely on isTimerRunning())
    if (isShutdown)
        return;

    // Read analyzer APVTS params used by view-side processing.
    auto& apvts = audioProcessor.getAPVTS();

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

    // Animate dB range changes (grid + FFT + peak mapping all derive from widget bottomDb).
    const float minDb = minDbAnim_.getNextValue();
    if (std::abs (minDb - lastAppliedMinDb_) > 1.0e-4f)
    {
        analyzerBridgeWidget_.setDbRange (kSpectrumTopDb, minDb);
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
                    const auto& peakHoldDb = renderStateProvider_.peakHoldDb();
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
                    analyzerBridgeWidget_.setFFTData (fftDb_, 
                                           &fftPeakDbDisplay_,
                                           !peakHoldDb.empty() ? &peakHoldDb : nullptr);
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
                    analyzerBridgeWidget_.setBandData (bandsDb_, &bandsPeakDbDisplay_);
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
                    analyzerBridgeWidget_.setLogData (logDb_, &logPeakDbDisplay_);
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
}

void AnalyzerDisplayView::handlePumpedSnapshot (const AnalyzerSnapshot& snapshot)
{
    if (isShutdown)
        return;

    lastValidSnapshot_ = snapshot;
    hasLastValid_ = true;
    updateFromSnapshot (snapshot);
}

void AnalyzerDisplayView::updateFromSnapshot (const AnalyzerSnapshot& snapshot)
{
    const int fftBinCount = (snapshot.fftBinCount > 0) ? snapshot.fftBinCount : snapshot.numBins;
    if (!snapshot.isValid || fftBinCount <= 0)
        return;
    
    // Keep widget mode synchronized before any data feeding.
    const int rtaMode = toRtaMode (currentMode_);
    analyzerBridgeWidget_.setViewMode (rtaMode);
    
    // ALWAYS call setFftMeta when snapshot has valid meta (required before first data frame)
    if (snapshot.fftSize > 0 && snapshot.sampleRate > 0.0)
    {
        analyzerBridgeWidget_.setFftMeta (snapshot.sampleRate, snapshot.fftSize);
        lastMetaSampleRate_ = snapshot.sampleRate;
        lastMetaFftSize_ = snapshot.fftSize;
        expectedBins_ = snapshot.fftSize / 2 + 1;
        fftMetaReady_ = true;
    }
    
    // Update Hold Status
    isHoldOn_ = snapshot.isHoldOn;
    analyzerBridgeWidget_.setHoldStatus (isHoldOn_);
    
    // Route data STRICTLY by mode (FFT data only sent in FFT mode)
    // -------------------------------------------------------------------------
    // 1. DATA PREPARATION (Common for ALL modes)
    // -------------------------------------------------------------------------
    
    auto prep = analyzerBridgeWidget_.prepareFftTraces (snapshot, expectedBins_);

    if (! prep.valid)
    {
        binMismatch_ = prep.binMismatch;
#if JUCE_DEBUG
        if (prep.binMismatch)
            dropReason_ = "DROP: bin mismatch (" + juce::String (prep.validBins) + " != " + juce::String (expectedBins_) + " expected " + juce::String (prep.expectedBins) + ")";
        else
            dropReason_ = "DROP: " + prep.noDataReason;
#endif
#if JUCE_DEBUG && ANALYZERPRO_FFT_DEBUG_LINE
        fftDebugLine_ = dropReason_;
#endif
        if (currentMode_ == Mode::FFT)
        {
             analyzerBridgeWidget_.setNoData (prep.noDataReason.isNotEmpty() ? prep.noDataReason : juce::String ("No Data"));
             analyzerBridgeWidget_.repaint();
        }
        return; // Skip update on mismatch
    }

    const int validBins = prep.validBins;
    binMismatch_ = false;
#if JUCE_DEBUG
    dropReason_.clear();
#endif
    
    smoother_.setEngineDidSpectralSmooth (snapshot.engineDidSpectralSmooth);
    smoother_.setUseUILogGaussianOnly (snapshot.useUILogGaussianOnly);
    
    // Consume builder output (Step 3 extraction: snapshot->FFT trace prep in mdsp_gui)
    const size_t validBinsSize = prep.validBinsSize;
    fftDb_ = prep.fftDb;
    fftPeakDb_ = prep.fftPeakDb;
    const bool usePeaks = prep.usePeaks;

    // Centralized Latch: Apply True Freeze logic to fftPeakDb_ BEFORE mode conversion
    // This ensures BAND and LOG modes also inherit the frozen peak values.
    juce::ignoreUnused (validBinsSize);
    
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

    // Sanitization now handled by mdsp_gui::analyzer::AnalyzerRenderStateBuilder
    
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
            // FFT mode: validate data and feed to analyzer widget
            
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
            
            renderStateProvider_.updateFromSnapshot (snapshot.isValid,
                                                     fftBinCount,
                                                     snapshot.fftSize,
                                                     snapshot.sampleRate,
                                                     snapshot.isHoldOn,
                                                     snapshot.fftDb,
                                                     snapshot.fftPeakHoldDb,
                                                     fftPeakDb_,
                                                     usePeaks,
                                                     fftPeakDbDisplay_);
            const bool usePeakHold = renderStateProvider_.usePeakHold();
            const auto& fftPeakHoldDb = renderStateProvider_.peakHoldDb();

            // Keep RMS under peak, but leave multi-traces untouched to avoid clipped/squared tops.
            if (usePeaks && fftPeakDbDisplay_.size() == validBinsSize)
            {
                for (size_t i = 0; i < validBinsSize; ++i)
                {
                    const float peakDb = fftPeakDbDisplay_[i];
                    if (i < fftDb_.size())
                        fftDb_[i] = juce::jmin (fftDb_[i], peakDb);
                }
            }

            // Feed widget with FFT data (ONLY in FFT mode)
            // Send data to Display (including session marker)
            analyzerBridgeWidget_.setFFTData (fftDb_, 
                                   usePeaks ? &fftPeakDbDisplay_ : nullptr,
                                   usePeakHold ? &fftPeakHoldDb : nullptr);
            analyzerBridgeWidget_.setSessionMarker (renderStateProvider_.sessionMarkerVisible(),
                                                    renderStateProvider_.sessionMarkerBin(),
                                                    renderStateProvider_.sessionMarkerDb());
            
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
                 analyzerBridgeWidget_.setMultiTraceData (scratchPowerL_.data(), scratchPowerR_.data(),
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
            analyzerBridgeWidget_.setGenerations (traceDataGen_, smoothingGen_);
            
            // Force repaint after data update
            analyzerBridgeWidget_.repaint();
            break;
        }
        
        case Mode::BAND:
        {
            // BANDS mode: Convert FFT bins to 1/3-octave bands
            if (fftBinCount <= 0 || snapshot.fftSize <= 0 || snapshot.sampleRate <= 0.0)
            {
                analyzerBridgeWidget_.setNoData ("Invalid snapshot for BANDS");
                break;
            }
            
            renderStateProvider_.updateFromSnapshot (snapshot.isValid,
                                                     fftBinCount,
                                                     snapshot.fftSize,
                                                     snapshot.sampleRate,
                                                     snapshot.isHoldOn,
                                                     snapshot.fftDb,
                                                     snapshot.fftPeakHoldDb,
                                                     fftPeakDb_,
                                                     usePeaks);
            bandCentersHz_ = renderStateProvider_.bandCenters();
            bandsDb_ = renderStateProvider_.bandsDb();
            bandsPeakDb_ = renderStateProvider_.bandsPeakDb();
            analyzerBridgeWidget_.setBandCenters (bandCentersHz_);
            
            // Guard against transient provider/snapshot size skew in debug/runtime.
            // Recover by clamping all arrays to a common minimum size instead of asserting.
            const size_t bandCount = std::min (bandCentersHz_.size(), std::min (bandsDb_.size(), bandsPeakDb_.size()));
            if (bandCount == 0)
            {
#if JUCE_DEBUG
                static bool warnedEmptyBands = false;
                if (! warnedEmptyBands)
                {
                    DBG ("BANDS skipped: empty data (centers=" << bandCentersHz_.size()
                         << " db=" << bandsDb_.size()
                         << " peaks=" << bandsPeakDb_.size() << ")");
                    warnedEmptyBands = true;
                }
#endif
                analyzerBridgeWidget_.setNoData ("No BANDS data");
                break;
            }

            if (bandCentersHz_.size() != bandCount || bandsDb_.size() != bandCount || bandsPeakDb_.size() != bandCount)
            {
#if JUCE_DEBUG
                static bool warnedBandSizeMismatch = false;
                if (! warnedBandSizeMismatch)
                {
                    DBG ("BANDS size mismatch: centers=" << bandCentersHz_.size()
                         << " db=" << bandsDb_.size()
                         << " peaks=" << bandsPeakDb_.size()
                         << " -> clamping to " << bandCount);
                    warnedBandSizeMismatch = true;
                }
#endif
                bandCentersHz_.resize (bandCount);
                bandsDb_.resize (bandCount);
                bandsPeakDb_.resize (bandCount);
                analyzerBridgeWidget_.setBandCenters (bandCentersHz_);
            }
            
            // Feed widget with band data
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
                analyzerBridgeWidget_.setBandData (bandsDb_, &bandsPeakDbDisplay_);
            }
            else
            {
                analyzerBridgeWidget_.setBandData (bandsDb_, nullptr);
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
                analyzerBridgeWidget_.setNoData ("Invalid snapshot for LOG");
                break;
            }
            
            const float octaves = snapshot.smoothingOctaves;
            const bool applyGaussian = (!snapshot.engineDidSpectralSmooth && octaves > 0.0f);
            logGaussian_.setConfig (octaves);
            renderStateProvider_.updateFromSnapshot (snapshot.isValid,
                                                     fftBinCount,
                                                     snapshot.fftSize,
                                                     snapshot.sampleRate,
                                                     snapshot.isHoldOn,
                                                     snapshot.fftDb,
                                                     snapshot.fftPeakHoldDb,
                                                     fftPeakDb_,
                                                     usePeaks,
                                                     {},
                                                     applyGaussian ? &AnalyzerDisplayView::applyLogSmoothingThunk : nullptr,
                                                     applyGaussian ? this : nullptr);
            logDb_ = renderStateProvider_.logDb();
            logPeakDb_ = renderStateProvider_.logPeakDb();
            
            // Feed widget with LOG data
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
                analyzerBridgeWidget_.setLogData (logDb_, &logPeakDbDisplay_);
            }
            else
            {
                analyzerBridgeWidget_.setLogData (logDb_, nullptr);
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
