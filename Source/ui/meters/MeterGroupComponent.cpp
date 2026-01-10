#include "MeterGroupComponent.h"
#include <mdsp_ui/UiContext.h>

namespace
{
    static juce::String labelFor (MeterGroupComponent::GroupType t)
    {
        return (t == MeterGroupComponent::GroupType::Output) ? "OUT" : "IN";
    }

    static juce::String channelLabel (int channelCount, int index)
    {
        if (channelCount <= 1)
            return "MONO";
        return (index == 0) ? "L" : "R";
    }
}

MeterGroupComponent::MeterGroupComponent (mdsp_ui::UiContext& ui,
                                          AnalayzerProAudioProcessor& processor, GroupType type)
    : ui_ (ui), processor_ (processor), type_ (type)
{
    rmsButton_.setClickingTogglesState (false);
    peakButton_.setClickingTogglesState (false);

    rmsButton_.setConnectedEdges (juce::Button::ConnectedOnRight);
    peakButton_.setConnectedEdges (juce::Button::ConnectedOnLeft);

    rmsButton_.onClick  = [this] { setDisplayMode (MeterComponent::DisplayMode::RMS); };
    peakButton_.onClick = [this] { setDisplayMode (MeterComponent::DisplayMode::Peak); };

    addAndMakeVisible (rmsButton_);
    addAndMakeVisible (peakButton_);

    setDisplayMode (MeterComponent::DisplayMode::RMS);

    // Build meter components (wiring to processor state).
    const auto* states = (type_ == GroupType::Output) ? processor_.getOutputMeterStates()
                                                      : processor_.getInputMeterStates();

    meter0_ = std::make_unique<MeterComponent> (ui_, &states[0].peakDb, &states[0].rmsDb, &states[0].clipLatched, channelLabel (channelCount_, 0));
    meter1_ = std::make_unique<MeterComponent> (ui_, &states[1].peakDb, &states[1].rmsDb, &states[1].clipLatched, channelLabel (channelCount_, 1));

    addAndMakeVisible (*meter0_);
    addAndMakeVisible (*meter1_);

    startTimerHz (40);
}

MeterGroupComponent::~MeterGroupComponent()
{
    stopTimer();
}

int MeterGroupComponent::getPreferredWidth() const noexcept
{
    return (channelCount_ <= 1) ? 56 : 98;
}

void MeterGroupComponent::setChannelCount (int count)
{
    const int clamped = juce::jlimit (1, 2, count);
    if (channelCount_ == clamped)
        return;

    channelCount_ = clamped;

    if (meter0_ != nullptr) meter0_->setLabelText (channelLabel (channelCount_, 0));
    if (meter1_ != nullptr) meter1_->setLabelText (channelLabel (channelCount_, 1));

    resized();
}

void MeterGroupComponent::setDisplayMode (MeterComponent::DisplayMode mode)
{
    displayMode_ = mode;

    const bool rmsOn = (mode == MeterComponent::DisplayMode::RMS);
    rmsButton_.setToggleState (rmsOn, juce::NotificationType::dontSendNotification);
    peakButton_.setToggleState (! rmsOn, juce::NotificationType::dontSendNotification);

    const auto& theme = ui_.theme();
    auto applyButtonStyle = [&] (juce::TextButton& b, bool on)
    {
        b.setColour (juce::TextButton::buttonColourId, (on ? theme.accent.withAlpha (0.35f) : theme.panel.withAlpha (0.85f)));
        b.setColour (juce::TextButton::buttonOnColourId, theme.accent.withAlpha (0.35f));
        b.setColour (juce::TextButton::textColourOffId, theme.text.withAlpha (0.85f));
        b.setColour (juce::TextButton::textColourOnId, theme.text.withAlpha (0.95f));
    };

    applyButtonStyle (rmsButton_, rmsOn);
    applyButtonStyle (peakButton_, ! rmsOn);

    if (meter0_ != nullptr) meter0_->setDisplayMode (mode);
    if (meter1_ != nullptr) meter1_->setDisplayMode (mode);
}

void MeterGroupComponent::timerCallback()
{
    const int desired = (type_ == GroupType::Output) ? processor_.getMeterOutputChannelCount()
                                                     : processor_.getMeterInputChannelCount();
    setChannelCount (desired);

    if (meter0_ != nullptr) meter0_->updateFromAtomics();
    if (meter1_ != nullptr) meter1_->updateFromAtomics();
}

void MeterGroupComponent::paint (juce::Graphics& g)
{
    const auto& theme = ui_.theme();
    const auto& type = ui_.type();

    g.setColour (theme.textMuted.withAlpha (0.7f));
    g.setFont (type.labelFont());
    g.drawText (labelFor (type_), labelArea_, juce::Justification::centred);
}

void MeterGroupComponent::resized()
{
    const auto& m = ui_.metrics();
    auto b = getLocalBounds();

    headerArea_ = b.removeFromTop (34);  // TODO: Move to metrics
    labelArea_ = headerArea_.removeFromTop (16);  // TODO: Move to metrics
    toggleArea_ = headerArea_;

    metersArea_ = b.reduced (m.strokeThick, m.strokeThick);

    auto toggle = toggleArea_.reduced (m.padSmall, m.strokeThick);
    const int toggleW = toggle.getWidth();
    const int half = toggleW / 2;
    rmsButton_.setBounds (toggle.removeFromLeft (half));
    peakButton_.setBounds (toggle);

    const int meterW = 44;  // TODO: Move to metrics
    const int gap = 6;  // TODO: Move to metrics

    if (channelCount_ <= 1)
    {
        if (meter0_ != nullptr)
        {
            meter0_->setVisible (true);
            meter0_->setBounds (metersArea_.withSizeKeepingCentre (meterW, metersArea_.getHeight()));
        }
        if (meter1_ != nullptr)
            meter1_->setVisible (false);
    }
    else
    {
        auto row = metersArea_;
        const int totalW = meterW * 2 + gap;
        row = row.withSizeKeepingCentre (totalW, row.getHeight());

        auto left = row.removeFromLeft (meterW);
        row.removeFromLeft (gap);
        auto right = row.removeFromLeft (meterW);

        if (meter0_ != nullptr)
        {
            meter0_->setVisible (true);
            meter0_->setBounds (left);
        }
        if (meter1_ != nullptr)
        {
            meter1_->setVisible (true);
            meter1_->setBounds (right);
        }
    }
}

