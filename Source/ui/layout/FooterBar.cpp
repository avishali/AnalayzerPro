#include "FooterBar.h"

//==============================================================================
FooterBar::FooterBar()
{
    statusLabel.setText ("Ready", juce::dontSendNotification);
    statusLabel.setFont (juce::Font (11.0f));
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (statusLabel);
}

FooterBar::~FooterBar() = default;

void FooterBar::paint (juce::Graphics& g)
{
    // Dark background with subtle contrast
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::darkgrey.withAlpha (0.3f));
    g.fillRect (getLocalBounds().removeFromTop (1));
}

void FooterBar::resized()
{
    auto area = getLocalBounds().reduced (10);
    statusLabel.setBounds (area);
}
