/*
  ==============================================================================

    PresetManager.cpp
    Created: 12 Jan 2026
    Author:  Antigravity

  ==============================================================================
*/

#include "PresetManager.h"

namespace AnalyzerPro::state
{

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& apvts)
    : apvts_ (apvts)
{
    // Ensure preset directory exists
    const auto folder = getPresetFolder();
    if (! folder.exists())
        folder.createDirectory();
        
    // Always attempt to load default on startup
    loadDefaultPreset();
}

juce::File PresetManager::getPresetFolder() const
{
    auto rootDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
    
#if JUCE_MAC
    return rootDir.getChildFile ("MelechDSP").getChildFile ("AnalyzerPro").getChildFile ("Presets");
#else
    // Windows usually Roaming/MelechDSP/...
    return rootDir.getChildFile ("MelechDSP").getChildFile ("AnalyzerPro").getChildFile ("Presets");
#endif
}

juce::StringArray PresetManager::getPresetList()
{
    juce::StringArray presets;
    const auto folder = getPresetFolder();
    
    // Find all .mdspreset files
    auto files = folder.findChildFiles (juce::File::findFiles, false, "*" + fileExtension_);
    
    for (const auto& f : files)
    {
        // Don't include "Default" in the visible user list usually, 
        // unless requested. Runbook treats Default.preset as separate file.
        // We will include it if it matches the extension, but typically users 
        // name their own presets. Let's include everything for visibility.
        presets.add (f.getFileNameWithoutExtension());
    }
    
    presets.sort (true);
    return presets;
}

void PresetManager::savedPreset (const juce::String& name)
{
    const auto folder = getPresetFolder();
    auto file = folder.getChildFile (name + fileExtension_);
    
    auto state = apvts_.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    
    if (xml->writeTo (file))
    {
        currentPresetName_ = name;
    }
}

void PresetManager::loadPreset (const juce::String& name)
{
    const auto folder = getPresetFolder();
    auto file = folder.getChildFile (name + fileExtension_);
    
    if (file.existsAsFile())
    {
        loadPresetFromFile (file);
        currentPresetName_ = name;
    }
}

void PresetManager::loadPresetFromFile (const juce::File& file)
{
    auto xml = juce::parseXML (file);
    if (xml != nullptr)
    {
        if (xml->hasTagName (apvts_.state.getType()))
        {
            auto newState = juce::ValueTree::fromXml (*xml);
            apvts_.replaceState (newState);
        }
    }
}

void PresetManager::deletePreset (const juce::String& name)
{
    const auto folder = getPresetFolder();
    auto file = folder.getChildFile (name + fileExtension_);
    
    if (file.existsAsFile())
    {
        file.deleteFile();
        if (currentPresetName_ == name)
            currentPresetName_ = "";
    }
}

// Default Preset
void PresetManager::saveDefaultPreset()
{
    const auto folder = getPresetFolder();
    auto file = folder.getChildFile (defaultPresetName_ + ".preset"); // Different extension for internal default?
    // Runbook says: "Default.preset" (but implied checking extension consistency).
    // Let's stick to .preset or .mdspreset based on strict runbook rule?
    // Runbook decision C "Default.preset". 
    // Runbook decision A "Preset file extension: .mdspreset"
    // Let's use "Default.mdspreset" to be consistent if possible, 
    // or distinct if we want to hide it.
    // Runbook explicitly listed "Default.preset" separately. Let's follow strictly.
    
    auto state = apvts_.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    xml->writeTo (file);
}

void PresetManager::loadDefaultPreset()
{
    const auto folder = getPresetFolder();
    auto file = folder.getChildFile (defaultPresetName_ + ".preset");
    
    if (file.existsAsFile())
    {
        loadPresetFromFile (file);
        currentPresetName_ = defaultPresetName_;
    }
    else
    {
        loadFactoryPreset();
    }
}

void PresetManager::loadFactoryPreset()
{
    // Option A: Reset parameters to defaults
    // Iterate all parameters in APVTS
    // NOTE: This assumes all params are RangedAudioParameters managed by APVTS
    
    auto& processor = apvts_.processor;
    const auto& params = processor.getParameters();
    
    for (auto* p : params)
    {
        if (dynamic_cast<juce::AudioProcessorParameterWithID*> (p) != nullptr)
        {
            // Reset to default
            p->setValueNotifyingHost (p->getDefaultValue());
        }
    }
    
    currentPresetName_ = "Factory";
}

} // namespace AnalyzerPro::state
