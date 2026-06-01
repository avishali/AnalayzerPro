#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <mdsp_ui/UiContext.h>

namespace AnalyzerPro
{

/**
    Draggable value label for parameter control.
    Displays formatted value; vertical drag or mouse wheel adjusts value.
    Uses parameter gestures for host automation.
*/
class DraggableParamValueLabel : public juce::Component,
                                 private juce::Timer
{
public:
    explicit DraggableParamValueLabel (mdsp_ui::UiContext& ui);
    ~DraggableParamValueLabel() override;

    void setParameter (juce::RangedAudioParameter* param);

    /** Manual (non-APVTS) mode: drag/wheel a plain value in [min,max]; reports via onManualChanged. */
    void setManualValue (double value, double min, double max);
    std::function<void (double)> onManualChanged;

    void paint (juce::Graphics& g) override;
    void timerCallback() override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

private:
    juce::String formatValue (double value) const;
    double getSensitivity (bool fine, bool coarse) const;

    mdsp_ui::UiContext& ui_;
    juce::RangedAudioParameter* param_ = nullptr;
    bool hover_ = false;
    bool dragging_ = false;
    double startValue_ = 0.0;
    int startY_ = 0;
    juce::String suffix_ = " ms";

    // Manual mode (no APVTS param)
    bool   manual_ = false;
    double manualValue_ = 0.0;
    double manualMin_ = 0.0;
    double manualMax_ = 1.0;

    static constexpr int kPixelsForFullRange = 800;
    static constexpr double kFineMultiplier = 0.1;
    static constexpr double kCoarseMultiplier = 2.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DraggableParamValueLabel)
};

} // namespace AnalyzerPro
