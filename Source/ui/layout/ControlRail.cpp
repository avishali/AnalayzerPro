#include "ControlRail.h"
#include "../../control/ControlIds.h"

//==============================================================================
ControlRail::ControlRail (mdsp_ui::UiContext& ui)
    : ui_ (ui),
      releaseTimeValue_ (ui),
      navigateHeader (ui, "Navigate"),
      analyzerHeader (ui, "Analyzer"),
      displayHeader (ui, "Display"),
      metersHeader (ui, "Meters"),
      holdRow (ui, "Hold", holdButton),
      tiltRow (ui, "Tilt", tiltCombo),
      scopeModeRow (ui, "Scope Mode", scopeModeCombo),
      scopeShapeRow (ui, "Scope Shape", scopeShapeCombo),
      scopeInputRow (ui, "Scope Input", scopeInputCombo), // New
      scopePeakHoldRow (ui, "Scope Hold", scopePeakHoldButton),
      
      // Meter
      meterInputRow (ui, "Meter Input", meterInputCombo), // New
      meterPeakHoldRow (ui, "Meter Hold", meterPeakHoldButton),
      
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
    releaseTimeLabel_.setText ("Release Time", juce::dontSendNotification);
    releaseTimeLabel_.setFont (type.labelSmallFont());
    releaseTimeLabel_.setJustificationType (juce::Justification::centredLeft);
    releaseTimeLabel_.setColour (juce::Label::textColourId, theme.grey);
    releaseTimeLabel_.setTooltip ("Peak decay / release time. Drag value or use mouse wheel to change.");
    addAndMakeVisible (releaseTimeLabel_);
    addAndMakeVisible (releaseTimeValue_);
    tiltRow.attachToParent (*this);
    scopeModeRow.attachToParent (*this);
    scopeShapeRow.attachToParent (*this);
    scopeInputRow.attachToParent (*this);
    scopePeakHoldRow.attachToParent (*this);
    scopePeakHoldButton.setTooltip ("Hold stereo scope peak.");
    
    meterInputRow.attachToParent (*this);
    meterPeakHoldRow.attachToParent (*this);
    meterPeakHoldButton.setTooltip ("Hold meter peak.");
    
    showLrRow.attachToParent (*this);
    showMonoRow.attachToParent (*this);
    showLRow.attachToParent (*this);
    showRRow.attachToParent (*this);
    showMidRow.attachToParent (*this);
    showSideRow.attachToParent (*this);
    showRmsRow.attachToParent (*this);
    showLrButton.setTooltip ("Show left/right stereo trace.");
    showMonoButton.setTooltip ("Show mono sum trace.");
    showLButton.setTooltip ("Show left channel trace.");
    showRButton.setTooltip ("Show right channel trace.");
    showMidButton.setTooltip ("Show mid (L+R) trace.");
    showSideButton.setTooltip ("Show side (L-R) trace.");
    showRmsButton.setTooltip ("Show RMS trace.");

    smoothingRow.attachToParent (*this);
    weightingRow.attachToParent (*this);

    // Configure combos
    tiltCombo.addItem ("Flat", 1);
    tiltCombo.addItem ("Pink", 2);
    tiltCombo.addItem ("White", 3);
    tiltCombo.setSelectedId (1, juce::dontSendNotification);
    tiltCombo.setTooltip ("Frequency tilt: Flat, Pink, or White noise weighting.");
    
    // Smoothing Combo
    // Options: Off, 1/24, 1/12, 1/6, 1/3, 1 Octave
    smoothingCombo.addItem ("Off", 1);
    smoothingCombo.addItem ("1/24 Oct", 2);
    smoothingCombo.addItem ("1/12 Oct", 3);
    smoothingCombo.addItem ("1/6 Oct", 4);
    smoothingCombo.addItem ("1/3 Oct", 5);
    smoothingCombo.addItem ("1 Octave", 6);
    smoothingCombo.setSelectedId (4, juce::dontSendNotification); // Default 1/6 (matches plugin default)
    smoothingCombo.setTooltip ("Spectrum smoothing (averaging). 1/6 octave is a common default.");

    // Weighting Combo
    // Options: None, A-Weighting, BS.468-4
    weightingCombo.addItem ("None", 1);
    weightingCombo.addItem ("A-Wgt", 2);
    weightingCombo.addItem ("BS.468", 3);
    weightingCombo.setSelectedId (1, juce::dontSendNotification);
    weightingCombo.setTooltip ("Frequency weighting: None, A-Weighting, or BS.468-4.");

    // Scope Combos

    // Scope Combos
    scopeModeCombo.addItem ("Peak", 1);
    scopeModeCombo.addItem ("RMS", 2);
    scopeModeCombo.setSelectedId (1, juce::dontSendNotification);
    scopeModeCombo.setTooltip ("Stereo scope display: Peak or RMS.");
    scopeModeCombo.onChange = [this] { if (onScopeModeChanged) onScopeModeChanged (scopeModeCombo.getSelectedId()); };

    scopeShapeCombo.addItem ("Basic", 1);
    scopeShapeCombo.addItem ("PAZ", 2);
    scopeShapeCombo.setSelectedId (1, juce::dontSendNotification);
    scopeShapeCombo.setTooltip ("Stereo scope shape: Basic or PAZ (phase-amplitude).");
    scopeShapeCombo.onChange = [this] { if (onScopeShapeChanged) onScopeShapeChanged (scopeShapeCombo.getSelectedId()); };
    
    // Scope Input: Stereo Scope, Mid-Side
    scopeInputCombo.addItem ("M/S", 1);
    scopeInputCombo.addItem ("Stereo", 2);
    scopeInputCombo.setSelectedId (1, juce::dontSendNotification); // Default Stereo (matches Param default)
    scopeInputCombo.setTooltip ("Stereo scope input: Mid-Side (M/S) or Stereo.");

    // Meter Input: Stereo, M/S
    meterInputCombo.addItem ("Stereo", 1);
    meterInputCombo.addItem ("Mid-Side", 2);
    meterInputCombo.setSelectedId (1, juce::dontSendNotification); // Default Stereo
    meterInputCombo.setTooltip ("Meter input: Stereo or Mid-Side.");

    // Configure toggles
    holdButton.setButtonText ("Hold Peaks");
    holdButton.setTooltip ("Hold analyzer peak trace.");

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
        controlBinder->bindDraggableValueLabel (AnalyzerPro::ControlId::AnalyzerPeakDecay, releaseTimeValue_);
        controlBinder->bindCombo (AnalyzerPro::ControlId::AnalyzerTilt, tiltCombo);
        controlBinder->bindCombo (AnalyzerPro::ControlId::AnalyzerAveraging, smoothingCombo);
        controlBinder->bindCombo (AnalyzerPro::ControlId::AnalyzerWeighting, weightingCombo);
        
        controlBinder->bindCombo (AnalyzerPro::ControlId::ScopeChannelMode, scopeInputCombo);
        controlBinder->bindCombo (AnalyzerPro::ControlId::MeterChannelMode, meterInputCombo);
        controlBinder->bindToggle (AnalyzerPro::ControlId::MeterPeakHold, meterPeakHoldButton);
        controlBinder->bindToggle (AnalyzerPro::ControlId::ScopePeakHold, scopePeakHoldButton);
        
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

