# Phase-1 / Slice A — Verification

Verify AnalyzerPro uses only shared mdsp_dsp MeterBallistics (and Smoother if applicable). No local DSP ballistics/smoother definitions.

---

## 1. Build steps

### melechdsp-hq (if needed)

AnalyzerPro builds melechdsp-hq via add_subdirectory; no separate build required. If building melechdsp-hq standalone:

```bash
cd /path/to/melechdsp-hq
cmake -B build -DJUCE_PATH="$JUCE_PATH"
cmake --build build --target mdsp_dsp
```

### AnalyzerPro

```bash
cd /path/to/AnalyzerPro
cmake -B build -DJUCE_PATH="$JUCE_PATH"
cmake --build build --target AnalyzerPro
```

Expect: build completes with no errors. Plugin artefact in `build/AnalyzerPro_artefacts/`.

---

## 2. Static checks (ripgrep)

Run from AnalyzerPro repo root.

**Prove AnalyzerPro does not define local MeterBallistics or DSP Smoother:**

```bash
rg 'class MeterBallistics|struct MeterBallistics|class Smoother|struct Smoother' --type-add 'src:*.{h,cpp}' -t src Source/
```

Expected: no matches in Source/ (any match would be a local definition; mdsp_dsp is under third_party).

```bash
rg 'class MeterBallistics|struct MeterBallistics' Source/
```

Expected: no matches. (AnalyzerPro only uses mdsp_dsp::MeterBallistics; no local class.)

**Prove ballistics include comes from mdsp_dsp:**

```bash
rg 'MeterBallistics|#include.*MeterBallistics|#include.*Smoother' Source/ --type-add 'src:*.{h,cpp}' -t src
```

Expected: only `#include <mdsp_dsp/MeterBallistics.h>` and references to `mdsp_dsp::MeterBallistics`. No `#include` of a local MeterBallistics or Smoother header.

```bash
rg '#include.*mdsp_dsp' Source/
```

Expected: at least one match (e.g. AnalyzerEngine.h: `#include <mdsp_dsp/MeterBallistics.h>`).

Note: LogGaussianSmoother in AnalyzerDisplayView is UI spectral smoothing per mission; it is not the mdsp_dsp Smoother and is left out of scope.

---

## 3. Link / ODR checks

**No duplicate MeterBallistics symbols:**

After a clean build, inspect the plugin binary for MeterBallistics symbols:

```bash
cd build
nm -C AnalyzerPro_artefacts/libAnalyzerPro_SharedCode.a 2>/dev/null | grep -i MeterBallistics
# or for a linked .so/.dylib:
nm -C AnalyzerPro_artefacts/Debug/AnalyzerPro.vst3/Contents/MacOS/AnalyzerPro 2>/dev/null | grep -i MeterBallistics
```

Expected: symbols (e.g. from mdsp_dsp) appear once. Multiple definitions of the same symbol would indicate ODR violation.

**Linker expectation:** Build must complete without "multiple definition of" or "redefinition of" errors for MeterBallistics or Smoother. If the project links mdsp_dsp and uses only `#include <mdsp_dsp/MeterBallistics.h>` with no local definition, no duplicate symbols should appear.

---

## 4. Runtime smoke steps

1. Open the plugin (DAW or AnalyzerPro Standalone from build).
2. Feed a continuous signal (e.g. tone or music) to the input.
3. Confirm analyzer meters/traces respond: RMS and peak traces move with level; attack/release behavior looks correct.
4. Change release/averaging (if exposed); confirm ballistics update (e.g. slower release).
5. Reset peaks; confirm peak trace resets as before.
6. Change FFT size; confirm no crash and display updates (ballistics state resampled).

Pass: behavior matches pre-slice (meters and traces unchanged in character).

---

## 5. Rollback plan

To revert Phase-1 Slice A only (AnalyzerPro ballistics unification):

```bash
cd /path/to/AnalyzerPro
git checkout -- Source/analyzer/AnalyzerEngine.h Source/analyzer/AnalyzerEngine.cpp
```

To revert and remove any untracked verification doc:

```bash
cd /path/to/AnalyzerPro
git checkout -- Source/analyzer/AnalyzerEngine.h Source/analyzer/AnalyzerEngine.cpp
git clean -fd docs/VERIFICATION.md 2>/dev/null || true
```

To revert the entire working tree to last commit (discard all local changes):

```bash
cd /path/to/AnalyzerPro
git checkout -- .
git clean -fd
```

Rebuild after rollback:

```bash
cmake --build build --target AnalyzerPro
```
