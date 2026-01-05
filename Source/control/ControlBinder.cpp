#include "ControlBinder.h"

namespace AnalyzerPro
{

//==============================================================================
// UiState implementation

float UiState::getValue(ControlId id) const noexcept
{
    juce::ScopedLock sl(lock);
    const auto it = values.find(id);
    return (it != values.end()) ? it->second : 0.0f;
}

void UiState::setValue(ControlId id, float normalizedValue) noexcept
{
    juce::ScopedLock sl(lock);
    values[id] = juce::jlimit(0.0f, 1.0f, normalizedValue);
}

void UiState::clear() noexcept
{
    juce::ScopedLock sl(lock);
    values.clear();
}

//==============================================================================
// ControlBinder implementation

ControlBinder::ControlBinder(juce::AudioProcessorValueTreeState* apvtsIn, ParamIdMap paramIdMapIn)
    : apvts(apvtsIn), paramIdMap(std::move(paramIdMapIn))
{
}

void ControlBinder::clear()
{
    sliderAttachments.clear();
    buttonAttachments.clear();
    comboAttachments.clear();
    uiState.clear();
}

void ControlBinder::bindSlider(ControlId id, juce::Slider& slider)
{
    if (apvts && paramIdMap)
    {
        const auto paramId = paramIdMap(id);
        if (paramId.isNotEmpty())
        {
            auto attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                *apvts, paramId, slider);
            sliderAttachments.push_back(std::move(attachment));
            return;
        }
    }

    // Fallback to UiState
    const float currentValue = uiState.getValue(id);
    const double range = slider.getMaximum() - slider.getMinimum();
    if (range > 0.0)
    {
        slider.setValue(slider.getMinimum() + currentValue * range, juce::dontSendNotification);
    }
    slider.onValueChange = [this, id, &slider]()
    {
        const double range = slider.getMaximum() - slider.getMinimum();
        if (range > 0.0)
        {
            const float normalized = static_cast<float>((slider.getValue() - slider.getMinimum()) / range);
            uiState.setValue(id, normalized);
        }
    };
}

void ControlBinder::bindToggle(ControlId id, juce::Button& button)
{
    if (apvts && paramIdMap)
    {
        const auto paramId = paramIdMap(id);
        if (paramId.isNotEmpty())
        {
            auto attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                *apvts, paramId, button);
            buttonAttachments.push_back(std::move(attachment));
            return;
        }
    }

    // Fallback to UiState
    const float currentValue = uiState.getValue(id);
    button.setToggleState(currentValue > 0.5f, juce::dontSendNotification);
    button.onClick = [this, id, &button]()
    {
        const float normalized = button.getToggleState() ? 1.0f : 0.0f;
        uiState.setValue(id, normalized);
    };
}

void ControlBinder::bindCombo(ControlId id, juce::ComboBox& combo)
{
    if (apvts && paramIdMap)
    {
        const auto paramId = paramIdMap(id);
        if (paramId.isNotEmpty())
        {
            auto attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                *apvts, paramId, combo);
            comboAttachments.push_back(std::move(attachment));
            return;
        }
    }

    // Fallback to UiState
    const float currentValue = uiState.getValue(id);
    const int numItems = combo.getNumItems();
    if (numItems > 1)
    {
        const int selectedIndex = juce::roundToInt(currentValue * (numItems - 1));
        combo.setSelectedId(selectedIndex + 1, juce::dontSendNotification);
    }
    else if (numItems == 1)
    {
        combo.setSelectedId(1, juce::dontSendNotification);
    }
    combo.onChange = [this, id, &combo]()
    {
        const int numItems = combo.getNumItems();
        if (numItems > 1)
        {
            const int selectedIndex = combo.getSelectedId() - 1;
            const float normalized = static_cast<float>(selectedIndex) / static_cast<float>(numItems - 1);
            uiState.setValue(id, normalized);
        }
        else if (numItems == 1)
        {
            uiState.setValue(id, 0.0f);
        }
    };
}

} // namespace AnalyzerPro

