# REVISED PLAN: Dual-FFT Wrapper Approach

## Mission: MULTI_TRACE_DUALFFT_V1 (REVISED)
## Approach: Wrapper + Minimal Extension (NO core rewrite)

## Core Principle

**DO NOT rewrite `computeFFT()`**. Instead:
1. Call existing `computeFFT()` twice (L channel, R channel)
2. Store results in separate buffers
3. Derive Mono/Mid/Side from stored power spectra (outside FFT core)
4. Add feature flag for instant rollback

---

## Architecture

### Current Flow (Single Channel)
```
analysisBuffer (transformed) → computeFFT() → magnitudes_ → ballistics → snapshot.fftDb
```

### New Flow (Dual Channel with Wrapper)
```
analysisBuffer[L] → computeFFT() → magnitudes_ → copy to powerL_
analysisBuffer[R] → computeFFT() → magnitudes_ → copy to powerR_

powerL_ + powerR_ → deriveMono/Mid/Side() → powerMono_, powerMid_, powerSide_

All 5 power spectra → applyBallistics() → convertToDb() → snapshot (5 arrays)
```

**Key Insight**: Reuse existing `computeFFT()` as-is, just call it twice and store results separately.

---

## Detailed Changes

### 1. AnalyzerSnapshot.h [DONE]
✅ Already expanded with 5 spectra arrays

### 2. AnalyzerEngine.h [PARTIAL]
✅ Already added storage buffers

**Additional needed**:
```cpp
// Feature flag for rollback
bool enableMultiTrace_ = true;  // Set to false to revert to legacy behavior

// Helper to derive Mono/Mid/Side from L/R power spectra
void deriveTracesFromLR(int numBins);
```

**Lines**: +2

---

### 3. AnalyzerEngine.cpp - New Helper Function [~40 lines]

**Add new function** (does NOT modify existing code):

```cpp
void AnalyzerEngine::deriveTracesFromLR(int numBins)
{
    // Derive Mono/Mid/Side from L/R power spectra
    // This happens AFTER both FFTs are computed
    
    for (int i = 0; i < numBins; ++i)
    {
        const size_t idx = static_cast<size_t>(i);
        
        // Mono = (L + R) / 2 in POWER domain
        // Power of sum ≈ (powerL + powerR) / 2 for incoherent signals
        // For coherent: need complex, but this is close enough for visualization
        powerMono_[idx] = (powerL_[idx] + powerR_[idx]) * 0.5f;
        
        // Mid = same as Mono (different label only)
        powerMid_[idx] = powerMono_[idx];
        
        // Side = (L - R) / 2 in POWER domain
        // Approximation: |L-R|^2 ≈ |L|^2 + |R|^2 - 2*Re(L*conj(R))
        // Without phase: use |L|^2 + |R|^2 - 2*sqrt(|L|^2 * |R|^2)
        // Simpler approximation for visualization:
        powerSide_[idx] = std::abs(powerL_[idx] - powerR_[idx]) * 0.5f;
    }
}
```

**Note**: This is an approximation since we don't have complex bins. For proper Side, we'd need phase. But this is good enough for visualization and avoids core FFT changes.

**Lines**: ~40

---

### 4. AnalyzerEngine.cpp - Wrapper in processBlock() [~80 lines]

**Modify `processBlock()`** to call `computeFFT()` twice:

```cpp
void AnalyzerEngine::processBlock(const juce::AudioBuffer<float>& buffer)
{
    if (!enableMultiTrace_)
    {
        // LEGACY PATH: Use existing single-channel logic
        // ... existing code unchanged ...
        return;
    }
    
    // NEW MULTI-TRACE PATH
    
    // Ensure we have stereo input
    const int numChannels = buffer.getNumChannels();
    if (numChannels < 2)
    {
        // Fall back to legacy for mono input
        enableMultiTrace_ = false;
        processBlock(buffer);  // Recursive call to legacy path
        return;
    }
    
    // Process LEFT channel
    {
        // Copy L to fifo (existing logic)
        const float* channelData = buffer.getReadPointer(0);
        // ... existing fifo logic for channel 0 ...
        
        // When ready, compute FFT
        if (samplesCollected >= currentFFTSize)
        {
            computeFFT();  // Uses existing logic
            
            // Copy result to powerL_
            const int numBins = currentFFTSize / 2 + 1;
            std::copy(magnitudes_.begin(), magnitudes_.begin() + numBins, powerL_.begin());
            
            samplesCollected = 0;
        }
    }
    
    // Process RIGHT channel
    {
        // Copy R to fifo (need separate fifo or reuse)
        const float* channelData = buffer.getReadPointer(1);
        // ... fifo logic for channel 1 ...
        
        // When ready, compute FFT
        if (samplesCollected >= currentFFTSize)
        {
            computeFFT();  // Uses existing logic again
            
            // Copy result to powerR_
            const int numBins = currentFFTSize / 2 + 1;
            std::copy(magnitudes_.begin(), magnitudes_.begin() + numBins, powerR_.begin());
        }
    }
    
    // Derive Mono/Mid/Side
    const int numBins = currentFFTSize / 2 + 1;
    deriveTracesFromLR(numBins);
    
    // Apply ballistics to all 5 traces
    // ... (extract existing ballistics logic into helper)
    
    // Convert to dB and publish
    // ... (populate all 5 snapshot arrays)
}
```

