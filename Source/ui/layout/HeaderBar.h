#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>
#include "../../control/ControlBinder.h"
#include <functional>

//==============================================================================
/**
    Header bar component with title and analyzer controls.
*/
class HeaderBar : public juce::Component
{
public:
    explicit HeaderBar (mdsp_ui::UiContext& ui);
    ~HeaderBar() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void setControlBinder (AnalyzerPro::ControlBinder& binder);

    std::function<void (int)> onDbRangeChanged;
    void setDbRangeSelectedId (int id);

    std::function<void (int)> onPeakRangeChanged;
    void setPeakRangeSelectedId (int id);

    std::function<void()> onResetPeaks;

private:
    mdsp_ui::UiContext& ui_;
    AnalyzerPro::ControlBinder* controlBinder = nullptr;

    juce::Label titleLabel;
    juce::ComboBox modeCombo_;
    juce::ComboBox fftSizeCombo_;
    juce::ComboBox averagingCombo_;
    juce::ComboBox dbRangeBox_;
    juce::ComboBox peakRangeBox_;
    juce::TextButton resetPeaksButton_;
    juce::TextButton presetButton;
    juce::TextButton saveButton;
    juce::TextButton menuButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderBar)
};
