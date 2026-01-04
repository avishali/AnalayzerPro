#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PluginTemplateAudioProcessorEditor::PluginTemplateAudioProcessorEditor (PluginTemplateAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), mainView (p)
{
    addAndMakeVisible (mainView);
        // REPLACE — Source/PluginEditor.cpp (inside constructor)
    setSize (audioProcessor.getParameters().getEditorWidth(),
    audioProcessor.getParameters().getEditorHeight());


         // ADD — PluginTemplateAudioProcessorEditor constructor (Source/PluginEditor.cpp)
#if PLUGIN_EDITOR_RESIZABLE
    setResizable (true, true);
    setResizeLimits (360, 360, 900, 900);
#endif
    // END REPLACE  
}


PluginTemplateAudioProcessorEditor::~PluginTemplateAudioProcessorEditor()
{   
         
}


//==============================================================================
void PluginTemplateAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

// ADD — Source/PluginEditor.cpp
void PluginTemplateAudioProcessorEditor::resized()
{
    mainView.setBounds (getLocalBounds());

#if PLUGIN_EDITOR_RESIZABLE
    audioProcessor.getParameters().setEditorSize (getWidth(), getHeight());
#endif
}
