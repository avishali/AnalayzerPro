#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>
#include "../../control/ControlBinder.h"
#include <mdsp_ui/controls/SectionHeader.h>
#include <mdsp_ui/controls/ChoiceRow.h>
#include <mdsp_ui/controls/ToggleRow.h>
#include <mdsp_ui/controls/SliderRow.h>
#include <functional>

//==============================================================================
/**
    Control rail component for right-side controls.
    Contains placeholder sections for control groups.
*/
class ControlRail : public juce::Component
{
public:
    explicit ControlRail (mdsp_ui::UiContext& ui);
    ~ControlRail() override;

    void setControlBinder (AnalyzerPro::ControlBinder& binder);
    void setResetPeaksCallback (std::function<void()> cb);
    // DbRange removed
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    AnalyzerPro::ControlBinder* controlBinder = nullptr;

    void triggerResetPeaks();
    std::function<void()> onResetPeaks_;

    // DbRange callback removed
    
    mdsp_ui::UiContext& ui_;

    // Underlying controls (must be declared before primitives that reference them)
    // juce::ComboBox dbRangeCombo; // Removed
    juce::ToggleButton peakHoldButton;
    juce::ToggleButton holdButton;
    juce::Slider peakDecaySlider;
    juce::Slider displayGainSlider;
    juce::ComboBox tiltCombo;
    juce::TextButton resetPeaksButton { "Reset" };
    
    // Section headers (using primitives)
    mdsp_ui::SectionHeader navigateHeader;
    mdsp_ui::SectionHeader analyzerHeader;
    mdsp_ui::SectionHeader displayHeader;
    mdsp_ui::SectionHeader metersHeader;
    
    // Control rows (using primitives - reference controls above)
    // mdsp_ui::ChoiceRow dbRangeRow; // Removed
    mdsp_ui::ToggleRow peakHoldRow;
    mdsp_ui::ToggleRow holdRow;
    mdsp_ui::SliderRow peakDecayRow;
    mdsp_ui::SliderRow displayGainRow;
    mdsp_ui::ChoiceRow tiltRow;
    
    // Navigate section (placeholder)
    juce::Label placeholderLabel1;
    
    // Display section (placeholder)
    juce::Label placeholderLabel3;
    
    // Meters section (placeholder)
    juce::Label placeholderLabel4;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControlRail)
};
