#include "CollapsibleArrowComponent.h"

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
    const float s2 = size * 0.5f;

    // Base shape: right-pointing triangle (▶) centered at origin
    juce::Path p;
    p.addTriangle (-s2, -s2, -s2, s2, s2, 0.0f);

    g.setColour (theme.lightGrey);
    g.addTransform (juce::AffineTransform::translation (cx, cy).rotated (arrowAngle_));
    g.fillPath (p);
}
