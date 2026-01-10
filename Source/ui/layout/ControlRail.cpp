#include "ControlRail.h"
#include "../../control/ControlIds.h"

//==============================================================================
ControlRail::ControlRail (mdsp_ui::UiContext& ui)
    : ui_ (ui),
      navigateHeader (ui, "Navigate"),
      analyzerHeader (ui, "Analyzer"),
      displayHeader (ui, "Display"),
      metersHeader (ui, "Meters"),
      modeRow (ui, "Mode", modeCombo),
      fftSizeRow (ui, "FFT Size", fftSizeCombo),
      averagingRow (ui, "Averaging", averagingCombo),
      dbRangeRow (ui, "dB Range", dbRangeCombo),
      peakHoldRow (ui, "Peak Hold", peakHoldButton),
      holdRow (ui, "Hold", holdButton),
      peakDecayRow (ui, "Peak Decay", peakDecaySlider, 0.0, 10.0, 0.1, 1.0),
      displayGainRow (ui, "Display Gain", displayGainSlider, -24.0, 24.0, 0.5, 0.0),
      tiltRow (ui, "Tilt", tiltCombo)
{
    const auto& theme = ui_.theme();
    const auto& type = ui_.type();
    const auto& m = ui_.metrics();

    // Attach section headers to parent
    navigateHeader.attachToParent (*this);
    analyzerHeader.attachToParent (*this);
    displayHeader.attachToParent (*this);
    metersHeader.attachToParent (*this);

    // Attach control rows to parent
    modeRow.attachToParent (*this);
    fftSizeRow.attachToParent (*this);
    averagingRow.attachToParent (*this);
    dbRangeRow.attachToParent (*this);
    peakHoldRow.attachToParent (*this);
    holdRow.attachToParent (*this);
    peakDecayRow.attachToParent (*this);
    displayGainRow.attachToParent (*this);
    tiltRow.attachToParent (*this);

    // Configure combos (items and defaults)
    modeCombo.addItem ("FFT", 1);
    modeCombo.addItem ("BANDS", 2);
    modeCombo.addItem ("LOG", 3);
    modeCombo.setSelectedId (1, juce::dontSendNotification);  // Default: FFT
    
    fftSizeCombo.addItem ("1024", 1);
    fftSizeCombo.addItem ("2048", 2);
    fftSizeCombo.addItem ("4096", 3);
    fftSizeCombo.addItem ("8192", 4);
    fftSizeCombo.setSelectedId (2, juce::dontSendNotification);  // Default: 2048
    
    averagingCombo.addItem ("Off", 1);
    averagingCombo.addItem ("50 ms", 2);
    averagingCombo.addItem ("100 ms", 3);
    averagingCombo.addItem ("250 ms", 4);
    averagingCombo.addItem ("500 ms", 5);
    averagingCombo.addItem ("1 s", 6);
    averagingCombo.setSelectedId (3, juce::dontSendNotification);  // Default: 100ms
    
    dbRangeCombo.addItem ("-60 dB", 1);
    dbRangeCombo.addItem ("-90 dB", 2);
    dbRangeCombo.addItem ("-120 dB", 3);
    dbRangeCombo.setSelectedId (3, juce::dontSendNotification);  // Default: -120 dB
    dbRangeCombo.onChange = [this]
    {
        triggerDbRangeChanged();
    };
    
    tiltCombo.addItem ("Flat", 1);
    tiltCombo.addItem ("Pink", 2);
    tiltCombo.addItem ("White", 3);
    tiltCombo.setSelectedId (1, juce::dontSendNotification);  // Default: Flat

    // Configure toggles
    peakHoldButton.setButtonText ("On");
    peakHoldButton.setToggleState (true, juce::dontSendNotification);  // Default: enabled
    
    holdButton.setButtonText ("Hold");

    // Configure reset button
    resetPeaksButton.setTooltip ("Clear peak trace");
    resetPeaksButton.onClick = [this]
    {
        triggerResetPeaks();
    };
    addAndMakeVisible (resetPeaksButton);

    // Placeholder labels (smaller, dimmer)
    placeholderLabel1.setText ("Controls...", juce::dontSendNotification);
    placeholderLabel1.setFont (type.placeholderFont());
    placeholderLabel1.setJustificationType (juce::Justification::centredLeft);
    placeholderLabel1.setColour (juce::Label::textColourId, theme.grey);
    addAndMakeVisible (placeholderLabel1);

    placeholderLabel3.setText ("Controls...", juce::dontSendNotification);
    placeholderLabel3.setFont (type.placeholderFont());
    placeholderLabel3.setJustificationType (juce::Justification::centredLeft);
    placeholderLabel3.setColour (juce::Label::textColourId, theme.grey);
    addAndMakeVisible (placeholderLabel3);

    placeholderLabel4.setText ("Controls...", juce::dontSendNotification);
    placeholderLabel4.setFont (type.placeholderFont());
    placeholderLabel4.setJustificationType (juce::Justification::centredLeft);
    placeholderLabel4.setColour (juce::Label::textColourId, theme.grey);
    addAndMakeVisible (placeholderLabel4);
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
        controlBinder->bindToggle (AnalyzerPro::ControlId::AnalyzerPeakHold, peakHoldButton);
        controlBinder->bindToggle (AnalyzerPro::ControlId::AnalyzerHold, holdButton);
        controlBinder->bindSlider (AnalyzerPro::ControlId::AnalyzerPeakDecay, peakDecaySlider);
        controlBinder->bindSlider (AnalyzerPro::ControlId::AnalyzerDisplayGain, displayGainSlider);
        controlBinder->bindCombo (AnalyzerPro::ControlId::AnalyzerTilt, tiltCombo);
    }
}

