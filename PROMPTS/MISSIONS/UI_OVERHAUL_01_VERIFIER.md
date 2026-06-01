# UI_OVERHAUL_01_FREQ_RANGE_AND_DETAIL — VERIFIER

| Check | Status | Notes |
|---|---|---|
| Limits reach 10 Hz to Nyquist | Build-pass, eye-check pending | Clamp is dynamic from snapshot sample-rate metadata with 20 kHz fallback before metadata arrives. |
| Default/reset view is 10 Hz to Nyquist, -90 dB | Build-pass, eye-check pending | Constructor, first valid FFT metadata, reset button, and double-click reset use the full default view. |
| Grid/labels follow active view | Build-pass, eye-check pending | Shared RTA geometry/grid/hover/log paths use the effective Nyquist-clamped max. |
| FFT sizes stop at 16384 | Pass | ControlRail, SettingsPopupPanel, APVTS, MainView, processor mapping, and engine capacity are capped at 16384 / 8193 bins. |
| FFT resize remains RT-safe | Pass by code path | Audio thread requests only; `applyPendingFftSizeIfNeeded` performs resize on the message thread. |
| Detail control changes display density | Build-pass, eye-check pending | `AnalyzerDetail` maps 256/512/1024 log bins into `AnalyzerRenderStateProvider`. |
| `LogGaussianSmoother` capacity bumped | Pass | `kMaxBins` is 1024 and smoothing uses active bin count/range. |
| Header nav and Reset Peaks | Build-pass, eye-check pending | Frequency pan/zoom/reset and momentary Reset Peaks are in `HeaderBar`; plot overlay is disabled. |
| Continuous vertical zoom | Build-pass, eye-check pending | Drag/wheel now adjust bottom dB continuously; combo presets remain coarse APVTS values. |
| No-signal trace fade | Build-pass, eye-check pending | FFT/log/band render paths fade at the visible floor instead of drawing a flat floor trace. |
| Peak/hold crosshair follows selected trace | Build-pass, eye-check pending | Hover readout uses the selected trace bin directly; rendered peak envelope max-window remains for drawing. |
| Phase-2/glassy render regression | Eye-check pending | Build succeeded; visual validation still needed. |

STOP
