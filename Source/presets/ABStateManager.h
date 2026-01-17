#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>

namespace AnalyzerPro::presets
{

class ABStateManager
{
public:
    enum class Slot
    {
        A,
        B
    };

    explicit ABStateManager (juce::AudioProcessorValueTreeState& apvts);
    ~ABStateManager() = default;

    // A/B Logic
    void storeSnapshot (Slot slot, const juce::ValueTree& state);
    void loadSnapshot (Slot slot); // Replaces APVTS state with stored snapshot
    
    void setActiveSlot (Slot slot);
    Slot getActiveSlot() const { return activeSlot; }
    
    void copyAToB(); // Optional helper? Task doesn't explicitly ask but useful.
    
    // Persistence helpers (Step 4 needs this)
    juce::ValueTree getSnapshot (Slot slot) const;
    void saveToState (juce::ValueTree& state) const;
    void restoreFromState (const juce::ValueTree& state);

    // Callback for UI updates
    std::function<void(Slot)> onSlotChanged;

private:
    juce::AudioProcessorValueTreeState& apvts;
    Slot activeSlot { Slot::A };
    
    juce::ValueTree stateA;
    juce::ValueTree stateB;
};

} // namespace AnalyzerPro::presets
