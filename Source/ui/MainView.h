#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <mdsp_ui/UiContext.h>
#include <mdsp_ui/scopes/StereoScopeRenderStateProvider.h>
#include <mdsp_ui/scopes/PhaseFanRenderStateProvider.h>
#include "ui/tooltips/TooltipManager.h" // Added include
#include "../PluginProcessor.h" // Added include
#include "../control/AnalyzerProControlContext.h"
#include "layout/LayoutConstants.h"
#include "layout/HeaderBar.h"
#include "layout/SettingsPopupPanel.h"
#include "layout/ControlRail.h"
#include "layout/FooterBar.h"
#include "analyzer/AnalyzerDisplayView.h"
#include "meters/MeterGroupComponent.h"
#include "meters/StereoScopeComponent.h"
#include "meters/PhaseFanScopeComponent.h"
#include "loudness/LoudnessNumericPanel.h"
#include <array>

//==============================================================================
/**
    Main UI view component.
    Contains the plugin's user interface elements.
*/
class MainView : public juce::Component,
                  public juce::AudioProcessorValueTreeState::Listener,
                  public juce::KeyListener,
                  private juce::Timer
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
    
    void setTooltipManager (mdsp_ui::TooltipManager* manager);

    enum class LayoutMode { Compact, Normal, Wide };
    static LayoutMode getLayoutMode (int width) noexcept;

    /** Shutdown: stop timers, clear callbacks, detach listeners. Safe to call multiple times. */
    void shutdown();
    
    // Tooltips
    mdsp_ui::TooltipManager* tooltipManager_ = nullptr;
    
#if JUCE_DEBUG
    /** DEBUG: Audit APVTS parameters for missing UI bindings (runs once at startup) */
    void auditApvtsParameters();
    using DebugRectCallback = std::function<void(const juce::String&, juce::Rectangle<int>, juce::Colour)>;
    void setDebugRectCallback (DebugRectCallback cb) { debugRectCallback_ = std::move (cb); }
#endif

private:
    void syncAnalyzerTraceConfig();
    void triggerResetPeaks();
    void timerCallback() override;

    // Settings popup helpers
    void showSettingsPopup (SettingsPopupPanel::Section section, juce::Component* anchor);

    // Non-APVTS state mirrored locally so popups open in the right state
    int currentAnalyzerMode_  = 1;   // 1=FFT 2=Band 3=Log
    int currentScopeMode_     = 1;   // 1=Peak 2=RMS
    int currentScopeShape_    = 1;   // 1=Basic 2=PAZ

    // Popup toggle tracking — second click on same section button closes the popup
    juce::Component::SafePointer<SettingsPopupPanel> currentPopup_;
    SettingsPopupPanel::Section currentPopupSection_ = SettingsPopupPanel::Section::Spectrum;

    bool isShutdown = false;
    AnalayzerProAudioProcessor& audioProcessor;
    juce::AudioProcessorValueTreeState* apvts_ = nullptr;
    AnalyzerPro::control::AnalyzerProControlContext controls_;

    mdsp_ui::UiContext& ui_;  // Reference to shared UiContext from PluginEditor

    HeaderBar header_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> dbRangeAttachment_;
    juce::Viewport railViewport_;
    ControlRail rail_;
    FooterBar footer_;
    
    bool railIsOpen_ = false; // Default: rail is closed
    int animatedRailWidth_ = AnalyzerPro::Layout::railNormalWidth; // Current animated width
    juce::ComponentAnimator railAnimator_;
    
    void toggleRail();
    void animateRailWidth (int targetWidth);
    AnalyzerDisplayView analyzerView_;
    StereoScopeComponent stereoScopeComponent_;
    PhaseFanScopeComponent phaseFanScopeComponent_;
    LoudnessNumericPanel loudnessPanel_; // New Loudness Panel
    MeterGroupComponent outputMeters_;
    MeterGroupComponent inputMeters_;
    mdsp_ui::scopes::StereoScopeRenderStateProvider stereoScopeProvider_;
    mdsp_ui::scopes::PhaseFanRenderStateProvider phaseFanProvider_;
    std::array<float, mdsp_ui::scopes::StereoScopeRenderState::kMaxPoints> scopeLeftScratch_ {};
    std::array<float, mdsp_ui::scopes::StereoScopeRenderState::kMaxPoints> scopeRightScratch_ {};

#if JUCE_DEBUG
    DebugRectCallback debugRectCallback_;
#endif
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
