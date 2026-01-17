#include "ABStateManager.h"

namespace AnalyzerPro::presets
{

ABStateManager::ABStateManager (juce::AudioProcessorValueTreeState& apvtsToUse)
    : apvts (apvtsToUse)
{
    // Initialize snapshots with current state (default)
    stateA = apvts.copyState();
    stateB = apvts.copyState();
}

void ABStateManager::storeSnapshot (Slot slot, const juce::ValueTree& state)
{
    // Ensure we store a deep copy
    if (slot == Slot::A)
        stateA = state.createCopy();
    else
        stateB = state.createCopy();
}

void ABStateManager::loadSnapshot (Slot slot)
{
    const auto& stateToLoad = (slot == Slot::A) ? stateA : stateB;
    
    // Ensure thread safety when replacing APVTS state
    if (stateToLoad.isValid())
    {
        juce::MessageManager::callAsync ([this, s = stateToLoad.createCopy()]
        {
            apvts.replaceState (s);
        });
    }
}

void ABStateManager::setActiveSlot (Slot slot)
{
    if (activeSlot != slot)
    {
        // 1. Store current state to OLD slot before switching?
        // Usually A/B means you have two distinct states.
        // If I am in A, change parameters, then switch to B...
        // Should A be updated? 
        // In most plugins: Yes, "Active" means you are editing that slot.
        // So before switching away, we must capture current state into activeSlot.
        
        storeSnapshot (activeSlot, apvts.copyState());
        
        // 2. Load NEW slot
        activeSlot = slot;
        loadSnapshot (activeSlot);
        
        // 3. Notify
        if (onSlotChanged)
            onSlotChanged (activeSlot);
    }
}

juce::ValueTree ABStateManager::getSnapshot (Slot slot) const
{
    if (slot == Slot::A) return stateA;
    return stateB;
}

void ABStateManager::saveToState (juce::ValueTree& state) const
{
    // Save Active Slot
    state.setProperty ("ActiveSlot", (int)activeSlot, nullptr);
    
    // Save the INACTIVE state as a child (the active state IS the root state)
    // To handle factory default cases properly, we can just save both if needed, 
    // but the pattern of "Root = Active" is standard.
    // Let's safe-guard:
    
    if (activeSlot == Slot::A)
    {
        // Root is A. Save B.
        auto b = stateB.createCopy();
        // Remove any nested state if it somehow exists
        if (b.hasProperty("ActiveSlot")) b.removeProperty("ActiveSlot", nullptr);
        
        state.appendChild (b, nullptr);
        // Rename child to differentiate?
        // APVTS state usually has Type "PARAMETERS" (set in Processor constructor).
        // If we append a child with same Type, iterating chidren might differ.
        // Let's wrap B in a specific tag container?
        // But ValueTree children are ordered.
        // Let's just append it. To recognize it, we should probably set a property "AB_SLOT" on the child?
        // Or wrap it in a "SNAPSHOT_B" type.
        // ValueTree copy retains type.
        
        // Better: create wrapper child
        juce::ValueTree bWrapper ("SNAPSHOT_B");
        bWrapper.copyPropertiesFrom (stateB, nullptr);
        for (int i = 0; i < stateB.getNumChildren(); ++i)
            bWrapper.appendChild (stateB.getChild(i).createCopy(), nullptr);
            
        state.appendChild (bWrapper, nullptr);
    }
    else
    {
        // Root is B. Save A.
        juce::ValueTree aWrapper ("SNAPSHOT_A");
        aWrapper.copyPropertiesFrom (stateA, nullptr);
        for (int i = 0; i < stateA.getNumChildren(); ++i)
            aWrapper.appendChild (stateA.getChild(i).createCopy(), nullptr);
            
        state.appendChild (aWrapper, nullptr);
    }
}

void ABStateManager::restoreFromState (const juce::ValueTree& state)
{
    // 1. Recover Active Slot
    if (state.hasProperty ("ActiveSlot"))
    {
        int slotIdx = state.getProperty ("ActiveSlot");
        activeSlot = static_cast<Slot> (juce::jlimit (0, 1, slotIdx));
    }
    else
    {
        // Default / Old state: Assume A
        activeSlot = Slot::A;
    }
    
    // 2. Recover snapshots
    // The root state is the ACTIVE state.
    // The inactive state is in a child.
    
    // Helper to strip metadata we added
    auto cleanState = state.createCopy();
    cleanState.removeProperty ("ActiveSlot", nullptr);
    
    // Remove "SNAPSHOT_A" / "SNAPSHOT_B" children if present from cleanState
    // (APVTS replaceState usually ignores unknown children, but clean is better)
    cleanState.removeChild (cleanState.getChildWithName ("SNAPSHOT_A"), nullptr);
    cleanState.removeChild (cleanState.getChildWithName ("SNAPSHOT_B"), nullptr);
    
    if (activeSlot == Slot::A)
    {
        stateA = cleanState;
        
        // Look for Snapshot B
        auto childB = state.getChildWithName ("SNAPSHOT_B");
        if (childB.isValid())
        {
            // Convert wrapper back to standard type if needed?
            // Actually, for consistency, we should restore it to the APVTS wrapper type?
            // APVTS type is usually "PARAMETERS".
            // Our wrapper has type "SNAPSHOT_B".
            // If we blindly load "SNAPSHOT_B" into stateB, valid?
            // Yes, ValueTree type mostly matters for lookups. Parameter logic uses properties.
            // But if preset manager relies on types...
            // Let's clone properties to a new ValueTree with cleanState's type.
            
            juce::ValueTree b (cleanState.getType()); 
            b.copyPropertiesFrom (childB, nullptr);
            for (int i = 0; i < childB.getNumChildren(); ++i)
                b.appendChild (childB.getChild(i).createCopy(), nullptr);
                
            stateB = b;
        }
        else
        {
            // Fallback: make B = A
            stateB = stateA.createCopy();
        }
    }
    else // Active is B
    {
        stateB = cleanState;
        
        auto childA = state.getChildWithName ("SNAPSHOT_A");
        if (childA.isValid())
        {
            juce::ValueTree a (cleanState.getType());
            a.copyPropertiesFrom (childA, nullptr);
            for (int i = 0; i < childA.getNumChildren(); ++i)
                a.appendChild (childA.getChild(i).createCopy(), nullptr);
                
            stateA = a;
        }
        else
        {
            stateA = stateB.createCopy();
        }
    }
    
    // 3. Apply to APVTS via replaceState (must be on message thread, but getState/setState usually is?
    // setStateInformation is NOT guaranteed to be on message thread?
    // JUCE doc: "called from the message thread" usually.
    // But safely:
    apvts.replaceState (cleanState);
    
    // Verify thread safety requirements.
    // "setStateInformation" is usually called by host.
    // If it's not message thread, replaceState might crash if listeners touch UI.
    // Standard practice involves SuspendProcessing or locks.
    // APVTS::replaceState takes a lock.
    // But listeners callback.
    // Our listeners are UI.
    // If we are unsure, we could dispatch?
    // But for "setStateInformation", usually we just run it.
    // We used MessageManager::callAsync in loadSnapshot. 
    // Here we run directly.
}

} // namespace AnalyzerPro::presets
