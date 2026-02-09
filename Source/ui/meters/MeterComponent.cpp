#include "MeterComponent.h"
#include <juce_core/juce_core.h>
#include <mdsp_ui/UiContext.h>
#include <cmath>
#include <utility>

namespace
{
    constexpr float kFullRangeMinDb = -120.0f;
    constexpr float kFullRangeMaxDb = 6.0f;
    constexpr juce::int64 kPeakHoldTimeMs = 1500;
    constexpr float kPeakDecayDbPerSec = 12.0f;
    constexpr int kPeakDecayTimerHz = 25;
}

MeterComponent::MeterComponent (mdsp_ui::UiContext& ui,
                                const std::atomic<float>* peakDb,
                                const std::atomic<float>* rmsDb,
                                const std::atomic<bool>* clipLatched,
                                juce::String labelText)
    : ui_ (ui),
      peakDb_ (peakDb),
      rmsDb_ (rmsDb),
      clipLatched_ (clipLatched),
      label_ (std::move (labelText))
{
    numericTextPeak_ = "-inf";
    numericTextRms_  = "-inf";
    setOpaque (false);
    lastTimePeakAtOrAboveMax_ = juce::Time::getMillisecondCounter();
    startTimer (1000 / kPeakDecayTimerHz);
}

MeterComponent::~MeterComponent()
{
    stopTimer();
}

void MeterComponent::setLabelText (juce::String labelText)
{
    if (label_ == labelText)
        return;

    label_ = std::move (labelText);
    repaint();
}

void MeterComponent::setBypassed (bool bypassed)
{
    if (isBypassed_ == bypassed)
        return;
        
    isBypassed_ = bypassed;
    repaint();
}

void MeterComponent::setLevels (float peakDb, float rmsDb, bool clipped)
{
    // Update internal state directly
    cachedPeakDb_ = peakDb;
    cachedRmsDb_  = rmsDb;
    cachedClip_   = clipped;
    
    // Handle visual hold: if holdEnabled_, only latch upward
    if (holdEnabled_)
    {
        if (cachedPeakDb_ > maxPeakDb_) maxPeakDb_ = cachedPeakDb_;
        if (cachedRmsDb_ > maxRmsDb_)   maxRmsDb_  = cachedRmsDb_;
    }
    else
    {
        // Live mode: max follows current (but still latches for this frame for display)
        if (cachedPeakDb_ > maxPeakDb_) maxPeakDb_ = cachedPeakDb_;
        if (cachedRmsDb_ > maxRmsDb_)   maxRmsDb_  = cachedRmsDb_;
    }
    if (cachedPeakDb_ >= maxPeakDb_)
        lastTimePeakAtOrAboveMax_ = juce::Time::getMillisecondCounter();
    
    // Convert for rendering
    cachedPeakNorm_ = dbToNorm (cachedPeakDb_);
    cachedRmsNorm_  = dbToNorm (cachedRmsDb_);
    maxPeakNorm_    = dbToNorm (maxPeakDb_);  // Track max for hold marker
    
    // Update numeric text display (same logic as updateFromAtomics)
    auto formatDb = [] (float val) -> juce::String
    {
        if (!std::isfinite (val) || val <= -100.0f) return "-inf";
        return juce::String (val, 1) + " dB";
    };
    
    numericTextPeak_ = formatDb (maxPeakDb_);
    numericTextRms_  = formatDb (maxRmsDb_);
    
    repaint();
}

void MeterComponent::setDisplayMode (DisplayMode mode)
{
    if (displayMode_ == mode)
        return;

    displayMode_ = mode;
    updateFromAtomics();
    repaint();
}

void MeterComponent::setScaleMode (ScaleMode mode)
{
    if (scaleMode_ == mode)
        return;
    scaleMode_ = mode;
    cachedPeakNorm_ = dbToNorm (cachedPeakDb_);
    cachedRmsNorm_  = dbToNorm (cachedRmsDb_);
    maxPeakNorm_    = dbToNorm (maxPeakDb_);
    repaint();
}

float MeterComponent::getScaleMinDb() const noexcept
{
    switch (scaleMode_)
    {
        case ScaleMode::FullRange: return kFullRangeMinDb;
        case ScaleMode::Top24Db:   return -18.0f;
        case ScaleMode::Top12Db:   return -6.0f;
    }
    return kFullRangeMinDb;
}

