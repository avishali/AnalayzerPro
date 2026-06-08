#include "MeterGroupComponent.h"

#include "../../config/UiRates.h"
#include <mdsp_ui/UiContext.h>

#include <cmath>

namespace
{
constexpr int kMeterRepaintTickInterval = (AnalyzerPro::UiRates::kMeterFeedHz + AnalyzerPro::UiRates::kMeterHz / 2)
                                        / AnalyzerPro::UiRates::kMeterHz;
constexpr int kCenterScaleGutterWidth = 24;
constexpr int kToggleTotalHeight = 36;
constexpr int kScaleRowHeight = 16;
constexpr bool kUsePerceptualMeterScale = true;
constexpr float kMidSideSmoothingAttackMs = 15.0f;
constexpr float kMidSideSmoothingReleaseMs = 300.0f;
constexpr float kCenterScaleFontHeight = 9.0f;
constexpr float kCenterScaleTickThin = 0.75f;
constexpr float kCenterScaleTickZero = 1.5f;

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

static bool nearTick (float value, float target) noexcept
{
    return std::abs (value - target) < 0.001f;
}

static float smoothingCoefficient (float timeMs) noexcept
{
    const float dtSeconds = 1.0f / static_cast<float> (AnalyzerPro::UiRates::kMeterFeedHz);
    const float timeSeconds = juce::jmax (0.001f, timeMs * 0.001f);
    return 1.0f - std::exp (-dtSeconds / timeSeconds);
}

static float smoothDbValue (float inputDb, float smoothedDb) noexcept
{
    const float coefficient = (inputDb >= smoothedDb)
                                  ? smoothingCoefficient (kMidSideSmoothingAttackMs)
                                  : smoothingCoefficient (kMidSideSmoothingReleaseMs);
    return smoothedDb + (inputDb - smoothedDb) * coefficient;
}

static juce::String dbLabelForScale (float db)
{
    if (nearTick (db, 0.0f))
        return "0";

    if (nearTick (db, std::round (db)))
    {
        const int dbInt = static_cast<int> (std::round (db));
        return dbInt > 0 ? juce::String ("+") + juce::String (dbInt)
                         : juce::String (dbInt);
    }

    return db > 0.0f ? juce::String ("+") + juce::String (db, 1)
                     : juce::String (db, 1);
}

static void drawScaleTick (juce::Graphics& g,
                           const mdsp_ui::Theme& theme,
                           mdsp_ui::meters::MeterScaleMode scaleMode,
                           bool perceptualScale,
                           juce::Rectangle<float> gutter,
                           float barTop,
                           float barBottom,
                           float db)
{
    const float barHeight = barBottom - barTop;
    if (barHeight <= 1.0f || gutter.getWidth() <= 1.0f)
        return;

    const float norm = mdsp_ui::meters::MeterRenderStateProvider::normaliseDb (db, scaleMode, perceptualScale);
    const float y = barBottom - norm * barHeight;
    if (! std::isfinite (y) || y < barTop - 0.5f || y > barBottom + 0.5f)
        return;

    const bool isZero = nearTick (db, 0.0f);
    const bool isPositive = db > 0.0f;
    const auto colour = isZero ? theme.text.withAlpha (0.68f)
                               : (isPositive ? theme.danger.withAlpha (0.62f)
                                             : theme.textMuted.withAlpha (0.58f));

    g.setColour (colour);
    g.drawLine (gutter.getX() + 2.0f,
                y,
                gutter.getRight() - 2.0f,
                y,
                isZero ? kCenterScaleTickZero : kCenterScaleTickThin);

    g.drawText (dbLabelForScale (db),
                juce::Rectangle<float> (gutter.getX(),
                                        juce::jmax (barTop, y - kCenterScaleFontHeight - 2.0f),
                                        gutter.getWidth(),
                                        kCenterScaleFontHeight + 1.0f),
                juce::Justification::centred);
}

static void drawSharedDbScale (juce::Graphics& g,
                               const mdsp_ui::Theme& theme,
                               juce::Font font,
                               mdsp_ui::meters::MeterScaleMode scaleMode,
                               bool perceptualScale,
                               juce::Rectangle<float> gutter,
                               float barTop,
                               float barBottom)
{
    g.setFont (font);

    if (scaleMode == mdsp_ui::meters::MeterScaleMode::FullRange)
    {
        static constexpr float ticks[] = { 6.0f, 3.0f, 0.0f, -6.0f, -12.0f, -24.0f, -48.0f, -72.0f, -96.0f, -120.0f };
        for (const auto db : ticks)
            drawScaleTick (g, theme, scaleMode, perceptualScale, gutter, barTop, barBottom, db);
        return;
    }

    if (scaleMode == mdsp_ui::meters::MeterScaleMode::Top24Db)
    {
        for (int db = 0; db >= -24; db -= 3)
            drawScaleTick (g, theme, scaleMode, perceptualScale, gutter, barTop, barBottom, static_cast<float> (db));
        return;
    }

    if (scaleMode == mdsp_ui::meters::MeterScaleMode::Top12Db)
    {
        for (int db = 0; db >= -12; --db)
            drawScaleTick (g, theme, scaleMode, perceptualScale, gutter, barTop, barBottom, static_cast<float> (db));
        return;
    }

    if (scaleMode == mdsp_ui::meters::MeterScaleMode::Top6Db)
    {
        for (int db = 0; db >= -6; --db)
            drawScaleTick (g, theme, scaleMode, perceptualScale, gutter, barTop, barBottom, static_cast<float> (db));
        return;
    }

    for (int db = 0; db >= -48; db -= 6)
        drawScaleTick (g, theme, scaleMode, perceptualScale, gutter, barTop, barBottom, static_cast<float> (db));
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
    scale6Button_.setClickingTogglesState (false);
    scaleFullButton_.onClick = [this] { setScaleMode (ScaleMode::FullRange); };
    scale24Button_.onClick = [this] { setScaleMode (ScaleMode::Top24Db); };
    scale12Button_.onClick = [this] { setScaleMode (ScaleMode::Top12Db); };
    scale6Button_.onClick = [this] { setScaleMode (ScaleMode::Top6Db); };

    addAndMakeVisible (rmsButton_);
    addAndMakeVisible (peakButton_);
    addAndMakeVisible (scaleFullButton_);
    addAndMakeVisible (scale24Button_);
    addAndMakeVisible (scale12Button_);
    addAndMakeVisible (scale6Button_);

    scaleFullButton_.setTooltip ("Meter range: full (-120 dB to +6 dB)");
    scale24Button_.setTooltip ("Meter zoom: top 24 dB (0 to -24)");
    scale12Button_.setTooltip ("Meter zoom: top 12 dB (0 to -12)");
    scale6Button_.setTooltip ("Meter zoom: top 6 dB (0 to -6)");

    meter0_ = std::make_unique<MeterComponent> (ui_, channelLabel (channelMode_, channelCount_, 0));
    meter1_ = std::make_unique<MeterComponent> (ui_, channelLabel (channelMode_, channelCount_, 1));

    meter0_->setClipResetCallback (&MeterGroupComponent::clipResetThunk, this);
    meter1_->setClipResetCallback (&MeterGroupComponent::clipResetThunk, this);
    meter0_->setPeakResetCallback (&MeterGroupComponent::peakResetThunk, this);
    meter1_->setPeakResetCallback (&MeterGroupComponent::peakResetThunk, this);

    addAndMakeVisible (*meter0_);
    addAndMakeVisible (*meter1_);

    provider0_.setPerceptualScale (kUsePerceptualMeterScale);
    provider1_.setPerceptualScale (kUsePerceptualMeterScale);
    provider0_.setScaleMode (scaleMode_);
    provider1_.setScaleMode (scaleMode_);
    provider0_.setDisplayMode (displayMode_);
    provider1_.setDisplayMode (displayMode_);

    rmsButton_.setToggleState (displayMode_ == DisplayMode::Rms, juce::dontSendNotification);
    peakButton_.setToggleState (displayMode_ == DisplayMode::Peak, juce::dontSendNotification);
    scaleFullButton_.setToggleState (scaleMode_ == ScaleMode::FullRange, juce::dontSendNotification);
    scale24Button_.setToggleState (scaleMode_ == ScaleMode::Top24Db, juce::dontSendNotification);
    scale12Button_.setToggleState (scaleMode_ == ScaleMode::Top12Db, juce::dontSendNotification);
    scale6Button_.setToggleState (scaleMode_ == ScaleMode::Top6Db, juce::dontSendNotification);

    pushRenderStates();

    startTimerHz (AnalyzerPro::UiRates::kMeterFeedHz);
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
    resetMidSideSmoothing();
    provider0_.resetPeakHold();
    provider1_.resetPeakHold();
    pushRenderStates();
}

void MeterGroupComponent::resetMidSideSmoothing() noexcept
{
    midSideSmoothingInitialised_ = false;
    smoothedMidPeakDb_ = -120.0f;
    smoothedSidePeakDb_ = -120.0f;
    smoothedMidRmsDb_ = -120.0f;
    smoothedSideRmsDb_ = -120.0f;
}

int MeterGroupComponent::getPreferredWidth() const noexcept
{
    const auto& m = ui_.metrics();
    const int meterW = m.meterGroupMeterW;
    const int gap = juce::jmax (m.meterGroupGap, kCenterScaleGutterWidth);
    return (channelCount_ <= 1) ? meterW + m.meterGroupGap : meterW * 2 + gap;
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
    if (channelMode_ == ChannelMode::MidSide)
        resetMidSideSmoothing();

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

const MeterComponent* MeterGroupComponent::getMeter (int idx) const noexcept
{
    if (idx == 0)
        return meter0_.get();
    if (idx == 1)
        return meter1_.get();

    return nullptr;
}

void MeterGroupComponent::setTraceColorStore (AnalyzerPro::TraceColorStore* store) noexcept
{
    traceColors_ = store;

    if (meter0_ != nullptr)
        meter0_->setTraceColorStore (traceColors_);
    if (meter1_ != nullptr)
        meter1_->setTraceColorStore (traceColors_);
}

#if defined(ANALYZERPRO_METAL_EDITOR) && ANALYZERPRO_METAL_EDITOR
void MeterGroupComponent::setMetalTraceSuppressedForChromeCapture (bool shouldSuppress) noexcept
{
    if (meter0_ != nullptr)
        meter0_->setMetalTraceSuppressedForChromeCapture (shouldSuppress);
    if (meter1_ != nullptr)
        meter1_->setMetalTraceSuppressedForChromeCapture (shouldSuppress);
}
#endif

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
    scale6Button_.setToggleState (mode == ScaleMode::Top6Db, juce::dontSendNotification);

    pushRenderStates();
    repaint();
}

void MeterGroupComponent::pushRenderStates()
{
    provider0_.fillRenderState (renderState0_);
    provider1_.fillRenderState (renderState1_);
    meterFeedTick_ = 0;

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
        const auto* midSideStates = (type_ == GroupType::Output) ? processor_.getOutputMidSideMeterStates()
                                                                 : processor_.getInputMidSideMeterStates();
        const float midPeakDb = midSideStates[0].peakDb.load (std::memory_order_relaxed);
        const float midRmsDb = midSideStates[0].rmsDb.load (std::memory_order_relaxed);
        const float sidePeakDb = midSideStates[1].peakDb.load (std::memory_order_relaxed);
        const float sideRmsDb = midSideStates[1].rmsDb.load (std::memory_order_relaxed);

        if (! midSideSmoothingInitialised_)
        {
            smoothedMidPeakDb_ = midPeakDb;
            smoothedSidePeakDb_ = sidePeakDb;
            smoothedMidRmsDb_ = midRmsDb;
            smoothedSideRmsDb_ = sideRmsDb;
            midSideSmoothingInitialised_ = true;
        }
        else
        {
            smoothedMidPeakDb_ = smoothDbValue (midPeakDb, smoothedMidPeakDb_);
            smoothedSidePeakDb_ = smoothDbValue (sidePeakDb, smoothedSidePeakDb_);
            smoothedMidRmsDb_ = smoothDbValue (midRmsDb, smoothedMidRmsDb_);
            smoothedSideRmsDb_ = smoothDbValue (sideRmsDb, smoothedSideRmsDb_);
        }

        lPeakDb = smoothedMidPeakDb_;
        lRmsDb = smoothedMidRmsDb_;
        rPeakDb = smoothedSidePeakDb_;
        rRmsDb = smoothedSideRmsDb_;
        outClip0 = lClip || rClip;
        outClip1 = lClip || rClip;
    }

