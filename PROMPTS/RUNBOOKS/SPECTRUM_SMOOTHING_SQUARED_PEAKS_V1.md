MISSION_ID: SPECTRUM_SMOOTHING_SQUARED_PEAKS_V1

TITLE
Fix "squared / flat-topped" peaks that appear ONLY when fractional-octave smoothing (1/24, 1/12, 1/6, 1/3) is enabled. Make smoothed peaks rounded/pointy like reference analyzers.

EVIDENCE (owner, 2026-06-01)
- Squaring appears in ALL modes (FFT/LOG/BAND) and on BOTH the main RMS trace and the peak trace.
- Identical with VBlank interpolation ON and OFF (kill-switch A/B) → NOT caused by the Phase 2 render work. PRE-EXISTING.
- With smoothing OFF the trace looks correct. With 1/12 or 1/24 smoothing the peaks go flat-topped/squared.
- The UI smoother third_party/.../mdsp_dsp/src/LogGaussianSmoother.cpp is a CORRECT normalized log-Gaussian (rounds peaks). So that kernel is NOT the cause when it is the active path.

ROOT CAUSE (confirmed by code read, 2026-06-01)
Smoothing model is "Option A": engine does NOT spectrally smooth (useUILogGaussianOnly=true). The active smoothing is UI-side, in convertFFTToLog (third_party/.../mdsp_ui/src/analyzer/FftBinMapping.cpp), driven from AnalyzerRenderStateProvider. That function:
  (1) resamples the FFT into numLogBins by FLAT BOX-AVERAGING power within each log bin's frequency span (sumPower / binCount over [lowerBin..upperBin]);
  (2) applies the log-Gaussian smoother (smoothPowerFn / the *_.process thunk in AnalyzerDisplayView, gated by applyGaussian when smoothingOctaves>0);
  (3) converts back to dB.
With smoothing OFF, FFT mode draws the raw full-resolution spectrum (pointy). With smoothing ON, the trace routes through this coarse log-bin resampling → the flat box-average per bin + the limited numLogBins grid produce FACETED / FLAT-TOPPED peaks. The Gaussian is fine; the squaring is the resampling resolution + the rectangular (box) per-bin gather, plus likely straight-line rendering between the log-bin vertices.

HARD RULES
- Engine/DSP changes MUST stay RT-safe: no allocations on the audio thread, no locks. Kernel rebuilds happen off the audio thread (prepare/param-change), consume preallocated buffers in process.
- Do NOT regress the smoothness/Phase 2 render path. UI render code stays as-is.
- Match the existing log-Gaussian behavior/normalization so loudness/level is unchanged (smoothing must conserve energy — normalized weights summing to 1 in the power domain).
- Keep changes minimal and behind the existing smoothing selection.

FILES ALLOWED (STEP B)
- third_party/melechdsp-hq/shared/mdsp_dsp/src/analyzer/AnalyzerEngine.cpp (computeFFT smoothing + snapshot flag publish) and its header for any preallocated scratch member.
- third_party/.../mdsp_dsp/include/.../AnalyzerSnapshot.h only if a flag default needs adjusting.
- NOTE: this is the (intentionally) dirty melechdsp-hq submodule — layer on top of the existing WIP; do not revert it; do not stage the submodule pointer.
FORBIDDEN: UI render path / Phase-2 code (AnalyzerDisplayView render, RTADisplay), PluginProcessor. (LOG/BAND smooth-curve rendering is a separate deferred follow-up.)

