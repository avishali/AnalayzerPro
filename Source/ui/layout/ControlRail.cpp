#include "ControlRail.h"
#include "../../control/ControlIds.h"

//==============================================================================
ControlRail::ControlRail (mdsp_ui::UiContext& ui)
    : ui_ (ui),
      releaseTimeValue_ (ui),
      holdDecayValue_ (ui),
      scopeReleaseValue_ (ui),
      spectrumHeader (ui, "Spectrum"),
      scopesHeader (ui, "Scopes"),
      metersHeader (ui, "Meters"),
      tracesHeader (ui, "Traces"),
      fftSizeRow_ (ui, "FFT Size", fftSizeCombo_),
      detailRow_ (ui, "Detail", detailCombo_),
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
      showPeakRow (ui, "Show Peak", showPeakButton),
      showRmsRow (ui, "Show RMS", showRmsButton),
      smoothingRow (ui, "Smoothing", smoothingCombo),
      weightingRow (ui, "Weighting", weightingCombo)
{
    const auto& theme = ui_.theme();
    const auto& type = ui_.type();

    // Attach section headers to parent
    spectrumHeader.attachToParent (*this);
    scopesHeader.attachToParent (*this);
    metersHeader.attachToParent (*this);
    tracesHeader.attachToParent (*this);

    // Analysis Mode controls (FFT/BAND/LOG + FFT size)
    auto initModeBtn = [&](juce::TextButton& b, const juce::String& text)
    {
        b.setButtonText (text);
        b.setRadioGroupId (1001);
        b.setClickingTogglesState (true);
        b.setColour (juce::TextButton::buttonColourId, theme.background.brighter (0.05f));
        b.setColour (juce::TextButton::buttonOnColourId, theme.panel.brighter (0.28f));
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
    fftSizeCombo_.addItem ("16384", 5);
    fftSizeCombo_.setSelectedId (3, juce::dontSendNotification);
    fftSizeCombo_.setTooltip ("Analysis resolution (CPU). Larger = finer low-freq detail.");
    addAndMakeVisible (fftSizeCombo_);
    fftSizeRow_.attachToParent (*this);

    detailCombo_.addItem ("Low", 1);
    detailCombo_.addItem ("Medium", 2);
    detailCombo_.addItem ("High", 3);
    detailCombo_.setSelectedId (2, juce::dontSendNotification);
    detailCombo_.setTooltip ("Display trace density. Higher = smoother, more detailed curve.");
    addAndMakeVisible (detailCombo_);
    detailRow_.attachToParent (*this);

    // Attach control rows to parent
    holdRow.attachToParent (*this);
    releaseTimeLabel_.setText ("Release Time", juce::dontSendNotification);
    releaseTimeLabel_.setFont (type.labelSmallFont());
    releaseTimeLabel_.setJustificationType (juce::Justification::centredLeft);
    releaseTimeLabel_.setColour (juce::Label::textColourId, theme.grey);
    releaseTimeLabel_.setTooltip ("Peak decay / release time. Drag value or use mouse wheel to change.");
    addAndMakeVisible (releaseTimeLabel_);
    addAndMakeVisible (releaseTimeValue_);
    holdDecayLabel_.setText ("Hold Decay", juce::dontSendNotification);
    holdDecayLabel_.setFont (type.labelSmallFont());
    holdDecayLabel_.setJustificationType (juce::Justification::centredLeft);
    holdDecayLabel_.setColour (juce::Label::textColourId, theme.grey);
    holdDecayLabel_.setTooltip ("Peak-hold decay time when Hold is off. Drag value or use mouse wheel to change.");
    addAndMakeVisible (holdDecayLabel_);
    addAndMakeVisible (holdDecayValue_);
    tiltRow.attachToParent (*this);
    scopeModeRow.attachToParent (*this);
    scopeShapeRow.attachToParent (*this);
    scopeInputRow.attachToParent (*this);
    scopePeakHoldRow.attachToParent (*this);
    scopePeakHoldButton.setTooltip ("Hold phase-fan peak outline.");

    // Phase-fan release/decay: draggable ms value (like the spectrum Release Time).
    scopeReleaseLabel_.setText ("Release", juce::dontSendNotification);
    scopeReleaseLabel_.setFont (type.labelSmallFont());
    scopeReleaseLabel_.setJustificationType (juce::Justification::centredLeft);
    scopeReleaseLabel_.setColour (juce::Label::textColourId, theme.grey);
    scopeReleaseLabel_.setTooltip ("Phase-fan release / decay time. Drag value or use mouse wheel.");
    addChildComponent (scopeReleaseLabel_);
    scopeReleaseValue_.setManualValue (400.0, 50.0, 5000.0);
    scopeReleaseValue_.onManualChanged = [this] (double ms)
    {
        if (onScopeReleaseChanged)
            onScopeReleaseChanged (static_cast<float> (ms));
    };
    addChildComponent (scopeReleaseValue_);
    
    meterInputRow.attachToParent (*this);
    meterPeakHoldRow.attachToParent (*this);
    meterPeakHoldButton.setTooltip ("Hold meter peak.");
    
    showLrRow.attachToParent (*this);
    showMonoRow.attachToParent (*this);
    showLRow.attachToParent (*this);
    showRRow.attachToParent (*this);
    showMidRow.attachToParent (*this);
    showSideRow.attachToParent (*this);
    showPeakRow.attachToParent (*this);
    showRmsRow.attachToParent (*this);
    showLrButton.setTooltip ("Show left/right stereo trace.");
    showMonoButton.setTooltip ("Show mono sum trace.");
    showLButton.setTooltip ("Show left channel trace.");
    showRButton.setTooltip ("Show right channel trace.");
    showMidButton.setTooltip ("Show mid (L+R) trace.");
    showSideButton.setTooltip ("Show side (L-R) trace.");
    showPeakButton.setTooltip ("Show peak trace.");
    showRmsButton.setTooltip ("Show RMS trace.");

    // Trace colour swatches (Traces module). Hidden until a store is attached + module active.
    for (size_t i = 0; i < traceSwatches_.size(); ++i)
    {
        const auto id = static_cast<AnalyzerPro::TraceId> (i);
        traceSwatches_[i].setTooltip ("Click to choose this trace's colour");
        traceSwatches_[i].onClick = [this, id] { openTraceColourPicker (id); };
        addChildComponent (traceSwatches_[i]);
    }
    resetColorsButton_.setTooltip ("Reset all trace colours to defaults");
    resetColorsButton_.onClick = [this]
    {
        if (traceColors_ != nullptr) { traceColors_->resetToDefaults(); refreshTraceSwatches(); }
    };
    saveColorsDefaultButton_.setTooltip ("Save current trace colours as the default for new instances");
    saveColorsDefaultButton_.onClick = [this]
    {
        if (traceColors_ != nullptr) traceColors_->saveAsUserDefault();
    };
    addChildComponent (resetColorsButton_);
    addChildComponent (saveColorsDefaultButton_);

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
    scopeModeCombo.setSelectedId (2, juce::dontSendNotification); // default RMS (matches provider default)
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
        controlBinder->bindDraggableValueLabel (AnalyzerPro::ControlId::PeakHoldDecayTime, holdDecayValue_);
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
        controlBinder->bindToggle (AnalyzerPro::ControlId::TraceShowPeak, showPeakButton);
        controlBinder->bindToggle (AnalyzerPro::ControlId::TraceShowRMS, showRmsButton);
        controlBinder->bindCombo (AnalyzerPro::ControlId::AnalyzerFftSize, fftSizeCombo_);
        controlBinder->bindCombo (AnalyzerPro::ControlId::AnalyzerDetail, detailCombo_);
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

void ControlRail::setTraceColorStore (AnalyzerPro::TraceColorStore* store)
{
    traceColors_ = store;
    refreshTraceSwatches();
    resized();
}

void ControlRail::refreshTraceSwatches()
{
    if (traceColors_ == nullptr)
        return;
    for (size_t i = 0; i < traceSwatches_.size(); ++i)
        traceSwatches_[i].setSwatchColour (traceColors_->get (static_cast<AnalyzerPro::TraceId> (i)));
}

void ControlRail::setTraceColorUiVisible (bool visible)
{
    for (auto& sw : traceSwatches_)
        sw.setVisible (visible);
    resetColorsButton_.setVisible (visible);
    saveColorsDefaultButton_.setVisible (visible);
}

void ControlRail::openTraceColourPicker (AnalyzerPro::TraceId id)
{
    if (traceColors_ == nullptr)
        return;

    auto selector = std::make_unique<AnalyzerPro::LiveColourSelector> (
        juce::ColourSelector::showColourAtTop
        | juce::ColourSelector::showColourspace
        | juce::ColourSelector::showSliders);
    selector->setCurrentColour (traceColors_->get (id), juce::dontSendNotification);
    selector->setSize (240, 300);
    selector->onColour = [this, id] (juce::Colour c)
    {
        if (traceColors_ != nullptr)
            traceColors_->set (id, c);
        refreshTraceSwatches();
    };

    juce::CallOutBox::launchAsynchronously (std::move (selector),
                                            traceSwatches_[static_cast<size_t> (id)].getScreenBounds(),
                                            this);
}

void ControlRail::expandAnalysisModeSection()
{
    setActiveModule (ActiveModule::Spectrum);
}

void ControlRail::setMode (int modeIndex)
{
    setSelectedModeId (modeIndex);
}

void ControlRail::setActiveModule (ActiveModule module)
{
    if (activeModule_ == module)
        return;

    activeModule_ = module;
    if (onPreferredHeightChanged)
        onPreferredHeightChanged();
    resized();
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

    int y = padSmall;
    y += headerH;

    switch (activeModule_)
    {
        case ActiveModule::Spectrum:
            y += buttonSmallH + gapSmall;       // FFT / BAND / LOG
            y += choiceRowH * 5;                // FFT size, detail, smoothing, weighting, tilt
            y += toggleRowH;                    // Hold + Reset
            y += secondaryHeight + valueLabelH + gapSmall;
            break;
        case ActiveModule::Scopes:
            y += choiceRowH + (secondaryHeight + valueLabelH + gapSmall) + toggleRowH;
            break;
        case ActiveModule::Meters:
            y += choiceRowH + toggleRowH;
            break;
        case ActiveModule::Traces:
            y += toggleRowH * 7;
            if (traceColors_ != nullptr)
                y += toggleRowH * 2; // Peak colour row + reset/save buttons
            break;
    }

    return y + sectionSpacing + padSmall;
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

    // Semi-transparent panel so the analyzer grid/labels show through the overlay rail.
    // (Metal content is clipped out of the rail rect, so this reveals the chrome behind it.)
    g.fillAll (theme.panel.withAlpha (0.60f));
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

    auto setChoiceVisible = [] (mdsp_ui::ChoiceRow& row, bool visible)
    {
        row.getLabel().setVisible (visible);
        row.getCombo().setVisible (visible);
    };
    auto setToggleVisible = [] (mdsp_ui::ToggleRow& row, bool visible)
    {
        row.getLabel().setVisible (visible);
        row.getToggle().setVisible (visible);
    };
    auto setHeaderVisible = [] (mdsp_ui::SectionHeader& header, bool visible)
    {
        header.getLabel().setVisible (visible);
    };

    setHeaderVisible (spectrumHeader, false);
    setHeaderVisible (scopesHeader, false);
    setHeaderVisible (metersHeader, false);
    setHeaderVisible (tracesHeader, false);

    fftButton_.setVisible (false);
    bandButton_.setVisible (false);
    logButton_.setVisible (false);
    resetPeaksButton.setVisible (false);
    releaseTimeLabel_.setVisible (false);
    releaseTimeValue_.setVisible (false);
    holdDecayLabel_.setVisible (false);
    holdDecayValue_.setVisible (false);

    setChoiceVisible (fftSizeRow_, false);
    setChoiceVisible (detailRow_, false);
    setChoiceVisible (smoothingRow, false);
    setChoiceVisible (weightingRow, false);
    setChoiceVisible (tiltRow, false);
    setChoiceVisible (scopeModeRow, false);
    setChoiceVisible (scopeShapeRow, false);
    setChoiceVisible (scopeInputRow, false);
    scopeReleaseLabel_.setVisible (false);
    scopeReleaseValue_.setVisible (false);
    setChoiceVisible (meterInputRow, false);

    setToggleVisible (holdRow, false);
    setToggleVisible (scopePeakHoldRow, false);
    setToggleVisible (meterPeakHoldRow, false);
    setToggleVisible (showLrRow, false);
    setToggleVisible (showMonoRow, false);
    setToggleVisible (showLRow, false);
    setToggleVisible (showRRow, false);
    setToggleVisible (showMidRow, false);
    setToggleVisible (showSideRow, false);
    setToggleVisible (showPeakRow, false);
    setToggleVisible (showRmsRow, false);
    setTraceColorUiVisible (false);

    auto addSectionHeader = [&](mdsp_ui::SectionHeader& header)
    {
        setHeaderVisible (header, true);
        const int y0 = y;
        header.layout (bounds, y);
        y = y0 + headerH;
    };
    auto placeChoiceRow = [&] (mdsp_ui::ChoiceRow& row)
    {
        setChoiceVisible (row, true);
        const int y0 = y;
        row.layout (bounds, y);
        y = y0 + choiceRowH;
    };
    auto placeToggleRow = [&] (mdsp_ui::ToggleRow& row)
    {
        setToggleVisible (row, true);
        const int y0 = y;
        row.layout (bounds, y);
        y = y0 + toggleRowH;
    };

    switch (activeModule_)
    {
        case ActiveModule::Spectrum:
        {
            addSectionHeader (spectrumHeader);

            const int modeBtnW = juce::jmax (1, juce::jmin (56, (w - gapSmall * 2) / 3));
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

            placeChoiceRow (fftSizeRow_);
            placeChoiceRow (detailRow_);
            placeChoiceRow (smoothingRow);
            placeChoiceRow (weightingRow);
            placeChoiceRow (tiltRow);

            setToggleVisible (holdRow, true);
            const int holdY = y;
            holdRow.layout (bounds, y);
            resetPeaksButton.setVisible (true);
            resetPeaksButton.setBounds (x + buttonSmallW + gapSmall,
                                        holdY + secondaryHeight,
                                        buttonW,
                                        buttonSmallH);
            y = holdY + toggleRowH;

            releaseTimeLabel_.setVisible (true);
            releaseTimeLabel_.setBounds (x, y, w, secondaryHeight);
            y += secondaryHeight;
            releaseTimeValue_.setVisible (true);
            releaseTimeValue_.setBounds (x, y, w, valueLabelH);
            y += valueLabelH + gapSmall;

            holdDecayLabel_.setVisible (true);
            holdDecayLabel_.setBounds (x, y, w, secondaryHeight);
            y += secondaryHeight;
            holdDecayValue_.setVisible (true);
            holdDecayValue_.setBounds (x, y, w, valueLabelH);
            y += valueLabelH + gapSmall;
            break;
        }
        case ActiveModule::Scopes:
        {
            addSectionHeader (scopesHeader);
            placeChoiceRow (scopeModeRow);    // Peak / RMS

            // Release / decay — draggable ms value (label above value, like the spectrum).
            scopeReleaseLabel_.setVisible (true);
            scopeReleaseLabel_.setBounds (x, y, w, secondaryHeight);
            y += secondaryHeight;
            scopeReleaseValue_.setVisible (true);
            scopeReleaseValue_.setBounds (x, y, w, valueLabelH);
            y += valueLabelH + gapSmall;

            placeToggleRow (scopePeakHoldRow); // Peak hold
            break;
        }
        case ActiveModule::Meters:
        {
            addSectionHeader (metersHeader);
            placeChoiceRow (meterInputRow);
            placeToggleRow (meterPeakHoldRow);
            break;
        }
        case ActiveModule::Traces:
        {
            addSectionHeader (tracesHeader);

            const bool hasColors = (traceColors_ != nullptr);
            const int swatchSize = juce::jmin (18, buttonSmallH);
            const int swatchRight = bounds.getRight() - swatchSize;
            const int rowTrim = hasColors ? (swatchSize + gapSmall) : 0;

            // A trace toggle row, with its colour swatch on the right edge.
            auto placeTraceRow = [&] (mdsp_ui::ToggleRow& row, AnalyzerPro::TraceId id)
            {
                setToggleVisible (row, true);
                const int y0 = y;
                row.layout (bounds.withTrimmedRight (rowTrim), y);
                if (hasColors)
                {
                    auto& sw = traceSwatches_[static_cast<size_t> (id)];
                    sw.setVisible (true);
                    sw.setBounds (swatchRight, y0 + (toggleRowH - swatchSize) / 2, swatchSize, swatchSize);
                }
                y = y0 + toggleRowH;
            };

            placeTraceRow (showLrRow,   AnalyzerPro::TraceId::LR);
            placeTraceRow (showMonoRow, AnalyzerPro::TraceId::Mono);
            placeTraceRow (showLRow,    AnalyzerPro::TraceId::L);
            placeTraceRow (showRRow,    AnalyzerPro::TraceId::R);
            placeTraceRow (showMidRow,  AnalyzerPro::TraceId::Mid);
            placeTraceRow (showSideRow, AnalyzerPro::TraceId::Side);
            placeTraceRow (showPeakRow, AnalyzerPro::TraceId::Peak);
            placeTraceRow (showRmsRow,  AnalyzerPro::TraceId::Rms);

            if (hasColors)
            {
                // Reset / Save-default actions.
                y += gapSmall;
                const int halfW = (w - gapSmall) / 2;
                resetColorsButton_.setVisible (true);
                resetColorsButton_.setBounds (x, y, halfW, buttonSmallH);
                saveColorsDefaultButton_.setVisible (true);
                saveColorsDefaultButton_.setBounds (x + halfW + gapSmall, y, w - halfW - gapSmall, buttonSmallH);
                y += buttonSmallH + gapSmall;
            }
            break;
        }
    }
}
