#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>
#include <mdsp_ui/meters/MeterRenderState.h>

namespace AnalyzerPro
{
class TraceColorStore;
}

class MeterComponent : public juce::Component
{
public:
    using MeterRenderState = mdsp_ui::meters::MeterRenderState;
    using Callback = void (*) (void*) noexcept;

    MeterComponent (mdsp_ui::UiContext& ui, juce::String labelText);
    ~MeterComponent() override = default;

    void setLabelText (juce::String labelText);
    void setRenderState (const MeterRenderState& state);
    void setTraceColorStore (AnalyzerPro::TraceColorStore* store) noexcept { traceColors_ = store; }
    void setClipResetCallback (Callback cb, void* ctx) noexcept;
    void setPeakResetCallback (Callback cb, void* ctx) noexcept;

    void mouseDown (const juce::MouseEvent&) override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static float dbToNormForScale (float db, mdsp_ui::meters::MeterScaleMode mode) noexcept;

    mdsp_ui::UiContext& ui_;
    MeterRenderState renderState_ {};
    AnalyzerPro::TraceColorStore* traceColors_ = nullptr;

    juce::String label_;
    juce::String numericTextPeak_ { "-inf" };
    juce::String numericTextRms_ { "-inf" };
    Callback onClipReset_ = nullptr;
    Callback onPeakReset_ = nullptr;
    void* onClipResetCtx_ = nullptr;
    void* onPeakResetCtx_ = nullptr;

    juce::Rectangle<int> labelArea_;
    juce::Rectangle<int> ledArea_;
    juce::Rectangle<int> meterArea_;
    juce::Rectangle<int> numericArea_;
};