float MeterComponent::getScaleMaxDb() const noexcept
{
    switch (scaleMode_)
    {
        case ScaleMode::FullRange: return kFullRangeMaxDb; // +6 dB
        case ScaleMode::Top24Db:
        case ScaleMode::Top12Db:   return 0.0f;            // zoom until 0 dBFS
    }
    return kFullRangeMaxDb;
}

float MeterComponent::clampForRenderDb (float db) const noexcept
{
    if (! std::isfinite (db))
        return getScaleMinDb();
    return juce::jlimit (getScaleMinDb(), kFullRangeMaxDb, db);
}

float MeterComponent::dbToNorm (float db) const noexcept
{
    const float minDb = getScaleMinDb();
    const float clamped = clampForRenderDb (db);
    if (scaleMode_ == ScaleMode::FullRange)
    {
        return (clamped - minDb) / (kFullRangeMaxDb - minDb);
    }
    // Zoomed modes: scale ends at 0 dBFS; top 10% reserved for over (0 to +6 dB)
    const float scaleEndDb = 0.0f;
    if (clamped <= scaleEndDb)
        return 0.9f * (clamped - minDb) / (scaleEndDb - minDb);
    return 0.9f + 0.1f * (juce::jmin (clamped, kFullRangeMaxDb) / kFullRangeMaxDb);
}

void MeterComponent::updateFromAtomics()
{
    if (peakDb_ == nullptr || rmsDb_ == nullptr || clipLatched_ == nullptr)
        return;

    const float peakDb = peakDb_->load (std::memory_order_relaxed);
    const float rmsDb  = rmsDb_->load (std::memory_order_relaxed);
    const bool clip    = clipLatched_->load (std::memory_order_relaxed);

    const float peakDbClamped = clampForRenderDb (peakDb);
    float rmsDbClamped = clampForRenderDb (rmsDb);
    rmsDbClamped = juce::jmin (rmsDbClamped, peakDbClamped);

    const bool changed =
        (std::abs (peakDbClamped - cachedPeakDb_) > 0.05f) ||
        (std::abs (rmsDbClamped - cachedRmsDb_) > 0.05f) ||
        (clip != cachedClip_);

    if (! changed)
        return;

    cachedPeakDb_ = peakDbClamped;
    cachedRmsDb_  = rmsDbClamped;
    cachedClip_   = clip;

    cachedPeakNorm_ = dbToNorm (cachedPeakDb_);
    cachedRmsNorm_  = dbToNorm (cachedRmsDb_);

    // Update Max Holds
    if (peakDb > maxPeakDb_) maxPeakDb_ = peakDb;
    if (rmsDb > maxRmsDb_)   maxRmsDb_  = rmsDb;
    if (peakDbClamped >= maxPeakDb_)
        lastTimePeakAtOrAboveMax_ = juce::Time::getMillisecondCounter();

    // Formatting helper
    auto formatDb = [] (float val) -> juce::String
    {
        if (!std::isfinite (val) || val <= -100.0f) return "-inf";
        return juce::String (val, 1) + " dB";
    };

    numericTextPeak_ = formatDb (maxPeakDb_);
    numericTextRms_  = formatDb (maxRmsDb_);

    repaint();
}

void MeterComponent::timerCallback()
{
    if (holdEnabled_)
        return;
    const juce::int64 now = juce::Time::getMillisecondCounter();
    if (cachedPeakDb_ >= maxPeakDb_)
    {
        lastTimePeakAtOrAboveMax_ = now;
        return;
    }
    if ((now - lastTimePeakAtOrAboveMax_) < kPeakHoldTimeMs)
        return;
    const float decayPerFrame = kPeakDecayDbPerSec / static_cast<float> (kPeakDecayTimerHz);
    const bool peakDecayed = maxPeakDb_ > cachedPeakDb_;
    const bool rmsDecayed  = maxRmsDb_ > cachedRmsDb_;
    if (peakDecayed)
        maxPeakDb_ = juce::jmax (cachedPeakDb_, maxPeakDb_ - decayPerFrame);
    if (rmsDecayed)
        maxRmsDb_  = juce::jmax (cachedRmsDb_,  maxRmsDb_  - decayPerFrame);
    if (!peakDecayed && !rmsDecayed)
        return;
    maxPeakNorm_ = dbToNorm (maxPeakDb_);
    auto formatDb = [] (float val) -> juce::String
    {
        if (!std::isfinite (val) || val <= -100.0f) return "-inf";
        return juce::String (val, 1) + " dB";
    };
    numericTextPeak_ = formatDb (maxPeakDb_);
    numericTextRms_  = formatDb (maxRmsDb_);
    repaint();
}

