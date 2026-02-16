#include "RTADisplayModel.h"
#include "RTAGeometry.h"
#include "RTACurveHelpers.h"
#include <mdsp_ui/Theme.h>
#include <cmath>
#include <algorithm>
#include <limits>

//==============================================================================
RTADisplayModel::RTADisplayModel()
{
    constexpr float kGridMinDb = -200.0f;
    state_.minHz = 20.0f;
    state_.maxHz = 20000.0f;
    state_.topDb = 6.0f;
    state_.bottomDb = kGridMinDb;
}

void RTADisplayModel::reset()
{
    state_.bandsDb.clear();
    state_.bandsPeakDb.clear();
    state_.bandCentersHz.clear();
    state_.logDb.clear();
    state_.logPeakDb.clear();
    state_.fftDb.clear();
    state_.fftPeakDb.clear();
    state_.fftPeakHoldDb.clear();
    state_.lDbL.clear();
    state_.lDbR.clear();
    state_.stereoDb.clear();
    state_.monoDb.clear();
    state_.midDb.clear();
    state_.sideDb.clear();
    state_.status = DataStatus::NoData;
    state_.noDataReason = "reset";
    state_.hasValidSpectrumFrame = false;
    invalidatePaths();
}

void RTADisplayModel::checkStructuralGeneration (uint32_t currentGen)
{
    if (currentGen != lastStructuralGen_)
    {
        lastStructuralGen_ = currentGen;
        reset();
    }
}

void RTADisplayModel::setGenerations (uint32_t traceDataGen, uint32_t smoothingGen)
{
    if (traceDataGen != currentTraceDataGen_ || smoothingGen != currentSmoothingGen_)
    {
        currentTraceDataGen_ = traceDataGen;
        currentSmoothingGen_ = smoothingGen;
        invalidatePaths();
    }
}

void RTADisplayModel::invalidatePaths()
{
    pathsValid_ = false;
    pathGen_++;
    lastWeightingMode_ = -1;
}

void RTADisplayModel::invalidateBackground()
{
    backgroundValid_ = false;
}

//==============================================================================
void RTADisplayModel::setBandData (const std::vector<float>& currentDb, const std::vector<float>* peakDbNullable)
{
    state_.bandsDb = currentDb;
    if (peakDbNullable != nullptr)
        state_.bandsPeakDb = *peakDbNullable;
    else
        state_.bandsPeakDb.clear();
    state_.status = DataStatus::Ok;
}

void RTADisplayModel::setBandCenters (const std::vector<float>& centersHz)
{
    state_.bandCentersHz = centersHz;
}

void RTADisplayModel::setLogCenters (const std::vector<float>&)
{
    // Log mode computes centers on-the-fly, no storage needed
}

void RTADisplayModel::setLogData (const std::vector<float>& logBandsDb, const std::vector<float>* peakBandsDbNullable)
{
    state_.logDb = logBandsDb;
    if (peakBandsDbNullable != nullptr)
        state_.logPeakDb = *peakBandsDbNullable;
    else
        state_.logPeakDb.clear();
    state_.status = DataStatus::Ok;
}

void RTADisplayModel::setFFTData (const std::vector<float>& fftBinsDb, 
                                   const std::vector<float>* peakBinsDbNullable,
                                   const std::vector<float>* peakHoldBinsDbNullable)
{
    state_.fftDb = fftBinsDb;
    
    if (peakBinsDbNullable != nullptr)
        state_.fftPeakDb = *peakBinsDbNullable;
    else
        state_.fftPeakDb.clear();

    if (peakHoldBinsDbNullable != nullptr)
        state_.fftPeakHoldDb = *peakHoldBinsDbNullable;
    else
        state_.fftPeakHoldDb.clear();

    // FFT No Data Guard: Content-based valid frame
    const float thresholdDb = state_.bottomDb + 6.0f;
    float maxDb = state_.bottomDb;
    for (float v : state_.fftDb)
        if (std::isfinite (v)) maxDb = std::max (maxDb, v);
    state_.hasValidSpectrumFrame = (maxDb > thresholdDb);

    state_.status = DataStatus::Ok;
    invalidatePaths();
}

