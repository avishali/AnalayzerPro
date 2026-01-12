#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AnalayzerProAudioProcessorEditor::AnalayzerProAudioProcessorEditor (AnalayzerProAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      ui_ (mdsp_ui::ThemeVariant::Dark),  // Default to Dark theme (Phase 1)
      lnf_ (ui_),
      mainView (ui_, p, &p.getAPVTS())
{
    // Apply custom LookAndFeel globally
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf_);

    addAndMakeVisible (mainView);

    // PAZ-like base size
    static constexpr int kBaseW = 766;
    static constexpr int kBaseH = 476;
    
    setResizable (true, true);
    setResizeLimits (kBaseW, kBaseH, 1000, 800);
    setSize (kBaseW, kBaseH);
}


AnalayzerProAudioProcessorEditor::~AnalayzerProAudioProcessorEditor()
{
    // Shutdown MainView BEFORE destruction to stop timers and clear callbacks
    mainView.shutdown();
    
    // Clear look and feel if set
    setLookAndFeel (nullptr);
}


//==============================================================================
void AnalayzerProAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void AnalayzerProAudioProcessorEditor::resized()
{
    mainView.setBounds (getLocalBounds());
}