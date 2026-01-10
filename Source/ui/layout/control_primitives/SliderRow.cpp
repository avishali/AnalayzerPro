#include "SliderRow.h"

namespace AnalyzerPro
{
namespace ControlPrimitives
{

SliderRow::SliderRow (mdsp_ui::UiContext& ui,
                      const juce::String& labelText,
                      juce::Slider& slider,
                      double minValue,
                      double maxValue,
                      double stepValue,
                      double defaultValue)
    : ui_ (ui),
      slider_ (slider)
{
    const auto& theme = ui_.theme();
    const auto& type = ui_.type();
    const auto& m = ui_.metrics();
    
    label_.setText (labelText, juce::dontSendNotification);
    label_.setFont (type.labelSmallFont());
    label_.setJustificationType (juce::Justification::centredLeft);
    label_.setColour (juce::Label::textColourId, theme.grey);
    
    slider_.setSliderStyle (juce::Slider::LinearHorizontal);
    slider_.setTextBoxStyle (juce::Slider::TextBoxRight, false, m.sliderTextBoxW, m.sliderTextBoxH);
    slider_.setRange (minValue, maxValue, stepValue);
    slider_.setValue (defaultValue, juce::dontSendNotification);
}

void SliderRow::attachToParent (juce::Component& parent)
{
    parent.addAndMakeVisible (label_);
    parent.addAndMakeVisible (slider_);
}

void SliderRow::layout (juce::Rectangle<int> bounds, int& y)
{
    const auto& m = ui_.metrics();
    label_.setBounds (bounds.getX(), y, bounds.getWidth(), m.secondaryHeight);
    y += m.secondaryHeight;
    slider_.setBounds (bounds.getX(), y, bounds.getWidth(), m.sliderH);
    y += m.sliderH + m.gapSmall;
}

} // namespace ControlPrimitives
} // namespace AnalyzerPro
