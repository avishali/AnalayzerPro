/*
  ==============================================================================

    PresetManager.h
    Created: 12 Jan 2026
    Author:  Antigravity

  ==============================================================================

    CLEANUP: UNUSED DUPLICATE CLASS - This entire file is unused!
    The active implementation is in Source/presets/PresetManager.h
    This state/PresetManager is never included or instantiated anywhere in the codebase.

    Commented out for review - safe to delete after validation.

  ==============================================================================
*/

/*
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace AnalyzerPro::state
{

// **
//     Manages loading, saving, and listing of presets.
//
//     Presets are stored as XML files containing a serialized ValueTree of the APVTS.
//
//     Location:
//     - macOS: ~/Library/Application Support/MelechDSP/AnalyzerPro/Presets
//
//     Rules:
//     - File extension: .mdspreset
//     - "Default" preset is saved/loaded from "Default.preset" (special case)
//     - Host-safe: State replacement happens on message thread or via APVTS atomics where valid.
// *
class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& apvts);
    ~PresetManager() = default;

    // File Operations
    void savedPreset (const juce::String& name);  // NOTE: Typo - should be "savePreset"
    void loadPreset (const juce::String& name);
    void deletePreset (const juce::String& name);

    // Default & Factory
    void saveDefaultPreset();
    void loadDefaultPreset();
    void loadFactoryPreset();

    // Listing
    juce::StringArray getPresetList();
    juce::File getPresetFolder() const;
    juce::String getCurrentPresetName() const { return currentPresetName_; }

private:
    void loadPresetFromFile (const juce::File& file);

    juce::AudioProcessorValueTreeState& apvts_;
    juce::String currentPresetName_;

    const juce::String fileExtension_ = ".mdspreset";
    const juce::String defaultPresetName_ = "Default";
};

} // namespace AnalyzerPro::state
*/
