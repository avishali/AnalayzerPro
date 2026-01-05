#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

//==============================================================================
/**
    Snapshot of analyzer data for transport from audio thread to UI thread.
    Pure data structure (no atomics) - trivially copyable.
*/
struct AnalyzerSnapshot
{
    static constexpr int kMaxFFTSize = 8192;
    static constexpr int kMaxFFTBins = 8192;  // Headroom for 8192 FFT (4097 bins) + future expansion
    
    // FFT data (dB values)
    // fftDb contains dB values already (converted in AnalyzerEngine::computeFFT).
    // UI clamps/sanitizes only; no conversion needed.
    std::array<float, kMaxFFTBins> fftDb{};
    std::array<float, kMaxFFTBins> fftPeakDb{};
    int numBins = 0;
    
    // Metadata
    double sampleRate = 48000.0;
    int fftSize = 2048;
    float displayBottomDb = -90.0f;
    float displayTopDb = 0.0f;
    // Validity flag (set to true after first valid FFT)
    bool isValid = false;
};

//==============================================================================
/**
    Published snapshot wrapper with atomic sequence counter for lock-free transport.
    Used internally by AnalyzerEngine for thread-safe snapshot publishing.
*/
struct PublishedAnalyzerSnapshot
{
    std::atomic<uint32_t> sequence{0};
    AnalyzerSnapshot data;
};
