#include "CollapsibleSection.h"

//==============================================================================
CollapsibleSection::CollapsibleSection (mdsp_ui::UiContext& ui, const juce::String& title)
    : ui_ (ui),
      arrowComp_ (ui)
{
    const auto& theme = ui_.theme();
    const auto& type = ui_.type();

    addAndMakeVisible (arrowComp_);
    titleLabel_.setText (title, juce::dontSendNotification);
    titleLabel_.setFont (type.sectionTitleFont());
    titleLabel_.setJustificationType (juce::Justification::centredLeft);
    titleLabel_.setColour (juce::Label::textColourId, theme.lightGrey);
    titleLabel_.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (titleLabel_);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void CollapsibleSection::attachToParent (juce::Component& parent)
{
    parent.addAndMakeVisible (this);
}

void CollapsibleSection::layout (juce::Rectangle<int> bounds, int& y)
{
    const int rowH = getRowHeight();
    setBounds (bounds.getX(), y, bounds.getWidth(), rowH);
    y += rowH;
}

void CollapsibleSection::resized()
{
    const int rowH = getRowHeight();
    const int w = getWidth();
    arrowComp_.setBounds (0, 0, kArrowWidth, rowH);
    const int titleX = kArrowWidth + kTitleArrowGap;
    titleLabel_.setBounds (titleX, 0, w - titleX, rowH);
}

void CollapsibleSection::setExpanded (bool expanded) noexcept
{
    if (expanded_ != expanded)
    {
        expanded_ = expanded;
        arrowComp_.setExpanded (expanded_);
    }
}

int CollapsibleSection::getRowHeight() const noexcept
{
    const auto& m = ui_.metrics();
    return m.titleHeight + m.titleSecondaryGap;
}

void CollapsibleSection::paint (juce::Graphics&)
{
    // Arrow is drawn by arrowComp_; nothing else to paint in header
}

void CollapsibleSection::mouseDown (const juce::MouseEvent&)
{
    setExpanded (!expanded_);
    if (onToggle)
        onToggle();
}
