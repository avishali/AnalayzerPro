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
    // Canonical parameter boundary:
    // All incoming values (UI, hardware, automation, modulation)
    // are clamped here and nowhere else.
    gain.store (juce::jlimit (0.0f, 2.0f, newGain));
}

float Parameters::getOutputGain() const noexcept
{
    return outputGain.load();
}

void Parameters::setOutputGain (float newOutputGain) noexcept
{
    outputGain.store (juce::jlimit (0.0f, 2.0f, newOutputGain));
}

int Parameters::getFocusedControlId() const noexcept
{
    return focusedControlId.load();
}

void Parameters::setFocusedControlId (int id) noexcept
{
    focusedControlId.store (id);
}

void Parameters::getState (juce::ValueTree& state) const
{
    state.setProperty ("gain", getGain(), nullptr);
    state.setProperty ("outputGain", getOutputGain(), nullptr);
    state.setProperty ("focusedControlId", getFocusedControlId(), nullptr);

        // ADD — Source/parameters/Parameters.cpp (inside Parameters::getState(ValueTree& state))
    state.setProperty ("editorWidth",  getEditorWidth(),  nullptr);
    state.setProperty ("editorHeight", getEditorHeight(), nullptr);

}

void Parameters::setState (const juce::ValueTree& state)
{
    setGain (static_cast<float> (state.getProperty ("gain", 1.0)));
    setOutputGain (static_cast<float> (state.getProperty ("outputGain", 1.0)));
    setFocusedControlId (static_cast<int> (state.getProperty ("focusedControlId", 1001)));

        // ADD — Source/parameters/Parameters.cpp (inside Parameters::setState(const ValueTree& state))
    if (state.hasProperty ("editorWidth") && state.hasProperty ("editorHeight"))
    {
        setEditorSize ((int) state["editorWidth"], (int) state["editorHeight"]);
    }

}
