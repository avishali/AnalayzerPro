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
      scopesSection_ (ui, "Scopes"),
      tracesSection_ (ui, "Traces"),
      analysisModeSection_ (ui, "Analysis Mode"),
      fftSizeRow_ (ui, "FFT Size", fftSizeCombo_),
      holdRow (ui, "Hold", holdButton),
      tiltRow (ui, "Tilt", tiltCombo),
      scopeModeRow (ui, "Scope Mode", scopeModeCombo),
      scopeShapeRow (ui, "Scope Shape", scopeShapeCombo),
      scopeInputRow (ui, "Scope Input", scopeInputCombo),
      scopePeakHoldRow (ui, "Scope Hold", scopePeakHoldButton),
      meterInputRow (ui, "Meter Input", meterInputCombo),
      meterPeakHoldRow (ui, "Meter Hold", meterPeakHoldButton),
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

    // Collapsible sections (default collapsed)
    scopesSection_.setExpanded (false);
    tracesSection_.setExpanded (false);
    analysisModeSection_.setExpanded (false);
    scopesSection_.attachToParent (*this);
    tracesSection_.attachToParent (*this);
    analysisModeSection_.attachToParent (*this);
    scopesSection_.onToggle = [this]
    {
        if (onPreferredHeightChanged) onPreferredHeightChanged();
        resized();
    };
    tracesSection_.onToggle = [this]
    {
        if (onPreferredHeightChanged) onPreferredHeightChanged();
        resized();
    };
    analysisModeSection_.onToggle = [this]
    {
        if (onPreferredHeightChanged) onPreferredHeightChanged();
        resized();
    };

    // Analysis Mode controls (FFT/BAND/LOG + FFT size)
    auto initModeBtn = [&](juce::TextButton& b, const juce::String& text)
    {
        b.setButtonText (text);
        b.setRadioGroupId (1001);
        b.setClickingTogglesState (true);
        b.setColour (juce::TextButton::buttonColourId, theme.background.brighter (0.05f));
        b.setColour (juce::TextButton::buttonOnColourId, theme.accent);
        addAndMakeVisible (b);
    };
    initModeBtn (fftButton_,  "FFT");
    initModeBtn (bandButton_, "BAND");
    initModeBtn (logButton_,  "LOG");
    fftButton_.onClick  = [this] { if (fftButton_.getToggleState()  && onModeChanged) onModeChanged (1); };
    bandButton_.onClick = [this] { if (bandButton_.getToggleState() && onModeChanged) onModeChanged (2); };
    logButton_.onClick  = [this] { if (logButton_.getToggleState()  && onModeChanged) onModeChanged (3); };
    setSelectedModeId (1);
    fftButton_.setTooltip ("FFT spectrum mode");
    bandButton_.setTooltip ("Band spectrum mode");
    logButton_.setTooltip ("Log spectrum mode");

    fftSizeCombo_.addItem ("1024", 1);
    fftSizeCombo_.addItem ("2048", 2);
    fftSizeCombo_.addItem ("4096", 3);
    fftSizeCombo_.addItem ("8192", 4);
    fftSizeCombo_.setSelectedId (3, juce::dontSendNotification);
    fftSizeCombo_.setTooltip ("FFT size (1024-8192). Larger = better frequency resolution, more CPU.");
    addAndMakeVisible (fftSizeCombo_);
    fftSizeRow_.attachToParent (*this);

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
        controlBinder->bindCombo (AnalyzerPro::ControlId::AnalyzerFftSize, fftSizeCombo_);
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

void ControlRail::expandAnalysisModeSection()
{
    analysisModeSection_.setExpanded (true);
    if (onPreferredHeightChanged)
        onPreferredHeightChanged();
    resized();
}

void ControlRail::setMode (int modeIndex)
{
    setSelectedModeId (modeIndex);
}

