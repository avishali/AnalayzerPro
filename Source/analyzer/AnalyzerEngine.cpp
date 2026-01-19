#include "AnalyzerEngine.h"
#include <cmath>
#include <algorithm>
#include <juce_events/juce_events.h>

//==============================================================================
AnalyzerEngine::AnalyzerEngine()
    : currentFFTSize (2048), currentHopSize (512)
{
    // Buffers will be resized in prepare()
    peakHoldEnabled_ = false;
    peakHoldMode_ = PeakHoldMode::HoldThenDecay;
}

AnalyzerEngine::~AnalyzerEngine() = default;

void AnalyzerEngine::prepare (double sampleRate, int /* samplesPerBlock */)
{
    currentSampleRate = sampleRate;
    peakHoldEnabled_ = false; // AC1: Ensure enabled on prepare
    
    // Initialize FFT size (use currentFFTSize, default 2048)
    initializeFFT (currentFFTSize);
    
    // CRITICAL: Keep sequence monotonic - do NOT reset to 0 (prevents UI "blink" detection issues)
    // Only initialize to 1 if this is the very first prepare (sequence is 0)
    if (published_.sequence.load (std::memory_order_relaxed) == 0)
    {
        published_.sequence.store (1, std::memory_order_relaxed);  // Start at 1, not 0
    }
    published_.data.isValid = false;
    
    // Initialize smoothing
    // averagingMs_ removed. Ballistics default in header.
    // updateSmoothingCoeff removed.
    
    prepared = true;
}

void AnalyzerEngine::initializeFFT (int fftSize)
{
    currentFFTSize = fftSize;
    currentHopSize = fftSize / 4;  // 25% overlap
    
    // Create FFT
    const int fftOrder = static_cast<int> (std::log2 (fftSize));
    fft = std::make_unique<juce::dsp::FFT> (fftOrder);
    
    // Resize buffers
    const std::size_t fftSizeSz = static_cast<std::size_t> (fftSize);
    fftOutput.resize (fftSizeSz * 2, 0.0f);
    window.resize (fftSizeSz, 1.0f);
    fifoBuffer.resize (fftSizeSz, 0.0f);
    const int numBins = fftSize / 2 + 1;
    smoothedMagnitude.resize (static_cast<size_t> (numBins), 0.0f);
    
    // Resize smoothing buffers
    smoothedMagnitude.resize(static_cast<size_t> (numBins), 0.0f);
    smoothedPeak.resize(static_cast<size_t> (numBins), 0.0f); // Restored
    
    smoothLowBounds.resize (static_cast<size_t> (numBins), 0);
    smoothHighBounds.resize (static_cast<size_t> (numBins), 0);
    updateSmoothingBounds();
    prefixSumMag.resize (static_cast<size_t> (numBins + 1), 0.0f);
    
    peakHold.resize (static_cast<size_t> (numBins), kDbFloor);
    
    peakHoldFramesRemaining_.resize (static_cast<size_t> (numBins), 0);
    
    // Resize per-frame computation buffers (eliminates allocations in computeFFT)
    magnitudes_.resize (static_cast<size_t> (numBins), 0.0f);
    dbValues_.resize (static_cast<size_t> (numBins), 0.0f);
    dbRaw_.resize (static_cast<size_t> (numBins), 0.0f);
    dbInstant_.resize (static_cast<size_t> (numBins), 0.0f);
    
    // Multi-trace: Preallocate L/R-channel FIFOs
    fifoBufferL_.resize (fftSizeSz, 0.0f);
    fifoWritePosL_ = 0;
    samplesCollectedL_ = 0;
    
    fifoBufferR_.resize (fftSizeSz, 0.0f);
    fifoWritePosR_ = 0;
    samplesCollectedR_ = 0;
    
    // Multi-trace: Preallocate power spectrum buffers for all 5 traces
    const size_t numBinsSz = static_cast<size_t>(numBins);
    powerL_.resize (numBinsSz, 0.0f);
    powerR_.resize (numBinsSz, 0.0f);
    powerMono_.resize (numBinsSz, 0.0f);
    powerMid_.resize (numBinsSz, 0.0f);
    powerSide_.resize (numBinsSz, 0.0f);
    
    // Multi-trace: Preallocate smoothed buffers
    smoothedL_.resize (numBinsSz, 0.0f);
    smoothedR_.resize (numBinsSz, 0.0f);
    smoothedMono_.resize (numBinsSz, 0.0f);
    smoothedMid_.resize (numBinsSz, 0.0f);
    smoothedSide_.resize (numBinsSz, 0.0f);
    
    // Multi-trace: Preallocate peak buffers
    peakL_.resize (numBinsSz, kDbFloor);
    peakR_.resize (numBinsSz, kDbFloor);
    peakMono_.resize (numBinsSz, kDbFloor);
    peakMid_.resize (numBinsSz, kDbFloor);
    peakSide_.resize (numBinsSz, kDbFloor);
    
    
    // Initialize window (Hann)
    const float pi = juce::MathConstants<float>::pi;
    for (int i = 0; i < fftSize; ++i)
    {
        const std::size_t idx = static_cast<std::size_t> (i);
        window[idx] = 0.5f * (1.0f - std::cos (2.0f * pi * static_cast<float> (i) / static_cast<float> (fftSize - 1)));
    }
    
    // Reset state
    fifoWritePos = 0;
    samplesCollected = 0;
    std::fill (fifoBuffer.begin(), fifoBuffer.end(), 0.0f);
    std::fill (fftOutput.begin(), fftOutput.end(), 0.0f);
    std::fill (smoothedMagnitude.begin(), smoothedMagnitude.end(), 0.0f);
    resetPeaks();
    
    // Safety guard: ensure numBins doesn't exceed array capacity
    jassert (numBins <= static_cast<int> (published_.data.fftDb.size()));
    jassert (numBins <= static_cast<int> (published_.data.fftPeakDb.size()));
    
    // CRITICAL: Mark snapshot as invalid on FFT size change to prevent "blink to floor"
    // Only publish valid snapshots after first real FFT computation
    // Do NOT fill arrays to floor here - UI will hold last valid frame
    published_.data.isValid = false;
    published_.data.fftSize = fftSize;
    published_.data.numBins = numBins;
    published_.data.fftBinCount = numBins;
    
    // M_2026_01_19_PEAK_HOLD_INIT_VALUE_FIX: Explicitly initialize snapshot peak arrays to floor
    // AnalyzerSnapshot uses std::array which defaults to 0.0f, causing startup glitch (-0dB white line).
    std::fill (stagingSnapshot_.fftPeakDb.begin(), stagingSnapshot_.fftPeakDb.end(), kDbFloor);
    std::fill (stagingSnapshot_.fftPeakHoldDb.begin(), stagingSnapshot_.fftPeakHoldDb.end(), kDbFloor);
    
    std::fill (stagingSnapshot_.fftPeakDbL.begin(), stagingSnapshot_.fftPeakDbL.end(), kDbFloor);
    std::fill (stagingSnapshot_.fftPeakDbR.begin(), stagingSnapshot_.fftPeakDbR.end(), kDbFloor);
    std::fill (stagingSnapshot_.fftPeakDbMono.begin(), stagingSnapshot_.fftPeakDbMono.end(), kDbFloor);
    std::fill (stagingSnapshot_.fftPeakDbMid.begin(), stagingSnapshot_.fftPeakDbMid.end(), kDbFloor);
    std::fill (stagingSnapshot_.fftPeakDbSide.begin(), stagingSnapshot_.fftPeakDbSide.end(), kDbFloor);
    
    std::fill (stagingSnapshot_.fftPeakHoldDbL.begin(), stagingSnapshot_.fftPeakHoldDbL.end(), kDbFloor);
    std::fill (stagingSnapshot_.fftPeakHoldDbR.begin(), stagingSnapshot_.fftPeakHoldDbR.end(), kDbFloor);
    std::fill (stagingSnapshot_.fftPeakHoldDbMono.begin(), stagingSnapshot_.fftPeakHoldDbMono.end(), kDbFloor);
    std::fill (stagingSnapshot_.fftPeakHoldDbMid.begin(), stagingSnapshot_.fftPeakHoldDbMid.end(), kDbFloor);
    std::fill (stagingSnapshot_.fftPeakHoldDbSide.begin(), stagingSnapshot_.fftPeakHoldDbSide.end(), kDbFloor);
}

