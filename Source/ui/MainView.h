#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <mdsp_ui/UiContext.h>
#include "../PluginProcessor.h"
#include <ui_core/UiCore.h>
#include "hardware/PluginHardwareAdapter.h"
#include "hardware/PluginHardwareOutputAdapter.h"
#include "../control/AnalyzerProControlContext.h"
#include "layout/HeaderBar.h"
#include "layout/ControlRail.h"
#include "layout/FooterBar.h"
#include "analyzer/AnalyzerDisplayView.h"
#include "views/PhaseCorrelationView.h"
#include "meters/MeterGroupComponent.h"
#include <memory>
#if JUCE_DEBUG
#include <set>
#endif

//==============================================================================
/**
    Main UI view component.
    Contains the plugin's user interface elements.
*/
class MainView : public juce::Component,
                  public juce::AudioProcessorValueTreeState::Listener,
                  public juce::KeyListener
{
public:
    explicit MainView (mdsp_ui::UiContext& ui, AnalayzerProAudioProcessor& p, juce::AudioProcessorValueTreeState* apvts);
    ~MainView() override;
    
    //==============================================================================
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    using juce::Component::keyPressed; // Avoid hiding Component::keyPressed(KeyPress)
    bool keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent) override;

    AnalyzerPro::ControlBinder& controlBinder() noexcept { return controls_.getBinder(); }
    const AnalyzerPro::ControlBinder& controlBinder() const noexcept { return controls_.getBinder(); }

    AnalyzerPro::UiState& controlUiState() noexcept { return controls_.getUiState(); }
    const AnalyzerPro::UiState& controlUiState() const noexcept { return controls_.getUiState(); }

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Shutdown: stop timers, clear callbacks, detach listeners. Safe to call multiple times. */
    void shutdown();

#if JUCE_DEBUG
    /** DEBUG: Audit APVTS parameters for missing UI bindings (runs once at startup) */
    void auditApvtsParameters();
#endif

private:
    void triggerResetPeaks();

    bool isShutdown = false;
    AnalayzerProAudioProcessor& audioProcessor;
    juce::AudioProcessorValueTreeState* apvts_ = nullptr;
    AnalyzerPro::control::AnalyzerProControlContext controls_;

    mdsp_ui::UiContext& ui_;  // Reference to shared UiContext from PluginEditor

    HeaderBar header_;
    ControlRail rail_;
    FooterBar footer_;
    AnalyzerDisplayView analyzerView_;
    PhaseCorrelationView phaseView_;
    MeterGroupComponent outputMeters_;
    MeterGroupComponent inputMeters_;

    // Temporary debug overlay rectangles
    juce::Rectangle<int> debugOuter;
    juce::Rectangle<int> debugContent;
    juce::Rectangle<int> debugHeader;
    juce::Rectangle<int> debugFooter;
    juce::Rectangle<int> debugRail;
    juce::Rectangle<int> debugLeft;
    juce::Rectangle<int> debugAnalyzerTop;
    juce::Rectangle<int> debugPhaseBottom;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};
