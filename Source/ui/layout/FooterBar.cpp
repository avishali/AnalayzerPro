#include "FooterBar.h"
#include "../../control/ControlBinder.h"
#include "../../control/ControlIds.h"

//==============================================================================
FooterBar::FooterBar (mdsp_ui::UiContext& ui)
    : ui_ (ui),
      releaseTimeValue_ (ui),
      holdDecayValue_ (ui)
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

    // ── Always-visible: Peak-hold decay (Hold off) ───────────────────────────
    holdDecayLabel_.setText ("Hold Decay", juce::dontSendNotification);
    holdDecayLabel_.setFont (type.labelSmallFont());
    holdDecayLabel_.setJustificationType (juce::Justification::centredRight);
    holdDecayLabel_.setColour (juce::Label::textColourId, theme.grey);
    holdDecayLabel_.setTooltip ("Peak-hold decay time when Hold is off");
    addAndMakeVisible (holdDecayLabel_);

    addAndMakeVisible (holdDecayValue_);

#if ANALYZERPRO_DEV_LOOK_PANEL
    devLookButton_.setTooltip ("Open DEV Look tuning panel");
    devLookButton_.setColour (juce::TextButton::buttonColourId, theme.black.withAlpha (0.35f));
    devLookButton_.setColour (juce::TextButton::buttonOnColourId, theme.black.withAlpha (0.50f));
    devLookButton_.setColour (juce::TextButton::textColourOffId, theme.grey.withAlpha (0.70f));
    devLookButton_.setColour (juce::TextButton::textColourOnId, theme.lightGrey.withAlpha (0.85f));
    devLookButton_.onClick = [this]
    {
        if (onDevLookClicked)
            onDevLookClicked();
    };
    addAndMakeVisible (devLookButton_);
#endif
}

FooterBar::~FooterBar() = default;

void FooterBar::setControlBinder (AnalyzerPro::ControlBinder& binder)
{
    binder.bindToggle (AnalyzerPro::ControlId::AnalyzerHoldPeaks, holdBtn_);
    binder.bindDraggableValueLabel (AnalyzerPro::ControlId::AnalyzerPeakDecay, releaseTimeValue_);
    binder.bindDraggableValueLabel (AnalyzerPro::ControlId::PeakHoldDecayTime, holdDecayValue_);
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

#if ANALYZERPRO_DEV_LOOK_PANEL
    constexpr int kDevBtnW = 28;
    auto devBtn = area.removeFromRight (kDevBtnW).withSizeKeepingCentre (kDevBtnW, btnH);
    area.removeFromRight (gap);
    devLookButton_.setBounds (devBtn);
#endif

    // Right side: Release | Hold Decay | Hold (right to left)
    auto right = area;

    // Release time: value + label
    auto releaseVal = right.removeFromRight (valW).withSizeKeepingCentre (valW, btnH);
    right.removeFromRight (gap);
    auto releaseLbl = right.removeFromRight (lblW).withSizeKeepingCentre (lblW, btnH);
    right.removeFromRight (gap * 2);

    // Hold decay: value + label
    auto holdDecayVal = right.removeFromRight (valW).withSizeKeepingCentre (valW, btnH);
    right.removeFromRight (gap);
    auto holdDecayLbl = right.removeFromRight (lblW + 8).withSizeKeepingCentre (lblW + 8, btnH);
    right.removeFromRight (gap * 2);

    // Hold: button + label
    auto holdToggle = right.removeFromRight (btnW).withSizeKeepingCentre (btnW, btnH);
    right.removeFromRight (gap);
    auto holdLbl    = right.removeFromRight (lblW).withSizeKeepingCentre (lblW, btnH);
    right.removeFromRight (gap * 2);

    holdLabel_      .setBounds (holdLbl);
    holdBtn_        .setBounds (holdToggle);
    holdDecayLabel_ .setBounds (holdDecayLbl);
    holdDecayValue_ .setBounds (holdDecayVal);
    releaseLabel_   .setBounds (releaseLbl);
    releaseTimeValue_.setBounds (releaseVal);

    // Status label takes the remaining left area
    statusLabel.setBounds (area.withRight (right.getRight()));
}
