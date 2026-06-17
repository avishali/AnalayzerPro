#include "PluginProcessor.h"
#include "PluginEditor.h"
#if ANALYZERPRO_DEV_LOOK_PANEL
#include "ui/dev/DevLookControlsComponent.h"
#endif
#if defined(ANALYZERPRO_METAL_EDITOR) && ANALYZERPRO_METAL_EDITOR
#include "ui/analyzer/metal/MetalEditorRenderer.h"
#include <cstdlib>
#endif

#if !defined(ANALYZERPRO_GIT_SHORT_HASH)
#define ANALYZERPRO_GIT_SHORT_HASH "unknown"
#endif

#if defined(ANALYZERPRO_METAL_EDITOR) && ANALYZERPRO_METAL_EDITOR
namespace
{
bool isAaxMetalEditorDisabled() noexcept
{
    const auto* value = std::getenv ("ANALYZERPRO_DISABLE_AAX_METAL");
    return value != nullptr && value[0] == '1';
}
}
#endif

namespace
{
constexpr int scaledEditorSize (int base, int percent) noexcept
{
    return (base * percent + 50) / 100;
}
}

#if ANALYZERPRO_DEV_LOOK_PANEL
AnalayzerProAudioProcessorEditor::DevLookWindow::DevLookWindow (juce::Colour backgroundColour)
    : juce::DocumentWindow ("AnalyzerPro DEV Look",
                            backgroundColour,
                            juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton)
{
    setUsingNativeTitleBar (true);
}

void AnalayzerProAudioProcessorEditor::DevLookWindow::closeButtonPressed()
{
    if (onClose)
        onClose();
}
#endif

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
        setResizeLimits (scaledEditorSize (kBaseEditorWidth, kMinEditorSizePreset),
                         scaledEditorSize (kBaseEditorHeight, kMinEditorSizePreset),
                         kMaxEditorSize,
                         kMaxEditorSize);
    }

    mainView.onSizePresetChanged = [this] (int percent)
    {
        applyEditorSizePreset (percent);
    };

#if ANALYZERPRO_DEV_LOOK_PANEL
    mainView.onToggleDevLookPanel = [this] { toggleDevLookPanel(); };
#endif

    // Restore a valid stored size, otherwise open at the 16:9 100% preset.
    const int minW = scaledEditorSize (kBaseEditorWidth, kMinEditorSizePreset);
    const int minH = scaledEditorSize (kBaseEditorHeight, kMinEditorSizePreset);
    const int storedW = p.getEditorWidth();
    const int storedH = p.getEditorHeight();
    currentEditorSizePreset_ = clampEditorSizePreset (p.getEditorSizePreset());

    if (storedW >= minW && storedH >= minH && storedW <= kMaxEditorSize && storedH <= kMaxEditorSize)
    {
        setSize (storedW, storedH);
    }
    else
    {
        int defaultPreset = 100;
        auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
        if (display != nullptr)
        {
            const auto area = display->userArea;
            if (area.getWidth() < kBaseEditorWidth || area.getHeight() < kBaseEditorHeight)
                defaultPreset = kMinEditorSizePreset;
        }

        currentEditorSizePreset_ = defaultPreset;
        const auto bounds = getPresetBoundsForPercent (defaultPreset);
        setSize (bounds.getWidth(), bounds.getHeight());
    }

    audioProcessor.setEditorSizePreset (currentEditorSizePreset_);
    mainView.setSizePresetPercent (currentEditorSizePreset_);

    // Build stamp (version + compile date/time) so the loaded build is always
    // identifiable on screen. __DATE__/__TIME__ reflect when PluginEditor.cpp was
    // compiled, which updates on every rebuild that touches it.
    const juce::String buildInfoText = "AnalyzerPro v" + juce::String (JucePlugin_VersionString)
                                           + "  \xe2\x80\xa2  build " __DATE__ " " __TIME__
                                           + "  \xe2\x80\xa2  " + juce::String (ANALYZERPRO_GIT_SHORT_HASH);
    buildInfoLabel_.setText (buildInfoText, juce::dontSendNotification);
    buildInfoLabel_.setJustificationType (juce::Justification::centredLeft);
    buildInfoLabel_.setInterceptsMouseClicks (false, false);
    buildInfoLabel_.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.45f));
    buildInfoLabel_.setFont (juce::Font (juce::FontOptions (11.0f)));
    addAndMakeVisible (buildInfoLabel_);
    buildInfoLabel_.toFront (false);

#if defined(ANALYZERPRO_METAL_EDITOR) && ANALYZERPRO_METAL_EDITOR
    if (isAAX && ! isAaxMetalEditorDisabled())
        startMetalSurfaceIfNeeded();
#endif
}


