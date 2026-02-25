#include "MeterGroupComponent.h"

#include <mdsp_ui/UiContext.h>

#include <cmath>

namespace
{
static juce::String labelFor (MeterGroupComponent::GroupType t)
{
    return (t == MeterGroupComponent::GroupType::Output) ? "OUT" : "IN";
}

static juce::String channelLabel (MeterGroupComponent::ChannelMode mode, int channelCount, int index)
{
    if (channelCount <= 1)
        return "MONO";

    if (mode == MeterGroupComponent::ChannelMode::MidSide)
        return (index == 0) ? "M" : "S";

    return (index == 0) ? "L" : "R";
}
}

MeterGroupComponent::MeterGroupComponent (mdsp_ui::UiContext& ui,
                                          AnalayzerProAudioProcessor& processor,
                                          GroupType type)
    : ui_ (ui),
      processor_ (processor),
      type_ (type)
{
    rmsButton_.setClickingTogglesState (false);
    peakButton_.setClickingTogglesState (false);

    rmsButton_.setConnectedEdges (juce::Button::ConnectedOnRight);
    peakButton_.setConnectedEdges (juce::Button::ConnectedOnLeft);

    rmsButton_.onClick = [this]
    {
        processor_.setMeterMode (AnalayzerProAudioProcessor::MeterMode::RMS);
    };

    peakButton_.onClick = [this]
    {
        processor_.setMeterMode (AnalayzerProAudioProcessor::MeterMode::Peak);
    };

    scaleFullButton_.setClickingTogglesState (false);
    scale24Button_.setClickingTogglesState (false);
    scale12Button_.setClickingTogglesState (false);
    scaleFullButton_.onClick = [this] { setScaleMode (ScaleMode::FullRange); };
    scale24Button_.onClick = [this] { setScaleMode (ScaleMode::Top24Db); };
    scale12Button_.onClick = [this] { setScaleMode (ScaleMode::Top12Db); };

    addAndMakeVisible (rmsButton_);
    addAndMakeVisible (peakButton_);
    addAndMakeVisible (scaleFullButton_);
    addAndMakeVisible (scale24Button_);
    addAndMakeVisible (scale12Button_);

    meter0_ = std::make_unique<MeterComponent> (ui_, channelLabel (channelMode_, channelCount_, 0));
    meter1_ = std::make_unique<MeterComponent> (ui_, channelLabel (channelMode_, channelCount_, 1));

    meter0_->setClipResetCallback (&MeterGroupComponent::clipResetThunk, this);
    meter1_->setClipResetCallback (&MeterGroupComponent::clipResetThunk, this);
    meter0_->setPeakResetCallback (&MeterGroupComponent::peakResetThunk, this);
    meter1_->setPeakResetCallback (&MeterGroupComponent::peakResetThunk, this);

    addAndMakeVisible (*meter0_);
    addAndMakeVisible (*meter1_);

    provider0_.setScaleMode (scaleMode_);
    provider1_.setScaleMode (scaleMode_);
    provider0_.setDisplayMode (displayMode_);
    provider1_.setDisplayMode (displayMode_);

    rmsButton_.setToggleState (displayMode_ == DisplayMode::Rms, juce::dontSendNotification);
    peakButton_.setToggleState (displayMode_ == DisplayMode::Peak, juce::dontSendNotification);
    scaleFullButton_.setToggleState (scaleMode_ == ScaleMode::FullRange, juce::dontSendNotification);
    scale24Button_.setToggleState (scaleMode_ == ScaleMode::Top24Db, juce::dontSendNotification);
    scale12Button_.setToggleState (scaleMode_ == ScaleMode::Top12Db, juce::dontSendNotification);

    pushRenderStates();

    startTimerHz (30);
}

MeterGroupComponent::~MeterGroupComponent()
{
    stopTimer();
}

void MeterGroupComponent::clipResetThunk (void* ctx) noexcept
{
    if (ctx != nullptr)
        static_cast<MeterGroupComponent*> (ctx)->handleClipReset();
}

void MeterGroupComponent::peakResetThunk (void* ctx) noexcept
{
    if (ctx != nullptr)
        static_cast<MeterGroupComponent*> (ctx)->handlePeakReset();
}

void MeterGroupComponent::handleClipReset() noexcept
{
    processor_.resetMeterClipLatches();
}

void MeterGroupComponent::handlePeakReset() noexcept
{
    provider0_.resetPeakHold();
    provider1_.resetPeakHold();
    pushRenderStates();
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

    if (meter0_ != nullptr)
        meter0_->setLabelText (channelLabel (channelMode_, channelCount_, 0));
    if (meter1_ != nullptr)
        meter1_->setLabelText (channelLabel (channelMode_, channelCount_, 1));

    resized();
}

void MeterGroupComponent::setDisplayMode (DisplayMode mode)
{
    if (displayMode_ == mode)
        return;

    displayMode_ = mode;
    provider0_.setDisplayMode (mode);
    provider1_.setDisplayMode (mode);

    rmsButton_.setToggleState (mode == DisplayMode::Rms, juce::dontSendNotification);
    peakButton_.setToggleState (mode == DisplayMode::Peak, juce::dontSendNotification);

    pushRenderStates();
}

void MeterGroupComponent::setChannelMode (ChannelMode mode)
{
    if (channelMode_ == mode)
        return;

    channelMode_ = mode;

    if (meter0_ != nullptr)
        meter0_->setLabelText (channelLabel (channelMode_, channelCount_, 0));
    if (meter1_ != nullptr)
        meter1_->setLabelText (channelLabel (channelMode_, channelCount_, 1));

    resized();
}

