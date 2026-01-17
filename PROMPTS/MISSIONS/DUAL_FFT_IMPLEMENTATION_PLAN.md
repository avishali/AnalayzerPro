# DUAL-FFT IMPLEMENTATION PLAN - COMPLETE

## Mission: MULTI_TRACE_DUALFFT_V1
## Status: IN PROGRESS (Step 2 partial)

## What's Been Done

### ✅ Completed
1. **AnalyzerSnapshot.h** - Expanded with 5 spectra arrays
   - Added: `fftDbL`, `fftDbR`, `fftDbMono`, `fftDbMid`, `fftDbSide`
   - Added: Peak versions for each
   - Kept legacy `fftDb`/`fftPeakDb` for backward compatibility

2. **AnalyzerEngine.h** - Added storage buffers
   - Added: `fftOutputL`, `fftOutputR` (complex FFT outputs)
   - Added: `powerL_`, `powerR_`, `powerMono_`, `powerMid_`, `powerSide_` (power spectra)
   - Added: Smoothed and peak versions for each trace

## What Remains

### Step 2A: AnalyzerEngine.cpp - initializeFFT() [~50 lines]

**Purpose**: Preallocate all new buffers (no audio-thread allocations)

**Changes Required**:
```cpp
void AnalyzerEngine::initializeFFT(int fftSize)
{
    // ... existing code ...
    
    // NEW: Preallocate dual-FFT buffers
    const size_t fftBufSize = static_cast<size_t>(fftSize * 2);
    fftOutputL.resize(fftBufSize, 0.0f);
    fftOutputR.resize(fftBufSize, 0.0f);
    
    // NEW: Preallocate power spectrum buffers (one per trace)
    const size_t numBins = static_cast<size_t>(fftSize / 2 + 1);
    powerL_.resize(numBins, 0.0f);
    powerR_.resize(numBins, 0.0f);
    powerMono_.resize(numBins, 0.0f);
    powerMid_.resize(numBins, 0.0f);
    powerSide_.resize(numBins, 0.0f);
    
    // NEW: Preallocate smoothed buffers
    smoothedL_.resize(numBins, 0.0f);
    smoothedR_.resize(numBins, 0.0f);
    smoothedMono_.resize(numBins, 0.0f);
    smoothedMid_.resize(numBins, 0.0f);
    smoothedSide_.resize(numBins, 0.0f);
    
    // NEW: Preallocate peak buffers
    peakL_.resize(numBins, kDbFloor);
    peakR_.resize(numBins, kDbFloor);
    peakMono_.resize(numBins, kDbFloor);
    peakMid_.resize(numBins, kDbFloor);
    peakSide_.resize(numBins, kDbFloor);
}
```

**Risk**: Low - straightforward buffer allocation

---

### Step 2B: AnalyzerEngine.cpp - computeFFT() Rewrite [~250 lines]

**Purpose**: Compute dual-channel FFT and derive all 5 spectra

**Current Flow**:
```
Single channel → FFT → Power → Smoothing → Ballistics → dB → Snapshot
```

**New Flow**:
```
L channel → FFT_L → Complex_L ┐
                                ├→ Derive Mono/Mid/Side → Power (×5) → Smoothing (×5) → Ballistics (×5) → dB (×5) → Snapshot
R channel → FFT_R → Complex_R ┘
```

**Detailed Changes**:

#### 2B.1: Dual FFT Computation [~40 lines]
```cpp
void AnalyzerEngine::computeFFT()
{
    // ... existing resize check ...
    
    const int numBins = currentFFTSize / 2 + 1;
    
    // Process LEFT channel
    applyWindow(0);  // Apply window to channel 0
    fft->performRealOnlyForwardTransform(fftOutputL.data(), false);
    
    // Process RIGHT channel  
    applyWindow(1);  // Apply window to channel 1
    fft->performRealOnlyForwardTransform(fftOutputR.data(), false);
    
    // ... continue to derive spectra ...
}
```

**Issue**: Current `applyWindow()` doesn't take channel parameter. Need to modify or duplicate.

