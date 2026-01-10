#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>
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

    explicit HeaderBar (mdsp_ui::UiContext& ui);
    ~HeaderBar() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    /** Set display mode (read-only mirror - reflects current mode from right-side control) */
    void setDisplayMode (DisplayMode m);

    std::function<void (int)> onDbRangeChanged;
    void setDbRangeSelectedId (int id);

    std::function<void (int)> onPeakRangeChanged;
    void setPeakRangeSelectedId (int id);

    std::function<void()> onResetPeaks;

private:
    mdsp_ui::UiContext& ui_;

    juce::Label titleLabel;
    juce::Label modeLabel;  // Read-only label showing current mode (replaces interactive modeBox)
    juce::Label dbRangeLabel_;
    juce::ComboBox dbRangeBox_;
    juce::Label peakRangeLabel_;
    juce::ComboBox peakRangeBox_;
    juce::TextButton resetPeaksButton_;
    juce::TextButton presetButton;
    juce::TextButton saveButton;
    juce::TextButton menuButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderBar)
};
