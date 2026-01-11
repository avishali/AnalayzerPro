#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>
#include "CompactControlRow.h"

namespace AnalyzerPro
{
namespace ControlPrimitives
{

/**
    Helper class for label + toggle button rows.
    Encapsulates setup and layout of toggle controls.
    
    Usage:
        ToggleRow row (ui_, "Label Text", toggleButton);
        row.attachToParent (parent);
        row.layout (bounds, y);
*/
class ToggleRow : public CompactControlRow
{
public:
    ToggleRow (mdsp_ui::UiContext& ui, const juce::String& labelText, juce::ToggleButton& toggle);
    
    void attachToParent (juce::Component& parent);
    void layout (juce::Rectangle<int> bounds, int& y);
    void resized() override;
    
    juce::Label& getLabel() { return label_; }
    juce::ToggleButton& getToggle() { return toggle_; }

private:
    juce::Label label_;
    juce::ToggleButton& toggle_;
};

} // namespace ControlPrimitives
} // namespace AnalyzerPro
