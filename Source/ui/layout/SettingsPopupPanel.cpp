#include "SettingsPopupPanel.h"
#include "../../control/AnalyzerProParamIdMap.h"
#include "../../control/ControlIds.h"

using namespace AnalyzerPro;
using AnalyzerPro::control::makeDefaultParamIdMap;

SettingsPopupPanel::SettingsPopupPanel (Section section,
                                        mdsp_ui::UiContext& ui,
                                        juce::AudioProcessorValueTreeState* apvts)
    : section_ (section),
      ui_ (ui),
      releaseTimeValue_ (ui),
      fftSizeRow_  (ui, "FFT Size",   fftSizeCombo_),
      tiltRow_     (ui, "Tilt",       tiltCombo_),
      smoothingRow_(ui, "Smoothing",  smoothingCombo_),
      weightingRow_(ui, "Weighting",  weightingCombo_),
      holdRow_     (ui, "Hold Peaks", holdBtn_),
      scopeModeRow_  (ui, "Scope Mode",  scopeModeCombo_),
      scopeShapeRow_ (ui, "Scope Shape", scopeShapeCombo_),
      scopeInputRow_ (ui, "Scope Input", scopeInputCombo_),
      scopeHoldRow_  (ui, "Scope Hold",  scopeHoldBtn_),
      meterInputRow_ (ui, "Meter Input", meterInputCombo_),
      meterHoldRow_  (ui, "Meter Hold",  meterHoldBtn_),
      lrRow_   (ui, "Show Stereo", lrBtn_),
      monoRow_ (ui, "Show Mono",   monoBtn_),
      lRow_    (ui, "Show Left",   lBtn_),
      rRow_    (ui, "Show Right",  rBtn_),
      midRow_  (ui, "Show Mid",    midBtn_),
      sideRow_ (ui, "Show Side",   sideBtn_),
      rmsRow_  (ui, "Show RMS",    rmsBtn_)
{
    binder_ = std::make_unique<AnalyzerPro::ControlBinder> (apvts, makeDefaultParamIdMap());

    switch (section_)
    {
        case Section::Spectrum: initSpectrum (apvts); break;
        case Section::Scopes:   initScopes   (apvts); break;
        case Section::Meters:   initMeters   (apvts); break;
        case Section::Traces:   initTraces   (apvts); break;
    }
}

SettingsPopupPanel::~SettingsPopupPanel() = default;

// ─────────────────────────────────────────────────────────────────────────────
// Section initialisation
// ─────────────────────────────────────────────────────────────────────────────

