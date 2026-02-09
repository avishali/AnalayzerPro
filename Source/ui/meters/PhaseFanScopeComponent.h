#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <mdsp_ui/UiContext.h>
#include "../../audio/IStereoScopeSink.h"
#include <atomic>
#include <array>

enum class PhaseFanRenderMode { Dots, Lines, Both };

class PhaseFanScopeComponent : public juce::Component,
                               public IStereoScopeSink,
                               private juce::Timer
{
public:
    // PAZ_SCOPE: polar density plot (angle x radius bins)
    static constexpr int kAngleBins = 180;
    static constexpr int kRadiusBins = 128;
    static constexpr float kDecayFactor = 0.98f;
    static constexpr float kDensityGain = 15.0f;
    static constexpr float kRScale = 3.0f;
    static constexpr float kDensityGamma = 0.5f;
    static constexpr float kMaxDensity = 1.0f;

    static constexpr int kNumBins = kAngleBins + 1;  // for correlation/backwards compat
    static constexpr int kFifoCapacity = 8192;
    static constexpr int kMaxPairsPerFrame = 8192;

    explicit PhaseFanScopeComponent (mdsp_ui::UiContext& ui);
    ~PhaseFanScopeComponent() override;

    void setEnabled (bool enabled) noexcept;
    bool isEnabled() const noexcept { return enabled_.load (std::memory_order_relaxed); }

    void setPersistence (float p) noexcept;
    float getPersistence() const noexcept { return persistence_.load (std::memory_order_relaxed); }

    void setPointStride (int stride) noexcept;
    int getPointStride() const noexcept { return pointStride_.load (std::memory_order_relaxed); }

    void setBallistics (float attackMs, float releaseMs) noexcept;
    void setReleaseMs (float releaseMs) noexcept;

    void setMaxRadiusPx (int maxRadius) noexcept;

    void setPeakHoldEnabled (bool enabled) noexcept;
    bool isPeakHoldEnabled() const noexcept { return peakHoldEnabled_.load (std::memory_order_relaxed); }
    void setPeakHoldMs (float ms) noexcept;
    void setPeakReleaseMs (float ms) noexcept;
    void resetPeakHold() noexcept;

    void setRenderMode (PhaseFanRenderMode mode) noexcept;
    PhaseFanRenderMode getRenderMode() const noexcept { return renderMode_; }


    void pushAudioBlock (const juce::AudioBuffer<float>& buffer,
                         int startSample,
                         int numSamples) noexcept override;

    float getCorrelation() const noexcept { return correlation_.load (std::memory_order_relaxed); }

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void drainFifoAndUpdateBins();
    void computeCurrentContourFromDensity();  // weighted centroid + spatial + temporal smoothing
    float computeCorrelation (const float* lr, int numPairs) const noexcept;

    mdsp_ui::UiContext& ui_;
    juce::AbstractFifo fifo_;
    std::vector<float> fifoBuffer_;
    std::vector<float> workBuffer_;
    // PAZ_SCOPE: 2D density buffer [angleBin][radiusBin], preallocated
    using DensityRow = std::array<float, static_cast<size_t> (kRadiusBins)>;
    std::array<DensityRow, static_cast<size_t> (kAngleBins)> density_;
    std::array<DensityRow, static_cast<size_t> (kAngleBins)> peakDensity_;
    // PAZ_SCOPE_PEAK_HOLD: per-angle peak radius norm [0..1], updated in timerCallback
    std::array<float, static_cast<size_t> (kAngleBins)> peakRNorm_;
    // PAZ_SCOPE_LINES_MODE: per-angle contour state (temporal smoothing)
    std::array<float, static_cast<size_t> (kAngleBins)> lineRNormEMA_{};
    static constexpr float kLineEmaAlpha = 0.20f;
    static constexpr float kLineSpatialSmooth = 0.25f;  // 3-tap: 0.25, 0.5, 0.25
    static constexpr float kMinDrawThreshold = 0.003f;
    static constexpr float kPeakDecayFactor = 0.992f;

    PhaseFanRenderMode renderMode_ = PhaseFanRenderMode::Both;  // Dots + rays from base (Lines) so yellow traces visible
    juce::Path arcPath_;
    juce::Path linePath_;   // PAZ_SCOPE_LINES_MODE: reused for fan outline
    juce::Path peakPath_;   // PAZ_SCOPE_PEAK_HOLD: reused for peak overlay
    int maxRadiusPx_ = 0;

    std::atomic<bool> enabled_ { true };
    std::atomic<float> persistence_ { 0.85f };
    std::atomic<int> pointStride_ { 1 };
    std::atomic<float> attackMs_ { 5.0f };
    std::atomic<float> releaseMs_ { 100.0f };
    std::atomic<float> correlation_ { 0.0f };
    std::atomic<bool> peakHoldEnabled_ { true };
    std::atomic<float> peakHoldMs_ { 750.0f };
    std::atomic<float> peakReleaseMs_ { 1500.0f };
    static constexpr float kPiHalf = 1.5707963267948966f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhaseFanScopeComponent)
};
