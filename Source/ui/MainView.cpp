#include "MainView.h"
#include "../PluginProcessor.h"
#include "analyzer/rta1_import/RTADisplay.h"
#include <mdsp_ui/UiContext.h>

//==============================================================================
MainView::MainView (mdsp_ui::UiContext& ui, AnalayzerProAudioProcessor& p, juce::AudioProcessorValueTreeState* apvts)
    : audioProcessor (p),
      apvts_ (apvts),
      controls_ (apvts),
      ui_ (ui),  // Store reference to shared UiContext from PluginEditor
      header_ (ui_),
      rail_ (ui_),
      footer_ (ui_),
      analyzerView_ (p),
      outputMeters_ (ui_, p, MeterGroupComponent::GroupType::Output),
      inputMeters_ (ui_, p, MeterGroupComponent::GroupType::Input)
{
    setWantsKeyboardFocus (true);
    setFocusContainerType (juce::Component::FocusContainerType::focusContainer);
    addKeyListener (this);

    // Add layout components
    addAndMakeVisible (header_);
    addAndMakeVisible (rail_);
    addAndMakeVisible (footer_);
    addAndMakeVisible (analyzerView_);
    addAndMakeVisible (phaseView_);
    addAndMakeVisible (outputMeters_);
    addAndMakeVisible (inputMeters_);
    
    // Initialize header and rail with control binder
    header_.setControlBinder (controls_.getBinder());
    rail_.setControlBinder (controls_.getBinder());
    rail_.setResetPeaksCallback ([this]
    {
        triggerResetPeaks();
    });
    rail_.setDbRangeChangedCallback ([this] (int selectedId)
    {
        AnalyzerDisplayView::DbRange range = AnalyzerDisplayView::DbRange::Minus120;
        switch (selectedId)
        {
            case 1: range = AnalyzerDisplayView::DbRange::Minus60; break;
            case 2: range = AnalyzerDisplayView::DbRange::Minus90; break;
            case 3: range = AnalyzerDisplayView::DbRange::Minus120; break;
            default: range = AnalyzerDisplayView::DbRange::Minus120; break;
        }
        if (apvts_ != nullptr)
        {
            if (auto* param = apvts_->getParameter ("DbRange"))
            {
                const int idx = juce::jlimit (0, 2, selectedId - 1);
                const float norm = static_cast<float> (idx) / 2.0f;
                param->beginChangeGesture();
                param->setValueNotifyingHost (norm);
                param->endChangeGesture();
            }
        }

        analyzerView_.setDbRange (range);
        header_.setDbRangeSelectedId (selectedId);
    });

    // Wire parameter changes to AnalyzerEngine and AnalyzerDisplayView
    if (apvts != nullptr)
    {
        apvts->addParameterListener ("Mode", this);
        apvts->addParameterListener ("FftSize", this);
        apvts->addParameterListener ("Averaging", this);
        apvts->addParameterListener ("PeakHold", this);
        apvts->addParameterListener ("Hold", this);
        apvts->addParameterListener ("PeakDecay", this);
        apvts->addParameterListener ("DbRange", this);
        apvts->addParameterListener ("DisplayGain", this);
        apvts->addParameterListener ("Tilt", this);
    }

    // HeaderBar is authoritative for Mode/FFT/Averaging controls
    // Set default mode to FFT (HeaderBar control will update via parameterChanged)
    analyzerView_.setMode (AnalyzerDisplayView::Mode::FFT);

    header_.onDbRangeChanged = [this] (int id)
    {
        using R = AnalyzerDisplayView::DbRange;
        R r = R::Minus120;
        if (id == 1) r = R::Minus60;
        else if (id == 2) r = R::Minus90;
        else if (id == 3) r = R::Minus120;

        if (apvts_ != nullptr)
        {
            if (auto* param = apvts_->getParameter ("DbRange"))
            {
                const int idx = juce::jlimit (0, 2, id - 1);
                const float norm = static_cast<float> (idx) / 2.0f;
                param->beginChangeGesture();
                param->setValueNotifyingHost (norm);
                param->endChangeGesture();
            }
        }

        // Apply immediately for responsiveness (listener will also keep things in sync)
        analyzerView_.setDbRange (r);
    };

    header_.onPeakRangeChanged = [this] (int id)
    {
        using R = AnalyzerDisplayView::DbRange;
        R r = R::Minus90;
        if (id == 1) r = R::Minus60;
        else if (id == 2) r = R::Minus90;
        else if (id == 3) r = R::Minus120;
        analyzerView_.setPeakDbRange (r);
    };

    header_.onResetPeaks = [this]
    {
        triggerResetPeaks();
    };

    // Apply initial DbRange from APVTS (startup/session restore)
    if (apvts_ != nullptr)
    {
        if (auto* raw = apvts_->getRawParameterValue ("DbRange"))
        {
            const int idx = juce::jlimit (0, 2, juce::roundToInt (raw->load()));
            analyzerView_.setDbRangeFromChoiceIndex (idx);
            header_.setDbRangeSelectedId (idx + 1);
        }
    }

    //setSize (900, 650);  // Slightly bigger to fit all controls

#if JUCE_DEBUG
    auditApvtsParameters();
#endif
}