============================================================
STEP A — CONFIRM + CHOOSE FIX (no fix yet; STOP for owner review)
============================================================
A1. Confirm FFT mode (not just LOG) routes through convertFFTToLog / the numLogBins resampling when smoothingOctaves>0, and bypasses it (raw full-res) when smoothing is OFF. This explains "fine without smoothing, squared with."
A2. Report the actual numLogBins value (the "256 log bins" from the AnalyzerEngine comment) and where it is set.
A3. Inspect the rendering of the log-bin trace: are the numLogBins points connected with straight line segments, or a smooth curve (Catmull-Rom/bezier)? Straight segments on a coarse grid read as faceted/squared.
A4. Pick the MINIMAL fix that rounds peaks while conserving energy (see STEP B); justify the choice.
STOP and write PROMPTS/MISSIONS/SPECTRUM_SMOOTHING_SQUARED_PEAKS_DIAG.md with: FFT-mode routing confirmation, numLogBins, render interpolation type, chosen fix + why. Do NOT implement yet.

============================================================
STEP B — FIX: cascaded box → triangular (OWNER-APPROVED 2026-06-01). Engine-side, RT-cheap.
============================================================
Chosen model: keep smoothing ENGINE-side (respects the Feb-2026 power-domain move); replace the single flat box average with a CASCADED box (2×, optionally 3×) so the effective window is TRIANGULAR / quasi-Gaussian. This rounds peaks while staying O(1)/bin via the existing prefix-sum machinery. Do NOT switch to a per-bin Gaussian gather and do NOT revert to UI-only smoothing.

B0 — Fix the flag contradiction FIRST (required, independent of method):
  - In mdsp_dsp::AnalyzerEngine::computeFFT(): engine smooths, so publish CONSISTENT flags:
      snapshot.engineDidSpectralSmooth = true (when smoothingOctaves>0)
      snapshot.useUILogGaussianOnly    = false
  - Verify every consumer of useUILogGaussianOnly still behaves (UI applyGaussian must stay false so there is no double smoothing). The UI LogGaussianSmoother stays unused/dead for now — do not delete in this commit.

B1 — Cascaded box in the engine (FFT main + multi-trace L/R, the prefix-sum box path):
  - Today: freqSmoothed[i] = (prefixSum[high+1] - prefixSum[low]) / count  (one rectangular pass → plateaus).
  - Change to TWO passes: box-average, rebuild a prefix sum over the result, box-average again → triangular window → rounded peaks. (3 passes ≈ Gaussian if 2 isn't round enough.)
  - WIDTH SCALING (critical): scale each pass's half-width so the CASCADED effective bandwidth still equals the requested smoothingOctaves — i.e. each box pass uses ~half the current width, so 2 cascaded passes ≈ the originally intended octave width (NOT double it). Document the width math.
  - Apply to the same traces that currently box-smooth (main RMS + multi L/R/Mid/Side/Mono as applicable).

RT-safety + energy (hard requirements):
  - No audio-thread allocations: preallocate the second prefix-sum/scratch buffer in prepare() (size = max bins). Reuse it each block.
  - Each averaging pass divides by count → conserves the power-domain mean → flat input stays flat, level unchanged. Verify.
  - Recompute per-bin [low,high] bounds only on smoothingOctaves / fftSize change (off the hot path), as today.

NOTE on LOG/BAND: they consume the engine-smoothed snapshot.fftDb, so the cascaded-box engine fix rounds their DATA too. Residual visual faceting from lineTo on the 256-bin grid is a RENDER-only follow-up (deferred by owner — only do smooth-curve LOG/BAND rendering if it still looks faceted after this).

STOP and report: files changed, the width-scaling math, RT-safety (no new audio-thread allocs), energy check, and which traces were updated.

============================================================
VERIFIER
============================================================
CHECK 1 — Scope/RT-safety: no audio-thread allocations or locks; kernel rebuild off the hot path.
CHECK 2 — Visual: with 1/24, 1/12, 1/6, 1/3 smoothing, peaks are rounded/pointy (no flat tops), all modes, both RMS and peak traces.
CHECK 3 — Level: smoothing conserves energy (no overall gain shift; flat input stays flat).
CHECK 4 — No regression to Phase 2 render path or smoothness.
OUTPUT: PROMPTS/MISSIONS/SPECTRUM_SMOOTHING_SQUARED_PEAKS_VERIFIER.md table. End with STOP.
