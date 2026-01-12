/*
  ==============================================================================

    StateManager.h
    Created: 12 Jan 2026
    Author:  Antigravity

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace AnalyzerPro::state
{

/**
    Manages A/B Comparison and transient state logic.
    
    Responsibilities:
    - Store snapshots for Slot A and Slot B
    - Handle switching between slots (applying to APVTS)
    - Initialize slots on startup
    
    Note: Bypass is handled via a direct APVTS parameter ("MasterBypass"), 
    but this manager can provide helpers if needed.
*/
class StateManager
{
public:
    enum class Slot { A, B };

    explicit StateManager (juce::AudioProcessorValueTreeState& apvts);
    ~StateManager() = default;

    // A/B Logic
    void setActiveSlot (Slot slot);
    Slot getActiveSlot() const noexcept { return activeSlot_; }
    void toggleSlot();
    
    void copyToOtherSlot(); // Copies current state to the INACTIVE slot
    
    // Call this before saving session to ensure current APVTS is synced to its slot memory
    void flushCurrentStateToSlot();
    
    // Callbacks
    std::function<void (Slot)> onSlotChanged;
    
    // Call this after loading session to populate slots
    void restoreFromState (const juce::ValueTree& state);
    void saveToState (juce::ValueTree& state);

private:
    void applyState (const juce::ValueTree& state);

    juce::AudioProcessorValueTreeState& apvts_;
    
    Slot activeSlot_ = Slot::A;
    juce::ValueTree stateA_;
    juce::ValueTree stateB_;
};

} // namespace AnalyzerPro::state
