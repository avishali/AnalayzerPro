#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

struct IStereoScopeSink
{
    virtual void pushAudioBlock (const juce::AudioBuffer<float>& buffer,
                                 int startSample,
                                 int numSamples) noexcept = 0;

protected:
    ~IStereoScopeSink() = default;
};