    provider0_.updateFromValues (lPeakDb, lRmsDb, outClip0, bypassed);
    provider1_.updateFromValues (rPeakDb, rRmsDb, outClip1, bypassed);

    provider0_.fillRenderState (renderState0_);
    provider1_.fillRenderState (renderState1_);

    if (meter0_ != nullptr)
        meter0_->updateRenderState (renderState0_);
    if (meter1_ != nullptr)
        meter1_->updateRenderState (renderState1_);

    const bool shouldRepaint = ++meterFeedTick_ >= juce::jmax (1, kMeterRepaintTickInterval);
    if (shouldRepaint)
    {
        meterFeedTick_ = 0;

        if (meter0_ != nullptr)
            meter0_->repaint();
        if (meter1_ != nullptr)
            meter1_->repaint();
    }
}

void MeterGroupComponent::paint (juce::Graphics& g)
{
    const auto& theme = ui_.theme();
    const auto& type = ui_.type();

    g.setColour (theme.textMuted.withAlpha (0.7f));
    g.setFont (type.labelFont());
    g.drawText (labelFor (type_), labelArea_, juce::Justification::centred);

    if (channelCount_ <= 1 || meter0_ == nullptr || meter1_ == nullptr || ! meter1_->isVisible())
        return;

    const auto leftBar = meter0_->getMeterBarBounds().translated (meter0_->getX(), meter0_->getY());
    const auto rightBar = meter1_->getMeterBarBounds().translated (meter1_->getX(), meter1_->getY());
    if (leftBar.isEmpty() || rightBar.isEmpty() || rightBar.getX() <= leftBar.getRight())
        return;

    const auto gutter = juce::Rectangle<int>::leftTopRightBottom (leftBar.getRight(),
                                                                  juce::jmin (leftBar.getY(), rightBar.getY()),
                                                                  rightBar.getX(),
                                                                  juce::jmax (leftBar.getBottom(), rightBar.getBottom()));
    drawSharedDbScale (g,
                       theme,
                       type.labelFont().withHeight (kCenterScaleFontHeight),
                       scaleMode_,
                       kUsePerceptualMeterScale,
                       gutter.toFloat(),
                       static_cast<float> (gutter.getY()),
                       static_cast<float> (gutter.getBottom()));
}

