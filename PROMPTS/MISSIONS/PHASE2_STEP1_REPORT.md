# PHASE 2 STEP 1 REPORT: computeFFT Dependencies Analysis

## Mission: MULTI_TRACE_DUALFFT_V1 — PHASE 2 STEP 1

## Status: ✅ ANALYZED

### computeFFT() Signature and Flow

**Function**: `void AnalyzerEngine::computeFFT()` (lines 161-400+)

**Current Dependencies**:

#### 1. Input Buffers (Read)
- `fifoBuffer` - Time-domain samples collected from audio
- `window` - Hann window coefficients (read-only)

#### 2. Scratch/Working Buffers (Read/Write)
- `fftOutput` - In-place FFT buffer (input: time-domain, output: complex frequency-domain)
  - Size: `fftSize * 2` (real + imaginary interleaved)
  - **CRITICAL**: This is the main scratch buffer

#### 3. Intermediate Storage (Write)
- `magnitudes_` - Power spectrum (re² + im²)
- `prefixSumMag` - Prefix sum for frequency smoothing
- `freqSmoothed` - Smoothed power spectrum (reuses `fftOutput` as scratch!)

#### 4. State Buffers (Read/Write)
- `smoothedMagnitude` - RMS ballistics state
- `smoothedPeak` - Peak ballistics state
- `peakHold` - Peak hold state
- `peakHoldFramesRemaining_` - Peak hold timer

### Key Finding: Scratch Buffer Reuse

**Line 223**: `float* freqSmoothed = fftOutput.data();`

The function **reuses `fftOutput` as scratch** for frequency smoothing after the FFT is complete. This is safe for sequential calls but means:
- `fftOutput` is overwritten during smoothing
- Cannot call `computeFFT()` twice in parallel
- **CAN** call it twice sequentially (L then R)

### Reentrancy Analysis

**Question**: Can `computeFFT()` be called twice safely?

**Answer**: ✅ **YES**, with sequential invocation

**Reasoning**:
1. **Scratch buffers are reused sequentially**: `fftOutput` is used, then overwritten, then reused
2. **No persistent state between calls**: Each call is independent
3. **State buffers are separate**: `smoothedMagnitude`, `smoothedPeak`, etc. are updated per-call

**Required for dual invocation**:
- Call `computeFFT()` for L channel → store results in `powerL_`
- Call `computeFFT()` for R channel → store results in `powerR_`
- **Sequential only** (not parallel)

### Scratch Buffer Strategy

**Option A: Shared Scratch (Current)**
- ✅ **CHOSEN**: Reuse existing `fftOutput` for both L and R
- Pro: No additional memory
- Pro: No code changes to `computeFFT()`
- Con: Must call sequentially

**Option B: Duplicate Scratch**
- Add `fftOutputR_` buffer
- Pro: Could parallelize (not needed)
- Con: Extra memory
- Con: More complex

**Decision**: Use **Option A** (shared scratch, sequential calls)

### Input/Output Routing

**Current**:
```
fifoBuffer → applyWindow() → fftOutput → computeFFT() → magnitudes_ → ballistics → snapshot
```

**Required for Dual**:
```
fifoBuffer (L) → applyWindow() → fftOutput → computeFFT() → magnitudes_ → copy to powerL_
fifoBufferR_   → applyWindow() → fftOutput → computeFFT() → magnitudes_ → copy to powerR_
```

**Issue**: `applyWindow()` currently reads from `fifoBuffer` and writes to `fftOutput` directly (member variables).

### applyWindow() Dependencies

**Function**: `void AnalyzerEngine::applyWindow()` (need to locate)

**Expected behavior**: 
- Reads from `fifoBuffer`
- Applies `window` coefficients
- Writes to `fftOutput`

**Required change**: Make `applyWindow()` parameterizable to accept different input FIFO.

### Summary

**Scratch Buffer Usage**: ✅ **Shared, sequential reuse is safe**

**Buffers Involved**:
1. **Input**: `fifoBuffer` (L), `fifoBufferR_` (R) - already exists
2. **Scratch**: `fftOutput` - shared, reused sequentially
3. **Window**: `window` - read-only, shared
4. **Output**: `magnitudes_` - temporary, copy to `powerL_`/`powerR_`
5. **State**: `smoothedMagnitude`, `smoothedPeak`, etc. - need separate L/R versions

**Required Modifications**:
1. ✅ **None to `computeFFT()` internals** - can call as-is
2. ⚠️ **Modify `applyWindow()`** - needs to accept input FIFO parameter
3. ✅ **Copy results** - after each call, copy `magnitudes_` to `powerL_`/`powerR_`

### Next Step

Step 2 will:
1. Modify `applyWindow()` to accept FIFO parameter (or create `applyWindowR()`)
2. Fill `fifoBufferR_` in parallel with `fifoBuffer`
3. Call `computeFFT()` twice sequentially when frame ready

## STOP

**Scratch Strategy**: Shared buffer, sequential calls
**Buffers**: `fftOutput` (shared), `fifoBuffer`/`fifoBufferR_` (separate), `powerL_`/`powerR_` (separate)
**Modifications needed**: `applyWindow()` parameterization only
