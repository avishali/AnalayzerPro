#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_gui/analyzer/AnalyzerDisplayWidget.h>
#include <mdsp_ui/analyzer/AnalyzerRenderStateProvider.h>
#include <mdsp_ui/Theme.h>
#include <mdsp_ui/ThemeVariant.h>
#include <mdsp_ui/UiContext.h>
#include <mdsp_ui/controls/FloatingIconPanel.h>
#include <array>
#include <vector>
#include "../../PluginProcessor.h"
#include "../theme/TraceColors.h"
#if defined(ANALYZERPRO_METAL_EDITOR) && ANALYZERPRO_METAL_EDITOR
#include "metal/MetalHostShared.h"
#endif

#if !defined(ANALYZERPRO_MODE_DEBUG_OVERLAY)
#define ANALYZERPRO_MODE_DEBUG_OVERLAY 0
#endif

#if !defined(ANALYZERPRO_FFT_DEBUG_LINE)
#define ANALYZERPRO_FFT_DEBUG_LINE 1
#endif

#if !defined(PLUGIN_DEV_MODE)
#define PLUGIN_DEV_MODE 0
#endif

#if PLUGIN_DEV_MODE
#define ANALYZERPRO_DEV_DIAGNOSTICS 1
#else
#define ANALYZERPRO_DEV_DIAGNOSTICS 0
#endif