void AnalyzerEngine::reset()
{
    prepared = false;
    fft.reset();
    fifoWritePos = 0;
    samplesCollected = 0;
    fftOutput.clear();
    window.clear();
    fifoBuffer.clear();
    fifoBuffer.clear();
    smoothedMagnitude.clear();
    smoothedPeak.clear(); // Restored
    peakHold.clear();
    magnitudes_.clear();
    dbValues_.clear();
    dbRaw_.clear();
    dbInstant_.clear();
}

void AnalyzerEngine::processBlock (const juce::AudioBuffer<float>& buffer)
{
    if (!prepared || fft == nullptr)
        return;
    
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    
    if (numChannels == 0)
        return;
    
    // Mono sum (or use left channel if mono)
    const float* inputChannel = buffer.getReadPointer (0);
    
    // Accumulate into FIFO buffer
    for (int i = 0; i < numSamples; ++i)
    {
        const float sampleL = inputChannel[i];
        const float sampleR = (numChannels > 1) ? buffer.getReadPointer (1)[i] : sampleL;
        const float sampleMono = (sampleL + sampleR) * 0.5f;
        
        // 1. Mono Sum FIFO (Legacy)
        fifoBuffer[static_cast<std::size_t> (fifoWritePos)] = sampleMono;
        fifoWritePos = (fifoWritePos + 1) % currentFFTSize;
        samplesCollected++;

        // 2. Multi-trace True L/R FIFOs
        if (enableMultiTrace_ && numChannels > 1)
        {
            // L FIFO
            fifoBufferL_[static_cast<std::size_t>(fifoWritePosL_)] = sampleL;
            fifoWritePosL_ = (fifoWritePosL_ + 1) % currentFFTSize;
            samplesCollectedL_++;

            // R FIFO
            fifoBufferR_[static_cast<std::size_t>(fifoWritePosR_)] = sampleR;
            fifoWritePosR_ = (fifoWritePosR_ + 1) % currentFFTSize;
            samplesCollectedR_++;
        }
        
        // When we have enough samples, compute FFT
        if (samplesCollected >= currentHopSize)
        {
            samplesCollected = 0;
            
            if (!enableMultiTrace_)
            {
                // Legacy path: single FFT on mono mix
                computeFFT();
            }
            else
            {
                // Multi-trace path: compute L and R FFTs
                const int numBins = currentFFTSize / 2 + 1;
                
                // L channel: Use fifoBufferL_
                samplesCollectedL_ = 0;
                applyWindow(fifoBufferL_, fifoWritePosL_);
                fft->performRealOnlyForwardTransform (fftOutput.data(), false);
                extractMagnitudes(powerL_.data(), numBins);
                
                // R channel: Use fifoBufferR_
                if (numChannels > 1)
                {
                    samplesCollectedR_ = 0;
                    applyWindow(fifoBufferR_, fifoWritePosR_);
                    fft->performRealOnlyForwardTransform (fftOutput.data(), false);
                    extractMagnitudes(powerR_.data(), numBins);
                }
                
                // Finally, do the Legacy FFT (Mono) to populate fftDb for the main traces
                computeFFT();
            }
        }
    }

    // Push samples to Stereo Scope (Audio thread lock-free)
    const float* left = buffer.getReadPointer (0);
    const float* right = (numChannels > 1) ? buffer.getReadPointer (1) : left;
    stereoScopeAnalyzer.pushSamples (left, right, numSamples);
}