void RTADisplayModel::setMultiTraceData (const float* powerL, const float* powerR,
                                         const float* powerMid, const float* powerSide, const float* powerMono,
                                         int binCount)
{
    if (powerL == nullptr || powerR == nullptr || binCount <= 0)
    {
        state_.lrBinCount = 0;
        state_.lDbL.clear();
        state_.lDbR.clear();
        state_.stereoDb.clear();
        state_.monoDb.clear();
        state_.midDb.clear();
        state_.sideDb.clear();
        state_.hasValidMultiTraceData = false;
        invalidatePaths();
        return;
    }
    
    state_.lrBinCount = binCount;
    if (state_.lDbL.size() != static_cast<size_t> (binCount))
    {
        size_t sz = static_cast<size_t> (binCount);
        state_.lDbL.resize (sz);
        state_.lDbR.resize (sz);
        state_.stereoDb.resize (sz);
        state_.monoDb.resize (sz);
        state_.midDb.resize (sz);
        state_.sideDb.resize (sz);
    }
    
    state_.hasValidMultiTraceData = true;
    
    float maxDb = -200.0f;

    for (int i = 0; i < binCount; ++i)
    {
        const size_t idx = static_cast<size_t> (i);
        state_.lDbL[idx] = powerL[idx];
        state_.lDbR[idx] = powerR[idx];
        
        if (powerMid) state_.midDb[idx] = powerMid[idx];
        if (powerSide) state_.sideDb[idx] = powerSide[idx];
        if (powerMono) state_.monoDb[idx] = powerMono[idx];
        
        state_.stereoDb[idx] = std::max(state_.lDbL[idx], state_.lDbR[idx]);

        float pMax = state_.stereoDb[idx]; 
        if (pMax > maxDb) maxDb = pMax;
    }
    
    if (state_.hasValidSpectrumFrame)
    {
        // Main spectrum is valid - multi-trace data is valid regardless of level
    }
    else if (std::isfinite (maxDb) && maxDb > (state_.bottomDb + 6.0f))
    {
        state_.hasValidSpectrumFrame = true;
    }
    
    invalidatePaths();
}

void RTADisplayModel::setFftMeta (double sampleRate, int fftSize)
{
    state_.sampleRate = sampleRate;
    state_.fftSize = fftSize;
    if (fftSize <= 0 || sampleRate <= 0.0)
         state_.hasValidSpectrumFrame = false;
    invalidatePaths();
}

void RTADisplayModel::setFrequencyRange (float minHz, float maxHz)
{
    if (std::abs (state_.minHz - minHz) > 1e-5f || std::abs (state_.maxHz - maxHz) > 1e-5f)
    {
        state_.minHz = minHz;
        state_.maxHz = maxHz;
        invalidateBackground();
        invalidatePaths();
    }
}

void RTADisplayModel::setDbRange (float topDb, float bottomDb)
{
    if (std::abs (state_.topDb - topDb) > 1e-5f || std::abs (state_.bottomDb - bottomDb) > 1e-5f)
    {
        state_.topDb = topDb;
        state_.bottomDb = bottomDb;
        invalidateBackground();
        invalidatePaths();
    }
}

void RTADisplayModel::setViewMode (int mode)
{
    if (state_.viewMode != mode)
    {
        state_.viewMode = mode;
        invalidatePaths();
    }
}

void RTADisplayModel::setNoData (const juce::String& reason)
{
    state_.status = DataStatus::NoData;
    state_.noDataReason = reason;
}

void RTADisplayModel::setHoldStatus (bool isHoldOn)
{
    state_.isHoldOn = isHoldOn;
}

void RTADisplayModel::setSessionMarker (bool visible, int bin, float db)
{
    if (state_.sessionMarkerVisible != visible || 
        state_.sessionMarkerBin != bin || 
        std::abs (state_.sessionMarkerDb - db) > 1e-4f)
    {
        state_.sessionMarkerVisible = visible;
        state_.sessionMarkerBin = bin;
        state_.sessionMarkerDb = db;
    }
}

//==============================================================================
void RTADisplayModel::ensurePathsBuilt (const RTAGeometry& geometry,
                                        float displayGainDb,
                                        rta::TiltMode tiltMode,
                                        const TraceConfig& traceConfig)
{
    if (pathsValid_ && lastBuiltGen_ == pathGen_)
        return;

    buildFftPaths (geometry, displayGainDb, tiltMode, traceConfig);
    
    lastBuiltGen_ = pathGen_;
    pathsValid_ = true;
}

