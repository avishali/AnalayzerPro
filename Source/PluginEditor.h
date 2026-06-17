#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>
#include <mdsp_ui/ThemeVariant.h>
#include <mdsp_ui/LookAndFeel.h>
#include <memory>


#include "PluginProcessor.h"
#include "analyzer/AnalyzerEngine.h"
#include "ui/MainView.h"
#include "ui/tooltips/TooltipManager.h"
#include "ui/dev/DevLookPanelConfig.h"
#if defined(ANALYZERPRO_METAL_EDITOR) && ANALYZERPRO_METAL_EDITOR
#include "ui/analyzer/metal/IEditorSurface.h"
#endif
#if JUCE_DEBUG
#include "ui/DebugGridOverlay.h"
#endif

//==============================================================================
/**
    Audio Processor Editor Template.
    Replace this with your plugin's UI implementation.
*/
class AnalayzerProAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    AnalayzerProAudioProcessorEditor (AnalayzerProAudioProcessor&);
    ~AnalayzerProAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
#if JUCE_DEBUG
    bool keyPressed (const juce::KeyPress&) override;
#endif
#if defined(ANALYZERPRO_METAL_EDITOR) && ANALYZERPRO_METAL_EDITOR
    void setMetalTraceSuppressedForChromeCapture (bool shouldSuppress) noexcept;
    bool fillMetalAnalyzerFrame (AnalyzerPro::metal::MetalAnalyzerFrame& frame, float backingScale);
    AnalyzerEngine& getMetalAnalyzerEngine() noexcept { return analyzerModule; }
#endif

private:
    static constexpr int kBaseEditorWidth = 1360;
    static constexpr int kBaseEditorHeight = 765;
    static constexpr int kMinEditorSizePreset = 75;
    static constexpr int kMaxEditorSize = 4096;

    static int clampEditorSizePreset (int percent) noexcept;
    static int deriveEditorSizePreset (int width, int height) noexcept;
    juce::Rectangle<int> getPresetBoundsForPercent (int percent) const;
    void applyEditorSizePreset (int percent);
#if ANALYZERPRO_DEV_LOOK_PANEL
    void toggleDevLookPanel();
    void closeDevLookPanelWindow();
#endif
#if defined(ANALYZERPRO_METAL_EDITOR) && ANALYZERPRO_METAL_EDITOR
    static AnalyzerPro::metal::MetalHostMechanism getConfiguredMetalHostMechanism();
    void startMetalSurfaceIfNeeded();
#endif

    AnalayzerProAudioProcessor& audioProcessor;
    AnalyzerEngine& analyzerModule;  // Reference to analyzer module for direct access
    mdsp_ui::UiContext ui_;  // Single shared UiContext instance for all UI
    mdsp_ui::LookAndFeel lnf_; // Custom LookAndFeel

    std::unique_ptr<mdsp_ui::TooltipManager> tooltipManager_;
    std::unique_ptr<juce::TooltipWindow> tooltipWindow_;
    MainView mainView;
    juce::Label buildInfoLabel_;  // version + build date/time, bottom-left
#if defined(ANALYZERPRO_METAL_EDITOR) && ANALYZERPRO_METAL_EDITOR
    std::unique_ptr<AnalyzerPro::metal::IEditorSurface> editorSurface_;
#endif
    int currentEditorSizePreset_ = 100;
#if ANALYZERPRO_DEV_LOOK_PANEL
    class DevLookWindow : public juce::DocumentWindow
    {
    public:
        explicit DevLookWindow (juce::Colour backgroundColour);
        void closeButtonPressed() override;

        std::function<void()> onClose;

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DevLookWindow)
    };

    std::unique_ptr<DevLookWindow> devLookWindow_;
#endif
#if JUCE_DEBUG
    DebugGridOverlay debugGrid;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalayzerProAudioProcessorEditor)
};
