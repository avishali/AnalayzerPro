#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../control/ControlBinder.h"

//==============================================================================
/**
    Control rail component for right-side controls.
    Contains placeholder sections for control groups.
*/
class ControlRail : public juce::Component
{
public:
    ControlRail();
    ~ControlRail() override;

    void setControlBinder (AnalyzerPro::ControlBinder& binder);
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    AnalyzerPro::ControlBinder* controlBinder = nullptr;
    
    // Section titles
    juce::Label navigateLabel;
    juce::Label analyzerLabel;
    juce::Label displayLabel;
    juce::Label metersLabel;
    
    // Navigate section (placeholder)
    juce::Label placeholderLabel1;
    
    // Analyzer section controls
    juce::Label modeLabel;
    juce::ComboBox modeCombo;
    juce::Label fftSizeLabel;
    juce::ComboBox fftSizeCombo;
    juce::Label averagingLabel;
    juce::ComboBox averagingCombo;
    juce::Label holdLabel;
    juce::ToggleButton holdButton;
    juce::Label peakDecayLabel;
    juce::Slider peakDecaySlider;
    juce::Label displayGainLabel;
    juce::Slider displayGainSlider;
    juce::Label tiltLabel;
    juce::ComboBox tiltCombo;
    
    // Display section (placeholder)
    juce::Label placeholderLabel3;
    
    // Meters section (placeholder)
    juce::Label placeholderLabel4;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControlRail)
};
