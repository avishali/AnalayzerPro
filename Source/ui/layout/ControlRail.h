#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>
#include "../../control/ControlBinder.h"
#include "control_primitives/SectionHeader.h"
#include "control_primitives/ChoiceRow.h"
#include "control_primitives/SliderRow.h"
#include "control_rows/PeakControlsRow.h"
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
    void setGainChangedCallback (std::function<void (float)> cb);
    void setGainValue (float gain);
    void paint (juce::Graphics& g) override;
    void resized() override;
    int getPreferredHeight() const noexcept;

private:
    AnalyzerPro::ControlBinder* controlBinder = nullptr;

    void triggerDbRangeChanged();
    std::function<void (int)> onDbRangeChanged_;

    void triggerGainChanged();
    std::function<void (float)> onGainChanged_;
    
    mdsp_ui::UiContext& ui_;

    // Underlying controls (must be declared before primitives that reference them)
    juce::ComboBox modeCombo;
    juce::ComboBox fftSizeCombo;
    juce::ComboBox averagingCombo;
    juce::ComboBox dbRangeCombo;
    juce::Slider displayGainSlider;
    juce::ComboBox tiltCombo;
    juce::Slider gainSlider;
    
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
    AnalyzerPro::ControlPrimitives::SliderRow displayGainRow;
    AnalyzerPro::ControlPrimitives::ChoiceRow tiltRow;
    AnalyzerPro::ControlPrimitives::SliderRow gainRow;
    PeakControlsRow peakControlsRow_;
    
    // Navigate section (placeholder)
    juce::Label placeholderLabel1;
    
    // Display section (placeholder)
    juce::Label placeholderLabel3;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControlRail)
};