void AnalyzerEngine::computeFFT()
{
    // If a resize is pending, hold publishing until the resize is applied on the message thread.
    if (fftResizeRequested_.load (std::memory_order_acquire))
        return;

    if (!prepared || fft == nullptr || currentFFTSize == 0)
        return;
    
    // Apply window and write directly to FFT output buffer
    applyWindow(fifoBuffer, fifoWritePos);
    
    // Perform real-only forward FFT (fftOutput now contains valid time-domain samples)
    fft->performRealOnlyForwardTransform (fftOutput.data(), false);
    
    // Compute magnitudes and convert to dB
    const int numBins = currentFFTSize / 2 + 1;
    
    // Extract power spectrum using helper (same math, now reusable for dual-FFT)
    extractMagnitudes(magnitudes_.data(), numBins);
    
    // -------------------------------------------------------------------------
    // Frequency Smoothing (Fractional Octave) - Applied to POWER
    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------
    // Frequency Smoothing (Fractional Octave) - Applied to POWER
    // -------------------------------------------------------------------------
    // We reuse fftOutput as a scratch buffer for spectrally smoothed power.
    // This serves as the "Instantaneous Smoothed Power" (pre-ballistics).
    float* freqSmoothed = fftOutput.data();
    
    int binsProcessed = 0;
    int binsFilled = 0;
    juce::ignoreUnused (binsProcessed, binsFilled);

    if (smoothingOctaves_ > 0.0f && static_cast<int>(smoothLowBounds.size()) == numBins)
    {
        // 1. Compute Prefix Sum of magnitudes (Power) (O(N))
        if(static_cast<int>(prefixSumMag.size()) != numBins + 1)
             prefixSumMag.resize(static_cast<std::size_t>(numBins + 1), 0.0f);

        prefixSumMag[0] = 0.0f;
        for (int i = 0; i < numBins; ++i)
        {
            prefixSumMag[static_cast<std::size_t> (i + 1)] = prefixSumMag[static_cast<std::size_t> (i)] + magnitudes_[static_cast<std::size_t> (i)];
        }
        
        // 2. Apply smoothing using bounds (O(N))
        for (int i = 0; i < numBins; ++i)
        {
            const int low = smoothLowBounds[static_cast<std::size_t> (i)];
            const int high = smoothHighBounds[static_cast<std::size_t> (i)];
            const int count = high - low + 1;
            
            if (count > 0)
            {
                const float sum = prefixSumMag[static_cast<std::size_t> (high + 1)] - prefixSumMag[static_cast<std::size_t> (low)];
                freqSmoothed[i] = sum / static_cast<float> (count);
            }
            else
            {
                freqSmoothed[i] = magnitudes_[static_cast<std::size_t> (i)];
            }
            binsFilled++;
        }
        binsProcessed = numBins;
    }
    else
    {
        // Bypass: Copy raw magnitudes (Power) to freqSmoothed
        // This ensures downstream logic (Time Smoothing, Peaks) always sees consumption-ready data
        std::copy (magnitudes_.begin(), magnitudes_.begin() + numBins, freqSmoothed);
        
        binsProcessed = numBins;
        binsFilled = numBins;
    }

    // -------------------------------------------------------------------------
    // Time Smoothing (Ballistics) - Applied to Power
    // -------------------------------------------------------------------------
    // Calculate coefficients based on current hop time
    const double hopSec = static_cast<double> (currentHopSize) / currentSampleRate;
    
    auto calcCoeff = [hopSec](float timeMs) -> float {
        if (timeMs <= 0.1f) return 0.0f; // Instant
        return static_cast<float>(std::exp(-hopSec / (static_cast<double>(timeMs) / 1000.0)));
    };

    const float rmsAttCoeff = calcCoeff(rmsAttackMs_);
    const float rmsRelCoeff = calcCoeff(rmsReleaseMs_);
    const float peakAttCoeff = calcCoeff(peakAttackMs_);
    const float peakRelCoeff = calcCoeff(peakReleaseMs_);

    for (int i = 0; i < numBins; ++i)
    {
        const std::size_t idx = static_cast<std::size_t> (i);
        const float inputPower = freqSmoothed[idx];

        // RMS Ballistics
        float& rmsState = smoothedMagnitude[idx];
        const float rmsCoeff = (inputPower > rmsState) ? rmsAttCoeff : rmsRelCoeff;
        rmsState = rmsCoeff * rmsState + (1.0f - rmsCoeff) * inputPower;

        // Peak Ballistics (Restored for "Silky" consistency)
        float& peakState = smoothedPeak[idx];
        const float peakCoeff = (inputPower > peakState) ? peakAttCoeff : peakRelCoeff;
        peakState = peakCoeff * peakState + (1.0f - peakCoeff) * inputPower;
    }
    
#if JUCE_DEBUG
    // DEBUG: Log smoothing stats once per second
    static uint32_t smoothDebugCounter = 0;
    if ((++smoothDebugCounter % 100) == 0)
    {
        DBG ("FFT Smoothing: oct=" << smoothingOctaves_ << " rmsAtt=" << rmsAttackMs_ << " rmsRel=" << rmsReleaseMs_);
    }
#endif
    
    // Convert Time-Smoothed POWER to dB for display (Main Trace / RMS)
    convertToDb (smoothedMagnitude.data(), dbValues_.data(), numBins);
    
    // Convert Fast-Peak Smoothed POWER to dB (Ballistic Peak Trace)
    // Feeds into the Peak Hold logic for decay tracking AND Display
    convertToDb (smoothedPeak.data(), dbRaw_.data(), numBins);
    // std::copy(dbInstant_.begin(), dbInstant_.end(), dbRaw_.begin()); // Removed instant copy
    
    // Peak Pipeline: Calculate Instantaneous dB from RAW magnitudes (no octave smoothing)
    // This ensures Peak latches the TRUE session max, independent from RMS smoothing.
    convertToDb (magnitudes_.data(), dbInstant_.data(), numBins);
    
    // Update peak hold
    // Pass dbInstant_ for Latching, dbRaw_ for Release tracking
    updatePeakHold (dbInstant_.data(), dbRaw_.data(), peakHold.data(), numBins);
    
    // CRITICAL: numBins must equal expectedBins (fftSize/2 + 1)
    jassert (numBins == (currentFFTSize / 2 + 1));  // DEBUG assert: bin count must match FFT size
    
    // Per-Bin Clamping to Peak Hold Ceiling (AC3)
    // Ensures no trace ever exceeds the "Maximum Envelope" (Peak Hold)
    // M_2026_01_19_PEAK_HOLD_PROFESSIONAL_BEHAVIOR
    const std::size_t numBinsSz = static_cast<std::size_t> (numBins);
    for (std::size_t i = 0; i < numBinsSz; ++i)
    {
        // 1. Peak Hold Integrity: 
        // If live traces (RMS or Ballistic Peak) exceed Hold, pull Hold UP.
        // If live traces are below Hold, clamp traces DOWN to Hold (ceiling).
        
        float hold = peakHold[i];
        
        // Clamp RMS (dbValues_) to Hold
        if (dbValues_[i] > hold)
        {
            hold = dbValues_[i]; // Push up
            peakHold[i] = hold;
        }
        else
        {
            // Clamp down (RMS cannot exceed Hold)
            // Actually, pushing up handles equality. 
            // We just need to ensure dbValues_ <= hold.
            // If dbValues_ was > hold, we raised hold to match.
            // If dbValues_ < hold, it's fine.
            // Wait, "Peak Hold represents absolute maximum".
            // So logic checks OUT: max(RMS, Peak) > Hold -> Hold = max.
        }
        
        // Clamp Ballistic Peak (dbRaw_) to Hold
        if (dbRaw_[i] > hold)
        {
             hold = dbRaw_[i];
             peakHold[i] = hold;
        }
        
        // Multi-Trace Clamping if enabled
        if (enableMultiTrace_)
        {
            // Note: multi-trace buffers are POWER domain here (powerL_, powerR_).
            // Clamping usually happens in dB domain. 
            // We'll trust UI to handle multi-trace clamping or do it in UI thread derivation.
            // For now, focus on RMS/Peak which are the main traces.
        }
    }

    // SANITIZATION (Fix 2: HF Spikes / NaN / Overflow protection)
    // Ensure no invalid values leak into the snapshot
    // SANITIZATION (Fix 2: HF Spikes / NaN / Overflow protection)
    // Ensure no invalid values leak into the snapshot
    for (std::size_t i = 0; i < numBinsSz; ++i)
    {
        // Clamp Peak Hold
        if (!std::isfinite(peakHold[i])) 
            peakHold[i] = kDbFloor;
        else
            peakHold[i] = juce::jlimit(kDbFloor, 12.0f, peakHold[i]);

        // Clamp Ballistic Peak (dbRaw_)
        if (!std::isfinite(dbRaw_[i])) 
            dbRaw_[i] = kDbFloor;
        else
            dbRaw_[i] = juce::jlimit(kDbFloor, 12.0f, dbRaw_[i]);
            
        // Clamp RMS (dbValues_)
        if (!std::isfinite(dbValues_[i])) 
            dbValues_[i] = kDbFloor;
        else
            dbValues_[i] = juce::jlimit(kDbFloor, 12.0f, dbValues_[i]);
    }

    // Use pre-allocated staging snapshot to prevent stack overflow (AC5, AC7)
    // Ref: M_2026_01_19_PEAK_HOLD_PROFESSIONAL_BEHAVIOR_RETRY
    AnalyzerSnapshot& snapshot = stagingSnapshot_; 
    
    snapshot.fftBinCount = numBins;
    snapshot.numBins = 0; // Legacy
    snapshot.sampleRate = currentSampleRate;
    snapshot.fftSize = currentFFTSize;
    snapshot.displayBottomDb = -120.0f;
    snapshot.displayTopDb = 0.0f;
    snapshot.isValid = true;
    snapshot.isHoldOn = freezePeaks_.load (std::memory_order_relaxed);
    
    // Safety guard: ensure numBins doesn't exceed array capacity
    jassert (numBins <= static_cast<int> (snapshot.fftDb.size()));
    jassert (numBins <= static_cast<int> (snapshot.fftPeakDb.size()));
    
    // Copy dB values with floor clamping (-120.0f floor for dB values)
    // CRITICAL: Use copyBins to prevent OOB writes
    constexpr float dbFloor = -120.0f;
    const int maxBins = static_cast<int> (snapshot.fftDb.size());
    const int copyBins = juce::jmin (numBins, maxBins);
    jassert (numBins <= static_cast<int> (AnalyzerSnapshot::kMaxFFTBins));  // Hard runtime check
    
    for (int i = 0; i < copyBins; ++i)
    {
        const std::size_t idx = static_cast<std::size_t> (i);
        snapshot.fftDb[idx] = juce::jmax (dbFloor, dbValues_[idx]);
        
        // Populate snapshot with Ballistic Peak (dbRaw_)
        // Previously used peakHold, now peakHold is separate
        snapshot.fftPeakDb[idx] = juce::jmax (dbFloor, dbRaw_[idx]);
        
        // Populate snapshot with Peak Hold (AC1 - Existing Buffer)
        snapshot.fftPeakHoldDb[idx] = juce::jmax (dbFloor, peakHold[idx]);
    }
    
    // Multi-trace: Copy power domain arrays for UI-side derivation
    snapshot.multiTraceEnabled = enableMultiTrace_;
    if (enableMultiTrace_)
    {
        for (int i = 0; i < copyBins; ++i)
        {
            const std::size_t idx = static_cast<std::size_t> (i);
            snapshot.powerL[idx] = powerL_[idx];
            snapshot.powerR[idx] = powerR_[idx];
        }
    }
    
#if JUCE_DEBUG
    // DEBUG: Log FFT data range once per second (throttled)
    static uint32_t debugLogCounter = 0;
    if ((++debugLogCounter % 100) == 0)  // Approx once per second at 48kHz/512 samples
    {
        float minDb = dbValues_[static_cast<std::size_t> (0)];
        float maxDb = dbValues_[static_cast<std::size_t> (0)];
        for (int i = 1; i < numBins; ++i)
        {
            const std::size_t idx = static_cast<std::size_t> (i);
            minDb = juce::jmin (minDb, dbValues_[idx]);
            maxDb = juce::jmax (maxDb, dbValues_[idx]);
        }
         // DBG ("FFT bins=" << numBins << " minDb=" << minDb << " maxDb=" << maxDb
         //     << " fftSize=" << currentFFTSize << " hop=" << currentHopSize);
        
        // Assert bin count consistency
        jassert (numBins <= static_cast<int> (AnalyzerSnapshot::kMaxFFTBins));
    }
#endif
    
    // Publish snapshot (audio thread, lock-free)
    publishSnapshot (snapshot);
}

