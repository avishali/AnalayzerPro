#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AnalayzerProAudioProcessorEditor::AnalayzerProAudioProcessorEditor (AnalayzerProAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      analyzerModule (p.getAnalyzerEngine()),  // Initialize reference to analyzer module
      ui_ (mdsp_ui::ThemeVariant::Custom),
      lnf_ (ui_),
      mainView (ui_, p, &p.getAPVTS())
{
    // Apply custom LookAndFeel globally
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf_);

    // Init Tooltips (custom overlay for rich tooltips + JUCE native for controls)
    tooltipManager_ = std::make_unique<mdsp_ui::TooltipManager> (*this, ui_);
    mainView.setTooltipManager (tooltipManager_.get());
    tooltipWindow_ = std::make_unique<juce::TooltipWindow> (this, 500);

    addAndMakeVisible (mainView);

#if JUCE_DEBUG
    setWantsKeyboardFocus (true);
    addAndMakeVisible (debugGrid);
    debugGrid.setEnabled (false);
    debugGrid.setStepPx (8);
    debugGrid.setMajorEvery (4);
    debugGrid.setShowLabels (false);
    debugGrid.setShowOriginCross (true);
    debugGrid.setShowRulers (true);
    debugGrid.setShowOuterBounds (true);
    debugGrid.setShowPanelHighlights (false);
    mainView.setDebugRectCallback ([this] (const juce::String& name, juce::Rectangle<int> r, juce::Colour c)
    {
        debugGrid.addDebugRect (name, r, c);
    });
    debugGrid.toFront (false);
#endif

    setResizable (true, true);
    setResizeLimits (1100, 720, 4096, 4096);

    // Restore State Size or Default to Screen 70% (never below minimum)
    const int minW = 1100;
    const int minH = 720;
    const int storedW = p.getEditorWidth();
    const int storedH = p.getEditorHeight();

    if (storedW >= minW && storedH >= minH)
    {
        setSize (storedW, storedH);
    }
    else
    {
        auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
        if (display != nullptr)
        {
            const auto area = display->userArea;
            const int w = juce::jmax (minW, 1300);
            const int h = juce::jmax (minH, static_cast<int> (area.getHeight() * 0.7));
            setSize (w, h);
        }
        else
        {
            setSize (juce::jmax (minW, 1300), juce::jmax (minH, 700));
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
#if JUCE_DEBUG
    debugGrid.clearDebugRects();
#endif
    mainView.setBounds (getLocalBounds());
#if JUCE_DEBUG
    debugGrid.setBounds (getLocalBounds());
    debugGrid.toFront (false);
#endif
    audioProcessor.setEditorSize (getWidth(), getHeight());
}

#if JUCE_DEBUG
bool AnalayzerProAudioProcessorEditor::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress ('g') || k == juce::KeyPress ('G'))
    {
        debugGrid.setEnabled (!debugGrid.isEnabled());
        return true;
    }
    if (k == juce::KeyPress ('r') || k == juce::KeyPress ('R'))
    {
        debugGrid.setShowRulers (!debugGrid.getShowRulers());
        return true;
    }
    if (k == juce::KeyPress ('o') || k == juce::KeyPress ('O'))
    {
        debugGrid.setShowOuterBounds (!debugGrid.getShowOuterBounds());
        return true;
    }
    if (k == juce::KeyPress ('l') || k == juce::KeyPress ('L'))
    {
        debugGrid.setShowLabels (!debugGrid.getShowLabels());
        return true;
    }
    if (k == juce::KeyPress ('p') || k == juce::KeyPress ('P'))
    {
        debugGrid.setShowPanelHighlights (!debugGrid.getShowPanelHighlights());
        return true;
    }
    if (k == juce::KeyPress ('+') || k == juce::KeyPress ('='))
    {
        debugGrid.setStepPx (debugGrid.getStepPx() + 1);
        return true;
    }
    if (k == juce::KeyPress ('-'))
    {
        debugGrid.setStepPx (debugGrid.getStepPx() - 1);
        return true;
    }
    return AudioProcessorEditor::keyPressed (k);
}
#endif
