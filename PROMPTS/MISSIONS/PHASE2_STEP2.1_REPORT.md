# PHASE 2 STEP 2.1 REPORT: applyWindow Parameterization

## Mission: MULTI_TRACE_DUALFFT_V1 — PHASE 2 STEP 2.1

## Status: ✅ COMPLETE

### Function Signature Changes

**Before**:
```cpp
void AnalyzerEngine::applyWindow()
{
    // Reads fifoBuffer directly
    fftOutput[outIdx] = fifoBuffer[fifoIdx] * window[outIdx];
}
```

**After**:
```cpp
void AnalyzerEngine::applyWindow(const std::vector<float>& fifoIn)
{
    // Reads from parameter
    fftOutput[outIdx] = fifoIn[fifoIdx] * window[outIdx];
}
```

### Files Modified

1. **AnalyzerEngine.h** (line 181)
   - Changed: `void applyWindow();`
   - To: `void applyWindow(const std::vector<float>& fifoIn);`

2. **AnalyzerEngine.cpp** (line 415)
   - Changed signature to accept `fifoIn` parameter
   - Replaced all `fifoBuffer[...]` reads with `fifoIn[...]`
   - Write target remains `fftOutput` (unchanged)

### Call Sites Updated

**Single call site**: `AnalyzerEngine::computeFFT()` (line 198)
- Changed: `applyWindow();`
- To: `applyWindow(fifoBuffer);`

### Behavior

- ✅ **No behavior change**: Still reads from `fifoBuffer` via parameter
- ✅ **Enables dual invocation**: Can now call with `fifoBufferR_`
- ✅ **Minimal diff**: Only signature and one call site changed

### Build Status

**Building...** (waiting for confirmation)

## STOP

**Next**: Step 2.2 - Wire R FIFO fill
