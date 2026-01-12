/*
  ==============================================================================

    StateManager.cpp
    Created: 12 Jan 2026
    Author:  Antigravity

  ==============================================================================
*/

#include "StateManager.h"

namespace AnalyzerPro::state
{

static const juce::Identifier idSlotA ("SlotA");
static const juce::Identifier idSlotB ("SlotB");
static const juce::Identifier idActiveSlot ("ActiveSlot");

StateManager::StateManager (juce::AudioProcessorValueTreeState& apvts)
    : apvts_ (apvts)
{
    // Initialize both slots with current state
    stateA_ = apvts_.copyState();
    stateB_ = stateA_.createCopy();
}

void StateManager::setActiveSlot (Slot slot)
{
    if (activeSlot_ == slot)
        return;
        
    // 1. Save current APVTS state to the OLD slot
    flushCurrentStateToSlot();
    
    // 2. Update active slot
    activeSlot_ = slot;
    
    // 3. Load NEW slot state into APVTS
    if (activeSlot_ == Slot::A)
        applyState (stateA_);
    else
        applyState (stateB_);
        
    if (onSlotChanged)
        onSlotChanged (activeSlot_);
}

void StateManager::toggleSlot()
{
    setActiveSlot (activeSlot_ == Slot::A ? Slot::B : Slot::A);
}

void StateManager::copyToOtherSlot()
{
    // Copy current live state to the inactive slot
    auto liveState = apvts_.copyState();
    
    if (activeSlot_ == Slot::A)
        stateB_ = liveState.createCopy();
    else
        stateA_ = liveState.createCopy();
}

void StateManager::flushCurrentStateToSlot()
{
    // Captures the current live state into the memory slot
    if (activeSlot_ == Slot::A)
        stateA_ = apvts_.copyState();
    else
        stateB_ = apvts_.copyState();
}

void StateManager::applyState (const juce::ValueTree& state)
{
    apvts_.replaceState (state.createCopy());
}

// Persistence
void StateManager::saveToState (juce::ValueTree& state)
{
    // Ensure the slots are up to date
    flushCurrentStateToSlot();
    
    // Store Active Slot
    state.setProperty (idActiveSlot, static_cast<int> (activeSlot_), nullptr);
    
    // Store Slot Data
    // We wrap them in specific tags
    auto nodeA = stateA_.createCopy();
    auto nodeB = stateB_.createCopy();
    
    // Important: The slot snapshots are full APVTS states (usually tag "Parameters" or similar).
    // To avoid confusion, we wrap them in container nodes.
    juce::ValueTree containerA (idSlotA);
    containerA.addChild (nodeA, -1, nullptr);
    
    juce::ValueTree containerB (idSlotB);
    containerB.addChild (nodeB, -1, nullptr);
    
    state.addChild (containerA, -1, nullptr);
    state.addChild (containerB, -1, nullptr);
}

void StateManager::restoreFromState (const juce::ValueTree& state)
{
    // Restore Active Slot
    if (state.hasProperty (idActiveSlot))
    {
        int slotIdx = state.getProperty (idActiveSlot);
        activeSlot_ = (slotIdx == 1) ? Slot::B : Slot::A;
    }
    
    // Restore Slots
    auto containerA = state.getChildWithName (idSlotA);
    if (containerA.isValid() && containerA.getNumChildren() > 0)
    {
        stateA_ = containerA.getChild(0).createCopy();
    }
    
    auto containerB = state.getChildWithName (idSlotB);
    if (containerB.isValid() && containerB.getNumChildren() > 0)
    {
        stateB_ = containerB.getChild(0).createCopy();
    }
    
    // Note: The logic here assumes that restoring the plugin state (APVTS) 
    // happens externally via standard APVTS setState logic.
    // However, if we just loaded A/B slots, we should ensure the *live* APVTS matches the *active* slot.
    // Usually host calls `setStateInformation` -> `APVTS::replaceState`.
    // That replaces the LIVE state.
    // So if the live state was just restored by the host/APVTS directly, it IS the active slot state.
    // Our job is merely to populate the *other* slot memory.
    
    // BUT: If `saveToState`/`restoreFromState` is handling the ENTIRE plugin chunk (including A/B), 
    // then the host gives us a blob that contains A/B wrapper + params.
    // We need to clarify who owns the root state restoration.
    // Typically: PluginProcessor::setStateInformation
    // 1. Reads XML.
    // 2. Passes XML to APVTS? NO, APVTS expects its own format.
    
    // Strategy:
    // In PluginProcessor::setStateInformation, we will:
    // 1. Deserialize to ValueTree.
    // 2. Pass to StateManager::restoreFromState.
    // 3. StateManager extracts A/B.
    // 4. StateManager sets `activeSlot`.
    // 5. StateManager *returns* or *applies* the correct active state to APVTS.
    
    // So actually:
    // If we call `applyState` here for the active slot, we are good.
    if (activeSlot_ == Slot::A)
        applyState (stateA_);
    else
        applyState (stateB_);
}

} // namespace AnalyzerPro::state
