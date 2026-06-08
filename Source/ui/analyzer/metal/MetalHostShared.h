#pragma once

#include <atomic>
#include <array>
#include <cstdint>
#include <vector>

namespace AnalyzerPro::metal
{

enum class MetalHostMechanism
{
    BackingLayer = 0,
    CoverView = 1
};

inline std::atomic<float> gMetalHostFps { 0.0f };
inline std::atomic<float> gMetalHostEncodeMs { 0.0f };
inline std::atomic<float> gMetalChromeCaptureMs { 0.0f };
inline std::atomic<float> gMetalChromeCaptureIntervalMs { 0.0f };
inline std::atomic<int> gMetalHostInputHits { 0 };
inline std::atomic<int> gMetalHostMechanism { static_cast<int> (MetalHostMechanism::BackingLayer) };
inline std::atomic<int> gMetalHostLiveRenderThreads { 0 };
inline std::atomic<uint64_t> gMetalHostRenderedFrames { 0 };

struct FrameTexturePayload
{
    std::vector<uint8_t> bgraPixels;
    int widthPx = 0;
    int heightPx = 0;
    int bytesPerRow = 0;
    float scale = 1.0f;
    uint64_t sequence = 0;
};

struct MetalColour
{
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

struct MetalTracePayload
{
    std::vector<float> db;
    MetalColour colour;
    bool visible = false;
    bool strokeVisible = true;
    bool fillToBottom = false;
    float fillTopAlpha = 0.0f;
    float fillBottomAlpha = 0.0f;
};

struct MetalRectPx
{
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    bool isEmpty() const noexcept { return w <= 1.0f || h <= 1.0f; }
};

struct MetalMeterBar
{
    MetalRectPx rectPx;
    float mainNorm = 0.0f;
    float peakNorm = 0.0f;
    float maxPeakNorm = 0.0f;
    MetalColour mainColour;
    MetalColour peakColour;
    MetalColour holdColour;
    int displayMode = 0;
    bool valid = false;
};

struct MetalPoint
{
    float x = 0.0f;
    float y = 0.0f;
};

struct MetalAnalyzerFrame
{
    MetalRectPx plotRectPx;
    static constexpr size_t kMaxMeters = 4;
    std::array<MetalMeterBar, kMaxMeters> meters {};
    int meterCount = 0;
    static constexpr size_t kPhaseFanAngleBins = 180;
    MetalRectPx phaseFanRectPx;
    MetalColour phaseFanColour;
    float phaseFanCx = 0.0f;
    float phaseFanCy = 0.0f;
    float phaseFanRadiusPx = 0.0f;
    std::array<float, kPhaseFanAngleBins> phaseFanContourRNorm {};
    std::array<float, kPhaseFanAngleBins> phaseFanPeakRNorm {};
    bool phaseFanPeakHoldEnabled = false;
    int phaseFanRenderMode = 0;
    bool phaseFanValid = false;
    static constexpr size_t kGonioMaxPoints = 2048;
    static constexpr size_t kGonioHistoryFrames = 24;
    static constexpr size_t kGonioPointsPerHistoryFrame = 256;
    static constexpr size_t kGonioHistoryPointCapacity = kGonioHistoryFrames * kGonioPointsPerHistoryFrame;
    static constexpr size_t kGonioHoldBins = 180;
    MetalRectPx gonioRectPx;
    float gonioCx = 0.0f;
    float gonioCy = 0.0f;
    float gonioHalfUsable = 0.0f;
    float gonioPointHalfSizePx = 0.75f;
    MetalColour gonioColour;
    int gonioNumPoints = 0;
    std::array<MetalPoint, kGonioMaxPoints> gonioPoints {};
    int gonioActiveHistoryFrames = 0;
    int gonioPointsPerHistoryFrame = 0;
    int gonioNewestHistoryFrame = -1;
    std::array<MetalPoint, kGonioHistoryPointCapacity> gonioHistoryPoints {};
    bool gonioHoldEnabled = false;
    int gonioHoldCount = 0;
    std::array<MetalPoint, kGonioHoldBins> gonioHoldPoints {};
    bool gonioValid = false;
    MetalColour rmsColour;
    MetalTracePayload rmsTrace;
    MetalTracePayload peakTrace;
    MetalTracePayload peakHoldTrace;
    MetalTracePayload stereoTrace;
    MetalTracePayload monoTrace;
    MetalTracePayload leftTrace;
    MetalTracePayload rightTrace;
    MetalTracePayload midTrace;
    MetalTracePayload sideTrace;
    float minHz = 10.0f;
    float maxHz = 20000.0f;
    float topDb = 6.0f;
    float bottomDb = -90.0f;
    float displayGainDb = 0.0f;
    float rmsAttackMs = 60.0f;
    float rmsReleaseMs = 300.0f;
    double sampleRate = 48000.0;
    int fftSize = 2048;
    int weightingMode = 0;
    int tiltMode = 0;
    uint64_t sequence = 0;
    bool valid = false;
};

inline const char* getMetalHostMechanismName (MetalHostMechanism mechanism) noexcept
{
    switch (mechanism)
    {
        case MetalHostMechanism::BackingLayer: return "backing";
        case MetalHostMechanism::CoverView:    return "cover";
    }

    return "unknown";
}

inline const char* getCurrentMetalHostMechanismName() noexcept
{
    return getMetalHostMechanismName (static_cast<MetalHostMechanism> (gMetalHostMechanism.load (std::memory_order_relaxed)));
}

} // namespace AnalyzerPro::metal
