#include "AnalyzerEngine.h"
#include <cmath>
#include <algorithm>
#include <juce_events/juce_events.h>

//==============================================================================
AnalyzerEngine::AnalyzerEngine()
    : currentFFTSize (2048), currentHopSize (512)
{
    // Buffers will be resized in prepare()
}

AnalyzerEngine::~AnalyzerEngine() = default;

void AnalyzerEngine::prepare (double sampleRate, int /* samplesPerBlock */)
{
    currentSampleRate = sampleRate;
    
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
    averagingMs_ = 100.0f;  // Default 100ms
    updateSmoothingCoeff (averagingMs_, sampleRate);
    
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
    peakHold.resize (static_cast<size_t> (numBins), kDbFloor);

    peakHoldFramesRemaining_.resize (static_cast<size_t> (numBins), 0);
    
    // Resize per-frame computation buffers (eliminates allocations in computeFFT)
    magnitudes_.resize (static_cast<size_t> (numBins), 0.0f);
    dbValues_.resize (static_cast<size_t> (numBins), 0.0f);
    dbRaw_.resize (static_cast<size_t> (numBins), 0.0f);
    
    
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
    smoothedMagnitude.clear();
    peakHold.clear();
    magnitudes_.clear();
    dbValues_.clear();
    dbRaw_.clear();
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
        // Sum channels if stereo
        float sample = inputChannel[i];
        if (numChannels > 1)
        {
            sample = (sample + buffer.getReadPointer (1)[i]) * 0.5f;
        }
        
        const std::size_t fifoIdx = static_cast<std::size_t> (fifoWritePos);
        fifoBuffer[fifoIdx] = sample;
        fifoWritePos = (fifoWritePos + 1) % currentFFTSize;
        samplesCollected++;
        
        // When we have enough samples, compute FFT
        if (samplesCollected >= currentHopSize)
        {
            samplesCollected = 0;
            computeFFT();
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
    applyWindow();
    
    // Perform real-only forward FFT (fftOutput now contains valid time-domain samples)
    fft->performRealOnlyForwardTransform (fftOutput.data(), false);
    
    // Compute magnitudes and convert to dB
    const int numBins = currentFFTSize / 2 + 1;
    
    // Compute magnitudes from complex FFT output (reuse member buffer, no allocation)
    // Real-only FFT output format: [DC, Nyquist, real1, imag1, real2, imag2, ...]
    // buffer[0] = DC, buffer[1] = Nyquist, buffer[2*i] = real, buffer[2*i+1] = imag
    magnitudes_[static_cast<std::size_t> (0)] = std::abs (fftOutput[static_cast<std::size_t> (0)]);  // DC component (real only)
    for (int i = 1; i < numBins - 1; ++i)
    {
        const std::size_t idx = static_cast<std::size_t> (i);
        const std::size_t fftIdx1 = static_cast<std::size_t> (2 * i);
        const std::size_t fftIdx2 = static_cast<std::size_t> (2 * i + 1);
        const float real = fftOutput[fftIdx1];
        const float imag = fftOutput[fftIdx2];
        magnitudes_[idx] = std::sqrt (real * real + imag * imag);
    }
    const std::size_t nyquistIdx = static_cast<std::size_t> (numBins - 1);
    magnitudes_[nyquistIdx] = std::abs (fftOutput[static_cast<std::size_t> (1)]);  // Nyquist (real only, at index 1)
    
    // Normalize by FFT size
    const float scale = 1.0f / static_cast<float> (currentFFTSize);
    for (int i = 0; i < numBins; ++i)
    {
        const std::size_t idx = static_cast<std::size_t> (i);
        magnitudes_[idx] *= scale;
    }
    
    // Apply smoothing (EMA): smoothed = alpha * smoothed + (1 - alpha) * magnitude
    for (int i = 0; i < numBins; ++i)
    {
        const std::size_t idx = static_cast<std::size_t> (i);
        smoothedMagnitude[idx] = smoothingCoeff * smoothedMagnitude[idx] + (1.0f - smoothingCoeff) * magnitudes_[idx];
    }
    
    // Convert smoothed magnitude to dB for display (reuse member buffer, no allocation)
    convertToDb (smoothedMagnitude.data(), dbValues_.data(), numBins);
    
    // Convert raw (unsmoothed) magnitude to dB for peak tracking (reuse member buffer, no allocation)
    // Peaks should track raw FFT, not smoothed display
    convertToDb (magnitudes_.data(), dbRaw_.data(), numBins);
    
    // Update peak hold: tracks raw dB, allows freeze
    updatePeakHold (dbRaw_.data(), peakHold.data(), numBins);
    
    // CRITICAL: numBins must equal expectedBins (fftSize/2 + 1)
    const int expectedBins = currentFFTSize / 2 + 1;
    jassert (numBins == expectedBins);  // DEBUG assert: bin count must match FFT size
    
    // Build local snapshot with computed FFT data
    AnalyzerSnapshot snapshot;
    snapshot.fftBinCount = numBins;
    // Legacy/compat: numBins is reserved for non-FFT series. FFT bin count lives in fftBinCount.
    snapshot.numBins = 0;
    snapshot.sampleRate = currentSampleRate;
    snapshot.fftSize = currentFFTSize;
    snapshot.displayBottomDb = -120.0f;
    snapshot.displayTopDb = 0.0f;
    snapshot.isValid = true;
    
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
        snapshot.fftPeakDb[idx] = juce::jmax (dbFloor, peakHold[idx]);
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
        DBG ("FFT bins=" << numBins << " minDb=" << minDb << " maxDb=" << maxDb
             << " fftSize=" << currentFFTSize << " hop=" << currentHopSize
             << " coeff=" << smoothingCoeff);
        
        // Assert bin count consistency
        jassert (numBins <= static_cast<int> (AnalyzerSnapshot::kMaxFFTBins));
    }
#endif
    
    // Publish snapshot (audio thread, lock-free)
    // Throttling temporarily disabled to restore smoothness - publish every frame
    // UI load issues should be addressed by reducing UI timer rate if needed
    publishSnapshot (snapshot);
}

