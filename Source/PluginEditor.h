#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>
#include <mdsp_ui/ThemeVariant.h>
#include <ui_core/UiCore.h>
#include <memory>


#include "PluginProcessor.h"
#include "ui/MainView.h"

//==============================================================================
/**
    Audio Processor Editor Template.
    Replace this with your plugin's UI implementation.
*/
class AnalayzerProAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    AnalayzerProAudioProcessorEditor (AnalayzerProAudioProcessor&);
    ~AnalayzerProAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    AnalayzerProAudioProcessor& audioProcessor;
    mdsp_ui::UiContext ui_;  // Single shared UiContext instance for all UI
    MainView mainView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalayzerProAudioProcessorEditor)
};
