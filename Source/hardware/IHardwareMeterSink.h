#pragma once

#include <cstdint>

// Hardware-facing meter payloads (UI-independent, stable contract).
struct HardwareMeterChannelLevels
{
    // Continuous linear levels (0..1) derived from dBFS in [-60, 0].
    float rms01 = 0.0f;
    float peak01 = 0.0f;

    // Discrete segment mapping.
    int litSegmentsRms = 0;
    int litSegmentsPeak = 0;

    // Optional: peak hold segment index (0..numSegments-1), or -1 if disabled.
    int peakHoldSegmentIndex = -1;

    bool clipLatched = false;
};

struct HardwareMeterSideLevels
{
    int channelCount = 2; // 1 (mono) or 2 (stereo)
    HardwareMeterChannelLevels ch[2];
};

struct HardwareMeterLevelsFrame
{
    HardwareMeterSideLevels input;
    HardwareMeterSideLevels output;
};

class IHardwareMeterSink
{
public:
    virtual ~IHardwareMeterSink() = default;

    // Called from the audio thread (must be RT-safe).
    virtual void publishMeterLevels (const HardwareMeterLevelsFrame& frame) noexcept = 0;
};

