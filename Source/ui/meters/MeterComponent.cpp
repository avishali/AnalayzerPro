#include "MeterComponent.h"
#include <mdsp_ui/UiContext.h>
#include <cmath>
#include <utility>

namespace
{
    constexpr float kMeterMinDb = -90.0f;
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
    numericText_ = utf8 ("—");
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

    const float valueForNumericDb = (displayMode_ == DisplayMode::Peak) ? peakDb : rmsDb;
    if (! std::isfinite (valueForNumericDb) || valueForNumericDb <= kMeterMinDb)
    {
        numericText_ = utf8 ("—");
    }
    else
    {
        numericText_ = juce::String (valueForNumericDb, 1) + " dB";
    }

    repaint();
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

    // RMS body (dominant)
    const float rmsH = cachedRmsNorm_ * static_cast<float> (meterArea_.getHeight());
    if (rmsH > 0.5f)
    {
        auto rmsRect = meterArea_.withTop (meterArea_.getBottom() - static_cast<int> (std::round (rmsH)));
        g.setColour (theme.accent.withAlpha (0.85f));
        g.fillRoundedRectangle (rmsRect.toFloat(), m.rSmall);
    }

    // Peak cap (thin line)
    const float peakY = static_cast<float> (meterArea_.getBottom()) - (cachedPeakNorm_ * static_cast<float> (meterArea_.getHeight()));
    g.setColour (theme.seriesPeak.withAlpha (0.95f));
    g.drawLine (static_cast<float> (meterArea_.getX()) + m.strokeThick,
                peakY,
                static_cast<float> (meterArea_.getRight()) - m.strokeThick,
                peakY,
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

    // Numeric readout box
    const auto boxR = numericArea_.toFloat();
    g.setColour (theme.background.withAlpha (0.55f));
    g.fillRoundedRectangle (boxR, m.rMed);
    g.setColour (theme.grid.withAlpha (0.35f));
    g.drawRoundedRectangle (boxR, m.rMed, m.strokeThin);

    g.setColour (theme.text.withAlpha (0.85f));
    g.setFont (juce::Font (juce::FontOptions().withHeight (ui_.type().labelH - 0.5f)));
    g.drawText (numericText_, numericArea_, juce::Justification::centred);
}

