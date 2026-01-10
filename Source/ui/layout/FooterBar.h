#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>

//==============================================================================
/**
    Footer bar component.
*/
class FooterBar : public juce::Component
{
public:
    explicit FooterBar (mdsp_ui::UiContext& ui);
    ~FooterBar() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    mdsp_ui::UiContext& ui_;

    juce::Label statusLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FooterBar)
};
