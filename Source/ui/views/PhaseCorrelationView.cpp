#include "PhaseCorrelationView.h"

//==============================================================================
PhaseCorrelationView::PhaseCorrelationView()
{
}

PhaseCorrelationView::~PhaseCorrelationView() = default;

void PhaseCorrelationView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Dark panel background
    g.fillAll (juce::Colour (0xff151515));

    // Title text top-left
    g.setColour (juce::Colours::lightgrey.withAlpha (0.8f));
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
    g.setColour (juce::Colours::darkgrey.withAlpha (0.3f));
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
    g.setColour (juce::Colours::darkgrey.withAlpha (0.4f));
    g.drawLine (static_cast<float> (centerX - radius), static_cast<float> (centerY),
                static_cast<float> (centerX + radius), static_cast<float> (centerY), 1.0f);
    g.drawLine (static_cast<float> (centerX), static_cast<float> (centerY - radius),
                static_cast<float> (centerX), static_cast<float> (centerY + radius), 1.0f);

    // Draw sample points if available
    if (numPoints_ > 0)
    {
        g.setColour (juce::Colours::cyan.withAlpha (0.6f));
        for (int i = 0; i < numPoints_; ++i)
        {
            const float x = centerX + points_[i].x * radius;
            const float y = centerY + points_[i].y * radius;
            g.fillEllipse (x - 1.5f, y - 1.5f, 3.0f, 3.0f);
        }
    }
    else
    {
        // "NO DATA" overlay
        g.setColour (juce::Colours::grey.withAlpha (0.5f));
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
    g.setColour (juce::Colours::darkgrey.withAlpha (0.2f));
    g.fillRect (meterArea);

    // Tick marks and labels
    g.setColour (juce::Colours::grey.withAlpha (0.5f));
    g.setFont (juce::Font (juce::FontOptions().withHeight (9.0f)));
    for (int tick = -1; tick <= 1; ++tick)
    {
        const float x = meterX + (meterWidth / 2.0f) * (1.0f + tick);
        g.drawLine (x, static_cast<float> (meterY), x, static_cast<float> (meterY + meterHeight), 1.0f);
        g.drawText (juce::String (tick), static_cast<int> (x - 8), meterY + meterHeight + 2,
                    16, 10, juce::Justification::centred);
    }

    // Correlation marker
    const float markerX = meterX + (meterWidth / 2.0f) * (1.0f + correlation_);
    g.setColour (juce::Colours::cyan);
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
            points_[i] = pts[i];
        }
    }
    repaint();
}