void MeterComponent::resetPeakHold()
{
    // Reset max hold to current instantaneous values
    maxPeakDb_ = cachedPeakDb_;
    maxRmsDb_  = cachedRmsDb_;
    maxPeakNorm_ = cachedPeakNorm_;
    lastTimePeakAtOrAboveMax_ = juce::Time::getMillisecondCounter();
    
    // Update numeric text
    auto formatDb = [] (float val) -> juce::String
    {
        if (!std::isfinite (val) || val <= -100.0f) return "-inf";
        return juce::String (val, 1) + " dB";
    };
    
    numericTextPeak_ = formatDb (maxPeakDb_);
    numericTextRms_  = formatDb (maxRmsDb_);
    
    repaint();
}

void MeterComponent::setHoldEnabled (bool hold)
{
    holdEnabled_ = hold;
    
    // When hold is disabled, reset to current live values
    if (!holdEnabled_)
    {
        resetPeakHold();
    }
}

void MeterComponent::mouseDown (const juce::MouseEvent& e)
{
    // Clip Reset (Global)
    if (ledArea_.contains (e.getPosition()))
    {
        if (onClipReset)
            onClipReset();
        return;
    }

    // Peak Reset (Linked L/R)
    if (onPeakReset)
    {
        onPeakReset();
    }
    else
    {
        // Fallback if no callback set
        resetPeakHold();
    }
}

void MeterComponent::resized()
{
    auto b = getLocalBounds();

    labelArea_ = b.removeFromTop (16);
    numericArea_ = b.removeFromBottom (20).reduced (2, 2);

    // Reserve a small LED dot inside the label row.
    ledArea_ = labelArea_.removeFromRight (14).withSizeKeepingCentre (10, 10);

    meterArea_ = b.reduced (6, 2);
}

