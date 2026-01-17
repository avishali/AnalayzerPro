#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>
#include "../../PluginProcessor.h"
#include "MeterComponent.h"

class MeterGroupComponent : public juce::Component,
                            private juce::Timer
{
public:
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

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void setDisplayMode (MeterComponent::DisplayMode mode);
    void timerCallback() override;

    mdsp_ui::UiContext& ui_;
    AnalayzerProAudioProcessor& processor_;
    const GroupType type_;

    int channelCount_ = 2;
    MeterComponent::DisplayMode displayMode_ = MeterComponent::DisplayMode::RMS;
    ChannelMode channelMode_ = ChannelMode::Stereo; // Default Stereo

    juce::TextButton rmsButton_ { "RMS" };
    juce::TextButton peakButton_ { "PEAK" };

    std::unique_ptr<MeterComponent> meter0_;
    std::unique_ptr<MeterComponent> meter1_;

    juce::Rectangle<int> headerArea_;
    juce::Rectangle<int> labelArea_;
    juce::Rectangle<int> toggleArea_;
    juce::Rectangle<int> metersArea_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeterGroupComponent)
};

