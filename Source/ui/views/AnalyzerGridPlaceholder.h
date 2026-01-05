#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/**
    Placeholder component for FFT / RTA display.
    Shows a simple grid pattern until the real analyzer view is implemented.
*/
class AnalyzerGridPlaceholder : public juce::Component
{
public:
    AnalyzerGridPlaceholder();
    ~AnalyzerGridPlaceholder() override;

    void paint (juce::Graphics& g) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalyzerGridPlaceholder)
};
