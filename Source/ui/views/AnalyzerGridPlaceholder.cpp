#include "AnalyzerGridPlaceholder.h"
#include <mdsp_ui/Theme.h>
#include <mdsp_ui/GridRenderer.h>

//==============================================================================
AnalyzerGridPlaceholder::AnalyzerGridPlaceholder()
{
}

AnalyzerGridPlaceholder::~AnalyzerGridPlaceholder() = default;

void AnalyzerGridPlaceholder::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    mdsp_ui::Theme theme;

    // Dark blue-ish background
    g.fillAll (theme.panel);

    // Draw grid using GridRenderer
    mdsp_ui::GridRenderer grid (mdsp_ui::GridStyle {
        .minorSpacingPx = 40,
        .majorEvery     = 5,
        .minorAlpha     = 0.25f,
        .majorAlpha     = 0.45f,
        .thickness      = 1.0f,
        .drawMajor      = true
    });
    grid.draw (g, bounds, theme);
    // Caption
    g.setColour (theme.textMuted.withAlpha (0.90f));
    g.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    g.drawText ("FFT / RTA Display (placeholder)", bounds.reduced (8).removeFromTop (20),
                juce::Justification::centredLeft);
}
