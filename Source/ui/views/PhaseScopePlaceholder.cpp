#include "PhaseScopePlaceholder.h"
#include <mdsp_ui/Theme.h>

//==============================================================================
PhaseScopePlaceholder::PhaseScopePlaceholder()
{
}

PhaseScopePlaceholder::~PhaseScopePlaceholder() = default;

void PhaseScopePlaceholder::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    mdsp_ui::Theme theme;

    // Neutral dark background
    g.fillAll (theme.panel);

    // Draw simple circular/arc guideline
    g.setColour (theme.grid.withAlpha (0.3f));
    const int centerX = bounds.getCentreX();
    const int centerY = bounds.getCentreY();
    const int radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 3;

    // Draw circle
    g.drawEllipse (static_cast<float> (centerX - radius),
                   static_cast<float> (centerY - radius),
                   static_cast<float> (radius * 2),
                   static_cast<float> (radius * 2),
                   1.0f);

    // Draw center crosshair
    g.drawLine (static_cast<float> (centerX - radius), static_cast<float> (centerY),
                static_cast<float> (centerX + radius), static_cast<float> (centerY), 1.0f);
    g.drawLine (static_cast<float> (centerX), static_cast<float> (centerY - radius),
                static_cast<float> (centerX), static_cast<float> (centerY + radius), 1.0f);

    // Caption
    g.setColour (theme.textMuted.withAlpha (0.6f));
    g.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    g.drawText ("Phase / Correlation (placeholder)", bounds.reduced (8).removeFromTop (20),
                juce::Justification::centredLeft);
}
