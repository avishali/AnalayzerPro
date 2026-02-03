#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/**
    Control panel component hosting the Cutoff (low-pass filter) rotary knob.
    Takes a reference to APVTS and links the slider via SliderAttachment.
*/
class ControlPanel : public juce::Component
{
public:
    explicit ControlPanel (juce::AudioProcessorValueTreeState& apvts);
    ~ControlPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts_;

    juce::Slider cutoffSlider_;
    juce::Label cutoffLabel_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutoffAttachment_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControlPanel)
};