void AnalyzerEngine::applyWindow(const std::vector<float>& fifoIn, int writePosIn)
{
    // Write windowed time-domain samples into the SAME buffer we pass to JUCE FFT.
    // JUCE real-only FFT expects first N floats to contain the real samples.
    for (int i = 0; i < currentFFTSize; ++i)
    {
        const int fifoIndex = (writePosIn + i) % currentFFTSize;
        const std::size_t idx = static_cast<std::size_t> (i);
        const std::size_t fifoIdx = static_cast<std::size_t> (fifoIndex);
        fftOutput[idx] = fifoIn[fifoIdx] * window[idx];
    }

    // Zero-pad the remainder (JUCE uses this buffer in-place for output too)
    const std::size_t fftSizeSz = static_cast<std::size_t> (currentFFTSize);
    std::fill (fftOutput.begin() + static_cast<std::ptrdiff_t> (fftSizeSz), fftOutput.end(), 0.0f);
}

void AnalyzerEngine::extractMagnitudes(float* powerOut, int numBins)
{
    // Extract power spectrum from fftOutput (assumes FFT already performed)
    // Real-only FFT output format: [DC, Nyquist, real1, imag1, real2, imag2, ...]
    
    // DC bin (real only)
    powerOut[0] = fftOutput[0] * fftOutput[0];
    
    // Middle bins (complex)
    for (int i = 1; i < numBins - 1; ++i)
    {
        const std::size_t fftIdx1 = static_cast<std::size_t> (2 * i);
        const std::size_t fftIdx2 = static_cast<std::size_t> (2 * i + 1);
        const float real = fftOutput[fftIdx1];
        const float imag = fftOutput[fftIdx2];
        powerOut[i] = real * real + imag * imag;
    }
    
    // Nyquist bin (real only, stored at index 1)
    const float nyquistVal = fftOutput[1];
    powerOut[numBins - 1] = nyquistVal * nyquistVal;
    
    // Apply FFT normalization
    const float scale = 2.0f / static_cast<float> (currentFFTSize);
    const float powerScale = (scale * scale) * 4.0f;  // Hann window correction
    
    for (int i = 0; i < numBins; ++i)
    {
        powerOut[i] *= powerScale;
    }
    
    // Correct DC and Nyquist (factor of 0.25)
    powerOut[0] *= 0.25f;
    powerOut[numBins - 1] *= 0.25f;
}

