# Template Changes Summary

This document details all changes made to make the template production-ready with a gain parameter and UI.

## New Files Created

### 1. `Source/parameters/Parameters.h` (NEW)
```cpp
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

//==============================================================================
/**
    Simple parameter container without APVTS.
    Owns plugin parameters with thread-safe access.
*/
class Parameters
{
public:
    Parameters();
    ~Parameters() = default;

    //==============================================================================
    float getGain() const noexcept;
    void setGain (float newGain) noexcept;

    //==============================================================================
    void getState (juce::ValueTree& state) const;
    void setState (const juce::ValueTree& state);

private:
    std::atomic<float> gain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Parameters)
};
```

### 2. `Source/parameters/Parameters.cpp` (NEW)
```cpp
#include "Parameters.h"

//==============================================================================
Parameters::Parameters()
    : gain (1.0f)
{
}

float Parameters::getGain() const noexcept
{
    return gain.load();
}

void Parameters::setGain (float newGain) noexcept
{
    gain.store (juce::jlimit (0.0f, 2.0f, newGain));
}

void Parameters::getState (juce::ValueTree& state) const
{
    state.setProperty ("gain", getGain(), nullptr);
}

void Parameters::setState (const juce::ValueTree& state)
{
    setGain (static_cast<float> (state.getProperty ("gain", 1.0)));
}
```

### 3. `Source/ui/MainView.h` (NEW)
```cpp
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

//==============================================================================
/**
    Main UI view component.
    Contains the plugin's user interface elements.
*/
class MainView : public juce::Component
{
public:
    explicit MainView (PluginTemplateAudioProcessor& p);
    ~MainView() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PluginTemplateAudioProcessor& audioProcessor;

    juce::Label gainLabel;
    juce::Slider gainSlider;

    void gainSliderChanged();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};
```

### 4. `Source/ui/MainView.cpp` (NEW)
```cpp
#include "MainView.h"

//==============================================================================
MainView::MainView (PluginTemplateAudioProcessor& p)
    : audioProcessor (p)
{
    // Gain Label
    gainLabel.setText ("Gain", juce::dontSendNotification);
    gainLabel.attachToComponent (&gainSlider, false);
    gainLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (gainLabel);

    // Gain Slider
    gainSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    gainSlider.setRange (0.0, 2.0, 0.01);
    gainSlider.setValue (audioProcessor.getParameters().getGain());
    gainSlider.onValueChange = [this] { gainSliderChanged(); };
    addAndMakeVisible (gainSlider);

    setSize (400, 300);
}

void MainView::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainView::resized()
{
    auto area = getLocalBounds().reduced (20);
    gainSlider.setBounds (area.removeFromTop (200).withSizeKeepingCentre (150, 150));
}

void MainView::gainSliderChanged()
{
    audioProcessor.getParameters().setGain (static_cast<float> (gainSlider.getValue()));
}
```

## Modified Files

### 5. `Source/PluginProcessor.h`

**Added after line 3:**
```cpp
#include "parameters/Parameters.h"
```

**Added before the private section (after line 48):**
```cpp
    //==============================================================================
    Parameters& getParameters() { return parameters; }
    const Parameters& getParameters() const { return parameters; }
```

**Added in private section (after line 49):**
```cpp
    Parameters parameters;
```

**Full diff context:**
- Line 4: Added `#include "parameters/Parameters.h"`
- Lines 50-52: Added `getParameters()` accessor methods
- Line 56: Added `Parameters parameters;` member variable

### 6. `Source/PluginProcessor.cpp`

**In `processBlock()` method, replaced lines 112-114:**
```cpp
    // Process audio here
    // This is a template - add your processing logic
```

**With:**
```cpp
    // Apply gain
    const auto gainValue = parameters.getGain();
    if (gainValue != 1.0f)
    {
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
            buffer.applyGain (channel, 0, buffer.getNumSamples(), gainValue);
    }
```

**Replaced `getStateInformation()` method (lines 128-132):**
```cpp
void PluginTemplateAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ignoreUnused (destData);
    // You should use this method to store your parameters in the memory block.
}
```

**With:**
```cpp
void PluginTemplateAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state ("PluginState");
    parameters.getState (state);
    juce::MemoryOutputStream mos (destData, true);
    state.writeToStream (mos);
}
```

