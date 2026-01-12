#pragma once

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <atomic>

//==============================================================================
/**
    StereoScopeAnalyzer
    Captures stereo (L/R) samples from the audio thread into a lock-free ring buffer
    for visualization by the UI.
*/
class StereoScopeAnalyzer
{
public:
    StereoScopeAnalyzer();
    ~StereoScopeAnalyzer();

    // Audio Thread: Pushes samples into the ring buffer
    void pushSamples (const float* left, const float* right, int numSamples);

    // UI Thread: Retrieves the latest N samples
    // Returns number of samples actually retrieved
    int getSnapshot (std::vector<float>& destLeft, std::vector<float>& destRight, int numSamplesToRead);

private:
    static constexpr int kBufferSize = 16384; // Power of 2
    static constexpr int kMask = kBufferSize - 1;

    std::vector<float> bufferLeft_;
    std::vector<float> bufferRight_;
    
    juce::AbstractFifo fifo_ { kBufferSize };
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StereoScopeAnalyzer)
};
