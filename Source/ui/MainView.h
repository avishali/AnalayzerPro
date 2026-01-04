#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include <ui_core/UiCore.h>
#include "hardware/PluginHardwareAdapter.h"
#include "hardware/PluginHardwareOutputAdapter.h"
#include <memory>

//==============================================================================
/**
    Main UI view component.
    Contains the plugin's user interface elements.
*/
class MainView : public juce::Component
{
public:
    explicit MainView (PluginTemplateAudioProcessor& p);
    ~MainView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    PluginTemplateAudioProcessor& audioProcessor;

    juce::Label gainLabel;
    juce::Slider gainSlider;
    juce::Label outputLabel;
    juce::Slider outputSlider;

    ui_core::FocusManager focusManager;
    ui_core::BindingRegistry bindingRegistry;
    std::unique_ptr<PluginHardwareAdapter> hardwareAdapter;
    std::unique_ptr<PluginHardwareOutputAdapter> hardwareOutput;

    // Focus state: tracks which control is focused (0 = none)
    ui_core::ControlId focusedControlId = 0;

    // Adapter object that FocusManager can call
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

    void gainSliderChanged();
    void outputSliderChanged();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};
