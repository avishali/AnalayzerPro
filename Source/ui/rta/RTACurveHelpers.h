#pragma once

#include "RTAEnums.h"

namespace mdsp_ui::rta
{

/**
    A-weighting filter response in dB.
    IEC 61672-1:2002 standard.
*/
float getAWeightingDb (float freqHz);

/**
    Compute tilt compensation in dB for a given frequency and tilt mode.
    @param freqHz Frequency in Hz
    @param tiltMode Tilt mode (Flat, Pink, or White)
    @return Tilt compensation in dB
*/
float computeTiltDb (float freqHz, ::rta::TiltMode tiltMode);

} // namespace mdsp_ui::rta
