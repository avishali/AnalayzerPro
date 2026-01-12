#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>
#include "../../analyzer/StereoScopeAnalyzer.h"

//==============================================================================
/**
    StereoScopeView
    Visualizes stereo correlation using a vectorscope plot (Mid/Side mapping).
    Includes persistence decay.
*/
class StereoScopeView : public juce::Component,
                        private juce::Timer
{
public:
    StereoScopeView (mdsp_ui::UiContext& ui, StereoScopeAnalyzer& analyzer);
    ~StereoScopeView() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void renderScopeToImage();

    mdsp_ui::UiContext& ui_;
    StereoScopeAnalyzer& analyzer_;

    // Data buffers
    std::vector<float> lBuffer_;
    std::vector<float> rBuffer_;
    
    // Visualization
    juce::Image displayImage_;
    juce::Image accumImage_;
    
    // Config
    float decayFactor_ = 0.85f;
    float scale_ = 0.8f; // Visual scaling
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StereoScopeView)
};
