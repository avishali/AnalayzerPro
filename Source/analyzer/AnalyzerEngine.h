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
class AnalyzerEngine
{
public:
    AnalyzerEngine();
    ~AnalyzerEngine();
    
    /** Prepare analyzer with sample rate and block size */
    void prepare (double sampleRate, int samplesPerBlock);
    
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
    void setAveragingMs (float averagingMs);
    void setHold (bool hold);
    void setPeakDecayDbPerSec (float decayDbPerSec);
    
private:
    static constexpr int kMaxFFTSize = 8192;
    static constexpr float kDbFloor = -90.0f;
    
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
    
    // Publish throttling for large FFT sizes (reduce UI update rate to prevent jitter)
    int publishThrottleCounter_ = 0;
    int publishThrottleDivisor_ = 1;  // Publish every Nth FFT frame (1 = every frame, 2 = every 2nd, etc.)
    
    // Smoothing and peak hold
    std::vector<float> smoothedMagnitude;
    std::vector<float> peakHold;
    std::vector<int> peakHoldCounter;
    
    // Per-frame computation buffers (resized in initializeFFT, reused to avoid allocations)
    std::vector<float> magnitudes_;
    std::vector<float> dbValues_;
    
    float smoothingCoeff = 0.9f;  // EMA retention coefficient (derived from averagingMs)
    float averagingMs_ = 100.0f;  // Current averaging time (ms) for recalculation after FFT size changes
    float peakDecayDbPerSec = 1.0f;
    bool holdEnabled = false;
    
    void initializeFFT (int fftSize);
    void updateSmoothingCoeff (float averagingMs, double sampleRate);
    
    void computeFFT();
    void applyWindow();
    void convertToDb (const float* magnitudes, float* dbOut, int numBins);
    void updatePeakHold (const float* dbIn, float* peakOut, int* counters, int numBins);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalyzerEngine)
};
