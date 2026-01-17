#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include <mdsp_ui/UiContext.h>
#include "TooltipData.h"
#include "TooltipOverlayComponent.h"

namespace mdsp_ui
{

class TooltipManager : public juce::MouseListener,
                       public juce::Timer
{
public:
    TooltipManager (juce::Component& editor, UiContext& ui);
    ~TooltipManager() override;

    // Call this to register a component for tooltip
    void registerTooltip (juce::Component* component, TooltipSpec spec);
    void unregisterTooltip (juce::Component* component);

    // MouseListener callbacks
    void mouseEnter (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override;

    // Timer callback
    void timerCallback() override;

private:
    void hideTooltip();
    void showTooltip();
    void updateTooltipPosition();

    juce::Component& editor_;
    std::unique_ptr<TooltipOverlayComponent> overlay_;

    // Registry
    std::map<juce::Component*, TooltipSpec> registry_;

    // State
    juce::Component* currentTarget_ = nullptr;
    bool isVisible_ = false;
    
    // Timer usage:
    // If !isVisible_: Timer creates initial delay (250ms).
    // If isVisible_: Timer updates value (10Hz).
};

} // namespace mdsp_ui
