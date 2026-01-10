#pragma once

#include "IHardwareMeterSink.h"
#include <atomic>

// Reference "software sink" implementation: stores the latest mapped values.
// No I/O, no allocations; safe to publish from the audio thread.
class SoftwareMeterSink final : public IHardwareMeterSink
{
public:
    struct AtomicChannel
    {
        std::atomic<float> rms01 { 0.0f };
        std::atomic<float> peak01 { 0.0f };
        std::atomic<int> litSegmentsRms { 0 };
        std::atomic<int> litSegmentsPeak { 0 };
        std::atomic<int> peakHoldSegmentIndex { -1 };
        std::atomic<bool> clipLatched { false };
    };

    struct AtomicSide
    {
        std::atomic<int> channelCount { 2 };
        AtomicChannel ch[2];
    };

    struct AtomicFrame
    {
        AtomicSide input;
        AtomicSide output;
    };

    void publishMeterLevels (const HardwareMeterLevelsFrame& frame) noexcept override
    {
        storeSide (frame.input, state_.input);
        storeSide (frame.output, state_.output);
    }

    const AtomicFrame& state() const noexcept { return state_; }

private:
    static inline void storeChannel (const HardwareMeterChannelLevels& in, AtomicChannel& out) noexcept
    {
        out.rms01.store (in.rms01, std::memory_order_relaxed);
        out.peak01.store (in.peak01, std::memory_order_relaxed);
        out.litSegmentsRms.store (in.litSegmentsRms, std::memory_order_relaxed);
        out.litSegmentsPeak.store (in.litSegmentsPeak, std::memory_order_relaxed);
        out.peakHoldSegmentIndex.store (in.peakHoldSegmentIndex, std::memory_order_relaxed);
        out.clipLatched.store (in.clipLatched, std::memory_order_relaxed);
    }

    static inline void storeSide (const HardwareMeterSideLevels& in, AtomicSide& out) noexcept
    {
        out.channelCount.store (in.channelCount, std::memory_order_relaxed);
        storeChannel (in.ch[0], out.ch[0]);
        storeChannel (in.ch[1], out.ch[1]);
    }

    AtomicFrame state_;
};