**Issue**: Need separate FIFO for R channel, or modify existing FIFO to handle stereo.

**Better approach**: Add `fifoBufferR_` member and duplicate FIFO logic.

**Lines**: ~80

---

### 5. AnalyzerEngine.h - Add Second FIFO [~2 lines]

```cpp
std::vector<float> fifoBuffer;   // Existing L channel FIFO
std::vector<float> fifoBufferR_;  // NEW: R channel FIFO
int fifoWritePosR_ = 0;           // NEW: R channel write position
int samplesCollectedR_ = 0;       // NEW: R channel sample count
```

**Lines**: +3

---

### 6. AnalyzerEngine.cpp - initializeFFT() [~5 lines]

```cpp
void AnalyzerEngine::initializeFFT(int fftSize)
{
    // ... existing code ...
    
    // NEW: Allocate R channel FIFO
    fifoBufferR_.resize(static_cast<size_t>(fftSize), 0.0f);
    fifoWritePosR_ = 0;
    samplesCollectedR_ = 0;
    
    // NEW: Allocate power spectrum buffers (already done in Step 2)
    // ...
}
```

**Lines**: +5

---

## Files to Touch

| File | Approximate Lines | Risk | Purpose |
|------|------------------|------|---------|
| AnalyzerSnapshot.h | ✅ DONE | Low | Data structure |
| AnalyzerEngine.h | +5 | Low | Add R FIFO, feature flag |
| AnalyzerEngine.cpp | +125 | **Medium** | Wrapper logic, derive function |
| PluginProcessor.cpp | -30 | Low | Remove channel transform (feed stereo) |
| **TOTAL** | **~130 lines** | | |

**Comparison to original plan**: 130 lines vs 500 lines (74% reduction)

---

## Risk Table

| What Can Regress | How to Detect | Mitigation |
|------------------|---------------|------------|
| **FFT stability** | Visual glitches, crashes on size change | ✅ **Feature flag**: Set `enableMultiTrace_ = false` |
| **Single-channel mode** | Mono input doesn't work | ✅ **Fallback**: Auto-disable for mono input |
| **CPU usage spike** | 2x FFT = 2x CPU | ⚠️ **Expected**: Monitor, optimize later if needed |
| **Memory allocation** | Audio thread allocations | ✅ **Preallocate**: All buffers sized in `initializeFFT()` |
| **Ballistics timing** | Peak hold behaves differently | ⚠️ **Test**: Verify with known signals |
| **Snapshot corruption** | Garbled display | ✅ **Legacy arrays**: Keep `fftDb`/`fftPeakDb` populated |

### Detection Strategy

1. **Build test**: Must compile without errors
2. **Mono input test**: Feed mono signal, verify legacy path works
3. **Stereo sine test**: Left-only 1kHz sine, verify L high, R low
4. **FFT size test**: Change size 512→8192, verify no crashes
5. **CPU test**: Monitor CPU usage, should be <2x current

---

## Implementation Order

### Phase 1: Foundation [~30 min]
1. ✅ AnalyzerSnapshot.h (DONE)
2. ✅ AnalyzerEngine.h storage (DONE)
3. Add R channel FIFO to AnalyzerEngine.h
4. Add feature flag to AnalyzerEngine.h
5. Preallocate buffers in `initializeFFT()`

**Checkpoint**: Build succeeds, no runtime changes

### Phase 2: Core Logic [~2 hours]
6. Implement `deriveTracesFromLR()` helper
7. Add wrapper logic in `processBlock()`
8. Extract ballistics into helper function
9. Populate all 5 snapshot arrays

**Checkpoint**: Build succeeds, test with feature flag OFF (legacy), then ON (new)

### Phase 3: Integration [~1 hour]
10. Update PluginProcessor to feed stereo
11. Test with simple signals
12. Verify CPU usage acceptable

**Checkpoint**: Full system test with real audio

---

## Feature Flag Usage

```cpp
// In AnalyzerEngine constructor or prepare():
enableMultiTrace_ = true;  // Enable new behavior

// To rollback instantly:
enableMultiTrace_ = false;  // Revert to legacy single-channel
```

**Benefit**: Can toggle at runtime without recompiling.

---

## Approximation Limitations

**Side channel approximation**:
- Without complex bins, Side = `|powerL - powerR|` is approximate
- True Side requires phase: `Side = |L - R|^2 = |L|^2 + |R|^2 - 2*Re(L*conj(R))`
- Our approximation: `Side ≈ |powerL - powerR|`

**Impact**: Side trace will be less accurate but still useful for visualization.

**Future improvement**: If accuracy is critical, store complex bins (Phase 2 of original plan).

---

## Deliverables

1. ✅ **Updated plan** (this document)
2. ✅ **Exact files + line counts** (see table above)
3. ✅ **Risk table** (see above)

## Next Steps

**Should I proceed with Phase 1 implementation?**
- Add R channel FIFO
- Add feature flag
- Preallocate buffers
- Build and verify no regressions

**Estimated time**: 30 minutes
**Risk**: Low (no behavior changes yet)