MainView::~MainView()
{
    shutdown();
}

void MainView::shutdown()
{
    if (isShutdown)
        return;
    
    isShutdown = true;
    
    // Remove parameter listeners
    if (apvts_ != nullptr)
    {
        apvts_->removeParameterListener ("Mode", this);
        apvts_->removeParameterListener ("FftSize", this);
        apvts_->removeParameterListener ("Averaging", this);
        apvts_->removeParameterListener ("PeakHold", this);
        apvts_->removeParameterListener ("Hold", this);
        apvts_->removeParameterListener ("PeakDecay", this);
        apvts_->removeParameterListener ("DbRange", this);
        apvts_->removeParameterListener ("DisplayGain", this);
        apvts_->removeParameterListener ("Tilt", this);
    }
    
    // Shutdown child views that have timers/listeners
    analyzerView_.shutdown();
    
    // Clear control binder attachments (must happen before controls are destroyed)
    controls_.getBinder().clear();
}

//==============================================================================
void MainView::parameterChanged (const juce::String& parameterID, float newValue)
{
    // Handle parameter changes on UI thread (safe to call AnalyzerEngine setters)
    if (parameterID == "Mode")
    {
        // HeaderBar Mode control is authoritative
        // Convert choice index to AnalyzerDisplayView::Mode (FFT=0, BANDS=1, LOG=2)
        const int index = juce::roundToInt (newValue);
        AnalyzerDisplayView::Mode viewMode = AnalyzerDisplayView::Mode::FFT;
        switch (index)
        {
            case 0:
                viewMode = AnalyzerDisplayView::Mode::FFT;
                break;
            case 1:
                viewMode = AnalyzerDisplayView::Mode::BAND;
                break;
            case 2:
                viewMode = AnalyzerDisplayView::Mode::LOG;
                break;
            default:
                viewMode = AnalyzerDisplayView::Mode::FFT;
                break;
        }
        
        // Update analyzer view
        analyzerView_.setMode (viewMode);
        
#if JUCE_DEBUG
        // Assertion: Verify mode sync
        const AnalyzerDisplayView::Mode currentMode = analyzerView_.getMode();
        const int expectedIndex = (currentMode == AnalyzerDisplayView::Mode::FFT) ? 0
                                : (currentMode == AnalyzerDisplayView::Mode::BAND) ? 1 : 2;
        if (expectedIndex != index)
        {
            DBG ("MODE SYNC ERROR: HeaderBar control index=" << index 
                 << " but AnalyzerDisplayView mode=" << (currentMode == AnalyzerDisplayView::Mode::FFT ? "FFT" : (currentMode == AnalyzerDisplayView::Mode::BAND ? "BANDS" : "LOG")));
            jassertfalse;
        }
#endif
    }
    else if (parameterID == "FftSize")
    {
        // Convert choice index to FFT size (handled in PluginProcessor::parameterChanged)
        // This is redundant but ensures UI thread safety
        const int sizes[] = { 1024, 2048, 4096, 8192 };
        const int index = juce::roundToInt (newValue);
        if (index >= 0 && index < 4)
            audioProcessor.getAnalyzerEngine().setFftSize (sizes[index]);
    }
    else if (parameterID == "Averaging")
    {
        // Convert choice index to ms (handled in PluginProcessor::parameterChanged)
        const float avgMs[] = { 0.0f, 50.0f, 100.0f, 250.0f, 500.0f, 1000.0f };
        const int index = juce::roundToInt (newValue);
        if (index >= 0 && index < 6)
            audioProcessor.getAnalyzerEngine().setAveragingMs (avgMs[index]);
    }
    else if (parameterID == "PeakHold")
    {
        // Enable/disable peak hold overlay
        audioProcessor.getAnalyzerEngine().setPeakHoldEnabled (newValue > 0.5f);
    }
    else if (parameterID == "Hold")
    {
        audioProcessor.getAnalyzerEngine().setHold (newValue > 0.5f);
    }
    else if (parameterID == "PeakDecay")
    {
        audioProcessor.getAnalyzerEngine().setPeakDecayDbPerSec (newValue);
    }
    else if (parameterID == "DbRange")
    {
        const int idx = juce::jlimit (0, 2, juce::roundToInt (newValue));
        analyzerView_.setDbRangeFromChoiceIndex (idx);
        header_.setDbRangeSelectedId (idx + 1);
    }
    else if (parameterID == "DisplayGain")
    {
        // Display gain is UI-only, applied to RTADisplay (not AnalyzerEngine)
        analyzerView_.getRTADisplay().setDisplayGainDb (newValue);
    }
    else if (parameterID == "Tilt")
    {
        // Tilt is UI-only, applied to RTADisplay (not AnalyzerEngine)
        // Convert choice index to TiltMode (Flat=0, Pink=1, White=2)
        const int index = juce::roundToInt (newValue);
        RTADisplay::TiltMode tiltMode = RTADisplay::TiltMode::Flat;
        switch (index)
        {
            case 0:
                tiltMode = RTADisplay::TiltMode::Flat;
                break;
            case 1:
                tiltMode = RTADisplay::TiltMode::Pink;
                break;
            case 2:
                tiltMode = RTADisplay::TiltMode::White;
                break;
            default:
                tiltMode = RTADisplay::TiltMode::Flat;
                break;
        }
        analyzerView_.getRTADisplay().setTiltMode (tiltMode);
    }
}

