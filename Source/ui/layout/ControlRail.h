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
    
    // Scope Callbacks
    std::function<void(int)> onScopeModeChanged;  // 1=Peak, 2=RMS
    std::function<void(int)> onScopeShapeChanged; // 1=Lissajous, 2=Scatter

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    AnalyzerPro::ControlBinder* controlBinder = nullptr;

    void triggerResetPeaks();
    std::function<void()> onResetPeaks_;

    mdsp_ui::UiContext& ui_;

    // Underlying controls
    juce::ToggleButton holdButton;
    juce::Slider peakDecaySlider;
    juce::ComboBox tiltCombo;
    juce::TextButton resetPeaksButton { "Reset" };
    
    // Scope Controls
    juce::ComboBox scopeModeCombo;
    juce::ComboBox scopeShapeCombo;
    juce::ComboBox scopeInputCombo; // New
    
    // Meter Controls
    juce::ComboBox meterInputCombo; // New
    
    // Section headers
    mdsp_ui::SectionHeader navigateHeader;
    mdsp_ui::SectionHeader analyzerHeader;
    mdsp_ui::SectionHeader displayHeader;
    mdsp_ui::SectionHeader metersHeader;
    
    // Control rows
    mdsp_ui::ToggleRow holdRow;
    mdsp_ui::SliderRow peakDecayRow;
    mdsp_ui::ChoiceRow tiltRow;
    
    mdsp_ui::ChoiceRow scopeModeRow;
    mdsp_ui::ChoiceRow scopeShapeRow;
    mdsp_ui::ChoiceRow scopeInputRow; // New
    mdsp_ui::ChoiceRow meterInputRow; // New
    
    // Trace Toggles
    juce::ToggleButton showLrButton;
    juce::ToggleButton showMonoButton;
    juce::ToggleButton showLButton;
    juce::ToggleButton showRButton;
    juce::ToggleButton showMidButton;
    juce::ToggleButton showSideButton;
    juce::ToggleButton showRmsButton;
    
    mdsp_ui::ToggleRow showLrRow;
    mdsp_ui::ToggleRow showMonoRow;
    mdsp_ui::ToggleRow showLRow;
    mdsp_ui::ToggleRow showRRow;
    mdsp_ui::ToggleRow showMidRow;
    mdsp_ui::ToggleRow showSideRow;
    mdsp_ui::ToggleRow showRmsRow;
    
    // Smoothing
    
    // Smoothing
    juce::ComboBox smoothingCombo;
    mdsp_ui::ChoiceRow smoothingRow;

    // Weighting
    juce::ComboBox weightingCombo;
    mdsp_ui::ChoiceRow weightingRow;
    
    // Navigate section (placeholder)
    juce::Label placeholderLabel1;
    
    // Display section (placeholder)
    juce::Label placeholderLabel3;
    
    // Meters section (placeholder)
    juce::Label placeholderLabel4;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControlRail)
};
