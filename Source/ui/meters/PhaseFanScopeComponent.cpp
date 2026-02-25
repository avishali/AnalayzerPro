#include "PhaseFanScopeComponent.h"

#include <cmath>

namespace
{
constexpr float kPadding = 10.0f;
constexpr float kContourMinDraw = 0.003f;
constexpr float kPeakMinDraw = 0.003f;
constexpr float kInnerProportion = 0.12f;
constexpr float kPiHalf = juce::MathConstants<float>::halfPi;
}

PhaseFanScopeComponent::PhaseFanScopeComponent (mdsp_ui::UiContext& ui)
    : ui_ (ui)
{
}

void PhaseFanScopeComponent::setRenderState (const mdsp_ui::scopes::PhaseFanRenderState& state)
{
    state_ = state;
    rebuildCachedPaths();
    repaint();
}

void PhaseFanScopeComponent::resized()
{
    rebuildCachedPaths();
}

void PhaseFanScopeComponent::rebuildCachedPaths()
{
    fanFillPath_.clear();
    contourPath_.clear();
    peakHoldPath_.clear();

    const auto bounds = getLocalBounds().toFloat();
    if (bounds.getWidth() < 2.0f || bounds.getHeight() < 2.0f)
        return;

    const auto padded = bounds.reduced (kPadding);
    if (padded.getWidth() < 2.0f || padded.getHeight() < 2.0f)
        return;

    const float radiusPx = juce::jmin (padded.getWidth() * 0.5f, padded.getHeight());
    if (radiusPx <= 0.0f)
        return;

    const float cx = padded.getCentreX();
    const float cy = padded.getBottom();

    const float rMin = radiusPx * 0.02f;
    const float rMax = radiusPx;

    auto rNormToPx = [rMin, rMax] (float rNorm)
    {
        return juce::jmap (juce::jlimit (0.0f, 1.0f, rNorm), 0.0f, 1.0f, rMin, rMax);
    };

    auto thetaForAngleBin = [] (int a)
    {
        return juce::jmap (static_cast<float> (a),
                           0.0f,
                           static_cast<float> (mdsp_ui::scopes::PhaseFanRenderState::kAngleBins - 1),
                           -kPiHalf,
                           kPiHalf);
    };

    auto pointFor = [cx, cy, &rNormToPx, &thetaForAngleBin] (int a, float rNorm)
    {
        const float theta = thetaForAngleBin (a);
        const float rPx = rNormToPx (rNorm);
        return juce::Point<float> (cx + std::sin (theta) * rPx,
                                   cy - std::cos (theta) * rPx);
    };

    const bool drawDots = (state_.renderMode == mdsp_ui::scopes::PhaseFanRenderMode::Dots
                        || state_.renderMode == mdsp_ui::scopes::PhaseFanRenderMode::Both);
    const bool drawLines = (state_.renderMode == mdsp_ui::scopes::PhaseFanRenderMode::Lines
                         || state_.renderMode == mdsp_ui::scopes::PhaseFanRenderMode::Both);

    if (drawDots)
    {
        constexpr float kDensityEpsilon = 1.0e-6f;
        for (int a = 0; a < mdsp_ui::scopes::PhaseFanRenderState::kAngleBins - 1; ++a)
        {
            float rOuterNorm = 0.0f;
            float maxD = 0.0f;

            for (int r = 0; r < mdsp_ui::scopes::PhaseFanRenderState::kRadiusBins; ++r)
                maxD = juce::jmax (maxD, state_.density[static_cast<size_t> (a)][static_cast<size_t> (r)]);

            if (maxD < kDensityEpsilon)
                continue;

            const float threshold = maxD * mdsp_ui::scopes::PhaseFanRenderState::kContourThresholdFrac;

            for (int r = mdsp_ui::scopes::PhaseFanRenderState::kRadiusBins - 1; r >= 0; --r)
            {
                if (state_.density[static_cast<size_t> (a)][static_cast<size_t> (r)]
                    >= threshold)
                {
                    rOuterNorm = static_cast<float> (r)
                               / static_cast<float> (mdsp_ui::scopes::PhaseFanRenderState::kRadiusBins - 1);
                    break;
                }
            }

            if (rOuterNorm <= 0.0f)
                continue;

            const float rInnerNorm = rOuterNorm * kInnerProportion;
            const auto outer0 = pointFor (a, rOuterNorm);
            const auto outer1 = pointFor (a + 1, rOuterNorm);
            const auto inner1 = pointFor (a + 1, rInnerNorm);
            const auto inner0 = pointFor (a, rInnerNorm);

            fanFillPath_.startNewSubPath (outer0);
            fanFillPath_.lineTo (outer1);
            fanFillPath_.lineTo (inner1);
            fanFillPath_.lineTo (inner0);
            fanFillPath_.closeSubPath();
        }
    }

    if (drawLines)
    {
        bool contourStarted = false;
        bool peakStarted = false;

        for (int a = 0; a < mdsp_ui::scopes::PhaseFanRenderState::kAngleBins; ++a)
        {
            const float contour = state_.contourRNorm[static_cast<size_t> (a)];
            if (contour > kContourMinDraw)
            {
                const auto p = pointFor (a, contour);
                if (!contourStarted)
                {
                    contourPath_.startNewSubPath (p);
                    contourStarted = true;
                }
                else
                {
                    contourPath_.lineTo (p);
                }
            }

            if (state_.peakHoldEnabled)
            {
                const float peak = state_.peakRNorm[static_cast<size_t> (a)];
                if (peak > kPeakMinDraw)
                {
                    const auto p = pointFor (a, peak);
                    if (!peakStarted)
                    {
                        peakHoldPath_.startNewSubPath (p);
                        peakStarted = true;
                    }
                    else
                    {
                        peakHoldPath_.lineTo (p);
                    }
                }
            }
        }
    }
}

