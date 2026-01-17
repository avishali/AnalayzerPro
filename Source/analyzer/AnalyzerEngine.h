#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "AnalyzerSnapshot.h"
#include <array>
#include <atomic>
#include <memory>

//==============================================================================
/**
    Real-time FFT analyzer engine.
    Runs on audio thread, produces snapshots for UI consumption.
*/
#include "StereoScopeAnalyzer.h"

class AnalyzerEngine
{
public:
    AnalyzerEngine();
    ~AnalyzerEngine();
    
    /** Prepare analyzer with sample rate and block size */
    void prepare (double sampleRate, int samplesPerBlock);

    StereoScopeAnalyzer& getStereoScopeAnalyzer() noexcept { return stereoScopeAnalyzer; }
    const StereoScopeAnalyzer& getStereoScopeAnalyzer() const noexcept { return stereoScopeAnalyzer; }
    
    /** Release resources */
    void reset();
    
    /** Process audio block and update FFT if ready */
    void processBlock (const juce::AudioBuffer<float>& buffer);
    
    /** Publish a new snapshot (audio thread only, after computing FFT) */
    void publishSnapshot (const AnalyzerSnapshot& source);
    
    /** Get latest snapshot (non-blocking, UI thread only) */
    bool getLatestSnapshot (AnalyzerSnapshot& dest) const;
    
    /** Update parameters from APVTS (call from UI thread or parameter change callback) */
    void setFftSize (int fftSize);

    // RT-safe: request an FFT size change (no allocations here)
    void requestFftSize (int fftSize);

    // Called on a non-audio thread (message thread) to apply pending resize
    void applyPendingFftSizeIfNeeded();

    void setAveragingMs (float averagingMs);
    void setSmoothingOctaves (float octaves);

    void resetPeaks();

    enum class PeakHoldMode
    {
        Off = 0,
        Infinite,
        Decay,
        HoldThenDecay
    };

    void setPeakHoldMode (PeakHoldMode mode);
    void setPeakHoldTimeMs (float holdTimeMs);
    void setHold (bool hold);  // Freeze peaks (no decay when enabled)
    void setPeakDecayDbPerSec (float decayDbPerSec);

    enum class PeakDecayCurve
    {
        DbPerSec = 0,
        TimeConstant60dB = 1
    };

    void setPeakDecayCurve (PeakDecayCurve curve);
    void setPeakDecayTimeConstantSec (float seconds);
    
private:
    static constexpr int kMaxFFTSize = 8192;
    static constexpr float kDbFloor = -120.0f;
    
    int currentFFTSize = 2048;
    int currentHopSize = 512;
    
    // FFT (dynamically sized, max kMaxFFTSize)
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> fftOutput;  // In-place buffer: input (first fftSize) + complex output (2*fftSize)
    std::vector<float> window;
    std::vector<float> fifoBuffer;
    int fifoWritePos = 0;
    int samplesCollected = 0;
    
    // Published snapshot for lock-free transport (audio thread writes, UI thread reads)
    PublishedAnalyzerSnapshot published_;
    
    // State
    double currentSampleRate = 44100.0;
    bool prepared = false;
    
    
    // Smoothing buffers (Power domain) - Legacy single-channel
    std::vector<float> smoothedMagnitude; // RMS State
    std::vector<float> smoothedPeak;      // Peak State

    // Multi-trace complex bin storage (for L/R channels)
    std::vector<float> fftOutputL;  // Complex FFT output for Left channel
    std::vector<float> fftOutputR;  // Complex FFT output for Right channel
    
    // Multi-trace power spectrum storage (derived from complex bins)
    std::vector<float> powerL_;      // Power spectrum for Left
    std::vector<float> powerR_;      // Power spectrum for Right
    std::vector<float> powerMono_;   // Power spectrum for Mono (L+R)/2
    std::vector<float> powerMid_;    // Power spectrum for Mid (L+R)/2
    std::vector<float> powerSide_;   // Power spectrum for Side (L-R)/2
    
    // Multi-trace smoothed power (RMS ballistics)
    std::vector<float> smoothedL_;
    std::vector<float> smoothedR_;
    std::vector<float> smoothedMono_;
    std::vector<float> smoothedMid_;
    std::vector<float> smoothedSide_;
    
    // Multi-trace peak hold
    std::vector<float> peakL_;
    std::vector<float> peakR_;
    std::vector<float> peakMono_;
    std::vector<float> peakMid_;
    std::vector<float> peakSide_;

    std::vector<int> smoothLowBounds;
    std::vector<int> smoothHighBounds;
    std::vector<float> prefixSumMag; 
    std::vector<float> peakHold;

    // Per-frame computation buffers (resized in initializeFFT, reused to avoid allocations)
    std::vector<float> magnitudes_;
    std::vector<float> dbValues_;
    std::vector<float> dbRaw_;  // Ballistic (smoothed) Peak dB
    std::vector<float> dbInstant_; // Instantaneous (raw) Peak dB

    // Ballistics Parameters (ms)
    float rmsAttackMs_ = 80.0f;
    float rmsReleaseMs_ = 250.0f;
    float peakAttackMs_ = 10.0f;
    float peakReleaseMs_ = 80.0f;
    float smoothingOctaves_ = 1.0f; // 0 = Off
    float peakDecayDbPerSec = 1.0f;
    bool peakHoldEnabled_ = true;  // Always enabled now (toggled by Hold logic)
    std::atomic<bool> freezePeaks_{ false };      // Atomic for thread safety

    PeakDecayCurve peakDecayCurve_ = PeakDecayCurve::DbPerSec;
    float peakDecayTimeConstantSec_ = 1.0f;

    // Pending FFT resize request (RT-safe)
    std::atomic<int> pendingFftSize_{ 0 };
    std::atomic<bool> fftResizeRequested_{ false };
    // shouldResetHoldToLive_ removed in V2 (using local edge detection)

    // Peak hold mode/timer (used by updatePeakHold)
    PeakHoldMode peakHoldMode_ = PeakHoldMode::HoldThenDecay;
    float peakHoldTimeMs_ = 0.0f;
    std::vector<int> peakHoldFramesRemaining_;
    

    
    StereoScopeAnalyzer stereoScopeAnalyzer;
    
    // Multi-trace feature flag (ENABLED for L/R/Mono/Mid/Side traces)
    bool enableMultiTrace_ = true;
    
    // L/R-channel FIFOs for dual-FFT processing (Phase 4 True L/R)
    std::vector<float> fifoBufferL_;
    int fifoWritePosL_ = 0;
    int samplesCollectedL_ = 0;
    
    std::vector<float> fifoBufferR_;
    int fifoWritePosR_ = 0;
    int samplesCollectedR_ = 0;
    
    void initializeFFT (int fftSize);
    // void updateSmoothingCoeff (float averagingMs, double sampleRate); // Removed in favor of Attack/Release ballistics
    void updateSmoothingBounds();
    
    void computeFFT();
    void applyWindow(const std::vector<float>& fifoIn, int writePos);  // Parameterized for dual-FFT
    void extractMagnitudes(float* powerOut, int numBins);  // Extract power from fftOutput
    void convertToDb (const float* magnitudes, float* dbOut, int numBins);
    // V1 Strict: dbInstant for Latch, dbBallistic (dbRaw_) for Release floor
    void updatePeakHold (const float* dbInstant, const float* dbBallistic, float* peakOut, int numBins);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalyzerEngine)
};
