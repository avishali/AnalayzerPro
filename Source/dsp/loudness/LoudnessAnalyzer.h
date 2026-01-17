/*
  ==============================================================================

    LoudnessAnalyzer.h
    Created: 15 Jan 2026
    Author:  Antigravity

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <atomic>

namespace AnalyzerPro::dsp
{

struct LoudnessSnapshot
{
    float momentaryLufs = -100.0f;
    float shortTermLufs = -100.0f;
    float integratedLufs = -100.0f;
    float peakDb = -100.0f;
};

class LoudnessAnalyzer
{
public:
    LoudnessAnalyzer();
    ~LoudnessAnalyzer() = default;

    void prepare (double sampleRate, int estimatedSamplesPerBlock);
    void reset();
    void resetPeak();
    void process (const juce::AudioBuffer<float>& buffer);

    LoudnessSnapshot getSnapshot() const;

private:
    double currentSampleRate = 48000.0;

    // Filters for K-weighting
    // We process L and R separately until summation
    using Filter = juce::dsp::IIR::Filter<float>;
    
    // Two stages of filters for 2 channels
    std::unique_ptr<Filter> preFilter[2]; // Stage 1 (High Shelf)
    std::unique_ptr<Filter> rlbFilter[2]; // Stage 2 (High Pass)

    // Block statistics for sliding windows
    struct BlockEnergy
    {
        float sumSquaresL = 0.0f;
        float sumSquaresR = 0.0f;
        int numSamples = 0;
    };

    // Circular buffer for history
    std::vector<BlockEnergy> historyBuffer;
    int historyWriteIndex = 0;
    int historyCapacity = 0;

    // Integration state
    double integratedSumSquaresL = 0.00000000001; // Avoid init zero log error
    double integratedSumSquaresR = 0.00000000001;
    int64_t integratedTotalSamples = 0;

    // Atomic primitives for UI snapshot
    std::atomic<float> atomicM {-100.0f};
    std::atomic<float> atomicS {-100.0f};
    std::atomic<float> atomicI {-100.0f};
    std::atomic<float> atomicPeak {-100.0f};

    void updateFilters();
    float unitsToLufs (double z) const; // z = mean square
};

} // namespace AnalyzerPro::dsp
