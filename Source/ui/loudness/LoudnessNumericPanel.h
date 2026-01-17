/*
  ==============================================================================

    LoudnessNumericPanel.h
    Created: 15 Jan 2026
    Author:  Antigravity

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>
#include "../../PluginProcessor.h"
#include "../../dsp/loudness/LoudnessAnalyzer.h"

class LoudnessNumericPanel : public juce::Component,
                             public juce::Timer
{
public:
    LoudnessNumericPanel (mdsp_ui::UiContext& ui, AnalayzerProAudioProcessor& p);
    ~LoudnessNumericPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void timerCallback() override;

private:
    mdsp_ui::UiContext& ui_;
    AnalayzerProAudioProcessor& processor;
    
    // Cached snapshot for painting
    AnalyzerPro::dsp::LoudnessSnapshot snapshot;

    // Sub-component for individual value? Or just paint text directly?
    // Paint text directly is simplest for "No magic numbers" layout. 
    // We can define 4 separate areas for labels/values.
    
    juce::Label mLabel, sLabel, iLabel, pLabel; // Static labels
    
    juce::String mValueText = "-.--";
    juce::String sValueText = "-.--";
    juce::String iValueText = "-.--";
    juce::String pValueText = "-.--";

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoudnessNumericPanel)
};
