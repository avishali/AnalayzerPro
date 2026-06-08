#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>
#include <mdsp_ui/meters/MeterRenderState.h>
#include <mdsp_ui/meters/MeterRenderStateProvider.h>
#include "../../PluginProcessor.h"
#include "MeterComponent.h"

namespace AnalyzerPro
{
class TraceColorStore;
}

class MeterGroupComponent : public juce::Component,
                            private juce::Timer
{
public:
    using DisplayMode = mdsp_ui::meters::MeterDisplayMode;
    using ScaleMode = mdsp_ui::meters::MeterScaleMode;

    enum class GroupType
    {
        Output = 0,
        Input = 1
    };

    MeterGroupComponent (mdsp_ui::UiContext& ui,
                         AnalayzerProAudioProcessor& processor, GroupType type);
    ~MeterGroupComponent() override;

    void setChannelCount (int count);
    int getChannelCount() const noexcept { return channelCount_; }

    int getPreferredWidth() const noexcept;
    
    enum class ChannelMode { Stereo, MidSide }; // 0=Stereo, 1=MidSide
    void setChannelMode (ChannelMode mode);
    
    // Enable/disable peak hold for both meters
    void setHoldEnabled (bool hold);

    void setScaleMode (ScaleMode mode);
    ScaleMode getScaleMode() const noexcept { return scaleMode_; }
    const MeterComponent* getMeter (int idx) const noexcept;
    void setTraceColorStore (AnalyzerPro::TraceColorStore* store) noexcept;
#if defined(ANALYZERPRO_METAL_EDITOR) && ANALYZERPRO_METAL_EDITOR
    void setMetalTraceSuppressedForChromeCapture (bool shouldSuppress) noexcept;
#endif

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static void clipResetThunk (void* ctx) noexcept;
    static void peakResetThunk (void* ctx) noexcept;

    void handleClipReset() noexcept;
    void handlePeakReset() noexcept;
    void resetMidSideSmoothing() noexcept;
    void pushRenderStates();
    void setDisplayMode (DisplayMode mode);
    void timerCallback() override;

    mdsp_ui::UiContext& ui_;
    AnalayzerProAudioProcessor& processor_;
    const GroupType type_;

    int channelCount_ = 2;
    DisplayMode displayMode_ = DisplayMode::Rms;
    ScaleMode scaleMode_ = ScaleMode::FullRange;
    ChannelMode channelMode_ = ChannelMode::Stereo; // Default Stereo
    AnalyzerPro::TraceColorStore* traceColors_ = nullptr;

    juce::TextButton rmsButton_ { "RMS" };
    juce::TextButton peakButton_ { "PEAK" };
    juce::TextButton scaleFullButton_ { "F." };
    juce::TextButton scale24Button_ { "24" };
    juce::TextButton scale12Button_ { "12" };
    juce::TextButton scale6Button_ { "6" };

    std::unique_ptr<MeterComponent> meter0_;
    std::unique_ptr<MeterComponent> meter1_;
    mdsp_ui::meters::MeterRenderStateProvider provider0_;
    mdsp_ui::meters::MeterRenderStateProvider provider1_;
    mdsp_ui::meters::MeterRenderState renderState0_ {};
    mdsp_ui::meters::MeterRenderState renderState1_ {};
    int meterFeedTick_ = 0;
    bool midSideSmoothingInitialised_ = false;
    float smoothedMidPeakDb_ = -120.0f;
    float smoothedSidePeakDb_ = -120.0f;
    float smoothedMidRmsDb_ = -120.0f;
    float smoothedSideRmsDb_ = -120.0f;

    juce::Rectangle<int> headerArea_;
    juce::Rectangle<int> labelArea_;
    juce::Rectangle<int> toggleArea_;
    juce::Rectangle<int> metersArea_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeterGroupComponent)
};
