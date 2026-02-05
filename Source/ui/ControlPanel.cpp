#include "ControlPanel.h"

//==============================================================================
ControlPanel::ControlPanel (juce::AudioProcessorValueTreeState& apvts)
    : apvts_ (apvts)
{
    cutoffSlider_.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    cutoffSlider_.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    cutoffSlider_.setNumDecimalPlacesToDisplay (0);
    cutoffSlider_.setTextValueSuffix (" Hz");
    cutoffSlider_.setName ("Cutoff");
    cutoffSlider_.setTooltip ("Low-pass filter cutoff (20 Hz - 20 kHz).");
    addAndMakeVisible (cutoffSlider_);

    cutoffLabel_.setText ("Cutoff", juce::dontSendNotification);
    cutoffLabel_.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (cutoffLabel_);

    cutoffAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts_, "Cutoff", cutoffSlider_);
}

ControlPanel::~ControlPanel() = default;

//==============================================================================
void ControlPanel::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

//==============================================================================
void ControlPanel::resized()
{
    juce::FlexBox fb;
    fb.flexDirection = juce::FlexBox::Direction::column;
    fb.justifyContent = juce::FlexBox::JustifyContent::center;
    fb.alignItems = juce::FlexBox::AlignItems::center;
    fb.alignContent = juce::FlexBox::AlignContent::center;

    const int knobSize = juce::jmin (getWidth(), getHeight(), 120);
    const int labelHeight = 20;

    fb.items.add (juce::FlexItem (cutoffLabel_).withWidth (static_cast<float> (knobSize)).withHeight (static_cast<float> (labelHeight)));
    fb.items.add (juce::FlexItem (cutoffSlider_).withWidth (static_cast<float> (knobSize)).withHeight (static_cast<float> (knobSize)));

    fb.performLayout (getLocalBounds().toFloat());
}