void RTADisplayModel::buildFftPaths (const RTAGeometry& geometry,
                                     float displayGainDb,
                                     rta::TiltMode tiltMode,
                                     const TraceConfig& traceConfig)
{
    cachedFftPath_.clear();
    cachedPeakPath_.clear();
    cachedPeakHoldPath_.clear();
    cachedLPath_.clear();
    cachedRPath_.clear();
    cachedStereoPath_.clear();
    cachedMonoPath_.clear();
    cachedMidPath_.clear();
    cachedSidePath_.clear();

    if (state_.sampleRate <= 0.0 || state_.fftSize <= 0)
        return;

    if (!state_.fftDb.empty())
        buildDecimatedPath(state_.fftDb, cachedFftPath_, geometry, displayGainDb, tiltMode);

    if (!state_.fftPeakDb.empty() && state_.fftPeakDb.size() == state_.fftDb.size())
    {
        bool hasPeakSignal = false;
        constexpr float kPeakVisibleThresholdDb = -100.0f;

        for (float db : state_.fftPeakDb)
        {
            if (db > kPeakVisibleThresholdDb)
            {
                hasPeakSignal = true;
                break;
            }
        }

        if (hasPeakSignal)
            buildDecimatedPath(state_.fftPeakDb, cachedPeakPath_, geometry, displayGainDb, tiltMode);
        else
            cachedPeakPath_.clear();
    }

    if (!state_.fftPeakHoldDb.empty() && state_.fftPeakHoldDb.size() == state_.fftDb.size())
    {
        bool hasPeakData = false;
        constexpr float kVisibleThresholdDb = -100.0f;
        
        for (float db : state_.fftPeakHoldDb)
        {
            if (db > kVisibleThresholdDb)
            {
                hasPeakData = true;
                break;
            }
        }
        
        if (hasPeakData)
            buildDecimatedPath(state_.fftPeakHoldDb, cachedPeakHoldPath_, geometry, displayGainDb, tiltMode);
    }
        
    if (state_.hasValidMultiTraceData && static_cast<size_t>(state_.lrBinCount) == state_.fftDb.size())
    {
         if (traceConfig.showL) buildDecimatedPath(state_.lDbL, cachedLPath_, geometry, displayGainDb, tiltMode);
         if (traceConfig.showR) buildDecimatedPath(state_.lDbR, cachedRPath_, geometry, displayGainDb, tiltMode);
         if (traceConfig.showMid) buildDecimatedPath(state_.midDb, cachedMidPath_, geometry, displayGainDb, tiltMode);
         if (traceConfig.showSide) buildDecimatedPath(state_.sideDb, cachedSidePath_, geometry, displayGainDb, tiltMode);
         if (traceConfig.showMono) buildDecimatedPath(state_.monoDb, cachedMonoPath_, geometry, displayGainDb, tiltMode);
         if (!state_.stereoDb.empty()) buildDecimatedPath(state_.stereoDb, cachedStereoPath_, geometry, displayGainDb, tiltMode);
    }
}