#### 2B.2: Derive Complex Spectra [~60 lines]
```cpp
// Derive Mono/Mid/Side from complex bins
for (int i = 0; i < numBins; ++i)
{
    // Extract complex values from L and R
    float realL, imagL, realR, imagR;
    extractComplexBin(fftOutputL.data(), i, numBins, realL, imagL);
    extractComplexBin(fftOutputR.data(), i, numBins, realR, imagR);
    
    // Mono/Mid = (L + R) / 2
    float realMono = 0.5f * (realL + realR);
    float imagMono = 0.5f * (imagL + imagR);
    
    // Side = (L - R) / 2
    float realSide = 0.5f * (realL - realR);
    float imagSide = 0.5f * (imagL - imagR);
    
    // Convert to power
    powerL_[i] = realL * realL + imagL * imagL;
    powerR_[i] = realR * realR + imagR * imagR;
    powerMono_[i] = realMono * realMono + imagMono * imagMono;
    powerMid_[i] = powerMono_[i];  // Same as Mono
    powerSide_[i] = realSide * realSide + imagSide * imagSide;
    
    // Apply FFT normalization
    const float scale = 2.0f / currentFFTSize;
    const float powerScale = (scale * scale) * 4.0f; // Hann window correction
    
    powerL_[i] *= powerScale;
    powerR_[i] *= powerScale;
    powerMono_[i] *= powerScale;
    powerMid_[i] *= powerScale;
    powerSide_[i] *= powerScale;
}

// Correct DC and Nyquist
powerL_[0] *= 0.25f;
powerR_[0] *= 0.25f;
// ... etc for all traces
```

#### 2B.3: Apply Smoothing to Each Trace [~50 lines]
```cpp
// Apply frequency smoothing to each power spectrum
applyFrequencySmoothing(powerL_.data(), smoothedL_.data(), numBins);
applyFrequencySmoothing(powerR_.data(), smoothedR_.data(), numBins);
applyFrequencySmoothing(powerMono_.data(), smoothedMono_.data(), numBins);
applyFrequencySmoothing(powerMid_.data(), smoothedMid_.data(), numBins);
applyFrequencySmoothing(powerSide_.data(), smoothedSide_.data(), numBins);
```

**Issue**: Need to extract smoothing logic into helper function (currently inline).

#### 2B.4: Apply Ballistics to Each Trace [~50 lines]
```cpp
// Apply RMS/Peak ballistics to each trace
applyBallistics(smoothedL_.data(), peakL_.data(), numBins);
applyBallistics(smoothedR_.data(), peakR_.data(), numBins);
applyBallistics(smoothedMono_.data(), peakMono_.data(), numBins);
applyBallistics(smoothedMid_.data(), peakMid_.data(), numBins);
applyBallistics(smoothedSide_.data(), peakSide_.data(), numBins);
```

**Issue**: Need to extract ballistics logic into helper function.

#### 2B.5: Convert to dB and Populate Snapshot [~50 lines]
```cpp
// Convert each trace to dB
convertToDb(smoothedL_.data(), snapshot.fftDbL.data(), numBins);
convertToDb(smoothedR_.data(), snapshot.fftDbR.data(), numBins);
convertToDb(smoothedMono_.data(), snapshot.fftDbMono.data(), numBins);
convertToDb(smoothedMid_.data(), snapshot.fftDbMid.data(), numBins);
convertToDb(smoothedSide_.data(), snapshot.fftDbSide.data(), numBins);

// Convert peaks
convertToDb(peakL_.data(), snapshot.fftPeakDbL.data(), numBins);
convertToDb(peakR_.data(), snapshot.fftPeakDbR.data(), numBins);
convertToDb(peakMono_.data(), snapshot.fftPeakDbMono.data(), numBins);
convertToDb(peakMid_.data(), snapshot.fftPeakDbMid.data(), numBins);
convertToDb(peakSide_.data(), snapshot.fftPeakDbSide.data(), numBins);

// Populate legacy arrays with Mono for backward compatibility
std::copy(snapshot.fftDbMono.begin(), snapshot.fftDbMono.begin() + numBins, snapshot.fftDb.begin());
std::copy(snapshot.fftPeakDbMono.begin(), snapshot.fftPeakDbMono.begin() + numBins, snapshot.fftPeakDb.begin());
```

**Risk**: High - This is a complete rewrite of core FFT logic. Must preserve existing behavior for Mono.

---

### Step 2C: AnalyzerEngine.cpp - Helper Functions [~80 lines]

**New Functions Needed**:

```cpp
// Extract complex bin from real-only FFT output
void extractComplexBin(const float* fftOutput, int binIndex, int numBins, float& real, float& imag)
{
    if (binIndex == 0) {
        real = fftOutput[0];
        imag = 0.0f;
    } else if (binIndex == numBins - 1) {
        real = fftOutput[1];
        imag = 0.0f;
    } else {
        real = fftOutput[2 * binIndex];
        imag = fftOutput[2 * binIndex + 1];
    }
}

// Apply frequency smoothing to a power spectrum
void applyFrequencySmoothing(const float* input, float* output, int numBins)
{
    // Extract existing smoothing logic
    // ...
}

// Apply ballistics (RMS/Peak) to a power spectrum
void applyBallistics(float* smoothed, float* peak, int numBins)
{
    // Extract existing ballistics logic
    // ...
}
```

**Risk**: Medium - Need to carefully extract and refactor existing logic.

---

### Step 2D: PluginProcessor.cpp - Input Routing [~20 lines]

