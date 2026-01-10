#include "ChoiceRow.h"

namespace AnalyzerPro
{
namespace ControlPrimitives
{

ChoiceRow::ChoiceRow (mdsp_ui::UiContext& ui, const juce::String& labelText, juce::ComboBox& combo)
    : ui_ (ui),
      combo_ (combo)
{
    const auto& theme = ui_.theme();
    const auto& type = ui_.type();
    
    label_.setText (labelText, juce::dontSendNotification);
    label_.setFont (type.labelSmallFont());
    label_.setJustificationType (juce::Justification::centredLeft);
    label_.setColour (juce::Label::textColourId, theme.grey);
}

void ChoiceRow::attachToParent (juce::Component& parent)
{
    parent.addAndMakeVisible (label_);
    parent.addAndMakeVisible (combo_);
}

void ChoiceRow::layout (juce::Rectangle<int> bounds, int& y)
{
    const auto& m = ui_.metrics();
    label_.setBounds (bounds.getX(), y, bounds.getWidth(), m.secondaryHeight);
    y += m.secondaryHeight;
    combo_.setBounds (bounds.getX(), y, bounds.getWidth(), m.comboH);
    y += m.comboH + m.gapSmall;
}

} // namespace ControlPrimitives
} // namespace AnalyzerPro
