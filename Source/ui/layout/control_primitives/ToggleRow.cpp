#include "ToggleRow.h"

namespace AnalyzerPro
{
namespace ControlPrimitives
{

ToggleRow::ToggleRow (mdsp_ui::UiContext& ui, const juce::String& labelText, juce::ToggleButton& toggle)
    : ui_ (ui),
      toggle_ (toggle)
{
    const auto& theme = ui_.theme();
    const auto& type = ui_.type();
    label_.setText (labelText, juce::dontSendNotification);
    label_.setFont (type.labelSmallFont());
    label_.setJustificationType (juce::Justification::centredLeft);
    label_.setColour (juce::Label::textColourId, theme.grey);
}

void ToggleRow::attachToParent (juce::Component& parent)
{
    parent.addAndMakeVisible (label_);
    parent.addAndMakeVisible (toggle_);
}

void ToggleRow::layout (juce::Rectangle<int> bounds, int& y)
{
    const auto& m = ui_.metrics();
    label_.setBounds (bounds.getX(), y, bounds.getWidth(), m.secondaryHeight);
    y += m.secondaryHeight;
    toggle_.setBounds (bounds.getX(), y, m.buttonSmallW, m.buttonSmallH);
    y += m.buttonSmallH + m.gapSmall;
}

} // namespace ControlPrimitives
} // namespace AnalyzerPro