void MeterGroupComponent::setHoldEnabled (bool hold)
{
    provider0_.setHoldEnabled (hold);
    provider1_.setHoldEnabled (hold);
    pushRenderStates();
}

void MeterGroupComponent::setScaleMode (ScaleMode mode)
{
    if (scaleMode_ == mode)
        return;

    scaleMode_ = mode;
    provider0_.setScaleMode (mode);
    provider1_.setScaleMode (mode);

    scaleFullButton_.setToggleState (mode == ScaleMode::FullRange, juce::dontSendNotification);
    scale24Button_.setToggleState (mode == ScaleMode::Top24Db, juce::dontSendNotification);
    scale12Button_.setToggleState (mode == ScaleMode::Top12Db, juce::dontSendNotification);

    pushRenderStates();
}

void MeterGroupComponent::pushRenderStates()
{
    provider0_.fillRenderState (renderState0_);
    provider1_.fillRenderState (renderState1_);

    if (meter0_ != nullptr)
        meter0_->setRenderState (renderState0_);
    if (meter1_ != nullptr)
        meter1_->setRenderState (renderState1_);
}

void MeterGroupComponent::timerCallback()
{
    const int newCount = (type_ == GroupType::Input) ? processor_.getMeterInputChannelCount()
                                                      : processor_.getMeterOutputChannelCount();
    if (newCount != channelCount_)
        setChannelCount (newCount);

    const auto procMode = processor_.getMeterMode();
    const auto targetUiMode = (procMode == AnalayzerProAudioProcessor::MeterMode::Peak)
                                  ? DisplayMode::Peak
                                  : DisplayMode::Rms;
    if (targetUiMode != displayMode_)
    {
        displayMode_ = targetUiMode;
        provider0_.setDisplayMode (displayMode_);
        provider1_.setDisplayMode (displayMode_);
        rmsButton_.setToggleState (displayMode_ == DisplayMode::Rms, juce::dontSendNotification);
        peakButton_.setToggleState (displayMode_ == DisplayMode::Peak, juce::dontSendNotification);
    }

    const bool bypassed = processor_.getBypassState();
    const auto* states = (type_ == GroupType::Output) ? processor_.getOutputMeterStates()
                                                       : processor_.getInputMeterStates();

    float lPeakDb = states[0].peakDb.load (std::memory_order_relaxed);
    float lRmsDb = states[0].rmsDb.load (std::memory_order_relaxed);
    const bool lClip = states[0].clipLatched.load (std::memory_order_relaxed);

    float rPeakDb = states[1].peakDb.load (std::memory_order_relaxed);
    float rRmsDb = states[1].rmsDb.load (std::memory_order_relaxed);
    const bool rClip = states[1].clipLatched.load (std::memory_order_relaxed);

    bool outClip0 = lClip;
    bool outClip1 = rClip;

    if (channelMode_ == ChannelMode::MidSide)
    {
        auto dbToLin = [] (float db) noexcept
        {
            return std::pow (10.0f, db / 20.0f);
        };

        auto linToDb = [] (float lin) noexcept
        {
            return (lin > 0.000001f) ? 20.0f * std::log10 (lin) : -120.0f;
        };

        const float lPeak = dbToLin (lPeakDb);
        const float rPeak = dbToLin (rPeakDb);
        const float lRms = dbToLin (lRmsDb);
        const float rRms = dbToLin (rRmsDb);

        const float midPeak = (lPeak + rPeak) * 0.5f;
        const float sidePeak = std::abs (lPeak - rPeak) * 0.5f;
        const float midRms = (lRms + rRms) * 0.5f;
        const float sideRms = std::abs (lRms - rRms) * 0.5f;

        lPeakDb = linToDb (midPeak);
        lRmsDb = linToDb (midRms);
        rPeakDb = linToDb (sidePeak);
        rRmsDb = linToDb (sideRms);
        outClip0 = lClip || rClip;
        outClip1 = lClip || rClip;
    }

    provider0_.updateFromValues (lPeakDb, lRmsDb, outClip0, bypassed);
    provider1_.updateFromValues (rPeakDb, rRmsDb, outClip1, bypassed);

    pushRenderStates();
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

    const int labelHeight = 16;
    headerArea_ = b.removeFromTop (labelHeight);
    labelArea_ = headerArea_;

    const int toggleTotalHeight = 44;
    toggleArea_ = b.removeFromBottom (toggleTotalHeight);
    metersArea_ = b.reduced (static_cast<int> (m.strokeThick), static_cast<int> (m.strokeThick));

    auto toggle = toggleArea_.reduced (m.padSmall, static_cast<int> (m.strokeThick));
    const int scaleRowHeight = 20;
    auto scaleRow = toggle.removeFromTop (scaleRowHeight);
    auto modeRow = toggle;

    const int scaleThird = scaleRow.getWidth() / 3;
    scaleFullButton_.setBounds (scaleRow.removeFromLeft (scaleThird));
    scale24Button_.setBounds (scaleRow.removeFromLeft (scaleThird));
    scale12Button_.setBounds (scaleRow);

    const int modeHalf = modeRow.getWidth() / 2;
    rmsButton_.setBounds (modeRow.removeFromLeft (modeHalf));
    peakButton_.setBounds (modeRow);

    const int meterW = m.meterGroupMeterW;
    const int gap = m.meterGroupGap;

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
