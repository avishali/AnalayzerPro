# Slice-C diffs

**Slice-C:** AnalyzerEngine DSP core moved into mdsp_dsp; AnalyzerPro uses adapter only.

---

## AnalyzerPro

- **Files changed:** `Source/analyzer/AnalyzerEngine.cpp`, `Source/analyzer/AnalyzerEngine.h`
- **Full diff:** [analyzer_engine_slice_c_analyzerpro.diff](analyzer_engine_slice_c_analyzerpro.diff) (1723 lines)

**Summary:** All FFT/ballistics/snapshot implementation removed from AnalyzerPro. Adapter keeps same public API and forwards to `mdsp_dsp::AnalyzerEngine core_`; `processBlock` also feeds `StereoScopeAnalyzer`. Header now includes `AnalyzerSnapshotAdapter.h`, `StereoScopeAnalyzer.h`, and `<mdsp_dsp/analyzer/AnalyzerEngine.h>`.

---

## melechdsp-hq

- **Modified:** `shared/mdsp_dsp/CMakeLists.txt` — [analyzer_engine_slice_c_melechdsp.diff](analyzer_engine_slice_c_melechdsp.diff)
- **New (untracked in git):**
  - `shared/mdsp_dsp/include/mdsp_dsp/analyzer/AnalyzerEngine.h`
  - `shared/mdsp_dsp/include/mdsp_dsp/analyzer/AnalyzerSnapshot.h`
  - `shared/mdsp_dsp/src/analyzer/AnalyzerEngine.cpp`

**CMakeLists.txt summary:** Add `src/analyzer/AnalyzerEngine.cpp` to `add_library` and `juce::juce_audio_basics` to `target_link_libraries`.
