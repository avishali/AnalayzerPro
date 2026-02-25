#include "MeterComponent.h"

#include <mdsp_ui/UiContext.h>
#include <mdsp_ui/meters/MeterRenderStateProvider.h>

#include <cmath>
#include <utility>

namespace
{
constexpr float kDbScaleFontHeight = 10.0f;
constexpr float kDbScaleLineThin = 1.25f;
constexpr float kDbScaleLine0Db = 1.75f;
constexpr float kDbScaleLineDense = 0.75f;

static bool nearTick (float value, float target) noexcept
{
    return std::abs (value - target) < 0.001f;
}
}

MeterComponent::MeterComponent (mdsp_ui::UiContext& ui, juce::String labelText)
    : ui_ (ui),
      label_ (std::move (labelText))
{
    setOpaque (false);
}

void MeterComponent::setLabelText (juce::String labelText)
{
    if (label_ == labelText)
        return;

    label_ = std::move (labelText);
    repaint();
}

void MeterComponent::setRenderState (const MeterRenderState& state)
{
    renderState_ = state;
    numericTextPeak_ = renderState_.peakText.data();
    numericTextRms_ = renderState_.rmsText.data();
    repaint();
}

void MeterComponent::setClipResetCallback (Callback cb, void* ctx) noexcept
{
    onClipReset_ = cb;
    onClipResetCtx_ = ctx;
}

void MeterComponent::setPeakResetCallback (Callback cb, void* ctx) noexcept
{
    onPeakReset_ = cb;
    onPeakResetCtx_ = ctx;
}

float MeterComponent::dbToNormForScale (float db, mdsp_ui::meters::MeterScaleMode mode) noexcept
{
    return mdsp_ui::meters::MeterRenderStateProvider::normaliseDb (db, mode);
}

void MeterComponent::mouseDown (const juce::MouseEvent& e)
{
    if (ledArea_.contains (e.getPosition()))
    {
        if (onClipReset_ != nullptr)
            onClipReset_ (onClipResetCtx_);
        return;
    }

    if (onPeakReset_ != nullptr)
        onPeakReset_ (onPeakResetCtx_);
}

void MeterComponent::resized()
{
    auto b = getLocalBounds();

    labelArea_ = b.removeFromTop (16);
    numericArea_ = b.removeFromBottom (20).reduced (2, 2);

    ledArea_ = labelArea_.removeFromRight (14).withSizeKeepingCentre (10, 10);
    meterArea_ = b.reduced (6, 2);
}