int ControlRail::getPreferredHeight() const noexcept
{
    const auto& m = ui_.metrics();
    // Integer-only math so content height matches resized() exactly (no phantom scrollbar).
    const int padSmall = juce::roundToInt (static_cast<double> (m.padSmall));
    const int titleHeight = juce::roundToInt (static_cast<double> (m.titleHeight));
    const int titleSecondaryGap = juce::roundToInt (static_cast<double> (m.titleSecondaryGap));
    const int secondaryHeight = juce::roundToInt (static_cast<double> (m.secondaryHeight));
    const int comboH = juce::roundToInt (static_cast<double> (m.comboH));
    const int gapSmall = juce::roundToInt (static_cast<double> (m.gapSmall));
    const int buttonSmallH = juce::roundToInt (static_cast<double> (m.buttonSmallH));
    const int sectionSpacing = juce::roundToInt (static_cast<double> (m.sectionSpacing));

    const int headerH = titleHeight + titleSecondaryGap;
    const int choiceRowH = secondaryHeight + comboH + gapSmall;
    const int toggleRowH = secondaryHeight + buttonSmallH + gapSmall;
    const int valueLabelH = secondaryHeight * 2;

    int y = padSmall;
    y += headerH + secondaryHeight + sectionSpacing;
    y += headerH + toggleRowH + secondaryHeight + valueLabelH + gapSmall + sectionSpacing;
    y += headerH + choiceRowH * 3 + sectionSpacing;
    y += toggleRowH * 7 + sectionSpacing;
    y += choiceRowH + sectionSpacing;
    y += choiceRowH + sectionSpacing;
    y += headerH + choiceRowH + toggleRowH + choiceRowH + toggleRowH + secondaryHeight;

    return y + padSmall;
}