void AnalyzerEngine::applyWindow()
{
    // Write windowed time-domain samples into the SAME buffer we pass to JUCE FFT.
    // JUCE real-only FFT expects first N floats to contain the real samples.
    for (int i = 0; i < currentFFTSize; ++i)
    {
        const int fifoIndex = (fifoWritePos + i) % currentFFTSize;
        const std::size_t idx = static_cast<std::size_t> (i);
        const std::size_t fifoIdx = static_cast<std::size_t> (fifoIndex);
        fftOutput[idx] = fifoBuffer[fifoIdx] * window[idx];
    }

    // Zero-pad the remainder (JUCE uses this buffer in-place for output too)
    const std::size_t fftSizeSz = static_cast<std::size_t> (currentFFTSize);
    std::fill (fftOutput.begin() + static_cast<std::ptrdiff_t> (fftSizeSz), fftOutput.end(), 0.0f);
}

void AnalyzerEngine::convertToDb (const float* magnitudes, float* dbOut, int numBins)
{
    // Convert magnitude to dB with floor
    // Use -120.0f floor for consistency (matches snapshot clamping)
    constexpr float dbFloor = -120.0f;
    constexpr float magFloor = 1e-8f;  // Avoid log(0)
    
    for (int i = 0; i < numBins; ++i)
    {
        const std::size_t idx = static_cast<std::size_t> (i);
        const float mag = juce::jmax (magFloor, magnitudes[idx]);
        const float db = 20.0f * std::log10 (mag);
        dbOut[i] = juce::jmax (dbFloor, db);
    }
}

void AnalyzerEngine::updatePeakHold (const float* dbRawIn, float* peakOut, int numBins)
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

    for (int i = 0; i < numBins; ++i)
    {
        const std::size_t idx = static_cast<std::size_t> (i);

        // Always update to new louder peaks
        if (dbRawIn[idx] > peakOut[idx])
        {
            peakOut[idx] = dbRawIn[idx];

            if (peakHoldMode_ == PeakHoldMode::HoldThenDecay && holdFramesTotal > 0)
                peakHoldFramesRemaining_[idx] = holdFramesTotal;
        }

        // User “Hold” checkbox: freeze the peak trace completely (no decay, no timer countdown).
        if (freezePeaks_)
            continue;

        switch (peakHoldMode_)
        {
            case PeakHoldMode::Infinite:
                // No decay
                break;

            case PeakHoldMode::Decay:
                // Continuous decay
                peakOut[idx] = juce::jmax (kDbFloor, peakOut[idx] - decayPerFrame);
                break;

            case PeakHoldMode::HoldThenDecay:
            {
                // Sticky hold for N frames, then decay
                int& frames = peakHoldFramesRemaining_[idx];
                if (frames > 0)
                {
                    --frames;
                }
                else
                {
                    peakOut[idx] = juce::jmax (kDbFloor, peakOut[idx] - decayPerFrame);
                }
                break;
            }

            case PeakHoldMode::Off:
            default:
                // Already handled above
                break;
        }
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
    updateSmoothingCoeff (averagingMs_, currentSampleRate);

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
    averagingMs_ = averagingMs;
    updateSmoothingCoeff (averagingMs, currentSampleRate);
}

void AnalyzerEngine::setPeakHoldEnabled (bool enabled)
{
    peakHoldEnabled_ = enabled;

    if (! enabled)
    {
        peakHoldMode_ = PeakHoldMode::Off;
        resetPeaks();
        return;
    }

    // If enabling from Off, restore a sensible default mode.
    if (peakHoldMode_ == PeakHoldMode::Off)
        peakHoldMode_ = PeakHoldMode::HoldThenDecay;
}

void AnalyzerEngine::resetPeaks()
{
    std::fill (peakHold.begin(), peakHold.end(), kDbFloor);
    std::fill (peakHoldFramesRemaining_.begin(), peakHoldFramesRemaining_.end(), 0);
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
    freezePeaks_ = hold;  // Freeze means no decay
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

void AnalyzerEngine::updateSmoothingCoeff (float averagingMs, double sampleRate)
{
    // Compute EMA retention coefficient: coeff = exp(-hopSec / tauSec)
    // tauSec = averaging time in seconds, hopSec = hop time in seconds
    if (averagingMs > 0.0f && sampleRate > 0.0 && currentHopSize > 0)
    {
        const double tauSec = juce::jmax (1.0, static_cast<double> (averagingMs)) / 1000.0;  // Clamp min 1ms
        const double hopSec = static_cast<double> (currentHopSize) / sampleRate;
        smoothingCoeff = static_cast<float> (std::exp (-hopSec / tauSec));
        smoothingCoeff = juce::jlimit (0.0f, 0.995f, smoothingCoeff);  // Clamp to [0.0, 0.995] (avoid stuck smoothing)
    }
    else
    {
        smoothingCoeff = 0.0f;  // No smoothing (instantaneous response) when averagingMs <= 0
    }
}

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
    published_.data.displayTopDb = source.displayTopDb;
    
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
        dest.displayTopDb = published_.data.displayTopDb;
        
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