void AnalyzerEngine::convertToDb (const float* magnitudes, float* dbOut, int numBins)
{
    // Convert POWER to dB
    // Use -120.0f floor for consistency (matches snapshot clamping)
    constexpr float dbFloor = -120.0f;
    constexpr float powerFloor = 1e-12f;  // 10*log10(1e-12) = -120dB
    
    for (int i = 0; i < numBins; ++i)
    {
        const std::size_t idx = static_cast<std::size_t> (i);
        const float power = juce::jmax (powerFloor, magnitudes[idx]);
        const float db = 10.0f * std::log10 (power); // 10*log10 for Power
        dbOut[i] = juce::jmax (dbFloor, db);
    }
}

void AnalyzerEngine::updatePeakHold (const float* dbInstant, const float* dbBallistic, float* peakOut, int numBins)
{
    // Legacy flag: if disabled, behave like Off.
    if (! peakHoldEnabled_ || peakHoldMode_ == PeakHoldMode::Off)
    {
        const std::size_t numBinsSz = static_cast<std::size_t> (numBins);
        std::fill (peakOut, peakOut + numBinsSz, kDbFloor);
        return;
    }

    // Ensure timer vector matches bins (defensive; initializeFFT should size this).
    if (static_cast<int> (peakHoldFramesRemaining_.size()) != numBins)
        peakHoldFramesRemaining_.assign (static_cast<std::size_t> (numBins), 0);

    // Decay per FFT frame: (dB/s) * (hopSec)
    const float hopSec = static_cast<float> (currentHopSize) / static_cast<float> (currentSampleRate);
    const float decayDbPerSec = (peakDecayCurve_ == PeakDecayCurve::TimeConstant60dB)
                                  ? (60.0f / juce::jmax (0.01f, peakDecayTimeConstantSec_))
                                  : peakDecayDbPerSec;
    const float decayPerFrame = (decayDbPerSec * hopSec);

    // Hold time expressed in FFT frames (only used for HoldThenDecay)
    const int holdFramesTotal = (hopSec > 0.0f && peakHoldTimeMs_ > 0.0f)
                                  ? juce::jmax (1, static_cast<int> (std::ceil ((peakHoldTimeMs_ / 1000.0f) / hopSec)))
                                  : 0;

    // Strict Hold Logic V1
    const bool isHoldOn = freezePeaks_.load (std::memory_order_acquire);

    for (int i = 0; i < numBins; ++i)
    {
        const std::size_t idx = static_cast<std::size_t> (i);
        const float instant = dbInstant[idx];
        const float ballistic = dbBallistic[idx];

        // 1. Strict Latch (Attack):
        // Always bump UP if we see a higher instantaneous peak.
        // This applies in BOTH Hold ON and Hold OFF modes (Peak never drops below instant)
        if (instant > peakOut[idx])
        {
            peakOut[idx] = instant;
            
            if (peakHoldMode_ == PeakHoldMode::HoldThenDecay && holdFramesTotal > 0)
                peakHoldFramesRemaining_[idx] = holdFramesTotal;
        }

        // 2. Hold ON: Strict Freeze
        if (isHoldOn)
        {
            // Requirement 1: NEVER decay.
            // Requirement 2: Strict Session Max (handled by Attack above).
            // Requirement 3: Do NOT reset to 'instant' on transition (True Freeze).
            continue; 
        }

        // 4. Hold OFF: Release/Decay Mode (Step 3)
        // Requirement 2: Decay towards Live Peak (Ballistic), never below it.
        
        switch (peakHoldMode_)
        {
            case PeakHoldMode::Infinite:
                break;

            case PeakHoldMode::Decay:
                // Decay, but clamp to Ballistic floor
                peakOut[idx] = juce::jmax (ballistic, peakOut[idx] - decayPerFrame);
                break;

            case PeakHoldMode::HoldThenDecay:
            {
                int& frames = peakHoldFramesRemaining_[idx];
                if (frames > 0)
                {
                    --frames;
                }
                else
                {
                    peakOut[idx] = juce::jmax (ballistic, peakOut[idx] - decayPerFrame);
                }
                break;
            }

            case PeakHoldMode::Off:
            default:
                break;
        }
        
        // Final guard: Ensure we didn't drift below RMS (Consistency)
        // Note: ballistic usually >= RMS, but just in case.
        if (peakOut[idx] < dbValues_[idx])
            peakOut[idx] = dbValues_[idx];
    }
}

