#pragma once

#include <juce_core/juce_core.h>
#include <cmath>
#include "IHardwareMeterSink.h"

class HardwareMeterMapper
{
public:
    struct Config
    {
        int numSegments;
        bool enablePeakHoldSegment;
    };

    HardwareMeterMapper() : cfg_ (Config { 16, false }) {}
    explicit HardwareMeterMapper (Config cfg) : cfg_ (cfg) {}

    void setConfig (Config cfg) noexcept { cfg_ = cfg; }
    Config getConfig() const noexcept { return cfg_; }

    // Pure mapping: no allocations, safe to call on audio thread.
    static inline float dbToLevel01 (float db) noexcept
    {
        // Input expected to be dBFS. Mapping range is [-60, 0].
        if (! std::isfinite (db) || db <= -60.0f)
            return 0.0f;

        const float clamped = juce::jlimit (-60.0f, 0.0f, db);
        const float level01 = (clamped + 60.0f) / 60.0f;
        return juce::jlimit (0.0f, 1.0f, level01);
    }

    static inline int level01ToLitSegments (float level01, int numSegments) noexcept
    {
        const int n = juce::jmax (1, numSegments);
        const float v = juce::jlimit (0.0f, 1.0f, level01);
        const int lit = static_cast<int> (std::lround (v * static_cast<float> (n)));
        return juce::jlimit (0, n, lit);
    }

    static inline int level01ToSegmentIndex (float level01, int numSegments) noexcept
    {
        const int n = juce::jmax (1, numSegments);
        const float v = juce::jlimit (0.0f, 1.0f, level01);
        const int idx = static_cast<int> (std::lround (v * static_cast<float> (n - 1)));
        return juce::jlimit (0, n - 1, idx);
    }

    inline HardwareMeterChannelLevels mapChannel (float rmsDb, float peakDb, bool clipLatched) const noexcept
    {
        HardwareMeterChannelLevels out;
        out.rms01 = dbToLevel01 (rmsDb);
        out.peak01 = dbToLevel01 (peakDb);

        out.litSegmentsRms = level01ToLitSegments (out.rms01, cfg_.numSegments);
        out.litSegmentsPeak = level01ToLitSegments (out.peak01, cfg_.numSegments);

        out.peakHoldSegmentIndex = cfg_.enablePeakHoldSegment
            ? level01ToSegmentIndex (out.peak01, cfg_.numSegments)
            : -1;

        out.clipLatched = clipLatched;
        return out;
    }

private:
    Config cfg_;
};

