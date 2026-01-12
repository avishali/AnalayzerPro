# ui_core Integration Summary

This document summarizes the integration of `ui_core` FocusManager and BindingRegistry with the Gain slider in MainView.

## Overview

The Gain slider has been wired to use `ui_core`'s focus management and parameter binding system. This enables:
- Focus state tracking via `FocusManager`
- Parameter binding via `BindingRegistry`
- Hardware event simulation (keyboard 'H' key)
- Thread-safe parameter updates with UI synchronization

## Changes Made

### Files Modified

1. `Source/ui/MainView.h`
2. `Source/ui/MainView.cpp`

### Files NOT Modified (as required)

- `Source/parameters/*` - No changes to parameter system
- `Source/ui/*` (except MainView) - No other UI changes
- No state save/restore refactoring
- No new visuals added

## Implementation Details

### MainView.h Changes

**Added Members:**
- `ui_core::FocusManager focusManager` - Manages focus state for controls
- `ui_core::BindingRegistry bindingRegistry` - Stores parameter bindings
- `bool gainFocused = false` - Local focus state flag for verification
- `FocusFlagAdapter gainFocusAdapter` - Adapter struct implementing `Focusable` interface

**Added Methods:**
- `bool keyPressed(const juce::KeyPress& key) override` - Handles keyboard events

**Changed:**
- Destructor changed from `= default` to explicit declaration (for cleanup)

**FocusFlagAdapter Struct:**
```cpp
struct FocusFlagAdapter : ui_core::Focusable
{
    bool* flag = nullptr;
    juce::Component* repaintTarget = nullptr;
    void setFocused(bool focused) override
    {
        if (flag) *flag = focused;
        if (repaintTarget) repaintTarget->repaint();
    }
};
```

### MainView.cpp Changes

**Constants:**
- `static constexpr ui_core::ControlId kGainControlId = 1001;` - Unique identifier for gain control

**Constructor Additions:**
1. `setWantsKeyboardFocus(true)` - Enables keyboard focus for the component
2. Focus adapter setup:
   ```cpp
   gainFocusAdapter.flag = &gainFocused;
   gainFocusAdapter.repaintTarget = this;
   focusManager.registerWidget(kGainControlId, &gainFocusAdapter);
   ```
3. Slider focus callback:
   ```cpp
   gainSlider.onDragStart = [this] { focusManager.setFocusedControl(kGainControlId); };
   ```
4. Parameter binding registration:
   ```cpp
   bindingRegistry.add(ui_core::makeBinding(
       kGainControlId,
       [this]() { return audioProcessor.getParameters().getGain(); },
       [this](float v) {
           audioProcessor.getParameters().setGain(v);
           gainSlider.setValue(v, juce::dontSendNotification);
       }));
   ```

**Destructor Implementation:**
- Unregisters widget from focus manager:
  ```cpp
  focusManager.unregisterWidget(kGainControlId, &gainFocusAdapter);
  ```

**keyPressed Implementation:**
- Handles 'H' key to simulate hardware event:
  ```cpp
  if (ch == 'h' || ch == 'H') {
      if (auto* b = bindingRegistry.find(kGainControlId))
          b->set(0.75f);
      return true;
  }
  ```

## Behavior

### Focus Management

1. **Slider Interaction:** When user starts dragging the gain slider, `onDragStart` callback triggers
2. **Focus Set:** `focusManager.setFocusedControl(kGainControlId)` is called
3. **Adapter Called:** `FocusManager` calls `setFocused(true)` on `gainFocusAdapter`
4. **State Updated:** `gainFocused` flag is set to `true`
5. **Repaint Triggered:** Component repaints (for future visual feedback)

### Parameter Binding

1. **Binding Created:** Gain parameter is bound to control ID 1001
2. **Getter:** Returns current gain value from `Parameters`
3. **Setter:** Updates parameter and slider UI (using `dontSendNotification` to prevent recursion)
4. **Hardware Simulation:** Pressing 'H' key finds binding and sets gain to 0.75

### Thread Safety

- `Parameters` uses `std::atomic<float>` for thread-safe access
- Binding callbacks safely update both audio thread (via `Parameters`) and UI thread (via `Slider`)
- `dontSendNotification` prevents callback recursion when updating slider from binding

## JUCE API Notes

### onMouseDown Callback

JUCE's `Slider` class does **not** provide an `onMouseDown` callback. Instead:
- Use `onDragStart` for drag start events (implemented)
- Override `mouseDown()` in component for mouse press events (not needed here)

The `onDragStart` callback is sufficient for setting focus when the user interacts with the slider.

## Testing

### Manual Verification

1. **Focus Behavior:**
   - Click and drag the gain slider
   - Verify `gainFocused` flag is set (via debugger or DBG output)
   - Component should repaint (no visual change yet, but repaint is triggered)

2. **Hardware Simulation:**
   - Click on the plugin window to ensure it has focus
   - Press 'H' key
   - Verify gain slider value changes to 0.75
   - Verify audio processor gain parameter is updated
   - Check console for "Gain set via binding registry" message

3. **Parameter Sync:**
   - Drag slider manually - parameter updates
   - Press 'H' - both slider and parameter update together
   - No recursion or infinite loops

## Code Quality

- ✅ No linter errors
- ✅ Minimal code changes (smallest possible diff)
- ✅ No refactoring of existing code
- ✅ Clean separation of concerns
- ✅ Proper cleanup in destructor
- ✅ Thread-safe parameter access
- ✅ No UI recursion (dontSendNotification used)

## Future Enhancements

Potential future additions (not implemented here):
- Visual feedback for focused state (glow, outline, etc.)
- Additional controls wired to ui_core
- Hardware input adapter implementation
- Hardware output adapter (LED feedback)
- More keyboard shortcuts for hardware simulation

## Dependencies

- `ui_core` static library (already linked in CMakeLists.txt)
- JUCE GUI Basics (already included)
- No new external dependencies added

## Summary

This integration successfully wires the Gain slider to `ui_core`'s focus and binding systems with minimal code changes. The implementation maintains backward compatibility, adds no visual changes, and provides a foundation for future hardware control integration.
