#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <type_traits>
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
        float bottomDb = -90.0f;
        
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
        
        // Meta (optional)
        double sampleRate = 48000.0;
        int fftSize = 2048;
        
        // Status
        DataStatus status = DataStatus::Ok;
        juce::String noDataReason;
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
    
    // Frequency axis hover controller (for Log mode)
    mdsp_ui::AxisHoverController logHover_;
           
#if JUCE_DEBUG
           // Debug-only runtime toggle for envelope decimator (OFF by default)
           bool useEnvelopeDecimator = false;
#endif
    
    // Display gain offset (UI-only, affects rendering, not DSP)
    float displayGainDb = 0.0f;
    
    // Tilt mode for frequency compensation (UI-only, affects rendering, not DSP)
    TiltMode tiltMode = TiltMode::Flat;
    
    // Structural generation tracking (to detect structural changes)
    uint32_t lastStructuralGen = 0;

    // Data status
    DataStatus dataStatus = DataStatus::Ok;
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RTADisplay)
};
