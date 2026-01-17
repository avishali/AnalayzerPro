#include "MeterComponent.h"
#include <mdsp_ui/UiContext.h>
#include <cmath>
#include <utility>

namespace
{
    constexpr float kMeterMinDb = -120.0f;
    constexpr float kMeterMaxDb = 6.0f;
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
    
    // Handle visual decay/hold
    if (cachedPeakDb_ > maxPeakDb_) maxPeakDb_ = cachedPeakDb_;
    if (cachedRmsDb_ > maxRmsDb_)   maxRmsDb_  = cachedRmsDb_;
    
    // Convert for rendering
    cachedPeakNorm_ = dbToNorm (cachedPeakDb_);
    cachedRmsNorm_  = dbToNorm (cachedRmsDb_);
    
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

float MeterComponent::clampForRenderDb (float db) noexcept
{
    if (! std::isfinite (db))
        return kMeterMinDb;
    return juce::jlimit (kMeterMinDb, kMeterMaxDb, db);
}

float MeterComponent::dbToNorm (float db) noexcept
{
    const float clamped = clampForRenderDb (db);
    return (clamped - kMeterMinDb) / (kMeterMaxDb - kMeterMinDb);
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

void MeterComponent::resetPeakHold()
{
    if (peakDb_ && rmsDb_)
    {
        maxPeakDb_ = peakDb_->load (std::memory_order_relaxed);
        maxRmsDb_  = rmsDb_->load (std::memory_order_relaxed);
        updateFromAtomics(); // Force update
        repaint();
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
    
    // Draw dB Scale
    g.setColour (theme.textMuted.withAlpha (0.5f));
    g.setFont (ui_.type().labelFont().withHeight (8.0f)); // Smaller font for dense scale
    
    constexpr float dbTicks[] = { 6.0f, 0.0f, -6.0f, -12.0f, -24.0f, -48.0f, -72.0f, -96.0f, -120.0f };
    const float yMax = static_cast<float> (meterArea_.getBottom());
    const float h    = static_cast<float> (meterArea_.getHeight());
    const float xLeft = static_cast<float> (meterArea_.getX());
    const float xRight = static_cast<float> (meterArea_.getRight());
    const float width = static_cast<float> (meterArea_.getWidth());
    
    for (float db : dbTicks)
    {
        const float norm = dbToNorm (db);
        const float y = yMax - (norm * h);
        
        // Tick mark
        if (std::abs(db) < 0.001f)
        {
            // 0 dB Line - Emphasized
            g.setColour (theme.text.withAlpha (0.6f));
            g.drawLine (xLeft, y, xRight, y, 1.0f);
        }
        else if (db > 0.0f)
        {
            // Positive ticks - Warmer
            g.setColour (theme.danger.withAlpha (0.4f));
            g.drawLine (xLeft, y, xRight, y, 1.0f);
        }
        else
        {
            // Negative ticks
            g.setColour (theme.textMuted.withAlpha (0.3f));
            g.drawLine (xLeft, y, xRight, y, 1.0f);
        }

        // Labels (if space allows)
        // Draw centered small numbers
        // Only label select values to avoid clutter
        if (db == 6.0f || db == 0.0f || db == -12.0f || db == -24.0f || db == -48.0f || db == -96.0f) 
        {
            g.setColour (db >= 0.0f ? theme.danger.withAlpha(0.8f) : theme.textMuted.withAlpha(0.8f));
            juce::String label = juce::String (static_cast<int> (db));
            if (db > 0) label = "+" + label;
            g.drawText (label, xLeft, y - 4.0f, width, 8.0f, juce::Justification::centred);
        }
    }

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
