MISSION_ID: UI_OVERHAUL_01_FREQ_RANGE_AND_DETAIL

TITLE
Extend the spectrum to 10 Hz → Nyquist (beyond 20 Hz–20 kHz), expose larger FFT sizes for low-frequency detail, and add a clearly-labeled "Detail" (display density) control separate from "FFT Size".

GOAL (owner decisions 2026-06-01)
- Visible/zoomable frequency range: low edge 10 Hz, high edge = Nyquist (sample-rate dependent: ~22.05k@44.1k … 48k@96k …). Default VIEW can stay 20 Hz–20 kHz (familiar) but the user can zoom/pan out to 10 Hz–Nyquist and see more LF peaks.
- More low-frequency detail: add larger FFT sizes (16384, 32768) — bigger FFT = finer LF resolution (bin = sampleRate/fftSize).
- "FFT Size" and "Detail" are SEPARATE, clearly labeled controls:
    FFT Size  = analysis resolution / CPU (engine FFT order).
    Detail    = display trace density (log-bin count used in convertFFTToLog and the render path), decoupled from CPU-heavy FFT size.

CURRENT STATE (verified)
- Source/ui/analyzer/AnalyzerDisplayView.h:177-178 — static constexpr kAbsFreqMin=20.0f, kAbsFreqMax=20000.0f (these clamp zoom/pan).
- Grid draw uses kLogFreqMinHz=20.0 / kLogFreqMaxHz=20000.0 (AnalyzerDisplayView.cpp ~1925).
- FFT Size combos: 1024/2048/4096/8192 in BOTH Source/ui/layout/ControlRail.cpp and SettingsPopupPanel.cpp; param ControlId::AnalyzerFftSize.
- convertFFTToLog uses numLogBins=256 (AnalyzerRenderStateProvider.cpp); LogGaussianSmoother kMaxBins=256.
- Engine has RT-safe deferred FFT resize (applyPendingFftSizeIfNeeded; requestFftSize).

HARD RULES
- RT-safe: FFT-size changes use the existing deferred-resize path (no audio-thread allocations). Larger sizes (16384/32768) must be validated against engine buffer capacity (bump kMaxFftSize/capacities in prepare, off the hot path).
- numLogBins increase: bump LogGaussianSmoother kMaxBins and any 256-sized scratch consistently; preallocate; no per-frame allocation.
- Do NOT regress Phase 2 render or the triangular smoothing.
- Keep the default VIEW at 20 Hz–20 kHz so existing users aren't surprised; only the LIMITS extend.

============================================================
IMPLEMENTER (STOP-gated)
============================================================
STEP 1 — Extend the absolute range limits
- AnalyzerDisplayView.h: kAbsFreqMin 20→10. Replace the fixed kAbsFreqMax=20000 with a sample-rate-derived Nyquist limit: effective max = jmin(currentNyquist, kHardFreqCeil) where currentNyquist = lastMetaSampleRate_*0.5 and kHardFreqCeil is a generous constant (e.g. 96000). Where sampleRate isn't known yet, fall back to 20000.
- Update setFrequencyView / zoomFrequency / resetFrequencyView clamps to use the dynamic max.
STOP and report how Nyquist is plumbed to the clamp.

STEP 2 — Extend the grid + axis labels
- The frequency grid/labels (kLogFreqMinHz/MaxHz ~1925, and the axis label generator in mdsp_ui rta AxisRenderer/ScaleLabelRenderer) must draw decade ticks across the active view range, including sub-20 Hz (10, 12.5, 16) and >20 kHz (25k, 31.5k, 40k…up to Nyquist).
- Labels follow the CURRENT view range (zoomed), not a fixed 20–20k.
STOP and report which files render the grid/labels and how they now follow the view range.

STEP 3 — Add larger FFT sizes
- Add "16384" and "32768" to fftSizeCombo_ in ControlRail.cpp AND SettingsPopupPanel.cpp (keep IDs/param mapping consistent with ControlId::AnalyzerFftSize and MainView's order mapping).
- Engine: confirm/raise max FFT capacity so 16384/32768 allocate safely in prepare()/deferred-resize (not on the audio thread). Validate CPU is acceptable at 32768.
STOP and report engine capacity changes + CPU note at 32768.

STEP 4 — Add the "Detail" control (display density)
- Add a "Detail" combo + param (e.g. ControlId::AnalyzerDetail) with options mapping to numLogBins: e.g. Low=256, Medium=512, High=1024 (default Medium or keep 256 as Low for parity).
- Wire it to AnalyzerRenderStateProvider numLogBins; bump LogGaussianSmoother kMaxBins and any 256-fixed scratch to the new max (1024); preallocate. Render/interpolation paths must handle the variable bin count with no per-frame allocation.
- Label clearly: "FFT Size" tooltip = "Analysis resolution (CPU). Larger = finer low-freq detail." ; "Detail" tooltip = "Display trace density. Higher = smoother, more detailed curve."
STOP and report the param, the numLogBins plumbing, and the kMaxBins bump.

STEP 5 — (optional) Range presets / focus
- Optional convenience: a small range control (e.g. presets Full / 20–20k / Sub 10–200 / custom drag) using the existing zoom/pan. Only if it fits the IA cleanly; otherwise leave the existing zoom/pan (now reaching 10–Nyquist) and defer presets to Stage 2.
STOP and report whether presets were added or deferred.

STEP 6 — Build + eye-check
- Dev build (-DPLUGIN_DEV_MODE=1 -DANALYZERPRO_COPY_AFTER_BUILD=OFF). With audio:
  - Zoom/pan out — confirm the trace + grid now show down to 10 Hz and up to Nyquist.
  - At 32768 + Detail High, confirm more LF peaks resolve and the curve is detailed without faceting/squaring (Phase-2 triangular smoothing still applies).
  - Confirm no glitches at FFT-size or Detail changes (RT-safe resize), CPU acceptable.
STOP and write PROMPTS/MISSIONS/UI_OVERHAUL_01_RESULT.md (range plumbing, FFT sizes, Detail param, CPU at 32768, eye-check). End with STOP.

============================================================
VERIFIER
============================================================
CHECK 1 — Limits: zoom/pan reaches 10 Hz and Nyquist (sample-rate dependent); default view still 20–20k.
CHECK 2 — Grid/labels follow the view range incl. sub-20 and >20k ticks.
CHECK 3 — FFT sizes 16384/32768 present, RT-safe (deferred resize, no audio-thread alloc), CPU acceptable.
CHECK 4 — Detail control changes display density (numLogBins) independent of FFT Size; clearly labeled; no per-frame alloc; kMaxBins bumped.
CHECK 5 — No regression to Phase-2 glassy render or triangular smoothing.
OUTPUT: PROMPTS/MISSIONS/UI_OVERHAUL_01_VERIFIER.md table. End with STOP.
