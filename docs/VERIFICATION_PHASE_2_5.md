# Phase-2.5 Stabilization (Post Slice-C)

## 1. Summary of changes

**Snapshot copy semantics (mdsp_dsp):**
- Added `snapshotReady_` atomic (single-consumer gate). Producer sets it to `true` with `std::memory_order_release` only after the full snapshot is written. Consumer uses `snapshotReady_.exchange(false, std::memory_order_acq_rel)` at the start of `getLatestSnapshot`; if the exchange returns `false`, no snapshot was available and the function returns `false`. Copy remains seqlock-based for tear-free reads. No references or pointers to internal snapshots are returned from the consumer API.
- `snapshotReady_` is cleared on `prepare()` and on FFT resize (`initializeFFT`) so invalidated state is not signalled as ready.

**Peak >= RMS invariant (mdsp_dsp):**
- Right before `publishSnapshot()` in `computeFFT()`, for each FFT bin: `peakDb[i] = std::max(peakDb[i], rmsDb[i])` for the main trace (`fftPeakDb` / `fftDb`). This prevents the peak trace from ever being below the RMS trace in published snapshots.
- `JUCE_DEBUG` assertions added: `jassert(std::isfinite(peakDb[i]) && std::isfinite(rmsDb[i]))` and `jassert(peakDb[i] >= rmsDb[i] - 0.01f)`.

**Adapter purity (AnalyzerPro):**
- Comment block added at the top of `AnalyzerEngine.cpp`: adapter must not perform smoothing/math; it only maps params to config and forwards audio/snapshots.
- TODO comment added next to the StereoScope feed: future Slice should extract StereoScope DSP to `mdsp_dsp/scopes` when generalized. No behavior changes in the adapter.

## 2. Files changed

- `melechdsp-hq/shared/mdsp_dsp/include/mdsp_dsp/analyzer/AnalyzerEngine.h` — added `snapshotReady_` member.
- `melechdsp-hq/shared/mdsp_dsp/src/analyzer/AnalyzerEngine.cpp` — snapshot gate (exchange in `getLatestSnapshot`, store release in `publishSnapshot`, clear on prepare/resize); peak>=rms clamp and debug asserts before publish.
- `AnalyzerPro/Source/analyzer/AnalyzerEngine.cpp` — adapter purity comment block; StereoScope TODO comment only.

## 3. Verification (ripgrep)

Peak clamp in mdsp_dsp AnalyzerEngine.cpp:

    rg "std::max.*fftPeakDb.*fftDb" melechdsp-hq/shared/mdsp_dsp/src/analyzer/AnalyzerEngine.cpp

popSnapshot / getLatestSnapshot uses exchange + memory_order:

    rg "snapshotReady_.*exchange|memory_order_acq_rel" melechdsp-hq/shared/mdsp_dsp/src/analyzer/AnalyzerEngine.cpp

Publish uses release for snapshotReady_:

    rg "snapshotReady_.*store.*memory_order_release" melechdsp-hq/shared/mdsp_dsp/src/analyzer/AnalyzerEngine.cpp

mdsp_dsp analyzer has no GUI includes:

    rg "juce_gui|juce_graphics" melechdsp-hq/shared/mdsp_dsp/

(Should return no matches in analyzer code.)

AnalyzerPro adapter has no smoothing/math (no exp/pow/log/onePole/calcCoeff in adapter):

    rg "exp\\(|pow\\(|log\\(|log10|onePole|calcCoeff" AnalyzerPro/Source/analyzer/AnalyzerEngine.cpp

(Should return no matches.)

## 4. Build commands

**melechdsp-hq:** From repo root, use the project’s CMake/build flow (e.g. open in IDE or `cmake -B build && cmake --build build`). If AnalyzerPro uses `add_subdirectory(third_party/melechdsp-hq)` or similar, building AnalyzerPro will build mdsp_dsp as part of that.

**AnalyzerPro:** Build via your usual method (e.g. CMake or Projucer-generated project) from the AnalyzerPro repo root.

## 5. Runtime smoke steps

1. Load the plugin, feed audio to the analyzer.
2. Confirm spectrum and peak traces render; peak trace never visibly drops below RMS.
3. Change FFT size and/or reset peaks; confirm no crash and display updates.
4. Run a Debug build and exercise analyzer; peak>=rms and finite-value asserts should not fire.

## 6. Rollback

Revert the listed files to the pre–Phase-2.5 commit, or:

    git checkout -- melechdsp-hq/shared/mdsp_dsp/include/mdsp_dsp/analyzer/AnalyzerEngine.h
    git checkout -- melechdsp-hq/shared/mdsp_dsp/src/analyzer/AnalyzerEngine.cpp
    git checkout -- AnalyzerPro/Source/analyzer/AnalyzerEngine.cpp

(Paths relative to each repo root; adjust if your workspace layout differs.)
