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
    // Plugin-safe: scope LookAndFeel to this editor only. setDefaultLookAndFeel replaces the
    // process-wide default and can crash or corrupt hosts that share a GUI stack (e.g. some
    // embedded / multi-plugin shells) when the plugin is first opened.
    setLookAndFeel (&lnf_);

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

    // NOTE: per-format behavior MUST be a runtime wrapperType check, not
    // #if JucePlugin_Build_AAX. This is a single shared-code compilation where
    // JucePlugin_Build_AAX/VST3/Standalone are ALL 1, so the macro is true in
    // every format. wrapperType is the only correct discriminator at runtime.
    const bool isAAX = (audioProcessor.wrapperType == juce::AudioProcessor::wrapperType_AAX);

    if (isAAX)
    {
        // Pro Tools: no free corner-drag resize; use the discrete size presets.
        setResizable (false, false);
    }
    else
    {
        setResizable (true, true);
        setResizeLimits (1100, 720, 4096, 4096);
    }

    mainView.onSizePresetChanged = [this] (int percent)
    {
        applyEditorSizePreset (percent);
    };

    // Restore State Size or Default to Screen 70% (never below minimum)
    const int minW = kBaseEditorWidth;
    const int minH = kBaseEditorHeight;
    const int storedW = p.getEditorWidth();
    const int storedH = p.getEditorHeight();
    currentEditorSizePreset_ = clampEditorSizePreset (p.getEditorSizePreset());

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

    audioProcessor.setEditorSizePreset (currentEditorSizePreset_);
    mainView.setSizePresetPercent (currentEditorSizePreset_);

    // Build stamp (version + compile date/time) so the loaded build is always
    // identifiable on screen. __DATE__/__TIME__ reflect when PluginEditor.cpp was
    // compiled, which updates on every rebuild that touches it.
    const juce::String buildInfoText = "AnalyzerPro v" + juce::String (JucePlugin_VersionString)
                                           + "  \xe2\x80\xa2  build " __DATE__ " " __TIME__;
    buildInfoLabel_.setText (buildInfoText, juce::dontSendNotification);
    buildInfoLabel_.setJustificationType (juce::Justification::centredLeft);
    buildInfoLabel_.setInterceptsMouseClicks (false, false);
    buildInfoLabel_.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.45f));
    buildInfoLabel_.setFont (juce::Font (juce::FontOptions (11.0f)));
    addAndMakeVisible (buildInfoLabel_);
    buildInfoLabel_.toFront (false);
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
#if JucePlugin_Build_AAX
    DBG ("[AAX] PluginEditor::resized " << getWidth() << " x " << getHeight()
         << " scale=" << juce::String (getDesktopScaleFactor(), 3));
#endif
#if JUCE_DEBUG
    debugGrid.clearDebugRects();
#endif
    mainView.setBounds (getLocalBounds());
#if JUCE_DEBUG
    debugGrid.setBounds (getLocalBounds());
    debugGrid.toFront (false);
#endif
    audioProcessor.setEditorSize (getWidth(), getHeight());
    audioProcessor.setEditorSizePreset (currentEditorSizePreset_);

    // Build stamp, bottom-left near the host's status footer.
    buildInfoLabel_.setBounds (8, getHeight() - 16, 460, 14);
    buildInfoLabel_.toFront (false);
}

int AnalayzerProAudioProcessorEditor::clampEditorSizePreset (int percent) noexcept
{
    if (percent >= 150)
        return 150;
    if (percent >= 125)
        return 125;
    return 100;
}

int AnalayzerProAudioProcessorEditor::deriveEditorSizePreset (int width, int height) noexcept
{
    const int fromWidth = juce::roundToInt (100.0 * static_cast<double> (width) / static_cast<double> (kBaseEditorWidth));
    const int fromHeight = juce::roundToInt (100.0 * static_cast<double> (height) / static_cast<double> (kBaseEditorHeight));
    return clampEditorSizePreset (juce::jmax (fromWidth, fromHeight));
}

juce::Rectangle<int> AnalayzerProAudioProcessorEditor::getPresetBoundsForPercent (int percent) const
{
    percent = clampEditorSizePreset (percent);

    const int desiredW = kBaseEditorWidth * percent / 100;
    const int desiredH = kBaseEditorHeight * percent / 100;
    int maxW = kMaxEditorSize;
    int maxH = kMaxEditorSize;

    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        maxW = juce::jmax (kBaseEditorWidth, display->userArea.getWidth());
        maxH = juce::jmax (kBaseEditorHeight, display->userArea.getHeight());
    }

    return { 0, 0,
             juce::jlimit (kBaseEditorWidth, maxW, desiredW),
             juce::jlimit (kBaseEditorHeight, maxH, desiredH) };
}

void AnalayzerProAudioProcessorEditor::applyEditorSizePreset (int percent)
{
    currentEditorSizePreset_ = clampEditorSizePreset (percent);
    const auto bounds = getPresetBoundsForPercent (currentEditorSizePreset_);
    mainView.setSizePresetPercent (currentEditorSizePreset_);
    audioProcessor.setEditorSizePreset (currentEditorSizePreset_);
    setSize (bounds.getWidth(), bounds.getHeight());
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