**Replaced `setStateInformation()` method (lines 134-138):**
```cpp
void PluginTemplateAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::ignoreUnused (data, sizeInBytes);
    // You should use this method to restore your parameters from this memory block.
}
```

**With:**
```cpp
void PluginTemplateAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData (data, static_cast<size_t> (sizeInBytes));
    if (tree.isValid())
        parameters.setState (tree);
}
```

### 7. `Source/PluginEditor.h`

**Added after line 5:**
```cpp
#include "ui/MainView.h"
```

**Added in private section (after line 23):**
```cpp
    MainView mainView;
```

**Full diff context:**
- Line 6: Added `#include "ui/MainView.h"`
- Line 24: Added `MainView mainView;` member variable

### 8. `Source/PluginEditor.cpp`

**Modified constructor (line 5-8), changed from:**
```cpp
PluginTemplateAudioProcessorEditor::PluginTemplateAudioProcessorEditor (PluginTemplateAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (400, 300);
}
```

**To:**
```cpp
PluginTemplateAudioProcessorEditor::PluginTemplateAudioProcessorEditor (PluginTemplateAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), mainView (p)
{
    addAndMakeVisible (mainView);
    setSize (400, 300);
}
```

**Replaced `paint()` method (lines 16-23):**
```cpp
void PluginTemplateAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ("Plugin Template", getLocalBounds(), juce::Justification::centred, 1);
}
```

**With:**
```cpp
void PluginTemplateAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}
```

**Replaced `resized()` method (lines 25-29):**
```cpp
void PluginTemplateAudioProcessorEditor::resized()
{
    // This is where you'll lay out the positions of any
    // subcomponents in your editor..
}
```

**With:**
```cpp
void PluginTemplateAudioProcessorEditor::resized()
{
    mainView.setBounds (getLocalBounds());
}
```

### 9. `CMakeLists.txt`

**Added to `target_sources()` section (after line 129):**
```cmake
        Source/parameters/Parameters.cpp
        Source/parameters/Parameters.h
        Source/ui/MainView.cpp
        Source/ui/MainView.h
```

**Full context (lines 124-134):**
```cmake
target_sources(${PLUGIN_NAME}
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/parameters/Parameters.cpp
        Source/parameters/Parameters.h
        Source/ui/MainView.cpp
        Source/ui/MainView.h
)
```

## Rename Tokens

When using this template, you should search and replace the following tokens:

### Primary Rename Tokens:

1. **`PluginTemplateAudioProcessor`** → Your plugin processor class name
   - Used in: `PluginProcessor.h`, `PluginProcessor.cpp`, `PluginEditor.h`, `PluginEditor.cpp`, `ui/MainView.h`, `ui/MainView.cpp`

2. **`PluginTemplateAudioProcessorEditor`** → Your plugin editor class name
   - Used in: `PluginEditor.h`, `PluginEditor.cpp`, `PluginProcessor.cpp`

3. **`PluginTemplate`** → Your plugin name
   - Used in: `CMakeLists.txt` (line 43: `set(PLUGIN_NAME "PluginTemplate" ...)`)

### CMake Configuration Tokens (in CMakeLists.txt):

These should be updated when creating a new plugin:

- **`PLUGIN_NAME`** (line 43): Set to your plugin name
- **`COMPANY_NAME`** (line 47): Set to your company name
- **`PLUGIN_CODE`** (line 48): Set to your 4-character plugin code
- **`MANUFACTURER_CODE`** (line 49): Set to your 4-character manufacturer code

### Optional Rename Tokens:

- **`MainView`** → Your main UI component name (if you want to rename it)
  - Used in: `ui/MainView.h`, `ui/MainView.cpp`, `PluginEditor.h`, `PluginEditor.cpp`

- **`Parameters`** → Your parameters class name (if you want to rename it)
  - Used in: `parameters/Parameters.h`, `parameters/Parameters.cpp`, `PluginProcessor.h`, `PluginProcessor.cpp`

## Summary

- **4 new files** created (Parameters.h/cpp, MainView.h/cpp)
- **5 files** modified (PluginProcessor.h/cpp, PluginEditor.h/cpp, CMakeLists.txt)
- **2 new folders** created (Source/parameters/, Source/ui/)
- **No external dependencies** added
- **No APVTS** used (simple parameter pattern)
- **Thread-safe** parameter access (std::atomic<float>)
- **State persistence** implemented (ValueTree-based)
