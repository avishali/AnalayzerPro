# PHASE 2 STATUS SUMMARY

## Mission: MULTI_TRACE_DUALFFT_V1 — PHASE 2

## Current Status: PAUSED FOR REVIEW

### Completed (Steps 2.1)
✅ **applyWindow() Parameterization**
- Changed signature to accept FIFO parameter
- Enables calling with different input buffers
- Build successful

### Engineering Decision (User Input)
**Simplified Scope for Phase 2**:
- Focus ONLY on dual FFT correctness (raw power spectra)
- Defer ballistics (smoothing/peak hold) to Phase 3/4
- This keeps Phase 2 strictly about validating L/R FFT computation

### Remaining Work for Phase 2

#### Step 2.2: Wire R FIFO Fill (~10 lines)
**Location**: `processBlock()` loop
**Action**: Mirror L channel FIFO fill for R channel
```cpp
// Add inside sample loop:
if (enableMultiTrace_ && numChannels > 1) {
    fifoBufferR_[fifoIdxR] = buffer.getReadPointer(1)[i];
    fifoWritePosR_ = (fifoWritePosR_ + 1) % currentFFTSize;
    samplesCollectedR_++;
}
```

#### Step 2.3: Dual Invocation (~15 lines)
**Location**: Frame-ready condition in `processBlock()`
**Action**: Call `computeFFT()` twice when `enableMultiTrace_ == true`
```cpp
if (samplesCollected >= currentHopSize) {
    if (!enableMultiTrace_) {
        computeFFT();  // Legacy
    } else {
        // L
        applyWindow(fifoBuffer);
        fft->performRealOnlyForwardTransform(fftOutput.data(), false);
        // Extract and store to powerL_ (Phase 3)
        
        // R
        if (samplesCollectedR_ >= currentHopSize) {
            applyWindow(fifoBufferR_);
            fft->performRealOnlyForwardTransform(fftOutput.data(), false);
            // Extract and store to powerR_ (Phase 3)
        }
        
        // Fallback to legacy for now
        computeFFT();
    }
}
```

#### Step 2.4: Magnitude Extraction (Deferred to Phase 3)
**Reason**: Requires extracting magnitude computation from `computeFFT()`
**Complexity**: Would need to refactor `computeFFT()` internals (violates mission constraint)

**Recommendation**: 
- Phase 2 validates FIFO fill and dual invocation structure
- Phase 3 extracts magnitude computation into helper function
- Phase 4 adds per-trace ballistics

### Blocker Identified

**Issue**: Current `computeFFT()` is monolithic:
1. Applies window (now parameterized ✅)
2. Performs FFT
3. Extracts magnitudes
4. Applies frequency smoothing
5. Applies ballistics
6. Converts to dB
7. Publishes snapshot

**To complete dual FFT without rewriting `computeFFT()`**:
- Need to extract steps 3-6 into helper functions
- This violates "no computeFFT rewrite" constraint

### Recommended Path Forward

**Option A: Minimal Helper Extraction**
- Extract magnitude computation into `extractMagnitudes(const float* fftOut, float* powerOut, int numBins)`
- Keep rest of `computeFFT()` intact
- ~20 lines of refactoring

**Option B: Phase 2 = Infrastructure Only**
- Complete FIFO fill and dual invocation structure
- Leave magnitude extraction for Phase 3
- Phase 2 validates that dual FFT *can* be called
- Phase 3 makes it *useful*

**Option C: Accept Partial Rewrite**
- Acknowledge that some `computeFFT()` refactoring is necessary
- Extract helpers while preserving exact behavior
- Document as "minimal necessary refactoring"

### Files Modified So Far

**Phase 1**:
- `AnalyzerSnapshot.h` (+18 lines) - 5 spectra arrays
- `AnalyzerEngine.h` (+33 lines) - buffers + R FIFO
- `AnalyzerEngine.cpp` (+27 lines) - buffer allocation

**Phase 2 Step 2.1**:
- `AnalyzerEngine.h` (1 line) - applyWindow signature
- `AnalyzerEngine.cpp` (2 lines) - applyWindow impl + call site

**Total**: 3 files, ~81 lines added/modified

### Next Decision Point

**User input needed**:
1. Which option (A/B/C) for completing Phase 2?
2. Is helper extraction acceptable, or strict "no rewrite"?
3. Should Phase 2 stop at infrastructure validation?

## STOP

Awaiting direction on how to proceed with magnitude extraction.
