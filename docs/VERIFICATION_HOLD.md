# Peak-Hold Verification

## 1. Build

From AnalyzerPro root:
- Build melechdsp-hq (mdsp_dsp + mdsp_ui): build the AnalyzerPro target (pulls in third_party/melechdsp-hq).
- Build AnalyzerPro: open CMake, build AnalyzerPro app/plugin target.

## 2. Static checks (rg)

### Hold initialization on FFT init/reinit

Engine: peakHold_ and staging snapshot hold arrays are resized/initialized to floor in initializeFFT.

rg -n 'peakHold_\.resize|stagingSnapshot_.*fftPeakHoldDb.*fill|kDbFloor' --type-add 'cpp:*.{cpp,h}' -t cpp third_party/melechdsp-hq/shared/mdsp_dsp/

Expect:
- peakHold_.resize (static_cast<size_t> (numBins), kDbFloor) in AnalyzerEngine.cpp (initializeFFT).
- std::fill (stagingSnapshot_.fftPeakHoldDb.begin(), stagingSnapshot_.fftPeakHoldDb.end(), kDbFloor) in AnalyzerEngine.cpp (initializeFFT).
- std::fill for fftPeakHoldDbL/R/Mono/Mid/Side in same block.

rg -n 'initializeFFT|applyPendingFftSizeIfNeeded' --type-add 'cpp:*.{cpp,h}' -t cpp third_party/melechdsp-hq/shared/mdsp_dsp/

Expect: initializeFFT called from prepare() and from applyPendingFftSizeIfNeeded() (FFT reinit path).

### Hold initialization on reset / resetPeaks

rg -n 'resetPeaks|reset\(\)|peakHold_.*fill|peakHold_\.clear' --type-add 'cpp:*.{cpp,h}' -t cpp third_party/melechdsp-hq/shared/mdsp_dsp/

Expect:
- resetPeaks() implementation: std::fill (peakHold_.begin(), peakHold_.end(), kDbFloor).
- reset(): peakHold_.clear() (teardown).
- setPeakHoldMode(Off) or similar calls resetPeaks().

### UI: hold array size and validity before renderer

rg -n 'fftPeakHoldDb_.*resize|kPeakHoldDbFloor|kHoldFloorDb|std::fill.*fftPeakHoldDb' --type-add 'cpp:*.{cpp,h}' -t cpp Source/ui/analyzer/

Expect:
- In AnalyzerDisplayView.cpp: fftPeakHoldDb_.resize (validBinsSize) and std::fill (..., kPeakHoldDbFloor) when copying from snapshot.
- When building RenderState: if (rs.isHoldOn && nFft > 0) branch with size check; on mismatch rs.fftPeakHoldDb.resize (nFft) and std::fill (..., kHoldFloorDb); else copy and sanitize (non-finite -> kHoldFloorDb, hold >= fftDb, hold >= fftPeakDb).

### Renderer: draw hold only when enabled and size matches

rg -n 'isHoldOn|fftPeakHoldDb\.size\(\) == .*fftDb' --type-add 'cpp:*.{cpp,h}' -t cpp third_party/melechdsp-hq/shared/mdsp_ui/src/analyzer/AnalyzerRenderer.cpp

Expect: condition like state.isHoldOn && !state.fftPeakHoldDb.empty() && state.fftPeakHoldDb.size() == state.fftDb.size() before drawing hold path.

## 3. Runtime smoke

- Toggle hold on/off: With signal present, enable hold. Verify no flat line at top of spectrum; hold trace appears and follows or holds peak envelope. Disable hold; trace returns to live peak. Re-enable; again no flat line at top.
- Hold captures peaks and decays/resets as expected: With hold on, feed a burst of signal. Hold trace rises and holds peaks. Let signal drop; verify decay (or indefinite hold) per product settings. Use “Reset peaks” (or equivalent); hold and peaks clear to floor, no stuck line at top.
- Changing FFT size does not break hold: With hold on and signal present, change FFT size (e.g. 2048 ↔ 4096). Verify analyzer redraws correctly, hold array matches new bin count, no flat line at top and no crash or garbage.
