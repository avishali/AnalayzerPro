#include "SectionHeader.h"

namespace AnalyzerPro
{
namespace ControlPrimitives
{

SectionHeader::SectionHeader (mdsp_ui::UiContext& ui, const juce::String& text)
    : ui_ (ui)
{
    const auto& theme = ui_.theme();
    const auto& type = ui_.type();
    
    label_.setText (text, juce::dontSendNotification);
    label_.setFont (type.sectionTitleFont());
    label_.setJustificationType (juce::Justification::centredLeft);
    label_.setColour (juce::Label::textColourId, theme.lightGrey);
}

void SectionHeader::attachToParent (juce::Component& parent)
{
    parent.addAndMakeVisible (label_);
}

void SectionHeader::layout (juce::Rectangle<int> bounds, int& y)
{
    const auto& m = ui_.metrics();
    label_.setBounds (bounds.getX(), y, bounds.getWidth(), m.titleHeight);
    y += m.titleHeight + m.titleSecondaryGap;
}

} // namespace ControlPrimitives
} // namespace AnalyzerPro
