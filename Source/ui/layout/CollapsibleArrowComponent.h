#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>

//==============================================================================
/**
    Animated arrow for collapsible section headers.
    Right (0°) when collapsed, down (+90°) when expanded.
    ~120 ms rotation animation at 60 Hz with simple easing.
*/
class CollapsibleArrowComponent : public juce::Component,
                                  public juce::Timer
{
public:
    explicit CollapsibleArrowComponent (mdsp_ui::UiContext& ui);
    ~CollapsibleArrowComponent() override = default;

    /** Sets target rotation and starts animation if needed. */
    void setExpanded (bool expanded) noexcept;

    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override;

    mdsp_ui::UiContext& ui_;
    float arrowAngle_ = 0.0f;   // radians, current
    float targetAngle_ = 0.0f;  // radians

    static constexpr float kEpsilon = 0.002f;
    static constexpr float kLerpFactor = 0.25f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CollapsibleArrowComponent)
};
