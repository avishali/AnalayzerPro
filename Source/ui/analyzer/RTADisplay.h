#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/analyzer/AnalyzerRenderState.h>
#include <mdsp_ui/Theme.h>
#include <mdsp_ui/rta/RTAEnums.h>
#include <mdsp_ui/rta/RTAGeometry.h>
#include <mdsp_ui/rta/RTADisplayModel.h>
#include <mdsp_ui/rta/RTADisplayController.h>
#include <mdsp_ui/rta/RTADisplayRenderer.h>
#include <vector>
#include <cstdint>
#include <cmath>
#include <functional>


//==============================================================================
/**
    Professional RTA display component for 1/3-octave and 1-octave bands.
    Renders band bars, peak trace, grid, labels, and cursor readout.
*/
class RTADisplay : public juce::Component,
                   private juce::Timer
{
public:
    RTADisplay();
    ~RTADisplay() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override;
    


    /** Set current band data in dBFS */
    void setBandData (const std::vector<float>& currentDb, const std::vector<float>* peakDbNullable = nullptr);

    /** Set band center frequencies in Hz */
    void setBandCenters (const std::vector<float>& centersHz);
    
    /** Set log band center frequencies in Hz */
    void setLogCenters (const std::vector<float>& centersHz);
    
    /** Set FFT metadata (sample rate and FFT size for frequency mapping) */
    void setFftMeta (double sampleRate, int fftSize);

    /** Set frequency range for display */
    void setFrequencyRange (float minHz, float maxHz);

    /** Set dB range for display */
    void setDbRange (float topDb, float bottomDb);

    /** Set view mode: 0=FFT, 1=Log, 2=Bands */
    void setViewMode (int mode);

    /** Set FFT data for FFT view mode */
    void setFFTData (const std::vector<float>& fftBinsDb, 
                     const std::vector<float>* peakBinsDbNullable = nullptr,
                     const std::vector<float>* peakHoldBinsDbNullable = nullptr);

    /** Set log band data for Log view mode */
    void setLogData (const std::vector<float>& logBandsDb, const std::vector<float>* peakBandsDbNullable = nullptr);

    /** Set no-data state (call when data is unavailable) */
    void setNoData (const juce::String& reason);
    
    /** Set display gain offset (UI-only, affects display rendering, not DSP) */
    void setDisplayGainDb (float db);
    
    /** Tilt mode for frequency compensation (defined in rta::RTAEnums.h) */
    using TiltMode = rta::TiltMode;

    /** Set tilt mode (UI-only, affects display rendering, not DSP) */
    void setTiltMode (TiltMode mode);
    
    /** Set multi-trace power spectrum data (L, R, Mid, Side, Mono) */
    void setMultiTraceData (const float* powerL, const float* powerR,
                            const float* powerMid, const float* powerSide, const float* powerMono,
                            int binCount);

    /** Trace configuration for multi-trace rendering (defined in RTADisplayModel) */
    using TraceConfig = RTADisplayModel::TraceConfig;

    /** Set trace configuration (which traces to render) */
    void setTraceConfig (const TraceConfig& config);

    /** Set hold status (UI-only, affects debug overlay) */
    void setHoldStatus (bool isHoldOn);

    /** Set session marker (visual indicator for highest peak in session) */
    void setSessionMarker (bool visible, int bin, float db);
    
    /** Check structural generation and clear cache if changed (call before pulling data) */
    void checkStructuralGeneration (uint32_t currentGen);

    /** SMOOTHING_RENDERING_STABILITY_V2: Set generation counters for path validity gating */
    void setGenerations (uint32_t traceDataGen, uint32_t smoothingGen);
    
    /** SMOOTHING_RENDERING_STABILITY_V2: Invalidate cached paths (force rebuild on next paint) */
    /** SMOOTHING_RENDERING_STABILITY_V2: Invalidate cached paths (force rebuild on next paint) */
    void invalidatePaths();
    
    /** Invalidate background cache (call on resize or range change) */
    void invalidateBackground();

    /** When set, paint() uses renderer with state from callback for analyzer plot and overlays. */
    void setGetRenderState (std::function<mdsp_ui::AnalyzerRenderState()> cb);

    /** Set theme callback. When set, paint() uses this instead of default-constructed theme. */
    void setGetTheme (std::function<const mdsp_ui::Theme&()> cb);

#if JUCE_DEBUG
    /** Set debug info with structural generation - DEBUG only */
    void setDebugInfo (int viewMode, size_t fftSize, size_t logSize, size_t bandsSize,
                       bool fftValid, bool logValid, bool bandsValid, uint32_t structuralGen,
                       int bandMode, float minDb, float maxDb, float peakMinDb, float peakMaxDb);
#endif

private:
    using RenderState = RTADisplayModel::RenderState;
    using RenderConfigKey = RTADisplayModel::RenderConfigKey;
    
    void refreshBackground();

    // Helper: compute tilt compensation in dB for a given frequency
    float computeTiltDb (float freqHz) const;
    // FFT crosshair: get dB at bin for active trace (peak if enabled, else main); returns -200.0f if invalid
    float getActiveTraceDbAtBin (int binIndex, const RenderState& s) const;
    
    // Mapping function wrappers (delegate to RTAGeometry)
    int findNearestBand (float x) const;
    int findNearestLogBand (float x, const RenderState& s) const;
    float mapXToFreqFFT (float x, const RenderState& s) const;
    int mapFreqToBinIndex (float freqHz, const RenderState& s) const;
    float dbToYWithCompensation (float db, float freqHz, const RenderState& s) const;

    std::function<mdsp_ui::AnalyzerRenderState()> getRenderState_;
    std::function<const mdsp_ui::Theme&()> getTheme_;
    mdsp_ui::Theme fallbackTheme_;

    // Model owns all data and cached paths
    RTADisplayModel model_;

    // Geometry and coordinate mapping
    RTAGeometry geometry_;

    // Interaction controller (mouse, wheel, hover, drag, timer smoothing)
    RTADisplayController controller_;

    // Renderer (all paint logic)
    RTADisplayRenderer renderer_;

    void timerCallback() override;
    
    // Display gain offset (UI-only, affects rendering, not DSP)
    float displayGainDb = 0.0f;
    
    // Tilt mode for frequency compensation (UI-only, affects rendering, not DSP)
    TiltMode tiltMode = TiltMode::Flat;
    
    // Trace configuration (which traces to render)
    TraceConfig traceConfig_;

    // Debug info (DEBUG only)
#if JUCE_DEBUG
    int debugViewMode = 2;
    size_t debugFFTSize = 0;
    size_t debugLogSize = 0;
    size_t debugBandsSize = 0;
    bool debugFFTValid = false;
    bool debugLogValid = false;
    bool debugBandsValid = false;
    uint32_t debugStructuralGen = 0;
    int debugBandMode = 0;
    float debugMinDb = 0.0f;
    float debugMaxDb = 0.0f;
    float debugPeakMinDb = 0.0f;
    float debugPeakMaxDb = 0.0f;
#endif

    // Fonts and colors (cached)
    juce::Font labelFont { juce::FontOptions().withHeight (12.0f) };
    juce::Font smallFont { juce::FontOptions().withHeight (10.0f) };



    RenderConfigKey lastRenderKey_;  // For weighting path cache key

    // Step 4 & 5: Per-pixel aggregation buffers
    // These replace the ad-hoc SeriesRenderer decimation for RMS correctness
    std::vector<float> pixelRms_;    // Accumulator for Power (Mean) -> then dB
    std::vector<float> pixelPeak_;   // Accumulator for Peak dB (Max)
    std::vector<int> pixelCounts_;   // Count of bins per pixel (for Mean)
    std::vector<float> pixelFreqs_;  // Cache x -> freq mapping (optional, but good for drawing)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RTADisplay)
};