void ControlRail::setResetPeaksCallback (std::function<void()> cb)
{
    onResetPeaks_ = std::move (cb);
}

void ControlRail::triggerResetPeaks()
{
    if (onResetPeaks_)
        onResetPeaks_();
}

void ControlRail::setDbRangeChangedCallback (std::function<void (int)> cb)
{
    onDbRangeChanged_ = std::move (cb);
}

void ControlRail::triggerDbRangeChanged()
{
    if (onDbRangeChanged_ != nullptr)
        onDbRangeChanged_ (dbRangeCombo.getSelectedId());
}

void ControlRail::paint (juce::Graphics& g)
{
    const auto& theme = ui_.theme();

    // Dark panel background with subtle contrast
    g.fillAll (theme.panel);
    g.setColour (theme.borderDivider);
    g.fillRect (getLocalBounds().removeFromLeft (1));
}

void ControlRail::resized()
{
    const auto& m = ui_.metrics();
    auto bounds = getLocalBounds().reduced (m.padSmall);  // Reduced outer padding
    int y = bounds.getY();

    // Section 1: Navigate
    navigateHeader.layout (bounds, y);
    placeholderLabel1.setBounds (bounds.getX(), y, bounds.getWidth(), m.secondaryHeight);
    y += m.secondaryHeight + m.sectionSpacing;

    // Section 2: Analyzer
    analyzerHeader.layout (bounds, y);
    
    modeRow.layout (bounds, y);
    fftSizeRow.layout (bounds, y);
    averagingRow.layout (bounds, y);
    dbRangeRow.layout (bounds, y);
    peakHoldRow.layout (bounds, y);
    
    // Hold (special case: reset button next to hold button)
    holdRow.layout (bounds, y);
    // Adjust y back to position reset button next to hold button
    y -= m.buttonSmallH + m.gapSmall;
    resetPeaksButton.setBounds (bounds.getX() + m.buttonSmallW + m.gapSmall, y, m.buttonW, m.buttonSmallH);
    y += m.buttonSmallH + m.gapSmall;
    
    peakDecayRow.layout (bounds, y);
    displayGainRow.layout (bounds, y);
    y += m.sectionSpacing;

    // Section 3: Display
    displayHeader.layout (bounds, y);
    tiltRow.layout (bounds, y);
    // TiltRow already added gapSmall, so we only need to add sectionSpacing - gapSmall for proper spacing
    y += m.sectionSpacing - m.gapSmall;

    // Section 4: Meters (placeholder)
    metersHeader.layout (bounds, y);
    placeholderLabel4.setBounds (bounds.getX(), y, bounds.getWidth(), m.secondaryHeight);
}
