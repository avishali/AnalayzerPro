#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>
#include "../../presets/PresetManager.h"
#include "../../presets/ABStateManager.h"
#include <functional>
#include <memory>

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

    // Control Rail Toggle
    std::function<void()> onRailToggleClicked;

    // State Management
    void setManagers (AnalyzerPro::presets::PresetManager* pm, AnalyzerPro::presets::ABStateManager* sm);
    
private:
    void updateActiveSlot();

    mdsp_ui::UiContext& ui_;
    AnalyzerPro::ControlBinder* controlBinder = nullptr;
    AnalyzerPro::presets::PresetManager* presetManager = nullptr;
    AnalyzerPro::presets::ABStateManager* abStateManager = nullptr;

    juce::Label titleLabel;
    juce::ComboBox peakRangeBox_;
    
    // Presets & State
    juce::TextButton presetButton; // Opens menu
    juce::TextButton saveButton;
    // juce::TextButton menuButton; // Removed or reused? Prefer A/B
    juce::TextButton slotAButton;
    juce::TextButton slotBButton;
    juce::ToggleButton bypassButton; // Bound to param
    juce::ToggleButton railToggleButton; // Toggle control rail visibility

    std::unique_ptr<juce::LookAndFeel> headerBarLook_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderBar)
};
