#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>
#include "TooltipData.h"

namespace mdsp_ui
{

class TooltipOverlayComponent : public juce::Component
{
public:
    explicit TooltipOverlayComponent (UiContext& ui);
    ~TooltipOverlayComponent() override = default;

    void updateSpec (const TooltipSpec& spec);
    void updateValue(); // Polls value provider and refreshes layout if needed

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void refreshLayout();

    UiContext& ui_;
    TooltipSpec currentSpec_;
    
    // Cached Layouts to avoid allocations in paint
    juce::TextLayout titleLayout_;
    juce::TextLayout valueLayout_;
    juce::TextLayout descLayout_;

    juce::String lastValueString_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TooltipOverlayComponent)
};

} // namespace mdsp_ui
