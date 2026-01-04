// This class defines the canonical parameter pattern for all plugins using this template.

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

    float getOutputGain() const noexcept;
    void setOutputGain (float newOutputGain) noexcept;

    int getFocusedControlId() const noexcept;
    void setFocusedControlId (int id) noexcept;

    //==============================================================================
    void getState (juce::ValueTree& state) const;
    void setState (const juce::ValueTree& state);

        // ADD — Source/parameters/Parameters.h (public section)
    int  getEditorWidth()  const noexcept { return editorWidth.load(); }
    int  getEditorHeight() const noexcept { return editorHeight.load(); }

    void setEditorSize (int w, int h) noexcept
    {
        // clamp to reasonable limits (match your resize limits)
        editorWidth.store  (juce::jlimit (360, 900, w));
        editorHeight.store (juce::jlimit (360, 900, h));
    }


private:
    std::atomic<float> gain;
    std::atomic<float> outputGain { 1.0f };
    std::atomic<int> focusedControlId { 1001 };
    // ADD — Source/parameters/Parameters.h (inside class Parameters, private section)
    std::atomic<int> editorWidth  { 420 };
    std::atomic<int> editorHeight { 520 };


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Parameters)
};
