#include "RTACurveHelpers.h"
#include <juce_core/juce_core.h>
#include <cmath>

namespace mdsp_ui::rta
{

float getAWeightingDb (float freqHz)
{
    // IEC 61672-1:2002 standard A-weighting
    const float f2 = freqHz * freqHz;
    const float f4 = f2 * f2;
    const float c1 = 12194.0f * 12194.0f;
    const float c2 = 20.6f * 20.6f;
    const float c3 = 107.7f * 107.7f;
    const float c4 = 737.9f * 737.9f;
    const float c5 = 12194.0f * 12194.0f;
    
    const float num = c1 * f4;
    const float den = (f2 + c2) * std::sqrt((f2 + c3) * (f2 + c4)) * (f2 + c5);
    
    if (den == 0.0f) return -120.0f;
    
    float gain = num / den;
    return 20.0f * std::log10(gain) + 2.0f; 
}

float computeTiltDb (float freqHz, ::rta::TiltMode tiltMode)
{
    // For DC (i==0) or very low frequencies, no tilt
    if (freqHz <= 0.0f)
        return 0.0f;
    
    // Clamp frequency to minimum before log2
    const float clampedFreq = juce::jmax (1.0f, freqHz);
    
    // Reference frequency: 1000 Hz
    constexpr float f0 = 1000.0f;
    
    // Compute octaves from reference
    const float octaves = std::log2 (clampedFreq / f0);
    
    // Apply tilt based on mode
    float slopeDbPerOct = 0.0f;
    switch (tiltMode)
    {
        case ::rta::TiltMode::Flat:
            slopeDbPerOct = 0.0f;
            break;
        case ::rta::TiltMode::Pink:
            // Pink noise has -3 dB/oct slope, so apply +3 dB/oct compensation to flatten it
            slopeDbPerOct = +3.0f;
            break;
        case ::rta::TiltMode::White:
            // White noise is flat in power/Hz, but apply -3 dB/oct for perceptual compensation
            slopeDbPerOct = -3.0f;
            break;
    }
    
    return slopeDbPerOct * octaves;
}

} // namespace mdsp_ui::rta