bool MainView::keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent)
{
    // Do not consume "R" while typing in a text field.
    if (dynamic_cast<juce::TextEditor*> (originatingComponent) != nullptr)
        return false;

#if JUCE_MAC
    const auto mods = juce::ModifierKeys::commandModifier | juce::ModifierKeys::altModifier;
    const juce::KeyPress optCmdRLower { 'r', mods, 0 };
    const juce::KeyPress optCmdRUpper { 'R', mods, 0 };
    if (key == optCmdRLower || key == optCmdRUpper)
    {
        triggerResetPeaks();
        return true;
    }
#endif

    const juce::KeyPress dLower { 'd', juce::ModifierKeys{}, 0 };
    const juce::KeyPress dUpper { 'D', juce::ModifierKeys{}, 0 };
    if (key == dLower || key == dUpper)
    {
        using R = AnalyzerDisplayView::DbRange;

        const auto current = analyzerView_.getDbRange();
        R next = R::Minus60;

        switch (current)
        {
            case R::Minus60:  next = R::Minus90;  break;
            case R::Minus90:  next = R::Minus120; break;
            case R::Minus120: next = R::Minus60;  break;
        }

        analyzerView_.setDbRange (next);

        const int nextId = (next == R::Minus60) ? 1 : (next == R::Minus90) ? 2 : 3;
        header_.setDbRangeSelectedId (nextId);
        return true;
    }

    return false;
}

void MainView::triggerResetPeaks()
{
    audioProcessor.getAnalyzerEngine().resetPeaks();
    audioProcessor.resetMeterClipLatches();
    analyzerView_.triggerPeakFlash();
    analyzerView_.repaint();
}

