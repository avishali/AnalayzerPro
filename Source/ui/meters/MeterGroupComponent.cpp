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

    // Button Listeners: Update Source of Truth (Processor)
    rmsButton_.onClick = [this]
    {
        processor_.setMeterMode (AnalayzerProAudioProcessor::MeterMode::RMS);
    };

    peakButton_.onClick = [this]
    {
        processor_.setMeterMode (AnalayzerProAudioProcessor::MeterMode::Peak);
    };

    addAndMakeVisible (rmsButton_);
    addAndMakeVisible (peakButton_);

    // Initial sync (will be corrected by timer if needed)
    setDisplayMode (MeterComponent::DisplayMode::RMS);

    // Build meter components (wiring to processor state).
    // Build meter components (wiring to processor state).
    // const auto* states = (type_ == GroupType::Output) ? processor_.getOutputMeterStates()
    //                                                   : processor_.getInputMeterStates();

    // Define reset handlers
    auto handleClipReset = [this] { processor_.resetMeterClipLatches(); };
    
    // Peak reset linked for this group
    auto handlePeakReset = [this] 
    { 
        if (meter0_) meter0_->resetPeakHold();
        if (meter1_) meter1_->resetPeakHold();
    };

    // Initialize with nullptr for atomics (we drive manually for M/S support)
    meter0_ = std::make_unique<MeterComponent> (ui_, nullptr, nullptr, nullptr, channelLabel (channelCount_, 0));
    meter0_->onClipReset = handleClipReset;
    meter0_->onPeakReset = handlePeakReset;
    
    meter1_ = std::make_unique<MeterComponent> (ui_, nullptr, nullptr, nullptr, channelLabel (channelCount_, 1));
    meter1_->onClipReset = handleClipReset;
    meter1_->onPeakReset = handlePeakReset;

    addAndMakeVisible (*meter0_);
    addAndMakeVisible (*meter1_);

    // Force sync meters to ensure they are visible on startup
    if (meter0_) meter0_->setDisplayMode (displayMode_);
    if (meter1_) meter1_->setDisplayMode (displayMode_);

    startTimerHz (30); // 30Hz visual update
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

    if (meter0_) meter0_->setDisplayMode (mode);
    if (meter1_) meter1_->setDisplayMode (mode);

    // Update buttons
    rmsButton_.setToggleState (mode == MeterComponent::DisplayMode::RMS, juce::dontSendNotification);
    peakButton_.setToggleState (mode == MeterComponent::DisplayMode::Peak, juce::dontSendNotification);
}

void MeterGroupComponent::setChannelMode (ChannelMode mode)
{
    if (channelMode_ == mode)
        return;
        
    channelMode_ = mode;
    
    // Update labels immediately
    if (meter0_) meter0_->setLabelText (channelMode_ == ChannelMode::Stereo ? "L" : "M");
    if (meter1_) meter1_->setLabelText (channelMode_ == ChannelMode::Stereo ? "R" : "S");
    
    resized();
    

}

void MeterGroupComponent::setHoldEnabled (bool hold)
{
    if (meter0_) meter0_->setHoldEnabled (hold);
    if (meter1_) meter1_->setHoldEnabled (hold);
}

void MeterGroupComponent::timerCallback()
{
    // Poll channel count
    const int newCount = (type_ == GroupType::Input) ? processor_.getMeterInputChannelCount()
                                                     : processor_.getMeterOutputChannelCount();
    if (newCount != channelCount_)
    {
        setChannelCount (newCount);
    }
    
    // Poll Meter Mode
    const auto procMode = processor_.getMeterMode();
    
    // Map Processor mode to UI mode
    MeterComponent::DisplayMode targetUiMode = MeterComponent::DisplayMode::RMS;
    if (procMode == AnalayzerProAudioProcessor::MeterMode::Peak)
        targetUiMode = MeterComponent::DisplayMode::Peak;
        
    // Update if changed
    if (targetUiMode != displayMode_)
    {
        setDisplayMode (targetUiMode);
    }

    const bool bypassed = processor_.getBypassState();
    
    // Manual Drive Logic
    const auto* states = (type_ == GroupType::Output) ? processor_.getOutputMeterStates()
                                                      : processor_.getInputMeterStates();

    if (meter0_ && meter1_)
    {
        meter0_->setBypassed (bypassed);
        meter1_->setBypassed (bypassed);
        
        if (!bypassed)
        {
            // Read Raw Values
            float lPeakDb = states[0].peakDb.load (std::memory_order_relaxed);
            float lRmsDb  = states[0].rmsDb.load (std::memory_order_relaxed);
            bool  lClip   = states[0].clipLatched.load (std::memory_order_relaxed);
            
            float rPeakDb = states[1].peakDb.load (std::memory_order_relaxed);
            float rRmsDb  = states[1].rmsDb.load (std::memory_order_relaxed);
            bool  rClip   = states[1].clipLatched.load (std::memory_order_relaxed);
            
            if (channelMode_ == ChannelMode::MidSide)
            {
                // M/S Processing
                // Convert DB to Linear
                auto dbToLin = [] (float db) { return std::pow (10.0f, db / 20.0f); };
                auto linToDb = [] (float lin) { return (lin > 0.000001f) ? 20.0f * std::log10 (lin) : -120.0f; };
                
                float lPeak = dbToLin (lPeakDb);
                float rPeak = dbToLin (rPeakDb);
                float lRms  = dbToLin (lRmsDb);
                float rRms  = dbToLin (rRmsDb);
                
                // M = (L+R)/2
                // S = (L-R)/2
                float midPeak = (lPeak + rPeak) * 0.5f;
                float sidePeak = std::abs (lPeak - rPeak) * 0.5f;
                
                float midRms = (lRms + rRms) * 0.5f;
                float sideRms = std::abs (lRms - rRms) * 0.5f;
                
                meter0_->setLevels (linToDb (midPeak), linToDb (midRms), lClip || rClip);
                meter1_->setLevels (linToDb (sidePeak), linToDb (sideRms), lClip || rClip);
            }
            else
            {
                // Stereo Pass-through
                meter0_->setLevels (lPeakDb, lRmsDb, lClip);
                meter1_->setLevels (rPeakDb, rRmsDb, rClip);
            }
        }
    }
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

    metersArea_ = b.reduced (static_cast<int> (m.strokeThick), static_cast<int> (m.strokeThick));

    auto toggle = toggleArea_.reduced (m.padSmall, static_cast<int> (m.strokeThick));
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

