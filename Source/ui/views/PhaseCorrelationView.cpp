#include "PhaseCorrelationView.h"
#include <mdsp_ui/Theme.h>
#include <mdsp_ui/AxisRenderer.h>

//==============================================================================
PhaseCorrelationView::PhaseCorrelationView()
{
}

PhaseCorrelationView::~PhaseCorrelationView() = default;

void PhaseCorrelationView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    mdsp_ui::Theme theme;

    // Dark panel background
    g.fillAll (theme.panel);

    // Title text top-left
    g.setColour (theme.text.withAlpha (0.8f));
    g.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    g.drawText ("Phase / Correlation", bounds.reduced (8).removeFromTop (18),
                juce::Justification::centredLeft);

    // Polar scope area (centered, square-ish)
    auto scopeArea = bounds.reduced (8, 24);  // Top margin for title
    const int scopeSize = juce::jmin (scopeArea.getWidth(), scopeArea.getHeight());
    const int centerX = scopeArea.getCentreX();
    const int centerY = scopeArea.getCentreY();
    const int radius = scopeSize / 2 - 4;  // Small margin

    // Draw concentric circles/arcs
    g.setColour (theme.grid.withAlpha (0.3f));
    for (int i = 1; i <= 3; ++i)
    {
        const float r = radius * (i / 3.0f);
        g.drawEllipse (static_cast<float> (centerX - r),
                       static_cast<float> (centerY - r),
                       r * 2.0f,
                       r * 2.0f,
                       1.0f);
    }

    // Draw crosshair
    g.setColour (theme.grid.withAlpha (0.4f));
    g.drawLine (static_cast<float> (centerX - radius), static_cast<float> (centerY),
                static_cast<float> (centerX + radius), static_cast<float> (centerY), 1.0f);
    g.drawLine (static_cast<float> (centerX), static_cast<float> (centerY - radius),
                static_cast<float> (centerX), static_cast<float> (centerY + radius), 1.0f);

    // Draw sample points if available
    if (numPoints_ > 0)
    {
        g.setColour (theme.accent.withAlpha (0.6f));
        for (int i = 0; i < numPoints_; ++i)
        {
            const auto idx = static_cast<std::size_t> (i);
            const float x = centerX + points_[idx].x * radius;
            const float y = centerY + points_[idx].y * radius;
            g.fillEllipse (x - 1.5f, y - 1.5f, 3.0f, 3.0f);
        }
    }
    else
    {
        // "NO DATA" overlay
        g.setColour (theme.textMuted.withAlpha (0.5f));
        g.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)));
        g.drawText ("NO DATA", scopeArea, juce::Justification::centred);
    }

    // Correlation meter strip (bottom edge, inside panel)
    auto meterBounds = bounds;
    auto meterArea = meterBounds.removeFromBottom (16).reduced (8, 2);
    const int meterWidth = meterArea.getWidth();
    const int meterHeight = meterArea.getHeight();
    const int meterX = meterArea.getX();
    const int meterY = meterArea.getY();

    // Meter background
    g.setColour (theme.background.withAlpha (0.2f));
    g.fillRect (meterArea);

    // Build axis ticks for correlation meter (-1, 0, +1)
    juce::Array<mdsp_ui::AxisTick> correlationTicks;
    for (int tick = -1; tick <= 1; ++tick)
    {
        const float normalizedPos = (tick + 1.0f) / 2.0f;  // Map -1..+1 to 0..1
        const float posPx = normalizedPos * static_cast<float> (meterWidth);
        correlationTicks.add ({ posPx, juce::String (tick), true });  // All ticks are major and labeled
    }

    // Draw correlation axis using AxisRenderer (Bottom edge)
    const juce::Rectangle<int> meterPlotBounds (meterX, meterY, meterWidth, meterHeight);
    mdsp_ui::AxisStyle meterStyle;
    meterStyle.tickAlpha = 0.5f;
    meterStyle.labelAlpha = 0.5f;
    meterStyle.tickThickness = 1.0f;
    meterStyle.labelFontHeight = 9.0f;
    meterStyle.labelInsetPx = 2.0f;
    meterStyle.minorTickLengthPx = static_cast<float> (meterHeight);
    meterStyle.majorTickLengthPx = static_cast<float> (meterHeight);
    meterStyle.ticksOnly = false;
    meterStyle.clipTicksToPlot = true;
    mdsp_ui::AxisRenderer::draw (g, meterPlotBounds, theme, correlationTicks, mdsp_ui::AxisEdge::Bottom, meterStyle);

    // Correlation marker (keep as manual drawing - not an axis element)
    const float markerX = meterX + (meterWidth / 2.0f) * (1.0f + correlation_);
    g.setColour (theme.accent);
    g.fillRect (static_cast<int> (markerX - 1), meterY, 2, meterHeight);
}

void PhaseCorrelationView::resized()
{
    // No layout needed - everything is drawn in paint()
}

void PhaseCorrelationView::setCorrelation (float correlation)
{
    correlation_ = juce::jlimit (-1.0f, 1.0f, correlation);
    repaint();
}

void PhaseCorrelationView::setPoints (const Sample* pts, int numPts)
{
    if (pts == nullptr || numPts <= 0)
    {
        numPoints_ = 0;
    }
    else
    {
        numPoints_ = juce::jmin (numPts, kMaxPoints);
        for (int i = 0; i < numPoints_; ++i)
        {
            const auto idx = static_cast<std::size_t> (i);
            points_[idx] = pts[i];
        }
    }
    repaint();
}