void MainView::paint (juce::Graphics& g)
{
    // Background from shared theme (variant-aware)
    const auto& theme = ui_.theme();
    g.fillAll (theme.background);

#if JUCE_DEBUG
    // Temporary debug overlay
    g.setFont (juce::Font (juce::FontOptions().withHeight (10.0f)));
    g.setColour (juce::Colours::red.withAlpha (0.7f));

    // Draw outer bounds
    g.drawRect (debugOuter.toFloat(), 2.0f);
    g.drawText (juce::String ("Outer: ") + juce::String (debugOuter.getWidth()) + "x" + juce::String (debugOuter.getHeight()),
                debugOuter.getX() + 2, debugOuter.getY() + 2, 200, 12, juce::Justification::centredLeft);

    // Draw content bounds
    g.setColour (juce::Colours::orange.withAlpha (0.7f));
    g.drawRect (debugContent.toFloat(), 2.0f);
    g.drawText (juce::String ("Content: ") + juce::String (debugContent.getWidth()) + "x" + juce::String (debugContent.getHeight()),
                debugContent.getX() + 2, debugContent.getY() + 2, 200, 12, juce::Justification::centredLeft);

    // Draw header
    g.setColour (juce::Colours::yellow.withAlpha (0.7f));
    g.drawRect (debugHeader.toFloat(), 1.5f);
    g.drawText (juce::String ("Header: ") + juce::String (debugHeader.getWidth()) + "x" + juce::String (debugHeader.getHeight()),
                debugHeader.getX() + 2, debugHeader.getY() + 2, 200, 12, juce::Justification::centredLeft);

    // Draw footer
    g.setColour (juce::Colours::cyan.withAlpha (0.7f));
    g.drawRect (debugFooter.toFloat(), 1.5f);
    g.drawText (juce::String ("Footer: ") + juce::String (debugFooter.getWidth()) + "x" + juce::String (debugFooter.getHeight()),
                debugFooter.getX() + 2, debugFooter.getY() + 2, 200, 12, juce::Justification::centredLeft);

    // Draw rail
    g.setColour (juce::Colours::magenta.withAlpha (0.7f));
    g.drawRect (debugRail.toFloat(), 1.5f);
    g.drawText (juce::String ("Rail: ") + juce::String (debugRail.getWidth()) + "x" + juce::String (debugRail.getHeight()),
                debugRail.getX() + 2, debugRail.getY() + 2, 200, 12, juce::Justification::centredLeft);

    // Draw left area
    g.setColour (juce::Colours::green.withAlpha (0.7f));
    g.drawRect (debugLeft.toFloat(), 1.5f);
    g.drawText (juce::String ("Left: ") + juce::String (debugLeft.getWidth()) + "x" + juce::String (debugLeft.getHeight()),
                debugLeft.getX() + 2, debugLeft.getY() + 2, 200, 12, juce::Justification::centredLeft);

    // Draw analyzer top
    g.setColour (juce::Colours::blue.withAlpha (0.7f));
    g.drawRect (debugAnalyzerTop.toFloat(), 2.0f);
    g.drawText (juce::String ("Analyzer: ") + juce::String (debugAnalyzerTop.getWidth()) + "x" + juce::String (debugAnalyzerTop.getHeight()),
                debugAnalyzerTop.getX() + 2, debugAnalyzerTop.getY() + 2, 200, 12, juce::Justification::centredLeft);

    // Draw phase bottom
    g.setColour (juce::Colours::lightblue.withAlpha (0.7f));
    g.drawRect (debugPhaseBottom.toFloat(), 1.5f);
    g.drawText (juce::String ("Phase: ") + juce::String (debugPhaseBottom.getWidth()) + "x" + juce::String (debugPhaseBottom.getHeight()),
                debugPhaseBottom.getX() + 2, debugPhaseBottom.getY() + 2, 200, 12, juce::Justification::centredLeft);
#endif
}

