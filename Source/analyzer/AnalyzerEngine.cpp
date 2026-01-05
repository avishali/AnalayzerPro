#include "AnalyzerEngine.h"
#include <cmath>
#include <algorithm>

//==============================================================================
AnalyzerEngine::AnalyzerEngine()
    : currentFFTSize (2048), currentHopSize (512)
{
    // Buffers will be resized in prepare()
}

AnalyzerEngine::~AnalyzerEngine() = default;

void AnalyzerEngine::prepare (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    
    // Initialize FFT size (use currentFFTSize, default 2048)
    initializeFFT (currentFFTSize);
    
    // Reset published snapshot
    published_.sequence.store (0, std::memory_order_relaxed);
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
    fftOutput.resize (fftSize * 2, 0.0f);
    window.resize (fftSize, 1.0f);
    fifoBuffer.resize (fftSize, 0.0f);
    const int numBins = fftSize / 2 + 1;
    smoothedMagnitude.resize (static_cast<size_t> (numBins), 0.0f);
    peakHold.resize (static_cast<size_t> (numBins), kDbFloor);
    peakHoldCounter.resize (static_cast<size_t> (numBins), 0);
    
    // Resize per-frame computation buffers (eliminates allocations in computeFFT)
    magnitudes_.resize (static_cast<size_t> (numBins), 0.0f);
    dbValues_.resize (static_cast<size_t> (numBins), 0.0f);
    
    // Set publish throttling for large FFT sizes (8192 is expensive, reduce UI update rate)
    publishThrottleCounter_ = 0;
    publishThrottleDivisor_ = (fftSize >= 8192) ? 2 : 1;  // Publish every 2nd frame for 8192
    
    // Initialize window (Hann)
    const float pi = juce::MathConstants<float>::pi;
    for (int i = 0; i < fftSize; ++i)
    {
        window[i] = 0.5f * (1.0f - std::cos (2.0f * pi * i / (fftSize - 1)));
    }
    
    // Reset state
    fifoWritePos = 0;
    samplesCollected = 0;
    std::fill (fifoBuffer.begin(), fifoBuffer.end(), 0.0f);
    std::fill (fftOutput.begin(), fftOutput.end(), 0.0f);
    std::fill (smoothedMagnitude.begin(), smoothedMagnitude.end(), 0.0f);
    std::fill (peakHold.begin(), peakHold.end(), kDbFloor);
    std::fill (peakHoldCounter.begin(), peakHoldCounter.end(), 0);
    
    // Initialize published snapshot arrays to floor value
    constexpr float dbFloor = -120.0f;
    
    // Safety guard: ensure numBins doesn't exceed array capacity
    jassert (numBins <= static_cast<int> (published_.data.fftDb.size()));
    jassert (numBins <= static_cast<int> (published_.data.fftPeakDb.size()));
    
    // CRITICAL: Mark snapshot as invalid on FFT size change to prevent "blink to floor"
    // Only publish valid snapshots after first real FFT computation
    published_.data.isValid = false;
    published_.data.fftSize = fftSize;
    published_.data.numBins = numBins;
    
    // Initialize arrays safely using copyBins to prevent OOB
    const int maxBins = static_cast<int> (published_.data.fftDb.size());
    const int copyBins = juce::jmin (numBins, maxBins);
    std::fill (published_.data.fftDb.begin(), published_.data.fftDb.begin() + copyBins, dbFloor);
    std::fill (published_.data.fftPeakDb.begin(), published_.data.fftPeakDb.begin() + copyBins, dbFloor);
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
    peakHoldCounter.clear();
    magnitudes_.clear();
    dbValues_.clear();
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
        
        fifoBuffer[fifoWritePos] = sample;
        fifoWritePos = (fifoWritePos + 1) % currentFFTSize;
        samplesCollected++;
        
        // When we have enough samples, compute FFT
        if (samplesCollected >= currentHopSize)
        {
            samplesCollected = 0;
            computeFFT();
        }
    }
}

