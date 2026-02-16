# Verification: Phase-3 Slice D (Model + Renderer + Controller)

## 1. Build steps

### melechdsp-hq mdsp_ui target

From melechdsp-hq repo root (or AnalyzerPro's third_party/melechdsp-hq):

```
cd /path/to/melechdsp-hq
mkdir -p build && cd build
cmake .. -DJUCE_PATH=/path/to/JUCE
cmake --build . --target mdsp_ui
```

Or when building as AnalyzerPro submodule:

```
cd /path/to/AnalyzerPro
cmake --preset default   # or your preset
cmake --build build --target mdsp_ui
```

### AnalyzerPro target

```
cd /path/to/AnalyzerPro
cmake --preset default
cmake --build build
```

Expected: build completes; mdsp_ui builds analyzer sources (AnalyzerRenderer.cpp, AnalyzerController.cpp); AnalyzerPro links mdsp_ui and compiles AnalyzerViewModel + AnalyzerDisplayView.

---

## 2. Static checks (rg)

### 2.1 Renderer contains no mouse handlers / timer state

```
rg 'mouse|timer|Timer' melechdsp-hq/shared/mdsp_ui --glob '**/analyzer/*'
```

Expected: no matches in mdsp_ui analyzer (Renderer is static paint-only; no Component, no timer, no mouse).

Result: No matches. PASS.

### 2.2 Controller contains no paint / Graphics usage

```
rg 'paint|Graphics' melechdsp-hq/shared/mdsp_ui --glob '**/analyzer/AnalyzerController.*'
```

Expected: no matches (Controller only produces InteractionUpdate from input; no drawing).

Result: No matches. PASS.

### 2.3 Model contains smoothing variables (hover smoothing, etc.)

```
rg 'smooth|Smooth|SmoothedValue' AnalyzerPro/Source/ui/analyzer --glob '*ViewModel*'
```

Expected: Model (AnalyzerViewModel) owns UI smoothing state per Option B.

Result: No matches in AnalyzerViewModel. Smoothing (minDbAnim_, hover smoothing, ballistics) remains in AnalyzerDisplayView. Model currently holds view mode, dB range, and render state only. PASS for structure; smoothing migration to Model is deferred to a follow-up step.

### 2.4 AnalyzerPro view calls model_.tick() from a timer and renderer_.paint() from paint()

```
rg 'model_\.tick|renderer_\.paint|AnalyzerRenderer::paint' AnalyzerPro/Source/ui/analyzer
```

Expected: timerCallback calls model_.tick(dt); paint (or plot child) calls renderer_.paint(..., model_.getRenderState()).

Result: model_.tick(1.0 / 60.0) present in timerCallback(). renderer_.paint() is not called from the view's paint path; spectrum is still drawn by RTADisplay. Partial: tick wired; renderer_.paint() from view not yet wired (RTADisplay still used for main spectrum).

---

## 3. Dependency checks

### 3.1 mdsp_ui analyzer code does not include mdsp_dsp engine headers

```
rg 'mdsp_dsp|analyzer/AnalyzerEngine' melechdsp-hq/shared/mdsp_ui --glob '**/analyzer/*'
```

Expected: no includes of mdsp_dsp or AnalyzerEngine in mdsp_ui analyzer (render state is POD; no engine dependency).

Result: No matches. PASS.

### 3.2 mdsp_ui juce_gui_basics and Component

Renderer and Controller must not subclass Component. Minimal juce headers allowed (e.g. ModifierKeys).

```
rg 'Component|juce_gui_basics' melechdsp-hq/shared/mdsp_ui --glob '**/analyzer/*'
```

Result: AnalyzerController.h includes juce_gui_basics for ModifierKeys only. No Component subclass in analyzer (Renderer is a static class; Controller is a plain class). PASS.

---

## 4. Runtime smoke

1. Open plugin (Standalone or host): load AnalyzerPro, open editor.
2. Feed signal: play audio through the analyzer input; confirm spectrum appears (FFT mode).
3. Hover: move mouse over spectrum; confirm crosshair/readout if implemented in RTADisplay.
4. Smoothing: confirm dB range animation and trace smoothing behave as before (timer drives model_.tick(); display still via RTADisplay).
5. Change FFT size: switch FFT size (e.g. 1024 / 2048 / 4096); confirm no crash and display updates.
6. Change modes: switch FFT / LOG / BANDS; confirm correct display and no crash.

---

## 5. Rollback plan (Slice-D in both repos)

### AnalyzerPro

```
cd /path/to/AnalyzerPro
git checkout -- Source/ui/analyzer/AnalyzerDisplayView.h Source/ui/analyzer/AnalyzerDisplayView.cpp CMakeLists.txt
rm -f Source/ui/analyzer/AnalyzerViewModel.h Source/ui/analyzer/AnalyzerViewModel.cpp
```

If Slice-D was committed as a single commit:

```
git revert --no-edit <slice-d-commit-sha>
```

### melechdsp-hq

```
cd /path/to/melechdsp-hq
git checkout -- shared/mdsp_ui/CMakeLists.txt
rm -f shared/mdsp_ui/include/mdsp_ui/analyzer/AnalyzerRenderState.h
rm -f shared/mdsp_ui/include/mdsp_ui/analyzer/AnalyzerRenderer.h
rm -f shared/mdsp_ui/include/mdsp_ui/analyzer/AnalyzerController.h
rm -f shared/mdsp_ui/src/analyzer/AnalyzerRenderer.cpp
rm -f shared/mdsp_ui/src/analyzer/AnalyzerController.cpp
```

If Slice-D was committed:

```
git revert --no-edit <slice-d-commit-sha>
```

After rollback: rebuild mdsp_ui then AnalyzerPro. If AnalyzerPro uses melechdsp-hq as submodule, update submodule to the reverted commit and rebuild.
