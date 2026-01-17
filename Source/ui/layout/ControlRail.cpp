#include "ControlRail.h"
#include "../../control/ControlIds.h"

//==============================================================================
ControlRail::ControlRail (mdsp_ui::UiContext& ui)
    : ui_ (ui),
      navigateHeader (ui, "Navigate"),
      analyzerHeader (ui, "Analyzer"),
      displayHeader (ui, "Display"),
      metersHeader (ui, "Meters"),
      holdRow (ui, "Hold", holdButton),
      peakDecayRow (ui, "Peak Decay", peakDecaySlider, 0.0, 10.0, 0.1, 1.0),
      tiltRow (ui, "Tilt", tiltCombo),
      scopeModeRow (ui, "Scope Mode", scopeModeCombo),
      scopeShapeRow (ui, "Scope Shape", scopeShapeCombo),
      scopeInputRow (ui, "Scope Input", scopeInputCombo), // New
      
      // Meter
      meterInputRow (ui, "Meter Input", meterInputCombo), // New
      
      // Trace Toggles
      showLrRow (ui, "Show Stereo", showLrButton),
      showMonoRow (ui, "Show Mono", showMonoButton),
      showLRow (ui, "Show Left", showLButton),
      showRRow (ui, "Show Right", showRButton),
      showMidRow (ui, "Show Mid", showMidButton),
      showSideRow (ui, "Show Side", showSideButton),
      showRmsRow (ui, "Show RMS", showRmsButton),
      
      smoothingRow (ui, "Smoothing", smoothingCombo),
      weightingRow (ui, "Weighting", weightingCombo)
{
    const auto& theme = ui_.theme();
    const auto& type = ui_.type();

    // Attach section headers to parent
    navigateHeader.attachToParent (*this);
    analyzerHeader.attachToParent (*this);
    displayHeader.attachToParent (*this);
    metersHeader.attachToParent (*this);

    // Attach control rows to parent
    holdRow.attachToParent (*this);
    peakDecayRow.attachToParent (*this);
    tiltRow.attachToParent (*this);
    scopeModeRow.attachToParent (*this);
    scopeShapeRow.attachToParent (*this);
    scopeInputRow.attachToParent (*this);
    
    meterInputRow.attachToParent (*this);
    
    showLrRow.attachToParent (*this);
    showMonoRow.attachToParent (*this);
    showLRow.attachToParent (*this);
    showRRow.attachToParent (*this);
    showMidRow.attachToParent (*this);
    showSideRow.attachToParent (*this);
    showRmsRow.attachToParent (*this);

    smoothingRow.attachToParent (*this);
    weightingRow.attachToParent (*this);

    // Configure combos
    tiltCombo.addItem ("Flat", 1);
    tiltCombo.addItem ("Pink", 2);
    tiltCombo.addItem ("White", 3);
    tiltCombo.setSelectedId (1, juce::dontSendNotification);
    
    // Smoothing Combo
    // Options: Off, 1/24, 1/12, 1/6, 1/3, 1 Octave
    smoothingCombo.addItem ("Off", 1);
    smoothingCombo.addItem ("1/24 Oct", 2);
    smoothingCombo.addItem ("1/12 Oct", 3);
    smoothingCombo.addItem ("1/6 Oct", 4);
    smoothingCombo.addItem ("1/3 Oct", 5);
    smoothingCombo.addItem ("1 Octave", 6);
    smoothingCombo.setSelectedId (4, juce::dontSendNotification); // Default 1/6 (matches plugin default)

    // Weighting Combo
    // Options: None, A-Weighting, BS.468-4
    weightingCombo.addItem ("None", 1);
    weightingCombo.addItem ("A-Wgt", 2);
    weightingCombo.addItem ("BS.468", 3);
    weightingCombo.setSelectedId (1, juce::dontSendNotification);

    // Scope Combos

    // Scope Combos
    scopeModeCombo.addItem ("Peak", 1);
    scopeModeCombo.addItem ("RMS", 2);
    scopeModeCombo.setSelectedId (1, juce::dontSendNotification);
    scopeModeCombo.onChange = [this] { if (onScopeModeChanged) onScopeModeChanged (scopeModeCombo.getSelectedId()); };

    scopeShapeCombo.addItem ("Basic", 1);
    scopeShapeCombo.addItem ("PAZ", 2);
    scopeShapeCombo.setSelectedId (2, juce::dontSendNotification);
    scopeShapeCombo.onChange = [this] { if (onScopeShapeChanged) onScopeShapeChanged (scopeShapeCombo.getSelectedId()); };
    
    // Scope Input: Stereo, M/S
    scopeInputCombo.addItem ("Stereo", 1);
    scopeInputCombo.addItem ("Mid-Side", 2);
    scopeInputCombo.setSelectedId (2, juce::dontSendNotification); // Default M/S

    // Meter Input: Stereo, M/S
    meterInputCombo.addItem ("Stereo", 1);
    meterInputCombo.addItem ("Mid-Side", 2);
    meterInputCombo.setSelectedId (1, juce::dontSendNotification); // Default Stereo

    // Configure toggles
    holdButton.setButtonText ("Hold Peaks");

    // Configure reset button
    resetPeaksButton.setTooltip ("Clear peak trace");
    resetPeaksButton.onClick = [this]
    {
        triggerResetPeaks();
    };
    addAndMakeVisible (resetPeaksButton);

    // Placeholder labels
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
    
    // Bind controls
    if (controlBinder != nullptr)
    {
        controlBinder->bindToggle (AnalyzerPro::ControlId::AnalyzerHoldPeaks, holdButton);
        controlBinder->bindSlider (AnalyzerPro::ControlId::AnalyzerPeakDecay, peakDecaySlider);
        controlBinder->bindCombo (AnalyzerPro::ControlId::AnalyzerTilt, tiltCombo);
        controlBinder->bindCombo (AnalyzerPro::ControlId::AnalyzerAveraging, smoothingCombo);
        controlBinder->bindCombo (AnalyzerPro::ControlId::AnalyzerWeighting, weightingCombo);
        
        controlBinder->bindCombo (AnalyzerPro::ControlId::ScopeChannelMode, scopeInputCombo);
        controlBinder->bindCombo (AnalyzerPro::ControlId::MeterChannelMode, meterInputCombo);
        
        controlBinder->bindToggle (AnalyzerPro::ControlId::TraceShowLR, showLrButton);
        controlBinder->bindToggle (AnalyzerPro::ControlId::TraceShowMono, showMonoButton);
        controlBinder->bindToggle (AnalyzerPro::ControlId::TraceShowL, showLButton);
        controlBinder->bindToggle (AnalyzerPro::ControlId::TraceShowR, showRButton);
        controlBinder->bindToggle (AnalyzerPro::ControlId::TraceShowMid, showMidButton);
        controlBinder->bindToggle (AnalyzerPro::ControlId::TraceShowSide, showSideButton);
        controlBinder->bindToggle (AnalyzerPro::ControlId::TraceShowRMS, showRmsButton);
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
    auto bounds = getLocalBounds().reduced (m.padSmall);
    int y = bounds.getY();

    // Section 1: Navigate
    navigateHeader.layout (bounds, y);
    placeholderLabel1.setBounds (bounds.getX(), y, bounds.getWidth(), m.secondaryHeight);
    y += m.secondaryHeight + m.sectionSpacing;

    // Section 2: Analyzer
    analyzerHeader.layout (bounds, y);
    
    // Hold + Reset
    holdRow.layout (bounds, y);
    y -= m.buttonSmallH + m.gapSmall;
    resetPeaksButton.setBounds (bounds.getX() + m.buttonSmallW + m.gapSmall, y, m.buttonW, m.buttonSmallH);
    y += m.buttonSmallH + m.gapSmall;
    
    peakDecayRow.layout (bounds, y);
    y += m.sectionSpacing;

    // Section 3: Display
    displayHeader.layout (bounds, y);
    tiltRow.layout (bounds, y);
    // Scope Controls
    scopeModeRow.layout (bounds, y);
    scopeShapeRow.layout (bounds, y);
    scopeShapeRow.layout (bounds, y);
    y += m.sectionSpacing;
    
    // Traces
    showLrRow.layout (bounds, y);
    showMonoRow.layout (bounds, y);
    showLRow.layout (bounds, y);
    showRRow.layout (bounds, y);
    showMidRow.layout (bounds, y);
    showSideRow.layout (bounds, y);
    showRmsRow.layout (bounds, y);
    y += m.sectionSpacing;
    
    // Smoothing
    smoothingRow.layout (bounds, y);
    y += m.sectionSpacing;

    // Section 4: Meters
    metersHeader.layout (bounds, y);
    placeholderLabel4.setBounds (bounds.getX(), y, bounds.getWidth(), m.secondaryHeight);
}
