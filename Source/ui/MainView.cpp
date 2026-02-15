#include "MainView.h"
#include "layout/LayoutConstants.h"
#include "../PluginProcessor.h"
#include "analyzer/RTADisplay.h"
#include <mdsp_ui/UiContext.h>

using namespace AnalyzerPro::Layout;

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
      stereoScopeView_ (ui, p.getAnalyzerEngine().getStereoScopeAnalyzer()),
      phaseFanScopeComponent_ (ui),
      loudnessPanel_ (ui, p),
      outputMeters_ (ui_, p, MeterGroupComponent::GroupType::Output),
      inputMeters_ (ui_, p, MeterGroupComponent::GroupType::Input)
{
    setWantsKeyboardFocus (true);
    setFocusContainerType (juce::Component::FocusContainerType::focusContainer);
    addKeyListener (this);

    // Add layout components
    addAndMakeVisible (header_);
    addAndMakeVisible (railViewport_);
    railViewport_.setViewedComponent (&rail_, false);
    railViewport_.setScrollBarsShown (true, false);
    railViewport_.setScrollBarThickness (10);
    railViewport_.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::never);
    addAndMakeVisible (footer_);
    addAndMakeVisible (analyzerView_);
    addAndMakeVisible (stereoScopeView_);
    addAndMakeVisible (phaseFanScopeComponent_);
    addAndMakeVisible (loudnessPanel_);
    addAndMakeVisible (outputMeters_);
    addAndMakeVisible (inputMeters_);
    
    // Initialize header and rail with control binder
    header_.setControlBinder (controls_.getBinder());
    header_.setManagers (&p.getPresetManager(), &p.getABStateManager());
    header_.onRailToggleClicked = [this] { toggleRail(); };
    rail_.setControlBinder (controls_.getBinder());
    rail_.setResetPeaksCallback ([this]
    {
        triggerResetPeaks();
    });
    rail_.onPreferredHeightChanged = [this] { resized(); };

    rail_.onScopeModeChanged = [this] (int id)
    {
        stereoScopeView_.setScopeMode (id == 2 ? StereoScopeView::ScopeMode::RMS : StereoScopeView::ScopeMode::Peak);
        stereoScopeView_.repaint();
    };
    rail_.onScopeShapeChanged = [this] (int id)
    {
        stereoScopeView_.setScopeShape (id == 2 ? StereoScopeView::ScopeShape::Scatter : StereoScopeView::ScopeShape::Lissajous);
        stereoScopeView_.repaint();
    };

    // Wire parameter changes to AnalyzerEngine and AnalyzerDisplayView.
    // Spectrum engine wiring (HeaderBar controls -> APVTS -> parameterChanged -> display view):
    //   fftSizeComboBox (AnalyzerFftSize) -> FftSize -> analyzerView_.setSpectrumFftOrder(order)
    //   decaySlider (PeakDecay)          -> PeakDecay -> analyzerView_.setSpectrumDecayRate(decayNorm)
    //   viewModeButton (Mode)            -> Mode -> analyzerView_.setMode(...) -> spectrumEngine.setAnalysisMode(...)
    if (apvts != nullptr)
    {
        apvts->addParameterListener ("Mode", this);
        apvts->addParameterListener ("FftSize", this);
        apvts->addParameterListener ("Averaging", this);
        apvts->addParameterListener ("HoldPeaks", this);
        apvts->addParameterListener ("PeakDecay", this);
        apvts->addParameterListener ("DbRange", this);
        apvts->addParameterListener ("DisplayGain", this);
        // CLEANUP: DUPLICATE - Removed duplicate parameter listener registration (line 76)
        // apvts->addParameterListener ("DisplayGain", this);
        apvts->addParameterListener ("Tilt", this);
        apvts->addParameterListener ("scopeChannelMode", this); // New
        apvts->addParameterListener ("meterChannelMode", this); // New
        apvts->addParameterListener ("meterPeakHold", this); // Peak Hold
        apvts->addParameterListener ("scopePeakHold", this); // Peak Hold
    }

    // ControlRail is authoritative for Mode/FFT (Analysis Mode section); set default to FFT
    analyzerView_.setMode (AnalyzerDisplayView::Mode::FFT);

    rail_.onModeChanged = [this] (int id)
    {
        // 1=FFT, 2=BAND, 3=LOG
        if (apvts_ != nullptr)
        {
            if (auto* param = apvts_->getParameter ("Mode"))
            {
                const int idx = juce::jlimit (0, 2, id - 1);
                const float norm = static_cast<float> (idx) / 2.0f;
                param->beginChangeGesture();
                param->setValueNotifyingHost (norm);
                param->endChangeGesture();
            }
        }
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

    analyzerView_.onDbRangeUserChanged = [this] (AnalyzerDisplayView::DbRange range)
    {
        if (apvts_ != nullptr)
        {
            if (auto* param = apvts_->getParameter ("DbRange"))
            {
                const int idx = static_cast<int> (range);
                const float norm = static_cast<float> (idx) / 2.0f;
                param->beginChangeGesture();
                param->setValueNotifyingHost (norm);
                param->endChangeGesture();
            }
        }
    };



    // Apply initial Mode from APVTS (startup/session restore)
    if (apvts_ != nullptr)
    {
        if (auto* raw = apvts_->getRawParameterValue ("Mode"))
        {
            const int idx = juce::jlimit (0, 2, juce::roundToInt (raw->load()));
            rail_.setMode (idx + 1);

            // Sync view immediately
            AnalyzerDisplayView::Mode viewMode = AnalyzerDisplayView::Mode::FFT;
            if (idx == 1) viewMode = AnalyzerDisplayView::Mode::BAND;
            if (idx == 2) viewMode = AnalyzerDisplayView::Mode::LOG;
            analyzerView_.setMode (viewMode);
        }
        if (auto* raw = apvts_->getRawParameterValue ("FftSize"))
        {
            const int index = juce::jlimit (0, 3, juce::roundToInt (raw->load()));
            const int order = 10 + index;
            analyzerView_.setSpectrumFftOrder (order);
        }
        if (auto* raw = apvts_->getRawParameterValue ("PeakDecay"))
        {
            const float ms = raw->load();
            const float decayNorm = juce::jlimit (0.0f, 1.0f, (ms - 100.0f) / 4900.0f);
            analyzerView_.setSpectrumDecayRate (decayNorm);
        }
    }
        
    // Apply Scope Channel Mode
    if (apvts_ != nullptr)
    {
        if (auto* raw = apvts_->getRawParameterValue ("scopeChannelMode"))
        {
            const int val = juce::roundToInt (raw->load());
            stereoScopeView_.setChannelMode (val == 0 ? StereoScopeView::ChannelMode::Stereo : StereoScopeView::ChannelMode::MidSide);
        }
        if (auto* raw = apvts_->getRawParameterValue ("meterChannelMode"))
        {
            const int val = juce::roundToInt (raw->load());
            const auto mode = (val == 0 ? MeterGroupComponent::ChannelMode::Stereo : MeterGroupComponent::ChannelMode::MidSide);
            outputMeters_.setChannelMode (mode);
            inputMeters_.setChannelMode (mode);
        }
        
        // Apply Meter Peak Hold
        if (auto* raw = apvts_->getRawParameterValue ("meterPeakHold"))
        {
            const bool hold = raw->load() > 0.5f;
            outputMeters_.setHoldEnabled (hold);
            inputMeters_.setHoldEnabled (hold);
        }
        if (auto* raw = apvts_->getRawParameterValue ("scopePeakHold"))
        {
            const bool hold = raw->load() > 0.5f;
            phaseFanScopeComponent_.setPeakHoldEnabled (hold);  // Scope peak hold applies to PhaseFanScope only, not StereoScopeView
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
        apvts_->removeParameterListener ("HoldPeaks", this);
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
        
        rail_.setMode (index + 1);

#if JUCE_DEBUG
        const AnalyzerDisplayView::Mode currentMode = analyzerView_.getMode();
        const int expectedIndex = (currentMode == AnalyzerDisplayView::Mode::FFT) ? 0
                                : (currentMode == AnalyzerDisplayView::Mode::BAND) ? 1 : 2;
        if (expectedIndex != index)
        {
            DBG ("MODE SYNC ERROR: rail control index=" << index
                 << " but AnalyzerDisplayView mode=" << (currentMode == AnalyzerDisplayView::Mode::FFT ? "FFT" : (currentMode == AnalyzerDisplayView::Mode::BAND ? "BANDS" : "LOG")));
            jassertfalse;
        }
#endif
    }
    else if (parameterID == "FftSize")
    {
        // Convert choice index to FFT size (handled in PluginProcessor::parameterChanged)
        const int sizes[] = { 1024, 2048, 4096, 8192 };
        const int index = juce::roundToInt (newValue);
        if (index >= 0 && index < 4)
        {
            audioProcessor.getAnalyzerEngine().setFftSize (sizes[index]);
            const int order = 10 + index;  // 1024=10, 2048=11, 4096=12, 8192=13
            analyzerView_.setSpectrumFftOrder (order);
        }
    }
    else if (parameterID == "Averaging")
    {
        // Averaging is fractional octave smoothing (Off, 1/24, 1/12, 1/6, 1/3, 1 Oct).
        // PluginProcessor::processBlock applies it via setSmoothingOctaves.
        // AnalyzerDisplayView::timerCallback pushes it to spectrumEngine via AnalyzerSettings.
        // No message-thread action needed here.
    }
    else if (parameterID == "scopeChannelMode")
    {
        const int raw = juce::roundToInt (newValue);
        const int val = juce::jlimit (0, 1, raw);
        juce::MessageManager::callAsync ([this, val]
        {
            stereoScopeView_.resetHold();
            stereoScopeView_.setChannelMode (val == 0 ? StereoScopeView::ChannelMode::Stereo : StereoScopeView::ChannelMode::MidSide);
            stereoScopeView_.repaint();
        });
    }
    else if (parameterID == "meterChannelMode")
    {
        const int val = juce::roundToInt (newValue);
        juce::MessageManager::callAsync ([this, val]
        {
            const auto mode = (val == 0 ? MeterGroupComponent::ChannelMode::Stereo : MeterGroupComponent::ChannelMode::MidSide);
            outputMeters_.setChannelMode (mode);
            inputMeters_.setChannelMode (mode);
        });
    }
    else if (parameterID == "meterPeakHold")
    {
        const bool hold = newValue > 0.5f;
        juce::MessageManager::callAsync ([this, hold]
        {
            outputMeters_.setHoldEnabled (hold);
            inputMeters_.setHoldEnabled (hold);
        });
    }
    else if (parameterID == "scopePeakHold")
    {
        const bool hold = newValue > 0.5f;
        juce::MessageManager::callAsync ([this, hold]
        {
            phaseFanScopeComponent_.setPeakHoldEnabled (hold);  // Scope peak hold: PhaseFanScope only
        });
    }
    else if (parameterID == "HoldPeaks")
    {
        audioProcessor.getAnalyzerEngine().setHold (newValue > 0.5f);
    }
    else if (parameterID == "PeakDecay")
    {
        audioProcessor.getAnalyzerEngine().setReleaseTimeMs (newValue);
        const float decayNorm = juce::jlimit (0.0f, 1.0f, (newValue - 100.0f) / 4900.0f);
        analyzerView_.setSpectrumDecayRate (decayNorm);
    }
    else if (parameterID == "DbRange")
    {
        const int idx = juce::jlimit (0, 2, juce::roundToInt (newValue));
        analyzerView_.setDbRangeFromChoiceIndex (idx);
        // header_.setDbRangeSelectedId (idx + 1); // Removed
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

        analyzerView_.setDbRange (next);

        // Header update removed (control removed)
        return true;
    }

    return false;
}

void MainView::triggerResetPeaks()
{
    audioProcessor.getAnalyzerEngine().resetPeaks();
    audioProcessor.resetMeterClipLatches();
    audioProcessor.resetMeterClipLatches();
    analyzerView_.resetViewPeaks();
    analyzerView_.repaint();
}

void MainView::toggleRail()
{
    railIsOpen_ = !railIsOpen_;
    
    // Calculate target width based on state
    auto mode = getLayoutMode (getWidth());
    int targetWidth = 0;
    if (railIsOpen_)
    {
        targetWidth = AnalyzerPro::Layout::railNormalWidth;
        if (mode == LayoutMode::Wide)
            targetWidth = juce::jmin (AnalyzerPro::Layout::railWideWidth, getWidth() / 4);
        targetWidth = juce::jmax (AnalyzerPro::Layout::railMinWidth, targetWidth);
    }
    
    animateRailWidth (targetWidth);
}

void MainView::animateRailWidth (int targetWidth)
{
    // Use ComponentAnimator to smoothly animate the viewport bounds
    auto currentBounds = railViewport_.getBounds();
    auto targetBounds = currentBounds.withWidth (targetWidth);
    
    // Animate the viewport - ComponentAnimator will update bounds during animation
    railAnimator_.animateComponent (&railViewport_,
                                    targetBounds,
                                    1.0f,
                                    300, // 300ms animation duration
                                    false, // don't use proxy
                                    0.0,  // start speed (ease in)
                                    0.0); // end speed (ease out)
    
    // Set up a timer to update layout during animation
    // ComponentAnimator updates the viewport bounds, but we need to update other components
    struct LayoutUpdater : juce::Timer
    {
        LayoutUpdater (MainView* mv) : mainView (mv) { startTimer (16); }
        void timerCallback() override
        {
            if (!mainView->railAnimator_.isAnimating (&mainView->railViewport_))
            {
                // Animation complete - final layout update
                mainView->resized();
                stopTimer();
                delete this;
            }
            else
            {
                // Update layout during animation - resized() will use current viewport width
                mainView->resized();
            }
        }
        MainView* mainView;
    };
    
    new LayoutUpdater (this);
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
    g.setColour (juce::Colours::pink.withAlpha (0.7f));
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

MainView::LayoutMode MainView::getLayoutMode (int width) noexcept
{
    if (width < compactBreakpoint)
        return LayoutMode::Compact;
    if (width >= wideBreakpoint)
        return LayoutMode::Wide;
    return LayoutMode::Normal;
}

void MainView::resized()
{
    auto bounds = getLocalBounds().reduced (outerPadding);
#if JUCE_DEBUG
    debugOuter = getLocalBounds();
    debugContent = bounds;
#endif

    if (bounds.isEmpty())
        return;

    // Header (fixed height)
    auto headerArea = bounds.removeFromTop (topBarHeight);
    if (headerArea.getHeight() > 0 && headerArea.getWidth() > 0)
        header_.setBounds (headerArea);
#if JUCE_DEBUG
    debugHeader = headerArea;
#endif

    // Footer
    auto footerArea = bounds.removeFromBottom (footerHeight);
    if (footerArea.getHeight() > 0 && footerArea.getWidth() > 0)
        footer_.setBounds (footerArea);
#if JUCE_DEBUG
    debugFooter = footerArea;
#endif

    // Meter rails (never hidden)
    auto leftMeters = bounds.removeFromLeft (meterRailWidth);
    auto rightMeters = bounds.removeFromRight (meterRailWidth);
    if (meterRailHeight > 0)
    {
        const int h = juce::jmin (leftMeters.getHeight(), meterRailHeight);
        leftMeters = leftMeters.withHeight (h);
        rightMeters = rightMeters.withHeight (h);
    }
    if (leftMeters.getWidth() > 0 && leftMeters.getHeight() > 0)
        inputMeters_.setBounds (leftMeters);
    if (rightMeters.getWidth() > 0 && rightMeters.getHeight() > 0)
        outputMeters_.setBounds (rightMeters);

    if (bounds.isEmpty())
        return;

    // Calculate rail width - use animated width if animating, otherwise calculate normally
    auto mode = getLayoutMode (bounds.getWidth());
    int railWidth;
    if (railAnimator_.isAnimating (&railViewport_))
    {
        // During animation, use the current viewport width
        railWidth = railViewport_.getWidth();
    }
    else
    {
        // Normal calculation
        railWidth = railIsOpen_ ? AnalyzerPro::Layout::railNormalWidth : 0;
        if (railIsOpen_ && mode == LayoutMode::Wide)
            railWidth = juce::jmin (AnalyzerPro::Layout::railWideWidth, bounds.getWidth() / 4);
        if (railIsOpen_)
            railWidth = juce::jmax (AnalyzerPro::Layout::railMinWidth, railWidth);
        animatedRailWidth_ = railWidth; // Update stored width
    }

    auto railArea = bounds.removeFromRight (railWidth);
    if (railArea.getWidth() > 0 && railArea.getHeight() > 0)
    {
        const int viewY = railViewport_.getViewPosition().getY();
        rail_.setLayoutMode (static_cast<int> (mode));
        railViewport_.setBounds (railArea);
        const int preferredH = rail_.getPreferredHeight();
        rail_.setBounds (0, 0, railViewport_.getWidth(), preferredH);
        const int maxY = juce::jmax (0, preferredH - railArea.getHeight());
        railViewport_.setViewPosition (0, juce::jlimit (0, maxY, viewY));
    }
#if JUCE_DEBUG
    debugRail = railArea;
#endif

    if (bounds.isEmpty())
        return;

    // Bottom area (scope + loudness) — clamp so rail is never starved
    int bottomHeight = juce::jlimit (180, 260, bounds.getHeight() / 3);
    auto bottomArea = bounds.removeFromBottom (bottomHeight);
#if JUCE_DEBUG
    debugPhaseBottom = bottomArea;
#endif
    if (bottomArea.getHeight() > 0 && bottomArea.getWidth() > 0)
    {
        auto scopeArea = bottomArea.removeFromLeft (bottomArea.getWidth() * 2 / 3);
        if (scopeArea.getWidth() > 0 && scopeArea.getHeight() > 0)
        {
            const int availableW = scopeArea.getWidth();
            const int availableH = scopeArea.getHeight();
            int squareSize = juce::jmin (availableW, availableH, AnalyzerPro::Layout::kScopeMaxSize);
            squareSize = juce::jmax (0, squareSize);
            const int gap = 8;
            auto stereoBounds = juce::Rectangle<int> (squareSize, squareSize)
                .withPosition (scopeArea.getX(), scopeArea.getCentreY() - squareSize / 2);
            auto phaseFanArea = scopeArea.withTrimmedLeft (squareSize + gap);
            stereoScopeView_.setBounds (stereoBounds);
            phaseFanScopeComponent_.setBounds (phaseFanArea);
        }
        if (bottomArea.getWidth() > 0 && bottomArea.getHeight() > 0)
            loudnessPanel_.setBounds (bottomArea);
    }

    // Main analyzer plot (all remaining center space)
    if (bounds.getWidth() > 0 && bounds.getHeight() > 0)
        analyzerView_.setBounds (bounds);
#if JUCE_DEBUG
    debugAnalyzerTop = bounds;
    debugLeft = bounds;
    if (debugRectCallback_)
    {
        debugRectCallback_ ("Analyzer", debugAnalyzerTop, juce::Colours::blue);
        debugRectCallback_ ("Header", debugHeader, juce::Colours::yellow);
        debugRectCallback_ ("Footer", debugFooter, juce::Colours::cyan);
        debugRectCallback_ ("ControlRail", debugRail, juce::Colours::magenta);
        debugRectCallback_ ("InputMeters", inputMeters_.getBoundsInParent(), juce::Colours::green);
        debugRectCallback_ ("OutputMeters", outputMeters_.getBoundsInParent(), juce::Colours::green);
        debugRectCallback_ ("StereoScope", stereoScopeView_.getBoundsInParent(), juce::Colours::lightblue);
        debugRectCallback_ ("PhaseFanScope", phaseFanScopeComponent_.getBoundsInParent(), juce::Colours::lightgreen);
        debugRectCallback_ ("Loudness", loudnessPanel_.getBoundsInParent(), juce::Colours::orange);
    }
#endif
}

void MainView::setTooltipManager (mdsp_ui::TooltipManager* manager)
{
    tooltipManager_ = manager;
    
    // Register tooltips for known components
    if (tooltipManager_ != nullptr)
    {
        tooltipManager_->registerTooltip (&stereoScopeView_, {
            "stereoscope",
            "Stereo Scope",
            "Lissajous vectorscope showing stereo width and phase correlation.",
            []() { return ""; },
            nullptr
        });
        tooltipManager_->registerTooltip (&phaseFanScopeComponent_, {
            "phasefanscope",
            "Phase Fan Scope",
            "PAZ-style phase fan display showing stereo correlation by angle. "
            "Left/Right labels indicate stereo image; Anti Phase indicates out-of-phase content.",
            []() { return ""; },
            nullptr
        });
    }
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
    // Trace toggle params
    apvtsParams.insert ("TraceShowLR");
    apvtsParams.insert ("analyzerShowMono");
    apvtsParams.insert ("analyzerShowL");
    apvtsParams.insert ("analyzerShowR");
    apvtsParams.insert ("analyzerShowMid");
    apvtsParams.insert ("analyzerShowSide");
    apvtsParams.insert ("analyzerShowRMS");
    apvtsParams.insert ("analyzerShowRMS");
    apvtsParams.insert ("analyzerWeighting");
    apvtsParams.insert ("scopeChannelMode");
    apvtsParams.insert ("meterChannelMode");

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
