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

    std::function<void (int)> onPeakRangeChanged;
    void setPeakRangeSelectedId (int id);
    
    std::function<void (int)> onModeChanged;
    void setMode (int modeIndex); // 1=FFT, 2=Bands, 3=Log

    std::function<void()> onResetPeaks;

private:
    mdsp_ui::UiContext& ui_;
    AnalyzerPro::ControlBinder* controlBinder = nullptr;

    juce::Label titleLabel;
    
    // Mode Toggles (replacing combo)
    juce::TextButton fftButton_;
    juce::TextButton bandButton_;
    juce::TextButton logButton_;

    juce::ComboBox fftSizeCombo_;
    juce::ComboBox averagingCombo_;
    // juce::ComboBox dbRangeBox_; // Removed
    juce::ComboBox peakRangeBox_;
    juce::TextButton resetPeaksButton_;
    juce::TextButton presetButton;
    juce::TextButton saveButton;
    juce::TextButton menuButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderBar)
};
