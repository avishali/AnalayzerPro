#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>
#include "../analyzer/metal/MetalHostShared.h"
#include "DevLookPanelConfig.h"

#if ANALYZERPRO_DEV_LOOK_PANEL

namespace AnalyzerPro
{

class DevLookControlsComponent : public juce::Component
{
public:
    DevLookControlsComponent (metal::MetalLookTunables& tunables, std::function<void()> onChanged);
    ~DevLookControlsComponent() override = default;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    struct SliderRow
    {
        juce::Label label;
        juce::Slider slider;
    };

    void addRow (juce::Component& parent,
                 const juce::String& text,
                 double minValue,
                 double maxValue,
                 double step,
                 int decimals,
                 std::function<float()> getter,
                 std::function<void (float)> setter);

    static juce::String formatTunablesForClipboard (const metal::MetalLookTunables& t);

    metal::MetalLookTunables& tunables_;
    std::function<void()> onChanged_;
    juce::Viewport viewport_;
    juce::Component scrollContent_;
    juce::TextButton copyButton_ { "Copy values" };
    std::vector<std::unique_ptr<SliderRow>> rows_;
    int contentHeight_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DevLookControlsComponent)
};

} // namespace AnalyzerPro

#endif
