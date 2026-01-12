#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>
#include "../../control/ControlIds.h"
#include "../../state/PresetManager.h"
#include "../../state/StateManager.h"
#include <functional>

namespace AnalyzerPro { class ControlBinder; }

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

    // State Managment
    void setManagers (AnalyzerPro::state::PresetManager* pm, AnalyzerPro::state::StateManager* sm);
    
private:
    void updateActiveSlot();

    mdsp_ui::UiContext& ui_;
    AnalyzerPro::ControlBinder* controlBinder = nullptr;
    AnalyzerPro::state::PresetManager* presetManager = nullptr;
    AnalyzerPro::state::StateManager* stateManager = nullptr;

    juce::Label titleLabel;
    
    // Mode Toggles
    juce::TextButton fftButton_;
    juce::TextButton bandButton_;
    juce::TextButton logButton_;

    juce::ComboBox fftSizeCombo_;
    juce::ComboBox averagingCombo_;
    // juce::ComboBox dbRangeBox_; // Removed
    juce::ComboBox peakRangeBox_;
    juce::TextButton resetPeaksButton_;
    
    // Presets & State
    juce::TextButton presetButton; // Opens menu
    juce::TextButton saveButton;
    // juce::TextButton menuButton; // Removed or reused? Prefer A/B
    juce::TextButton slotAButton;
    juce::TextButton slotBButton;
    juce::ToggleButton bypassButton; // Bound to param

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderBar)
};
