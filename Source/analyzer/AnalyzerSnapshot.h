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
    
    // Multi-trace FFT data (dB values)
    // Each trace has its own spectrum for independent rendering
    std::array<float, kMaxFFTBins> fftDbL{};      // Left channel
    std::array<float, kMaxFFTBins> fftDbR{};      // Right channel
    std::array<float, kMaxFFTBins> fftDbMono{};   // Mono (L+R)/2
    std::array<float, kMaxFFTBins> fftDbMid{};    // Mid (L+R)/2 (same as Mono, different label)
    std::array<float, kMaxFFTBins> fftDbSide{};   // Side (L-R)/2

    // Engine-side RMS-processed multi-trace arrays (with audio-thread ballistics)
    std::array<float, kMaxFFTBins> fftDbLRms{};    // L channel with RMS ballistics
    std::array<float, kMaxFFTBins> fftDbRRms{};    // R channel with RMS ballistics
    std::array<float, kMaxFFTBins> fftDbMidRms{};  // Mid with RMS ballistics
    std::array<float, kMaxFFTBins> fftDbSideRms{}; // Side with RMS ballistics
    std::array<float, kMaxFFTBins> fftDbMonoRms{}; // Mono with RMS ballistics

    // Peak hold versions for each trace
    std::array<float, kMaxFFTBins> fftPeakDbL{};
    std::array<float, kMaxFFTBins> fftPeakDbR{};
    std::array<float, kMaxFFTBins> fftPeakDbMono{};
    std::array<float, kMaxFFTBins> fftPeakDbMid{};
    std::array<float, kMaxFFTBins> fftPeakDbSide{};
    
    // Peak Hold versions (Maximum envelope, slow/no decay)
    std::array<float, kMaxFFTBins> fftPeakHoldDbL{};
    std::array<float, kMaxFFTBins> fftPeakHoldDbR{};
    std::array<float, kMaxFFTBins> fftPeakHoldDbMono{};
    std::array<float, kMaxFFTBins> fftPeakHoldDbMid{};
    std::array<float, kMaxFFTBins> fftPeakHoldDbSide{};
    
    // Legacy/Main Peak Hold (corresponding to fftDb)
    std::array<float, kMaxFFTBins> fftPeakHoldDb{};
    
    // Power domain arrays for UI-side derivation of Mono/Mid/Side
    // These are in linear power (NOT dB), enabling proper spectral math
    std::array<float, kMaxFFTBins> powerL{};
    std::array<float, kMaxFFTBins> powerR{};
    
    // Legacy single-spectrum arrays (kept for backward compatibility, will be populated with Mono)
    std::array<float, kMaxFFTBins> fftDb{};
    std::array<float, kMaxFFTBins> fftPeakDb{};

    // FFT: authoritative bin count for all spectra
    // Contract: fftBinCount == (fftSize / 2 + 1).
    int fftBinCount = 0;

    // Legacy/compat only:
    // Historically this field was also used as the FFT bin count. Going forward, FFT uses fftBinCount
    // exclusively. numBins is reserved for non-FFT series (Bands/Log) if/when those are ever stored
    // in the snapshot directly.
    int numBins = 0;
    
    // Metadata
    double sampleRate = 48000.0;
    int fftSize = 2048;
    float displayBottomDb = -90.0f;
    float displayTopDb = 0.0f;
    // Validity flag (set to true after first valid FFT)
    bool isValid = false;
    
    // Debug / Status
    bool isHoldOn = false;
    bool multiTraceEnabled = false;
    int weightingMode = 0; // 0=None, 1=A, 2=BS.468 (applied engine-side to power before smoothing)
    /** True if Engine applied spectral smoothing. UI must bypass its own smoothing when true. */
    bool engineDidSpectralSmooth = false;
    /** True when Option A (UISmoothingLogGaussian): smoothing only in UI convertFFTToLog. SmoothingProcessor must bypass. */
    bool useUILogGaussianOnly = true;
    /** Smoothing octaves (0=Off, 1/24, 1/12, 1/6, 1/3, 1). Used by UI for single-stage enforcement. */
    float smoothingOctaves = 0.0f;
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
