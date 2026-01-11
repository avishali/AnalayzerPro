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
    : CompactControlRow (ui),
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

    CompactControlRow::LayoutSpec spec;
    spec.rowH = m.secondaryHeight + m.sliderH + m.gapSmall;
    spec.padX = 0;
    spec.padY = 0;
    spec.gap = 0;
    spec.labelW = 0;
    spec.readoutW = 0;
    spec.minControlW = 0;
    setLayoutSpec (spec);
    setShowLabel (false);
    setShowReadout (false);

    addAndMakeVisible (label_);
    addAndMakeVisible (slider_);
}

void SliderRow::attachToParent (juce::Component& parent)
{
    parent.addAndMakeVisible (*this);
}

void SliderRow::layout (juce::Rectangle<int> bounds, int& y)
{
    const auto& m = ui_.metrics();
    setBounds (bounds.getX(), y, bounds.getWidth(), getPreferredHeight());
    y += m.secondaryHeight + m.sliderH + m.gapSmall;
}

void SliderRow::resized()
{
    CompactControlRow::resized();

    const auto& m = ui_.metrics();
    const auto content = getControlBounds();
    int y = content.getY();
    label_.setBounds (content.getX(), y, content.getWidth(), m.secondaryHeight);
    y += m.secondaryHeight;
    slider_.setBounds (content.getX(), y, content.getWidth(), m.sliderH);
}

} // namespace ControlPrimitives
} // namespace AnalyzerPro
