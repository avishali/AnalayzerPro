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

private:
    static constexpr int kBaseEditorWidth = 1100;
    static constexpr int kBaseEditorHeight = 720;
    static constexpr int kMaxEditorSize = 4096;

    static int clampEditorSizePreset (int percent) noexcept;
    static int deriveEditorSizePreset (int width, int height) noexcept;
    juce::Rectangle<int> getPresetBoundsForPercent (int percent) const;
    void applyEditorSizePreset (int percent);

    AnalayzerProAudioProcessor& audioProcessor;
    AnalyzerEngine& analyzerModule;  // Reference to analyzer module for direct access
    mdsp_ui::UiContext ui_;  // Single shared UiContext instance for all UI
    mdsp_ui::LookAndFeel lnf_; // Custom LookAndFeel

    std::unique_ptr<mdsp_ui::TooltipManager> tooltipManager_;
    std::unique_ptr<juce::TooltipWindow> tooltipWindow_;
    MainView mainView;
    juce::Label buildInfoLabel_;  // version + build date/time, bottom-left
    int currentEditorSizePreset_ = 100;
#if JUCE_DEBUG
    DebugGridOverlay debugGrid;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalayzerProAudioProcessorEditor)
};
