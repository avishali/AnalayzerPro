#include "ControlRail.h"
#include "../../control/ControlIds.h"

//==============================================================================
ControlRail::ControlRail()
{
    // Section titles (smaller, bold)
    navigateLabel.setText ("Navigate", juce::dontSendNotification);
    navigateLabel.setFont (juce::Font (juce::FontOptions().withHeight (15.0f).withStyle ("bold")));
    navigateLabel.setJustificationType (juce::Justification::centredLeft);
    navigateLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (navigateLabel);

    analyzerLabel.setText ("Analyzer", juce::dontSendNotification);
    analyzerLabel.setFont (juce::Font (juce::FontOptions().withHeight (15.0f).withStyle ("bold")));
    analyzerLabel.setJustificationType (juce::Justification::centredLeft);
    analyzerLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (analyzerLabel);

    displayLabel.setText ("Display", juce::dontSendNotification);
    displayLabel.setFont (juce::Font (juce::FontOptions().withHeight (15.0f).withStyle ("bold")));
    displayLabel.setJustificationType (juce::Justification::centredLeft);
    displayLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (displayLabel);

    metersLabel.setText ("Meters", juce::dontSendNotification);
    metersLabel.setFont (juce::Font (juce::FontOptions().withHeight (15.0f).withStyle ("bold")));
    metersLabel.setJustificationType (juce::Justification::centredLeft);
    metersLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (metersLabel);

    // Placeholder labels (smaller, dimmer)
    placeholderLabel1.setText ("Controls...", juce::dontSendNotification);
    placeholderLabel1.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    placeholderLabel1.setJustificationType (juce::Justification::centredLeft);
    placeholderLabel1.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (placeholderLabel1);

    placeholderLabel3.setText ("Controls...", juce::dontSendNotification);
    placeholderLabel3.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    placeholderLabel3.setJustificationType (juce::Justification::centredLeft);
    placeholderLabel3.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (placeholderLabel3);

    placeholderLabel4.setText ("Controls...", juce::dontSendNotification);
    placeholderLabel4.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    placeholderLabel4.setJustificationType (juce::Justification::centredLeft);
    placeholderLabel4.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (placeholderLabel4);
    
    // Analyzer section controls
    modeLabel.setText ("Mode", juce::dontSendNotification);
    modeLabel.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)));
    modeLabel.setJustificationType (juce::Justification::centredLeft);
    modeLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (modeLabel);
    
    modeCombo.addItem ("FFT", 1);
    modeCombo.addItem ("BANDS", 2);
    modeCombo.addItem ("LOG", 3);
    modeCombo.setSelectedId (1, juce::dontSendNotification);  // Default: FFT
    addAndMakeVisible (modeCombo);
    
    fftSizeLabel.setText ("FFT Size", juce::dontSendNotification);
    fftSizeLabel.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)));
    fftSizeLabel.setJustificationType (juce::Justification::centredLeft);
    fftSizeLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (fftSizeLabel);
    
    fftSizeCombo.addItem ("1024", 1);
    fftSizeCombo.addItem ("2048", 2);
    fftSizeCombo.addItem ("4096", 3);
    fftSizeCombo.addItem ("8192", 4);
    fftSizeCombo.setSelectedId (2, juce::dontSendNotification);  // Default: 2048
    addAndMakeVisible (fftSizeCombo);
    
    averagingLabel.setText ("Averaging", juce::dontSendNotification);
    averagingLabel.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)));
    averagingLabel.setJustificationType (juce::Justification::centredLeft);
    averagingLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (averagingLabel);
    
    averagingCombo.addItem ("Off", 1);
    averagingCombo.addItem ("50 ms", 2);
    averagingCombo.addItem ("100 ms", 3);
    averagingCombo.addItem ("250 ms", 4);
    averagingCombo.addItem ("500 ms", 5);
    averagingCombo.addItem ("1 s", 6);
    averagingCombo.setSelectedId (3, juce::dontSendNotification);  // Default: 100ms
    addAndMakeVisible (averagingCombo);
    
    holdLabel.setText ("Hold", juce::dontSendNotification);
    holdLabel.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)));
    holdLabel.setJustificationType (juce::Justification::centredLeft);
    holdLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (holdLabel);
    
    holdButton.setButtonText ("Hold");
    addAndMakeVisible (holdButton);
    
    peakDecayLabel.setText ("Peak Decay", juce::dontSendNotification);
    peakDecayLabel.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)));
    peakDecayLabel.setJustificationType (juce::Justification::centredLeft);
    peakDecayLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (peakDecayLabel);
    
    peakDecaySlider.setSliderStyle (juce::Slider::LinearHorizontal);
    peakDecaySlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 40, 16);
    peakDecaySlider.setRange (0.0, 10.0, 0.1);
    peakDecaySlider.setValue (1.0, juce::dontSendNotification);
    addAndMakeVisible (peakDecaySlider);
    
    displayGainLabel.setText ("Display Gain", juce::dontSendNotification);
    displayGainLabel.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)));
    displayGainLabel.setJustificationType (juce::Justification::centredLeft);
    displayGainLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (displayGainLabel);
    
    displayGainSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    displayGainSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 40, 16);
    displayGainSlider.setRange (-24.0, 24.0, 0.5);
    displayGainSlider.setValue (0.0, juce::dontSendNotification);
    addAndMakeVisible (displayGainSlider);
    
    tiltLabel.setText ("Tilt", juce::dontSendNotification);
    tiltLabel.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)));
    tiltLabel.setJustificationType (juce::Justification::centredLeft);
    tiltLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (tiltLabel);
    
    tiltCombo.addItem ("Flat", 1);
    tiltCombo.addItem ("Pink", 2);
    tiltCombo.addItem ("White", 3);
    tiltCombo.setSelectedId (1, juce::dontSendNotification);  // Default: Flat
    addAndMakeVisible (tiltCombo);
}