void AnalyzerEngine::setFftSize (int fftSize)
{
    // Validate FFT size (must be power of 2, within range)
    // Do NOT clamp 1024 to 2048 - user explicitly chose 1024, respect it
    int validSize = 2048;
    if (fftSize == 1024 || fftSize == 2048 || fftSize == 4096 || fftSize == 8192)
        validSize = fftSize;
    else if (fftSize < 1024)
        validSize = 1024;  // Clamp to minimum 1024 (not 2048)
    else if (fftSize > 8192)
        validSize = 8192;
    else
    {
        // Round to nearest power of 2
        validSize = 1 << static_cast<int> (std::ceil (std::log2 (fftSize)));
        validSize = juce::jlimit (1024, 8192, validSize);
    }

    // Always route through requestFftSize (RT-safe, centralizes metadata invalidation).
    // requestFftSize will handle prepared/not-prepared state.
    if (validSize != currentFFTSize)
        requestFftSize (validSize);
}

void AnalyzerEngine::requestFftSize (int fftSize)
{
    // Micro-polish: avoid re-requesting the same FFT size
    const int pending = pendingFftSize_.load (std::memory_order_acquire);
    if ((pending == fftSize && fftResizeRequested_.load (std::memory_order_acquire)) ||
        (pending == 0 && fftSize == currentFFTSize))
    {
        return;
    }

    pendingFftSize_.store (fftSize, std::memory_order_release);
    fftResizeRequested_.store (true, std::memory_order_release);

    // Metadata invalidation (no allocations)
    const int numBins = fftSize / 2 + 1;

    jassert (numBins <= static_cast<int> (published_.data.fftDb.size()));
    jassert (numBins <= static_cast<int> (published_.data.fftPeakDb.size()));

    published_.data.isValid = false;
    published_.data.fftSize = fftSize;
    published_.data.sampleRate = currentSampleRate;
    published_.data.numBins = numBins;      // legacy/compat
    published_.data.fftBinCount = numBins;
}

    void AnalyzerEngine::applyPendingFftSizeIfNeeded()
    {
    #if JUCE_DEBUG
        jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    #endif

        if (! fftResizeRequested_.load (std::memory_order_acquire))
            return;

        const int requested = pendingFftSize_.load (std::memory_order_acquire);

        // Nothing meaningful pending: clear the request state.
        if (requested <= 0 || requested == currentFFTSize)
        {
            pendingFftSize_.store (0, std::memory_order_release);
            fftResizeRequested_.store (false, std::memory_order_release);
            return;
        }

        // Apply the resize on the message thread (allocations/resizes are allowed here).
        initializeFFT (requested);
        // updateSmoothingCoeff removed

        // If a newer request arrived while we were resizing, keep the flag set.
        const int nowPending = pendingFftSize_.load (std::memory_order_acquire);
        if (nowPending > 0 && nowPending != currentFFTSize)
        {
            fftResizeRequested_.store (true, std::memory_order_release);
            return;
        }

        // No newer request: clear state.
        pendingFftSize_.store (0, std::memory_order_release);
        fftResizeRequested_.store (false, std::memory_order_release);
    }

    void AnalyzerEngine::setAveragingMs (float averagingMs)
    {
        // Removed in favor of Attack/Release. 
        // Mapping legacy "Averaging" to "Release" for basic compatibility if needed, 
        // or just ignoring.
        // For now, let's map it to RMS Release to keep some control if UI calls this.
        rmsReleaseMs_ = juce::jlimit(10.0f, 2000.0f, averagingMs);
    }

