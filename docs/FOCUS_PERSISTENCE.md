# Focus Persistence Implementation Summary

This document summarizes the implementation of focus persistence, which stores the focused `ui_core::ControlId` in plugin state and restores it when the editor opens.

## Overview

The focused control ID is now persisted in plugin state using the `Parameters` class. When the editor opens, the previously focused control is restored, including sending hardware focus feedback. This ensures a consistent user experience across plugin sessions.

## Files Modified

### 1. `Source/parameters/Parameters.h`

**Added Getter/Setter Methods:**
```cpp
int getFocusedControlId() const noexcept;
void setFocusedControlId (int id) noexcept;
```

**Added Member Variable:**
```cpp
private:
    std::atomic<float> gain;
    std::atomic<float> outputGain { 1.0f };
    std::atomic<int> focusedControlId { 1001 };  // Default to Gain control
    // ... existing members ...
```

**Key Points:**
- Uses `std::atomic<int>` for thread-safe access
- Default value is `1001` (kGainControlId)
- Allows `0` to represent "no focus" (though default is Gain)

### 2. `Source/parameters/Parameters.cpp`

**Added Getter/Setter Implementation:**
```cpp
int Parameters::getFocusedControlId() const noexcept
{
    return focusedControlId.load();
}

void Parameters::setFocusedControlId (int id) noexcept
{
    focusedControlId.store (id);
}
```

**Added State Persistence - getState():**
```cpp
void Parameters::getState (juce::ValueTree& state) const
{
    state.setProperty ("gain", getGain(), nullptr);
    state.setProperty ("outputGain", getOutputGain(), nullptr);
    state.setProperty ("focusedControlId", getFocusedControlId(), nullptr);
    // ... existing properties ...
}
```

**Added State Persistence - setState():**
```cpp
void Parameters::setState (const juce::ValueTree& state)
{
    setGain (static_cast<float> (state.getProperty ("gain", 1.0)));
    setOutputGain (static_cast<float> (state.getProperty ("outputGain", 1.0)));
    setFocusedControlId (static_cast<int> (state.getProperty ("focusedControlId", 1001)));
    // ... existing properties ...
}
```

**Key Points:**
- Stored in ValueTree with key `"focusedControlId"`
- Default value `1001` (Gain) if property is missing
- Thread-safe atomic operations

### 3. `Source/ui/MainView.cpp`

#### A. Focus Restoration in Constructor

**Added after hardwareOutput creation:**
```cpp
// Create hardware output adapter
hardwareOutput = std::make_unique<PluginHardwareOutputAdapter>();

// Restore persisted focus
int persistedId = audioProcessor.getParameters().getFocusedControlId();
ui_core::ControlId focusIdToRestore = kGainControlId; // default fallback

// Validate: check if binding exists for persisted ID
if (bindingRegistry.find (static_cast<ui_core::ControlId> (persistedId)) != nullptr)
    focusIdToRestore = static_cast<ui_core::ControlId> (persistedId);

focusManager.setFocusedControl (focusIdToRestore);
if (hardwareOutput)
{
    // Clear other controls
    if (focusIdToRestore != kGainControlId)
        hardwareOutput->setFocus (kGainControlId, false);
    if (focusIdToRestore != kOutputControlId)
        hardwareOutput->setFocus (kOutputControlId, false);
    // Set focused control
    hardwareOutput->setFocus (focusIdToRestore, true);
}

setSize (400, 500);
```

**Key Points:**
- Reads persisted ID from Parameters
- Validates ID exists in binding registry
- Falls back to kGainControlId if invalid
- Sends hardware focus feedback on restore
- Clears focus on other controls

#### B. Focus Persistence on Gain Slider Drag

**Modified gainSlider.onDragStart callback:**
```cpp
gainSlider.onDragStart = [this]
{
    const auto prev = focusManager.getFocusedControl();
    focusManager.setFocusedControl (kGainControlId);
    audioProcessor.getParameters().setFocusedControlId (static_cast<int> (kGainControlId));  // NEW
    if (hardwareOutput)
    {
        if (prev && *prev != kGainControlId)
            hardwareOutput->setFocus (*prev, false);
        hardwareOutput->setFocus (kGainControlId, true);
    }
};
```

#### C. Focus Persistence on Output Slider Drag

**Modified outputSlider.onDragStart callback:**
```cpp
outputSlider.onDragStart = [this]
{
    const auto prev = focusManager.getFocusedControl();
    focusManager.setFocusedControl (kOutputControlId);
    audioProcessor.getParameters().setFocusedControlId (static_cast<int> (kOutputControlId));  // NEW
    if (hardwareOutput)
    {
        if (prev && *prev != kOutputControlId)
            hardwareOutput->setFocus (*prev, false);
        hardwareOutput->setFocus (kOutputControlId, true);
    }
};
```

#### D. Focus Persistence on Tab Key Navigation

**Modified Tab key handler in keyPressed():**
```cpp
if (key == juce::KeyPress::tabKey)
{
    // Tab cycles focus between controls
    const auto prev = focusManager.getFocusedControl();
    ui_core::ControlId newFocusId;
    if (focusedControlId == kGainControlId)
        newFocusId = kOutputControlId;
    else
        newFocusId = kGainControlId;
    
    focusManager.setFocusedControl (newFocusId);
    audioProcessor.getParameters().setFocusedControlId (static_cast<int> (newFocusId));  // NEW
    
    // Send hardware focus feedback
    if (hardwareOutput)
    {
        if (prev && *prev != newFocusId)
            hardwareOutput->setFocus (*prev, false);
        hardwareOutput->setFocus (newFocusId, true);
    }
    return true;
}
```

## Implementation Details

### State Storage

