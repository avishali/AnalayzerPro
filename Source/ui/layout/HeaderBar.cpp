#include "HeaderBar.h"
#include "../../control/ControlIds.h"

//==============================================================================
HeaderBar::HeaderBar (mdsp_ui::UiContext& ui)
    : ui_ (ui)
{
    const auto& theme = ui_.theme();
    const auto& type = ui_.type();

    titleLabel.setText ("AnalyzerPro", juce::dontSendNotification);
    titleLabel.setFont (type.titleFont());
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setColour (juce::Label::textColourId, theme.lightGrey);
    addAndMakeVisible (titleLabel);

    // Mode Toggles (replacing combo)
    auto initModeBtn = [&](juce::TextButton& b, const juce::String& text, int id)
    {
        b.setButtonText (text);
        b.setRadioGroupId (1001);
        b.setClickingTogglesState (true);
        b.setColour (juce::TextButton::buttonColourId, theme.background.brighter (0.05f));
        b.setColour (juce::TextButton::buttonOnColourId, theme.accent);
        b.onClick = [this, id] { if (onModeChanged) onModeChanged (id); };
        addAndMakeVisible (b);
    };
    initModeBtn (fftButton_, "FFT", 1);
    initModeBtn (bandButton_, "BAND", 2);
    initModeBtn (logButton_, "LOG", 3);
    fftButton_.setToggleState (true, juce::dontSendNotification);

    // FFT Size control
    fftSizeCombo_.addItem ("1024", 1);
    fftSizeCombo_.addItem ("2048", 2);
    fftSizeCombo_.addItem ("4096", 3);
    fftSizeCombo_.addItem ("8192", 4);
    fftSizeCombo_.setSelectedId (2, juce::dontSendNotification);
    fftSizeCombo_.setTooltip ("FFT Size");
    addAndMakeVisible (fftSizeCombo_);

    // Averaging control
    averagingCombo_.addItem ("Off", 1);
    averagingCombo_.addItem ("50 ms", 2);
    averagingCombo_.addItem ("100 ms", 3);
    averagingCombo_.addItem ("250 ms", 4);
    averagingCombo_.addItem ("500 ms", 5);
    averagingCombo_.addItem ("1 s", 6);
    averagingCombo_.setSelectedId (3, juce::dontSendNotification);
    averagingCombo_.setTooltip ("Averaging");
    addAndMakeVisible (averagingCombo_);

    presetButton.setButtonText ("Preset");
    presetButton.setEnabled (false);
    addAndMakeVisible (presetButton);

    saveButton.setButtonText ("Save");
    saveButton.setEnabled (false);
    addAndMakeVisible (saveButton);

    menuButton.setButtonText ("Menu");
    menuButton.setEnabled (false);
    addAndMakeVisible (menuButton);

    // DbRange Removed from UI
    
    peakRangeBox_.addItem ("-60 dB", 1);
    peakRangeBox_.addItem ("-90 dB", 2);
    peakRangeBox_.addItem ("-120 dB", 3);
    peakRangeBox_.setSelectedId (2, juce::dontSendNotification);
    peakRangeBox_.setTooltip ("Peak Range");
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
    // Layout constants (normalized)
    static constexpr int headerPadX = 12;
    static constexpr int headerGap = 8;
    static constexpr int controlH = 22;
    static constexpr int comboW = 112;
    static constexpr int smallBtnW = 22;
    static constexpr int modeBtnW = 60; // Button width for toggles
    
    const auto& m = ui_.metrics();
    auto area = getLocalBounds().reduced (headerPadX, 0);
    const int centerY = area.getCentreY();
    const int controlTop = centerY - controlH / 2;
    const int buttonTop = centerY - controlH / 2;

    // Right zone: Peak Range + Reset + Preset/Save/Menu buttons
    // Removed DbRange
    const int rightZoneWidth = comboW        // Peak Range only
                              + headerGap
                              + smallBtnW    // Reset button
                              + headerGap
                              + m.headerButtonW * 3  // Preset/Save/Menu
                              + headerGap * 2;
    auto rightZone = area.removeFromRight (rightZoneWidth);
    
    // Right zone layout (right-to-left)
    auto buttonArea = rightZone.removeFromRight (m.headerButtonW);
    menuButton.setBounds (buttonArea.getX(), buttonTop, m.headerButtonW, controlH);
    rightZone.removeFromRight (headerGap);
    
    buttonArea = rightZone.removeFromRight (m.headerButtonW);
    saveButton.setBounds (buttonArea.getX(), buttonTop, m.headerButtonW, controlH);
    rightZone.removeFromRight (headerGap);
    
    buttonArea = rightZone.removeFromRight (m.headerButtonW);
    presetButton.setBounds (buttonArea.getX(), buttonTop, m.headerButtonW, controlH);
    rightZone.removeFromRight (headerGap);
    
    resetPeaksButton_.setBounds (rightZone.removeFromRight (smallBtnW).getX(), controlTop, smallBtnW, smallBtnW);
    rightZone.removeFromRight (headerGap);
    
    peakRangeBox_.setBounds (rightZone.removeFromRight (comboW).getX(), controlTop, comboW, controlH);

    // Left zone: Mode Buttons (x3) + FFT Size + Averaging
    const int leftZoneWidth = (modeBtnW * 3 + headerGap) // Mode Group
                              + headerGap
                              + comboW * 2               // FFT + Avg
                              + headerGap * 2;
    auto leftZone = area.removeFromLeft (leftZoneWidth);
    
    // Mode Buttons
    fftButton_.setBounds (leftZone.removeFromLeft (modeBtnW).getX(), controlTop, modeBtnW, controlH);
    bandButton_.setBounds (leftZone.removeFromLeft (modeBtnW).getX(), controlTop, modeBtnW, controlH);
    logButton_.setBounds (leftZone.removeFromLeft (modeBtnW).getX(), controlTop, modeBtnW, controlH);
    leftZone.removeFromLeft (headerGap);
    
    fftSizeCombo_.setBounds (leftZone.removeFromLeft (comboW).getX(), controlTop, comboW, controlH);
    leftZone.removeFromLeft (headerGap);
    
    averagingCombo_.setBounds (leftZone.removeFromLeft (comboW).getX(), controlTop, comboW, controlH);

    // Center zone: Title (centered, fills remaining space)
    const int titleTop = centerY - static_cast<int> (ui_.type().titleH / 2.0f);
    const int titleWidth = juce::jmax (80, area.getWidth());
    titleLabel.setBounds (area.getX(), titleTop, titleWidth, static_cast<int> (ui_.type().titleH + 6.0f));
    titleLabel.setJustificationType (juce::Justification::centred);
}

void HeaderBar::setControlBinder (AnalyzerPro::ControlBinder& binder)
{
    controlBinder = &binder;
    
    if (controlBinder != nullptr)
    {
        // Mode binding is now Manual (via onModeChanged callback)
        // controlBinder->bindCombo (AnalyzerPro::ControlId::AnalyzerMode, modeCombo_);
        
        controlBinder->bindCombo (AnalyzerPro::ControlId::AnalyzerFftSize, fftSizeCombo_);
        controlBinder->bindCombo (AnalyzerPro::ControlId::AnalyzerAveraging, averagingCombo_);
    }
}

void HeaderBar::setPeakRangeSelectedId (int id)
{
    peakRangeBox_.setSelectedId (id, juce::dontSendNotification);
}

void HeaderBar::setMode (int modeIndex)
{
    if (modeIndex == 1) fftButton_.setToggleState (true, juce::dontSendNotification);
    else if (modeIndex == 2) bandButton_.setToggleState (true, juce::dontSendNotification);
    else if (modeIndex == 3) logButton_.setToggleState (true, juce::dontSendNotification);
}
