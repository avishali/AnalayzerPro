#include "AnalyzerEngine.h"
#include <juce_events/juce_events.h>

//==============================================================================
// Adapter must not do smoothing/math; only maps params -> config and forwards
// audio/snapshots. No exp/pow/log/onePole/calcCoeff or other DSP in this layer.
//==============================================================================

AnalyzerEngine::AnalyzerEngine() = default;

AnalyzerEngine::~AnalyzerEngine() = default;

void AnalyzerEngine::prepare (double sampleRate, int samplesPerBlock)
{
    core_.prepare (sampleRate, samplesPerBlock);
}

void AnalyzerEngine::reset()
{
    core_.reset();
}

void AnalyzerEngine::processBlock (const juce::AudioBuffer<float>& buffer)
{
    core_.processBlock (buffer);

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    const float* left = buffer.getReadPointer (0);
    const float* right = (numChannels > 1) ? buffer.getReadPointer (1) : left;
    // TODO (future Slice): extract StereoScope DSP processor to mdsp_dsp/scopes when generalized.
    stereoScopeAnalyzer_.pushSamples (left, right, numSamples);
}

bool AnalyzerEngine::getLatestSnapshot (AnalyzerSnapshot& dest) const
{
    return core_.getLatestSnapshot (dest);
}

const float* AnalyzerEngine::getFFTData() const noexcept
{
    return core_.getFFTData();
}

void AnalyzerEngine::setFftSize (int fftSize)
{
    core_.setFftSize (fftSize);
}

void AnalyzerEngine::requestFftSize (int fftSize)
{
    core_.requestFftSize (fftSize);
}

void AnalyzerEngine::applyPendingFftSizeIfNeeded()
{
#if JUCE_DEBUG
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
#endif
    core_.applyPendingFftSizeIfNeeded();
}

void AnalyzerEngine::setAveragingMs (float averagingMs)
{
    core_.setAveragingMs (averagingMs);
}

void AnalyzerEngine::setSmoothingOctaves (float octaves)
{
    core_.setSmoothingOctaves (octaves);
}

void AnalyzerEngine::setWeightingMode (int mode)
{
    core_.setWeightingMode (mode);
}

void AnalyzerEngine::resetPeaks()
{
    core_.resetPeaks();
}

void AnalyzerEngine::setPeakHoldMode (PeakHoldMode mode)
{
    core_.setPeakHoldMode (toCore (mode));
}

void AnalyzerEngine::setPeakHoldTimeMs (float holdTimeMs)
{
    core_.setPeakHoldTimeMs (holdTimeMs);
}

void AnalyzerEngine::setHold (bool hold)
{
    core_.setHold (hold);
}

void AnalyzerEngine::setPeakDecayDbPerSec (float decayDbPerSec)
{
    core_.setPeakDecayDbPerSec (decayDbPerSec);
}

void AnalyzerEngine::setPeakDecayCurve (PeakDecayCurve curve)
{
    core_.setPeakDecayCurve (toCore (curve));
}

void AnalyzerEngine::setPeakDecayTimeConstantSec (float seconds)
{
    core_.setPeakDecayTimeConstantSec (seconds);
}

void AnalyzerEngine::setReleaseTimeMs (float ms)
{
    core_.setReleaseTimeMs (ms);
}

mdsp_dsp::AnalyzerEngine::PeakHoldMode AnalyzerEngine::toCore (PeakHoldMode m)
{
    switch (m)
    {
        case PeakHoldMode::Off:          return mdsp_dsp::AnalyzerEngine::PeakHoldMode::Off;
        case PeakHoldMode::Infinite:     return mdsp_dsp::AnalyzerEngine::PeakHoldMode::Infinite;
        case PeakHoldMode::Decay:        return mdsp_dsp::AnalyzerEngine::PeakHoldMode::Decay;
        case PeakHoldMode::HoldThenDecay: return mdsp_dsp::AnalyzerEngine::PeakHoldMode::HoldThenDecay;
    }
    return mdsp_dsp::AnalyzerEngine::PeakHoldMode::HoldThenDecay;
}

mdsp_dsp::AnalyzerEngine::PeakDecayCurve AnalyzerEngine::toCore (PeakDecayCurve c)
{
    switch (c)
    {
        case PeakDecayCurve::DbPerSec:        return mdsp_dsp::AnalyzerEngine::PeakDecayCurve::DbPerSec;
        case PeakDecayCurve::TimeConstant60dB: return mdsp_dsp::AnalyzerEngine::PeakDecayCurve::TimeConstant60dB;
    }
    return mdsp_dsp::AnalyzerEngine::PeakDecayCurve::DbPerSec;
}
