#include "HeaderBar.h"

//==============================================================================
HeaderBar::HeaderBar (mdsp_ui::UiContext& ui)
    : ui_ (ui)
{
    const auto& theme = ui_.theme();
    const auto& type = ui_.type();

    titleLabel.setText ("AnalyzerPro", juce::dontSendNotification);
    titleLabel.setFont (type.titleFont());
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setColour (juce::Label::textColourId, theme.lightGrey);
    addAndMakeVisible (titleLabel);

    // Mode label (read-only mirror - reflects current mode from right-side control)
    modeLabel.setText ("FFT", juce::dontSendNotification);
    modeLabel.setFont (type.sectionTitleFont());
    modeLabel.setJustificationType (juce::Justification::centredLeft);
    modeLabel.setColour (juce::Label::textColourId, theme.lightGrey);
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

    dbRangeLabel_.setText ("dB Range", juce::dontSendNotification);
    dbRangeLabel_.setFont (type.labelFont());
    dbRangeLabel_.setJustificationType (juce::Justification::centredLeft);
    dbRangeLabel_.setColour (juce::Label::textColourId, theme.lightGrey);
    addAndMakeVisible (dbRangeLabel_);

    dbRangeBox_.addItem ("-60 dB", 1);
    dbRangeBox_.addItem ("-90 dB", 2);
    dbRangeBox_.addItem ("-120 dB", 3);
    dbRangeBox_.setSelectedId (3, juce::dontSendNotification);
    dbRangeBox_.onChange = [this]
    {
        if (onDbRangeChanged)
            onDbRangeChanged (dbRangeBox_.getSelectedId());
    };
    addAndMakeVisible (dbRangeBox_);

    peakRangeLabel_.setText ("Peak", juce::dontSendNotification);
    peakRangeLabel_.setFont (type.labelFont());
    peakRangeLabel_.setJustificationType (juce::Justification::centredLeft);
    peakRangeLabel_.setColour (juce::Label::textColourId, theme.lightGrey);
    addAndMakeVisible (peakRangeLabel_);

    peakRangeBox_.addItem ("-60 dB", 1);
    peakRangeBox_.addItem ("-90 dB", 2);
    peakRangeBox_.addItem ("-120 dB", 3);
    peakRangeBox_.setSelectedId (2, juce::dontSendNotification); // Default: -90 dB
    peakRangeBox_.onChange = [this]
    {
        if (onPeakRangeChanged)
            onPeakRangeChanged (peakRangeBox_.getSelectedId());
    };
    addAndMakeVisible (peakRangeBox_);

    resetPeaksButton_.setButtonText (juce::String (juce::CharPointer_UTF8 ("⟲")));
    resetPeaksButton_.setTooltip (juce::String (juce::CharPointer_UTF8 ("Reset Peaks (⌥⌘R)")));
    resetPeaksButton_.setColour (juce::TextButton::buttonColourId, theme.transparentBlack);
    resetPeaksButton_.setColour (juce::TextButton::buttonOnColourId, theme.transparentBlack);
    resetPeaksButton_.setColour (juce::TextButton::textColourOffId, theme.lightGrey);
    resetPeaksButton_.setColour (juce::TextButton::textColourOnId, theme.lightGrey);
    resetPeaksButton_.onClick = [this]
    {
        if (onResetPeaks)
            onResetPeaks();
    };
    addAndMakeVisible (resetPeaksButton_);
}

HeaderBar::~HeaderBar() = default;

void HeaderBar::paint (juce::Graphics& g)
{
    const auto& theme = ui_.theme();

    // Dark background with subtle contrast
    g.fillAll (theme.black);
    g.setColour (theme.borderDivider);
    g.fillRect (getLocalBounds().removeFromBottom (1));
}

void HeaderBar::resized()
{
    const auto& m = ui_.metrics();
    
    auto area = getLocalBounds().reduced (m.pad);
    const int centerY = area.getCentreY();

    // Right group: dB Range + Peak Range + Preset/Save/Menu buttons (right-justified, vertically centered)
    auto rightGroup = area.removeFromRight (m.headerButtonW * 3 + m.gap * 2
                                            + m.headerDbRangeLabelW + m.headerDbRangeBoxW + m.gap * 2
                                            + m.headerPeakRangeLabelW + m.headerPeakRangeBoxW + m.gap * 2
                                            + m.headerResetButtonSize + m.gap);
    const int buttonTop = centerY - m.headerButtonH / 2;
    
    const int dbRangeTop = centerY - m.headerDbRangeH / 2;
    auto dbArea = rightGroup.removeFromLeft (m.headerDbRangeLabelW);
    dbRangeLabel_.setBounds (dbArea.getX(), dbRangeTop, m.headerDbRangeLabelW, m.headerDbRangeH);
    rightGroup.removeFromLeft (m.gap);
    dbArea = rightGroup.removeFromLeft (m.headerDbRangeBoxW);
    dbRangeBox_.setBounds (dbArea.getX(), dbRangeTop, m.headerDbRangeBoxW, m.headerDbRangeH);
    rightGroup.removeFromLeft (m.gap);

    dbArea = rightGroup.removeFromLeft (m.headerPeakRangeLabelW);
    peakRangeLabel_.setBounds (dbArea.getX(), dbRangeTop, m.headerPeakRangeLabelW, m.headerDbRangeH);
    rightGroup.removeFromLeft (m.gap);
    dbArea = rightGroup.removeFromLeft (m.headerPeakRangeBoxW);
    peakRangeBox_.setBounds (dbArea.getX(), dbRangeTop, m.headerPeakRangeBoxW, m.headerDbRangeH);
    rightGroup.removeFromLeft (m.gap);

    dbArea = rightGroup.removeFromLeft (m.headerResetButtonSize);
    resetPeaksButton_.setBounds (dbArea.getX(), dbRangeTop, m.headerResetButtonSize, m.headerResetButtonSize);
    rightGroup.removeFromLeft (m.gap);

    auto buttonArea = rightGroup.removeFromRight (m.headerButtonW);
    menuButton.setBounds (buttonArea.getX(), buttonTop, m.headerButtonW, m.headerButtonH);
    rightGroup.removeFromRight (m.gap);
    
    buttonArea = rightGroup.removeFromRight (m.headerButtonW);
    saveButton.setBounds (buttonArea.getX(), buttonTop, m.headerButtonW, m.headerButtonH);
    rightGroup.removeFromRight (m.gap);
    
    buttonArea = rightGroup.removeFromRight (m.headerButtonW);
    presetButton.setBounds (buttonArea.getX(), buttonTop, m.headerButtonW, m.headerButtonH);

    // Left group: Title + Mode label (vertically centered)
    const int modeLabelTop = centerY - m.headerModeLabelH / 2;
    
    // Mode label (read-only, shows current mode)
    modeLabel.setBounds (area.getX(), modeLabelTop, m.headerModeLabelW, m.headerModeLabelH);
    area.removeFromLeft (m.headerModeLabelW + m.gap);
    
    // Title label (remaining left space, vertically centered)
    const int titleTop = centerY - static_cast<int> (ui_.type().titleH / 2.0f);
    titleLabel.setBounds (area.getX(), titleTop, area.getWidth(), static_cast<int> (ui_.type().titleH + 6.0f));
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

void HeaderBar::setDbRangeSelectedId (int id)
{
    dbRangeBox_.setSelectedId (id, juce::dontSendNotification);
}

void HeaderBar::setPeakRangeSelectedId (int id)
{
    peakRangeBox_.setSelectedId (id, juce::dontSendNotification);
}
