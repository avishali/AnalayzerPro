# SPECTRUM_SMOOTHING_SQUARED_PEAKS_RESULT

Date: 2026-06-01

## Outcome: FIXED (owner-verified by eye)

Root cause (confirmed STEP A): flat box-average (rectangular) smoothing squared the peaks — engine-side prefix-sum box for FFT/main/multi traces; box-per-log-bin for LOG; third-octave box for BAND. A flag contradiction (engineDidSpectralSmooth=true AND useUILogGaussianOnly=true) also left the UI LogGaussianSmoother dead.

Fix (STEP B, cascaded box → triangular, owner-approved):
- Engine computeFFT() now applies TWO cascaded box passes (prefix sum rebuilt between passes → genuine triangular window) to main + multi-trace power. Verified: prefix sum IS rebuilt from pass-1 output (not box-twice).
- Each pass uses half the requested octave width so cascaded support ≈ requested smoothing.
- Flags made consistent: engineDidSpectralSmooth = spectralSmoothingActive, useUILogGaussianOnly = false.
- RT-safe: reuses preallocated buffers, no audio-thread allocations; scratch==output aliasing safe via prefix-sum decoupling. Energy-conserving (each pass divides by count).

Owner verification: "all the traces I can see are working good" — peaks rounded, no squaring.

## Caveats / open items
- Side trace could NOT be toggled to verify because the Side trace on/off control ("scrolling menu") is MISSING in the UI. Owner confirms this is PRE-EXISTING and NOT caused by this work. Logged as a separate task (HeaderBar tracesBtn_ dropdown / SettingsPopupPanel sideBtn_ / ControlId::TraceShown).
- Mid/Side/Mono computation was restructured (magnitude-domain pre-smoothing + single ballistics, vs prior post-ballistics derivation). Visible traces look correct; Side specifically unverified pending the missing toggle.
- LOG/BAND smooth-curve render polish (lineTo on coarse grid) remains a DEFERRED optional follow-up; engine fix already rounds the underlying data.
- The engine change lives in the (intentionally) dirty melechdsp-hq submodule — not yet committed/pinned.

STOP.
