#pragma once

#include <atomic>
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

struct MetalRectPx
{
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    bool isEmpty() const noexcept { return w <= 1.0f || h <= 1.0f; }
};

struct MetalAnalyzerFrame
{
    MetalRectPx plotRectPx;
    MetalColour rmsColour;
    float minHz = 10.0f;
    float maxHz = 20000.0f;
    float topDb = 6.0f;
    float bottomDb = -90.0f;
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
