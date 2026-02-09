#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>
#include <atomic>

class MeterComponent : public juce::Component,
                      private juce::Timer
{
public:
    enum class DisplayMode
    {
        RMS = 0,
        Peak = 1
    };

    enum class ScaleMode
    {
        FullRange = 0,  // -120 dB to +6 dB (as now)
        Top24Db   = 1,  // -18 dB to +6 dB (focus on top 24 dB)
        Top12Db   = 2   // -6 dB to +6 dB (focus on top 12 dB)
    };

    MeterComponent (mdsp_ui::UiContext& ui,
                    const std::atomic<float>* peakDb,
                    const std::atomic<float>* rmsDb,
                    const std::atomic<bool>* clipLatched,
                    juce::String labelText);
    ~MeterComponent() override;

    void setLabelText (juce::String labelText);
    void setBypassed (bool bypassed);
    
    // Direct drive (for M/S processing in parent)
    void setLevels (float peakDb, float rmsDb, bool clipped);

    void setDisplayMode (DisplayMode mode);
    DisplayMode getDisplayMode() const noexcept { return displayMode_; }

    void setScaleMode (ScaleMode mode);
    ScaleMode getScaleMode() const noexcept { return scaleMode_; }

    // Pull latest values from atomics (safe on message thread).
    void updateFromAtomics();
    
    // Explicitly reset the visual peak hold (linked reset support)
    void resetPeakHold();
    
    // Enable/disable true-freeze hold (independent from analyzer hold)
    void setHoldEnabled (bool hold);

    // Callbacks for linked behavior
    std::function<void()> onClipReset;
    std::function<void()> onPeakReset;

    void mouseDown (const juce::MouseEvent&) override;
    void paint (juce::Graphics&) override;
    void resized() override;

    void timerCallback() override;

private:
    float getScaleMinDb() const noexcept;
    float getScaleMaxDb() const noexcept;
    float clampForRenderDb (float db) const noexcept;
    float dbToNorm (float db) const noexcept;

    mdsp_ui::UiContext& ui_;

    const std::atomic<float>* peakDb_ = nullptr;
    const std::atomic<float>* rmsDb_ = nullptr;
    const std::atomic<bool>* clipLatched_ = nullptr;

    juce::String label_;
    juce::String numericTextPeak_;
    juce::String numericTextRms_;

    float cachedPeakDb_ = -120.0f;
    float cachedRmsDb_  = -120.0f;
    bool  cachedClip_   = false;
    bool  isBypassed_   = false;

    float cachedPeakNorm_ = 0.0f;
    float cachedRmsNorm_  = 0.0f;
    float maxPeakNorm_    = 0.0f;  // Normalized max peak for rendering hold marker

    // Max Holds for Numeric Display
    float maxPeakDb_ = -120.0f;
    float maxRmsDb_  = -120.0f;

    DisplayMode displayMode_ = DisplayMode::RMS;
    ScaleMode scaleMode_ = ScaleMode::FullRange;
    bool holdEnabled_ = false; // True-freeze peak hold

    // Peak hold decay: time when peak last met or exceeded max (ms)
    juce::int64 lastTimePeakAtOrAboveMax_ = 0;

    juce::Rectangle<int> labelArea_;
    juce::Rectangle<int> ledArea_;
    juce::Rectangle<int> meterArea_;
    juce::Rectangle<int> numericArea_;
};