void RTADisplayModel::buildDecimatedPath (const std::vector<float>& data,
                                          juce::Path& path,
                                          const RTAGeometry& geometry,
                                          float displayGainDb,
                                          rta::TiltMode tiltMode)
{
    path.clear();
    if (data.empty()) return;
    
    const int w = static_cast<int>(geometry.getPlotAreaWidth());
    if (w <= 0) return;

    std::vector<juce::Point<float>> pts;
    pts.reserve(static_cast<size_t>(w + 2));
    
    const float logMin = std::log10(state_.minHz);
    const float logMax = std::log10(state_.maxHz);
    const float logRange = logMax - logMin;
    const size_t numBins = data.size();
    const float binWidthHz = static_cast<float> (state_.sampleRate) / static_cast<float> (state_.fftSize);
    
    float lastX = -std::numeric_limits<float>::max();
    
    for (int x = 0; x <= w; ++x)
    {
        const float x0 = geometry.getPlotAreaLeft() + static_cast<float>(x);
        const float x1 = x0 + 1.0f;
        
        if (!std::isfinite(x0)) continue;
        
        auto xToFreq = [&](float px) -> float {
            const float norm = (px - geometry.getPlotAreaLeft()) / geometry.getPlotAreaWidth();
            return std::pow(10.0f, logMin + norm * logRange);
        };
        
        const float freqStart = xToFreq(x0);
        const float freqEnd = xToFreq(x1);
        
        const float binStartF = freqStart / binWidthHz;
        const float binEndF = freqEnd / binWidthHz;
        
        const size_t b0 = juce::jlimit((size_t)0, numBins - 1, static_cast<size_t>(binStartF));
        const size_t b1 = juce::jlimit((size_t)0, numBins, static_cast<size_t>(std::ceil(binEndF)));
        
        float finalDb = -200.0f;
        
        if ((binEndF - binStartF) < 1.0f)
        {
            const float freqCenter = xToFreq(x0 + 0.5f);
            const float exactBin = freqCenter / binWidthHz;
            const size_t idx = static_cast<size_t>(exactBin);
            const float frac = exactBin - static_cast<float>(idx);
            
            if (idx < numBins - 1)
            {
                 float v1 = data[idx];
                 float v2 = data[idx+1];
                 if (!std::isfinite(v1)) v1 = -200.0f;
                 if (!std::isfinite(v2)) v2 = -200.0f;
                 finalDb = v1 * (1.0f - frac) + v2 * frac;
            }
            else if (idx < numBins)
            {
                 finalDb = data[idx];
            }
        }
        else
        {
            size_t validB0 = b0;
            size_t validB1 = std::max(b1, validB0 + 1);
            validB1 = std::min(validB1, numBins);
            
            float maxVal = -200.0f;
            for (size_t k = validB0; k < validB1; ++k)
            {
                const float val = data[k];
                if (std::isfinite(val) && val > maxVal) maxVal = val;
            }
            finalDb = maxVal;
        }
            
        finalDb = juce::jlimit(state_.bottomDb, state_.topDb + 20.0f, finalDb);
        const float freqForY = xToFreq(x0 + 0.5f);
        const float tiltDb = mdsp_ui::rta::computeTiltDb (freqForY, tiltMode);
        const float y = geometry.dbToYWithCompensation(finalDb + displayGainDb, freqForY, tiltDb, 0.0f);

        if (!std::isfinite(y)) continue;
        
        if (x0 > lastX)
        {
            pts.emplace_back(x0, y);
            lastX = x0;
        }
    }
    
    if (pts.size() >= 3)
    {
        std::vector<float> ys (pts.size());
        for (size_t i = 0; i < pts.size(); ++i)
            ys[i] = pts[i].y;
        for (size_t i = 1; i < pts.size() - 1; ++i)
        {
            const float smoothed = 0.25f * ys[i - 1] + 0.5f * ys[i] + 0.25f * ys[i + 1];
            pts[i].y = smoothed;
        }
    }
    
    if (pts.empty()) return;
    
    juce::Path newPath;
    newPath.startNewSubPath(pts[0]);
    
    if (pts.size() < 3)
    {
        for (size_t i = 1; i < pts.size(); ++i)
            newPath.lineTo(pts[i]);
    }
    else
    {
        for (size_t i = 1; i < pts.size() - 2; ++i)
        {
            const auto& p1 = pts[i];
            const auto& p2 = pts[i+1];
            const auto mid = (p1 + p2) * 0.5f;
            newPath.quadraticTo(p1, mid);
        }
        
        const auto& secondLast = pts[pts.size() - 2];
        const auto& last = pts[pts.size() - 1];
        newPath.quadraticTo(secondLast, last);
    }
    
    const auto bounds = newPath.getBounds();
    if (!bounds.isFinite())
        return;
        
    const float maxDimension = std::max(geometry.getPlotAreaWidth(), geometry.getPlotAreaHeight()) * 10.0f;
    if (bounds.getWidth() > maxDimension || bounds.getHeight() > maxDimension)
        return;

    path = std::move(newPath);
}

void RTADisplayModel::setWeightingPath (const juce::Path& path, int mode, const RenderConfigKey& key)
{
    weightingPath_ = path;
    lastWeightingMode_ = mode;
    lastWeightingKey_ = key;
}

void RTADisplayModel::refreshBackground (const RTAGeometry& geometry,
                                         const RenderState& state,
                                         float displayGainDb,
                                         const mdsp_ui::Theme& theme,
                                         std::function<void(juce::Graphics&, const RenderState&, const mdsp_ui::Theme&)> drawGrid)
{
    if (backgroundValid_ && cachedBackground_.isValid())
        return;

    const auto bounds = juce::Rectangle<int> (0, 0, 
                                             static_cast<int>(geometry.getPlotAreaWidth() + 60),
                                             static_cast<int>(geometry.getPlotAreaHeight() + 40));
    if (bounds.isEmpty())
        return;
        
    if (cachedBackground_.getWidth() != bounds.getWidth() || cachedBackground_.getHeight() != bounds.getHeight())
    {
        cachedBackground_ = juce::Image (juce::Image::ARGB, bounds.getWidth(), bounds.getHeight(), true);
    }
    else
    {
        cachedBackground_.clear (bounds);
    }
    
    juce::Graphics g (cachedBackground_);
    drawGrid (g, state, theme);
    
    backgroundValid_ = true;
}
