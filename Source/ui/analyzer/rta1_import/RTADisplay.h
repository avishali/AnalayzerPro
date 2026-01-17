#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/AxisInteraction.h>
#include <mdsp_ui/AxisHoverController.h>
#include <mdsp_ui/PeakSnapController.h>
#include <vector>
#include <cstdint>
#include <cmath>

namespace mdsp_ui { struct Theme; }


//==============================================================================
/**
    Professional RTA display component for 1/3-octave and 1-octave bands.
    Renders band bars, peak trace, grid, labels, and cursor readout.
*/
class RTADisplay : public juce::Component
{
public:
    RTADisplay();
    ~RTADisplay() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    
#if JUCE_DEBUG
    void mouseDown (const juce::MouseEvent& e) override;
#endif

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
    void setFFTData (const std::vector<float>& fftBinsDb, const std::vector<float>* peakBinsDbNullable = nullptr);

    /** Set log band data for Log view mode */
    void setLogData (const std::vector<float>& logBandsDb, const std::vector<float>* peakBandsDbNullable = nullptr);

    /** Set no-data state (call when data is unavailable) */
    void setNoData (const juce::String& reason);
    
    /** Set display gain offset (UI-only, affects display rendering, not DSP) */
    void setDisplayGainDb (float db);
    
    /** Tilt mode for frequency compensation */
    enum class TiltMode
    {
        Flat = 0,  // 0 dB/oct
        Pink = 1,  // +3 dB/oct (compensate pink noise downward slope)
        White = 2  // -3 dB/oct (compensate white noise upward slope)
    };
    
    /** Set tilt mode (UI-only, affects display rendering, not DSP) */
    void setTiltMode (TiltMode mode);
    
    /** Set L/R power spectrum data for multi-trace rendering */
    void setLRPowerData (const float* powerL, const float* powerR, int binCount);

    /** Trace configuration for multi-trace rendering */
    struct TraceConfig
    {
        bool showLR = true;
        bool showMono = false;
        bool showL = false;
        bool showR = false;
        bool showMid = false;
        bool showSide = false;
        bool showRMS = true;  // Blue RMS trace
        
        // 0=None, 1=A-Weighting, 2=BS.468-4
        int weightingMode = 0;
    };
    
    /** Set trace configuration (which traces to render) */
    void setTraceConfig (const TraceConfig& config);

    /** Set hold status (UI-only, affects debug overlay) */
    void setHoldStatus (bool isHoldOn);

    /** Set session marker (visual indicator for highest peak in session) */
    void setSessionMarker (bool visible, int bin, float db);
    
    /** Check structural generation and clear cache if changed (call before pulling data) */
    void checkStructuralGeneration (uint32_t currentGen);

#if JUCE_DEBUG
    /** Set debug info with structural generation - DEBUG only */
    void setDebugInfo (int viewMode, size_t fftSize, size_t logSize, size_t bandsSize,
                       bool fftValid, bool logValid, bool bandsValid, uint32_t structuralGen,
                       int bandMode, float minDb, float maxDb, float peakMinDb, float peakMaxDb);
#endif

private:
    enum class DataStatus
    {
        Ok,
        NoData
    };
    
    // Cached geometry
    struct BandGeometry
    {
        float xCenter = 0.0f;
        float xLeft = 0.0f;
        float xRight = 0.0f;
    };
    
    // B1: RenderState - single state instance owned by RTADisplay
    struct RenderState
    {
        int viewMode = 2;  // 0=FFT, 1=Log, 2=Bands
        
        // Ranges
        float minHz = 20.0f;
        float maxHz = 20000.0f;
        float topDb = 0.0f;
        float bottomDb = -120.0f;
        
        // Bands view
        std::vector<float> bandCentersHz;
        std::vector<float> bandsDb;
        std::vector<float> bandsPeakDb;  // empty => no peaks
        
        // Log view (no logCentersHz - compute from index on-the-fly)
        std::vector<float> logDb;
        std::vector<float> logPeakDb;  // empty => no peaks
        
        // FFT view
        std::vector<float> fftDb;
        std::vector<float> fftPeakDb;  // empty => no peaks
        
        // Multi-trace: L/R power data (converted to dB on store)
        std::vector<float> lDbL;
        std::vector<float> lDbR;
        int lrBinCount = 0;
        
        // Derived traces (computed from L/R in setLRPowerData, NOT in paint)
        std::vector<float> stereoDb; // Max(L, R) combined envelope
        std::vector<float> monoDb;
        std::vector<float> midDb;
        std::vector<float> sideDb;
        
        bool hasValidMultiTraceData = false;
        
        // Meta (optional)
        double sampleRate = 48000.0;
        int fftSize = 2048;
        
        // Status
        DataStatus status = DataStatus::Ok;
        juce::String noDataReason;
        bool isHoldOn = false;
        
        // Session Marker
        bool sessionMarkerVisible = false;
        int sessionMarkerBin = -1;
        float sessionMarkerDb = 0.0f;

        // Unified source of truth for rendering limit
        float getEffectiveMaxHz() const
        {
            const float nyquist = static_cast<float> (sampleRate * 0.5);
            // If sampleRate is 0 or invalid, callback to maxHz (likely 20k)
            if (nyquist <= 1.0f) return maxHz;
            return std::min (maxHz, nyquist);
        }
    };
    
