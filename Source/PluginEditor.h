#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <ui_core/UiCore.h>

#include "PluginProcessor.h"
#include "ui/MainView.h"

//==============================================================================
/**
    Audio Processor Editor Template.
    Replace this with your plugin's UI implementation.
*/
class PluginTemplateAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    PluginTemplateAudioProcessorEditor (PluginTemplateAudioProcessor&);
    ~PluginTemplateAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PluginTemplateAudioProcessor& audioProcessor;
    MainView mainView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginTemplateAudioProcessorEditor)
};