AnalayzerProAudioProcessorEditor::~AnalayzerProAudioProcessorEditor()
{
#if ANALYZERPRO_DEV_LOOK_PANEL
    devLookWindow_.reset();
#endif
#if defined(ANALYZERPRO_METAL_EDITOR) && ANALYZERPRO_METAL_EDITOR
    editorSurface_.reset();
#endif

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
    buildInfoLabel_.setBounds (8, getHeight() - 16, 560, 14);
    buildInfoLabel_.toFront (false);

#if defined(ANALYZERPRO_METAL_EDITOR) && ANALYZERPRO_METAL_EDITOR
    if (editorSurface_ != nullptr)
        editorSurface_->resized();
#endif
}

#if defined(ANALYZERPRO_METAL_EDITOR) && ANALYZERPRO_METAL_EDITOR
void AnalayzerProAudioProcessorEditor::setMetalTraceSuppressedForChromeCapture (bool shouldSuppress) noexcept
{
    mainView.setMetalTraceSuppressedForChromeCapture (shouldSuppress);
}

bool AnalayzerProAudioProcessorEditor::fillMetalAnalyzerFrame (AnalyzerPro::metal::MetalAnalyzerFrame& frame,
                                                               float backingScale)
{
    return mainView.fillMetalAnalyzerFrame (frame, *this, backingScale);
}

AnalyzerPro::metal::MetalHostMechanism AnalayzerProAudioProcessorEditor::getConfiguredMetalHostMechanism()
{
    // Phase 1A is locked to BackingLayer: native JUCE input worked in PT, so no
    // cover-view input forwarding is built on the active path.
    return AnalyzerPro::metal::MetalHostMechanism::BackingLayer;
}

void AnalayzerProAudioProcessorEditor::startMetalSurfaceIfNeeded()
{
    if (audioProcessor.wrapperType != juce::AudioProcessor::wrapperType_AAX)
        return;

    juce::ignoreUnused (getConfiguredMetalHostMechanism());

    if (editorSurface_ == nullptr)
        editorSurface_ = std::make_unique<AnalyzerPro::metal::MetalEditorRenderer>();

    if (! editorSurface_->start (*this))
        editorSurface_.reset();

    if (editorSurface_ != nullptr)
    {
        mainView.setMetalChromeCaptureCallback ([this]()
        {
            if (editorSurface_ != nullptr)
                editorSurface_->requestChromeCapture();
        });
    }
}
#endif

int AnalayzerProAudioProcessorEditor::clampEditorSizePreset (int percent) noexcept
{
    static constexpr int presets[] { 75, 100, 125, 150 };
    int nearest = presets[0];
    int nearestDistance = percent > nearest ? percent - nearest : nearest - percent;

    for (const auto preset : presets)
    {
        const int distance = percent > preset ? percent - preset : preset - percent;
        if (distance < nearestDistance)
        {
            nearest = preset;
            nearestDistance = distance;
        }
    }

    return nearest;
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

    const int desiredW = scaledEditorSize (kBaseEditorWidth, percent);
    const int desiredH = scaledEditorSize (kBaseEditorHeight, percent);
    const int minW = scaledEditorSize (kBaseEditorWidth, kMinEditorSizePreset);
    const int minH = scaledEditorSize (kBaseEditorHeight, kMinEditorSizePreset);
    int maxW = kMaxEditorSize;
    int maxH = kMaxEditorSize;

    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        maxW = juce::jmax (minW, display->userArea.getWidth());
        maxH = juce::jmax (minH, display->userArea.getHeight());
    }

    return { 0, 0,
             juce::jlimit (minW, maxW, desiredW),
             juce::jlimit (minH, maxH, desiredH) };
}

void AnalayzerProAudioProcessorEditor::applyEditorSizePreset (int percent)
{
    currentEditorSizePreset_ = clampEditorSizePreset (percent);
    const auto bounds = getPresetBoundsForPercent (currentEditorSizePreset_);
    mainView.setSizePresetPercent (currentEditorSizePreset_);
    audioProcessor.setEditorSizePreset (currentEditorSizePreset_);
    setSize (bounds.getWidth(), bounds.getHeight());
}

#if ANALYZERPRO_DEV_LOOK_PANEL
void AnalayzerProAudioProcessorEditor::toggleDevLookPanel()
{
    if (devLookWindow_ != nullptr)
    {
        closeDevLookPanelWindow();
        return;
    }

    auto window = std::make_unique<DevLookWindow> (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    window->onClose = [this] { closeDevLookPanelWindow(); };
    window->setResizable (true, true);
    window->setResizeLimits (320, 480, 900, 1600);
    window->setContentOwned (new AnalyzerPro::DevLookControlsComponent (
                                 mainView.analyzerDevLookTunables(),
                                 [this] { mainView.notifyDevLookTunablesChanged(); }),
                             true);
    window->centreWithSize (380, 720);
    window->setVisible (true);
    devLookWindow_ = std::move (window);
}

void AnalayzerProAudioProcessorEditor::closeDevLookPanelWindow()
{
    devLookWindow_.reset();
}
#endif

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
