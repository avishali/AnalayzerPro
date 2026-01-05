#include "AnalyzerGridPlaceholder.h"

//==============================================================================
AnalyzerGridPlaceholder::AnalyzerGridPlaceholder()
{
}

AnalyzerGridPlaceholder::~AnalyzerGridPlaceholder() = default;

void AnalyzerGridPlaceholder::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Dark blue-ish background
    g.fillAll (juce::Colour (0xff0a1520));

    // Draw simple grid
    g.setColour (juce::Colours::darkgrey.withAlpha (0.2f));
    const int gridSpacing = 40;

    // Vertical lines
    for (int x = gridSpacing; x < bounds.getWidth(); x += gridSpacing)
    {
        g.drawVerticalLine (x, 0.0f, static_cast<float> (bounds.getHeight()));
    }

    // Horizontal lines
    for (int y = gridSpacing; y < bounds.getHeight(); y += gridSpacing)
    {
        g.drawHorizontalLine (y, 0.0f, static_cast<float> (bounds.getWidth()));
    }

    // Caption
    g.setColour (juce::Colours::lightgrey.withAlpha (0.6f));
    g.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    g.drawText ("FFT / RTA Display (placeholder)", bounds.reduced (8).removeFromTop (20),
                juce::Justification::centredLeft);
}