//==============================================================================
/**
    AnalyzerDisplayView hosts analyzer display widget with mode switching.
    Provides FFT / BAND / LOG mode selection.
*/
class AnalyzerDisplayView : public juce::Component,
                            private juce::Timer
{
public:
    enum class Mode
    {
        FFT,   // 0
        LOG,   // 1
        BAND   // 2
    };

    enum class DbRange
    {
        Minus60 = 0,
        Minus90 = 1,
        Minus120 = 2
    };

    AnalyzerDisplayView (mdsp_ui::UiContext& ui, AnalayzerProAudioProcessor& processor);
    ~AnalyzerDisplayView() override;

    /** User-customisable trace colours; applied to the render theme each frame. */
    void setTraceColorStore (AnalyzerPro::TraceColorStore* store) { traceColors_ = store; }

    void paint (juce::Graphics& g) override;
    void paintOverChildren (juce::Graphics& g) override;
    void resized() override;
    
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    void mouseMagnify (const juce::MouseEvent& event, float scaleFactor) override;

    // Frequency axis zoom / pan
    void setFrequencyView (float minHz, float maxHz);
    void zoomFrequency (float factor, float centerHz); // factor > 1 = zoom in
    void zoomFrequencyAroundViewCenter (float factor);
    void panFrequencyOctaves (float octaves);          // positive = shift to higher frequencies
    void resetFrequencyView();
    float pixelToFreq (float xPx) const noexcept;

    void setMode (Mode mode);
    Mode getMode() const noexcept { return currentMode_; }

    void setDbRange (DbRange r);
    DbRange getDbRange() const noexcept { return dbRange_; }
    void setDbRangeFromChoiceIndex (int idx);
    void setDbBottomContinuous (float bottomDb);
    void setDisplayDetailFromChoiceIndex (int idx);

    // Session Marker
    void resetSessionMarker();
    void resetViewPeaks();

    enum class TiltMode
    {
        Flat = 0,
        Pink = 1,
        White = 2
    };

    void setPeakDbRange (DbRange r);
    DbRange getPeakDbRange() const noexcept { return peakDbRange_; }
    void triggerPeakFlash();
    
    std::function<void(DbRange)> onDbRangeUserChanged;

    void setDisplayGainDb (float db);
    void setTiltMode (TiltMode mode);
    void setTraceConfig (const mdsp::gui::AnalyzerDisplayWidget::TraceConfig& cfg);

    /** Forward to shared spectrum engine: FFT order (e.g. 10=1024, 11=2048). */
    void setSpectrumFftOrder (int order);
    /** Forward to shared spectrum engine: decay 0.0 (instant) to 1.0 (max smooth). */
    void setSpectrumDecayRate (float decay);

    /** Shutdown: stop timer and clear references. Safe to call multiple times. */
    void shutdown();

#if defined(ANALYZERPRO_METAL_EDITOR) && ANALYZERPRO_METAL_EDITOR
    void setMetalTraceSuppressedForChromeCapture (bool shouldSuppress) noexcept;
    bool fillMetalAnalyzerFrame (AnalyzerPro::metal::MetalAnalyzerFrame& frame,
                                 const juce::Component& editor,
                                 float backingScale);
#endif

    /** Used by VBlank marshaler; avoids exposing internal shutdown flag to nested types. */
    bool isAnalyzerViewShutdown() const noexcept { return isShutdown; }

private:
    static void applyLogSmoothingThunk (float* power, int bins, void* userData) noexcept;
    void timerCallback() override;
    /** Message-thread spectrum UI pump (APVTS read, ballistics, pump FFT apply). */
    bool analyzerUiTickCore (double nowMs);
#if ANALYZERPRO_DEV_DIAGNOSTICS
    void accumulateDiagnosticsAndMaybeHud (bool tickFromVBlank);
#endif
    void updateFromSnapshot (const AnalyzerSnapshot& snapshot);
    void handlePumpedSnapshot (const AnalyzerSnapshot& snapshot);
    void kickSnapshotPumpImmediate();
    void requestAnalyzerRepaint();
    void syncRenderProviderConfig();
    float getEffectiveAbsFreqMax() const noexcept;
    void resetViewToDefault();
    void feedInterpolatedRenderFrame (double nowMs);
    bool advanceDbRangeAnimation (double nowMs);
    bool isRenderDispatchCapped (double nowMs) const noexcept;
    bool shouldSkipRenderFrame (double nowMs) const noexcept;

    // Map AnalyzerDisplayView::Mode to widget mode (0=FFT, 1=LOG, 2=BAND)
    static int toRtaMode (Mode m) noexcept;

    struct TraceFrameBuffers
    {
        std::vector<float> prev_;
        std::vector<float> curr_;
        std::vector<float> display_;
        double captureTimestampMs_ = 0.0;
        double lastChangeTimestampMs_ = 0.0;
        bool hasCurrent_ = false;
        bool hasPrevious_ = false;

        double currentTimestampMs() const noexcept { return captureTimestampMs_; }
        const std::vector<float>& display() const noexcept { return display_; }
    };

    static bool traceFrameIsMoving (const TraceFrameBuffers& buffers, double nowMs) noexcept;
    static bool latchTraceFrame (TraceFrameBuffers& buffers,
                                 const std::vector<float>& values,
                                 double captureTimestampMs);
    static void interpolateTraceFrame (TraceFrameBuffers& buffers, double nowMs);

#if JUCE_DEBUG && ANALYZERPRO_MODE_DEBUG_OVERLAY
    void updateModeOverlayText();
#endif

    mdsp_ui::UiContext& ui_;
    AnalayzerProAudioProcessor& audioProcessor;
    mdsp_ui::Theme theme_ { mdsp_ui::ThemeVariant::Custom };
    AnalyzerPro::TraceColorStore* traceColors_ = nullptr; // user trace colours (not owned)
    Mode currentMode_ = Mode::FFT;
    DbRange dbRange_ = DbRange::Minus90;
    DbRange appliedDbRange_ = DbRange::Minus90;
    DbRange peakDbRange_ = DbRange::Minus90;
    bool peakScaleDirty_ = false;

    juce::Point<float> dragStartPos_;
    float dragStartDbBottom_ = -90.0f;

    // Horizontal zoom / pan state
    float viewFreqMin_ = kDefaultFreqMin;
    float viewFreqMax_ = 20000.0f;
    float dragStartFreqMin_ = kDefaultFreqMin;
    float dragStartFreqMax_ = 20000.0f;
    bool dragAxisLocked_ = false;
    bool dragIsHorizontal_ = false;

    static constexpr float kAbsFreqMin = 10.0f;
    static constexpr float kDefaultFreqMin = 10.0f;
    static constexpr float kDefaultDbBottom = -90.0f;
    static constexpr float kMinDbBottom = -160.0f;
    static constexpr float kMaxDbBottom = -24.0f;
    static constexpr float kFallbackAbsFreqMax = 20000.0f;
    static constexpr float kHardFreqCeil = 96000.0f;
    static constexpr float kMinFreqSpanOctaves = 1.0f; // minimum 1-octave zoom window

    float targetMinDb_ = kDefaultDbBottom;
    float lastAppliedMinDb_ = kDefaultDbBottom;
    float minDbAnimStartDb_ = kDefaultDbBottom;
    double minDbAnimStartMs_ = 0.0;
    bool minDbAnimActive_ = false;

    std::vector<float> fftPeakDbDisplay_;
    std::vector<float> bandsPeakDbDisplay_;
    std::vector<float> logPeakDbDisplay_;

    bool peakFlashActive_ = false;
    double peakFlashUntilMs_ = 0.0;
    // uint32_t lastSequence_ = 0;  // Unused
    // CLEANUP: DUPLICATE - Removed duplicate commented variable (line 137)
    AnalyzerSnapshot lastValidSnapshot_;  // Hold last valid frame for grace period
    bool hasLastValid_ = false;
    bool isHoldOn_ = false;
    mdsp::gui::AnalyzerDisplayWidget analyzerBridgeWidget_;
    mdsp::gui::AnalyzerRenderStateProvider renderStateProvider_;
    std::vector<float> fftDb_;
    std::vector<float> fftPeakDb_;
    std::vector<float> bandsDb_;
    std::vector<float> bandsPeakDb_;
    std::vector<float> logDb_;
    std::vector<float> logPeakDb_;
    TraceFrameBuffers fftFrame_;
    TraceFrameBuffers bandFrame_;
    TraceFrameBuffers logFrame_;
    TraceFrameBuffers multiTraceLFrame_;
    TraceFrameBuffers multiTraceRFrame_;
    TraceFrameBuffers multiTraceMidFrame_;
    TraceFrameBuffers multiTraceSideFrame_;
    TraceFrameBuffers multiTraceMonoFrame_;
    bool latestMultiTraceEnabled_ = false;
    bool renderNoDataPending_ = false;
    juce::String renderNoDataReason_;
    uint64_t dataFrameSerial_ = 0;
    uint64_t renderedDataFrameSerial_ = 0;
    bool forceNextRenderFrame_ = true;
    std::vector<float> rmsState_;    // Ballistics state for Main RMS
    // NOTE: Multi-trace ballistics state removed (Fix 3)
    // Engine already applies full RMS ballistics to multi-traces in AnalyzerEngine::computeFFT

    // Scratch buffers for derived trace processing
    std::vector<float> scratchPowerMid_;
    std::vector<float> scratchPowerSide_;
    std::vector<float> scratchPowerMono_;

    float releaseMs_ = 300.0f; // Parameter cache
    mdsp::gui::AnalyzerDisplayWidget::TraceConfig traceConfig_;
    
    std::vector<float> bandCentersHz_;  // Cached 1/3-octave band centers
    float lastPeakDb_ = -1000.0f;
    float lastMinDb_ = 0.0f;
    float lastMaxDb_ = 0.0f;
    int lastBins_ = 0;
    int lastFftSize_ = 0;
    
    // Weighting support
    std::vector<float> cachedWeightingTable_;
    int lastWeightingMode_ = -1; // 0=None, 1=A, 2=BS.468
    int currentWeightingMode_ = 0; // Tracks parameter state
    TiltMode currentTiltMode_ = TiltMode::Flat;
    int lastWeightingFftSize_ = 0; // Check for rebuild
    double lastWeightingSampleRate_ = 0.0; 
    
    void rebuildWeightingTable (int mode, double sampleRate, int fftSize);
    static float getAWeightingDb (float freqHz);
    static float getBS468WeightingDb (float freqHz);
    
    // RMS Ballistics Tuning
    static constexpr float kRmsAttackMs = 60.0f;
    static constexpr float kRmsReleaseMs = 300.0f;
    
    // Helper to apply time-domain ballistics to a buffer
    // releaseMs allows parameter-driven release time (attack is fixed at 60ms for now)
    void applyBallistics (float* data, std::vector<float>& state, size_t numBins, float releaseMs);

    bool binMismatch_ = false;
    bool isShutdown = false;
    // Debug counters for BANDS/LOG mode
#if JUCE_DEBUG
    int bandsFedCount_ = 0;
    int logFedCount_ = 0;
    juce::Time lastDebugLogTime_;
#endif
    
    // Meta-before-data guard
    bool fftMetaReady_ = false;
    double lastMetaSampleRate_ = 0.0;
    int lastMetaFftSize_ = 0;
    int expectedBins_ = 0;
#if JUCE_DEBUG
    juce::String dropReason_;
#endif
#if JUCE_DEBUG && ANALYZERPRO_FFT_DEBUG_LINE
    juce::String fftDebugLine_;
#endif
#if ANALYZERPRO_DEV_DIAGNOSTICS
    juce::String devModeDebugLine_;  // Temporary: UI/widget mode / bins / min/max dB
#endif

    // Helper for fractional octave smoothing
    struct SmoothingProcessor
    {
        void setConfig (float octaves, int fftSize);
        void process (const float* inputPower, float* outputPower, int numBins);
        void setEngineDidSpectralSmooth (bool v) noexcept { engineDidSpectralSmooth_ = v; }
        void setUseUILogGaussianOnly (bool v) noexcept { useUILogGaussianOnly_ = v; }
        
        float smoothingOctaves_ = 0.0f;
        bool useUILogGaussianOnly_ = true;
        int currentFFTSize_ = 0;
        bool engineDidSpectralSmooth_ = false;
        
        std::vector<int> smoothLowBounds;
        std::vector<int> smoothHighBounds;
        std::vector<float> prefixSumMag;
    };
    SmoothingProcessor smoother_;

    // Log-domain Gaussian smoother for 256 log bins (Option A: avoids squared tops)
    struct LogGaussianSmoother
    {
        void setConfig (float octaves, int numBins, float minFreq, float maxFreq);
        void process (float* powerInOut, int numBins);
        
        float smoothingOctaves_ = 0.0f;
        int configuredBins_ = 0;
        float configuredMinFreq_ = 0.0f;
        float configuredMaxFreq_ = 0.0f;
        static constexpr int kMaxBins = 1024;
        std::array<float, kMaxBins * 2 + 1> weights_{};
        std::array<float, kMaxBins> scratch_{};
        int radius_ = 0;
    };
    LogGaussianSmoother logGaussian_;
    std::vector<float> scratchPowerL_;
    std::vector<float> scratchPowerR_;
    float smoothingOctaves_ = 1.0f / 6.0f; // Default 1/6 Oct
    int lastSmoothingIdx_ = -1; // Cache for param change detection
    int detailLogBins_ = 512;
    
    // Generation counters for render stability (SMOOTHING_RENDERING_STABILITY_V2)
    uint32_t traceDataGen_ = 0;   // Increments when trace buffer content changes
    uint32_t smoothingGen_ = 0;   // Increments when smoothing param changes
#if defined(ANALYZERPRO_METAL_EDITOR) && ANALYZERPRO_METAL_EDITOR
    uint64_t metalAnalyzerSequence_ = 1;
#endif

#if ANALYZERPRO_DEV_DIAGNOSTICS
    /** Visual diagnostics (message thread only; HUD refreshed from timerCallback or VBlank marshaler). */
    float uiDiagLastPaintMs_ = 0.0f;
    uint32_t uiDiagPaintEventsAccum_ = 0;
    uint32_t uiDiagSpectrumResizesAccum_ = 0;
    double uiDiagTimerPrevMs_ = 0.0;
    double uiDiagTimerJitterSumMs_ = 0.0;
    int uiDiagTimerJitterSamples_ = 0;
    double uiDiagTimerDtSumMs_ = 0.0;
    int uiDiagTimerTickCount_ = 0;
    uint32_t uiDiagTimerLateCountAccum_ = 0;
    double uiDiagHudWallMs_ = 0.0;
    uint32_t uiDiagRepaintRequestsAccum_ = 0;
    uint32_t uiDiagPumpThrottleAccum_ = 0;
    uint32_t uiDiagPumpRejectAccum_ = 0;
#endif

    struct VBlankRenderMarshaler final : public juce::AsyncUpdater
    {
        explicit VBlankRenderMarshaler (AnalyzerDisplayView& ownerIn) : owner (ownerIn) {}
        void handleAsyncUpdate() override;
        AnalyzerDisplayView& owner;
    };
    VBlankRenderMarshaler vBlankRenderMarshaler_;
    juce::VBlankAttachment vBlankAttachment_;
    double lastRenderDispatchMs_ = -1.0;
    bool vBlankAsyncPending_ = false;
    
#if JUCE_DEBUG && ANALYZERPRO_MODE_DEBUG_OVERLAY
    struct ModeDebugOverlay final : public juce::Component
    {
        void setText (juce::String t) { text = std::move (t); repaint(); }
        void paint (juce::Graphics& g) override
        {
            // Debug overlay uses theme-aware colors (variant-aware, defaults to Dark for Phase 1)
            const mdsp_ui::Theme theme (mdsp_ui::ThemeVariant::Custom);
            auto r = getLocalBounds().toFloat();
            g.setColour (theme.background.withAlpha (0.55f));
            g.fillRoundedRectangle (r, 4.0f);
            
            g.setColour (theme.warning);  // Yellow warning color from theme
            g.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
            g.drawText (text, getLocalBounds().reduced (6, 2), juce::Justification::centredLeft);
        }
        juce::String text;
    };
    ModeDebugOverlay modeOverlay_;
#endif
    


    mdsp_ui::FloatingIconPanel navOverlay_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalyzerDisplayView)
};
