#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace mdsp_ui
{

/**
    Data model for a hover tooltip.
    Represents what to show and where to show it.
*/
struct TooltipSpec
{
    // Unique identifier for the tooltip source (e.g. "param_gain")
    juce::String id;

    // Static text content
    juce::String title;
    juce::String description;

    // Dynamic value provider (optional)
    // Caller provides a function that returns the current value string.
    // This allows the tooltip to update value while visible without continuous polling logic in the manager.
    std::function<juce::String()> valueProvider;

    // Anchor Provider
    // Returns the screen or local bounds of the target component using shared coordinates (usually Editor-relative).
    // The manager will use this to position the tooltip overlay.
    std::function<juce::Rectangle<int>()> anchorRectProvider;
};

} // namespace mdsp_ui
