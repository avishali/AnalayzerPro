#include "MeterComponent.h"
#include <mdsp_ui/UiContext.h>
#include <cmath>
#include <utility>

namespace
{
    constexpr float kMeterMinDb = -60.0f;
    constexpr float kMeterMaxDb = 0.0f;

    static inline juce::String utf8 (const char* s)
    {
        return juce::String (juce::CharPointer_UTF8 (s));
    }
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

void MeterComponent::mouseDown (const juce::MouseEvent&)
{
    // Reset max values to current
    if (peakDb_ && rmsDb_)
    {
        maxPeakDb_ = peakDb_->load (std::memory_order_relaxed);
        maxRmsDb_  = rmsDb_->load (std::memory_order_relaxed);
        updateFromAtomics(); // Force update
        repaint();
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

    g.setColour (theme.background.withAlpha (0.65f));
    g.drawRoundedRectangle (meterArea_.toFloat(), m.rSmall, m.strokeThin);
    
    // Draw dB Scale
    g.setColour (theme.textMuted.withAlpha (0.4f));
    g.setFont (ui_.type().labelFont().withHeight (9.0f));
    
    constexpr float dbTicks[] = { 0.0f, -6.0f, -12.0f, -24.0f, -36.0f, -48.0f };
    const float yMax = static_cast<float> (meterArea_.getBottom());
    const float h    = static_cast<float> (meterArea_.getHeight());
    
    for (float db : dbTicks)
    {
        const float norm = dbToNorm (db);
        const float y = yMax - (norm * h);
        
        // Tick mark
        g.drawLine (static_cast<float> (meterArea_.getX()), y, static_cast<float> (meterArea_.getRight()), y, 1.0f);
        
        // Label (skip 0 to avoid clutter if desired, or keep)
        // Draw centered
        // g.drawText (juce::String (static_cast<int>(db)), ...); 
        // Small detail: maybe just lines for cleanliness or tiny numbers if space permits?
        // User asked for "draw a db scale". I'll add lines.
    }

    // RMS body (dominant)
    const float rmsH = cachedRmsNorm_ * h;
    const float rmsTop = yMax - rmsH;
    
    if (rmsH > 0.5f)
    {
        auto rmsRect = meterArea_.withTop (static_cast<int> (std::round (rmsTop)));
        g.setColour (theme.accent.withAlpha (0.85f));
        g.fillRoundedRectangle (rmsRect.toFloat(), m.rSmall);
    }

    // Range Fill (Peak - RMS)
    // Draw from Peak Top down to RMS Top
    const float peakTop = yMax - (cachedPeakNorm_ * h);
    if (cachedPeakNorm_ > cachedRmsNorm_)
    {
        // Fill the gap
        g.setColour (theme.accent.withAlpha (0.3f));
        g.fillRect (static_cast<float> (meterArea_.getX()) + 2.0f, 
                    peakTop, 
                    static_cast<float> (meterArea_.getWidth()) - 4.0f, 
                    rmsTop - peakTop);
    }

    // Peak cap (thin line)
    g.setColour (theme.seriesPeak.withAlpha (0.95f));
    g.drawLine (static_cast<float> (meterArea_.getX()) + m.strokeThick,
                peakTop,
                static_cast<float> (meterArea_.getRight()) - m.strokeThick,
                peakTop,
                m.strokeMed);

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
}

