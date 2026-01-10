#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/Theme.h>
#include <mdsp_ui/ThemeVariant.h>
#include <vector>
#include <algorithm>
#include "rta1_import/RTADisplay.h"
#include "../../analyzer/AnalyzerSnapshot.h"
#include "../../PluginProcessor.h"

#if !defined(ANALYZERPRO_MODE_DEBUG_OVERLAY)
#define ANALYZERPRO_MODE_DEBUG_OVERLAY 0
#endif

#if !defined(ANALYZERPRO_FFT_DEBUG_LINE)
#define ANALYZERPRO_FFT_DEBUG_LINE 1
#endif

#if !defined(PLUGIN_DEV_MODE)
#define PLUGIN_DEV_MODE 1  // Temporary debug overlay
#endif

//==============================================================================
/**
    AnalyzerDisplayView wraps RTADisplay with mode switching.
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

    AnalyzerDisplayView (AnalayzerProAudioProcessor& processor);
    ~AnalyzerDisplayView() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void setMode (Mode mode);
    Mode getMode() const noexcept { return currentMode_; }

    void setDbRange (DbRange r);
    DbRange getDbRange() const noexcept { return dbRange_; }
    void setDbRangeFromChoiceIndex (int idx);

    void setPeakDbRange (DbRange r);
    DbRange getPeakDbRange() const noexcept { return peakDbRange_; }
    void triggerPeakFlash();

    RTADisplay& getRTADisplay() noexcept { return rtaDisplay; }
    const RTADisplay& getRTADisplay() const noexcept { return rtaDisplay; }

    /** Shutdown: stop timer and clear references. Safe to call multiple times. */
    void shutdown();

private:
    void timerCallback() override;
    void updateFromSnapshot (const AnalyzerSnapshot& snapshot);
    
    // Convert FFT bins to 1/3-octave bands
    void convertFFTToBands (const AnalyzerSnapshot& snapshot, std::vector<float>& bandsDb, std::vector<float>& bandsPeakDb);
    
    // Convert FFT bins to log-spaced bins
    void convertFFTToLog (const AnalyzerSnapshot& snapshot, std::vector<float>& logDb, std::vector<float>& logPeakDb);
    
    // Generate standard 1/3-octave band centers (20 Hz to 20 kHz)
    static std::vector<float> generateThirdOctaveBands();
    
    // Map AnalyzerDisplayView::Mode to RTADisplay view mode (0=FFT, 1=LOG, 2=BAND)
    static int toRtaMode (Mode m) noexcept;
    
#if JUCE_DEBUG
    void assertModeSync() const;
#endif

#if JUCE_DEBUG && ANALYZERPRO_MODE_DEBUG_OVERLAY
    void updateModeOverlayText();
#endif

    AnalayzerProAudioProcessor& audioProcessor;
    RTADisplay rtaDisplay;
    Mode currentMode_ = Mode::FFT;
    DbRange dbRange_ = DbRange::Minus120;
    DbRange appliedDbRange_ = DbRange::Minus120;
    DbRange peakDbRange_ = DbRange::Minus90;
    bool peakScaleDirty_ = false;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> minDbAnim_;
    float targetMinDb_ = -120.0f;
    float lastAppliedMinDb_ = -120.0f;

    std::vector<float> fftPeakDbDisplay_;
    std::vector<float> bandsPeakDbDisplay_;
    std::vector<float> logPeakDbDisplay_;

    bool peakFlashActive_ = false;
    double peakFlashUntilMs_ = 0.0;
    uint32_t lastSequence_ = 0;  // Track sequence to avoid unnecessary updates
    AnalyzerSnapshot snapshot_;
    AnalyzerSnapshot lastValidSnapshot_;  // Hold last valid frame for grace period
    bool hasLastValid_ = false;
    std::vector<float> fftDb_;
    std::vector<float> fftPeakDb_;
    std::vector<float> bandsDb_;
    std::vector<float> bandsPeakDb_;
    std::vector<float> logDb_;
    std::vector<float> logPeakDb_;
    std::vector<float> bandCentersHz_;  // Cached 1/3-octave band centers
    float lastPeakDb_ = -1000.0f;
    float lastMinDb_ = 0.0f;
    float lastMaxDb_ = 0.0f;
    int lastBins_ = 0;
    int lastFftSize_ = 0;
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
    int lastSentRtaMode_ = 0;  // Cache for mode sync assertion
#endif
#if JUCE_DEBUG && ANALYZERPRO_FFT_DEBUG_LINE
    juce::String fftDebugLine_;
#endif
#if defined(PLUGIN_DEV_MODE) && PLUGIN_DEV_MODE
    juce::String devModeDebugLine_;  // Temporary: UI=mode / RTADisplay=mode / bins / min/max dB
#endif
    
#if JUCE_DEBUG && ANALYZERPRO_MODE_DEBUG_OVERLAY
    struct ModeDebugOverlay final : public juce::Component
    {
        void setText (juce::String t) { text = std::move (t); repaint(); }
        void paint (juce::Graphics& g) override
        {
            // Debug overlay uses theme-aware colors (variant-aware, defaults to Dark for Phase 1)
            const mdsp_ui::Theme theme (mdsp_ui::ThemeVariant::Dark);
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalyzerDisplayView)
};
