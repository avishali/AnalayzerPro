#include "CollapsibleArrowComponent.h"
#include <mdsp_ui/IconCache.h>
#include <mdsp_ui/IconIds.generated.h>

//==============================================================================
CollapsibleArrowComponent::CollapsibleArrowComponent (mdsp_ui::UiContext& ui)
    : ui_ (ui)
{
    setInterceptsMouseClicks (false, false);
}

void CollapsibleArrowComponent::setExpanded (bool expanded) noexcept
{
    targetAngle_ = expanded ? juce::MathConstants<float>::halfPi : 0.0f;
    if (std::abs (arrowAngle_ - targetAngle_) < kEpsilon)
        return;
    startTimerHz (60);
}

void CollapsibleArrowComponent::timerCallback()
{
    arrowAngle_ += (targetAngle_ - arrowAngle_) * kLerpFactor;
    if (std::abs (targetAngle_ - arrowAngle_) < kEpsilon)
    {
        arrowAngle_ = targetAngle_;
        stopTimer();
    }
    repaint();
}

void CollapsibleArrowComponent::paint (juce::Graphics& g)
{
    const auto& theme = ui_.theme();
    const int w = getWidth();
    const int h = getHeight();
    const float cx = w * 0.5f;
    const float cy = h * 0.5f;
    const int size = juce::jmin (juce::jmin (w, h) - 4, 14);

    // Get SVG icon from cache instead of manual triangle drawing
    const juce::Drawable* iconDrawable = ui_.icons().get (mdsp_ui::IconId::chevron_down);
    if (iconDrawable != nullptr)
    {
        // Create a tinted copy for this component
        auto tintedIcon = ui_.icons().makeTinted (mdsp_ui::IconId::chevron_down, theme.lightGrey);
        if (tintedIcon != nullptr)
        {
            // Calculate bounds centered in component
            juce::Rectangle<float> iconBounds (cx - size * 0.5f, cy - size * 0.5f, size, size);
            
            // Apply rotation transform
            g.addTransform (juce::AffineTransform::translation (cx, cy)
                                              .rotated (arrowAngle_)
                                              .translated (-cx, -cy));
            
            tintedIcon->drawWithin (g, iconBounds, juce::RectanglePlacement::centred, 1.0f);
        }
    }
}