void MainView::resized()
{
    // Debug: capture full bounds
    debugOuter = getLocalBounds();

    // Layout constants
    static constexpr int padding = 10;
    static constexpr int headerH = 32;
    static constexpr int footerH = 22;
    static constexpr int metersW = 60;  // Fixed width for meters

    auto bounds = getLocalBounds().reduced (padding);
    debugContent = bounds;

    // Slice order: footer, header, then split remaining into analyzer and controls
    // 1) Footer from bottom
    auto footerArea = bounds.removeFromBottom (footerH);
    debugFooter = footerArea;
    footer_.setBounds (footerArea);

    // 2) Header from top
    auto headerArea = bounds.removeFromTop (headerH);
    debugHeader = headerArea;
    header_.setBounds (headerArea);

    // 3) Calculate responsive controls height
    int controlsH = juce::jlimit (110, 180, bounds.getHeight() / 4);

    // 4) Split remaining area into analyzer (top) and controls (bottom)
    auto analyzerArea = bounds;
    auto controlsArea = analyzerArea.removeFromBottom (controlsH);
    debugRail = controlsArea;
    rail_.setBounds (controlsArea);

    // 5) Save analyzerArea dimensions before removing meter widths
    const int analyzerY = analyzerArea.getY();
    const int analyzerHeight = analyzerArea.getHeight();

    // 6) Place meters aligned with analyzerArea (same height as analyzerView_)
    auto inputMetersArea = analyzerArea.removeFromLeft (metersW);
    inputMetersArea.setY (analyzerY);
    inputMetersArea.setHeight (analyzerHeight);
    inputMeters_.setBounds (inputMetersArea);

    auto outputMetersArea = analyzerArea.removeFromRight (metersW);
    outputMetersArea.setY (analyzerY);
    outputMetersArea.setHeight (analyzerHeight);
    outputMeters_.setBounds (outputMetersArea);

    // 7) Place analyzer view in remaining center of analyzerArea
    debugLeft = analyzerArea;
    debugAnalyzerTop = analyzerArea;
    analyzerView_.setBounds (analyzerArea);

    // Phase view hidden for now (or can be placed elsewhere if needed)
    phaseView_.setBounds (juce::Rectangle<int>());
    debugPhaseBottom = juce::Rectangle<int>();
}

#if JUCE_DEBUG
void MainView::auditApvtsParameters()
{
    static bool auditRun = false;
    if (auditRun)
        return;
    auditRun = true;

    if (apvts_ == nullptr)
        return;

    // Collect all APVTS parameter IDs (manual list from createParameterLayout)
    // Note: APVTS doesn't expose iteration API, so we manually enumerate known parameters
    std::set<juce::String> apvtsParams;
    apvtsParams.insert ("Mode");
    apvtsParams.insert ("FftSize");
    apvtsParams.insert ("Averaging");
    apvtsParams.insert ("PeakHold");
    apvtsParams.insert ("Hold");
    apvtsParams.insert ("PeakDecay");
    apvtsParams.insert ("DisplayGain");
    apvtsParams.insert ("Tilt");
    apvtsParams.insert ("DbRange");

    // Collect UI-represented parameters (best effort manual set)
    std::set<juce::String> uiRepresented;
    uiRepresented.insert ("Mode");
    uiRepresented.insert ("FftSize");
    uiRepresented.insert ("Averaging");
    uiRepresented.insert ("PeakHold");
    uiRepresented.insert ("Hold");
    uiRepresented.insert ("PeakDecay");
    uiRepresented.insert ("DbRange");
    uiRepresented.insert ("DisplayGain");
    uiRepresented.insert ("Tilt");

    // Print all APVTS parameters
    for (const auto& id : apvtsParams)
    {
        DBG ("APVTS param: " + id);
    }

    // Print UI-represented parameters
    for (const auto& id : uiRepresented)
    {
        DBG ("UI represented: " + id);
    }

    // Find missing parameters
    for (const auto& id : apvtsParams)
    {
        if (uiRepresented.find (id) == uiRepresented.end())
        {
            DBG ("MISSING UI FOR PARAM: " + id);
        }
    }
}
#endif
