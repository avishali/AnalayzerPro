#pragma once

#include "RTAEnums.h"
#include <juce_graphics/juce_graphics.h>
#include <mdsp_ui/Theme.h>
#include <vector>
#include <cstdint>
#include <string>
#include <functional>

class RTAGeometry;

//==============================================================================
/**
    Model for RTA display data and cached rendering paths.
    Owns all data buffers, cached paths, and path building logic.
    Does NOT paint (no juce::Graphics usage).
*/
class RTADisplayModel
{
public:
    RTADisplayModel();
    ~RTADisplayModel() = default;

    enum class DataStatus
    {
        Ok,
        NoData
    };

    // Render state structure (moved from RTADisplay)
    struct RenderState
    {
        int viewMode = 2;  // 0=FFT, 1=Log, 2=Bands
        
        // Ranges
        float minHz = 20.0f;
        float maxHz = 20000.0f;
        float topDb = 6.0f;
        float bottomDb = -200.0f;
        
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
        std::vector<float> fftPeakHoldDb; // Peak Hold Trace
        
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

        // FFT No Data Guard
        bool hasValidSpectrumFrame = false;

        // Unified source of truth for rendering limit
        float getEffectiveMaxHz() const
        {
            const float nyquist = static_cast<float> (sampleRate * 0.5);
            if (nyquist <= 1.0f) return maxHz;
            return std::min (maxHz, nyquist);
        }
    };

    /** Trace configuration for multi-trace rendering (shared by model and display) */
    struct TraceConfig
    {
        bool showLR = false;
        bool showMono = false;
        bool showL = false;
        bool showR = false;
        bool showMid = false;
        bool showSide = false;
        bool showRMS = false;
        int weightingMode = 0;
    };

    // Render config key for cache invalidation
    struct RenderConfigKey
    {
        int fftSize = 0;
        double sampleRate = 0.0;
        float minHz = 0.0f;
        float maxHz = 0.0f;
        float plotWidth = 0.0f;
        bool isLog = true;

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

    // Reset all data
    void reset();

    // Structural change tracking
    void checkStructuralGeneration (uint32_t currentGen);
    uint32_t getLastStructuralGen() const { return lastStructuralGen_; }

    // Generation tracking for path validity gating
    void setGenerations (uint32_t traceDataGen, uint32_t smoothingGen);
    uint32_t getCurrentTraceDataGen() const { return currentTraceDataGen_; }
    uint32_t getCurrentSmoothingGen() const { return currentSmoothingGen_; }

    // Data setters
    void setBandData (const std::vector<float>& currentDb, const std::vector<float>* peakDbNullable = nullptr);
    void setBandCenters (const std::vector<float>& centersHz);
    void setLogCenters (const std::vector<float>&);
    void setLogData (const std::vector<float>& logBandsDb, const std::vector<float>* peakBandsDbNullable = nullptr);
    void setFFTData (const std::vector<float>& fftBinsDb, 
                     const std::vector<float>* peakBinsDbNullable = nullptr,
                     const std::vector<float>* peakHoldBinsDbNullable = nullptr);
    void setMultiTraceData (const float* powerL, const float* powerR,
                            const float* powerMid, const float* powerSide, const float* powerMono,
                            int binCount);
    void setFftMeta (double sampleRate, int fftSize);
    void setFrequencyRange (float minHz, float maxHz);
    void setDbRange (float topDb, float bottomDb);
    void setViewMode (int mode);
    void setNoData (const juce::String& reason);
    void setHoldStatus (bool isHoldOn);
    void setSessionMarker (bool visible, int bin, float db);

    // Accessors for RenderState
    const RenderState& getState() const { return state_; }
    RenderState& getState() { return state_; }

    // Path cache management
    void invalidatePaths();
    void invalidateBackground();
    bool arePathsValid() const { return pathsValid_; }
    bool isBackgroundValid() const { return backgroundValid_; }

    // Path building (called from paint when needed)
    void ensurePathsBuilt (const RTAGeometry& geometry,
                          float displayGainDb,
                          rta::TiltMode tiltMode,
                          const TraceConfig& traceConfig);
    
    /** Build cached background. Theme is passed in (rendering concern); model does not store it. */
    void refreshBackground (const RTAGeometry& geometry,
                           const RenderState& state,
                           float displayGainDb,
                           const mdsp_ui::Theme& theme,
                           std::function<void(juce::Graphics&, const RenderState&, const mdsp_ui::Theme&)> drawGrid);

    // Path accessors (for paint code)
    const juce::Path& getCachedFftPath() const { return cachedFftPath_; }
    const juce::Path& getCachedPeakPath() const { return cachedPeakPath_; }
    const juce::Path& getCachedPeakHoldPath() const { return cachedPeakHoldPath_; }
    const juce::Path& getCachedLPath() const { return cachedLPath_; }
    const juce::Path& getCachedRPath() const { return cachedRPath_; }
    const juce::Path& getCachedStereoPath() const { return cachedStereoPath_; }
    const juce::Path& getCachedMonoPath() const { return cachedMonoPath_; }
    const juce::Path& getCachedMidPath() const { return cachedMidPath_; }
    const juce::Path& getCachedSidePath() const { return cachedSidePath_; }
    const juce::Path& getWeightingPath() const { return weightingPath_; }
    const juce::Image& getCachedBackground() const { return cachedBackground_; }

    // Weighting path management
    int getLastWeightingMode() const { return lastWeightingMode_; }
    const RenderConfigKey& getLastWeightingKey() const { return lastWeightingKey_; }
    void setWeightingPath (const juce::Path& path, int mode, const RenderConfigKey& key);

private:
    RenderState state_;

    // Structural generation tracking
    uint32_t lastStructuralGen_ = 0;

    // Path validity gating
    uint32_t currentTraceDataGen_ = 0;
    uint32_t currentSmoothingGen_ = 0;
    uint32_t lastPathTraceDataGen_ = 0;
    uint32_t lastPathSmoothingGen_ = 0;

    bool pathsValid_ = false;
    uint32_t pathGen_ = 0;
    uint32_t lastBuiltGen_ = 0;

    // Cached paths
    juce::Path cachedFftPath_;
    juce::Path cachedPeakPath_;
    juce::Path cachedPeakHoldPath_;
    juce::Path cachedLPath_;
    juce::Path cachedRPath_;
    juce::Path cachedStereoPath_;
    juce::Path cachedMonoPath_;
    juce::Path cachedMidPath_;
    juce::Path cachedSidePath_;

    // Weighting overlay
    juce::Path weightingPath_;
    int lastWeightingMode_ = 0;
    RenderConfigKey lastWeightingKey_;

    // Background cache
    juce::Image cachedBackground_;
    bool backgroundValid_ = false;

    // Internal path building helpers
    void buildFftPaths (const RTAGeometry& geometry,
                       float displayGainDb,
                       rta::TiltMode tiltMode,
                       const TraceConfig& traceConfig);
    void buildDecimatedPath (const std::vector<float>& data,
                            juce::Path& path,
                            const RTAGeometry& geometry,
                            float displayGainDb,
                            rta::TiltMode tiltMode);
};
