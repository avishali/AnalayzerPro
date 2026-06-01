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

============================================================
STEP A — CONFIRM + CHOOSE FIX (no fix yet; STOP for owner review)
============================================================
A1. Confirm FFT mode (not just LOG) routes through convertFFTToLog / the numLogBins resampling when smoothingOctaves>0, and bypasses it (raw full-res) when smoothing is OFF. This explains "fine without smoothing, squared with."
A2. Report the actual numLogBins value (the "256 log bins" from the AnalyzerEngine comment) and where it is set.
A3. Inspect the rendering of the log-bin trace: are the numLogBins points connected with straight line segments, or a smooth curve (Catmull-Rom/bezier)? Straight segments on a coarse grid read as faceted/squared.
A4. Pick the MINIMAL fix that rounds peaks while conserving energy (see STEP B); justify the choice.
STOP and write PROMPTS/MISSIONS/SPECTRUM_SMOOTHING_SQUARED_PEAKS_DIAG.md with: FFT-mode routing confirmation, numLogBins, render interpolation type, chosen fix + why. Do NOT implement yet.

============================================================
STEP B — FIX (after owner approves the diagnosis) — pick the minimal combination that rounds peaks
============================================================
Candidate fixes (the Gaussian is NOT the problem — do not change its shape):
  B1. Increase numLogBins (e.g. 256 → 512/1024) so the log grid is fine enough that peaks aren't faceted. Cheapest; verify CPU.
  B2. Replace the flat per-log-bin BOX average (sumPower/binCount) with a Gaussian/triangular WEIGHTED gather across the contributing FFT bins, so the mapping itself rounds rather than flattens. Keep it power-domain and energy-conserving (weights sum to 1).
  B3. Render the log-bin trace with a smooth curve (Catmull-Rom / Cardinal spline) instead of straight segments between vertices, so a coarse grid still draws rounded peaks. (Render-only; in the submodule path builder.)
- All must conserve energy (flat input stays flat; integrated band energy of a sine preserved) and stay RT-safe (no audio-thread allocations; any kernel rebuild off the hot path).
- Prefer B1 and/or B3 first if they resolve it (smallest blast radius); use B2 if the box gather is the dominant cause.
STOP and report files changed, which fix(es) applied, and RT-safety/energy notes.

============================================================
VERIFIER
============================================================
CHECK 1 — Scope/RT-safety: no audio-thread allocations or locks; kernel rebuild off the hot path.
CHECK 2 — Visual: with 1/24, 1/12, 1/6, 1/3 smoothing, peaks are rounded/pointy (no flat tops), all modes, both RMS and peak traces.
CHECK 3 — Level: smoothing conserves energy (no overall gain shift; flat input stays flat).
CHECK 4 — No regression to Phase 2 render path or smoothness.
OUTPUT: PROMPTS/MISSIONS/SPECTRUM_SMOOTHING_SQUARED_PEAKS_VERIFIER.md table. End with STOP.
