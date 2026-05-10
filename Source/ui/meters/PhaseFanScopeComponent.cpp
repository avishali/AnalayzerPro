#include "PhaseFanScopeComponent.h"

#include <cmath>

namespace
{
constexpr float kEdgeInset      = 3.0f;   // minimal breathing room at all edges
constexpr float kContourMinDraw = 0.003f;
constexpr float kPeakMinDraw    = 0.003f;
constexpr float kInnerProportion = 0.12f;
constexpr float kPiHalf = juce::MathConstants<float>::halfPi;

// Derive cx / cy / radiusPx from component bounds, filling the panel.
// cy is placed at the bottom so the semicircle opens upward.
// Radius = full height so the arc always fills vertically.
// On narrow panels the arc clips at the sides (symmetric, like PAZ), never at the top.
void fanGeometry (juce::Rectangle<float> bounds, float& cx, float& cy, float& radiusPx)
{
    cx       = bounds.getCentreX();
    cy       = bounds.getBottom() - kEdgeInset;
    radiusPx = bounds.getHeight() - kEdgeInset * 2.0f;
}
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

    float cx, cy, radiusPx;
    fanGeometry (bounds, cx, cy, radiusPx);
    if (radiusPx <= 0.0f)
        return;

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

    float cx, cy, radiusPx;
    fanGeometry (area, cx, cy, radiusPx);
    if (radiusPx <= 0.0f)
        return;

    const float rMin = radiusPx * 0.02f;
    const float rMax = radiusPx;

    // ── Concentric arc rings ──────────────────────────────────────────────────
    for (int ring = 1; ring <= 5; ++ring)
    {
        const float rr  = juce::jmap (static_cast<float> (ring) / 5.0f, 0.0f, 1.0f, rMin, rMax);
        const bool  outer = (ring == 5);
        g.setColour (theme.grid.withAlpha (outer ? 0.80f : 0.50f));
        arcPath_.clear();
        arcPath_.addArc (cx - rr, cy - rr, rr * 2.0f, rr * 2.0f,
                         -kPiHalf,   // 9 o'clock (left endpoint)
                         kPiHalf,    // 3 o'clock (right endpoint), through 12 o'clock (top)
                         true);
        g.strokePath (arcPath_, juce::PathStrokeType (outer ? 1.0f : 0.5f));
    }

    // ── Radial spokes every 15° ───────────────────────────────────────────────
    for (int deg = -90; deg <= 90; deg += 15)
    {
        const bool  cardinal = (deg == 0 || deg == -90 || deg == 90);
        const float alpha    = cardinal ? 0.55f : 0.30f;
        g.setColour (theme.grid.withAlpha (alpha));
        const float theta = static_cast<float> (deg) * juce::MathConstants<float>::pi / 180.0f;
        const float ex = cx + std::sin (theta) * radiusPx;
        const float ey = cy - std::cos (theta) * radiusPx;
        g.drawLine (cx, cy, ex, ey, cardinal ? 1.0f : 0.5f);
    }

    // ── Signal ────────────────────────────────────────────────────────────────
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

    // ── Labels at arc geometry positions ─────────────────────────────────────
    g.setColour (theme.textMuted);
    g.setFont (juce::FontOptions (10.0f));

    // "L" / "R" — ideally at the ±45° arc intersection, clamped inside the component
    {
        const float t45  = juce::MathConstants<float>::pi / 4.0f;
        const float arcLx = cx - std::sin (t45) * radiusPx;
        const float arcRx = cx + std::sin (t45) * radiusPx;
        const float arcY  = cy - std::cos (t45) * radiusPx;
        const float lbW   = 24.0f, lbH = 14.0f;

        // Clamp x so the label stays visible when the arc clips the sides
        const float visLx = juce::jmax (area.getX() + lbW + 4.0f, arcLx);
        const float visRx = juce::jmin (area.getRight() - lbW - 4.0f, arcRx);
        const float visY  = juce::jmax (area.getY() + 4.0f, arcY);

        g.drawText ("L", juce::Rectangle<float> (visLx - lbW - 2.0f, visY - lbH * 0.5f, lbW, lbH),
                    juce::Justification::centredRight, false);
        g.drawText ("R", juce::Rectangle<float> (visRx + 2.0f, visY - lbH * 0.5f, lbW, lbH),
                    juce::Justification::centredLeft, false);
    }

    // "Anti Phase" — always anchored to the component left/right edges near the baseline
    {
        const float labelW = 70.0f, labelH = 14.0f;
        g.drawText ("Anti Phase",
                    juce::Rectangle<float> (area.getX() + 2.0f, cy - labelH - 2.0f, labelW, labelH),
                    juce::Justification::centredLeft, false);
        g.drawText ("Anti Phase",
                    juce::Rectangle<float> (area.getRight() - labelW - 2.0f, cy - labelH - 2.0f, labelW, labelH),
                    juce::Justification::centredRight, false);
    }

    g.setColour (theme.borderDivider);
    g.drawRect (area, 1.0f);
}
