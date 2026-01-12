# Two Parameter Focus Navigation Implementation Summary

This document summarizes the implementation of a second parameter (outputGain) with focus navigation between two controls.

## Overview

Extended the template to support two parameters (Gain and Output Gain) with focus navigation. The implementation demonstrates:
- Multi-control focus management using ControlId tracking
- Tab key navigation between controls
- Focus-aware hardware control (H/J/K keys operate on focused control)
- Visual focus feedback on the currently focused control

## Files Modified

### 1. `Source/parameters/Parameters.h`

**Added outputGain Parameter:**
- Added `std::atomic<float> outputGain { 1.0f };` member
- Added `getOutputGain()` and `setOutputGain(float)` methods
- Maintains thread-safe access pattern consistent with existing gain parameter

**Key Changes:**
```cpp
float getOutputGain() const noexcept;
void setOutputGain (float newOutputGain) noexcept;

private:
    std::atomic<float> gain;
    std::atomic<float> outputGain { 1.0f };
```

### 2. `Source/parameters/Parameters.cpp`

**Implementation Details:**
- Implemented getter/setter for outputGain with 0..2 range clamping
- Added state persistence: `getState()` and `setState()` now handle both parameters
- Backward compatible: defaults to 1.0f if "outputGain" key is missing in state

**Key Changes:**
```cpp
float Parameters::getOutputGain() const noexcept
{
    return outputGain.load();
}

void Parameters::setOutputGain (float newOutputGain) noexcept
{
    outputGain.store (juce::jlimit (0.0f, 2.0f, newOutputGain));
}

void Parameters::getState (juce::ValueTree& state) const
{
    state.setProperty ("gain", getGain(), nullptr);
    state.setProperty ("outputGain", getOutputGain(), nullptr);
}

void Parameters::setState (const juce::ValueTree& state)
{
    setGain (static_cast<float> (state.getProperty ("gain", 1.0)));
    setOutputGain (static_cast<float> (state.getProperty ("outputGain", 1.0)));
}
```

### 3. `Source/ui/MainView.h`

**Added UI Components:**
- `juce::Label outputLabel`
- `juce::Slider outputSlider`
- `FocusFlagAdapter outputFocusAdapter`
- `outputSliderChanged()` method

**Focus System Refactoring:**
- Replaced `bool gainFocused` with `ui_core::ControlId focusedControlId = 0`
- Updated `FocusFlagAdapter` to track ControlId instead of bool
- Adapter now sets/clears focusedControlId based on focus state

**Key Changes:**
```cpp
// Focus state: tracks which control is focused (0 = none)
ui_core::ControlId focusedControlId = 0;

struct FocusFlagAdapter : ui_core::Focusable
{
    ui_core::ControlId controlId = 0;
    ui_core::ControlId* focusedControlIdPtr = nullptr;
    juce::Component* repaintTarget = nullptr;
    void setFocused (bool focused) override
    {
        if (focusedControlIdPtr)
        {
            if (focused)
                *focusedControlIdPtr = controlId;
            else if (*focusedControlIdPtr == controlId)
                *focusedControlIdPtr = 0;
        }
        if (repaintTarget)
            repaintTarget->repaint();
    }
};

FocusFlagAdapter gainFocusAdapter;
FocusFlagAdapter outputFocusAdapter;
```

### 4. `Source/ui/MainView.cpp`

**Control IDs:**
```cpp
static constexpr ui_core::ControlId kGainControlId = 1001;
static constexpr ui_core::ControlId kOutputControlId = 1002;
```

**UI Setup:**
- Added Output Label and Slider configuration (matching Gain slider style)
- Both sliders use range 0.0-2.0 with 0.01 step
- Both sliders trigger focus on drag start

**Focus Adapter Registration:**
```cpp
// Setup focus adapters
gainFocusAdapter.controlId = kGainControlId;
gainFocusAdapter.focusedControlIdPtr = &focusedControlId;
gainFocusAdapter.repaintTarget = this;
focusManager.registerWidget (kGainControlId, &gainFocusAdapter);

outputFocusAdapter.controlId = kOutputControlId;
outputFocusAdapter.focusedControlIdPtr = &focusedControlId;
outputFocusAdapter.repaintTarget = this;
focusManager.registerWidget (kOutputControlId, &outputFocusAdapter);
```

**Parameter Bindings:**
- Added second mapped binding for Output Gain
- Both bindings use normalized 0..1 ↔ native 0..2 mapping
- Both update their respective sliders when values change

```cpp
bindingRegistry.add (ui_core::makeMappedBinding (
    kOutputControlId,
    [this]() { return audioProcessor.getParameters().getOutputGain(); },
    [this](float native)
    {
        audioProcessor.getParameters().setOutputGain (native);
        outputSlider.setValue (native, juce::dontSendNotification);
    },
    [](float normalized) { return normalized * 2.0f; },
    [](float native) { return native / 2.0f; }));
```

**Focus Rendering:**
- Updated `paint()` to check `focusedControlId` instead of `gainFocused`
- Draws outline on whichever control is currently focused

```cpp
void MainView::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    if (focusedControlId == kGainControlId)
    {
        auto focusBounds = gainSlider.getBounds().expanded (4);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawRoundedRectangle (focusBounds.toFloat(), 12.0f, 2.0f);
    }
    else if (focusedControlId == kOutputControlId)
    {
        auto focusBounds = outputSlider.getBounds().expanded (4);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawRoundedRectangle (focusBounds.toFloat(), 12.0f, 2.0f);
    }
}
```

**Layout:**
- Updated `resized()` to stack two controls vertically
- Increased view size from 400x300 to 400x500
- Both sliders use same size (150x150) and centered positioning