void SettingsPopupPanel::initSpectrum (juce::AudioProcessorValueTreeState* /*apvts*/)
{
    const auto& theme = ui_.theme();

    // Mode buttons
    auto initMode = [&] (juce::TextButton& b, const juce::String& text)
    {
        b.setButtonText (text);
        b.setRadioGroupId (3001);
        b.setClickingTogglesState (true);
        b.setColour (juce::TextButton::buttonColourId, theme.background.brighter (0.05f));
        b.setColour (juce::TextButton::buttonOnColourId, theme.panel.brighter (0.28f));
        addAndMakeVisible (b);
    };
    initMode (fftBtn_,  "FFT");
    initMode (bandBtn_, "BAND");
    initMode (logBtn_,  "LOG");
    fftBtn_ .setTooltip ("FFT spectrum mode");
    bandBtn_.setTooltip ("Band spectrum mode");
    logBtn_ .setTooltip ("Log spectrum mode");
    fftBtn_.setToggleState (true, juce::dontSendNotification); // default

    fftBtn_ .onClick = [this] { if (fftBtn_ .getToggleState() && onModeChanged) onModeChanged (1); };
    bandBtn_.onClick = [this] { if (bandBtn_.getToggleState() && onModeChanged) onModeChanged (2); };
    logBtn_ .onClick = [this] { if (logBtn_ .getToggleState() && onModeChanged) onModeChanged (3); };

    // FFT Size
    fftSizeCombo_.addItem ("1024", 1);
    fftSizeCombo_.addItem ("2048", 2);
    fftSizeCombo_.addItem ("4096", 3);
    fftSizeCombo_.addItem ("8192", 4);
    fftSizeCombo_.setTooltip ("FFT size. Larger = better frequency resolution.");
    fftSizeRow_.attachToParent (*this);
    binder_->bindCombo (ControlId::AnalyzerFftSize, fftSizeCombo_);

    // Hold + Reset
    holdBtn_.setButtonText ("Hold Peaks");
    holdBtn_.setTooltip ("Hold analyzer peak trace.");
    holdRow_.attachToParent (*this);
    binder_->bindToggle (ControlId::AnalyzerHoldPeaks, holdBtn_);

    resetBtn_.setButtonText ("Reset");
    resetBtn_.setTooltip ("Clear peak trace");
    resetBtn_.onClick = [this] { if (onResetPeaks) onResetPeaks(); };
    addAndMakeVisible (resetBtn_);

    // Release time
    const auto& type = ui_.type();
    releaseTimeLabel_.setText ("Release Time", juce::dontSendNotification);
    releaseTimeLabel_.setFont (type.labelSmallFont());
    releaseTimeLabel_.setJustificationType (juce::Justification::centredLeft);
    releaseTimeLabel_.setColour (juce::Label::textColourId, theme.grey);
    releaseTimeLabel_.setTooltip ("Peak decay / release time.");
    addAndMakeVisible (releaseTimeLabel_);
    addAndMakeVisible (releaseTimeValue_);
    binder_->bindDraggableValueLabel (ControlId::AnalyzerPeakDecay, releaseTimeValue_);

    // Tilt / Smoothing / Weighting
    tiltCombo_.addItem ("Flat",  1);
    tiltCombo_.addItem ("Pink",  2);
    tiltCombo_.addItem ("White", 3);
    tiltCombo_.setTooltip ("Frequency tilt weighting.");
    tiltRow_.attachToParent (*this);
    binder_->bindCombo (ControlId::AnalyzerTilt, tiltCombo_);

    smoothingCombo_.addItem ("Off",      1);
    smoothingCombo_.addItem ("1/24 Oct", 2);
    smoothingCombo_.addItem ("1/12 Oct", 3);
    smoothingCombo_.addItem ("1/6 Oct",  4);
    smoothingCombo_.addItem ("1/3 Oct",  5);
    smoothingCombo_.addItem ("1 Octave", 6);
    smoothingCombo_.setTooltip ("Spectrum smoothing.");
    smoothingRow_.attachToParent (*this);
    binder_->bindCombo (ControlId::AnalyzerAveraging, smoothingCombo_);

    weightingCombo_.addItem ("None",   1);
    weightingCombo_.addItem ("A-Wgt",  2);
    weightingCombo_.addItem ("BS.468", 3);
    weightingCombo_.setTooltip ("Frequency weighting.");
    weightingRow_.attachToParent (*this);
    binder_->bindCombo (ControlId::AnalyzerWeighting, weightingCombo_);
}

void SettingsPopupPanel::initScopes (juce::AudioProcessorValueTreeState* /*apvts*/)
{
    scopeModeCombo_.addItem ("Peak", 1);
    scopeModeCombo_.addItem ("RMS",  2);
    scopeModeCombo_.setTooltip ("Stereo scope display mode.");
    scopeModeCombo_.onChange = [this] { if (onScopeModeChanged) onScopeModeChanged (scopeModeCombo_.getSelectedId()); };
    scopeModeRow_.attachToParent (*this);

    scopeShapeCombo_.addItem ("Basic", 1);
    scopeShapeCombo_.addItem ("PAZ",   2);
    scopeShapeCombo_.setTooltip ("Stereo scope shape.");
    scopeShapeCombo_.onChange = [this] { if (onScopeShapeChanged) onScopeShapeChanged (scopeShapeCombo_.getSelectedId()); };
    scopeShapeRow_.attachToParent (*this);

    scopeInputCombo_.addItem ("M/S",    1);
    scopeInputCombo_.addItem ("Stereo", 2);
    scopeInputCombo_.setTooltip ("Scope input routing.");
    scopeInputRow_.attachToParent (*this);
    binder_->bindCombo (ControlId::ScopeChannelMode, scopeInputCombo_);

    scopeHoldBtn_.setButtonText ("Scope Hold");
    scopeHoldBtn_.setTooltip ("Hold stereo scope peak.");
    scopeHoldRow_.attachToParent (*this);
    binder_->bindToggle (ControlId::ScopePeakHold, scopeHoldBtn_);
}

void SettingsPopupPanel::initMeters (juce::AudioProcessorValueTreeState* /*apvts*/)
{
    meterInputCombo_.addItem ("Stereo",   1);
    meterInputCombo_.addItem ("Mid-Side", 2);
    meterInputCombo_.setTooltip ("Meter input routing.");
    meterInputRow_.attachToParent (*this);
    binder_->bindCombo (ControlId::MeterChannelMode, meterInputCombo_);

    meterHoldBtn_.setButtonText ("Meter Hold");
    meterHoldBtn_.setTooltip ("Hold meter peak.");
    meterHoldRow_.attachToParent (*this);
    binder_->bindToggle (ControlId::MeterPeakHold, meterHoldBtn_);
}