int ControlRail::getPreferredHeight() const noexcept
{
    const auto& m = ui_.metrics();
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

    // Same order and conditions as resized()
    int y = padSmall;
    y += headerH + secondaryHeight + sectionSpacing;                                    // Navigate
    y += headerH + toggleRowH + secondaryHeight + valueLabelH + gapSmall + sectionSpacing; // Analyzer
    y += headerH + choiceRowH + sectionSpacing;                                          // Display (tilt only)
    y += headerH;                                                                        // Scopes header
    if (scopesSection_.isExpanded())
        y += choiceRowH * 3 + toggleRowH;
    y += sectionSpacing;
    y += headerH;                                                                        // Traces header
    if (tracesSection_.isExpanded())
        y += toggleRowH * 7;
    y += sectionSpacing;
    y += choiceRowH + sectionSpacing;                                                    // Smoothing
    y += choiceRowH + sectionSpacing;                                                    // Weighting
    y += headerH;                                                                        // Analysis Mode header
    if (analysisModeSection_.isExpanded())
        y += buttonSmallH + gapSmall + choiceRowH;
    y += sectionSpacing;
    y += headerH + choiceRowH + toggleRowH + secondaryHeight;                           // Meters (meter input + meter hold only)

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
    const int padSmall = juce::roundToInt (static_cast<double> (m.padSmall));
    const int titleHeight = juce::roundToInt (static_cast<double> (m.titleHeight));
    const int titleSecondaryGap = juce::roundToInt (static_cast<double> (m.titleSecondaryGap));
    const int secondaryHeight = juce::roundToInt (static_cast<double> (m.secondaryHeight));
    const int gapSmall = juce::roundToInt (static_cast<double> (m.gapSmall));
    const int buttonSmallH = juce::roundToInt (static_cast<double> (m.buttonSmallH));
    const int sectionSpacing = juce::roundToInt (static_cast<double> (m.sectionSpacing));
    const int buttonSmallW = juce::roundToInt (static_cast<double> (m.buttonSmallW));
    const int buttonW = juce::roundToInt (static_cast<double> (m.buttonW));
    const int comboH = juce::roundToInt (static_cast<double> (m.comboH));

    const int headerH = titleHeight + titleSecondaryGap;
    const int toggleRowH = secondaryHeight + buttonSmallH + gapSmall;
    const int choiceRowH = secondaryHeight + comboH + gapSmall;
    const int valueLabelH = secondaryHeight * 2;

    auto bounds = getLocalBounds().reduced (padSmall);
    const int x = bounds.getX();
    const int w = bounds.getWidth();
    int y = bounds.getY();

    auto addSectionHeader = [&](mdsp_ui::SectionHeader& header)
    {
        const int y0 = y;
        header.layout (bounds, y);
        y = y0 + headerH;
    };
    auto addGap = [&](int px) { y += px; };

    // Navigate
    addSectionHeader (navigateHeader);
    placeholderLabel1.setBounds (x, y, w, secondaryHeight);
    y += secondaryHeight;
    addGap (sectionSpacing);

    // Analyzer
    addSectionHeader (analyzerHeader);
    {
        const int y0 = y;
        holdRow.layout (bounds, y);
        y = y0 + toggleRowH;
    }
    y -= buttonSmallH + gapSmall;
    resetPeaksButton.setBounds (x + buttonSmallW + gapSmall, y, buttonW, buttonSmallH);
    y += buttonSmallH + gapSmall;
    releaseTimeLabel_.setBounds (x, y, w, secondaryHeight);
    y += secondaryHeight;
    releaseTimeValue_.setBounds (x, y, w, valueLabelH);
    y += valueLabelH + gapSmall;
    addGap (sectionSpacing);

    // Display (tilt only; scope controls moved to Scopes collapsible)
    addSectionHeader (displayHeader);
    {
        int y0 = y;
        tiltRow.layout (bounds, y);
        y = y0 + choiceRowH;
    }
    addGap (sectionSpacing);

    // Scopes (collapsible): header always; children only when expanded
    scopesSection_.setVisible (true);
    scopesSection_.setBounds (x, y, w, headerH);
    y += headerH;
    auto setScopeRowsVisible = [](bool visible, mdsp_ui::ChoiceRow& r)
    {
        r.getLabel().setVisible (visible);
        r.getCombo().setVisible (visible);
    };
    auto setScopeToggleVisible = [](bool visible, mdsp_ui::ToggleRow& r)
    {
        r.getLabel().setVisible (visible);
        r.getToggle().setVisible (visible);
    };
    if (scopesSection_.isExpanded())
    {
        auto placeScopeChoiceRow = [&](mdsp_ui::ChoiceRow& row)
        {
            setScopeRowsVisible (true, row);
            const int y0 = y;
            row.layout (bounds, y);
            y = y0 + choiceRowH;
        };
        auto placeScopeToggleRow = [&](mdsp_ui::ToggleRow& row)
        {
            setScopeToggleVisible (true, row);
            const int y0 = y;
            row.layout (bounds, y);
            y = y0 + toggleRowH;
        };
        placeScopeChoiceRow (scopeModeRow);
        placeScopeChoiceRow (scopeShapeRow);
        placeScopeChoiceRow (scopeInputRow);
        placeScopeToggleRow (scopePeakHoldRow);
    }
    else
    {
        setScopeRowsVisible (false, scopeModeRow);
        setScopeRowsVisible (false, scopeShapeRow);
        setScopeRowsVisible (false, scopeInputRow);
        setScopeToggleVisible (false, scopePeakHoldRow);
    }
    addGap (sectionSpacing);

    // Traces (collapsible): header always; children only when expanded
    tracesSection_.setVisible (true);
    tracesSection_.setBounds (x, y, w, headerH);
    y += headerH;
    auto setTraceRowsVisible = [](bool visible, mdsp_ui::ToggleRow& r)
    {
        r.getLabel().setVisible (visible);
        r.getToggle().setVisible (visible);
    };
    if (tracesSection_.isExpanded())
    {
        auto placeTraceRow = [&](mdsp_ui::ToggleRow& row)
        {
            setTraceRowsVisible (true, row);
            const int y0 = y;
            row.layout (bounds, y);
            y = y0 + toggleRowH;
        };
        placeTraceRow (showLrRow);
        placeTraceRow (showMonoRow);
        placeTraceRow (showLRow);
        placeTraceRow (showRRow);
        placeTraceRow (showMidRow);
        placeTraceRow (showSideRow);
        placeTraceRow (showRmsRow);
    }
    else
    {
        setTraceRowsVisible (false, showLrRow);
        setTraceRowsVisible (false, showMonoRow);
        setTraceRowsVisible (false, showLRow);
        setTraceRowsVisible (false, showRRow);
        setTraceRowsVisible (false, showMidRow);
        setTraceRowsVisible (false, showSideRow);
        setTraceRowsVisible (false, showRmsRow);
    }
    addGap (sectionSpacing);

    // Smoothing
    {
        const int y0 = y;
        smoothingRow.layout (bounds, y);
        y = y0 + choiceRowH;
    }
    addGap (sectionSpacing);

    // Weighting
    {
        const int y0 = y;
        weightingRow.layout (bounds, y);
        y = y0 + choiceRowH;
    }
    addGap (sectionSpacing);

    // Analysis Mode (collapsible): header always; children only when expanded
    analysisModeSection_.setVisible (true);
    analysisModeSection_.setBounds (x, y, w, headerH);
    y += headerH;
    if (analysisModeSection_.isExpanded())
    {
        const int modeBtnW = juce::jmin (56, w / 3 - gapSmall);
        int mx = x;
        fftButton_.setVisible (true);
        fftButton_.setBounds (mx, y, modeBtnW, buttonSmallH);
        mx += modeBtnW + gapSmall;
        bandButton_.setVisible (true);
        bandButton_.setBounds (mx, y, modeBtnW, buttonSmallH);
        mx += modeBtnW + gapSmall;
        logButton_.setVisible (true);
        logButton_.setBounds (mx, y, modeBtnW, buttonSmallH);
        y += buttonSmallH + gapSmall;
        fftSizeRow_.getLabel().setVisible (true);
        fftSizeRow_.getCombo().setVisible (true);
        {
            const int y0 = y;
            fftSizeRow_.layout (bounds, y);
            y = y0 + choiceRowH;
        }
    }
    else
    {
        fftButton_.setVisible (false);
        bandButton_.setVisible (false);
        logButton_.setVisible (false);
        fftSizeRow_.getLabel().setVisible (false);
        fftSizeRow_.getCombo().setVisible (false);
    }
    addGap (sectionSpacing);

    // Meters (scope input/hold moved to Scopes collapsible)
    addSectionHeader (metersHeader);
    {
        const int y0 = y;
        meterInputRow.layout (bounds, y);
        y = y0 + choiceRowH;
    }
    {
        const int y0 = y;
        meterPeakHoldRow.layout (bounds, y);
        y = y0 + toggleRowH;
    }
    placeholderLabel4.setBounds (x, y, w, secondaryHeight);
}
