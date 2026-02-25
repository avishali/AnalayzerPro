#pragma once

#include <mdsp_ui/scopes/PhaseFanRenderState.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>

class PhaseFanScopeComponent : public juce::Component
{
public:
    explicit PhaseFanScopeComponent (mdsp_ui::UiContext& ui);
    ~PhaseFanScopeComponent() override = default;

    void setRenderState (const mdsp_ui::scopes::PhaseFanRenderState& state);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void rebuildCachedPaths();

    mdsp_ui::UiContext& ui_;
    mdsp_ui::scopes::PhaseFanRenderState state_ {};

    juce::Path fanFillPath_;
    juce::Path contourPath_;
    juce::Path peakHoldPath_;
    juce::Path arcPath_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhaseFanScopeComponent)
};
