#include "MainView.h"
#include "../PluginProcessor.h"

//==============================================================================
MainView::MainView (AnalayzerProAudioProcessor& p, juce::AudioProcessorValueTreeState* apvts)
    : audioProcessor (p), controls_ (apvts), analyzerView_ (p), apvts_ (apvts)
{
    setWantsKeyboardFocus (true);

    // Add layout components
    addAndMakeVisible (header_);
    addAndMakeVisible (rail_);
    addAndMakeVisible (footer_);
    addAndMakeVisible (analyzerView_);
    addAndMakeVisible (phaseView_);
    
    // Initialize rail with control binder
    rail_.setControlBinder (controls_.getBinder());

    // Wire parameter changes to AnalyzerEngine and AnalyzerDisplayView
    if (apvts != nullptr)
    {
        apvts->addParameterListener ("Mode", this);
        apvts->addParameterListener ("FftSize", this);
        apvts->addParameterListener ("Averaging", this);
        apvts->addParameterListener ("Hold", this);
        apvts->addParameterListener ("PeakDecay", this);
    }

    // Wire HeaderBar mode selector to AnalyzerDisplayView
    header_.onDisplayModeChanged = [this](HeaderBar::DisplayMode m)
    {
        // Map HeaderBar::DisplayMode to AnalyzerDisplayView::Mode
        AnalyzerDisplayView::Mode viewMode = AnalyzerDisplayView::Mode::FFT;
        switch (m)
        {
            case HeaderBar::DisplayMode::FFT:
                viewMode = AnalyzerDisplayView::Mode::FFT;
                break;
            case HeaderBar::DisplayMode::Band:
                viewMode = AnalyzerDisplayView::Mode::BAND;
                break;
            case HeaderBar::DisplayMode::Log:
                viewMode = AnalyzerDisplayView::Mode::LOG;
                break;
        }
        analyzerView_.setMode (viewMode);
    };

    // Set default mode to FFT
    analyzerView_.setMode (AnalyzerDisplayView::Mode::FFT);

    setSize (800, 600);
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
        apvts_->removeParameterListener ("Hold", this);
        apvts_->removeParameterListener ("PeakDecay", this);
    }
    
    // Clear lambda callbacks that capture 'this'
    header_.onDisplayModeChanged = nullptr;
    
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
        analyzerView_.setMode (viewMode);
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
    else if (parameterID == "Hold")
    {
        audioProcessor.getAnalyzerEngine().setHold (newValue > 0.5f);
    }
    else if (parameterID == "PeakDecay")
    {
        audioProcessor.getAnalyzerEngine().setPeakDecayDbPerSec (newValue);
    }
}

void MainView::paint (juce::Graphics& g)
{
    // Dark background
    g.fillAll (juce::Colours::black);

    // Temporary debug overlay
    g.setFont (juce::Font (10.0f));
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
}

void MainView::resized()
{
    // Debug: capture full bounds
    debugOuter = getLocalBounds();

    // Layout constants
    static constexpr int padding = 10;
    static constexpr int headerH = 32;
    static constexpr int footerH = 22;
    static constexpr int railW = 220;

    auto bounds = getLocalBounds().reduced (padding);
    debugContent = bounds;

    // Slice order: footer, header, rail, left analyzer stack
    // 1) Footer from bottom
    auto footerArea = bounds.removeFromBottom (footerH);
    debugFooter = footerArea;
    footer_.setBounds (footerArea);

    // 2) Header from top (tight, PAZ-like)
    auto headerArea = bounds.removeFromTop (headerH);
    debugHeader = headerArea;
    header_.setBounds (headerArea);

    // 3) Rail from right
    auto railArea = bounds.removeFromRight (railW);
    debugRail = railArea;
    rail_.setBounds (railArea);

    // 4) Left analyzer stack (remaining area)
    auto leftArea = bounds;
    debugLeft = leftArea;

    // Split left area: top ~58%, bottom ~42%
    const int gap = 6;
    const int topH = juce::roundToInt (leftArea.getHeight() * 0.58f);
    auto topArea = leftArea.removeFromTop (topH);
    debugAnalyzerTop = topArea;
    leftArea.removeFromTop (gap);
    auto bottomArea = leftArea;
    debugPhaseBottom = bottomArea;

    analyzerView_.setBounds (topArea);
    phaseView_.setBounds (bottomArea);
}
