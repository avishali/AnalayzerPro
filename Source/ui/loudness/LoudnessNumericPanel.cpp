/*
  ==============================================================================

    LoudnessNumericPanel.cpp
    Created: 15 Jan 2026
    Author:  Antigravity

  ==============================================================================
*/

#include "LoudnessNumericPanel.h"

LoudnessNumericPanel::LoudnessNumericPanel (mdsp_ui::UiContext& ui, AnalayzerProAudioProcessor& p)
    : ui_ (ui), processor (p)
{
    const auto& theme = ui_.theme();
    const auto& type = ui_.type();

    auto setupLabel = [&](juce::Label& l, const juce::String& text, const juce::String& tooltip)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (type.labelFont());
        l.setColour (juce::Label::textColourId, theme.textMuted);
        l.setJustificationType (juce::Justification::centred);
        l.setTooltip (tooltip);
        addAndMakeVisible (l);
    };

    setupLabel (mLabel, "NO M.", "Momentary Loudness (400ms)"); // "M" is maybe too short? "Momentary"? Space is tight.
    setupLabel (sLabel, "SHORT", "Short-term Loudness (3s)");
    setupLabel (iLabel, "INT (v1)", "Integrated Loudness (Cumulative)");
    setupLabel (pLabel, "PEAK", "Max Peak (dB)");
    
    // Labels should be clearer. Full words as per requirement.
    mLabel.setText ("Momentary", juce::dontSendNotification);
    sLabel.setText ("Short-term", juce::dontSendNotification);
    iLabel.setText ("Integrated", juce::dontSendNotification);
    pLabel.setText ("Loudness Peak", juce::dontSendNotification);

    startTimerHz (15); // ~15-20 FPS is smooth enough for numbers
}

LoudnessNumericPanel::~LoudnessNumericPanel()
{
}

void LoudnessNumericPanel::timerCallback()
{
    // Fetch snapshot
    snapshot = processor.getLoudnessAnalyzer().getSnapshot();

    auto format = [](float val, const juce::String& suffix = "") -> juce::String
    {
        if (val <= -100.0f) return "-inf";
        return juce::String (val, 1) + suffix;
    };
    
    mValueText = format (snapshot.momentaryLufs, " LUFS");
    sValueText = format (snapshot.shortTermLufs, " LUFS");
    iValueText = format (snapshot.integratedLufs, " LUFS");
    pValueText = format (snapshot.peakDb, " dB");
    
    repaint();
}

void LoudnessNumericPanel::mouseDown (const juce::MouseEvent& e)
{
    // Check if clicked on Peak area
    // We need to re-derive the area or store it.
    // Simpler: just check if in bottom-right quadrant?
    // Let's copy layout logic nicely or just use simple bounds check.
    
    auto bounds = getLocalBounds().reduced (ui_.metrics().pad);
    // Top/Bottom split
    auto bottomRow = bounds.removeFromBottom (bounds.getHeight() / 2);
    // Left/Right split of bottom
    auto pArea = bottomRow.removeFromRight (bottomRow.getWidth() / 2); // Right half is Peak
    
    if (pArea.contains (e.getPosition()))
    {
        processor.getLoudnessAnalyzer().resetPeak();
        repaint(); // Instant feedback
    }
}

void LoudnessNumericPanel::paint (juce::Graphics& g)
{
    const auto& theme = ui_.theme();
    const auto& type = ui_.type();
    
    g.fillAll (theme.panel); // Use panel background, or maybe transparent if parent has bg?
    // MainView uses theme.background (dark).
    // Let's use theme.panel (slightly lighter) or just border.
    // Spec says "bottom section". StereoScope has background.
    // Let's ensure this matches.

    g.setColour (theme.background.brighter(0.02f));
    g.fillAll();
    
    g.setColour (theme.borderDivider);
    g.drawRect (getLocalBounds());
    
    // Draw values
    // We rely on resized logic to place labels. Values are drawn below labels.
    
    auto drawValue = [&](const juce::String& text, juce::Rectangle<int> area, bool warn = false)
    {
        g.setColour (warn ? theme.danger : theme.text);
        g.setFont (type.titleFont()); // Large numbers
        g.drawText (text, area, juce::Justification::centred, true);
    };
    
    // Layout logic repeated here or stored?
    // Let's use getLocalBounds and split same way as resized.
    
    // 2x2 Grid
    auto bounds = getLocalBounds().reduced (ui_.metrics().pad);
    auto topRow = bounds.removeFromTop (bounds.getHeight() / 2);
    auto bottomRow = bounds;
    
    auto mArea = topRow.removeFromLeft (topRow.getWidth() / 2);
    auto sArea = topRow;
    auto iArea = bottomRow.removeFromLeft (bottomRow.getWidth() / 2);
    auto pArea = bottomRow;
    
    // Adjust for labels being at "top" of these boxes
    // Actually labels are Components. Values are painted.
    // Let's reserve top 15px for label comp, rest for value paint?
    const int labelH = 16;
    
    drawValue (mValueText, mArea.withTrimmedTop(labelH));
    drawValue (sValueText, sArea.withTrimmedTop(labelH));
    // Integrated
    drawValue (iValueText, iArea.withTrimmedTop(labelH));
    // Peak
    drawValue (pValueText, pArea.withTrimmedTop(labelH), (snapshot.peakDb > -0.1f));
}

void LoudnessNumericPanel::resized()
{
    const auto& m = ui_.metrics();
    auto bounds = getLocalBounds().reduced (m.pad);
    
    auto topRow = bounds.removeFromTop (bounds.getHeight() / 2);
    auto bottomRow = bounds;
    
    auto mArea = topRow.removeFromLeft (topRow.getWidth() / 2);
    auto sArea = topRow;
    auto iArea = bottomRow.removeFromLeft (bottomRow.getWidth() / 2);
    auto pArea = bottomRow;
    
    const int labelH = 16;
    
    mLabel.setBounds (mArea.removeFromTop (labelH));
    sLabel.setBounds (sArea.removeFromTop (labelH));
    iLabel.setBounds (iArea.removeFromTop (labelH));
    pLabel.setBounds (pArea.removeFromTop (labelH));
}
