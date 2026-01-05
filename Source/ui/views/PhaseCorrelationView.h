#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

//==============================================================================
/**
    Phase/Correlation polar scope view component.
    PAZ-inspired design with polar grid, crosshair, and correlation meter.
*/
class PhaseCorrelationView : public juce::Component
{
public:
    struct Sample
    {
        float x;  // normalized [-1..+1]
        float y;  // normalized [-1..+1]
    };

    PhaseCorrelationView();
    ~PhaseCorrelationView() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    /** Set correlation value (-1..+1). Default: 0. */
    void setCorrelation (float correlation);

    /** Set sample points for polar display. Optional, can be empty. */
    void setPoints (const Sample* pts, int numPts);

    /** Get current correlation value. */
    float getCorrelation() const noexcept { return correlation_; }

private:
    static constexpr int kMaxPoints = 512;
    std::array<Sample, kMaxPoints> points_;
    int numPoints_ = 0;
    float correlation_ = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhaseCorrelationView)
};
