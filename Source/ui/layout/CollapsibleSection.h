#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>
#include <functional>
#include "CollapsibleArrowComponent.h"

//==============================================================================
/**
    Reusable collapsible section header: title + chevron.
    Manages expanded state; parent lays out content and uses getPreferredHeight().
    Call onToggle when expanded state changes so parent can resized() and update height.
*/
class CollapsibleSection : public juce::Component
{
public:
    explicit CollapsibleSection (mdsp_ui::UiContext& ui, const juce::String& title);
    ~CollapsibleSection() override = default;

    void attachToParent (juce::Component& parent);
    /** Sets this component's bounds to the row at y and advances y by row height. */
    void layout (juce::Rectangle<int> bounds, int& y);

    bool isExpanded() const noexcept { return expanded_; }
    void setExpanded (bool expanded) noexcept;

    /** Called when user toggles; parent should call resized() and update viewport content size. */
    std::function<void()> onToggle;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    static constexpr int kArrowWidth = 18;
    static constexpr int kTitleArrowGap = 6;
    mdsp_ui::UiContext& ui_;
    CollapsibleArrowComponent arrowComp_;
    juce::Label titleLabel_;
    bool expanded_ = false;

    int getRowHeight() const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CollapsibleSection)
};
