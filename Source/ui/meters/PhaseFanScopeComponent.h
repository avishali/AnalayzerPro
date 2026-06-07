#pragma once

#include <mdsp_ui/scopes/PhaseFanRenderState.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>

namespace AnalyzerPro
{
class TraceColorStore;
}

class PhaseFanScopeComponent : public juce::Component
{
public:
    explicit PhaseFanScopeComponent (mdsp_ui::UiContext& ui);
    ~PhaseFanScopeComponent() override = default;

    void setRenderState (const mdsp_ui::scopes::PhaseFanRenderState& state);
    void setTraceColorStore (AnalyzerPro::TraceColorStore* store) noexcept { traceColors_ = store; }
#if defined(ANALYZERPRO_METAL_EDITOR) && ANALYZERPRO_METAL_EDITOR
    void setMetalTraceSuppressedForChromeCapture (bool shouldSuppress) noexcept;
#endif

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void rebuildCachedPaths();

    mdsp_ui::UiContext& ui_;
    mdsp_ui::scopes::PhaseFanRenderState state_ {};
    AnalyzerPro::TraceColorStore* traceColors_ = nullptr;

    juce::Path fanFillPath_;
    juce::Path contourPath_;
    juce::Path peakHoldPath_;
    juce::Path arcPath_;
#if defined(ANALYZERPRO_METAL_EDITOR) && ANALYZERPRO_METAL_EDITOR
    bool metalTraceSuppressed_ = false;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhaseFanScopeComponent)
};
