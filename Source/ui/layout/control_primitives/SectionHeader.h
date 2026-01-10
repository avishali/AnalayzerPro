#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>

namespace AnalyzerPro
{
namespace ControlPrimitives
{

/**
    Helper class for section header labels.
    Encapsulates setup and layout of section titles.
    
    Usage:
        SectionHeader header (ui_, "Section Name");
        header.attachToParent (parent);
        header.layout (bounds, y);
*/
class SectionHeader
{
public:
    explicit SectionHeader (mdsp_ui::UiContext& ui, const juce::String& text);
    
    void attachToParent (juce::Component& parent);
    void layout (juce::Rectangle<int> bounds, int& y);
    
    juce::Label& getLabel() { return label_; }

private:
    mdsp_ui::UiContext& ui_;
    juce::Label label_;
};

} // namespace ControlPrimitives
} // namespace AnalyzerPro
