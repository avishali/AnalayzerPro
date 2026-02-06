# FFT Analyzer Traces - Fix Summary

## Date: 2026-02-06

All 7 fixes from the plan implemented and build verified (zero errors).

---

## Fix 1: Bounds Validation on Snapshot Copy (HIGH)

**Files:** `Source/ui/analyzer/AnalyzerDisplayView.cpp`

- Added `safeCopyBins` clamped against `AnalyzerSnapshot::kMaxFFTBins` for main `fftDb`/`fftPeakDb` copies (line ~951).
- Added bounds-checked copy for `fftPeakHoldDb` using `safeHoldBins` with fallback to fill with floor value (line ~1245).
- All `std::copy` operations now use `std::ptrdiff_t` casts and are clamped to prevent buffer overflows.

---

## Fix 2: Thread-Safety - publishSnapshot Missing Multi-Trace Data (HIGH)

**Files:** `Source/analyzer/AnalyzerEngine.cpp`

**Bug found:** `publishSnapshot()` copied `powerL`/`powerR` but **never** copied the RMS dB arrays (`fftDbLRms`, `fftDbRRms`, `fftDbMidRms`, `fftDbSideRms`, `fftDbMonoRms`) into `published_.data`. Meanwhile, `getLatestSnapshot()` was reading these arrays. This meant the UI was reading **stale/zero data** through the seqlock for all multi-trace rendering.

**Fix:** Added the 5 RMS dB array copies inside the `publishSnapshot()` multi-trace block (lines ~1115-1120).

**Bonus cleanup:** Removed duplicate `displayBottomDb` assignments in both `publishSnapshot()` and `getLatestSnapshot()`.

---

## Fix 3: Remove Double UI-Side Ballistics (MEDIUM)

**Files:** `Source/ui/analyzer/AnalyzerDisplayView.cpp`, `Source/ui/analyzer/AnalyzerDisplayView.h`

**Status:** Already implemented prior to this session. The `applyBallistics()` calls on multi-trace scratch buffers were already removed, and the unused state vectors (`powerLState_`, etc.) were already removed from the header.

---

## Fix 4: Re-enable Weighting for Multi-Traces (MEDIUM)

**Files:** `Source/ui/analyzer/AnalyzerDisplayView.cpp`

- **Main trace:** Uncommented the weighting application block for `fftDb_` and `fftPeakDb_` (lines ~1018-1033). Previously disabled due to suspected double-weighting with `RTADisplay::dbToYWithCompensation`, but that function's tilt/weighting compensation is independently disabled (returns 0).
- **Multi-traces:** Uncommented the weighting application block for all 5 scratch buffers (`scratchPowerL_`, `scratchPowerR_`, `scratchPowerMid_`, `scratchPowerSide_`, `scratchPowerMono_`) at lines ~1176-1187.
- All traces now respect A-weighting and BS.468-4 weighting modes uniformly.

---

## Fix 5: Peak Trace Semantics - Main Signal Only (MEDIUM)

**Files:** `Source/ui/analyzer/AnalyzerDisplayView.cpp`

- Removed the multi-trace aggregation block from peak trace calculation (previously lines ~1219-1226 that took `juce::jmax` across `scratchPowerL_`, `scratchPowerR_`, `scratchPowerMid_`, `scratchPowerSide_`, `scratchPowerMono_`).
- Peak trace now represents `max(fftPeakDb, fftDb)` for the main signal only.
- This prevents the peak trace from becoming a "global envelope" of all channels, which was visually misleading.

---

## Fix 6: Preserve Ballistics on FFT Resize (LOW)

**Files:** `Source/analyzer/AnalyzerEngine.cpp`

- Added `resampleBallistics` lambda that linearly interpolates ballistics state from old bin count to new bin count.
- Applied to 7 buffers: `smoothedMagnitude`, `smoothedPeak`, `smoothedLRms_`, `smoothedRRms_`, `smoothedMidRms_`, `smoothedSideRms_`, `smoothedMonoRms_`.
- Removed the hard `std::fill(smoothedMagnitude, 0)` reset that previously zeroed ballistics state on every FFT resize.
- This prevents the visual "pop" (spectrum dropping to floor and recovering) when changing FFT size during playback.

---

## Fix 7: Per-Trace Data Guard (LOW)

**Files:** `Source/ui/analyzer/rta1_import/RTADisplay.cpp`

- Modified the multi-trace data guard in `setMultiTraceData()` (line ~455).
- If `hasValidSpectrumFrame` is already true (set by main trace's `setFFTData`), multi-trace data is always considered valid regardless of individual channel levels.
- This ensures quiet channels still render at floor level rather than being suppressed entirely.
- Fallback: if main trace hasn't been set yet but multi-trace has audible data, `hasValidSpectrumFrame` is still set to true.

---

## Files Modified

| File | Fixes |
|------|-------|
| `Source/analyzer/AnalyzerEngine.cpp` | Fix 2 (seqlock data copy), Fix 6 (ballistics preservation) |
| `Source/ui/analyzer/AnalyzerDisplayView.cpp` | Fix 1 (bounds), Fix 4 (weighting), Fix 5 (peak semantics) |
| `Source/ui/analyzer/AnalyzerDisplayView.h` | Fix 3 (already done - removed unused state vectors) |
| `Source/ui/analyzer/rta1_import/RTADisplay.cpp` | Fix 7 (per-trace data guard) |

---

## Verification

- Build: **PASS** (zero errors, 1 pre-existing unused parameter warning in `dbToYWithCompensation`)
- All fixes compile cleanly with Ninja/Debug configuration