void AnalyzerEngine::computeFFT()
{
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
    magnitudes_[0] = std::abs (fftOutput[0]);  // DC component (real only)
    for (int i = 1; i < numBins - 1; ++i)
    {
        const float real = fftOutput[2 * i];
        const float imag = fftOutput[2 * i + 1];
        magnitudes_[i] = std::sqrt (real * real + imag * imag);
    }
    magnitudes_[numBins - 1] = std::abs (fftOutput[1]);  // Nyquist (real only, at index 1)
    
    // Normalize by FFT size
    const float scale = 1.0f / static_cast<float> (currentFFTSize);
    for (int i = 0; i < numBins; ++i)
    {
        magnitudes_[i] *= scale;
    }
    
    // Apply smoothing (EMA): smoothed = alpha * smoothed + (1 - alpha) * magnitude
    for (int i = 0; i < numBins; ++i)
    {
        smoothedMagnitude[i] = smoothingCoeff * smoothedMagnitude[i] + (1.0f - smoothingCoeff) * magnitudes_[i];
    }
    
    // Convert to dB (reuse member buffer, no allocation)
    convertToDb (smoothedMagnitude.data(), dbValues_.data(), numBins);
    
    // Update peak hold: always track new peaks, only decay when hold is disabled
    updatePeakHold (dbValues_.data(), peakHold.data(), peakHoldCounter.data(), numBins);
    
    // CRITICAL: numBins must equal expectedBins (fftSize/2 + 1)
    const int expectedBins = currentFFTSize / 2 + 1;
    jassert (numBins == expectedBins);  // DEBUG assert: bin count must match FFT size
    
    // Build local snapshot with computed FFT data
    AnalyzerSnapshot snapshot;
    snapshot.numBins = numBins;
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
        snapshot.fftDb[i] = juce::jmax (dbFloor, dbValues_[i]);
        snapshot.fftPeakDb[i] = juce::jmax (dbFloor, peakHold[i]);
    }
    
#if JUCE_DEBUG
    // DEBUG: Log FFT data range once per second (throttled)
    static uint32_t debugLogCounter = 0;
    if ((++debugLogCounter % 100) == 0)  // Approx once per second at 48kHz/512 samples
    {
        float minDb = dbValues_[0];
        float maxDb = dbValues_[0];
        for (int i = 1; i < numBins; ++i)
        {
            minDb = juce::jmin (minDb, dbValues_[i]);
            maxDb = juce::jmax (maxDb, dbValues_[i]);
        }
        DBG ("FFT bins=" << numBins << " minDb=" << minDb << " maxDb=" << maxDb
             << " fftSize=" << currentFFTSize << " hop=" << currentHopSize
             << " coeff=" << smoothingCoeff);
        
        // Assert bin count consistency
        jassert (numBins <= static_cast<int> (AnalyzerSnapshot::kMaxFFTBins));
    }
#endif
    
    // Publish snapshot with throttling for large FFT sizes (audio thread, lock-free)
    // For 8192, publish every 2nd frame to reduce UI update load and prevent jitter
    publishThrottleCounter_++;
    if (publishThrottleCounter_ >= publishThrottleDivisor_)
    {
        publishThrottleCounter_ = 0;
        publishSnapshot (snapshot);
    }
}

void AnalyzerEngine::applyWindow()
{
    // Write windowed time-domain samples into the SAME buffer we pass to JUCE FFT.
    // JUCE real-only FFT expects first N floats to contain the real samples.
    for (int i = 0; i < currentFFTSize; ++i)
    {
        const int fifoIndex = (fifoWritePos + i) % currentFFTSize;
        fftOutput[(size_t) i] = fifoBuffer[(size_t) fifoIndex] * window[(size_t) i];
    }

    // Zero-pad the remainder (JUCE uses this buffer in-place for output too)
    std::fill (fftOutput.begin() + (size_t) currentFFTSize, fftOutput.end(), 0.0f);
}

void AnalyzerEngine::convertToDb (const float* magnitudes, float* dbOut, int numBins)
{
    // Convert magnitude to dB with floor
    // Use -120.0f floor for consistency (matches snapshot clamping)
    constexpr float dbFloor = -120.0f;
    constexpr float magFloor = 1e-8f;  // Avoid log(0)
    
    for (int i = 0; i < numBins; ++i)
    {
        const float mag = juce::jmax (magFloor, magnitudes[i]);
        const float db = 20.0f * std::log10 (mag);
        dbOut[i] = juce::jmax (dbFloor, db);
    }
}

