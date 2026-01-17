/*
  ==============================================================================

    PresetManager.h
    Created: 15 Jan 2026
    Author:  Antigravity

  ==============================================================================

    LOCKED DECISIONS:
    - Format: APVTS ValueTree serialized to XML string. Extension: ".mdspreset"
    - Folder: userApplicationDataDirectory / "MelechDSP" / "AnalyzerPro" / "Presets"
    - Default Preset: "Default.mdspreset" (loaded on startup if exists, else Factory)
    - Factory Preset: Reset APVTS parameters to defaults (no embedded file)
    - A/B: Two in-memory ValueTree snapshots (A/B). Active slot loads into APVTS. No disk I/O in v1.
    - Bypass: APVTS bool "bypass". Visuals only (freeze/overlay). Meters freeze/tag.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace AnalyzerPro::presets
{

class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& apvts);
    ~PresetManager() = default;

    // Core API
    juce::StringArray listPresets() const; // Returns basenames (files without extension)
    void savePreset (const juce::String& name);
    void loadPreset (const juce::String& name); // Load by name (searches folder)
    void loadingPresetFromFile (const juce::File& file); // Internal worker or public? Task says loadPreset(file)
    void loadFactory();
    void saveDefault();
    void loadDefaultOrFactory();
    
    // Config
    juce::File getPresetFolder() const;
    juce::String getCurrentPresetName() const { return currentPresetName; }

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::String currentPresetName { "Factory" };
    
    void loadPresetInternal (const juce::File& file);
};

} // namespace AnalyzerPro::presets
