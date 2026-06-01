# SPECTRUM_SMOOTHING_SQUARED_PEAKS_DIAG

Date: 2026-06-01

## Scope

STEP A only. No fix implemented.

## A1 — FFT-Mode Routing Confirmation

The current code does **not** confirm the runbook's stated FFT-mode route through `convertFFTToLog`.

Actual current routing:

- FFT mode consumes full-resolution `prep.fftDb` from `AnalyzerDisplayWidget::prepareFftTraces()` / `AnalyzerRenderStateBuilder::prepareFftTraces()`.
- `AnalyzerRenderStateBuilder::prepareFftTraces()` copies and sanitizes `snapshot.fftDb` / `snapshot.fftPeakDb`; it does not call `convertFFTToLog()`.
- `AnalyzerDisplayView::Mode::FFT` latches `fftDb_` directly into `fftFrame_`.
- `FftPathBuilder::build()` draws `state.fftDb` via `LogPathBuilder::buildDecimatedPath()`.

The smoothing that affects FFT mode is currently engine-side in `mdsp_dsp::AnalyzerEngine::computeFFT()`:

- `kEngineAppliesSpectral = true`.
- When `smoothingOctaves_ > 0`, the engine uses `smoothLowBounds_` / `smoothHighBounds_` plus `prefixSumMag_` to compute a flat box average in the power domain:
  `freqSmoothed[i] = (prefixSumMag_[high + 1] - prefixSumMag_[low]) / count`.
- With smoothing off, it copies raw `magnitudes_` into `freqSmoothed`.
- Main RMS and multi-trace L/R smoothing use the same box-average style.

So the visible "fine without smoothing, squared with smoothing" explanation is still consistent with a rectangular/box smoothing window, but for FFT mode the active box smoother is engine-side full-resolution smoothing, not `convertFFTToLog()`.

LOG mode does route through `convertFFTToLog()`:

- `AnalyzerDisplayView::Mode::LOG` sets `applyGaussian = (!snapshot.engineDidSpectralSmooth && octaves > 0.0f)`.
- It calls `AnalyzerRenderStateProvider::updateFromSnapshot(..., smoothFn, smoothUserCtx)`.
- `AnalyzerRenderStateProvider` calls `convertFFTToLog()` only when `cfg_.mode == 1`.

Important mismatch: `AnalyzerEngine::computeFFT()` publishes:

- `snapshot.engineDidSpectralSmooth = kEngineAppliesSpectral` (`true`)
- `snapshot.useUILogGaussianOnly = true`

Those flags conflict semantically. Because `engineDidSpectralSmooth` is true, LOG mode does **not** pass the UI Gaussian smoother, even though `useUILogGaussianOnly` says UI-only smoothing.

## A2 — Actual `numLogBins`

`numLogBins` is `256`.

Where set:

- `third_party/melechdsp-hq/shared/mdsp_ui/src/analyzer/AnalyzerRenderStateProvider.cpp`
- Inside `AnalyzerRenderStateProvider::updateFromSnapshot()`, LOG mode branch:
  `constexpr int numLogBins = 256;`

Related fixed-size assumption:

- `Source/ui/analyzer/AnalyzerDisplayView.h`
- `LogGaussianSmoother::kMaxBins = 256`

## A3 — Render Interpolation Type

FFT mode:

- `FftPathBuilder::build()` delegates the main FFT and peak paths to `LogPathBuilder::buildDecimatedPath()`.
- `buildDecimatedPath()` samples one point per plot pixel, applies a small 3-tap Y smoothing pass, then builds a quadratic path using `quadraticTo()`.
- This is not straight `lineTo()` rendering for FFT mode.

LOG mode:

- `LogPathBuilder::buildLogPaths()` connects the 256 log-bin values with `lineTo()`.
- The peak log path also uses `lineTo()`.
- This can make the coarse 256-point log trace look faceted/flat-topped.

BAND mode:

- `RTADisplayModel::buildBandsPaths()` connects band centers with `lineTo()`.
- BAND also derives data using box averaging in `convertFFTToBands()`.

## A4 — Chosen Minimal Fix

Recommended first fix: replace the rectangular/box smoothing gather with an energy-normalized weighted gather in the active smoothing paths.

Why:

- The current evidence now points to box averaging as the common denominator:
  - FFT/main/multi-trace smoothing: engine-side flat prefix-sum average over octave bounds.
  - LOG mapping: flat box average per log bin before any optional smoother.
  - BAND mapping: flat third-octave box average.
- Increasing `numLogBins` alone only helps LOG mode; it does not address FFT mode, where the current squared peak can happen before UI rendering.
- Smooth-curve rendering alone may help LOG/BAND visual faceting, but it does not fix the box-smoothed values feeding FFT mode.
- A triangular or Gaussian weighted power-domain gather, with weights normalized to sum to 1, preserves flat input and avoids the rectangular-window plateau that creates squared peaks.

Suggested implementation order after owner approval:

1. Fix the flag mismatch first: decide whether engine smoothing or UI-only smoothing is the intended active model, then make `engineDidSpectralSmooth` and `useUILogGaussianOnly` agree.
2. Apply weighted, normalized gather to the active smoothing path:
   - If keeping engine smoothing: replace the engine flat prefix-sum average for main and multi-trace smoothing.
   - If reverting to UI-only smoothing: ensure FFT mode truly routes to the intended UI smoothing path, then fix `convertFFTToLog()`/mapping as needed.
3. Consider `B3` smooth rendering as a secondary LOG/BAND polish if the weighted gather does not fully remove visual faceting.

## Files Inspected

- `PROMPTS/RUNBOOKS/SPECTRUM_SMOOTHING_SQUARED_PEAKS_V1.md`
- `Source/ui/analyzer/AnalyzerDisplayView.cpp`
- `Source/ui/analyzer/AnalyzerDisplayView.h`
- `Source/analyzer/AnalyzerEngine.h`
- `Source/analyzer/AnalyzerEngine.cpp`
- `third_party/melechdsp-hq/shared/mdsp_dsp/src/analyzer/AnalyzerEngine.cpp`
- `third_party/melechdsp-hq/shared/mdsp_dsp/include/mdsp_dsp/analyzer/AnalyzerEngine.h`
- `third_party/melechdsp-hq/shared/mdsp_gui/include/mdsp_gui/analyzer/AnalyzerDisplayWidget.h`
- `third_party/melechdsp-hq/shared/mdsp_gui/src/analyzer/AnalyzerDisplayWidget.cpp`
- `third_party/melechdsp-hq/shared/mdsp_gui/include/mdsp_gui/analyzer/AnalyzerRenderStateBuilder.h`
- `third_party/melechdsp-hq/shared/mdsp_gui/src/analyzer/AnalyzerRenderStateBuilder.cpp`
- `third_party/melechdsp-hq/shared/mdsp_ui/src/analyzer/AnalyzerRenderStateProvider.cpp`
- `third_party/melechdsp-hq/shared/mdsp_ui/src/analyzer/FftBinMapping.cpp`
- `third_party/melechdsp-hq/shared/mdsp_ui/src/rta/FftPathBuilder.cpp`
- `third_party/melechdsp-hq/shared/mdsp_ui/src/rta/LogPathBuilder.cpp`
- `third_party/melechdsp-hq/shared/mdsp_ui/src/rta/RTADisplayModel.cpp`

STOP.