void PhaseFanScopeComponent::paint (juce::Graphics& g)
{
    const auto& theme = ui_.theme();
    g.fillAll (theme.panel);

    const auto area = getLocalBounds().toFloat();
    const float w = area.getWidth();
    const float h = area.getHeight();
    if (w < 2.0f || h < 2.0f)
        return;

    const auto padded = area.reduced (kPadding);
    if (padded.getWidth() < 2.0f || padded.getHeight() < 2.0f)
        return;

    const float radiusPx = juce::jmin (padded.getWidth() * 0.5f, padded.getHeight());
    if (radiusPx <= 0.0f)
        return;

    const float cx = padded.getCentreX();
    const float cy = padded.getBottom();

    const float rMin = radiusPx * 0.02f;
    const float rMax = radiusPx;

    g.setColour (theme.grid.withAlpha (0.6f));
    for (int ring = 1; ring <= 5; ++ring)
    {
        const float rr = juce::jmap (static_cast<float> (ring) / 5.0f, 0.0f, 1.0f, rMin, rMax);
        arcPath_.clear();
        arcPath_.addArc (cx - rr,
                         cy - rr,
                         rr * 2.0f,
                         rr * 2.0f,
                         juce::MathConstants<float>::pi,
                         juce::MathConstants<float>::twoPi,
                         true);
        g.strokePath (arcPath_, juce::PathStrokeType (0.5f));
    }

    for (int deg = -90; deg <= 90; deg += 15)
    {
        const float theta = static_cast<float> (deg) * juce::MathConstants<float>::pi / 180.0f;
        const float ex = cx + std::sin (theta) * radiusPx;
        const float ey = cy - std::cos (theta) * radiusPx;
        g.drawLine (cx, cy, ex, ey, 0.5f);
    }

    if (!fanFillPath_.isEmpty())
    {
        g.setColour (theme.seriesPeak.withAlpha (0.33f));
        g.fillPath (fanFillPath_);
    }

    if (!contourPath_.isEmpty())
    {
        g.setColour (theme.seriesPeak.withAlpha (0.85f));
        g.strokePath (contourPath_, juce::PathStrokeType (1.6f));
    }

    if (state_.peakHoldEnabled && !peakHoldPath_.isEmpty())
    {
        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.strokePath (peakHoldPath_, juce::PathStrokeType (1.1f));
    }

    g.setColour (theme.textMuted);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("Left", juce::Rectangle<float> (0.0f, 0.0f, w * 0.3f, 24.0f), juce::Justification::centredLeft, false);
    g.drawText ("Right", juce::Rectangle<float> (w * 0.7f, 0.0f, w * 0.3f, 24.0f), juce::Justification::centredRight, false);

    const float labelH = 18.0f;
    const float bottomY = h - labelH - kPadding;
    g.drawText ("Anti Phase", juce::Rectangle<float> (0.0f, bottomY, w * 0.4f, labelH), juce::Justification::centredLeft, false);
    g.drawText ("Anti Phase", juce::Rectangle<float> (w * 0.6f, bottomY, w * 0.4f, labelH), juce::Justification::centredRight, false);

    g.setColour (theme.borderDivider);
    g.drawRect (area, 1.0f);
}