void AnalyzerEngine::updatePeakHold (const float* dbIn, float* peakOut, int* counters, int numBins)
{
    // Calculate decay per frame (for audio thread: decay per FFT frame = (decayDbPerSec * hopSize) / sampleRate)
    const float decayPerFrame = (peakDecayDbPerSec * static_cast<float> (currentHopSize)) / static_cast<float> (currentSampleRate);
    
    for (int i = 0; i < numBins; ++i)
    {
        // Always update to new louder peaks (peak hold tracks maxima)
        if (dbIn[i] > peakOut[i])
        {
            peakOut[i] = dbIn[i];
        }
        
        // Only decay when hold is disabled
        if (!holdEnabled)
        {
            peakOut[i] = juce::jmax (kDbFloor, peakOut[i] - decayPerFrame);
        }
    }
    
    juce::ignoreUnused (counters);  // Counters not used in current implementation
}

void AnalyzerEngine::setFftSize (int fftSize)
{
    // Validate FFT size (must be power of 2, within range)
    // Clamp minimum to 2048 for meaningful sub-50 Hz resolution
    int validSize = 2048;
    if (fftSize == 2048 || fftSize == 4096 || fftSize == 8192)
        validSize = fftSize;
    else if (fftSize == 1024)
    {
        // Allow 1024 but acknowledge low-end limitation (expected physics, not a bug)
        validSize = 1024;
#if JUCE_DEBUG
        static bool logged1024 = false;
        if (!logged1024)
        {
            const double binWidthHz = (currentSampleRate > 0.0) ? (currentSampleRate / 1024.0) : 46.875;
            DBG ("FFT 1024 @" << currentSampleRate << "Hz => bin width ~" << binWidthHz 
                 << " Hz; low-end resolution limited (expected physics).");
            logged1024 = true;
        }
#endif
    }
    else if (fftSize < 1024)
        validSize = 2048;  // Clamp to minimum
    else if (fftSize > 8192)
        validSize = 8192;
    else
    {
        // Round to nearest power of 2
        validSize = 1 << static_cast<int> (std::ceil (std::log2 (fftSize)));
        validSize = juce::jlimit (2048, 8192, validSize);  // Clamp to 2048 minimum
    }
    
    if (validSize != currentFFTSize && prepared)
    {
        // Re-initialize FFT (this is safe to call from UI thread, but will affect next processBlock)
        // For safety, we'll do this on next prepare() or use a pending flag
        // For now, just update if not processing
        initializeFFT (validSize);
        // Update smoothing coefficient after FFT size changes (hop size changes)
        updateSmoothingCoeff (averagingMs_, currentSampleRate);
    }
    
    // CRITICAL: Resize published snapshot FFT buffers when FFT size changes
    // This ensures processBlock() can write data to correctly-sized buffers
    if (prepared)
    {
        const int numBins = validSize / 2 + 1;
        constexpr float dbFloor = -120.0f;
        
        // Safety guard: ensure numBins doesn't exceed array capacity
        jassert (numBins <= static_cast<int> (published_.data.fftDb.size()));
        jassert (numBins <= static_cast<int> (published_.data.fftPeakDb.size()));
        
        // CRITICAL: Mark snapshot as invalid on FFT size change to prevent "blink to floor"
        // Only publish valid snapshots after first real FFT computation
        published_.data.isValid = false;
        published_.data.fftSize = validSize;
        published_.data.sampleRate = currentSampleRate;
        published_.data.numBins = numBins;
        
        // Initialize arrays safely using copyBins to prevent OOB
        const int maxBins = static_cast<int> (published_.data.fftDb.size());
        const int copyBins = juce::jmin (numBins, maxBins);
        std::fill (published_.data.fftDb.begin(), published_.data.fftDb.begin() + copyBins, dbFloor);
        std::fill (published_.data.fftPeakDb.begin(), published_.data.fftPeakDb.begin() + copyBins, dbFloor);
    }
}