void SettingsPopupPanel::initTraces (juce::AudioProcessorValueTreeState* /*apvts*/)
{
    const auto& theme = ui_.theme();

    // Each trace gets a colour matching its series colour in the analyzer
    struct TraceInfo { mdsp_ui::ToggleRow& row; juce::ToggleButton& btn; ControlId id;
                       const char* tip; juce::Colour col; };
    TraceInfo traces[] = {
        { lrRow_,   lrBtn_,   ControlId::TraceShowLR,   "Show left/right stereo trace.", theme.seriesStereo },
        { monoRow_, monoBtn_, ControlId::TraceShowMono,  "Show mono sum trace.",          theme.seriesMono   },
        { lRow_,    lBtn_,    ControlId::TraceShowL,     "Show left channel trace.",      theme.seriesLeft   },
        { rRow_,    rBtn_,    ControlId::TraceShowR,     "Show right channel trace.",     theme.seriesRight  },
        { midRow_,  midBtn_,  ControlId::TraceShowMid,   "Show mid (L+R) trace.",         theme.seriesMid    },
        { sideRow_, sideBtn_, ControlId::TraceShowSide,  "Show side (L-R) trace.",        theme.seriesSide   },
        { rmsRow_,  rmsBtn_,  ControlId::TraceShowRMS,   "Show RMS trace.",               theme.seriesRms    },
    };

    for (auto& t : traces)
    {
        t.btn.setTooltip (t.tip);
        // Colour the toggle tick to match the trace series colour
        t.btn.setColour (juce::ToggleButton::tickColourId,         t.col);
        t.btn.setColour (juce::ToggleButton::tickDisabledColourId, t.col.withAlpha (0.4f));
        t.row.attachToParent (tracesContainer_);
        binder_->bindToggle (t.id, t.btn);
    }

    // Scrollable container — no visible scrollbar (trackpad/wheel scrolls)
    tracesViewport_.setViewedComponent (&tracesContainer_, false);
    tracesViewport_.setScrollBarsShown (true, false);
    tracesViewport_.setScrollBarThickness (4);
    addAndMakeVisible (tracesViewport_);
}

// ─────────────────────────────────────────────────────────────────────────────
// State setters for non-APVTS controls
// ─────────────────────────────────────────────────────────────────────────────

void SettingsPopupPanel::setCurrentMode (int modeId)
{
    fftBtn_ .setToggleState (modeId == 1, juce::dontSendNotification);
    bandBtn_.setToggleState (modeId == 2, juce::dontSendNotification);
    logBtn_ .setToggleState (modeId == 3, juce::dontSendNotification);
}

void SettingsPopupPanel::setCurrentScopeMode (int id)
{
    scopeModeCombo_.setSelectedId (id, juce::dontSendNotification);
}

void SettingsPopupPanel::setCurrentScopeShape (int id)
{
    scopeShapeCombo_.setSelectedId (id, juce::dontSendNotification);
}

// ─────────────────────────────────────────────────────────────────────────────
// Preferred height
// ─────────────────────────────────────────────────────────────────────────────

int SettingsPopupPanel::getPreferredHeight() const
{
    const auto& m = ui_.metrics();
    const int pad     = juce::roundToInt (static_cast<double> (m.padSmall));
    const int choiceH = juce::roundToInt (static_cast<double> (m.secondaryHeight + m.comboH + m.gapSmall));
    const int toggleH = juce::roundToInt (static_cast<double> (m.secondaryHeight + m.buttonSmallH + m.gapSmall));
    const int btnRowH = juce::roundToInt (static_cast<double> (m.buttonSmallH)) + 6;
    const int gap     = juce::roundToInt (static_cast<double> (m.gapSmall));

    int h = pad * 2;
    switch (section_)
    {
        case Section::Spectrum:
            h += btnRowH + gap;     // mode buttons
            h += choiceH;           // FFT size
            h += toggleH;           // hold
            h += btnRowH + gap;     // reset button
            h += toggleH;           // release time label + value
            h += choiceH;           // tilt
            h += choiceH;           // smoothing
            h += choiceH;           // weighting
            break;
        case Section::Scopes:
            h += choiceH * 3 + toggleH;
            break;
        case Section::Meters:
            h += choiceH + toggleH;
            break;
        case Section::Traces:
            // Seven trace toggles (Stereo, Mono, L, R, Mid, Side, RMS); height must fit all rows in the CallOutBox.
            h += toggleH * 7;
            break;
    }
    return h;
}

