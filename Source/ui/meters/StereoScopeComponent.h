#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <mdsp_ui/UiContext.h>
#include "../../audio/IStereoScopeSink.h"
#include <atomic>

class StereoScopeComponent : public juce::Component,
                             public IStereoScopeSink,
                             private juce::Timer
{
public:
    static constexpr int kDefaultMaxViewportSize = 360;
    static constexpr int kFifoCapacity = 8192;
    static constexpr int kMaxPointsPerFrame = 2048;

    explicit StereoScopeComponent (mdsp_ui::UiContext& ui);
    ~StereoScopeComponent() override;

    void setEnabled (bool enabled) noexcept;
    bool isEnabled() const noexcept { return enabled_.load (std::memory_order_relaxed); }

    void setPersistence (float p) noexcept;
    float getPersistence() const noexcept { return persistence_.load (std::memory_order_relaxed); }

    void setPointStride (int stride) noexcept;
    int getPointStride() const noexcept { return pointStride_.load (std::memory_order_relaxed); }

    void setMaxViewportSize (int maxSize) noexcept;

    void pushAudioBlock (const juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept override;

    float getCorrelation() const noexcept { return correlation_.load (std::memory_order_relaxed); }

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void drainFifoAndRender();
    float computeCorrelation (const float* lr, int numPairs) const noexcept;

    mdsp_ui::UiContext& ui_;
    juce::AbstractFifo fifo_;
    std::vector<float> fifoBuffer_;
    std::vector<float> workBuffer_;
    juce::Path tracePath_;

    juce::Image accumImage_;
    juce::Rectangle<int> viewportRect_;
    int maxViewportSize_ = kDefaultMaxViewportSize;

    std::atomic<bool> enabled_ { true };
    std::atomic<float> persistence_ { 0.85f };
    std::atomic<int> pointStride_ { 1 };
    std::atomic<float> correlation_ { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StereoScopeComponent)
};