```cpp
void MainView::resized()
{
    auto area = getLocalBounds().reduced (20);
    auto sliderSize = 150;
    
    auto gainArea = area.removeFromTop (200);
    gainSlider.setBounds (gainArea.withSizeKeepingCentre (sliderSize, sliderSize));
    
    auto outputArea = area.removeFromTop (200);
    outputSlider.setBounds (outputArea.withSizeKeepingCentre (sliderSize, sliderSize));
}
```

**Keyboard Navigation:**
- **Tab Key**: Cycles focus between Gain and Output controls
- **H Key**: Sets focused control to normalized 0.375 (native 0.75)
- **J Key**: Increases focused control by +0.025 normalized
- **K Key**: Decreases focused control by -0.025 normalized
- Defaults to Gain control if no control is focused

```cpp
bool MainView::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::tabKey)
    {
        // Tab cycles focus between controls
        if (focusedControlId == kGainControlId)
            focusManager.setFocusedControl (kOutputControlId);
        else
            focusManager.setFocusedControl (kGainControlId);
        return true;
    }

    // Hardware control keys operate on focused control (default to gain if none focused)
    ui_core::ControlId targetId = focusedControlId != 0 ? focusedControlId : kGainControlId;
    
    auto ch = key.getTextCharacter();
    if (ch == 'h' || ch == 'H')
    {
        ui_core::HardwareControlEvent e { targetId, 0.375f, false };
        hardwareAdapter->processEvent (e);
        return true;
    }
    if (ch == 'j' || ch == 'J')
    {
        ui_core::HardwareControlEvent e { targetId, 0.025f, true };
        hardwareAdapter->processEvent (e);
        return true;
    }
    if (ch == 'k' || ch == 'K')
    {
        ui_core::HardwareControlEvent e { targetId, -0.025f, true };
        hardwareAdapter->processEvent (e);
        return true;
    }
    return false;
}
```

## Architecture Changes

### Focus System Evolution

**Before:**
- Single boolean flag (`gainFocused`)
- Only one control could be tracked
- Focus state tied to specific control

**After:**
- ControlId-based tracking (`focusedControlId`)
- Supports multiple controls
- Generic focus state (0 = none, ControlId = focused control)
- Focus adapters update shared focus state

### Focus Adapter Pattern

The `FocusFlagAdapter` now:
1. Stores its own `controlId`
2. Points to shared `focusedControlId` state
3. Updates state when focus changes
4. Clears state only if it matches (prevents race conditions)

## Behavior

### Focus Navigation

1. **Mouse Interaction**: Clicking/dragging a slider sets focus to that control
2. **Tab Navigation**: Tab key cycles focus between Gain → Output → Gain
3. **Visual Feedback**: Outline appears on currently focused control
4. **Hardware Control**: H/J/K keys operate on the focused control

### Parameter Control

1. **Independent Operation**: Both parameters work independently
2. **Normalized Space**: Hardware adapter uses normalized 0..1 values
3. **Native Space**: UI and DSP use native 0..2 range
4. **State Persistence**: Both parameters saved/restored together

### Default Behavior

- If no control is focused, hardware keys (H/J/K) default to Gain control
- Tab always cycles between the two controls
- Focus outline only appears when a control is actually focused

## Testing

### Manual Verification

1. **Slider Interaction:**
   - Click/drag Gain slider → Focus outline appears on Gain
   - Click/drag Output slider → Focus outline appears on Output
   - Outline moves between controls as expected

2. **Tab Navigation:**
   - Press Tab → Focus cycles from Gain to Output (or Output to Gain)
   - Outline follows focus
   - Multiple Tab presses cycle correctly

3. **Hardware Control:**
   - Focus Gain, press H → Gain set to 0.75 (normalized 0.375)
   - Focus Output, press H → Output set to 0.75
   - Press J/K to increment/decrement focused control
   - Values clamp correctly at 0.0 and 2.0

4. **State Persistence:**
   - Change both parameters
   - Save plugin state
   - Reload → Both parameters restored correctly

5. **UI Updates:**
   - Hardware control updates slider positions
   - Slider changes update parameters
   - No recursion or infinite loops

## Implementation Notes

### Design Decisions

1. **ControlId Tracking**: Using ControlId (uint32_t) instead of bool enables multi-control support
2. **Shared State**: Both adapters update same `focusedControlId` for consistency
3. **Default Focus**: Hardware keys default to Gain if no focus set (user-friendly)
4. **Stacked Layout**: Vertical layout keeps UI simple and consistent

### Thread Safety

- Parameters use `std::atomic<float>` for thread-safe access
- Focus state only accessed on UI thread (no threading concerns)
- Bindings safely update both audio thread (Parameters) and UI thread (Sliders)

### Code Quality

- ✅ Minimal diffs (only necessary files modified)
- ✅ Consistent patterns (Output matches Gain implementation)
- ✅ No code duplication (shared adapter pattern)
- ✅ Clean separation of concerns
- ✅ Backward compatible state loading

## Summary

The implementation successfully adds a second parameter with focus navigation. Key achievements:

- **Two Parameters**: Gain and Output Gain work independently
- **Focus Navigation**: Tab key cycles between controls
- **Focus-Aware Hardware Control**: H/J/K keys operate on focused control
- **Visual Feedback**: Outline shows which control is focused
- **Clean Architecture**: ControlId-based focus system scales to more controls
- **State Persistence**: Both parameters saved/restored correctly
- **Minimal Changes**: Only necessary files modified, architecture preserved

The template now demonstrates multi-control focus management and hardware control integration, providing a solid foundation for plugins with multiple parameters.
