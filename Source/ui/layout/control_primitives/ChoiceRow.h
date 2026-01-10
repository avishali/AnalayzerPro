#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>

namespace AnalyzerPro
{
namespace ControlPrimitives
{

/**
    Helper class for label + combo box rows.
    Encapsulates setup and layout of choice controls.
    
    Usage:
        ChoiceRow row (ui_, "Label Text", comboBox);
        row.attachToParent (parent);
        row.layout (bounds, y);
*/
class ChoiceRow
{
public:
    ChoiceRow (mdsp_ui::UiContext& ui, const juce::String& labelText, juce::ComboBox& combo);
    
    void attachToParent (juce::Component& parent);
    void layout (juce::Rectangle<int> bounds, int& y);
    
    juce::Label& getLabel() { return label_; }
    juce::ComboBox& getCombo() { return combo_; }

private:
    mdsp_ui::UiContext& ui_;
    juce::Label label_;
    juce::ComboBox& combo_;
};

} // namespace ControlPrimitives
} // namespace AnalyzerPro