void MeterGroupComponent::resized()
{
    const auto& m = ui_.metrics();
    auto b = getLocalBounds();

    const int labelHeight = 16;
    headerArea_ = b.removeFromTop (labelHeight);
    labelArea_ = headerArea_;

    const int toggleTotalHeight = kToggleTotalHeight;
    toggleArea_ = b.removeFromBottom (toggleTotalHeight);
    b.removeFromBottom (6); // gap so the readout/buttons sit clear of the meter bar
    metersArea_ = b.reduced (static_cast<int> (m.strokeThick), static_cast<int> (m.strokeThick));

    auto toggle = toggleArea_.reduced (m.padSmall, static_cast<int> (m.strokeThick));
    const int scaleRowHeight = kScaleRowHeight;
    auto scaleRow = toggle.removeFromTop (scaleRowHeight);
    auto modeRow = toggle;

    const int scaleQuarter = juce::jmax (1, scaleRow.getWidth() / 4);
    scaleFullButton_.setBounds (scaleRow.removeFromLeft (scaleQuarter));
    scale24Button_.setBounds (scaleRow.removeFromLeft (scaleQuarter));
    scale12Button_.setBounds (scaleRow.removeFromLeft (scaleQuarter));
    scale6Button_.setBounds (scaleRow);

    const int modeHalf = modeRow.getWidth() / 2;
    rmsButton_.setBounds (modeRow.removeFromLeft (modeHalf));
    peakButton_.setBounds (modeRow);

    const int baseMeterW = m.meterGroupMeterW;
    const int gap = juce::jmax (m.meterGroupGap, kCenterScaleGutterWidth);

    if (channelCount_ <= 1)
    {
        if (meter0_ != nullptr)
        {
            meter0_->setVisible (true);
            meter0_->setBounds (metersArea_.withSizeKeepingCentre (baseMeterW, metersArea_.getHeight()));
        }
        if (meter1_ != nullptr)
            meter1_->setVisible (false);
    }
    else
    {
        auto row = metersArea_;
        const int meterW = baseMeterW;
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
