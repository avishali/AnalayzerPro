#include "PresetManager.h"

namespace AnalyzerPro::presets
{

static const juce::String kPresetExtension = ".mdspreset";
static const juce::String kDefaultPresetName = "Default";

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& apvtsToUse)
    : apvts (apvtsToUse)
{
    // Create folder if not exists
    getPresetFolder();
}

juce::File PresetManager::getPresetFolder() const
{
    auto folder = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                      .getChildFile ("MelechDSP")
                      .getChildFile ("AnalyzerPro")
                      .getChildFile ("Presets");
    
    if (! folder.exists())
        folder.createDirectory();
        
    return folder;
}

juce::StringArray PresetManager::listPresets() const
{
    juce::StringArray presets;
    auto folder = getPresetFolder();
    
    auto files = folder.findChildFiles (juce::File::findFiles, false, "*" + kPresetExtension);
    files.sort();
    
    for (const auto& file : files)
        presets.add (file.getFileNameWithoutExtension());
        
    return presets;
}

void PresetManager::savePreset (const juce::String& name)
{
    auto file = getPresetFolder().getChildFile (name + kPresetExtension);
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    xml->writeTo (file);
    
    currentPresetName = name;
}

void PresetManager::loadPreset (const juce::String& name)
{
    auto file = getPresetFolder().getChildFile (name + kPresetExtension);
    if (file.existsAsFile())
        loadPresetInternal (file);
}

void PresetManager::loadingPresetFromFile (const juce::File& file)
{
    if (file.existsAsFile())
        loadPresetInternal (file);
}

void PresetManager::loadPresetInternal (const juce::File& file)
{
    auto xml = juce::parseXML (file);
    if (xml != nullptr)
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (state.isValid())
        {
            currentPresetName = file.getFileNameWithoutExtension();
            
            // Ensure thread safety for APVTS state replacement
            juce::MessageManager::callAsync ([this, state]
            {
                apvts.replaceState (state);
            });
        }
    }
}

void PresetManager::loadFactory()
{
    currentPresetName = "Factory";
    
    juce::MessageManager::callAsync ([this]
    {
        auto& params = apvts.processor.getParameters();
        for (auto* p : params)
        {
            if (dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            {
                // APVTS parameters are usually RangedAudioParameter, but base interface works for get/set
                 // However, APVTS manages parameters. 
                 // We should use the raw parameter pointer to reset to default.
                 // p->getDefaultValue() returns 0..1
                 
                 // Important: setValueNotifyingHost expects 0..1
                 p->setValueNotifyingHost (p->getDefaultValue());
            }
        }
    });
}

void PresetManager::saveDefault()
{
    savePreset (kDefaultPresetName);
}

void PresetManager::loadDefaultOrFactory()
{
    auto file = getPresetFolder().getChildFile (kDefaultPresetName + kPresetExtension);
    if (file.existsAsFile())
        loadPreset (kDefaultPresetName);
    else
        loadFactory();
}

} // namespace AnalyzerPro::presets
