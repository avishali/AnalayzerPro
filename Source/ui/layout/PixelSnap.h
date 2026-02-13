#pragma once

#include <juce_graphics/juce_graphics.h>
#include <cmath>

namespace AnalyzerPro
{
namespace Layout
{

/** Snaps a value to the nearest integer pixel (for sub-pixel rounding at HiDPI). */
inline float snapToPixel (float v) noexcept
{
    return std::round (v);
}

/** Snaps a rectangle so x, y, width, height are integer pixels (edges land on pixel boundaries). */
inline juce::Rectangle<int> snapRectToPixels (juce::Rectangle<int> r) noexcept
{
    return juce::Rectangle<int> (
        static_cast<int> (std::round (static_cast<float> (r.getX()))),
        static_cast<int> (std::round (static_cast<float> (r.getY()))),
        static_cast<int> (std::round (static_cast<float> (r.getWidth()))),
        static_cast<int> (std::round (static_cast<float> (r.getHeight()))));
}

/** Snaps a float rect to integer pixel boundaries. */
inline juce::Rectangle<float> snapRectToPixels (juce::Rectangle<float> r) noexcept
{
    return juce::Rectangle<float> (
        std::round (r.getX()),
        std::round (r.getY()),
        std::round (r.getWidth()),
        std::round (r.getHeight()));
}

/** Returns rect inset by half the stroke width (for 1px inside stroke), then snapped. */
inline juce::Rectangle<float> insetForInsideStroke (juce::Rectangle<float> r, float strokePx) noexcept
{
    const float half = strokePx * 0.5f;
    return snapRectToPixels (r.reduced (half));
}

/** Snaps corner radius to nearest 0.5 for crisp edges at non-integer scale. */
inline float snapRadius (float radius) noexcept
{
    return std::round (radius * 2.0f) * 0.5f;
}

} // namespace Layout
} // namespace AnalyzerPro
