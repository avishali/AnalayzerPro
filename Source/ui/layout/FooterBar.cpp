#include "FooterBar.h"
#include "../../control/ControlBinder.h"
#include "../../control/ControlIds.h"

//==============================================================================
FooterBar::FooterBar (mdsp_ui::UiContext& ui)
    : ui_ (ui),
      releaseTimeValue_ (ui)
{
    const auto& theme = ui_.theme();
    const auto& type  = ui_.type();

    statusLabel.setText ("Ready", juce::dontSendNotification);
    statusLabel.setFont (type.statusFont());
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setColour (juce::Label::textColourId, theme.lightGrey);
    addAndMakeVisible (statusLabel);

    // ── Always-visible: Peak Hold ────────────────────────────────────────────
    holdLabel_.setText ("Hold", juce::dontSendNotification);
    holdLabel_.setFont (type.labelSmallFont());
    holdLabel_.setJustificationType (juce::Justification::centredRight);
    holdLabel_.setColour (juce::Label::textColourId, theme.grey);
    holdLabel_.setTooltip ("Hold analyzer peak trace");
    addAndMakeVisible (holdLabel_);

    holdBtn_.setTooltip ("Hold analyzer peak trace");
    holdBtn_.setClickingTogglesState (true);
    addAndMakeVisible (holdBtn_);

    // ── Always-visible: Release Time ─────────────────────────────────────────
    releaseLabel_.setText ("Release", juce::dontSendNotification);
    releaseLabel_.setFont (type.labelSmallFont());
    releaseLabel_.setJustificationType (juce::Justification::centredRight);
    releaseLabel_.setColour (juce::Label::textColourId, theme.grey);
    releaseLabel_.setTooltip ("Peak decay / release time");
    addAndMakeVisible (releaseLabel_);

    addAndMakeVisible (releaseTimeValue_);
}

FooterBar::~FooterBar() = default;

void FooterBar::setControlBinder (AnalyzerPro::ControlBinder& binder)
{
    binder.bindToggle (AnalyzerPro::ControlId::AnalyzerHoldPeaks, holdBtn_);
    binder.bindDraggableValueLabel (AnalyzerPro::ControlId::AnalyzerPeakDecay, releaseTimeValue_);
}

void FooterBar::paint (juce::Graphics& g)
{
    const auto& theme = ui_.theme();
    g.fillAll (theme.black);
    g.setColour (theme.borderDivider);
    g.fillRect (getLocalBounds().removeFromTop (1));
}

void FooterBar::resized()
{
    const auto& m = ui_.metrics();
    auto area = getLocalBounds().reduced (m.pad, 2);

    const int btnH    = juce::jmin (area.getHeight(), m.buttonSmallH);
    const int valW    = 52;  // release time value width
    const int lblW    = 44;  // label width
    const int btnW    = 32;  // toggle pill width
    const int gap     = 6;

    // Right side: Release label + value | Hold label + button
    auto right = area;

    // Release time: value + label
    auto releaseVal = right.removeFromRight (valW).withSizeKeepingCentre (valW, btnH);
    right.removeFromRight (gap);
    auto releaseLbl = right.removeFromRight (lblW).withSizeKeepingCentre (lblW, btnH);
    right.removeFromRight (gap * 2);

    // Hold: button + label
    auto holdToggle = right.removeFromRight (btnW).withSizeKeepingCentre (btnW, btnH);
    right.removeFromRight (gap);
    auto holdLbl    = right.removeFromRight (lblW).withSizeKeepingCentre (lblW, btnH);
    right.removeFromRight (gap * 2);

    holdLabel_      .setBounds (holdLbl);
    holdBtn_        .setBounds (holdToggle);
    releaseLabel_   .setBounds (releaseLbl);
    releaseTimeValue_.setBounds (releaseVal);

    // Status label takes the remaining left area
    statusLabel.setBounds (area.withRight (right.getRight()));
}
