#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/**
    Footer bar component.
*/
class FooterBar : public juce::Component
{
public:
    FooterBar();
    ~FooterBar() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    juce::Label statusLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FooterBar)
};
