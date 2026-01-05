#include "HeaderBar.h"

//==============================================================================
HeaderBar::HeaderBar()
{
    titleLabel.setText ("AnalyzerPro", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions().withHeight (14.0f)));  // Smaller, header-style
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (titleLabel);

    // Mode label (read-only mirror - reflects current mode from right-side control)
    modeLabel.setText ("FFT", juce::dontSendNotification);
    modeLabel.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    modeLabel.setJustificationType (juce::Justification::centredLeft);
    modeLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    modeLabel.setInterceptsMouseClicks (false, false);  // Disable interaction
    addAndMakeVisible (modeLabel);

    presetButton.setButtonText ("Preset");
    presetButton.setEnabled (false);
    addAndMakeVisible (presetButton);

    saveButton.setButtonText ("Save");
    saveButton.setEnabled (false);
    addAndMakeVisible (saveButton);

    menuButton.setButtonText ("Menu");
    menuButton.setEnabled (false);
    addAndMakeVisible (menuButton);
}

HeaderBar::~HeaderBar() = default;

void HeaderBar::paint (juce::Graphics& g)
{
    // Dark background with subtle contrast
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::darkgrey.withAlpha (0.3f));
    g.fillRect (getLocalBounds().removeFromBottom (1));
}

void HeaderBar::resized()
{
    // Layout constants (PAZ-like tight header)
    static constexpr int padding = 10;
    static constexpr int buttonH = 22;
    static constexpr int buttonW = 75;
    static constexpr int gap = 8;
    
    auto area = getLocalBounds().reduced (padding);
    const int centerY = area.getCentreY();

    // Right group: Preset/Save/Menu buttons (right-justified, vertically centered)
    auto rightGroup = area.removeFromRight (buttonW * 3 + gap * 2);
    const int buttonTop = centerY - buttonH / 2;
    
    auto buttonArea = rightGroup.removeFromRight (buttonW);
    menuButton.setBounds (buttonArea.getX(), buttonTop, buttonW, buttonH);
    rightGroup.removeFromRight (gap);
    
    buttonArea = rightGroup.removeFromRight (buttonW);
    saveButton.setBounds (buttonArea.getX(), buttonTop, buttonW, buttonH);
    rightGroup.removeFromRight (gap);
    
    buttonArea = rightGroup.removeFromRight (buttonW);
    presetButton.setBounds (buttonArea.getX(), buttonTop, buttonW, buttonH);

    // Left group: Title + Mode label (vertically centered)
    const int modeLabelWidth = 60;
    const int modeLabelHeight = 20;
    const int modeLabelTop = centerY - modeLabelHeight / 2;
    
    // Mode label (read-only, shows current mode)
    modeLabel.setBounds (area.getX(), modeLabelTop, modeLabelWidth, modeLabelHeight);
    area.removeFromLeft (modeLabelWidth + gap);
    
    // Title label (remaining left space, vertically centered)
    const int titleTop = centerY - 10;  // Approximate label height / 2
    titleLabel.setBounds (area.getX(), titleTop, area.getWidth(), 20);
}

void HeaderBar::setDisplayMode (DisplayMode m)
{
    // Update read-only label to reflect current mode (mirror only, no authority)
    juce::String modeText = "FFT";
    switch (m)
    {
        case DisplayMode::FFT:
            modeText = "FFT";
            break;
        case DisplayMode::Band:
            modeText = "BANDS";
            break;
        case DisplayMode::Log:
            modeText = "LOG";
            break;
    }
    modeLabel.setText (modeText, juce::dontSendNotification);
    repaint();
}