void AnalyzerEngine::setAveragingMs (float averagingMs)
{
    averagingMs_ = averagingMs;
    updateSmoothingCoeff (averagingMs, currentSampleRate);
}

void AnalyzerEngine::setHold (bool hold)
{
    holdEnabled = hold;
}

void AnalyzerEngine::setPeakDecayDbPerSec (float decayDbPerSec)
{
    peakDecayDbPerSec = juce::jlimit (0.0f, 10.0f, decayDbPerSec);
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
        smoothingCoeff = juce::jlimit (0.0f, 0.999f, smoothingCoeff);  // Clamp to valid range
    }
    else
    {
        smoothingCoeff = 0.0f;  // No smoothing (instantaneous response)
    }
}

void AnalyzerEngine::publishSnapshot (const AnalyzerSnapshot& source)
{
    // Copy ALL fields into published snapshot (deep copy arrays)
    published_.data.isValid = source.isValid;
    published_.data.numBins = source.numBins;
    published_.data.sampleRate = source.sampleRate;
    published_.data.fftSize = source.fftSize;
    published_.data.displayBottomDb = source.displayBottomDb;
    published_.data.displayTopDb = source.displayTopDb;
    
    // Deep copy arrays (fixed-size, but copy only used portion)
    const int numBins = source.numBins;
    
    // Safety guard: ensure numBins doesn't exceed array capacity
    jassert (numBins <= static_cast<int> (published_.data.fftDb.size()));
    jassert (numBins <= static_cast<int> (published_.data.fftPeakDb.size()));
    jassert (numBins <= static_cast<int> (source.fftDb.size()));
    jassert (numBins <= static_cast<int> (source.fftPeakDb.size()));
    
    const int copyBins = juce::jmin (numBins, static_cast<int> (published_.data.fftDb.size()));
    for (int i = 0; i < copyBins; ++i)
    {
        published_.data.fftDb[i] = source.fftDb[i];
        published_.data.fftPeakDb[i] = source.fftPeakDb[i];
    }
    
    // Increment sequence AFTER data copy completes (release fence ensures visibility)
    const uint32_t next = published_.sequence.load (std::memory_order_relaxed) + 1;
    published_.sequence.store (next, std::memory_order_release);
}

bool AnalyzerEngine::getLatestSnapshot (AnalyzerSnapshot& dest) const
{
    if (!prepared)
        return false;
    
    // Two-read stability check (lock-free, avoids torn reads)
    const uint32_t seq1 = published_.sequence.load (std::memory_order_acquire);
    
    if (seq1 == 0)
        return false;  // No data published yet
    
    // Copy published data into destination
    dest.isValid = published_.data.isValid;
    dest.numBins = published_.data.numBins;
    dest.sampleRate = published_.data.sampleRate;
    dest.fftSize = published_.data.fftSize;
    dest.displayBottomDb = published_.data.displayBottomDb;
    dest.displayTopDb = published_.data.displayTopDb;
    
    // Deep copy arrays
    const int numBins = published_.data.numBins;
    
    // Safety guard: ensure numBins doesn't exceed array capacity
    jassert (numBins <= static_cast<int> (dest.fftDb.size()));
    jassert (numBins <= static_cast<int> (dest.fftPeakDb.size()));
    jassert (numBins <= static_cast<int> (published_.data.fftDb.size()));
    jassert (numBins <= static_cast<int> (published_.data.fftPeakDb.size()));
    
    const int copyBins = juce::jmin (numBins, static_cast<int> (dest.fftDb.size()));
    for (int i = 0; i < copyBins; ++i)
    {
        dest.fftDb[i] = published_.data.fftDb[i];
        dest.fftPeakDb[i] = published_.data.fftPeakDb[i];
    }
    
    // Second read to verify stability
    const uint32_t seq2 = published_.sequence.load (std::memory_order_acquire);
    
    // Return true only if sequence didn't change during copy (stable read)
    if (seq1 == seq2 && seq1 != 0)
        return true;
    
    // Sequence changed during copy (torn read), caller can try again next timer tick
    return false;
}
