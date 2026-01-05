#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

//==============================================================================
/**
    Header bar component with title and mode selector.
*/
class HeaderBar : public juce::Component
{
public:
    enum class DisplayMode
    {
        FFT = 0,
        Band = 1,
        Log = 2
    };

    HeaderBar();
    ~HeaderBar() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void setDisplayMode (DisplayMode m);

    std::function<void(DisplayMode)> onDisplayModeChanged;

private:
    juce::Label titleLabel;
    juce::ComboBox modeBox;
    juce::TextButton presetButton;
    juce::TextButton saveButton;
    juce::TextButton menuButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderBar)
};
