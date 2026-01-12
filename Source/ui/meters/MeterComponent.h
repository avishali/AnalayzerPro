#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>
#include <atomic>

class MeterComponent : public juce::Component
{
public:
    enum class DisplayMode
    {
        RMS = 0,
        Peak = 1
    };

    MeterComponent (mdsp_ui::UiContext& ui,
                    const std::atomic<float>* peakDb,
                    const std::atomic<float>* rmsDb,
                    const std::atomic<bool>* clipLatched,
                    juce::String labelText);

    void setLabelText (juce::String labelText);

    void setDisplayMode (DisplayMode mode);
    DisplayMode getDisplayMode() const noexcept { return displayMode_; }

    // Pull latest values from atomics (safe on message thread).
    void updateFromAtomics();

    void mouseDown (const juce::MouseEvent&) override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static float clampForRenderDb (float db) noexcept;
    static float dbToNorm (float db) noexcept;

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

    float cachedPeakNorm_ = 0.0f;
    float cachedRmsNorm_  = 0.0f;

    // Max Holds for Numeric Display
    float maxPeakDb_ = -120.0f;
    float maxRmsDb_  = -120.0f;

    DisplayMode displayMode_ = DisplayMode::RMS;

    juce::Rectangle<int> labelArea_;
    juce::Rectangle<int> ledArea_;
    juce::Rectangle<int> meterArea_;
    juce::Rectangle<int> numericArea_;
};

