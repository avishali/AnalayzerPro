#include "ChoiceRow.h"

namespace AnalyzerPro
{
namespace ControlPrimitives
{

ChoiceRow::ChoiceRow (mdsp_ui::UiContext& ui, const juce::String& labelText, juce::ComboBox& combo)
    : CompactControlRow (ui),
      combo_ (combo)
{
    const auto& theme = ui_.theme();
    const auto& type = ui_.type();
    const auto& m = ui_.metrics();
    
    label_.setText (labelText, juce::dontSendNotification);
    label_.setFont (type.labelSmallFont());
    label_.setJustificationType (juce::Justification::centredLeft);
    label_.setColour (juce::Label::textColourId, theme.grey);

    CompactControlRow::LayoutSpec spec;
    spec.rowH = m.secondaryHeight + m.comboH + m.gapSmall;
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
    addAndMakeVisible (combo_);
}

void ChoiceRow::attachToParent (juce::Component& parent)
{
    parent.addAndMakeVisible (*this);
}

void ChoiceRow::layout (juce::Rectangle<int> bounds, int& y)
{
    const auto& m = ui_.metrics();
    setBounds (bounds.getX(), y, bounds.getWidth(), getPreferredHeight());
    y += m.secondaryHeight + m.comboH + m.gapSmall;
}

void ChoiceRow::resized()
{
    CompactControlRow::resized();

    const auto& m = ui_.metrics();
    const auto content = getControlBounds();
    int y = content.getY();
    label_.setBounds (content.getX(), y, content.getWidth(), m.secondaryHeight);
    y += m.secondaryHeight;
    combo_.setBounds (content.getX(), y, content.getWidth(), m.comboH);
}

} // namespace ControlPrimitives
} // namespace AnalyzerPro