void AnalyzerEngine::resetPeaks()
{
    std::fill (peakHold.begin(), peakHold.end(), kDbFloor);
    std::fill (peakHoldFramesRemaining_.begin(), peakHoldFramesRemaining_.end(), 0);
    
    // Multi-trace Peak Reset
    std::fill (peakL_.begin(), peakL_.end(), kDbFloor);
    std::fill (peakR_.begin(), peakR_.end(), kDbFloor);
    std::fill (peakMono_.begin(), peakMono_.end(), kDbFloor);
    std::fill (peakMid_.begin(), peakMid_.end(), kDbFloor);
    std::fill (peakSide_.begin(), peakSide_.end(), kDbFloor);
}

void AnalyzerEngine::setPeakHoldMode (PeakHoldMode mode)
{
    peakHoldMode_ = mode;

    // Keep legacy enable flag in sync.
    peakHoldEnabled_ = (peakHoldMode_ != PeakHoldMode::Off);

    // If turning off, clear peaks.
    if (peakHoldMode_ == PeakHoldMode::Off)
    {
        resetPeaks();
        return;
    }

    // Reset timers when switching modes (safe, avoids “stuck hold”).
    std::fill (peakHoldFramesRemaining_.begin(), peakHoldFramesRemaining_.end(), 0);
}

void AnalyzerEngine::setPeakHoldTimeMs (float holdTimeMs)
{
    peakHoldTimeMs_ = juce::jlimit (0.0f, 5000.0f, holdTimeMs);
    std::fill (peakHoldFramesRemaining_.begin(), peakHoldFramesRemaining_.end(), 0);
}

void AnalyzerEngine::setHold (bool hold)
{
    // V2: Just set the atomic flag. Reset logic is handled by edge detection in updatePeakHold.
    freezePeaks_.store (hold, std::memory_order_release);
}

void AnalyzerEngine::setPeakDecayDbPerSec (float decayDbPerSec)
{
    peakDecayDbPerSec = juce::jlimit (0.0f, 60.0f, decayDbPerSec);
}

void AnalyzerEngine::setReleaseTimeMs (float ms)
{
    const float clampedMs = juce::jlimit (100.0f, 5000.0f, ms);
    
    // 1. RMS Release
    rmsReleaseMs_ = clampedMs;
    
    // 2. Peak Release (Fix: Linked to Release Time)
    peakReleaseMs_ = clampedMs;
    
    // 3. Derive Peak Decay Rate (dB/sec)
    // Formula: 60dB drop over ReleaseTime
    // dbPerSec = 60.0f / (seconds)
    const float seconds = clampedMs / 1000.0f;
    const float computedDecay = 60.0f / seconds;
    
    peakDecayDbPerSec = computedDecay;
    
    // 3. Peak Hold Decay (inherits from peakDecayDbPerSec in updatePeakHold)
}

void AnalyzerEngine::setPeakDecayCurve (PeakDecayCurve curve)
{
    peakDecayCurve_ = curve;
}

void AnalyzerEngine::setPeakDecayTimeConstantSec (float seconds)
{
    peakDecayTimeConstantSec_ = juce::jlimit (0.01f, 10.0f, seconds);
}

void AnalyzerEngine::setSmoothingOctaves (float octaves)
{
    if (std::abs(smoothingOctaves_ - octaves) < 1e-4f)
        return;
        
    smoothingOctaves_ = octaves;
    updateSmoothingBounds();
}

void AnalyzerEngine::updateSmoothingBounds()
{
    if (smoothingOctaves_ <= 0.0f)
        return;

    const int numBins = currentFFTSize / 2 + 1;
    if (static_cast<int>(smoothLowBounds.size()) != numBins)
        smoothLowBounds.resize (static_cast<size_t> (numBins));
    if (static_cast<int>(smoothHighBounds.size()) != numBins)
        smoothHighBounds.resize (static_cast<size_t> (numBins));
        
    // Standard octave bandwidth calculation:
    // f_upper = f_center * 2^(oct/2)
    // f_lower = f_center * 2^(-oct/2)
    const double octaveFactor = std::pow (2.0, static_cast<double> (smoothingOctaves_) * 0.5);
    const double invOctaveFactor = 1.0 / octaveFactor;
    
    for (int i = 0; i < numBins; ++i)
    {
        if (i == 0) // DC
        {
            smoothLowBounds[0] = 0;
            smoothHighBounds[0] = 0;
            continue;
        }
        
        int low = static_cast<int> (std::floor (static_cast<double> (i) * invOctaveFactor));
        int high = static_cast<int> (std::ceil (static_cast<double> (i) * octaveFactor));
        
        // Clamp
        low = juce::jlimit (0, numBins - 1, low);
        high = juce::jlimit (0, numBins - 1, high);
        
        // Ensure effective smoothing for low bins (at least self)
        if (low > i) low = i;
        if (high < i) high = i;
        
        smoothLowBounds[static_cast<size_t> (i)] = low;
        smoothHighBounds[static_cast<size_t> (i)] = high;
    }
}

// Wave smoothing update removed.

