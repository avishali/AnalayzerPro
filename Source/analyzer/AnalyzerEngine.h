#pragma once

#include "../dsp_adapters/AnalyzerSnapshotAdapter.h"
#include "StereoScopeAnalyzer.h"
#include <mdsp_dsp/analyzer/AnalyzerEngine.h>

//==============================================================================
/**
    Thin product adapter for the shared mdsp_dsp::AnalyzerEngine.
    Maps AnalyzerPro API to shared engine; owns StereoScopeAnalyzer.
*/
class AnalyzerEngine
{
public:
    AnalyzerEngine();
    ~AnalyzerEngine();

    void prepare (double sampleRate, int samplesPerBlock);

    StereoScopeAnalyzer& getStereoScopeAnalyzer() noexcept { return stereoScopeAnalyzer_; }
    const StereoScopeAnalyzer& getStereoScopeAnalyzer() const noexcept { return stereoScopeAnalyzer_; }

    void reset();
    void processBlock (const juce::AudioBuffer<float>& buffer);

    bool getLatestSnapshot (AnalyzerSnapshot& dest) const;
    const float* getFFTData() const noexcept;
    int getFFTSize() const noexcept { return core_.getFFTSize(); }
    bool hasNextDataBlock() const noexcept { return core_.hasNextDataBlock(); }
    void clearDataFlag() noexcept { core_.clearDataFlag(); }

    void setFftSize (int fftSize);
    void requestFftSize (int fftSize);
    void applyPendingFftSizeIfNeeded();

    void setAveragingMs (float averagingMs);
    void setSmoothingOctaves (float octaves);
    void setWeightingMode (int mode);
    void resetPeaks();

    enum class PeakHoldMode
    {
        Off = 0,
        Infinite,
        Decay,
        HoldThenDecay
    };

    void setPeakHoldMode (PeakHoldMode mode);
    void setPeakHoldTimeMs (float holdTimeMs);
    void setHold (bool hold);
    void setPeakDecayDbPerSec (float decayDbPerSec);

    enum class PeakDecayCurve
    {
        DbPerSec = 0,
        TimeConstant60dB = 1
    };

    void setPeakDecayCurve (PeakDecayCurve curve);
    void setPeakDecayTimeConstantSec (float seconds);
    void setReleaseTimeMs (float ms);

    enum class SpectralSmoothingStage
    {
        UISmoothingLogGaussian
    };
    static constexpr SpectralSmoothingStage kSpectralSmoothingStage = SpectralSmoothingStage::UISmoothingLogGaussian;

private:
    mdsp_dsp::AnalyzerEngine core_;
    StereoScopeAnalyzer stereoScopeAnalyzer_;

    static mdsp_dsp::AnalyzerEngine::PeakHoldMode toCore (PeakHoldMode m);
    static mdsp_dsp::AnalyzerEngine::PeakDecayCurve toCore (PeakDecayCurve c);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalyzerEngine)
};