void ControlRail::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (auto* vp = dynamic_cast<juce::Viewport*> (getParentComponent()))
        vp->mouseWheelMove (e.getEventRelativeTo (vp), wheel);
    else
        juce::Component::mouseWheelMove (e, wheel);
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
    // Same rounded constants as getPreferredHeight() so content height matches exactly.
    const int padSmall = juce::roundToInt (static_cast<double> (m.padSmall));
    const int secondaryHeight = juce::roundToInt (static_cast<double> (m.secondaryHeight));
    const int gapSmall = juce::roundToInt (static_cast<double> (m.gapSmall));
    const int buttonSmallH = juce::roundToInt (static_cast<double> (m.buttonSmallH));
    const int sectionSpacing = juce::roundToInt (static_cast<double> (m.sectionSpacing));
    const int buttonSmallW = juce::roundToInt (static_cast<double> (m.buttonSmallW));
    const int buttonW = juce::roundToInt (static_cast<double> (m.buttonW));
    const int valueLabelH = secondaryHeight * 2;

    auto bounds = getLocalBounds().reduced (padSmall);
    int y = bounds.getY();

    // Section 1: Navigate
    navigateHeader.layout (bounds, y);
    y = juce::roundToInt (static_cast<double> (y));
    placeholderLabel1.setBounds (bounds.getX(), y, bounds.getWidth(), secondaryHeight);
    y += secondaryHeight + sectionSpacing;

    // Section 2: Analyzer
    analyzerHeader.layout (bounds, y);
    y = juce::roundToInt (static_cast<double> (y));
    holdRow.layout (bounds, y);
    y = juce::roundToInt (static_cast<double> (y));
    y -= buttonSmallH + gapSmall;
    resetPeaksButton.setBounds (bounds.getX() + buttonSmallW + gapSmall, y, buttonW, buttonSmallH);
    y += buttonSmallH + gapSmall;

    releaseTimeLabel_.setBounds (bounds.getX(), y, bounds.getWidth(), secondaryHeight);
    y += secondaryHeight;
    releaseTimeValue_.setBounds (bounds.getX(), y, bounds.getWidth(), valueLabelH);
    y += valueLabelH + gapSmall + sectionSpacing;

    // Section 3: Display
    displayHeader.layout (bounds, y);
    y = juce::roundToInt (static_cast<double> (y));
    tiltRow.layout (bounds, y);
    scopeModeRow.layout (bounds, y);
    scopeShapeRow.layout (bounds, y);
    y = juce::roundToInt (static_cast<double> (y));
    y += sectionSpacing;

    showLrRow.layout (bounds, y);
    showMonoRow.layout (bounds, y);
    showLRow.layout (bounds, y);
    showRRow.layout (bounds, y);
    showMidRow.layout (bounds, y);
    showSideRow.layout (bounds, y);
    showRmsRow.layout (bounds, y);
    y = juce::roundToInt (static_cast<double> (y));
    y += sectionSpacing;

    // Smoothing
    smoothingRow.layout (bounds, y);
    y = juce::roundToInt (static_cast<double> (y));
    y += sectionSpacing;

    // Weighting
    weightingRow.layout (bounds, y);
    y = juce::roundToInt (static_cast<double> (y));
    y += sectionSpacing;

    // Section 4: Meters
    metersHeader.layout (bounds, y);
    scopeInputRow.layout (bounds, y);
    scopePeakHoldRow.layout (bounds, y);
    meterInputRow.layout (bounds, y);
    meterPeakHoldRow.layout (bounds, y);
    y = juce::roundToInt (static_cast<double> (y));
    placeholderLabel4.setBounds (bounds.getX(), y, bounds.getWidth(), secondaryHeight);
}