void AnalyzerEngine::publishSnapshot (const AnalyzerSnapshot& source)
{
    // CRITICAL: Never publish invalid or "floor-only" snapshots
    // Only publish valid snapshots with actual FFT data
    // Prefer fftBinCount for FFT bin counts; fallback to numBins exists for legacy snapshots only.
    const int binCount = (source.fftBinCount > 0) ? source.fftBinCount : source.numBins;
    if (!source.isValid || binCount <= 0 || binCount > static_cast<int> (AnalyzerSnapshot::kMaxFFTBins))
    {
        // Do not publish invalid snapshot - UI will hold last valid frame
        return;
    }
    
    // Copy ALL fields into published snapshot (deep copy arrays)
    published_.data.isValid = source.isValid;
    published_.data.fftBinCount = binCount;
    published_.data.numBins = source.numBins;
    published_.data.sampleRate = source.sampleRate;
    published_.data.fftSize = source.fftSize;
    published_.data.displayBottomDb = source.displayBottomDb;
    published_.data.displayBottomDb = source.displayBottomDb;
    published_.data.displayTopDb = source.displayTopDb;
    published_.data.isHoldOn = source.isHoldOn;
    
    // Deep copy arrays (fixed-size, but copy only used portion)
    const int numBins = binCount;
    
    // Safety guard: ensure numBins doesn't exceed array capacity
    jassert (numBins <= static_cast<int> (published_.data.fftDb.size()));
    jassert (numBins <= static_cast<int> (published_.data.fftPeakDb.size()));
    jassert (numBins <= static_cast<int> (source.fftDb.size()));
    jassert (numBins <= static_cast<int> (source.fftPeakDb.size()));
    
    const int copyBins = juce::jmin (numBins, static_cast<int> (published_.data.fftDb.size()));
    for (int i = 0; i < copyBins; ++i)
    {
        const std::size_t idx = static_cast<std::size_t> (i);
        published_.data.fftDb[idx] = source.fftDb[idx];
        published_.data.fftPeakDb[idx] = source.fftPeakDb[idx];
    }
    
    // Multi-trace: Copy power domain arrays
    published_.data.multiTraceEnabled = source.multiTraceEnabled;
    if (source.multiTraceEnabled)
    {
        jassert (numBins <= static_cast<int> (published_.data.powerL.size()));
        jassert (numBins <= static_cast<int> (published_.data.powerR.size()));
        
        for (int i = 0; i < copyBins; ++i)
        {
            const std::size_t idx = static_cast<std::size_t> (i);
            published_.data.powerL[idx] = source.powerL[idx];
            published_.data.powerR[idx] = source.powerR[idx];
        }
    }
    
    // Increment sequence AFTER data copy completes (release fence ensures visibility)
    // CRITICAL: Keep sequence monotonic - never reset to 0
    const uint32_t currentSeq = published_.sequence.load (std::memory_order_relaxed);
    const uint32_t next = (currentSeq == 0) ? 1 : (currentSeq + 1);  // Ensure we never stay at 0
    published_.sequence.store (next, std::memory_order_release);
}

bool AnalyzerEngine::getLatestSnapshot (AnalyzerSnapshot& dest) const
{
    if (!prepared)
        return false;
    
    // Retry loop to handle seqlock-style reads: if sequence changes during copy,
    // retry up to 3 times to catch the next stable frame. This prevents dropped
    // frames that cause UI stutter when the timer only calls once per tick.
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        // First read: get sequence (acquire barrier ensures we see published data)
        const uint32_t seq1 = published_.sequence.load (std::memory_order_acquire);
        
        if (seq1 == 0)
            return false;  // No data published yet
        
        // Copy published data into destination
        dest.isValid = published_.data.isValid;
        dest.fftBinCount = published_.data.fftBinCount;
        dest.numBins = published_.data.numBins;
        dest.sampleRate = published_.data.sampleRate;
        dest.fftSize = published_.data.fftSize;
        dest.displayBottomDb = published_.data.displayBottomDb;
        dest.displayBottomDb = published_.data.displayBottomDb;
        dest.displayTopDb = published_.data.displayTopDb;
        dest.isHoldOn = published_.data.isHoldOn;
        
        // Deep copy arrays
        // Prefer fftBinCount for FFT bin counts; fallback to numBins exists for legacy snapshots only.
        const int numBins = (published_.data.fftBinCount > 0) ? published_.data.fftBinCount
                                                              : published_.data.numBins;
        
        // Safety guard: ensure numBins doesn't exceed array capacity
        jassert (numBins <= static_cast<int> (dest.fftDb.size()));
        jassert (numBins <= static_cast<int> (dest.fftPeakDb.size()));
        jassert (numBins <= static_cast<int> (published_.data.fftDb.size()));
        jassert (numBins <= static_cast<int> (published_.data.fftPeakDb.size()));
        
        const int copyBins = juce::jmin (numBins, static_cast<int> (dest.fftDb.size()));
        for (int i = 0; i < copyBins; ++i)
        {
            const std::size_t idx = static_cast<std::size_t> (i);
            dest.fftDb[idx] = published_.data.fftDb[idx];
            dest.fftPeakDb[idx] = published_.data.fftPeakDb[idx];
        }

        // Multi-trace: Copy power domain arrays
        dest.multiTraceEnabled = published_.data.multiTraceEnabled;
        if (dest.multiTraceEnabled)
        {
            for (int i = 0; i < copyBins; ++i)
            {
                const std::size_t idx = static_cast<std::size_t> (i);
                dest.powerL[idx] = published_.data.powerL[idx];
                dest.powerR[idx] = published_.data.powerR[idx];
            }
        }
        
        // Second read to verify stability
        const uint32_t seq2 = published_.sequence.load (std::memory_order_acquire);
        
        // Return true only if sequence didn't change during copy (stable read)
        if (seq1 == seq2 && seq1 != 0)
            return true;
        
        // Sequence changed during copy (torn read) - retry to catch next stable frame
        // Continue loop to retry
    }
    
    // After retries, return false (sequence kept changing, likely high update rate)
    return false;
}
