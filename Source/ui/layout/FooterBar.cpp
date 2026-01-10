#include "FooterBar.h"

//==============================================================================
FooterBar::FooterBar (mdsp_ui::UiContext& ui)
    : ui_ (ui)
{
    const auto& theme = ui_.theme();
    const auto& type = ui_.type();

    statusLabel.setText ("Ready", juce::dontSendNotification);
    statusLabel.setFont (type.statusFont());
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setColour (juce::Label::textColourId, theme.lightGrey);
    addAndMakeVisible (statusLabel);
}

FooterBar::~FooterBar() = default;

void FooterBar::paint (juce::Graphics& g)
{
    const auto& theme = ui_.theme();

    // Dark background with subtle contrast
    g.fillAll (theme.black);
    g.setColour (theme.borderDivider);
    g.fillRect (getLocalBounds().removeFromTop (1));
}

void FooterBar::resized()
{
    const auto& m = ui_.metrics();
    auto area = getLocalBounds().reduced (m.pad);
    statusLabel.setBounds (area);
}