    // B2: Geometry cache derived only from state + component bounds, never mutated in paint
    void updateGeometry();  // Called from resized() and setBandCenters(), never from paint
    float frequencyToX (float freqHz) const;  // For updateGeometry() - uses member variables
    int findNearestBand (float x) const;  // For Bands mode hover - uses bandGeometry
    void paintBandsMode (juce::Graphics& g, const RenderState& s, const mdsp_ui::Theme& theme);
    void paintLogMode (juce::Graphics& g, const RenderState& s, const mdsp_ui::Theme& theme);
    void paintFFTMode (juce::Graphics& g, const RenderState& s, const mdsp_ui::Theme& theme);
    void drawGrid (juce::Graphics& g, const RenderState& s, const mdsp_ui::Theme& theme);
    
    // Helper: compute log frequency from index (for log mode rendering)
    float computeLogFreqFromIndex (int index, int numBands, float minHz, float maxHz) const;
    // Helper: compute x position from frequency (with guardrails)
    float freqToX (float freqHz, const RenderState& s) const;
    // Helper: compute y position from dB
    float dbToY (float db, const RenderState& s) const;
    
    // Helper: build frequency axis config (ticks, mapping, snap options, style)
    struct FreqAxisConfig
    {
        juce::Array<mdsp_ui::AxisTick> ticks;
        mdsp_ui::AxisMapping mapping;
        mdsp_ui::AxisSnapOptions snap;
        mdsp_ui::AxisHoverControllerStyle style;
    };
    FreqAxisConfig buildFreqAxisConfig (const RenderState& s) const;
    
    // Helper: build dB axis config (ticks, mapping, snap options, style)
    struct DbAxisConfig
    {
        juce::Array<mdsp_ui::AxisTick> ticks;
        mdsp_ui::AxisMapping mapping;
        mdsp_ui::AxisSnapOptions snap;
        mdsp_ui::AxisHoverControllerStyle style;
    };
    DbAxisConfig buildDbAxisConfig (const RenderState& s) const;
    // Helper: compute y position from dB with frequency-dependent compensation (for FFT mode)
    float dbToYWithCompensation (float db, float freqHz, const RenderState& s) const;
    // Helper: compute tilt compensation in dB for a given frequency
    float computeTiltDb (float freqHz) const;
    // Helper: find nearest log band index from x position
    int findNearestLogBand (float x, const RenderState& s) const;

    // B1: RTADisplay owns one RenderState state
    RenderState state;

    // Cached geometry (BandGeometry struct defined above in private section)
    std::vector<BandGeometry> bandGeometry;
    std::vector<BandGeometry> logGeometry;  // Separate geometry for log mode
    bool geometryValid = false;

    // Coordinate mapping factors (cached)
    float logFreqRange = 0.0f;
    float logMinFreq = 0.0f;
    float dbRange = 0.0f;
    float plotAreaLeft = 0.0f;
    float plotAreaTop = 0.0f;
    float plotAreaWidth = 0.0f;
    float plotAreaHeight = 0.0f;

    // Hover state
    int hoveredBandIndex = -1;
    
    // Axis hover controllers
    mdsp_ui::AxisHoverController freqHover_;  // Frequency axis (X, for Log/FFT modes)
    mdsp_ui::AxisHoverController dbHover_;    // dB axis (Y, Left edge)
    
    // Peak snap controller (for Log mode sticky peak snap)
    mdsp_ui::PeakSnapController peakSnap_;
           
#if JUCE_DEBUG
           // Debug-only runtime toggle for envelope decimator (OFF by default)
           bool useEnvelopeDecimator = false;
#endif
    
    // Display gain offset (UI-only, affects rendering, not DSP)
    float displayGainDb = 0.0f;
    
    // Tilt mode for frequency compensation (UI-only, affects rendering, not DSP)
    TiltMode tiltMode = TiltMode::Flat;
    
    // Trace configuration (which traces to render)
    TraceConfig traceConfig_;
    
    // Structural generation tracking (to detect structural changes)
    uint32_t lastStructuralGen = 0;

    // Data status

    juce::String noDataReason;

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

    // Step 3: Strict cache invalidation key
    struct RenderConfigKey
    {
        int fftSize = 0;
        double sampleRate = 0.0;
        float minHz = 0.0f;
        float maxHz = 0.0f;
        float plotWidth = 0.0f;
        bool isLog = true; // FFT mode is always log x-axis here

        bool operator!= (const RenderConfigKey& other) const
        {
            return fftSize != other.fftSize ||
                   std::abs (sampleRate - other.sampleRate) > 1e-5 ||
                   std::abs (minHz - other.minHz) > 1e-5f ||
                   std::abs (maxHz - other.maxHz) > 1e-5f ||
                   std::abs (plotWidth - other.plotWidth) > 0.5f ||
                   isLog != other.isLog;
        }
    };

    RenderConfigKey lastRenderKey_;

    // Step 4 & 5: Per-pixel aggregation buffers
    // These replace the ad-hoc SeriesRenderer decimation for RMS correctness
    std::vector<float> pixelRms_;    // Accumulator for Power (Mean) -> then dB
    std::vector<float> pixelPeak_;   // Accumulator for Peak dB (Max)
    std::vector<int> pixelCounts_;   // Count of bins per pixel (for Mean)
    std::vector<float> pixelFreqs_;  // Cache x -> freq mapping (optional, but good for drawing)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RTADisplay)
};