ControlRail::~ControlRail() = default;

void ControlRail::setControlBinder (AnalyzerPro::ControlBinder& binder)
{
    controlBinder = &binder;
    
    // Bind controls now that binder is available
    if (controlBinder != nullptr)
    {
        controlBinder->bindCombo (AnalyzerPro::ControlId::AnalyzerMode, modeCombo);
        controlBinder->bindCombo (AnalyzerPro::ControlId::AnalyzerFftSize, fftSizeCombo);
        controlBinder->bindCombo (AnalyzerPro::ControlId::AnalyzerAveraging, averagingCombo);
        controlBinder->bindToggle (AnalyzerPro::ControlId::AnalyzerHold, holdButton);
        controlBinder->bindSlider (AnalyzerPro::ControlId::AnalyzerPeakDecay, peakDecaySlider);
        controlBinder->bindSlider (AnalyzerPro::ControlId::AnalyzerDisplayGain, displayGainSlider);
        controlBinder->bindCombo (AnalyzerPro::ControlId::AnalyzerTilt, tiltCombo);
    }
}

void ControlRail::paint (juce::Graphics& g)
{
    // Dark panel background with subtle contrast
    g.fillAll (juce::Colour (0xff1a1a1a));
    g.setColour (juce::Colours::darkgrey.withAlpha (0.3f));
    g.fillRect (getLocalBounds().removeFromLeft (1));
}

void ControlRail::resized()
{
    auto bounds = getLocalBounds().reduced (6);  // Outer padding
    int y = bounds.getY();

    const int titleHeight = 18;
    const int secondaryHeight = 16;
    const int titleSecondaryGap = 6;
    const int sectionSpacing = 14;

    // Section 1: Navigate
    navigateLabel.setBounds (bounds.getX(), y, bounds.getWidth(), titleHeight);
    y += titleHeight + titleSecondaryGap;
    placeholderLabel1.setBounds (bounds.getX(), y, bounds.getWidth(), secondaryHeight);
    y += secondaryHeight + sectionSpacing;

    // Section 2: Analyzer
    analyzerLabel.setBounds (bounds.getX(), y, bounds.getWidth(), titleHeight);
    y += titleHeight + titleSecondaryGap;
    
    // Mode
    modeLabel.setBounds (bounds.getX(), y, bounds.getWidth(), secondaryHeight);
    y += secondaryHeight;
    modeCombo.setBounds (bounds.getX(), y, bounds.getWidth(), 20);
    y += 20 + 4;
    
    // FFT Size
    fftSizeLabel.setBounds (bounds.getX(), y, bounds.getWidth(), secondaryHeight);
    y += secondaryHeight;
    fftSizeCombo.setBounds (bounds.getX(), y, bounds.getWidth(), 20);
    y += 20 + 4;
    
    // Averaging
    averagingLabel.setBounds (bounds.getX(), y, bounds.getWidth(), secondaryHeight);
    y += secondaryHeight;
    averagingCombo.setBounds (bounds.getX(), y, bounds.getWidth(), 20);
    y += 20 + 4;
    
    // Hold
    holdLabel.setBounds (bounds.getX(), y, bounds.getWidth(), secondaryHeight);
    y += secondaryHeight;
    holdButton.setBounds (bounds.getX(), y, 50, 18);
    y += 18 + 4;
    
    // Peak Decay
    peakDecayLabel.setBounds (bounds.getX(), y, bounds.getWidth(), secondaryHeight);
    y += secondaryHeight;
    peakDecaySlider.setBounds (bounds.getX(), y, bounds.getWidth(), 18);
    y += 18 + 4;
    
    // Display Gain
    displayGainLabel.setBounds (bounds.getX(), y, bounds.getWidth(), secondaryHeight);
    y += secondaryHeight;
    displayGainSlider.setBounds (bounds.getX(), y, bounds.getWidth(), 18);
    y += 18 + sectionSpacing;

    // Section 3: Display
    displayLabel.setBounds (bounds.getX(), y, bounds.getWidth(), titleHeight);
    y += titleHeight + titleSecondaryGap;
    placeholderLabel3.setBounds (bounds.getX(), y, bounds.getWidth(), secondaryHeight);
    y += secondaryHeight + sectionSpacing;

    // Section 4: Meters
    metersLabel.setBounds (bounds.getX(), y, bounds.getWidth(), titleHeight);
    y += titleHeight + titleSecondaryGap;
    placeholderLabel4.setBounds (bounds.getX(), y, bounds.getWidth(), secondaryHeight);
}
