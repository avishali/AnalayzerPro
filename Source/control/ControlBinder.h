#pragma once

#include "ControlIds.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <unordered_map>
#include <memory>
#include <type_traits>
#include <vector>

namespace AnalyzerPro
{

//==============================================================================
// Hash function for ControlId enum class
struct ControlIdHash
{
    std::size_t operator()(ControlId id) const noexcept
    {
        return std::hash<std::underlying_type_t<ControlId>>{}(static_cast<std::underlying_type_t<ControlId>>(id));
    }
};

//==============================================================================
/**
    UiState stores normalized values (0.0-1.0) for controls without APVTS parameters.
    Thread-safe for UI thread access only.
*/
class UiState
{
public:
    float getValue(ControlId id) const noexcept;
    void setValue(ControlId id, float normalizedValue) noexcept;
    void clear() noexcept;

private:
    std::unordered_map<ControlId, float, ControlIdHash> values;
    mutable juce::CriticalSection lock;
};

//==============================================================================
/**
    ParamIdMap converts ControlId to APVTS parameter ID string.
    Returns empty string if no parameter exists for the given ControlId.
*/
using ParamIdMap = std::function<juce::String(ControlId)>;

//==============================================================================
/**
    ControlBinder connects UI controls to CONTROL_IDS.
    Uses APVTS when ParamIdMap provides a parameter ID, otherwise falls back to UiState.
    All operations are UI-thread only.
*/
class ControlBinder
{
public:
    ControlBinder(juce::AudioProcessorValueTreeState* apvts, ParamIdMap paramIdMap);
    ~ControlBinder() = default;

    void clear();

    void bindSlider(ControlId id, juce::Slider& slider);
    void bindToggle(ControlId id, juce::Button& button);
    void bindCombo(ControlId id, juce::ComboBox& combo);

    UiState& getUiState() noexcept { return uiState; }
    const UiState& getUiState() const noexcept { return uiState; }

private:
    juce::AudioProcessorValueTreeState* apvts;
    ParamIdMap paramIdMap;
    UiState uiState;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;
};

} // namespace AnalyzerPro