**Current**: Processor transforms `analysisBuffer` to single channel before feeding analyzer

**Required**: Feed stereo (L/R) to analyzer, let analyzer handle derivations

**Changes**:
```cpp
// BEFORE (lines 292-350):
// Apply Mode derived from Trace Config
// ... channel mode transform logic ...
analyzerEngine.processBlock(analysisBuffer);

// AFTER:
// Feed untransformed stereo to analyzer
// Analyzer will compute all 5 traces internally
analyzerEngine.processBlock(analysisBuffer);  // Just pass stereo through
```

**Issue**: This removes the channel mode transform from processor. Need to ensure analyzer gets stereo input.

**Risk**: Low - Simple routing change, but need to verify stereo input.

---

### Step 2E: AnalyzerDisplayView.cpp - Update Snapshot Reading [~30 lines]

**Current**: Reads `snapshot.fftDb` and `snapshot.fftPeakDb`

**Required**: Read appropriate trace based on `TraceConfig`

**Changes**:
```cpp
void AnalyzerDisplayView::updateFromSnapshot(const AnalyzerSnapshot& snapshot)
{
    // ... existing code ...
    
    // NEW: Select which trace to display based on config
    const float* selectedTrace = nullptr;
    const float* selectedPeak = nullptr;
    
    if (traceConfig.showL) {
        selectedTrace = snapshot.fftDbL.data();
        selectedPeak = snapshot.fftPeakDbL.data();
    } else if (traceConfig.showR) {
        selectedTrace = snapshot.fftDbR.data();
        selectedPeak = snapshot.fftPeakDbR.data();
    }
    // ... etc for all traces
    
    // Use selectedTrace for rendering
}
```

**Risk**: Low - Straightforward selection logic.

---

## Risk Assessment

### High Risk
- **computeFFT() rewrite** - Core analyzer logic, easy to break FFT stability
- **Ballistics extraction** - Complex timing-dependent code

### Medium Risk
- **Helper function extraction** - Need to preserve exact behavior
- **Buffer preallocation** - Must ensure no audio-thread allocations

### Low Risk
- **Snapshot expansion** - Pure data structure
- **Input routing** - Simple passthrough
- **Display update** - Selection logic

---

## Testing Strategy

### Unit Tests
1. **Complex bin extraction** - Verify DC, Nyquist, and regular bins
2. **Mono derivation** - Verify (L+R)/2 matches existing Mono
3. **Side derivation** - Test with phase-inverted signals

### Integration Tests
1. **Left-only sine** - Verify L high, R low, Mono/Mid -6dB, Side -6dB
2. **Phase-inverted stereo** - Verify Mid cancels, Side dominant
3. **FFT size changes** - Verify stability across 512, 1024, 2048, 4096, 8192

### Performance Tests
1. **CPU usage** - Should be ~2x (two FFTs instead of one)
2. **Memory** - Verify no allocations in audio thread
3. **Latency** - Verify no increase in processing latency

---

## Implementation Order

### Phase 1: Foundation (Low Risk)
1. ✅ Expand AnalyzerSnapshot
2. ✅ Add storage buffers to AnalyzerEngine.h
3. ⏳ Modify initializeFFT() to preallocate buffers

### Phase 2: Core Logic (High Risk)
4. Extract helper functions (smoothing, ballistics)
5. Rewrite computeFFT() with dual-channel processing
6. Test with simple signals

### Phase 3: Integration (Medium Risk)
7. Update PluginProcessor routing
8. Update AnalyzerDisplayView trace selection
9. Test full pipeline

### Phase 4: Rendering (Separate Step)
10. Modify RTADisplay to render multiple traces (Step 4 of mission)

---

## Estimated Effort

- **Phase 1**: 1-2 hours (mostly done)
- **Phase 2**: 4-6 hours (complex, high risk)
- **Phase 3**: 2-3 hours (integration, testing)
- **Phase 4**: 3-4 hours (rendering logic)

**Total**: 10-15 hours of focused work

---

## Recommendation

**Proceed in phases with checkpoints:**
1. Complete Phase 1 (buffer allocation)
2. **CHECKPOINT**: Build and verify no regressions
3. Complete Phase 2 (core logic)
4. **CHECKPOINT**: Test with simple signals
5. Complete Phase 3 (integration)
6. **CHECKPOINT**: Full system test
7. Complete Phase 4 (rendering)

**Alternative**: If timeline is tight, consider **UI-thread computation** approach:
- Keep analyzer producing Mono only
- Compute L/R/Mid/Side on UI thread from raw audio samples
- Lower risk, but less accurate (no FFT ballistics)

## Decision Point

**Should I:**
A) Proceed with Phase 1 completion (buffer allocation)?
B) Switch to UI-thread computation approach?
C) Pause for further discussion?