void MeterComponent::paint (juce::Graphics& g)
{
    const auto& theme = ui_.theme();
    const auto& m = ui_.metrics();

    // Meter background track
    g.setColour (theme.panel.withAlpha (0.9f));
    g.fillRoundedRectangle (meterArea_.toFloat(), m.rSmall);

    // Clip Zone Background (> 0dB)
    const float norm0 = dbToNorm (0.0f);
    const float y0 = static_cast<float> (meterArea_.getBottom()) - (norm0 * static_cast<float> (meterArea_.getHeight()));
    const float yTop = static_cast<float> (meterArea_.getY());
    
    if (y0 > yTop)
    {
        // Fill Red Zone
        g.setColour (theme.danger.withAlpha (0.15f));
        g.fillRect (static_cast<float> (meterArea_.getX()), yTop, 
                    static_cast<float> (meterArea_.getWidth()), y0 - yTop);
    }

    g.setColour (theme.background.withAlpha (0.65f));
    g.drawRoundedRectangle (meterArea_.toFloat(), m.rSmall, m.strokeThin);

    const float yMax = static_cast<float> (meterArea_.getBottom());
    const float h    = static_cast<float> (meterArea_.getHeight());
    const float xLeft = static_cast<float> (meterArea_.getX());
    const float xRight = static_cast<float> (meterArea_.getRight());
    const float width = static_cast<float> (meterArea_.getWidth());

    // Determine Main Bar Level based on mode
    float mainNorm = 0.0f;
    if (displayMode_ == DisplayMode::Peak)
        mainNorm = cachedPeakNorm_;
    else
        mainNorm = cachedRmsNorm_; // RMS Mode

    // Main Body
    const float mainH = mainNorm * h;
    const float mainTop = yMax - mainH;
    
    if (mainH > 0.5f)
    {
        auto mainRect = meterArea_.withTop (static_cast<int> (std::round (mainTop)));
        g.setColour (theme.accent.withAlpha (0.85f));
        g.fillRoundedRectangle (mainRect.toFloat(), m.rSmall);
    }

    // Range Fill & Peak Cap (Logic varies by mode)
    float peakTop = yMax - (cachedPeakNorm_ * h);
    
    if (displayMode_ == DisplayMode::RMS)
    {
        // RMS Mode: Show separate Peak cap and range fill
        if (cachedPeakNorm_ > cachedRmsNorm_)
        {
            // Fill the gap
            g.setColour (theme.accent.withAlpha (0.3f));
            g.fillRect (xLeft + 2.0f, 
                        peakTop, 
                        width - 4.0f, 
                        mainTop - peakTop);
        }
        
        // Peak cap (thin line)
        g.setColour (theme.seriesPeak.withAlpha (0.95f));
        g.drawLine (xLeft + m.strokeThick,
                    peakTop,
                    xRight - m.strokeThick,
                    peakTop,
                    m.strokeMed);
    }
    else
    {
        // Peak Mode: Bar is already peak. Just draw cap at bar top for definition or skip.
        // Let's draw the cap at the bar top for visual consistency
        g.setColour (theme.seriesPeak.withAlpha (0.95f));
        g.drawLine (xLeft + m.strokeThick,
                    mainTop,
                    xRight - m.strokeThick,
                    mainTop,
                    m.strokeMed);
    }

    // Max Peak Hold Marker (session maximum - resets on click)
    if (maxPeakNorm_ > 0.001f)
    {
        const float maxPeakY = yMax - (maxPeakNorm_ * h);
        g.setColour (theme.warning.withAlpha (0.9f));  // Yellow/orange for max hold
        g.drawLine (xLeft + 1.0f,
                    maxPeakY,
                    xRight - 1.0f,
                    maxPeakY,
                    1.5f);
    }

    // Draw dB scale on top so it is visible at all times (scale-dependent ticks)
    constexpr float kDbScaleFontHeight = 10.0f;
    constexpr float kDbScaleLineThin = 1.25f;
    constexpr float kDbScaleLine0Db = 1.75f;
    constexpr float kDbScaleLineDense = 0.75f;  // thinner for dense zoom ticks
    g.setFont (ui_.type().labelFont().withHeight (kDbScaleFontHeight));
    constexpr float kLabelEps = 0.001f;
    auto nearTick = [] (float val, float target) noexcept {
        return std::abs (val - target) < kLabelEps;
    };

    if (scaleMode_ == ScaleMode::Top24Db)
    {
        // 24 scale: mark every 1 dB from 0 to -18; label every 3 dB
        for (int i = 0; i <= 18; ++i)
        {
            const float db = -static_cast<float> (i);
            const float norm = dbToNorm (db);
            const float y = yMax - (norm * h);
            const bool isZero = (i == 0);
            const float lineW = isZero ? kDbScaleLine0Db : kDbScaleLineDense;
            g.setColour (isZero ? theme.text.withAlpha (0.95f) : theme.text.withAlpha (0.6f));
            g.drawLine (xLeft, y, xRight, y, lineW);
            const bool shouldLabel = (i % 3) == 0;  // 0, -3, -6, -9, -12, -15, -18
            if (shouldLabel)
            {
                g.setColour (theme.text);
                g.drawText (juce::String (static_cast<int> (db)),
                            juce::Rectangle<float> (xLeft, y - 5.0f, width, 10.0f),
                            juce::Justification::centred);
            }
        }
    }
    else if (scaleMode_ == ScaleMode::Top12Db)
    {
        // 12 scale: mark every 0.5 dB from 0 to -6; label every 1 dB
        for (int i = 0; i <= 12; ++i)
        {
            const float db = -i * 0.5f;
            const float norm = dbToNorm (db);
            const float y = yMax - (norm * h);
            const bool isZero = (i == 0);
            const float lineW = isZero ? kDbScaleLine0Db : kDbScaleLineDense;
            g.setColour (isZero ? theme.text.withAlpha (0.95f) : theme.text.withAlpha (0.6f));
            g.drawLine (xLeft, y, xRight, y, lineW);
            const bool shouldLabel = (i % 2) == 0;  // 0, -1, -2, -3, -4, -5, -6
            if (shouldLabel)
            {
                g.setColour (theme.text);
                const bool isInteger = std::abs (db - std::floor (db)) < kLabelEps;
                juce::String label = isInteger ? juce::String (static_cast<int> (db))
                                               : juce::String (db, 1);
                g.drawText (label, juce::Rectangle<float> (xLeft, y - 5.0f, width, 10.0f),
                            juce::Justification::centred);
            }
        }
    }
    else
    {
        // Full range: sparse ticks as before
        static constexpr float kTicksFull[] = { 6.0f, 0.0f, -6.0f, -12.0f, -24.0f, -48.0f, -72.0f, -96.0f, -120.0f };
        constexpr int numTicks = 9;
        for (int i = 0; i < numTicks; ++i)
        {
            const float db = kTicksFull[i];
            const float norm = dbToNorm (db);
            const float y = yMax - (norm * h);
            const float lineW = nearTick (db, 0.0f) ? kDbScaleLine0Db : kDbScaleLineThin;
            if (nearTick (db, 0.0f))
            {
                g.setColour (theme.text.withAlpha (0.95f));
                g.drawLine (xLeft, y, xRight, y, lineW);
            }
            else if (db > 0.0f)
            {
                g.setColour (theme.danger.withAlpha (0.85f));
                g.drawLine (xLeft, y, xRight, y, lineW);
            }
            else
            {
                g.setColour (theme.text.withAlpha (0.75f));
                g.drawLine (xLeft, y, xRight, y, lineW);
            }
            const bool shouldLabel = nearTick (db, 6.0f) || nearTick (db, 0.0f)
                || nearTick (db, -12.0f) || nearTick (db, -24.0f) || nearTick (db, -18.0f)
                || nearTick (db, -48.0f) || nearTick (db, -96.0f) || nearTick (db, -6.0f);
            if (shouldLabel)
            {
                g.setColour (db >= 0.0f ? theme.danger : theme.text);
                const int dbInt = static_cast<int> (std::lround (static_cast<double> (db)));
                juce::String label = juce::String (dbInt);
                if (db > 0.0f) label = "+" + label;
                g.drawText (label, juce::Rectangle<float> (xLeft, y - 5.0f, width, 10.0f), juce::Justification::centred);
            }
        }
    }

    // Channel label
    g.setColour (theme.text.withAlpha (0.9f));
    g.setFont (ui_.type().labelFont());
    g.drawText (label_, labelArea_, juce::Justification::centred);

    // Clip LED (latching)
    const auto ledColour = cachedClip_ ? theme.danger : theme.textMuted.withAlpha (0.25f);
    g.setColour (ledColour);
    g.fillEllipse (ledArea_.toFloat());
    g.setColour (theme.background.withAlpha (0.7f));
    g.drawEllipse (ledArea_.toFloat(), m.strokeThin);

    // Numeric readout box (Dual value)
    const auto boxR = numericArea_.toFloat();
    g.setColour (theme.background.withAlpha (0.55f));
    g.fillRoundedRectangle (boxR, m.rMed);
    g.setColour (theme.grid.withAlpha (0.35f));
    g.drawRoundedRectangle (boxR, m.rMed, m.strokeThin);

    g.setFont (juce::Font (juce::FontOptions().withHeight (10.0f)));
    
    // Top: Peak, Bottom: RMS
    auto numBounds = numericArea_;
    auto peakBounds = numBounds.removeFromTop (numBounds.getHeight() / 2);
    auto rmsBounds = numBounds;
    
    g.setColour (theme.seriesPeak.withAlpha (0.9f));
    g.drawText (numericTextPeak_, peakBounds, juce::Justification::centred);
    
    g.setColour (theme.accent.withAlpha (0.9f));
    g.drawText (numericTextRms_, rmsBounds, juce::Justification::centred);
    
    // Bypass Overlay
    if (isBypassed_)
    {
        g.setColour (theme.background.withAlpha (0.7f));
        g.fillRoundedRectangle (meterArea_.toFloat(), m.rSmall);
        
        g.setColour (theme.danger);
        g.setFont (ui_.type().labelFont().withHeight (10.0f).boldened());
        
        g.drawText ("BYPASS", meterArea_, juce::Justification::centred);
    }
}
