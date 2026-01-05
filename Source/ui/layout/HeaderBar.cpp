#include "HeaderBar.h"

//==============================================================================
HeaderBar::HeaderBar()
{
    titleLabel.setText ("AnalyzerPro", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions().withHeight (14.0f)));  // Smaller, header-style
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (titleLabel);

    modeBox.addItem ("FFT", 1);
    modeBox.addItem ("BAND", 2);
    modeBox.addItem ("LOG", 3);
    modeBox.setSelectedId (1, juce::dontSendNotification);  // Default FFT
    modeBox.onChange = [this]
    {
        const int id = modeBox.getSelectedId();
        const auto mode = (id == 2) ? DisplayMode::Band
                         : (id == 3) ? DisplayMode::Log
                                    : DisplayMode::FFT;
        if (onDisplayModeChanged)
            onDisplayModeChanged (mode);
    };
    addAndMakeVisible (modeBox);

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

    // Left group: Title + Mode selector (vertically centered)
    const int modeBoxWidth = 100;
    const int modeBoxHeight = 22;
    const int modeBoxTop = centerY - modeBoxHeight / 2;
    
    // Mode selector
    modeBox.setBounds (area.getX(), modeBoxTop, modeBoxWidth, modeBoxHeight);
    area.removeFromLeft (modeBoxWidth + gap);
    
    // Title label (remaining left space, vertically centered)
    const int titleTop = centerY - 10;  // Approximate label height / 2
    titleLabel.setBounds (area.getX(), titleTop, area.getWidth(), 20);
}

void HeaderBar::setDisplayMode (DisplayMode m)
{
    const int id = (m == DisplayMode::Band) ? 2
                   : (m == DisplayMode::Log) ? 3
                                            : 1;
    modeBox.setSelectedId (id, juce::dontSendNotification);
}
