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
    const int oldNumBins = currentFFTSize / 2 + 1;
    const bool hasOldState = (currentFFTSize > 0 && !smoothedMagnitude.empty()
                              && static_cast<int> (smoothedMagnitude.size()) == oldNumBins);

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

    // Preserve ballistics state across FFT resize (prevents visual "pop")
    auto resampleBallistics = [](const std::vector<float>& old, int oldN, int newN) -> std::vector<float>
    {
        std::vector<float> result (static_cast<size_t> (newN), 0.0f);
        if (oldN <= 0 || old.empty())
            return result;
        for (int i = 0; i < newN; ++i)
        {
            const float srcPos = static_cast<float> (i) * static_cast<float> (oldN - 1) / static_cast<float> (newN - 1);
            const int lo = static_cast<int> (srcPos);
            const int hi = std::min (lo + 1, oldN - 1);
            const float frac = srcPos - static_cast<float> (lo);
            result[static_cast<size_t> (i)] = old[static_cast<size_t> (lo)] * (1.0f - frac)
                                            + old[static_cast<size_t> (hi)] * frac;
        }
        return result;
    };

    if (hasOldState && oldNumBins != numBins)
    {
        smoothedMagnitude = resampleBallistics (smoothedMagnitude, oldNumBins, numBins);
        smoothedPeak = resampleBallistics (smoothedPeak, oldNumBins, numBins);
        smoothedLRms_ = resampleBallistics (smoothedLRms_, oldNumBins, numBins);
        smoothedRRms_ = resampleBallistics (smoothedRRms_, oldNumBins, numBins);
        smoothedMidRms_ = resampleBallistics (smoothedMidRms_, oldNumBins, numBins);
        smoothedSideRms_ = resampleBallistics (smoothedSideRms_, oldNumBins, numBins);
        smoothedMonoRms_ = resampleBallistics (smoothedMonoRms_, oldNumBins, numBins);
    }
    else
    {
        smoothedMagnitude.resize (static_cast<size_t> (numBins), 0.0f);
        smoothedPeak.resize (static_cast<size_t> (numBins), 0.0f);
    }

    // Ensure correct size after resample
    smoothedMagnitude.resize (static_cast<size_t> (numBins), 0.0f);
    smoothedPeak.resize (static_cast<size_t> (numBins), 0.0f);

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

    // Multi-trace: Ensure RMS ballistics buffers are correctly sized
    // (resampleBallistics already handled interpolation if needed)
    smoothedLRms_.resize (numBinsSz, 0.0f);
    smoothedRRms_.resize (numBinsSz, 0.0f);
    smoothedMidRms_.resize (numBinsSz, 0.0f);
    smoothedSideRms_.resize (numBinsSz, 0.0f);
    smoothedMonoRms_.resize (numBinsSz, 0.0f);

    // Initialize window (Hann)
    const float pi = juce::MathConstants<float>::pi;
    for (int i = 0; i < fftSize; ++i)
    {
        const std::size_t idx = static_cast<std::size_t> (i);
        window[idx] = 0.5f * (1.0f - std::cos (2.0f * pi * static_cast<float> (i) / static_cast<float> (fftSize - 1)));
    }

    // Reset FIFO state (but preserve ballistics for smooth transitions)
    fifoWritePos = 0;
    samplesCollected = 0;
    std::fill (fifoBuffer.begin(), fifoBuffer.end(), 0.0f);
    std::fill (fftOutput.begin(), fftOutput.end(), 0.0f);
    // NOTE: smoothedMagnitude/smoothedPeak NOT reset here - preserved via resampleBallistics
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

    // Initialize RMS-processed multi-trace arrays to floor
    std::fill (stagingSnapshot_.fftDbLRms.begin(), stagingSnapshot_.fftDbLRms.end(), kDbFloor);
    std::fill (stagingSnapshot_.fftDbRRms.begin(), stagingSnapshot_.fftDbRRms.end(), kDbFloor);
    std::fill (stagingSnapshot_.fftDbMidRms.begin(), stagingSnapshot_.fftDbMidRms.end(), kDbFloor);
    std::fill (stagingSnapshot_.fftDbSideRms.begin(), stagingSnapshot_.fftDbSideRms.end(), kDbFloor);
    std::fill (stagingSnapshot_.fftDbMonoRms.begin(), stagingSnapshot_.fftDbMonoRms.end(), kDbFloor);
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
    // CLEANUP: DUPLICATE - Removed duplicate fifoBuffer.clear() call (line 173)
    // fifoBuffer.clear();
    smoothedMagnitude.clear();
    smoothedPeak.clear();
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

                // Apply frequency weighting to L/R power BEFORE smoothing
                rebuildWeightingGainTable();
                if (weightingMode_ != 0 && static_cast<int> (weightingGainTable_.size()) == numBins)
                {
                    for (int b = 0; b < numBins; ++b)
                    {
                        const auto idx = static_cast<size_t> (b);
                        powerL_[idx] *= weightingGainTable_[idx];
                        powerR_[idx] *= weightingGainTable_[idx];
                    }
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
    // Frequency Weighting - Applied to POWER (BEFORE smoothing)
    // -------------------------------------------------------------------------
    // Apply weighting in the power domain before octave smoothing so that
    // the smoothing averages weighted power, producing correct weighted+smoothed results.
    rebuildWeightingGainTable();
    if (weightingMode_ != 0 && static_cast<int> (weightingGainTable_.size()) == numBins)
    {
        for (int i = 0; i < numBins; ++i)
            magnitudes_[static_cast<size_t> (i)] *= weightingGainTable_[static_cast<size_t> (i)];
    }

    // -------------------------------------------------------------------------
    // Frequency Smoothing (Fractional Octave) - Applied to POWER
    // -------------------------------------------------------------------------
    // Option A (UISmoothingLogGaussian): Engine does NOT apply spectral smoothing.
    // UI applies Gaussian in convertFFTToLog over 256 log bins. Single source of truth.
    float* freqSmoothed = fftOutput.data();
    const bool engineAppliesSpectral = (kSpectralSmoothingStage != SpectralSmoothingStage::UISmoothingLogGaussian);

    if (!engineAppliesSpectral)
    {
        std::copy (magnitudes_.begin(), magnitudes_.begin() + numBins, freqSmoothed);
    }
    else if (smoothingOctaves_ > 0.0f && static_cast<int>(smoothLowBounds.size()) == numBins
             && static_cast<int>(prefixSumMag.size()) == numBins + 1)
    {
        prefixSumMag[0] = 0.0f;
        for (int i = 0; i < numBins; ++i)
            prefixSumMag[static_cast<std::size_t>(i + 1)] = prefixSumMag[static_cast<std::size_t>(i)] + magnitudes_[static_cast<std::size_t>(i)];
        for (int i = 0; i < numBins; ++i)
        {
            const int low = smoothLowBounds[static_cast<std::size_t>(i)];
            const int high = smoothHighBounds[static_cast<std::size_t>(i)];
            const int count = high - low + 1;
            if (count > 0)
                freqSmoothed[i] = (prefixSumMag[static_cast<std::size_t>(high + 1)] - prefixSumMag[static_cast<std::size_t>(low)]) / static_cast<float>(count);
            else
                freqSmoothed[i] = magnitudes_[static_cast<std::size_t>(i)];
        }
    }
    else
    {
        std::copy (magnitudes_.begin(), magnitudes_.begin() + numBins, freqSmoothed);
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

#if JUCE_DEBUG
    // Debug: Check input power values
    static int powerDebugCounter = 0;
    if ((++powerDebugCounter % 100) == 0)
    {
        float maxInput = freqSmoothed[100];
        float maxMag = magnitudes_[100];
        DBG ("POWER INPUT: freqSmoothed[100]=" << maxInput << " rawMag[100]=" << maxMag
             << " smoothedPeak[100]=" << smoothedPeak[100]);
    }
#endif

    for (int i = 0; i < numBins; ++i)
    {
        const std::size_t idx = static_cast<std::size_t> (i);
        const float inputPower = freqSmoothed[idx];

        // RMS Ballistics
        float& rmsState = smoothedMagnitude[idx];
        const float rmsCoeff = (inputPower > rmsState) ? rmsAttCoeff : rmsRelCoeff;
        rmsState = rmsCoeff * rmsState + (1.0f - rmsCoeff) * inputPower;

        // Peak Ballistics: envelope max with fast attack / release
        // NOTE: Only use main trace power for peak calculation to avoid multi-trace contamination
        float maxPower = inputPower;
        float& peakState = smoothedPeak[idx];
        const float peakCoeff = (maxPower > peakState) ? peakAttCoeff : peakRelCoeff;
        peakState = peakCoeff * peakState + (1.0f - peakCoeff) * maxPower;
    }

    // ============================================================================
    // MULTI-TRACE RMS BALLISTICS (Audio Thread)
    // Apply same ballistics to L/R/Mid/Side/Mono as main RMS trace
    // CRITICAL: Apply spectral smoothing to L/R first (same as main trace)
    // ============================================================================

    if (enableMultiTrace_)
    {
        if (!engineAppliesSpectral)
        {
            std::copy (powerL_.begin(), powerL_.begin() + numBins, smoothedL_.begin());
            std::copy (powerR_.begin(), powerR_.begin() + numBins, smoothedR_.begin());
        }
        else if (smoothingOctaves_ > 0.0f && static_cast<int>(smoothLowBounds.size()) == numBins
                 && static_cast<int>(prefixSumMag.size()) == numBins + 1)
        {
            prefixSumMag[0] = 0.0f;
            for (int i = 0; i < numBins; ++i)
                prefixSumMag[static_cast<size_t>(i + 1)] = prefixSumMag[static_cast<size_t>(i)] + powerL_[static_cast<size_t>(i)];
            for (int i = 0; i < numBins; ++i)
            {
                const int low = smoothLowBounds[static_cast<size_t>(i)];
                const int high = smoothHighBounds[static_cast<size_t>(i)];
                if (high - low + 1 > 0)
                    smoothedL_[static_cast<size_t>(i)] = (prefixSumMag[static_cast<size_t>(high + 1)] - prefixSumMag[static_cast<size_t>(low)]) / static_cast<float>(high - low + 1);
                else
                    smoothedL_[static_cast<size_t>(i)] = powerL_[static_cast<size_t>(i)];
            }
            prefixSumMag[0] = 0.0f;
            for (int i = 0; i < numBins; ++i)
                prefixSumMag[static_cast<size_t>(i + 1)] = prefixSumMag[static_cast<size_t>(i)] + powerR_[static_cast<size_t>(i)];
            for (int i = 0; i < numBins; ++i)
            {
                const int low = smoothLowBounds[static_cast<size_t>(i)];
                const int high = smoothHighBounds[static_cast<size_t>(i)];
                if (high - low + 1 > 0)
                    smoothedR_[static_cast<size_t>(i)] = (prefixSumMag[static_cast<size_t>(high + 1)] - prefixSumMag[static_cast<size_t>(low)]) / static_cast<float>(high - low + 1);
                else
                    smoothedR_[static_cast<size_t>(i)] = powerR_[static_cast<size_t>(i)];
            }
        }
        else
        {
            std::copy (powerL_.begin(), powerL_.begin() + numBins, smoothedL_.begin());
            std::copy (powerR_.begin(), powerR_.begin() + numBins, smoothedR_.begin());
        }

        // Apply RMS ballistics to L/R
        constexpr float kMinPower = 1.0e-20f;

        for (int i = 0; i < numBins; ++i)
        {
            const size_t idx = static_cast<size_t>(i);

            // L channel (using spectrally-smoothed input)
            const float inputPowerL = smoothedL_[idx];
            float& rmsStateL = smoothedLRms_[idx];
            const float coeffL = (inputPowerL > rmsStateL) ? rmsAttCoeff : rmsRelCoeff;
            rmsStateL = coeffL * rmsStateL + (1.0f - coeffL) * inputPowerL;

            // R channel (using spectrally-smoothed input)
            const float inputPowerR = smoothedR_[idx];
            float& rmsStateR = smoothedRRms_[idx];
            const float coeffR = (inputPowerR > rmsStateR) ? rmsAttCoeff : rmsRelCoeff;
            rmsStateR = coeffR * rmsStateR + (1.0f - coeffR) * inputPowerR;

            // Mid: derive from smoothed L/R (magnitude domain)
            const float magL = std::sqrt(std::max(rmsStateL, kMinPower));
            const float magR = std::sqrt(std::max(rmsStateR, kMinPower));
            const float magMid = 0.5f * (magL + magR);
            const float powerMid = magMid * magMid;

            float& rmsStateMid = smoothedMidRms_[idx];
            const float coeffMid = (powerMid > rmsStateMid) ? rmsAttCoeff : rmsRelCoeff;
            rmsStateMid = coeffMid * rmsStateMid + (1.0f - coeffMid) * powerMid;

            // Side: derive from smoothed L/R (magnitude domain)
            const float magSide = 0.5f * std::abs(magL - magR);
            const float powerSide = magSide * magSide;

            float& rmsStateSide = smoothedSideRms_[idx];
            const float coeffSide = (powerSide > rmsStateSide) ? rmsAttCoeff : rmsRelCoeff;
            rmsStateSide = coeffSide * rmsStateSide + (1.0f - coeffSide) * powerSide;

            // Mono: same as Mid
            smoothedMonoRms_[idx] = rmsStateMid;
        }
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

    // Ballistic Peak Trace: separate peak envelope (fast attack/release)
    convertToDb (smoothedPeak.data(), dbRaw_.data(), numBins);

    // Convert multi-trace RMS-smoothed power to dB (engine-side)
    if (enableMultiTrace_)
    {
        // Temporary buffers for dB conversion (reuse existing arrays in stagingSnapshot_)
        convertToDb (smoothedLRms_.data(), stagingSnapshot_.fftDbLRms.data(), numBins);
        convertToDb (smoothedRRms_.data(), stagingSnapshot_.fftDbRRms.data(), numBins);
        convertToDb (smoothedMidRms_.data(), stagingSnapshot_.fftDbMidRms.data(), numBins);
        convertToDb (smoothedSideRms_.data(), stagingSnapshot_.fftDbSideRms.data(), numBins);
        convertToDb (smoothedMonoRms_.data(), stagingSnapshot_.fftDbMonoRms.data(), numBins);

#if JUCE_DEBUG
        static int engineDebugCounter = 0;
        if (engineDebugCounter++ < 5)
        {
            DBG("AnalyzerEngine: Computed multi-trace RMS dB: L[0]=" << stagingSnapshot_.fftDbLRms[0]
                << " R[0]=" << stagingSnapshot_.fftDbRRms[0] << " Mid[0]=" << stagingSnapshot_.fftDbMidRms[0]
                << " numBins=" << numBins);
        }
#endif
    }

    // Peak Pipeline: Calculate Instantaneous dB from RAW magnitudes (no octave smoothing)
    // This ensures Peak latches the TRUE session max, independent from RMS smoothing.
    convertToDb (magnitudes_.data(), dbInstant_.data(), numBins);

#if JUCE_DEBUG
    // Quick debug: log first bin instant value every 50 frames
    static int instantDebugCounter = 0;
    if ((++instantDebugCounter % 50) == 0)
    {
        DBG ("INSTANT: bin[100]=" << dbInstant_[100] << " mag[100]=" << magnitudes_[100]);
    }
#endif

    // Update peak hold
    // Pass dbInstant_ for Latching, dbRaw_ for Release tracking
    updatePeakHold (dbInstant_.data(), dbRaw_.data(), peakHold.data(), numBins);

    // CRITICAL: numBins must equal expectedBins (fftSize/2 + 1)
    jassert (numBins == (currentFFTSize / 2 + 1));  // DEBUG assert: bin count must match FFT size

    // SANITIZATION (Fix 2: HF Spikes / NaN / Overflow protection)
    // Ensure no invalid values leak into the snapshot
    const std::size_t numBinsSz = static_cast<std::size_t> (numBins);
    for (std::size_t i = 0; i < numBinsSz; ++i)
    {
        // Clamp Peak Hold
        if (!std::isfinite(peakHold[i])) 
            peakHold[i] = kDbFloor;
        else
            peakHold[i] = juce::jlimit(kDbFloor, 18.0f, peakHold[i]);

        // Clamp Ballistic Peak (dbRaw_)
        if (!std::isfinite(dbRaw_[i])) 
            dbRaw_[i] = kDbFloor;
        else
            dbRaw_[i] = juce::jlimit(kDbFloor, 18.0f, dbRaw_[i]);
            
        // Clamp RMS (dbValues_)
        if (!std::isfinite(dbValues_[i])) 
            dbValues_[i] = kDbFloor;
        else
            dbValues_[i] = juce::jlimit(kDbFloor, 18.0f, dbValues_[i]);
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
    snapshot.weightingMode = weightingMode_;
    snapshot.engineDidSpectralSmooth = engineAppliesSpectral;
    snapshot.useUILogGaussianOnly = (kSpectralSmoothingStage == SpectralSmoothingStage::UISmoothingLogGaussian);
    snapshot.smoothingOctaves = smoothingOctaves_;
    
    // Safety guard: ensure numBins doesn't exceed array capacity
    jassert (numBins <= static_cast<int> (snapshot.fftDb.size()));
    jassert (numBins <= static_cast<int> (snapshot.fftPeakDb.size()));
    
    // Copy dB values to snapshot (using -200 dB internal floor)
    // CRITICAL: Use copyBins to prevent OOB writes
    // Use relaxed floor (-200 dB) to avoid hard clamping artifacts in smoothed traces.
    // The UI sanitizeDb and display range handle the visual floor.
    const int maxBins = static_cast<int> (snapshot.fftDb.size());
    const int copyBins = juce::jmin (numBins, maxBins);
    jassert (numBins <= static_cast<int> (AnalyzerSnapshot::kMaxFFTBins));  // Hard runtime check
    
    for (int i = 0; i < copyBins; ++i)
    {
        const std::size_t idx = static_cast<std::size_t> (i);
        snapshot.fftDb[idx] = juce::jmax (kDbFloor, dbValues_[idx]);

        // Peak trace is always the envelope: max(ballistic peak, RMS) so peak is never below other traces
        snapshot.fftPeakDb[idx] = juce::jmax (kDbFloor, dbRaw_[idx], dbValues_[idx]);

        // Populate snapshot with Peak Hold (AC1 - Existing Buffer)
        snapshot.fftPeakHoldDb[idx] = juce::jmax (kDbFloor, peakHold[idx]);
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
        float minDbRms = dbValues_[static_cast<std::size_t> (0)];
        float maxDbRms = dbValues_[static_cast<std::size_t> (0)];
        float minDbRaw = dbRaw_[static_cast<std::size_t> (0)];
        float maxDbRaw = dbRaw_[static_cast<std::size_t> (0)];
        float minPeakHold = peakHold[static_cast<std::size_t> (0)];
        float maxPeakHold = peakHold[static_cast<std::size_t> (0)];

        for (int i = 1; i < numBins; ++i)
        {
            const std::size_t idx = static_cast<std::size_t> (i);
            minDbRms = juce::jmin (minDbRms, dbValues_[idx]);
            maxDbRms = juce::jmax (maxDbRms, dbValues_[idx]);
            minDbRaw = juce::jmin (minDbRaw, dbRaw_[idx]);
            maxDbRaw = juce::jmax (maxDbRaw, dbRaw_[idx]);
            minPeakHold = juce::jmin (minPeakHold, peakHold[idx]);
            maxPeakHold = juce::jmax (maxPeakHold, peakHold[idx]);
        }
        DBG ("PEAK DEBUG: RMS[" << minDbRms << " to " << maxDbRms
             << "] RAW[" << minDbRaw << " to " << maxDbRaw
             << "] HOLD[" << minPeakHold << " to " << maxPeakHold << "]");

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
    // Use -200 dB internal floor (well below any display range) to avoid
    // hard clamping artifacts that make smoothed traces appear "squared".
    // The display/UI layer handles the visual floor at -120 dB or wherever the range is set.
    constexpr float dbFloor = -200.0f;
    constexpr float powerFloor = 1e-20f;  // 10*log10(1e-20) = -200dB

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

    // CRITICAL: Reset ballistic peak state (smoothedPeak feeds into yellow peak trace via dbRaw_)
    // Without this, the yellow peak trace stays at old max values even after "reset peaks"
    std::fill (smoothedPeak.begin(), smoothedPeak.end(), 0.0f);

    // Multi-trace Peak Reset
    std::fill (peakL_.begin(), peakL_.end(), kDbFloor);
    std::fill (peakR_.begin(), peakR_.end(), kDbFloor);
    std::fill (peakMono_.begin(), peakMono_.end(), kDbFloor);
    std::fill (peakMid_.begin(), peakMid_.end(), kDbFloor);
    std::fill (peakSide_.begin(), peakSide_.end(), kDbFloor);

    // Clear multi-trace RMS ballistics state (engine-side)
    std::fill (smoothedLRms_.begin(), smoothedLRms_.end(), 0.0f);
    std::fill (smoothedRRms_.begin(), smoothedRRms_.end(), 0.0f);
    std::fill (smoothedMidRms_.begin(), smoothedMidRms_.end(), 0.0f);
    std::fill (smoothedSideRms_.begin(), smoothedSideRms_.end(), 0.0f);
    std::fill (smoothedMonoRms_.begin(), smoothedMonoRms_.end(), 0.0f);
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

void AnalyzerEngine::setPeakDecayCurve (PeakDecayCurve curve)
{
    peakDecayCurve_ = curve;
}

void AnalyzerEngine::setPeakDecayTimeConstantSec (float seconds)
{
    peakDecayTimeConstantSec_ = juce::jlimit (0.01f, 10.0f, seconds);
}

void AnalyzerEngine::setReleaseTimeMs (float ms)
{
    const float clampedMs = juce::jlimit (100.0f, 5000.0f, ms);
    rmsReleaseMs_ = clampedMs;
    peakReleaseMs_ = clampedMs;
    const float seconds = clampedMs / 1000.0f;
    peakDecayDbPerSec = 60.0f / seconds;
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

void AnalyzerEngine::setWeightingMode (int mode)
{
    weightingMode_ = juce::jlimit (0, 2, mode);
}

void AnalyzerEngine::rebuildWeightingGainTable()
{
    if (weightingMode_ == lastWeightingMode_ &&
        currentFFTSize == lastWeightingFftSize_ &&
        std::abs (currentSampleRate - lastWeightingSampleRate_) < 0.1)
        return;

    lastWeightingMode_ = weightingMode_;
    lastWeightingFftSize_ = currentFFTSize;
    lastWeightingSampleRate_ = currentSampleRate;

    const int numBins = currentFFTSize / 2 + 1;
    weightingGainTable_.resize (static_cast<size_t> (numBins));

    if (weightingMode_ == 0)
    {
        // No weighting: fill with 1.0 (unity gain)
        std::fill (weightingGainTable_.begin(), weightingGainTable_.end(), 1.0f);
        return;
    }

    const float binWidthHz = static_cast<float> (currentSampleRate) / static_cast<float> (currentFFTSize);

    for (int i = 0; i < numBins; ++i)
    {
        float freq = static_cast<float> (i) * binWidthHz;
        if (freq < 1.0f) freq = 1.0f;

        float db = 0.0f;
        if (weightingMode_ == 1)
            db = getAWeightingDb (freq);
        else if (weightingMode_ == 2)
            db = getBS468WeightingDb (freq);

        // Convert dB to linear power gain (since we apply to power spectrum)
        // power_weighted = power * 10^(db/10)
        weightingGainTable_[static_cast<size_t> (i)] = std::pow (10.0f, db / 10.0f);
    }
}

float AnalyzerEngine::getAWeightingDb (float freqHz)
{
    const float f2 = freqHz * freqHz;
    const float f4 = f2 * f2;

    const float c1 = 12194.0f * 12194.0f;
    const float c2 = 20.6f * 20.6f;
    const float c3 = 107.7f * 107.7f;
    const float c4 = 737.9f * 737.9f;
    const float c5 = 12194.0f * 12194.0f;

    const float num = c1 * f4;
    const float den = (f2 + c2) * std::sqrt ((f2 + c3) * (f2 + c4)) * (f2 + c5);

    if (den == 0.0f) return -120.0f;

    float gain = num / den;
    return 20.0f * std::log10 (gain) + 2.0f;
}

float AnalyzerEngine::getBS468WeightingDb (float freqHz)
{
    const float f_kHz = freqHz / 1000.0f;
    const float f = f_kHz;

    const double a1 = 1.0458849;
    const double b2 = 1.6620626;
    const double c2 = 0.3181829;
    const double b3 = 0.5057538;
    const double c3 = 0.1691696;
    const double gainScale = 1.24633263;

    const double f2 = static_cast<double> (f * f);

    const double den1 = f2 + a1 * a1;
    const double term2_real = c2 - f2;
    const double term2_imag = b2 * f;
    const double den2 = term2_real * term2_real + term2_imag * term2_imag;

    const double term3_real = c3 - f2;
    const double term3_imag = b3 * f;
    const double den3 = term3_real * term3_real + term3_imag * term3_imag;

    const double den = den1 * den2 * den3;

    if (den == 0.0) return -120.0f;

    const double num = gainScale * f;
    const double magSq = (num * num) / den;

    return static_cast<float> (10.0 * std::log10 (magSq));
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

    // SEQLOCK PROTOCOL: Mark start of write with ODD sequence number
    // Odd = write in progress, Even = write complete
    // This allows readers to detect torn reads by checking if sequence is odd or changed
    const uint32_t currentSeq = published_.sequence.load (std::memory_order_relaxed);
    const uint32_t startSeq = (currentSeq == 0) ? 1 : ((currentSeq | 1u) + 2);  // Next odd number (or 1 if first)
    published_.sequence.store (startSeq, std::memory_order_release);
    std::atomic_thread_fence (std::memory_order_release);  // Ensure sequence visible before data

    // Copy ALL fields into published snapshot (deep copy arrays)
    published_.data.isValid = source.isValid;
    published_.data.fftBinCount = binCount;
    published_.data.numBins = source.numBins;
    published_.data.sampleRate = source.sampleRate;
    published_.data.fftSize = source.fftSize;
    published_.data.displayBottomDb = source.displayBottomDb;
    published_.data.displayTopDb = source.displayTopDb;
    published_.data.isHoldOn = source.isHoldOn;
    published_.data.engineDidSpectralSmooth = source.engineDidSpectralSmooth;
    published_.data.useUILogGaussianOnly = source.useUILogGaussianOnly;
    published_.data.smoothingOctaves = source.smoothingOctaves;

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
    
    // Multi-trace: Copy power domain arrays AND engine-side RMS dB arrays
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

            // Engine-side RMS-processed multi-trace dB arrays
            published_.data.fftDbLRms[idx] = source.fftDbLRms[idx];
            published_.data.fftDbRRms[idx] = source.fftDbRRms[idx];
            published_.data.fftDbMidRms[idx] = source.fftDbMidRms[idx];
            published_.data.fftDbSideRms[idx] = source.fftDbSideRms[idx];
            published_.data.fftDbMonoRms[idx] = source.fftDbMonoRms[idx];
        }
    }
    
    // SEQLOCK PROTOCOL: Mark end of write with EVEN sequence number
    // Odd = write in progress, Even = write complete
    std::atomic_thread_fence (std::memory_order_release);  // Ensure all data visible before sequence
    const uint32_t endSeq = published_.sequence.load (std::memory_order_relaxed) + 1;  // startSeq + 1 = even
    published_.sequence.store (endSeq, std::memory_order_release);

    // Set flag indicating new data is available (UI can peek without interrupting audio thread)
    hasNewData_.store (true, std::memory_order_release);
}

bool AnalyzerEngine::getLatestSnapshot (AnalyzerSnapshot& dest) const
{
    if (!prepared)
        return false;

    // SEQLOCK PROTOCOL: Retry loop to handle concurrent writes
    // Odd sequence = write in progress (skip and retry)
    // Even sequence = write complete (safe to read)
    // If sequence changes during copy, data may be torn - retry
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        // First read: get sequence (acquire barrier ensures we see published data)
        const uint32_t seq1 = published_.sequence.load (std::memory_order_acquire);

        if (seq1 == 0)
            return false;  // No data published yet

        // If sequence is ODD, a write is in progress - spin-wait briefly then retry
        if (seq1 & 1u)
            continue;
        
        // Copy published data into destination
        dest.isValid = published_.data.isValid;
        dest.fftBinCount = published_.data.fftBinCount;
        dest.numBins = published_.data.numBins;
        dest.sampleRate = published_.data.sampleRate;
        dest.fftSize = published_.data.fftSize;
        dest.displayBottomDb = published_.data.displayBottomDb;
        dest.displayTopDb = published_.data.displayTopDb;
        dest.isHoldOn = published_.data.isHoldOn;
        dest.engineDidSpectralSmooth = published_.data.engineDidSpectralSmooth;
        dest.useUILogGaussianOnly = published_.data.useUILogGaussianOnly;
        dest.smoothingOctaves = published_.data.smoothingOctaves;
        
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

        // Multi-trace: Copy power domain arrays and RMS-processed dB arrays
        dest.multiTraceEnabled = published_.data.multiTraceEnabled;
        if (dest.multiTraceEnabled)
        {
            for (int i = 0; i < copyBins; ++i)
            {
                const std::size_t idx = static_cast<std::size_t> (i);
                dest.powerL[idx] = published_.data.powerL[idx];
                dest.powerR[idx] = published_.data.powerR[idx];

                // Copy engine-side RMS-processed multi-trace dB arrays
                dest.fftDbLRms[idx] = published_.data.fftDbLRms[idx];
                dest.fftDbRRms[idx] = published_.data.fftDbRRms[idx];
                dest.fftDbMidRms[idx] = published_.data.fftDbMidRms[idx];
                dest.fftDbSideRms[idx] = published_.data.fftDbSideRms[idx];
                dest.fftDbMonoRms[idx] = published_.data.fftDbMonoRms[idx];
            }
        }
        
        // Second read to verify stability (with acquire fence to ensure all data was read)
        std::atomic_thread_fence (std::memory_order_acquire);
        const uint32_t seq2 = published_.sequence.load (std::memory_order_acquire);

        // Return true only if sequence didn't change during copy (stable read)
        // Also verify seq2 is even (write completed, not in-progress)
        if (seq1 == seq2 && (seq2 & 1u) == 0)
            return true;

        // Sequence changed during copy (torn read) - retry to catch next stable frame
    }

    // After retries, return false (sequence kept changing, likely high update rate)
    return false;
}

const float* AnalyzerEngine::getFFTData() const noexcept
{
    // Return pointer to published FFT data (thread-safe read)
    // UI should check hasNextDataBlock() before accessing
    return published_.data.fftDb.data();
}
