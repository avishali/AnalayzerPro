# UI_OVERHAUL_01_FREQ_RANGE_AND_DETAIL — RESULT

Date: 2026-06-01

## Range Plumbing
- Eye-check fix: default/reset view is now 10 Hz to effective Nyquist, with -90 dB vertical range.
- Absolute low limit is now 10 Hz.
- Absolute high limit is dynamic: `lastMetaSampleRate_ * 0.5`, capped by `kHardFreqCeil` and falling back to 20 kHz before metadata arrives.
- `setFrequencyView`, `zoomFrequency`, the shared RTA geometry/grid/hover/log paths, and sample-rate metadata updates all clamp through the dynamic limit.

## Grid And Labels
- RTA grid/layout now follows the active view range through the existing `setFrequencyRange` path.
- Preferred frequency ticks include sub-20 Hz and ultrasonic labels such as 10, 12.5, 16, 25k, 31.5k, and 40k when visible.

## FFT Sizes
- Eye-check fix: FFT choices now stop at 16384 to avoid the CPU risk of 32768.
- Shared analyzer capacity is capped at 16384 samples / 8193 FFT bins.
- Resize still routes through the existing `requestFftSize` / `applyPendingFftSizeIfNeeded` deferred path, so allocations happen off the audio thread.

## Detail Control
- Added APVTS parameter `AnalyzerDetail`.
- Detail maps Low / Medium / High to 256 / 512 / 1024 log bins.
- Detail feeds `AnalyzerRenderStateProvider::numLogBins` independently from FFT size.
- `LogGaussianSmoother::kMaxBins` is raised to 1024 and smoothing now uses the active view range/bin count.

## Build
- Dev build passed:
  `cmake -S . -B build-stage1-dev -DCMAKE_BUILD_TYPE=Debug -DPLUGIN_DEV_MODE=1 -DANALYZERPRO_COPY_AFTER_BUILD=OFF ... && cmake --build build-stage1-dev --config Debug -j 8`

## Eye-Check Corrective Pass
- Moved analyzer frequency pan/zoom/reset and momentary Reset Peaks controls into the header; the plot overlay is disabled.
- Vertical zoom is continuous via drag/wheel, while the APVTS dB range combo remains available as coarse presets.
- No-signal trace rendering now fades as traces approach the visible floor instead of lingering as a flat line.
- Peak/hold hover sampling now follows the selected trace/bin instead of using the rendered peak envelope window.
- Visual/audio eye-check at 44.1/48/96 kHz is still pending outside the build.

STOP