void MeterComponent::paint (juce::Graphics& g)
{
    const auto& theme = ui_.theme();
    const auto& m = ui_.metrics();

    g.setColour (theme.panel.withAlpha (0.9f));
    g.fillRoundedRectangle (meterArea_.toFloat(), m.rSmall);

    const float norm0 = dbToNormForScale (0.0f, renderState_.scaleMode);
    const float y0 = static_cast<float> (meterArea_.getBottom()) - (norm0 * static_cast<float> (meterArea_.getHeight()));
    const float yTop = static_cast<float> (meterArea_.getY());

    if (y0 > yTop)
    {
        g.setColour (theme.danger.withAlpha (0.15f));
        g.fillRect (static_cast<float> (meterArea_.getX()),
                    yTop,
                    static_cast<float> (meterArea_.getWidth()),
                    y0 - yTop);
    }

    g.setColour (theme.background.withAlpha (0.65f));
    g.drawRoundedRectangle (meterArea_.toFloat(), m.rSmall, m.strokeThin);

    const float yMax = static_cast<float> (meterArea_.getBottom());
    const float h = static_cast<float> (meterArea_.getHeight());
    const float xLeft = static_cast<float> (meterArea_.getX());
    const float xRight = static_cast<float> (meterArea_.getRight());
    const float width = static_cast<float> (meterArea_.getWidth());

    const float mainNorm = (renderState_.displayMode == mdsp_ui::meters::MeterDisplayMode::Peak)
                               ? renderState_.peakNorm
                               : renderState_.rmsNorm;

    const float mainH = mainNorm * h;
    const float mainTop = yMax - mainH;

    if (mainH > 0.5f)
    {
        auto mainRect = meterArea_.withTop (static_cast<int> (std::round (mainTop)));
        g.setColour (theme.accent.withAlpha (0.85f));
        g.fillRoundedRectangle (mainRect.toFloat(), m.rSmall);
    }

    const float peakTop = yMax - (renderState_.peakNorm * h);

    if (renderState_.displayMode == mdsp_ui::meters::MeterDisplayMode::Rms)
    {
        if (renderState_.peakNorm > renderState_.rmsNorm)
        {
            g.setColour (theme.accent.withAlpha (0.3f));
            g.fillRect (xLeft + 2.0f,
                        peakTop,
                        width - 4.0f,
                        mainTop - peakTop);
        }

        g.setColour (theme.seriesPeak.withAlpha (0.95f));
        g.drawLine (xLeft + m.strokeThick,
                    peakTop,
                    xRight - m.strokeThick,
                    peakTop,
                    m.strokeMed);
    }
    else
    {
        g.setColour (theme.seriesPeak.withAlpha (0.95f));
        g.drawLine (xLeft + m.strokeThick,
                    mainTop,
                    xRight - m.strokeThick,
                    mainTop,
                    m.strokeMed);
    }

    if (renderState_.maxPeakNorm > 0.001f)
    {
        const float maxPeakY = yMax - (renderState_.maxPeakNorm * h);
        g.setColour (theme.warning.withAlpha (0.9f));
        g.drawLine (xLeft + 1.0f,
                    maxPeakY,
                    xRight - 1.0f,
                    maxPeakY,
                    1.5f);
    }

    g.setFont (ui_.type().labelFont().withHeight (kDbScaleFontHeight));

    if (renderState_.scaleMode == mdsp_ui::meters::MeterScaleMode::Top24Db)
    {
        for (int i = 0; i <= 18; ++i)
        {
            const float db = -static_cast<float> (i);
            const float norm = dbToNormForScale (db, renderState_.scaleMode);
            const float y = yMax - (norm * h);
            const bool isZero = (i == 0);
            g.setColour (isZero ? theme.text.withAlpha (0.95f) : theme.text.withAlpha (0.6f));
            g.drawLine (xLeft, y, xRight, y, isZero ? kDbScaleLine0Db : kDbScaleLineDense);

            if ((i % 3) == 0)
            {
                const char* label = nullptr;
                switch (i)
                {
                    case 0: label = "0"; break;
                    case 3: label = "-3"; break;
                    case 6: label = "-6"; break;
                    case 9: label = "-9"; break;
                    case 12: label = "-12"; break;
                    case 15: label = "-15"; break;
                    case 18: label = "-18"; break;
                    default: break;
                }
                if (label != nullptr)
                {
                    g.setColour (theme.text);
                    g.drawText (label,
                                juce::Rectangle<float> (xLeft, y - 5.0f, width, 10.0f),
                                juce::Justification::centred);
                }
            }
        }
    }
    else if (renderState_.scaleMode == mdsp_ui::meters::MeterScaleMode::Top12Db)
    {
        for (int i = 0; i <= 12; ++i)
        {
            const float db = -i * 0.5f;
            const float norm = dbToNormForScale (db, renderState_.scaleMode);
            const float y = yMax - (norm * h);
            const bool isZero = (i == 0);
            g.setColour (isZero ? theme.text.withAlpha (0.95f) : theme.text.withAlpha (0.6f));
            g.drawLine (xLeft, y, xRight, y, isZero ? kDbScaleLine0Db : kDbScaleLineDense);

            if ((i % 2) == 0)
            {
                const char* label = nullptr;
                switch (i)
                {
                    case 0: label = "0"; break;
                    case 2: label = "-1"; break;
                    case 4: label = "-2"; break;
                    case 6: label = "-3"; break;
                    case 8: label = "-4"; break;
                    case 10: label = "-5"; break;
                    case 12: label = "-6"; break;
                    default: break;
                }
                if (label != nullptr)
                {
                    g.setColour (theme.text);
                    g.drawText (label,
                                juce::Rectangle<float> (xLeft, y - 5.0f, width, 10.0f),
                                juce::Justification::centred);
                }
            }
        }
    }
    else
    {
        static constexpr float kTicksFull[] = { 6.0f, 0.0f, -6.0f, -12.0f, -24.0f, -48.0f, -72.0f, -96.0f, -120.0f };
        for (const auto db : kTicksFull)
        {
            const float norm = dbToNormForScale (db, renderState_.scaleMode);
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

            const char* label = nullptr;
            if (nearTick (db, 6.0f)) label = "+6";
            else if (nearTick (db, 0.0f)) label = "0";
            else if (nearTick (db, -6.0f)) label = "-6";
            else if (nearTick (db, -12.0f)) label = "-12";
            else if (nearTick (db, -24.0f)) label = "-24";
            else if (nearTick (db, -48.0f)) label = "-48";
            else if (nearTick (db, -96.0f)) label = "-96";

            if (label != nullptr)
            {
                g.setColour (db >= 0.0f ? theme.danger : theme.text);
                g.drawText (label,
                            juce::Rectangle<float> (xLeft, y - 5.0f, width, 10.0f),
                            juce::Justification::centred);
            }
        }
    }

    g.setColour (theme.text.withAlpha (0.9f));
    g.setFont (ui_.type().labelFont());
    g.drawText (label_, labelArea_, juce::Justification::centred);

    const auto ledColour = renderState_.clipLatched ? theme.danger : theme.textMuted.withAlpha (0.25f);
    g.setColour (ledColour);
    g.fillEllipse (ledArea_.toFloat());
    g.setColour (theme.background.withAlpha (0.7f));
    g.drawEllipse (ledArea_.toFloat(), m.strokeThin);

    const auto boxR = numericArea_.toFloat();
    g.setColour (theme.background.withAlpha (0.55f));
    g.fillRoundedRectangle (boxR, m.rMed);
    g.setColour (theme.grid.withAlpha (0.35f));
    g.drawRoundedRectangle (boxR, m.rMed, m.strokeThin);

    g.setFont (juce::Font (juce::FontOptions().withHeight (10.0f)));

    auto numBounds = numericArea_;
    auto peakBounds = numBounds.removeFromTop (numBounds.getHeight() / 2);
    auto rmsBounds = numBounds;

    g.setColour (theme.seriesPeak.withAlpha (0.9f));
    g.drawText (numericTextPeak_, peakBounds, juce::Justification::centred);

    g.setColour (theme.accent.withAlpha (0.9f));
    g.drawText (numericTextRms_, rmsBounds, juce::Justification::centred);

    if (renderState_.bypassed)
    {
        g.setColour (theme.background.withAlpha (0.7f));
        g.fillRoundedRectangle (meterArea_.toFloat(), m.rSmall);

        g.setColour (theme.danger);
        g.setFont (ui_.type().labelFont().withHeight (10.0f).boldened());
        g.drawText ("BYPASS", meterArea_, juce::Justification::centred);
    }
}
