#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>

namespace AnalyzerPro
{
namespace ControlPrimitives
{

/**
    Helper class for label + slider rows.
    Encapsulates setup and layout of slider controls.
    
    Usage:
        SliderRow row (ui_, "Label Text", slider, min, max, step, defaultValue);
        row.attachToParent (parent);
        row.layout (bounds, y);
*/
class SliderRow
{
public:
    SliderRow (mdsp_ui::UiContext& ui,
               const juce::String& labelText,
               juce::Slider& slider,
               double minValue,
               double maxValue,
               double stepValue,
               double defaultValue);
    
    void attachToParent (juce::Component& parent);
    void layout (juce::Rectangle<int> bounds, int& y);
    
    juce::Label& getLabel() { return label_; }
    juce::Slider& getSlider() { return slider_; }

private:
    mdsp_ui::UiContext& ui_;
    juce::Label label_;
    juce::Slider& slider_;
};

} // namespace ControlPrimitives
} // namespace AnalyzerPro