// ─────────────────────────────────────────────────────────────────────────────
// Paint
// ─────────────────────────────────────────────────────────────────────────────

void SettingsPopupPanel::paint (juce::Graphics& g)
{
    g.fillAll (ui_.theme().panel);
}

// ─────────────────────────────────────────────────────────────────────────────
// Layout
// ─────────────────────────────────────────────────────────────────────────────

void SettingsPopupPanel::resized()
{
    const auto& m = ui_.metrics();
    const int pad      = juce::roundToInt (static_cast<double> (m.padSmall));
    const int choiceH  = juce::roundToInt (static_cast<double> (m.secondaryHeight + m.comboH + m.gapSmall));
    const int toggleH  = juce::roundToInt (static_cast<double> (m.secondaryHeight + m.buttonSmallH + m.gapSmall));
    const int btnH     = juce::roundToInt (static_cast<double> (m.buttonSmallH));
    const int gap      = juce::roundToInt (static_cast<double> (m.gapSmall));
    const int secH     = juce::roundToInt (static_cast<double> (m.secondaryHeight));
    const int valH     = secH * 2;
    const int btnW     = juce::roundToInt (static_cast<double> (m.buttonW));
    const int btnSmW   = juce::roundToInt (static_cast<double> (m.buttonSmallW));

    auto bounds = getLocalBounds().reduced (pad);
    const int x = bounds.getX();
    const int w = bounds.getWidth();
    int y = bounds.getY();

    auto placeChoice = [&] (mdsp_ui::ChoiceRow& row)
    {
        row.layout (bounds, y);
        y += choiceH;
    };
    auto placeToggle = [&] (mdsp_ui::ToggleRow& row)
    {
        row.layout (bounds, y);
        y += toggleH;
    };

    switch (section_)
    {
        case Section::Spectrum:
        {
            // Mode buttons row
            const int modeBtnW = juce::jmin (62, (w - gap * 2) / 3);
            int mx = x;
            fftBtn_ .setBounds (mx, y, modeBtnW, btnH); mx += modeBtnW + gap;
            bandBtn_.setBounds (mx, y, modeBtnW, btnH); mx += modeBtnW + gap;
            logBtn_ .setBounds (mx, y, modeBtnW, btnH);
            y += btnH + gap;

            placeChoice (fftSizeRow_);

            // Hold row with Reset button inline
            holdRow_.layout (bounds, y);
            resetBtn_.setBounds (x + btnSmW + gap, y + secH, btnW, btnH);
            y += toggleH;

            // Release time
            releaseTimeLabel_.setBounds (x, y, w, secH);
            y += secH;
            releaseTimeValue_.setBounds (x, y, w, valH);
            y += valH + gap;

            placeChoice (tiltRow_);
            placeChoice (smoothingRow_);
            placeChoice (weightingRow_);
            break;
        }
        case Section::Scopes:
            placeChoice (scopeModeRow_);
            placeChoice (scopeShapeRow_);
            placeChoice (scopeInputRow_);
            placeToggle (scopeHoldRow_);
            break;
        case Section::Meters:
            placeChoice (meterInputRow_);
            placeToggle (meterHoldRow_);
            break;
        case Section::Traces:
        {
            // Viewport fills remaining space; container holds all 7 rows
            const int totalContentH = toggleH * 7;
            tracesViewport_.setBounds (bounds.withY (y).withHeight (bounds.getBottom() - y));
            tracesContainer_.setBounds (0, 0, bounds.getWidth(), totalContentH);

            // Layout rows inside the container (y relative to container, starting at 0)
            int cy = 0;
            auto containerBounds = juce::Rectangle<int> (0, 0, bounds.getWidth(), totalContentH);
            auto placeInContainer = [&] (mdsp_ui::ToggleRow& row)
            {
                row.layout (containerBounds, cy);
                cy += toggleH;
            };
            placeInContainer (lrRow_);
            placeInContainer (monoRow_);
            placeInContainer (lRow_);
            placeInContainer (rRow_);
            placeInContainer (midRow_);
            placeInContainer (sideRow_);
            placeInContainer (rmsRow_);
            break;
        }
    }
}
