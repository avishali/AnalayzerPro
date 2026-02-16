# Phase-2 / Slice C — Verification

Verify AnalyzerEngine DSP core lives in mdsp_dsp; AnalyzerPro uses only the thin adapter (no local FFT/engine implementation).

---

## 1. Build steps

### melechdsp-hq — mdsp_dsp target

AnalyzerPro builds mdsp_dsp via add_subdirectory(third_party/melechdsp-hq/shared/mdsp_dsp); no separate build required for normal workflow. To build mdsp_dsp standalone (e.g. from melechdsp-hq repo):

```bash
cd /path/to/melechdsp-hq
cmake -B build -DJUCE_PATH="$JUCE_PATH"
cmake --build build --target mdsp_dsp
```

Expect: build completes. mdsp_dsp includes src/analyzer/AnalyzerEngine.cpp (full DSP: FFT, ballistics, snapshot).

### AnalyzerPro target

```bash
cd /path/to/AnalyzerPro
cmake -B build -DJUCE_PATH="$JUCE_PATH"
cmake --build build --target AnalyzerPro
```

Expect: build completes with no errors. Plugin artefact in build/AnalyzerPro_artefacts/.

---

## 2. Static checks (ripgrep)

Run from AnalyzerPro repo root.

**Prove no FFT/engine DSP implementation remains in AnalyzerPro (except adapter):**

Adapter is Source/analyzer/ (AnalyzerEngine.h + AnalyzerEngine.cpp) which only forwards to core_; no FFT logic there.

```bash
rg 'juce::dsp::FFT|dsp::FFT|performFFT|initializeFFT|applyWindow|extractMagnitudes|convertToDb|updatePeakHold|computeFFT' Source/ --type-add 'src:*.{h,cpp}' -t src
```

Expected: no matches in Source/. (All such implementation lives in mdsp_dsp under third_party.)

```bash
rg 'Windowing|window.*fft|fft.*window' Source/ -i
```

Expected: no implementation (only comments or display names like "FFT" are acceptable).

```bash
rg 'struct AnalyzerSnapshot|class AnalyzerSnapshot' Source/
```

Expected: no matches (definition in mdsp_dsp; AnalyzerPro uses adapter/snapshot type only).

**Prove AnalyzerPro includes mdsp_dsp/analyzer/AnalyzerEngine.h:**

```bash
rg '#include.*mdsp_dsp/analyzer/AnalyzerEngine\.h' Source/
```

Expected: at least one match (Source/analyzer/AnalyzerEngine.h).

```bash
rg 'mdsp_dsp/analyzer/AnalyzerEngine' Source/
```

Expected: matches in Source/analyzer/AnalyzerEngine.h and Source/analyzer/AnalyzerEngine.cpp (adapter includes and uses core_ only).

---

## 3. Dependency checks

**Confirm mdsp_dsp AnalyzerEngine does NOT include juce_gui_basics or juce_graphics:**

Run from melechdsp-hq repo root (or from AnalyzerPro against third_party copy):

```bash
rg 'juce_gui_basics|juce_graphics' shared/mdsp_dsp/
# or from AnalyzerPro:
rg 'juce_gui_basics|juce_graphics' third_party/melechdsp-hq/shared/mdsp_dsp/
```

Expected: no matches. mdsp_dsp analyzer uses only juce_dsp, juce_audio_basics, juce_core (and mdsp_dsp headers); no GUI modules.

Manual check: open shared/mdsp_dsp/include/mdsp_dsp/analyzer/AnalyzerEngine.h and shared/mdsp_dsp/src/analyzer/AnalyzerEngine.cpp — no #include of juce_gui_basics or juce_graphics.

---

## 4. Runtime smoke

1. Open the plugin (DAW or AnalyzerPro Standalone from build).
2. Feed a continuous signal (e.g. tone or music) to the input.
3. Confirm FFT works: spectrum/RTA display updates; bins show level; no blank or frozen trace.
4. Adjust release/averaging (if exposed); confirm ballistics update.
5. Reset peaks; confirm peak trace resets.
6. Change FFT size; confirm no crash and display updates.

Pass: behavior matches pre–Slice C (FFT and analyzer behavior unchanged; engine is in mdsp_dsp, invoked via adapter).

---

## 5. Rollback plan

To revert Slice C safely in both repos.

**AnalyzerPro:**

Restore full AnalyzerEngine implementation in Source/analyzer/ (if previously replaced by adapter-only) and remove dependency on mdsp_dsp for the engine. Option A: revert the Slice C commit(s). Option B: manual restore.

```bash
cd /path/to/AnalyzerPro
# If Slice C was a single commit:
git revert --no-edit <slice-C-commit-hash>
# Or restore files from commit before Slice C:
git checkout <pre-slice-C-commit> -- Source/analyzer/AnalyzerEngine.h Source/analyzer/AnalyzerEngine.cpp
```

If Source/analyzer/AnalyzerEngine.cpp was reduced to adapter-only, restore the full DSP implementation from the commit that still had it:

```bash
git show <pre-slice-C-commit>:Source/analyzer/AnalyzerEngine.cpp > Source/analyzer/AnalyzerEngine.cpp
git show <pre-slice-C-commit>:Source/analyzer/AnalyzerEngine.h > Source/analyzer/AnalyzerEngine.h
```

Remove include of mdsp_dsp/analyzer/AnalyzerEngine.h and usage of mdsp_dsp::AnalyzerEngine (core_); restore local FFT/ballistics/snapshot code. Then rebuild:

```bash
cmake --build build --target AnalyzerPro
```

**melechdsp-hq:**

Remove AnalyzerEngine from mdsp_dsp so no consumer relies on it until Slice C is re-applied:

```bash
cd /path/to/melechdsp-hq
git rm shared/mdsp_dsp/include/mdsp_dsp/analyzer/AnalyzerEngine.h
git rm shared/mdsp_dsp/src/analyzer/AnalyzerEngine.cpp
# Update shared/mdsp_dsp/CMakeLists.txt: remove src/analyzer/AnalyzerEngine.cpp from add_library
git checkout HEAD -- shared/mdsp_dsp/CMakeLists.txt
# Then manually remove the analyzer/AnalyzerEngine.cpp line from add_library if needed
```

If Slice C was a single commit, revert it:

```bash
git revert --no-edit <slice-C-commit-hash>
```

Rebuild mdsp_dsp if built standalone:

```bash
cmake --build build --target mdsp_dsp
```

**AnalyzerPro submodule (if melechdsp-hq is a submodule):**

After reverting melechdsp-hq, point AnalyzerPro at the reverted revision:

```bash
cd /path/to/AnalyzerPro
git submodule update --init third_party/melechdsp-hq
```

After rollback, AnalyzerPro must compile with its full local AnalyzerEngine (Source/analyzer/) and no dependency on mdsp_dsp for the analyzer engine; mdsp_dsp may still provide MeterBallistics, AnalyzerSnapshot, etc. if Slices A/B remain.
