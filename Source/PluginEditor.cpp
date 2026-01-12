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


    // Init Tooltips
    tooltipManager_ = std::make_unique<mdsp_ui::TooltipManager> (*this, ui_);
    mainView.setTooltipManager (tooltipManager_.get());

    addAndMakeVisible (mainView);

    // Restore State Size or Default to Screen 70%
    const int storedW = p.getEditorWidth();
    const int storedH = p.getEditorHeight();
    
    setResizeLimits (800, 400, 10000, 10000);

    if (storedW > 0 && storedH > 0)
    {
        // Use stored size
        setSize (storedW, storedH);
    }
    else
    {
        // First Run: Default 1300px Width, 70% Height (User Requested)
        auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
        if (display != nullptr)
        {
            const auto area = display->userArea;
            setSize (1300, static_cast<int> (area.getHeight() * 0.7));
        }
        else
        {
            setSize (1300, 700);
        }
    }
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
    audioProcessor.setEditorSize (getWidth(), getHeight());
}