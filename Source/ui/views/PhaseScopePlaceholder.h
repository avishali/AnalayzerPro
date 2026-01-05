#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/**
    Placeholder component for Phase / Correlation display.
    Shows a simple circular guideline until the real phase scope is implemented.
*/
class PhaseScopePlaceholder : public juce::Component
{
public:
    PhaseScopePlaceholder();
    ~PhaseScopePlaceholder() override;

    void paint (juce::Graphics& g) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhaseScopePlaceholder)
};
