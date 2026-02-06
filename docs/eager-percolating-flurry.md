# FFT Analyzer Traces - Fix Plan

## Summary
Fix multiple issues in the FFT analyzer's multi-trace implementation affecting thread-safety, ballistics accuracy, and rendering correctness.

---

## Issues Identified (by Priority)

### HIGH Priority (Crash Risk / Data Integrity)
1. **Missing Bounds Validation** - Snapshot copy can overflow buffers
2. **Thread-Safety Gap** - Seqlock implementation allows torn reads

### MEDIUM Priority (Functional Correctness)
3. **Double Ballistics** - UI applies ballistics to already-smoothed engine data
4. **Weighting Disabled** - Multi-traces ignore A/C weighting mode
5. **Peak Trace Aggregation** - Peak becomes global envelope, not main signal peak

### LOW Priority (Polish)
6. **Ballistics Reset on FFT Resize** - Visual "pop" when changing FFT size
7. **Multi-Trace Data Guard** - Quiet channels may not render

---

## Implementation Plan

### Phase 1: Critical Safety Fixes

#### Fix 1: Bounds Validation on Snapshot Copy
**File:** `Source/ui/analyzer/AnalyzerDisplayView.cpp` (~line 1159)

Add safe bounds check before std::copy operations:
```cpp
const size_t safeBins = std::min(static_cast<size_t>(validBins),
                                 AnalyzerSnapshot::kMaxFFTBins);
jassert(scratchPowerL_.size() >= safeBins);
jassert(snapshot.fftDbLRms.size() >= safeBins);
```

#### Fix 2: Thread-Safety in Snapshot Transport
**File:** `Source/analyzer/AnalyzerEngine.cpp` (~lines 1054-1197)

Implement proper seqlock with odd/even sequence:
- Odd sequence = write in progress
- Even sequence = write complete
- UI reader skips if odd, retries if sequence changed

---

### Phase 2: Functional Correctness

#### Fix 3: Remove Double UI-Side Ballistics
**File:** `Source/ui/analyzer/AnalyzerDisplayView.cpp` (~line 1199-1203)

**Remove these calls** since engine already applies full RMS ballistics:
```cpp
// REMOVE - Engine already applied ballistics
// applyBallistics(scratchPowerL_.data(), powerLState_, validBinsSz, releaseMs_);
// applyBallistics(scratchPowerR_.data(), powerRState_, validBinsSz, releaseMs_);
// applyBallistics(scratchPowerMid_.data(), midState_, validBinsSz, releaseMs_);
// applyBallistics(scratchPowerSide_.data(), sideState_, validBinsSz, releaseMs_);
// applyBallistics(scratchPowerMono_.data(), monoState_, validBinsSz, releaseMs_);
```

**Also remove** unused state vectors from header:
- `powerLState_`, `powerRState_`, `midState_`, `sideState_`, `monoState_`

#### Fix 4: Re-enable Weighting for Multi-Traces
**File:** `Source/ui/analyzer/AnalyzerDisplayView.cpp` (~line 1175-1189)

Uncomment and fix the weighting block so multi-traces respect A/C weighting mode.

#### Fix 5: Peak Trace Semantics
**File:** `Source/ui/analyzer/AnalyzerDisplayView.cpp` (~line 1229-1240)

Remove multi-trace aggregation from peak trace. Peak should represent main signal peak only:
```cpp
// Peak trace = main signal peak only (not multi-trace envelope)
float peakDb = fftPeakDb_[i];
if (i < fftDb_.size())
    peakDb = juce::jmax(peakDb, fftDb_[i]);
// REMOVE the multi-trace max aggregation
```

---

### Phase 3: Polish (Optional)

#### Fix 6: Preserve Ballistics on FFT Resize
**File:** `Source/analyzer/AnalyzerEngine.cpp`

Interpolate old ballistics state to new bin count instead of resetting to zero.

#### Fix 7: Per-Trace Data Guard
**File:** `Source/ui/analyzer/rta1_import/RTADisplay.cpp` (~line 456)

Add per-channel validity checks so quiet channels still render at floor level.

---

## Files to Modify

| File | Changes |
|------|---------|
| `Source/ui/analyzer/AnalyzerDisplayView.cpp` | Bounds check, remove double ballistics, weighting, peak trace |
| `Source/ui/analyzer/AnalyzerDisplayView.h` | Remove unused ballistics state vectors |
| `Source/analyzer/AnalyzerEngine.cpp` | Thread-safety seqlock, optional ballistics preservation |
| `Source/ui/analyzer/rta1_import/RTADisplay.cpp` | Optional per-trace data guard |

---

## Verification

1. **Build & Run** - Ensure no compiler errors/warnings
2. **Visual Test** - Play pink noise, verify:
   - Multi-traces respond correctly (no sluggish double-smoothing)
   - Peak trace tracks main signal only
   - Weighting mode affects all traces equally
3. **Thread Safety** - Run with ThreadSanitizer under sustained load
4. **Edge Cases** - Change FFT size during playback, test extreme dB values