- **Location**: Stored in `Parameters` class (ValueTree)
- **Key**: `"focusedControlId"` (string property)
- **Type**: `int` (converted to/from `ui_core::ControlId`)
- **Default**: `1001` (kGainControlId) if missing

### Focus Restoration Flow

```
Editor Opens
    ↓
MainView Constructor
    ↓
Read Parameters::getFocusedControlId()
    ↓
Validate: Check bindingRegistry.find(id)
    ↓
If valid → use persisted ID
If invalid → fallback to kGainControlId (1001)
    ↓
focusManager.setFocusedControl(validId)
    ↓
hardwareOutput->setFocus(validId, true)
    ↓
Focus Restored ✓
```

### Focus Persistence Flow

```
User Interaction (Drag/Tab)
    ↓
focusManager.setFocusedControl(newId)
    ↓
Parameters::setFocusedControlId(newId)  // NEW
    ↓
Stored in Parameters (atomic)
    ↓
Saved to plugin state on next save
    ↓
Restored on next editor open
```

### Validation and Fallback

- **Validation**: Checks if control ID exists in `bindingRegistry`
- **Fallback**: Uses `kGainControlId` (1001) if:
  - Persisted ID is invalid (no binding found)
  - Property is missing from state
  - Property value is 0 (treated as invalid for this use case)

### Thread Safety

- **Parameters**: Uses `std::atomic<int>` for thread-safe access
- **State Persistence**: ValueTree operations are thread-safe (main thread only)
- **Focus Manager**: Operations occur on UI thread

### Hardware Feedback

- **On Restore**: Sends focus feedback to hardware output
  - Clears focus on other controls (setFocus(id, false))
  - Sets focus on restored control (setFocus(id, true))
- **On Change**: Hardware feedback already handled in existing callbacks

## Behavior

### Expected Behavior

1. **First Time Use:**
   - No persisted state exists
   - Defaults to Gain control (1001)
   - Hardware receives focus feedback for Gain

2. **After Setting Focus on Output:**
   - User drags Output slider → focus set to 1002
   - Focus ID persisted to Parameters
   - Plugin state saved → focus ID stored in ValueTree

3. **Editor Reopens:**
   - Persisted ID (1002) read from Parameters
   - Validation passes (binding exists)
   - Focus restored to Output control
   - Hardware receives focus feedback for Output

4. **Invalid ID Scenario:**
   - Persisted ID doesn't exist in binding registry
   - Falls back to Gain control (1001)
   - Hardware receives focus feedback for Gain

### Edge Cases Handled

- **Missing Property**: Defaults to 1001 (Gain)
- **Invalid Control ID**: Validates against binding registry, falls back to Gain
- **Control IDs Changed**: If persisted ID no longer exists, safely falls back
- **Zero Value**: Treated as invalid, falls back to Gain (though default is 1001)

## Testing

### Manual Verification Steps

1. **First Launch:**
   - Open plugin editor
   - Verify Gain control has focus (outline visible)
   - Check console for hardware focus log: `HW OUT FOCUS id=1001 focused=1`

2. **Change Focus and Persist:**
   - Drag Output slider → focus moves to Output
   - Check console: `HW OUT FOCUS id=1002 focused=1`
   - Save plugin state (close/reopen or save project)
   - Close editor

3. **Restore Focus:**
   - Reopen editor
   - Verify Output control has focus (outline visible)
   - Check console: `HW OUT FOCUS id=1002 focused=1` (immediately on open)

4. **Fallback Test:**
   - Manually edit plugin state to have invalid control ID (e.g., 9999)
   - Open editor
   - Verify Gain control has focus (fallback)
   - Check console: `HW OUT FOCUS id=1001 focused=1`

### Expected Console Output

**On Editor Open (Output was focused):**
```
HW OUT FOCUS id=1001 focused=0
HW OUT FOCUS id=1002 focused=0
HW OUT FOCUS id=1002 focused=1
```

**On Editor Open (Gain was focused or first time):**
```
HW OUT FOCUS id=1001 focused=0
HW OUT FOCUS id=1002 focused=0
HW OUT FOCUS id=1001 focused=1
```

## Integration Points

### Parameters Class

- **New Methods**: `getFocusedControlId()`, `setFocusedControlId()`
- **New Member**: `std::atomic<int> focusedControlId { 1001 }`
- **State Integration**: Added to `getState()` and `setState()`

### MainView Class

- **Constructor**: Added focus restoration logic
- **Focus Change Callbacks**: Added persistence calls to:
  - `gainSlider.onDragStart`
  - `outputSlider.onDragStart`
  - `keyPressed()` Tab handler

### State Persistence

- **Save**: Focus ID saved automatically when plugin state is saved
- **Load**: Focus ID restored when plugin state is loaded
- **Format**: Integer stored in ValueTree property

## Design Decisions

1. **Storage Location**: Stored in `Parameters` class (not `ui_core`) to keep UI core framework-agnostic
2. **Default Value**: `1001` (Gain) as sensible default for first-time use
3. **Validation**: Check binding registry to ensure control ID is valid
4. **Fallback**: Always fallback to Gain if validation fails
5. **Hardware Feedback**: Send focus feedback on restore to keep hardware in sync
6. **Atomic Access**: Use `std::atomic` for thread-safe parameter access

## Constraints Met

- ✅ Store persistence in Parameters state (ValueTree), not in ui_core
- ✅ No ui_core refactors
- ✅ Minimal diffs (only necessary files modified)
- ✅ Focus restoration safe if control IDs change (fallback to Gain)
- ✅ Hardware focus feedback sent on restore

## Summary

The focus persistence implementation provides a seamless user experience by remembering which control was focused when the editor was last closed. The implementation is thread-safe, validates persisted IDs, and gracefully handles edge cases. Focus state is integrated into the existing parameter persistence system and works seamlessly with hardware output feedback.
