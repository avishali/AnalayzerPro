#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>
#include "../../control/ControlBinder.h"
#include "control_primitives/SectionHeader.h"
#include "control_primitives/ChoiceRow.h"
#include "control_primitives/ToggleRow.h"
#include "control_primitives/SliderRow.h"
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
    void setDbRangeChangedCallback (std::function<void (int)> cb);
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    AnalyzerPro::ControlBinder* controlBinder = nullptr;

    void triggerResetPeaks();
    std::function<void()> onResetPeaks_;

    void triggerDbRangeChanged();
    std::function<void (int)> onDbRangeChanged_;
    
    mdsp_ui::UiContext& ui_;

    // Underlying controls (must be declared before primitives that reference them)
    juce::ComboBox modeCombo;
    juce::ComboBox fftSizeCombo;
    juce::ComboBox averagingCombo;
    juce::ComboBox dbRangeCombo;
    juce::ToggleButton peakHoldButton;
    juce::ToggleButton holdButton;
    juce::Slider peakDecaySlider;
    juce::Slider displayGainSlider;
    juce::ComboBox tiltCombo;
    juce::TextButton resetPeaksButton { "Reset" };
    
    // Section headers (using primitives)
    AnalyzerPro::ControlPrimitives::SectionHeader navigateHeader;
    AnalyzerPro::ControlPrimitives::SectionHeader analyzerHeader;
    AnalyzerPro::ControlPrimitives::SectionHeader displayHeader;
    AnalyzerPro::ControlPrimitives::SectionHeader metersHeader;
    
    // Control rows (using primitives - reference controls above)
    AnalyzerPro::ControlPrimitives::ChoiceRow modeRow;
    AnalyzerPro::ControlPrimitives::ChoiceRow fftSizeRow;
    AnalyzerPro::ControlPrimitives::ChoiceRow averagingRow;
    AnalyzerPro::ControlPrimitives::ChoiceRow dbRangeRow;
    AnalyzerPro::ControlPrimitives::ToggleRow peakHoldRow;
    AnalyzerPro::ControlPrimitives::ToggleRow holdRow;
    AnalyzerPro::ControlPrimitives::SliderRow peakDecayRow;
    AnalyzerPro::ControlPrimitives::SliderRow displayGainRow;
    AnalyzerPro::ControlPrimitives::ChoiceRow tiltRow;
    
    // Navigate section (placeholder)
    juce::Label placeholderLabel1;
    
    // Display section (placeholder)
    juce::Label placeholderLabel3;
    
    // Meters section (placeholder)
    juce::Label placeholderLabel4;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControlRail)
};
